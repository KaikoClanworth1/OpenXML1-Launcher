#include "text.h"
#include <windows.h>
#include <fstream>
#include <cwctype>

namespace launcher {
namespace {
std::string trim(const std::string& s)
{
    auto a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
bool iequal(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}
// The value part of "key = value ; comment".
std::string strip_comment(const std::string& value)
{
    for (size_t i = 0; i < value.size(); ++i)
        if ((value[i] == ';' || value[i] == '#') && (i == 0 || value[i - 1] == ' ' || value[i - 1] == '\t'))
            return trim(value.substr(0, i));
    return trim(value);
}
}

std::wstring widen(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), out.data(), n);
    return out;
}

std::string narrow(const std::wstring& wide)
{
    if (wide.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring lower(std::wstring text)
{
    for (auto& c : text) c = (wchar_t)std::towlower(c);
    return text;
}

bool Ini::load(const std::filesystem::path& path)
{
    lines_.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), {});
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    crlf_ = text.find('\n') == std::string::npos || text.find("\r\n") != std::string::npos;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        std::string line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines_.push_back(line);
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return true;
}

Ini::Found Ini::find(const std::string& section, const std::string& key) const
{
    Found found;
    bool inside = false;
    for (int i = 0; i < (int)lines_.size(); ++i) {
        std::string line = trim(lines_[i]);
        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            if (inside) return found;
            inside = iequal(trim(line.substr(1, line.size() - 2)), section);
            if (inside) { found.section = true; found.section_end = i + 1; }
            continue;
        }
        if (!inside) continue;
        if (!line.empty() && line[0] != ';' && line[0] != '#') found.section_end = i + 1;
        auto eq = line.find('=');
        if (eq != std::string::npos && iequal(trim(line.substr(0, eq)), key) && found.line < 0) found.line = i;
    }
    return found;
}

std::vector<std::pair<std::string, std::string>> Ini::section(const std::string& name) const
{
    std::vector<std::pair<std::string, std::string>> out;
    bool inside = false;
    for (const auto& raw : lines_) {
        std::string line = trim(raw);
        if (!line.empty() && line.front() == '[' && line.back() == ']') { inside = iequal(trim(line.substr(1, line.size() - 2)), name); continue; }
        if (!inside || line.empty() || line[0] == ';' || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq != std::string::npos) out.push_back({trim(line.substr(0, eq)), strip_comment(line.substr(eq + 1))});
    }
    return out;
}

bool Ini::has(const std::string& section, const std::string& key) const { return find(section, key).line >= 0; }

std::string Ini::get(const std::string& section, const std::string& key) const
{
    auto found = find(section, key);
    if (found.line < 0) return {};
    const std::string& line = lines_[found.line];
    return strip_comment(line.substr(line.find('=') + 1));
}

void Ini::set(const std::string& section, const std::string& key, const std::string& value)
{
    auto found = find(section, key);
    if (found.line >= 0) {
        // Keep any trailing comment on the line.
        std::string& line = lines_[found.line];
        std::string rest = line.substr(line.find('=') + 1), comment;
        for (size_t i = 0; i < rest.size(); ++i)
            if ((rest[i] == ';' || rest[i] == '#') && i && (rest[i - 1] == ' ' || rest[i - 1] == '\t')) { comment = " " + rest.substr(i); break; }
        line = key + " = " + value + comment;
        return;
    }
    if (found.section) {
        lines_.insert(lines_.begin() + found.section_end, key + " = " + value);
        return;
    }
    if (!lines_.empty() && !trim(lines_.back()).empty()) lines_.push_back("");
    lines_.push_back("[" + section + "]");
    lines_.push_back(key + " = " + value);
}

void Ini::remove(const std::string& section, const std::string& key)
{
    auto found = find(section, key);
    if (found.line >= 0) lines_.erase(lines_.begin() + found.line);
}

bool Ini::save(const std::filesystem::path& path) const
{
    std::filesystem::path temp = path;
    temp += L".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        for (const auto& line : lines_) out << line << (crlf_ ? "\r\n" : "\n");
        out.close();
        if (!out) return false;
    }
    return MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

}
