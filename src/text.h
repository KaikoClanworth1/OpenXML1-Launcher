#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace launcher {

std::wstring widen(const std::string& utf8);
std::string narrow(const std::wstring& wide);
std::wstring lower(std::wstring text);

// A line-preserving INI editor. Comments, ordering and unrelated sections are
// kept exactly, so the launcher can change one key in build.ini without
// disturbing anything the game or the player put there. Section and key
// lookups ignore case, as build.ini's own parser does.
class Ini {
public:
    bool load(const std::filesystem::path& path);
    std::string get(const std::string& section, const std::string& key) const;
    bool has(const std::string& section, const std::string& key) const;
    void set(const std::string& section, const std::string& key, const std::string& value);
    void remove(const std::string& section, const std::string& key);  // the key's line; comments stay
    bool save(const std::filesystem::path& path) const;  // atomic replace
    // Every key = value line of a section, in file order.
    std::vector<std::pair<std::string, std::string>> section(const std::string& name) const;
private:
    struct Found { int line = -1; int section_end = -1; bool section = false; };
    Found find(const std::string& section, const std::string& key) const;
    std::vector<std::string> lines_;
    bool crlf_ = true;
};

}
