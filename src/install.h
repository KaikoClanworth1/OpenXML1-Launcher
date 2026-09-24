#pragma once
#include <filesystem>
#include <functional>
#include <string>

// Installs X-Men Legends from the player's own disc image, following the
// OpenXML1 release instructions step for step:
//   1. extract the Xbox filesystem from the ISO/XISO into the game folder
//   2. extract the OpenXML1 release zip over it, replacing the supplied files
//   3. the game's first launch then prepares the loose assets itself
// Step 3 is left to the game on purpose: its setup belongs to the game version
// that was just installed, so the launcher does not run a copy of its own.
namespace launcher {
namespace fs = std::filesystem;

struct InstallRequest {
    fs::path image;            // the player's ISO or XISO
    fs::path release_zip;      // an OpenXML1 release zip, or empty to download the latest
    fs::path target;           // the game folder
};

// progress(permille, what) returns false to cancel.
using InstallProgress = std::function<bool(int, const std::wstring&)>;

// Quick checks before starting, for the UI: is this an X-Men Legends image,
// how much space does it need. Empty string when fine.
std::wstring check_image(const fs::path& image, uint64_t* bytes_needed);

bool install_game(const InstallRequest& request, const InstallProgress& progress, std::wstring& error);

// A folder that already has a working install in it.
bool has_install(const fs::path& folder);

// OpenXML1 0.9b's release zip adds a sounds/eng folder with only two banks.
// Its presence makes the game look for every sound there, so the game is
// silent. repair_sounds folds it into sounds/zsds (see install.cpp).
bool sounds_need_repair(const fs::path& game);
unsigned repair_sounds(const fs::path& game, std::wstring& error);

}
