#include "install.h"
#include "game.h"
#include "mods.h"
#include "release.h"
#include "text.h"
#include "xiso.h"
#include <windows.h>
#include <cstdio>

namespace launcher {
namespace {

std::wstring megabytes(uint64_t bytes)
{
    wchar_t text[32];
    swprintf(text, 32, bytes >= (1ull << 30) ? L"%.1f GB" : L"%.0f MB",
             bytes >= (1ull << 30) ? bytes / 1073741824.0 : bytes / 1048576.0);
    return text;
}

uint64_t free_space(fs::path folder)
{
    std::error_code ec;
    while (!folder.empty() && !fs::exists(folder, ec)) folder = folder.parent_path();
    ULARGE_INTEGER available{};
    return GetDiskFreeSpaceExW(folder.c_str(), &available, nullptr, nullptr) ? available.QuadPart : 0;
}

// The disc's files plus room for first-run setup, which unpacks
// z/assetsfb.zip beside them (compressed data roughly doubles).
uint64_t space_needed(const Xiso& disc)
{
    const XisoFile* archive = disc.find("z/assetsfb.zip");
    return disc.total_bytes() + (archive ? archive->size * 3ull : 0) + (256ull << 20);
}

bool open_disc(const fs::path& image, Xiso& disc, std::wstring& error)
{
    if (!disc.open(image, error)) return false;
    uint32_t id = disc.title_id();
    if (id != kXml1TitleId) {
        wchar_t text[160];
        swprintf(text, 160, L"This disc is not X-Men Legends (its title ID is %08X; X-Men Legends is %08X).", id, kXml1TitleId);
        error = text;
        return false;
    }
    for (const char* needed : {"z/assetsfb.zip"})
        if (!disc.find(needed)) { error = L"The disc image is incomplete: it has no " + widen(needed) + L"."; return false; }
    return true;
}

} // namespace

// OpenXML1 0.9b's zip ships sounds/eng holding only the beta Bishop and
// Sunfire banks. The game routes every sounds/zsds/ request to sounds/<audio
// language>/ as soon as that folder exists (src/asset_routes.c), so a partial
// sounds/eng silences every other sound. When sounds/eng lacks banks the disc
// has, fold it into sounds/zsds: the release's banks still replace the disc's
// copies, exactly as the routing intended, and everything else is found.
bool sounds_need_repair(const fs::path& game)
{
    const fs::path language = game / L"sounds" / L"eng", disc = game / L"sounds" / L"zsds";
    std::error_code ec;
    if (!fs::is_directory(language, ec) || !fs::is_directory(disc, ec)) return false;
    // A complete language folder is a real translation; leave it alone.
    for (auto it = fs::recursive_directory_iterator(disc, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->is_regular_file(ec) && !fs::exists(language / fs::relative(it->path(), disc), ec)) return true;
    return false;
}

unsigned repair_sounds(const fs::path& game, std::wstring& error)
{
    const fs::path language = game / L"sounds" / L"eng", disc = game / L"sounds" / L"zsds";
    std::error_code ec;
    if (!sounds_need_repair(game)) return 0;
    unsigned moved = 0;
    std::vector<fs::path> files;
    for (auto it = fs::recursive_directory_iterator(language, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->is_regular_file(ec)) files.push_back(it->path());
    for (const auto& file : files) {
        fs::path target = disc / fs::relative(file, language);
        fs::create_directories(target.parent_path(), ec);
        if (!MoveFileExW(file.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            error = L"Could not move " + file.wstring() + L" into sounds\\zsds.";
            return moved;
        }
        ++moved;
    }
    fs::remove_all(language, ec);
    if (fs::exists(language)) error = L"Could not remove the partial sounds\\eng folder; the game would play no sound.";
    return moved;
}

bool has_install(const fs::path& folder) { return is_game_folder(folder); }

std::wstring check_image(const fs::path& image, uint64_t* bytes_needed)
{
    Xiso disc;
    std::wstring error;
    if (!open_disc(image, disc, error)) return error;
    if (bytes_needed) *bytes_needed = space_needed(disc);
    return {};
}

bool install_game(const InstallRequest& request, const InstallProgress& progress, std::wstring& error)
{
    Xiso disc;
    if (!progress(0, L"Reading the disc image...") ) { error = L"Cancelled."; return false; }
    if (!open_disc(request.image, disc, error)) return false;

    uint64_t needed = space_needed(disc), available = free_space(request.target);
    if (available && available < needed) {
        error = L"Not enough free space: the game needs about " + megabytes(needed) + L" and the drive has " + megabytes(available) + L".";
        return false;
    }
    std::error_code ec;
    fs::create_directories(request.target, ec);
    if (!fs::is_directory(request.target)) { error = L"Could not create " + request.target.wstring() + L"."; return false; }

    // Installed mods would be overwritten by the disc's originals and then
    // restored on top of them later; take them out first.
    if (!installed_mods(request.target).empty()) {
        auto removed = apply_mods(request.target, {});
        if (!removed.ok) { error = L"Could not remove the installed mods first: " + removed.error; return false; }
    }

    // 1. The disc: 0-80%.
    const bool downloading = request.release_zip.empty();
    const int disc_end = downloading ? 700 : 800;
    std::wstring what = L"Copying the game from the disc image (" + megabytes(disc.total_bytes()) + L")...";
    if (!disc.extract(request.target, [&](uint64_t done, uint64_t total) {
            return progress((int)(total ? done * disc_end / total : 0), what);
        }, error))
        return false;

    // 2. The release: download (70-90%) when none was given, then extract (90-98%).
    fs::path zip = request.release_zip;
    fs::path downloaded;
    if (downloading) {
        ReleaseInfo release;
        if (!progress(disc_end, L"Finding the latest OpenXML1 release...")) { error = L"Cancelled."; return false; }
        if (!latest_release(release, error)) return false;
        downloaded = request.target / release.name;
        std::wstring label = L"Downloading OpenXML1 " + release.tag + L" (" + megabytes(release.size) + L")...";
        if (!download(release.url, downloaded, [&](uint64_t done, uint64_t total) {
                if (!total) total = release.size;
                return progress(disc_end + (int)(total ? done * 200 / total : 0), label);
            }, error))
            return false;
        zip = downloaded;
    }
    if (!is_release_zip(zip, error)) return false;
    bool extracted = extract_zip(zip, request.target, [&](uint64_t done, uint64_t total) {
        return progress(900 + (int)(total ? done * 80 / total : 0), L"Installing OpenXML1...");
    }, error);
    if (!downloaded.empty()) fs::remove(downloaded, ec);
    if (!extracted) return false;
    progress(985, L"Arranging sound banks...");
    repair_sounds(request.target, error);
    if (!error.empty()) return false;

    // 3. Check the result has what the release instructions require.
    progress(990, L"Checking the installation...");
    for (const wchar_t* needed : {L"default.xbe", L"X-Men Legends.exe", L"build.ini", L"z/assetsfb.zip"})
        if (!fs::is_regular_file(request.target / needed)) { error = L"The installation is missing " + std::wstring(needed) + L"."; return false; }
    for (const wchar_t* needed : {L"media", L"movies", L"sounds"})
        if (!fs::is_directory(request.target / needed)) { error = L"The installation is missing the " + std::wstring(needed) + L" folder."; return false; }
    progress(1000, L"Installed.");
    return true;
}

}
