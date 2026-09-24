#pragma once
#include <filesystem>
#include <string>

namespace launcher {
namespace fs = std::filesystem;

constexpr const wchar_t* kGameExe = L"X-Men Legends.exe";

bool is_game_folder(const fs::path& folder);
fs::path launcher_folder();
// The launcher's own folder when it sits beside the game, else the one saved
// in %LOCALAPPDATA%\OpenXML1\launcher.ini. Empty when neither is a game folder.
fs::path find_game_folder();

// Launcher preferences, in %LOCALAPPDATA%\OpenXML1\launcher.ini.
std::string preference(const char* key);
void set_preference(const char* key, const std::string& value);

// build.ini [BUILD] keys the launcher offers.
bool modder_mode(const fs::path& game);
bool set_modder_mode(const fs::path& game, bool on, std::wstring& error);

// Bit n set = n-sample FSAA works on this GPU, checked the same way the DX8
// renderer checks it at startup (desktop format and D24S8 depth, windowed).
// Bit 0 is always set. All of 2/4/8 when D3D8 cannot be asked.
unsigned fsaa_modes(const fs::path& game);

bool game_running(const fs::path& game);
// Starts the game. On failure returns false with a message.
bool start_game(const fs::path& game, void** process, std::wstring& error);

}
