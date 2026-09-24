#include "bundled.h"
#include "mods.h"
#include "text.h"
#include <fstream>
#include <map>

namespace launcher {

#include "bundled_mods.inc"

const std::vector<BundledFile>& bundled_files()
{
    static const std::vector<BundledFile> files(std::begin(bundled_table), std::end(bundled_table));
    return files;
}

namespace {
// The Version line of a mod.ini's text.
std::string version_of(const std::string& ini_text)
{
    size_t at = 0;
    while (at < ini_text.size()) {
        size_t end = ini_text.find('\n', at);
        std::string line = ini_text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? ini_text.size() : end + 1;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        if (lower(widen(key)) != L"version") continue;
        std::string value = line.substr(eq + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r") + 1);
        return value;
    }
    return {};
}
}

std::vector<std::wstring> install_bundled_mods(const fs::path& game)
{
    // Group the files by mod folder: the first component of each path.
    std::map<std::string, std::vector<const BundledFile*>> mods;
    for (const auto& file : bundled_files()) {
        std::string path = file.path;
        size_t slash = path.find_first_of("/\\");
        if (slash == std::string::npos) continue;
        mods[path.substr(0, slash)].push_back(&file);
    }
    std::vector<std::wstring> written;
    for (const auto& [folder, files] : mods) {
        fs::path target = mods_dir(game) / fs::u8path(folder);
        std::string bundled_version;
        for (const auto* file : files)
            if (fs::u8path(file->path).filename() == L"mod.ini") bundled_version = version_of(std::string(file->contents));
        std::ifstream existing(target / L"mod.ini", std::ios::binary);
        std::string installed_text((std::istreambuf_iterator<char>(existing)), {});
        if (existing.is_open() && version_of(installed_text) == bundled_version) continue;
        std::error_code ec;
        bool ok = true;
        for (const auto* file : files) {
            fs::path out = mods_dir(game) / fs::u8path(file->path);
            fs::create_directories(out.parent_path(), ec);
            std::ofstream stream(out, std::ios::binary | std::ios::trunc);
            stream.write(file->contents.data(), (std::streamsize)file->contents.size());
            ok = ok && (bool)stream;
        }
        if (ok) written.push_back(widen(folder));
    }
    return written;
}

}
