#include "xiso.h"
#include "text.h"
#include <windows.h>
#include <cstring>

namespace launcher {
namespace {
constexpr uint32_t kSector = 2048;
constexpr char kMagic[] = "MICROSOFT*XBOX*MEDIA";
// Where the game partition begins: trimmed XISO, then full XGD1/2/3 dumps.
constexpr uint64_t kPartitions[] = {0, 0x18300000ull, 0xFD90000ull, 0x2080000ull};
constexpr uint8_t kDirectory = 0x10;

bool safe_name(const std::string& name)
{
    if (name.empty() || name == "." || name == "..") return false;
    for (char c : name)
        if (c == '/' || c == '\\' || c == ':' || (unsigned char)c < 32) return false;
    return true;
}
}

Xiso::~Xiso()
{
    if (handle_) CloseHandle(handle_);
}

bool Xiso::read_at(uint64_t offset, void* data, uint32_t size) const
{
    if (offset + size > image_size_) return false;
    OVERLAPPED at{};
    at.Offset = (DWORD)offset;
    at.OffsetHigh = (DWORD)(offset >> 32);
    DWORD got = 0;
    return ReadFile(handle_, data, size, &got, &at) && got == size;
}

bool Xiso::open(const fs::path& image, std::wstring& error)
{
    handle_ = CreateFileW(image.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) { handle_ = nullptr; error = L"The image could not be opened."; return false; }
    LARGE_INTEGER size{};
    GetFileSizeEx(handle_, &size);
    image_size_ = (uint64_t)size.QuadPart;
    for (uint64_t base : kPartitions) {
        char header[28] = {};
        if (!read_at(base + 32 * kSector, header, sizeof(header)) || std::memcmp(header, kMagic, 20) != 0) continue;
        base_ = base;
        uint32_t root_sector, root_size;
        std::memcpy(&root_sector, header + 20, 4);
        std::memcpy(&root_size, header + 24, 4);
        files_.clear();
        if (!walk(root_sector, root_size, "", 0, error)) return false;
        if (!find("default.xbe")) { error = L"The image has no default.xbe; it is not an Xbox game disc."; return false; }
        return true;
    }
    error = L"This is not an Xbox disc image (no Xbox game filesystem was found in it).";
    return false;
}

// A directory is a table of entries forming a binary tree; each entry holds
// the dword offsets of its left and right neighbours within the table.
bool Xiso::walk(uint32_t sector, uint32_t size, const std::string& prefix, int depth, std::wstring& error)
{
    if (depth > 32) { error = L"The disc's folders nest too deeply."; return false; }
    if (!size) return true;
    if (size > 64u * 1024 * 1024) { error = L"A folder table on the disc is damaged."; return false; }
    std::vector<unsigned char> table(size);
    if (!read_at(base_ + (uint64_t)sector * kSector, table.data(), size)) { error = L"The image is truncated."; return false; }
    std::vector<uint32_t> pending{0};
    std::vector<bool> seen(size / 4 + 1, false);
    while (!pending.empty()) {
        uint32_t at = pending.back();
        pending.pop_back();
        if (at + 14 > size || seen[at / 4]) continue;
        seen[at / 4] = true;
        uint16_t left, right;
        uint32_t start, length;
        std::memcpy(&left, &table[at], 2);
        std::memcpy(&right, &table[at + 2], 2);
        if (left == 0xFFFF && right == 0xFFFF) continue;  // sector padding
        std::memcpy(&start, &table[at + 4], 4);
        std::memcpy(&length, &table[at + 8], 4);
        uint8_t attributes = table[at + 12], name_length = table[at + 13];
        if (at + 14u + name_length > size) { error = L"A folder entry on the disc is damaged."; return false; }
        std::string name((const char*)&table[at + 14], name_length);
        if (left) pending.push_back((uint32_t)left * 4);
        if (right) pending.push_back((uint32_t)right * 4);
        if (!safe_name(name)) { error = L"The disc contains an unsafe file name."; return false; }
        std::string path = prefix + name;
        if (attributes & kDirectory) {
            directories_.push_back(path);
            if (!walk(start, length, path + "/", depth + 1, error)) return false;
        } else {
            uint64_t offset = base_ + (uint64_t)start * kSector;
            if (offset + length > image_size_) { error = L"The image is truncated: " + widen(path) + L" is incomplete."; return false; }
            files_.push_back({path, offset, length});
        }
    }
    return true;
}

uint64_t Xiso::total_bytes() const
{
    uint64_t total = 0;
    for (const auto& f : files_) total += f.size;
    return total;
}

const XisoFile* Xiso::find(const std::string& path) const
{
    for (const auto& f : files_)
        if (_stricmp(f.path.c_str(), path.c_str()) == 0) return &f;
    return nullptr;
}

bool Xiso::read(const XisoFile& file, std::vector<unsigned char>& out, std::wstring& error) const
{
    out.resize(file.size);
    if (file.size && !read_at(file.offset, out.data(), file.size)) { error = L"Could not read " + widen(file.path) + L"."; return false; }
    return true;
}

uint32_t Xiso::title_id() const
{
    const XisoFile* xbe = find("default.xbe");
    std::vector<unsigned char> data;
    std::wstring error;
    if (!xbe || xbe->size < 0x178 || !read(*xbe, data, error) || std::memcmp(data.data(), "XBEH", 4) != 0) return 0;
    uint32_t base, certificate;
    std::memcpy(&base, &data[0x104], 4);
    std::memcpy(&certificate, &data[0x118], 4);
    uint64_t at = (uint64_t)certificate - base + 8;
    if (certificate < base || at + 4 > data.size()) return 0;
    uint32_t id;
    std::memcpy(&id, &data[at], 4);
    return id;
}

bool Xiso::extract(const fs::path& target, const std::function<bool(uint64_t, uint64_t)>& progress, std::wstring& error) const
{
    const uint64_t total = total_bytes();
    uint64_t done = 0;
    std::vector<unsigned char> buffer(4 * 1024 * 1024);
    std::error_code ec;
    for (const auto& dir : directories_) fs::create_directories(target / fs::u8path(dir), ec);
    for (const auto& file : files_) {
        fs::path out = target / fs::u8path(file.path);
        fs::create_directories(out.parent_path(), ec);
        HANDLE handle = CreateFileW(out.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            error = L"Could not write " + out.wstring() + L" (error " + std::to_wstring(GetLastError()) + L").";
            return false;
        }
        bool ok = true;
        for (uint32_t written = 0; ok && written < file.size;) {
            uint32_t chunk = (uint32_t)std::min<uint64_t>(buffer.size(), file.size - written);
            DWORD put = 0;
            ok = read_at(file.offset + written, buffer.data(), chunk) &&
                 WriteFile(handle, buffer.data(), chunk, &put, nullptr) && put == chunk;
            written += chunk;
            done += chunk;
            if (ok && !progress(done, total)) { CloseHandle(handle); error = L"Cancelled."; return false; }
        }
        CloseHandle(handle);
        if (!ok) {
            error = L"Could not copy " + widen(file.path) + L". Is the disk full?";
            return false;
        }
        if (!file.size && !progress(done, total)) { error = L"Cancelled."; return false; }
    }
    return true;
}

}
