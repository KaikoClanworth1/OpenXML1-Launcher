#include "release.h"
#include "text.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <vector>
#include "miniz.h"

namespace launcher {
namespace {

struct Internet {
    HINTERNET session = nullptr, connection = nullptr, request = nullptr;
    ~Internet()
    {
        if (request) WinHttpCloseHandle(request);
        if (connection) WinHttpCloseHandle(connection);
        if (session) WinHttpCloseHandle(session);
    }
};

// GET an https URL; body is streamed to sink. Redirects (GitHub sends
// downloads to its content host) are followed by WinHTTP itself.
bool get(const std::wstring& url, const std::function<bool(const char*, DWORD, uint64_t)>& sink, std::wstring& error)
{
    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256] = {}, path[4096] = {};
    parts.lpszHostName = host; parts.dwHostNameLength = (DWORD)std::size(host);
    parts.lpszUrlPath = path; parts.dwUrlPathLength = (DWORD)std::size(path);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        error = L"Unsupported address: " + url;
        return false;
    }
    Internet net;
    net.session = WinHttpOpen(L"OpenXML1-Launcher/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (net.session) net.connection = WinHttpConnect(net.session, host, parts.nPort, 0);
    if (net.connection) net.request = WinHttpOpenRequest(net.connection, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    const wchar_t* headers = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
    if (!net.request || !WinHttpSendRequest(net.request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(net.request, nullptr)) {
        error = L"Could not reach GitHub (error " + std::to_wstring(GetLastError()) + L"). Check the internet connection.";
        return false;
    }
    DWORD status = 0, bytes = sizeof(status);
    WinHttpQueryHeaders(net.request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) { error = L"GitHub answered with status " + std::to_wstring(status) + L"."; return false; }
    wchar_t length_text[32] = {};
    DWORD length_size = sizeof(length_text);
    uint64_t length = 0;
    if (WinHttpQueryHeaders(net.request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, length_text, &length_size, WINHTTP_NO_HEADER_INDEX))
        length = _wcstoui64(length_text, nullptr, 10);
    std::vector<char> buffer(256 * 1024);
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(net.request, buffer.data(), (DWORD)buffer.size(), &got)) {
            error = L"The download was interrupted (error " + std::to_wstring(GetLastError()) + L").";
            return false;
        }
        if (!got) break;
        if (!sink(buffer.data(), got, length)) { if (error.empty()) error = L"Cancelled."; return false; }
    }
    return true;
}

// The value of "key": "..." starting the search at `from`.
std::string json_string(const std::string& json, const std::string& key, size_t from, size_t* at = nullptr)
{
    size_t k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return {};
    size_t open = json.find('"', json.find(':', k) + 1);
    size_t close = json.find('"', open + 1);
    if (open == std::string::npos || close == std::string::npos) return {};
    if (at) *at = k;
    return json.substr(open + 1, close - open - 1);
}

bool safe_entry(const std::string& name)
{
    if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos) return false;
    size_t start = 0;
    while (start <= name.size()) {
        size_t end = name.find_first_of("/\\", start);
        std::string part = name.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

struct Zip {
    std::vector<char> bytes;
    mz_zip_archive archive{};
    bool open = false;
    ~Zip() { if (open) mz_zip_reader_end(&archive); }
    bool load(const fs::path& path, std::wstring& error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) { error = L"Could not read " + path.wstring() + L"."; return false; }
        bytes.assign(std::istreambuf_iterator<char>(in), {});
        open = mz_zip_reader_init_mem(&archive, bytes.data(), bytes.size(), 0);
        if (!open) error = path.filename().wstring() + L" is not a valid zip file.";
        return open;
    }
};

} // namespace

bool latest_release(ReleaseInfo& info, std::wstring& error)
{
    std::string json;
    if (!get(L"https://api.github.com/repos/GTTeancum/OpenXML1xbox/releases/latest",
             [&](const char* data, DWORD size, uint64_t) { json.append(data, size); return json.size() < 4 * 1024 * 1024; }, error))
        return false;
    info.tag = widen(json_string(json, "tag_name", 0));
    // The Windows build is the asset named ...-win64.zip.
    for (size_t at = 0;;) {
        size_t found = std::string::npos;
        std::string url = json_string(json, "browser_download_url", at, &found);
        if (url.empty()) break;
        at = found + 1;
        if (url.size() < 10 || url.compare(url.size() - 10, 10, "-win64.zip") != 0) continue;
        info.url = widen(url);
        info.name = widen(url.substr(url.rfind('/') + 1));
        // "size" precedes the download URL within the same asset object.
        size_t size_at = json.rfind("\"size\"", found);
        if (size_at != std::string::npos) info.size = strtoull(json.c_str() + json.find(':', size_at) + 1, nullptr, 10);
        return true;
    }
    error = L"The latest OpenXML1 release (" + info.tag + L") has no Windows zip.";
    return false;
}

bool download(const std::wstring& url, const fs::path& target, const Progress& progress, std::wstring& error)
{
    fs::path partial = target;
    partial += L".part";
    uint64_t done = 0;
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) { error = L"Could not write " + partial.wstring() + L"."; return false; }
        bool ok = get(url, [&](const char* data, DWORD size, uint64_t total) {
            out.write(data, size);
            done += size;
            return (bool)out && progress(done, total);
        }, error);
        out.close();
        if (!ok || !out) {
            std::error_code ec;
            fs::remove(partial, ec);
            if (error.empty()) error = L"Could not save the download. Is the disk full?";
            return false;
        }
    }
    if (!MoveFileExW(partial.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) { error = L"Could not save the download."; return false; }
    return true;
}

bool is_release_zip(const fs::path& path, std::wstring& error)
{
    Zip zip;
    if (!zip.load(path, error)) return false;
    if (mz_zip_reader_locate_file(&zip.archive, "X-Men Legends.exe", nullptr, 0) >= 0) return true;
    error = path.filename().wstring() + L" is not an OpenXML1 release: it has no X-Men Legends.exe.";
    return false;
}

bool extract_zip(const fs::path& path, const fs::path& target, const Progress& progress, std::wstring& error)
{
    Zip zip;
    if (!zip.load(path, error)) return false;
    mz_uint count = mz_zip_reader_get_num_files(&zip.archive);
    uint64_t total = 0, done = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(&zip.archive, i, &stat)) total += stat.m_uncomp_size;
    }
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip.archive, i, &stat)) { error = L"The release zip is damaged."; return false; }
        std::string name = stat.m_filename;
        if (!safe_entry(name)) { error = L"The release zip contains an unsafe path: " + widen(name); return false; }
        fs::path out = target / fs::u8path(name);
        std::error_code ec;
        if (mz_zip_reader_is_file_a_directory(&zip.archive, i)) { fs::create_directories(out, ec); continue; }
        fs::create_directories(out.parent_path(), ec);
        fs::path temp = out;
        temp += L".launcher-tmp";
        HANDLE handle = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) { error = L"Could not write " + out.wstring() + L"."; return false; }
        auto write = [](void* opaque, mz_uint64, const void* data, size_t size) -> size_t {
            DWORD put = 0;
            return WriteFile((HANDLE)opaque, data, (DWORD)size, &put, nullptr) ? put : 0;
        };
        bool ok = mz_zip_reader_extract_to_callback(&zip.archive, i, write, handle, 0);
        CloseHandle(handle);
        // Replacing an existing file: the release's copy wins, as its instructions say.
        if (!ok || !MoveFileExW(temp.c_str(), out.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            fs::remove(temp, ec);
            error = L"Could not install " + widen(name) + L" from the release. Is the game running?";
            return false;
        }
        done += stat.m_uncomp_size;
        if (!progress(done, total)) { error = L"Cancelled."; return false; }
    }
    return true;
}

}
