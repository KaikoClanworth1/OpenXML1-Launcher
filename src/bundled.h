#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Mods that ship inside the launcher (the repository's mods/ folder, compiled
// in at build time). They are written into the game's mods folder so they
// appear in the Mods list like any other mod.
namespace launcher {
namespace fs = std::filesystem;

struct BundledFile { const char* path; const char* contents; };
const std::vector<BundledFile>& bundled_files();

// Writes each bundled mod into <game>/mods/<folder> when the folder is missing
// or its mod.ini Version differs from the bundled one. Returns the folders written.
std::vector<std::wstring> install_bundled_mods(const fs::path& game);

}
