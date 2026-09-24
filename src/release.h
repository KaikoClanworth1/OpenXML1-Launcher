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

// ---- the launcher's own updates --------------------------------------------

struct LauncherRelease {
    std::wstring tag, notes, url, page;
    uint64_t size = 0;
};

// The newest release of the launcher itself (LAUNCHER_REPOSITORY).
bool latest_launcher(LauncherRelease& release, std::wstring& error);
// <0, 0, >0 comparing "v1.2.3"-style versions; a leading v is optional.
int compare_versions(const std::wstring& a, const std::wstring& b);
// Downloads the release's exe beside `exe`, checks it, and swaps it in.
// The running exe is renamed to "<exe>.old" (Windows allows renaming, not
// overwriting, a running program) and removed on the next start.
bool install_launcher_update(const LauncherRelease& release, const fs::path& exe, const Progress& progress, std::wstring& error);
// Swaps `replacement` in for the running `exe`; exposed for tests.
bool replace_running_exe(const fs::path& exe, const fs::path& replacement, std::wstring& error);
void remove_previous_launcher(const fs::path& exe);

}
