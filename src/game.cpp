#include "game.h"
#include "text.h"
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
// mingw-w64 vocabulary used by the pinned DX8 headers, as in the renderer.
typedef BOOL WINBOOL;
#ifndef __MSABI_LONG
#define __MSABI_LONG(value) value##L
#endif
#include <d3d8.h>

namespace launcher {

bool is_game_folder(const fs::path& folder)
{
    std::error_code ec;
    return !folder.empty() && fs::is_regular_file(folder / L"default.xbe", ec) && fs::is_regular_file(folder / kGameExe, ec);
}

fs::path launcher_folder()
{
    wchar_t path[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, path, (DWORD)std::size(path));
    return fs::path(std::wstring(path, n)).parent_path();
}

static fs::path preferences_path()
{
    PWSTR local = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) out = fs::path(local) / L"OpenXML1" / L"launcher.ini";
    CoTaskMemFree(local);
    return out;
}

std::string preference(const char* key)
{
    Ini ini;
    ini.load(preferences_path());
    return ini.get("Launcher", key);
}

void set_preference(const char* key, const std::string& value)
{
    fs::path path = preferences_path();
    if (path.empty()) return;
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    Ini ini;
    ini.load(path);
    ini.set("Launcher", key, value);
    ini.save(path);
}

fs::path find_game_folder()
{
    if (is_game_folder(launcher_folder())) return launcher_folder();
    fs::path saved = fs::u8path(preference("GameFolder"));
    return is_game_folder(saved) ? saved : fs::path();
}

static bool truthy(const std::string& value)
{
    return value == "1" || lower(widen(value)) == L"true";
}

bool modder_mode(const fs::path& game)
{
    Ini ini;
    ini.load(game / L"build.ini");
    return truthy(ini.get("BUILD", "modderMode"));
}

bool set_modder_mode(const fs::path& game, bool on, std::wstring& error)
{
    Ini ini;
    ini.load(game / L"build.ini");
    if (ini.has("BUILD", "modderMode") ? truthy(ini.get("BUILD", "modderMode")) == on : !on) return true;
    ini.set("BUILD", "modderMode", on ? "1" : "0");
    if (ini.save(game / L"build.ini")) return true;
    error = L"Could not write build.ini.";
    return false;
}

unsigned fsaa_modes(const fs::path& game)
{
    const unsigned everything = 1u | (1u << 2) | (1u << 4) | (1u << 8);
    HMODULE d3d8 = LoadLibraryW(L"d3d8.dll");
    if (!d3d8) return everything;
    using Create = IDirect3D8*(WINAPI*)(UINT);
    auto create = (Create)GetProcAddress(d3d8, "Direct3DCreate8");
    IDirect3D8* api = create ? create(D3D_SDK_VERSION) : nullptr;
    if (!api) { FreeLibrary(d3d8); return everything; }
    // The renderer honours build.ini graphicsAdapter; ask the same adapter.
    Ini ini;
    ini.load(game / L"build.ini");
    std::string wanted = ini.get("BUILD", "graphicsAdapter");
    UINT adapter = D3DADAPTER_DEFAULT;
    if (!wanted.empty() && lower(widen(wanted)) != L"auto") adapter = (UINT)strtoul(wanted.c_str(), nullptr, 10);
    if (adapter >= api->GetAdapterCount()) adapter = D3DADAPTER_DEFAULT;
    unsigned modes = 1;
    D3DDISPLAYMODE mode{};
    if (SUCCEEDED(api->GetAdapterDisplayMode(adapter, &mode))) {
        for (unsigned count : {2u, 4u, 8u}) {
            auto samples = (D3DMULTISAMPLE_TYPE)count;
            if (SUCCEEDED(api->CheckDeviceMultiSampleType(adapter, D3DDEVTYPE_HAL, mode.Format, TRUE, samples)) &&
                SUCCEEDED(api->CheckDeviceMultiSampleType(adapter, D3DDEVTYPE_HAL, D3DFMT_D24S8, TRUE, samples)))
                modes |= 1u << count;
        }
    } else {
        modes = everything;
    }
    api->Release();
    FreeLibrary(d3d8);
    return modes;
}

bool game_running(const fs::path& game)
{
    fs::path exe = lower((game / kGameExe).lexically_normal().wstring());
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{sizeof(entry)};
    bool found = false;
    for (BOOL more = Process32FirstW(snapshot, &entry); more && !found; more = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, kGameExe) != 0) continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (!process) continue;
        wchar_t path[MAX_PATH * 4];
        DWORD size = (DWORD)std::size(path);
        if (QueryFullProcessImageNameW(process, 0, path, &size) && fs::path(lower(std::wstring(path, size))).lexically_normal() == exe)
            found = true;
        CloseHandle(process);
    }
    CloseHandle(snapshot);
    return found;
}

bool start_game(const fs::path& game, void** process, std::wstring& error)
{
    // The environment override beats pc-settings.ini; the launcher's choice
    // should be the one that counts.
    SetEnvironmentVariableW(L"XML1_DX8_RESOLUTION", nullptr);
    std::wstring exe = (game / kGameExe).wstring();
    std::wstring command = L"\"" + exe + L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, game.c_str(), &startup, &info)) {
        error = L"Windows could not start " + exe + L" (error " + std::to_wstring(GetLastError()) + L").";
        return false;
    }
    CloseHandle(info.hThread);
    *process = info.hProcess;
    return true;
}

}
