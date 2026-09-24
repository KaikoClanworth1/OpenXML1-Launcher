#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

// The OpenXML1 release: the zip published on GitHub that holds
// X-Men Legends.exe, build.ini and the PC menu files. The launcher installs it
// exactly as the release instructions say: extracted over the disc's files.
namespace launcher {
namespace fs = std::filesystem;

using Progress = std::function<bool(uint64_t done, uint64_t total)>;  // false cancels

struct ReleaseInfo {
    std::wstring tag, name, url;
    uint64_t size = 0;
};

// Asks GitHub for the newest OpenXML1 release and its Windows zip.
bool latest_release(ReleaseInfo& info, std::wstring& error);
bool download(const std::wstring& url, const fs::path& target, const Progress& progress, std::wstring& error);

// True when the zip holds X-Men Legends.exe, i.e. is an OpenXML1 release.
bool is_release_zip(const fs::path& zip, std::wstring& error);
bool extract_zip(const fs::path& zip, const fs::path& target, const Progress& progress, std::wstring& error);

constexpr const wchar_t* kReleasesPage = L"https://github.com/GTTeancum/OpenXML1xbox/releases";

}
