#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Modular install for the launcher's Mods tab.
//
// Each mod is a folder under <game>/mods/ with a mod.ini and either or both of
//   files/  copied over the game folder, same relative paths
//   merge/  XML data merged into the game's own file of the same path;
//           attributes on a fragment's root set those on the game file's root
//   append/ text added to the end of the game's file of the same path
// Merging is what lets several character mods share data/herostat.eng: each
// top-level entry (a <stats name="...">) replaces the game's entry of the same
// tag and name, or is appended. Text data is compiled to the binary form the
// game actually loads (herostat.eng -> herostat.engb) as it is installed.
//
// Every file the launcher writes is recorded, with the original backed up, so
// applying a different selection first puts the game back exactly as it was.
namespace launcher {
namespace fs = std::filesystem;

struct ModInfo {
    std::wstring folder;       // directory name under mods/, the mod's identity
    std::wstring name, author, version, description;
    bool character = false;    // Type = character
    unsigned files = 0, merges = 0, appends = 0;
    std::wstring problem;      // non-empty: the mod cannot be installed
    // [Import]: files taken from the player's own install of another game
    // (Game = its name, Detect = a file that identifies its folder), each as
    // "source path in that game = target path in this one".
    std::wstring import_game, import_detect;
    std::vector<std::pair<fs::path, fs::path>> imports;
    // [Copy]: "source = target", both in this game's own folder. The copy is a
    // new file the mod owns; a file of the same target under files\ replaces it.
    std::vector<std::pair<fs::path, fs::path>> game_copies;
    // [Settings]: build.ini [BUILD] keys the mod sets while it is installed.
    // Only keys mod_setting_allowed() accepts; uninstalling puts back each
    // key's previous value, or removes it if it was not there.
    std::vector<std::pair<std::string, std::string>> settings;
};

// The build.ini keys a mod may set, with the values each accepts.
bool mod_setting_allowed(const std::string& key, const std::string& value);

// Where the player's install of another game is, by the name a mod gives it.
// Imports from a game with no known folder are skipped.
void set_import_folder(const std::wstring& game, const fs::path& folder);
fs::path import_folder(const std::wstring& game);

struct ApplyResult {
    bool ok = false;
    std::wstring error;
    std::vector<std::wstring> conflicts;  // "path: A, then B (B wins)"
    unsigned written = 0;
    std::vector<std::wstring> skipped_imports;  // "Game: path" not copied
};

fs::path mods_dir(const fs::path& game);
std::vector<ModInfo> find_mods(const fs::path& game);

// The selection last applied, in install order.
std::vector<std::wstring> installed_mods(const fs::path& game);

// Files two or more selected mods both replace outright. Later mods win.
std::vector<std::wstring> find_conflicts(const fs::path& game, const std::vector<std::wstring>& enabled);

// Restore the game's original files, then install the selection in order.
ApplyResult apply_mods(const fs::path& game, const std::vector<std::wstring>& enabled);

// Merge the top-level entries of fragment into base. Exposed for tests.
bool merge_xml(const std::string& base, const std::string& fragment, std::string& out, std::string& error);

// Writes mods/README.txt describing the format when it is missing.
void ensure_mods_readme(const fs::path& game);
}
