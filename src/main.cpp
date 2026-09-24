// X-Men Legends launcher: Play, Settings, Mods and Install tabs.
//
// An add-on beside OpenXML1, never a change to it. Everything it changes goes
// through the files the game itself reads: pc-settings.ini through the game's
// own loader, validator and saver (external/OpenXML1/src/pc_controls.cpp),
// build.ini one key at a time, game data through the mod ledger in mods.cpp,
// and a new install exactly as the OpenXML1 release instructions lay it out.
#include "camera.h"
#include "bundled.h"
#include "game.h"
#include "install.h"
#include "release.h"
#include "version.h"
#include "mods.h"
#include "text.h"
#include "pc_controls.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <wincodec.h>
#include <cstring>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using namespace launcher;

namespace {

enum Page { kPlay, kSettings, kMods, kInstall, kPages, kMain = -1 };

enum Id {
    IDC_TABS = 100, IDC_STATUS, IDC_SAVE, IDC_PLAY,
    IDC_WINDOWED, IDC_FULLSCREEN, IDC_RESOLUTION, IDC_CLOSE_ON_START, IDC_FOLDER, IDC_CHANGE_FOLDER, IDC_PLAY_SUMMARY,
    IDC_FSAA, IDC_KEYBOARD, IDC_KEYBOARD_PLAYER, IDC_SENSITIVITY, IDC_SENSITIVITY_VALUE, IDC_INVERT, IDC_FLIP, IDC_CONTROLLERS,
    IDC_SHAKE, IDC_DEFAULTS,
    IDC_MOD_LIST, IDC_MOD_INFO, IDC_MODDER, IDC_OPEN_MODS, IDC_REFRESH_MODS, IDC_APPLY_MODS,
    IDC_IMAGE, IDC_BROWSE_IMAGE, IDC_DOWNLOAD, IDC_LOCAL_ZIP, IDC_ZIP, IDC_BROWSE_ZIP, IDC_TARGET, IDC_BROWSE_TARGET,
    IDC_PROGRESS, IDC_INSTALL_STATUS, IDC_INSTALL,
    IDC_VERSION, IDC_CHECK_UPDATES,
};

constexpr UINT WM_GAME_EXITED = WM_APP + 1;
constexpr UINT WM_INSTALL_PROGRESS = WM_APP + 2;  // wParam permille, lParam new std::wstring
constexpr UINT WM_INSTALL_DONE = WM_APP + 3;      // wParam ok, lParam new std::wstring (error)
constexpr UINT WM_UPDATE_CHECKED = WM_APP + 4;    // wParam manual, lParam new UpdateCheck
constexpr UINT WM_UPDATE_DONE = WM_APP + 5;       // wParam ok, lParam new std::wstring (error)

// Mods can be switched off for a release; installing them has been tested in
// the game with the bundled early-xmen-xtraction mod.
constexpr bool kModsEnabled = true;

struct UpdateCheck {
    bool ok = false;
    LauncherRelease release;
    std::wstring error;
};
constexpr int kClientW = 600, kClientH = 476;

struct Control { HWND hwnd; Page page; int x, y, w, h; };

HINSTANCE g_instance;
HWND g_main, g_tabs, g_pages[kPages];
std::vector<Control> g_controls;
HFONT g_font, g_bold;
UINT g_dpi = 96;
Page g_page = kPlay;

fs::path g_game;
Xml1PcSettings g_settings;
std::vector<ModInfo> g_mods;
unsigned g_fsaa_modes = 1;
bool g_populating = false;
HANDLE g_process;
fs::path g_capture;  // --capture: render each tab to PNG here and exit
bool g_installing = false;
std::atomic<bool> g_cancel_install{false};
bool g_updating = false;

int px(int logical) { return MulDiv(logical, (int)g_dpi, 96); }
HWND item(int id)
{
    for (const auto& c : g_controls) if (GetDlgCtrlID(c.hwnd) == id) return c.hwnd;
    return nullptr;
}

HWND add(Page page, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0, DWORD ex = 0)
{
    HWND parent = page == kMain ? g_main : g_pages[page];
    HWND hwnd = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
                                (HMENU)(INT_PTR)id, g_instance, nullptr);
    g_controls.push_back({hwnd, page, x, y, w, h});
    return hwnd;
}
HWND label(Page page, const wchar_t* text, int x, int y, int w, int h = 20, int id = 0, DWORD style = 0)
{
    return add(page, WC_STATICW, text, SS_LEFT | style, x, y, w, h, id);
}
HWND group(Page page, const wchar_t* text, int x, int y, int w, int h)
{
    return add(page, WC_BUTTONW, text, BS_GROUPBOX, x, y, w, h);
}
HWND button(Page page, const wchar_t* text, int x, int y, int w, int h, int id, DWORD style = BS_PUSHBUTTON)
{
    return add(page, WC_BUTTONW, text, style | WS_TABSTOP, x, y, w, h, id);
}
HWND combo(Page page, int x, int y, int w, int id)
{
    return add(page, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, x, y, w, 240, id);
}
HWND edit(Page page, int x, int y, int w, int id, DWORD style = 0)
{
    return add(page, WC_EDITW, L"", ES_AUTOHSCROLL | WS_TABSTOP | style, x, y, w, 24, id, WS_EX_CLIENTEDGE);
}
std::wstring text_of(int id)
{
    HWND hwnd = item(id);
    std::wstring text(GetWindowTextLengthW(hwnd) + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), (int)text.size());
    text.resize(wcslen(text.c_str()));
    return text;
}
void combo_add(HWND hwnd, const wchar_t* text, LPARAM data)
{
    int at = (int)SendMessageW(hwnd, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessageW(hwnd, CB_SETITEMDATA, at, data);
}
LPARAM combo_data(HWND hwnd)
{
    int at = (int)SendMessageW(hwnd, CB_GETCURSEL, 0, 0);
    return at < 0 ? -1 : SendMessageW(hwnd, CB_GETITEMDATA, at, 0);
}
void combo_select(HWND hwnd, LPARAM data)
{
    int count = (int)SendMessageW(hwnd, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i)
        if (SendMessageW(hwnd, CB_GETITEMDATA, i, 0) == data) { SendMessageW(hwnd, CB_SETCURSEL, i, 0); return; }
    SendMessageW(hwnd, CB_SETCURSEL, 0, 0);
}
bool checked(int id) { return Button_GetCheck(item(id)) == BST_CHECKED; }
void check(int id, bool on) { Button_SetCheck(item(id), on ? BST_CHECKED : BST_UNCHECKED); }

void status(const std::wstring& text) { SetWindowTextW(item(IDC_STATUS), text.c_str()); }
int ask(const std::wstring& text, UINT flags) { return MessageBoxW(g_main, text.c_str(), L"X-Men Legends", flags); }

// ---- layout ----------------------------------------------------------------

void make_fonts()
{
    if (g_font) DeleteObject(g_font);
    if (g_bold) DeleteObject(g_bold);
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, g_dpi);
    g_font = CreateFontIndirectW(&metrics.lfMessageFont);
    LOGFONTW bold = metrics.lfMessageFont;
    bold.lfWeight = FW_BOLD;
    bold.lfHeight = bold.lfHeight * 5 / 4;
    g_bold = CreateFontIndirectW(&bold);
}

void layout()
{
    MoveWindow(g_tabs, px(10), px(10), px(kClientW - 20), px(410), TRUE);
    SendMessageW(g_tabs, WM_SETFONT, (WPARAM)g_font, TRUE);
    RECT page{px(10), px(10), px(kClientW - 10), px(420)};
    TabCtrl_AdjustRect(g_tabs, FALSE, &page);
    for (auto hwnd : g_pages)
        SetWindowPos(hwnd, HWND_TOP, page.left, page.top, page.right - page.left, page.bottom - page.top, SWP_NOACTIVATE);
    for (const auto& c : g_controls) {
        MoveWindow(c.hwnd, px(c.x), px(c.y), px(c.w), px(c.h), TRUE);
        SendMessageW(c.hwnd, WM_SETFONT, (WPARAM)(GetDlgCtrlID(c.hwnd) == IDC_PLAY ? g_bold : g_font), TRUE);
    }
    HWND list = item(IDC_MOD_LIST);
    if (list) {
        ListView_SetColumnWidth(list, 0, px(300));
        ListView_SetColumnWidth(list, 1, px(80));
        ListView_SetColumnWidth(list, 2, px(130));
    }
}

void show_page(Page page)
{
    g_page = page;
    for (int i = 0; i < kPages; ++i) ShowWindow(g_pages[i], i == page ? SW_SHOW : SW_HIDE);
    TabCtrl_SetCurSel(g_tabs, page);
}

// ---- settings --------------------------------------------------------------

fs::path settings_path() { return g_game / L"pc-settings.ini"; }

void load_settings()
{
    char error[256] = {};
    if (xml1_pc_settings_load(settings_path().u8string().c_str(), &g_settings, error, sizeof(error))) return;
    xml1_pc_settings_defaults(&g_settings);
    int choice = ask(L"The game's settings file could not be read:\n\n    " + widen(error) +
                     L"\n\nThe game will not start with it as it is. Replace it with the default settings?"
                     L" The current file is kept as pc-settings.ini.bad.", MB_YESNO | MB_ICONWARNING);
    if (choice == IDYES) {
        fs::path bad = settings_path(); bad += L".bad";
        MoveFileExW(settings_path().c_str(), bad.c_str(), MOVEFILE_REPLACE_EXISTING);
        xml1_pc_settings_save(settings_path().u8string().c_str(), &g_settings, error, sizeof(error));
    }
}

void settings_to_ui()
{
    const auto& s = g_settings;
    check(IDC_WINDOWED, !s.fullscreen);
    check(IDC_FULLSCREEN, s.fullscreen != 0);
    combo_select(item(IDC_RESOLUTION), MAKELPARAM(s.width, s.height));
    combo_select(item(IDC_FSAA), s.fsaa);
    check(IDC_KEYBOARD, s.keyboard_enabled != 0);
    combo_select(item(IDC_KEYBOARD_PLAYER), s.keyboard_player);
    SendMessageW(item(IDC_SENSITIVITY), TBM_SETPOS, TRUE, s.mouse_sensitivity);
    SetWindowTextW(item(IDC_SENSITIVITY_VALUE), (std::to_wstring(s.mouse_sensitivity) + L"%").c_str());
    check(IDC_INVERT, s.invert_camera_y != 0);
    check(IDC_FLIP, camera_flipped(s, 0));
    combo_select(item(IDC_CONTROLLERS), s.separate_controllers);
    check(IDC_SHAKE, s.view_shake != 0);
    EnableWindow(item(IDC_KEYBOARD_PLAYER), s.keyboard_enabled != 0);
}

Xml1PcSettings settings_from_ui()
{
    Xml1PcSettings s = g_settings;  // key and button bindings stay as they are
    s.fullscreen = checked(IDC_FULLSCREEN);
    LPARAM resolution = combo_data(item(IDC_RESOLUTION));
    s.width = LOWORD(resolution);
    s.height = HIWORD(resolution);
    s.fsaa = (uint32_t)combo_data(item(IDC_FSAA));
    s.keyboard_enabled = checked(IDC_KEYBOARD);
    s.keyboard_player = (uint32_t)combo_data(item(IDC_KEYBOARD_PLAYER));
    s.mouse_sensitivity = (uint32_t)SendMessageW(item(IDC_SENSITIVITY), TBM_GETPOS, 0, 0);
    s.invert_camera_y = checked(IDC_INVERT);
    set_camera_flipped(s, checked(IDC_FLIP));
    s.separate_controllers = (uint32_t)combo_data(item(IDC_CONTROLLERS));
    s.view_shake = checked(IDC_SHAKE);
    return s;
}

bool settings_dirty()
{
    Xml1PcSettings s = settings_from_ui();
    return std::memcmp(&s, &g_settings, sizeof(s)) != 0 || checked(IDC_MODDER) != modder_mode(g_game);
}

bool save_settings()
{
    if (g_game.empty()) return false;
    Xml1PcSettings s = settings_from_ui();
    char error[256] = {};
    if (!xml1_pc_settings_save(settings_path().u8string().c_str(), &s, error, sizeof(error))) {
        ask(L"The settings were not saved:\n\n    " + widen(error), MB_OK | MB_ICONERROR);
        return false;
    }
    g_settings = s;
    std::wstring problem;
    if (!set_modder_mode(g_game, checked(IDC_MODDER), problem)) {
        ask(problem, MB_OK | MB_ICONERROR);
        return false;
    }
    set_preference("CloseOnStart", checked(IDC_CLOSE_ON_START) ? "1" : "0");
    return true;
}

// ---- mods ------------------------------------------------------------------

std::vector<std::wstring> selected_mods()
{
    std::vector<std::wstring> out;
    HWND list = item(IDC_MOD_LIST);
    int count = ListView_GetItemCount(list);
    // Characters first, then other mods, each alphabetical: the order shown.
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < count; ++i) {
            LVITEMW lv{LVIF_PARAM};
            lv.iItem = i;
            ListView_GetItem(list, &lv);
            const auto& mod = g_mods[lv.lParam];
            if (mod.character == (pass == 0) && ListView_GetCheckState(list, i)) out.push_back(mod.folder);
        }
    return out;
}

void describe_mod(int index)
{
    HWND info = item(IDC_MOD_INFO);
    if (g_mods.empty()) {
        SetWindowTextW(info, L"No mods yet. Put each mod in its own folder under mods\\ in the game folder; "
                             L"mods\\README.txt describes the layout.");
        return;
    }
    if (index < 0 || index >= (int)g_mods.size()) { SetWindowTextW(info, L"Tick the mods to install. They are installed when you press Play or Apply mods."); return; }
    const auto& mod = g_mods[index];
    std::wstring text = mod.description.empty() ? mod.name : mod.description;
    if (!mod.problem.empty()) text = L"Cannot be installed: " + mod.problem;
    else {
        std::wstring counts;
        auto count = [&](unsigned n, const wchar_t* what) {
            if (n) counts += (counts.empty() ? L"" : L", ") + std::to_wstring(n) + what;
        };
        count(mod.files, L" replaced");
        count(mod.merges, L" merged");
        count(mod.appends, L" appended");
        if (!counts.empty()) text += L"  (files: " + counts + L")";
    }
    SetWindowTextW(info, text.c_str());
}

void mods_status()
{
    if (!kModsEnabled) {
        SetWindowTextW(item(IDC_PLAY_SUMMARY), L"Mods: coming in a later launcher release.");
        return;
    }
    auto want = selected_mods(), have = installed_mods(g_game);
    std::wstring text;
    if (want == have) text = have.empty() ? L"No mods installed." : std::to_wstring(have.size()) + L" mod(s) installed.";
    else text = L"Mod changes will be installed when you press Play.";
    SetWindowTextW(item(IDC_PLAY_SUMMARY), text.c_str());
}

void load_mods()
{
    if (!kModsEnabled) {
        // Nothing is read from or written to the game folder for mods.
        SetWindowTextW(item(IDC_MOD_INFO), L"Mods are switched off in this release while installing them is tested "
                                            L"in the game. They will be turned on in a later launcher update.");
        mods_status();
        return;
    }
    ensure_mods_readme(g_game);
    // Mods that ship with the launcher appear in the list like any other.
    auto refreshed = install_bundled_mods(g_game);
    for (const auto& mod : find_mods(g_game))
        if (!mod.import_game.empty()) {
            fs::path known = fs::u8path(preference(("Import." + narrow(mod.import_game)).c_str()));
            if (!known.empty()) set_import_folder(mod.import_game, known);
        }
    auto previously = installed_mods(g_game);
    g_mods = find_mods(g_game);
    auto installed = installed_mods(g_game);
    HWND list = item(IDC_MOD_LIST);
    g_populating = true;
    ListView_DeleteAllItems(list);
    for (size_t i = 0; i < g_mods.size(); ++i) {
        const auto& mod = g_mods[i];
        LVITEMW lv{LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID};
        lv.iItem = (int)i;
        lv.pszText = const_cast<wchar_t*>(mod.name.c_str());
        lv.lParam = (LPARAM)i;
        lv.iGroupId = mod.character ? 1 : 2;
        int at = ListView_InsertItem(list, &lv);
        ListView_SetItemText(list, at, 1, const_cast<wchar_t*>(mod.version.c_str()));
        ListView_SetItemText(list, at, 2, const_cast<wchar_t*>(mod.author.c_str()));
        bool on = mod.problem.empty() && std::find(installed.begin(), installed.end(), mod.folder) != installed.end();
        ListView_SetCheckState(list, at, on);
    }
    g_populating = false;
    describe_mod(-1);
    mods_status();
    // A ticked mod that the launcher just refreshed is reinstalled straight
    // away, so the game never keeps playing an older version of it.
    bool stale = false;
    for (const auto& folder : refreshed)
        if (std::find(previously.begin(), previously.end(), folder) != previously.end()) stale = true;
    if (stale && g_capture.empty()) {
        if (game_running(g_game)) {
            status(L"A mod was updated. Close the game and press Apply mods to install the new version.");
        } else {
            auto result = apply_mods(g_game, previously);
            status(result.ok ? L"Installed the updated version of your mods." : L"An updated mod could not be installed: " + result.error);
            mods_status();
        }
    }
}

fs::path pick(bool folder, const wchar_t* title, const COMDLG_FILTERSPEC* filters, UINT filter_count);

// Mods can take files from the player's own copy of another game ([Import]
// in mod.ini). Its folder is remembered; a ticked mod whose game has not been
// located yet asks once. Declining installs the mod without those files.
void locate_import_games(const std::vector<std::wstring>& want)
{
    for (const auto& mod : g_mods) {
        if (mod.imports.empty() || mod.import_game.empty()) continue;
        if (std::find(want.begin(), want.end(), mod.folder) == want.end()) continue;
        const std::string key = "Import." + narrow(mod.import_game);
        fs::path known = fs::u8path(preference(key.c_str()));
        auto usable = [&](const fs::path& folder) {
            std::error_code ec;
            return !folder.empty() && (mod.import_detect.empty() || fs::is_regular_file(folder / mod.import_detect, ec));
        };
        if (usable(known)) { set_import_folder(mod.import_game, known); continue; }
        if (!import_folder(mod.import_game).empty()) continue;  // located earlier in this session
        std::wstring text = mod.name + L" can use " + std::to_wstring(mod.imports.size()) + L" file(s) from your own copy of " +
                            mod.import_game + L". Show the launcher where it is installed?\n\nChoose No to install the mod without them.";
        if (ask(text, MB_YESNO | MB_ICONQUESTION) != IDYES) continue;
        for (;;) {
            fs::path folder = pick(true, (L"Choose the folder where " + mod.import_game + L" is installed").c_str(), nullptr, 0);
            if (folder.empty()) break;
            if (usable(folder)) {
                set_import_folder(mod.import_game, folder);
                set_preference(key.c_str(), folder.u8string());
                break;
            }
            ask(L"That folder does not contain " + mod.import_detect + L".", MB_OK | MB_ICONWARNING);
        }
    }
}

bool apply_selected_mods(bool only_if_changed)
{
    if (g_game.empty()) return false;
    auto want = selected_mods();
    if (only_if_changed && want == installed_mods(g_game)) return true;
    if (game_running(g_game)) {
        ask(L"Close the game before changing mods.", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    auto conflicts = find_conflicts(g_game, want);
    if (!conflicts.empty()) {
        std::wstring text = L"Some of the ticked mods replace the same files:\n\n";
        for (size_t i = 0; i < conflicts.size() && i < 12; ++i) text += L"    " + conflicts[i] + L"\n";
        if (conflicts.size() > 12) text += L"    ...and " + std::to_wstring(conflicts.size() - 12) + L" more\n";
        text += L"\nInstall them anyway?";
        if (ask(text, MB_YESNO | MB_ICONWARNING) != IDYES) return false;
    }
    status(L"Installing mods...");
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    locate_import_games(want);
    auto result = apply_mods(g_game, want);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    if (!result.ok) {
        status(L"Mods were not installed.");
        ask(L"The mods could not be installed:\n\n" + result.error, MB_OK | MB_ICONERROR);
        load_mods();
        return false;
    }
    status(want.empty() ? L"The game's original files are in place." : L"Installed " + std::to_wstring(want.size()) + L" mod(s).");
    if (!result.skipped_imports.empty())
        status(L"Installed " + std::to_wstring(want.size()) + L" mod(s), without " + std::to_wstring(result.skipped_imports.size()) +
               L" file(s) from another game. Tick the mod again to locate that game.");
    mods_status();
    return true;
}

// ---- play ------------------------------------------------------------------

void play()
{
    if (g_process || g_installing) return;
    if (g_game.empty()) {
        show_page(kInstall);
        status(L"Install the game first, or choose an existing game folder on the Play tab.");
        return;
    }
    if (game_running(g_game)) { ask(L"The game is already running.", MB_OK | MB_ICONINFORMATION); return; }
    if (!save_settings() || (kModsEnabled && !apply_selected_mods(true))) return;
    void* process = nullptr;
    std::wstring error;
    if (!start_game(g_game, &process, error)) { ask(error, MB_OK | MB_ICONERROR); return; }
    if (checked(IDC_CLOSE_ON_START)) { CloseHandle(process); DestroyWindow(g_main); return; }
    g_process = process;
    EnableWindow(item(IDC_PLAY), FALSE);
    status(L"The game is running.");
    std::thread([hwnd = g_main, process] {
        WaitForSingleObject(process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        PostMessageW(hwnd, WM_GAME_EXITED, code, 0);
    }).detach();
}

void game_exited(DWORD code)
{
    CloseHandle(g_process);
    g_process = nullptr;
    EnableWindow(item(IDC_PLAY), TRUE);
    // The in-game Options menu may have changed the same file.
    load_settings();
    settings_to_ui();
    if (code == 0) { status(L"Ready."); return; }
    status(L"The game stopped with code " + std::to_wstring(code) + L".");
    if (code == 4)
        ask(L"The game stopped at startup because of a setup or configuration problem.\n\nDetails are in "
            L"build\\game-errors.log in the game folder.", MB_OK | MB_ICONWARNING);
}

// ---- folder ----------------------------------------------------------------

// A folder (pick_folder) or file (pick_file) chosen in the standard dialog.
fs::path pick(bool folder, const wchar_t* title, const COMDLG_FILTERSPEC* filters, UINT filter_count)
{
    fs::path chosen;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return chosen;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    dialog->SetTitle(title);
    if (filters) dialog->SetFileTypes(filter_count, filters);
    IShellItem* result = nullptr;
    PWSTR path = nullptr;
    if (SUCCEEDED(dialog->Show(g_main)) && SUCCEEDED(dialog->GetResult(&result)) && SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        chosen = path;
        CoTaskMemFree(path);
    }
    if (result) result->Release();
    dialog->Release();
    return chosen;
}

fs::path pick_game_folder()
{
    for (;;) {
        fs::path folder = pick(true, L"Choose the folder that holds X-Men Legends.exe and default.xbe", nullptr, 0);
        if (folder.empty() || is_game_folder(folder)) return folder;
        ask(L"That folder does not have both X-Men Legends.exe and default.xbe in it. "
            L"To set the game up from your disc image, use the Install tab.", MB_OK | MB_ICONWARNING);
    }
}

void use_game_folder(const fs::path& folder)
{
    g_game = folder;
    if (folder != launcher_folder() && g_capture.empty()) set_preference("GameFolder", folder.u8string());
    SetWindowTextW(item(IDC_FOLDER), folder.wstring().c_str());
    g_fsaa_modes = fsaa_modes(g_game);
    HWND fsaa = item(IDC_FSAA);
    SendMessageW(fsaa, CB_RESETCONTENT, 0, 0);
    combo_add(fsaa, L"Off", 0);
    for (unsigned n : {2u, 4u, 8u})
        if (g_fsaa_modes & (1u << n)) combo_add(fsaa, (std::to_wstring(n) + L"x").c_str(), n);
    load_settings();
    // A saved FSAA level this GPU cannot do would stop the renderer at startup.
    if (!(g_fsaa_modes & (1u << g_settings.fsaa))) g_settings.fsaa = 0;
    settings_to_ui();
    check(IDC_MODDER, modder_mode(g_game));
    load_mods();
    status(L"Ready.");
    if (g_capture.empty() && sounds_need_repair(g_game) && !game_running(g_game) &&
        ask(L"This installation will play without sound.\n\nThe OpenXML1 release added a sounds\\eng folder that "
            L"holds only two sound banks, and while that folder exists the game looks for every sound in it. "
            L"Move those two banks in with the disc's sounds (sounds\\zsds) so the game finds them all?",
            MB_YESNO | MB_ICONWARNING) == IDYES) {
        std::wstring error;
        unsigned moved = repair_sounds(g_game, error);
        if (!error.empty()) ask(error, MB_OK | MB_ICONERROR);
        else status(L"Sound fixed: moved " + std::to_wstring(moved) + L" sound bank(s) into sounds\\zsds.");
    }
}


// ---- install -----------------------------------------------------------------

// True when the folder holds nothing but this launcher.
bool only_launcher_in(const fs::path& folder)
{
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) return false;
    wchar_t self[MAX_PATH * 4];
    fs::path me = fs::path(std::wstring(self, GetModuleFileNameW(nullptr, self, (DWORD)std::size(self)))).filename();
    for (const auto& entry : fs::directory_iterator(folder, ec))
        if (lower(entry.path().filename().wstring()) != lower(me.wstring())) return false;
    return true;
}

// Beside the launcher when it was put in a folder of its own, so the game and
// launcher end up together; otherwise a Games folder.
fs::path default_install_folder()
{
    if (only_launcher_in(launcher_folder())) return launcher_folder();
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    return fs::path(profile) / L"Games" / L"X-Men Legends";
}

void install_controls_enabled(bool on)
{
    for (int id : {IDC_IMAGE, IDC_BROWSE_IMAGE, IDC_DOWNLOAD, IDC_LOCAL_ZIP, IDC_TARGET, IDC_BROWSE_TARGET})
        EnableWindow(item(id), on);
    bool local = checked(IDC_LOCAL_ZIP);
    EnableWindow(item(IDC_ZIP), on && local);
    EnableWindow(item(IDC_BROWSE_ZIP), on && local);
    EnableWindow(item(IDC_PLAY), on && !g_process);
    EnableWindow(item(IDC_SAVE), on);
}

void install_status(const std::wstring& text) { SetWindowTextW(item(IDC_INSTALL_STATUS), text.c_str()); }

void start_install()
{
    if (g_installing) { g_cancel_install = true; install_status(L"Cancelling..."); return; }
    InstallRequest request;
    request.image = text_of(IDC_IMAGE);
    request.target = text_of(IDC_TARGET);
    if (checked(IDC_LOCAL_ZIP)) request.release_zip = text_of(IDC_ZIP);
    std::error_code ec;
    if (request.image.empty() || !fs::is_regular_file(request.image, ec)) {
        ask(L"Choose your X-Men Legends disc image (ISO or XISO) first.", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (checked(IDC_LOCAL_ZIP) && (request.release_zip.empty() || !fs::is_regular_file(request.release_zip, ec))) {
        ask(L"Choose the OpenXML1 release zip, or let the launcher download it.", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (request.target.empty() || !request.target.is_absolute()) {
        ask(L"Choose the folder to install the game to.", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring problem = check_image(request.image, nullptr);
    if (!problem.empty()) { ask(problem, MB_OK | MB_ICONWARNING); return; }
    if (!request.release_zip.empty()) {
        std::wstring error;
        if (!is_release_zip(request.release_zip, error)) { ask(error, MB_OK | MB_ICONWARNING); return; }
    }
    if (has_install(request.target)) {
        if (game_running(request.target)) { ask(L"Close the game before reinstalling it.", MB_OK | MB_ICONINFORMATION); return; }
        if (ask(L"X-Men Legends is already installed in this folder. Install over it?\n\n"
                L"The game files are replaced from the disc image and the release. Saved games (UDATA, TDATA) "
                L"and settings are kept. Installed mods are taken out first; tick them again afterwards.",
                MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
    } else if (fs::exists(request.target, ec) && !fs::is_empty(request.target, ec) && !only_launcher_in(request.target)) {
        if (ask(L"The folder " + request.target.wstring() + L" is not empty. Install into it anyway?", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
    }
    set_preference("DiscImage", request.image.u8string());
    set_preference("InstallFolder", request.target.u8string());
    g_installing = true;
    g_cancel_install = false;
    install_controls_enabled(false);
    SetWindowTextW(item(IDC_INSTALL), L"Cancel");
    SendMessageW(item(IDC_PROGRESS), PBM_SETPOS, 0, 0);
    status(L"Installing...");
    std::thread([hwnd = g_main, request] {
        int last_permille = -1;
        std::wstring last_text, error;
        bool ok = install_game(request, [&](int permille, const std::wstring& text) {
            if (permille != last_permille || text != last_text) {
                last_permille = permille;
                last_text = text;
                PostMessageW(hwnd, WM_INSTALL_PROGRESS, permille, (LPARAM) new std::wstring(text));
            }
            return !g_cancel_install.load();
        }, error);
        PostMessageW(hwnd, WM_INSTALL_DONE, ok, (LPARAM) new std::wstring(error));
    }).detach();
}

void install_finished(bool ok, const std::wstring& error)
{
    g_installing = false;
    SetWindowTextW(item(IDC_INSTALL), L"Install");
    install_controls_enabled(true);
    if (!ok) {
        install_status(error == L"Cancelled." ? L"Installation cancelled." : L"Installation failed.");
        status(L"The game was not installed.");
        if (error != L"Cancelled.") ask(L"The installation did not finish:\n\n" + error, MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(item(IDC_PROGRESS), PBM_SETPOS, 1000, 0);
    fs::path target = text_of(IDC_TARGET);
    use_game_folder(target);
    install_status(L"Installed in " + target.wstring() + L".");
    status(L"Installed. The first launch prepares the game files.");
    if (ask(L"X-Men Legends is installed.\n\nThe first time it starts, the game unpacks and prepares its files, with a "
            L"progress window of its own. This takes a few minutes and happens only once.\n\nPlay now?",
            MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        show_page(kPlay);
        play();
    }
}

void browse_image()
{
    COMDLG_FILTERSPEC filters[] = {{L"Xbox disc images (*.iso, *.xiso)", L"*.iso;*.xiso"}, {L"All files", L"*.*"}};
    fs::path image = pick(false, L"Choose your X-Men Legends disc image", filters, 2);
    if (image.empty()) return;
    SetWindowTextW(item(IDC_IMAGE), image.c_str());
    std::wstring problem = check_image(image, nullptr);
    install_status(problem.empty() ? L"X-Men Legends disc image found." : problem);
}


// ---- launcher updates ----------------------------------------------------------

fs::path own_exe()
{
    wchar_t path[MAX_PATH * 4];
    return fs::path(std::wstring(path, GetModuleFileNameW(nullptr, path, (DWORD)std::size(path))));
}

void check_for_update(bool manual)
{
    if (g_updating) return;
    if (manual) {
        EnableWindow(item(IDC_CHECK_UPDATES), FALSE);
        status(L"Checking for launcher updates...");
    }
    std::thread([hwnd = g_main, manual] {
        auto* check = new UpdateCheck;
        check->ok = latest_launcher(check->release, check->error);
        PostMessageW(hwnd, WM_UPDATE_CHECKED, manual, (LPARAM)check);
    }).detach();
}

void update_checked(bool manual, const UpdateCheck& check)
{
    EnableWindow(item(IDC_CHECK_UPDATES), TRUE);
    const std::wstring current = widen(LAUNCHER_VERSION_TEXT);
    if (!check.ok) {
        // A quiet start-up check stays quiet when offline.
        if (manual) { status(L"Could not check for updates."); ask(check.error, MB_OK | MB_ICONWARNING); }
        return;
    }
    if (compare_versions(check.release.tag, current) <= 0) {
        if (manual) status(L"The launcher is up to date (version " + current + L").");
        return;
    }
    if (!manual && widen(preference("SkippedVersion")) == check.release.tag) return;
    std::wstring notes = check.release.notes;
    if (notes.size() > 900) notes = notes.substr(0, 900) + L"...";
    std::wstring text = L"Launcher " + check.release.tag + L" is available. You have version " + current + L".";
    if (!notes.empty()) text += L"\n\n" + notes;
    text += L"\n\nUpdate now? The launcher restarts when it is done.";
    if (ask(text, MB_YESNO | MB_ICONINFORMATION) != IDYES) {
        if (!manual) set_preference("SkippedVersion", narrow(check.release.tag));
        status(L"Launcher update " + check.release.tag + L" skipped.");
        return;
    }
    g_updating = true;
    EnableWindow(item(IDC_CHECK_UPDATES), FALSE);
    status(L"Downloading launcher " + check.release.tag + L"...");
    std::thread([hwnd = g_main, release = check.release, exe = own_exe()] {
        std::wstring error;
        bool ok = install_launcher_update(release, exe, [](uint64_t, uint64_t) { return true; }, error);
        PostMessageW(hwnd, WM_UPDATE_DONE, ok, (LPARAM) new std::wstring(error));
    }).detach();
}

void update_done(bool ok, const std::wstring& error)
{
    g_updating = false;
    EnableWindow(item(IDC_CHECK_UPDATES), TRUE);
    if (!ok) {
        status(L"The launcher was not updated.");
        ask(L"The launcher could not be updated:\n\n" + error, MB_OK | MB_ICONERROR);
        return;
    }
    // Start the new launcher, then close this one. A running game is not affected.
    std::wstring exe = own_exe().wstring(), command = L"\"" + exe + L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION info{};
    if (CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &info)) {
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
        DestroyWindow(g_main);
        return;
    }
    status(L"Updated. Restart the launcher to use the new version.");
}

// ---- building the window -----------------------------------------------------

INT_PTR CALLBACK page_proc(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message) {
    case WM_COMMAND: case WM_NOTIFY: case WM_HSCROLL:
        SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, SendMessageW(g_main, message, w, l));
        return TRUE;
    }
    return FALSE;
}

HWND make_page()
{
    struct { DLGTEMPLATE t; WORD menu, cls, title; } tmpl{};
    tmpl.t.style = WS_CHILD | DS_CONTROL;
    HWND page = CreateDialogIndirectParamW(g_instance, &tmpl.t, g_main, page_proc, 0);
    EnableThemeDialogTexture(page, ETDT_ENABLETAB);
    return page;
}

void build()
{
    g_tabs = add(kMain, WC_TABCONTROLW, L"", WS_CLIPSIBLINGS | WS_TABSTOP, 10, 10, kClientW - 20, 410, IDC_TABS);
    g_controls.pop_back();  // the tab control is placed by layout() itself
    const wchar_t* names[kPages] = {L"Play", L"Settings", L"Mods", L"Install"};
    for (int i = 0; i < kPages; ++i) {
        TCITEMW tab{TCIF_TEXT};
        tab.pszText = const_cast<wchar_t*>(names[i]);
        TabCtrl_InsertItem(g_tabs, i, &tab);
        g_pages[i] = make_page();
    }

    // Play
    group(kPlay, L"Display", 12, 8, 540, 132);
    button(kPlay, L"Windowed", 28, 32, 200, 22, IDC_WINDOWED, BS_AUTORADIOBUTTON | WS_GROUP);
    button(kPlay, L"Fullscreen", 28, 58, 110, 22, IDC_FULLSCREEN, BS_AUTORADIOBUTTON);
    label(kPlay, L"Borderless, covering the whole monitor.", 140, 60, 390);
    label(kPlay, L"Resolution", 28, 100, 100);
    HWND resolution = combo(kPlay, 140, 96, 220, IDC_RESOLUTION);
    combo_add(resolution, L"1920 × 1080 (1080p)", MAKELPARAM(1920, 1080));
    combo_add(resolution, L"1280 × 720 (720p)", MAKELPARAM(1280, 720));
    combo_add(resolution, L"640 × 480 (4:3, original)", MAKELPARAM(640, 480));
    group(kPlay, L"Launch", 12, 150, 540, 100);
    button(kPlay, L"Close the launcher when the game starts", 28, 174, 400, 22, IDC_CLOSE_ON_START, BS_AUTOCHECKBOX);
    label(kPlay, L"Game folder", 28, 212, 100);
    label(kPlay, L"", 140, 212, 290, 20, IDC_FOLDER, SS_PATHELLIPSIS | SS_NOPREFIX);
    button(kPlay, L"Change...", 440, 206, 96, 28, IDC_CHANGE_FOLDER);
    label(kPlay, L"", 12, 264, 540, 20, IDC_PLAY_SUMMARY);
    label(kPlay, (L"Launcher version " + widen(LAUNCHER_VERSION_TEXT)).c_str(), 12, 330, 300, 20, IDC_VERSION);
    button(kPlay, L"Check for updates", 402, 324, 150, 28, IDC_CHECK_UPDATES);

    // Settings: the PC options the game's own menus offer.
    group(kSettings, L"Graphics", 12, 8, 540, 58);
    label(kSettings, L"Anti-aliasing (FSAA)", 28, 34, 170);
    combo(kSettings, 210, 30, 150, IDC_FSAA);
    group(kSettings, L"Controls", 12, 74, 540, 150);
    button(kSettings, L"Keyboard and mouse", 28, 98, 300, 22, IDC_KEYBOARD, BS_AUTOCHECKBOX);
    label(kSettings, L"Keyboard plays as", 28, 130, 170);
    HWND player = combo(kSettings, 210, 126, 150, IDC_KEYBOARD_PLAYER);
    for (int i = 0; i < 4; ++i) combo_add(player, (L"Player " + std::to_wstring(i + 1)).c_str(), i);
    label(kSettings, L"Mouse sensitivity", 28, 162, 170);
    HWND slider = add(kSettings, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | TBS_TRANSPARENTBKGND | WS_TABSTOP, 204, 158, 270, 28, IDC_SENSITIVITY);
    SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(10, 300));
    SendMessageW(slider, TBM_SETPAGESIZE, 0, 25);
    SendMessageW(slider, TBM_SETLINESIZE, 0, 5);
    label(kSettings, L"", 480, 162, 60, 20, IDC_SENSITIVITY_VALUE);
    label(kSettings, L"Controllers", 28, 196, 170);
    HWND pads = combo(kSettings, 210, 192, 150, IDC_CONTROLLERS);
    combo_add(pads, L"Shared", 0);
    combo_add(pads, L"One per player", 1);
    group(kSettings, L"Camera", 12, 232, 540, 80);
    button(kSettings, L"Flip camera left and right", 28, 254, 250, 22, IDC_FLIP, BS_AUTOCHECKBOX);
    button(kSettings, L"Invert camera up and down", 290, 254, 250, 22, IDC_INVERT, BS_AUTOCHECKBOX);
    button(kSettings, L"View shake", 28, 282, 250, 22, IDC_SHAKE, BS_AUTOCHECKBOX);
    label(kSettings, L"Volume, music, subtitles, vibration and the camera angle are kept in the game's own save and "
                     L"are changed from Options in the game. Keys and buttons are under Options, Advanced.",
          12, 320, 400, 52);
    button(kSettings, L"Restore defaults", 420, 322, 132, 28, IDC_DEFAULTS);

    // Mods
    HWND list = add(kMods, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP, 12, 8, 540, 222, IDC_MOD_LIST);
    ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SetWindowTheme(list, L"Explorer", nullptr);
    const wchar_t* columns[] = {L"Name", L"Version", L"Author"};
    for (int i = 0; i < 3; ++i) {
        LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH};
        column.pszText = const_cast<wchar_t*>(columns[i]);
        column.cx = 100;
        ListView_InsertColumn(list, i, &column);
    }
    ListView_EnableGroupView(list, TRUE);
    const wchar_t* groups[] = {L"Characters", L"Other mods"};
    for (int i = 0; i < 2; ++i) {
        LVGROUP g{sizeof(g), LVGF_HEADER | LVGF_GROUPID};
        g.pszHeader = const_cast<wchar_t*>(groups[i]);
        g.iGroupId = i + 1;
        ListView_InsertGroup(list, -1, &g);
    }
    label(kMods, L"", 12, 234, 540, 50, IDC_MOD_INFO, SS_NOPREFIX);
    button(kMods, L"Unlock every installed character and costume (modder mode)", 12, 288, 540, 22, IDC_MODDER, BS_AUTOCHECKBOX);
    button(kMods, L"Open mods folder", 12, 318, 130, 28, IDC_OPEN_MODS);
    button(kMods, L"Refresh", 150, 318, 90, 28, IDC_REFRESH_MODS);
    button(kMods, L"Apply mods", 422, 318, 130, 28, IDC_APPLY_MODS);
    if (!kModsEnabled)
        for (int id : {IDC_MOD_LIST, IDC_MODDER, IDC_OPEN_MODS, IDC_REFRESH_MODS, IDC_APPLY_MODS}) EnableWindow(item(id), FALSE);

    // Install: from the player's own disc image, as the release instructions say.
    group(kInstall, L"1.  Your X-Men Legends disc image (ISO or XISO)", 12, 8, 540, 62);
    edit(kInstall, 28, 34, 404, IDC_IMAGE);
    button(kInstall, L"Browse...", 442, 32, 96, 28, IDC_BROWSE_IMAGE);
    group(kInstall, L"2.  OpenXML1", 12, 78, 540, 112);
    button(kInstall, L"Download the latest release from GitHub", 28, 100, 400, 22, IDC_DOWNLOAD, BS_AUTORADIOBUTTON | WS_GROUP);
    button(kInstall, L"Use a release zip I already have:", 28, 126, 400, 22, IDC_LOCAL_ZIP, BS_AUTORADIOBUTTON);
    edit(kInstall, 48, 154, 384, IDC_ZIP, WS_GROUP);
    button(kInstall, L"Browse...", 442, 152, 96, 28, IDC_BROWSE_ZIP);
    group(kInstall, L"3.  Install to", 12, 198, 540, 62);
    edit(kInstall, 28, 224, 404, IDC_TARGET);
    button(kInstall, L"Browse...", 442, 222, 96, 28, IDC_BROWSE_TARGET);
    HWND progress = add(kInstall, PROGRESS_CLASSW, L"", 0, 12, 272, 540, 16, IDC_PROGRESS);
    SendMessageW(progress, PBM_SETRANGE32, 0, 1000);
    label(kInstall, L"The disc image is only read, never changed.", 12, 296, 400, 40, IDC_INSTALL_STATUS, SS_NOPREFIX);
    button(kInstall, L"Install", 422, 330, 130, 32, IDC_INSTALL);

    // Always visible
    label(kMain, L"", 12, 438, 350, 20, IDC_STATUS, SS_ENDELLIPSIS | SS_NOPREFIX);
    button(kMain, L"Save", 376, 430, 96, 34, IDC_SAVE);
    button(kMain, L"Play", 482, 430, 108, 34, IDC_PLAY, BS_DEFPUSHBUTTON);
}

void command(int id, int code)
{
    switch (id) {
    case IDOK: case IDC_PLAY: play(); break;
    case IDC_SAVE:
        if (save_settings()) status(L"Settings saved.");
        break;
    case IDC_KEYBOARD: EnableWindow(item(IDC_KEYBOARD_PLAYER), checked(IDC_KEYBOARD)); break;
    case IDC_DEFAULTS: {
        Xml1PcSettings defaults;
        xml1_pc_settings_defaults(&defaults);
        Xml1PcSettings keep = g_settings;
        g_settings = defaults;
        // Only the options on this tab; display choices and bindings are kept.
        g_settings.width = keep.width; g_settings.height = keep.height; g_settings.fullscreen = keep.fullscreen;
        std::memcpy(g_settings.keys, keep.keys, sizeof(keep.keys));
        std::memcpy(g_settings.alternate_keys, keep.alternate_keys, sizeof(keep.alternate_keys));
        std::memcpy(g_settings.pad_bindings, keep.pad_bindings, sizeof(keep.pad_bindings));
        std::memcpy(g_settings.alternate_pad_bindings, keep.alternate_pad_bindings, sizeof(keep.alternate_pad_bindings));
        settings_to_ui();
        g_settings = keep;
        status(L"Defaults restored. Press Save or Play to keep them.");
        break;
    }
    case IDC_CHANGE_FOLDER: {
        fs::path folder = pick_game_folder();
        if (!folder.empty()) use_game_folder(folder);
        break;
    }
    case IDC_OPEN_MODS:
        if (g_game.empty()) break;
        ensure_mods_readme(g_game);
        ShellExecuteW(g_main, L"open", mods_dir(g_game).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case IDC_REFRESH_MODS: if (!g_game.empty()) load_mods(); break;
    case IDC_INSTALL: start_install(); break;
    case IDC_CHECK_UPDATES: check_for_update(true); break;
    case IDC_BROWSE_IMAGE: browse_image(); break;
    case IDC_DOWNLOAD: case IDC_LOCAL_ZIP: install_controls_enabled(!g_installing); break;
    case IDC_BROWSE_ZIP: {
        COMDLG_FILTERSPEC filters[] = {{L"OpenXML1 release (*.zip)", L"*.zip"}};
        fs::path zip = pick(false, L"Choose the OpenXML1 release zip", filters, 1);
        if (!zip.empty()) SetWindowTextW(item(IDC_ZIP), zip.c_str());
        break;
    }
    case IDC_BROWSE_TARGET: {
        fs::path folder = pick(true, L"Choose where to install X-Men Legends", nullptr, 0);
        if (!folder.empty()) SetWindowTextW(item(IDC_TARGET), folder.c_str());
        break;
    }
    case IDC_APPLY_MODS: apply_selected_mods(false); break;
    }
    (void)code;
}

LRESULT notify(NMHDR* header)
{
    if (header->idFrom == IDC_MOD_LIST && header->code == NM_CUSTOMDRAW) {
        // A mod that cannot be installed is drawn greyed out.
        auto* draw = (NMLVCUSTOMDRAW*)header;
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            size_t index = (size_t)draw->nmcd.lItemlParam;
            if (index < g_mods.size() && !g_mods[index].problem.empty()) draw->clrText = GetSysColor(COLOR_GRAYTEXT);
        }
        return CDRF_DODEFAULT;
    }
    if (header->idFrom == IDC_TABS && header->code == TCN_SELCHANGE) {
        show_page((Page)TabCtrl_GetCurSel(g_tabs));
        return 0;
    }
    if (header->idFrom != IDC_MOD_LIST || header->code != LVN_ITEMCHANGED || g_populating) return 0;
    auto* change = (NMLISTVIEW*)header;
    LVITEMW lv{LVIF_PARAM};
    lv.iItem = change->iItem;
    ListView_GetItem(header->hwndFrom, &lv);
    if (change->uNewState & LVIS_SELECTED) describe_mod((int)lv.lParam);
    bool was = ((change->uOldState & LVIS_STATEIMAGEMASK) >> 12) == 2;
    bool now = ((change->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2;
    if ((change->uChanged & LVIF_STATE) && now && !was && !g_mods[lv.lParam].problem.empty()) {
        g_populating = true;
        ListView_SetCheckState(header->hwndFrom, change->iItem, FALSE);
        g_populating = false;
        describe_mod((int)lv.lParam);
        MessageBeep(MB_ICONWARNING);
    }
    if (now != was) mods_status();
    return 0;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message) {
    case WM_COMMAND: command(LOWORD(w), HIWORD(w)); return 0;
    case WM_NOTIFY: return notify((NMHDR*)l);
    case WM_HSCROLL:
        if ((HWND)l == item(IDC_SENSITIVITY)) {
            auto value = SendMessageW(item(IDC_SENSITIVITY), TBM_GETPOS, 0, 0);
            SetWindowTextW(item(IDC_SENSITIVITY_VALUE), (std::to_wstring(value) + L"%").c_str());
        }
        return 0;
    case WM_GAME_EXITED: game_exited((DWORD)w); return 0;
    case WM_UPDATE_CHECKED: {
        std::unique_ptr<UpdateCheck> check((UpdateCheck*)l);
        update_checked(w != 0, *check);
        return 0;
    }
    case WM_UPDATE_DONE: {
        std::unique_ptr<std::wstring> error((std::wstring*)l);
        update_done(w != 0, *error);
        return 0;
    }
    case WM_INSTALL_PROGRESS: {
        std::unique_ptr<std::wstring> text((std::wstring*)l);
        SendMessageW(item(IDC_PROGRESS), PBM_SETPOS, w, 0);
        install_status(*text);
        return 0;
    }
    case WM_INSTALL_DONE: {
        std::unique_ptr<std::wstring> error((std::wstring*)l);
        install_finished(w != 0, *error);
        return 0;
    }
    case WM_DPICHANGED: {
        g_dpi = HIWORD(w);
        make_fonts();
        auto* suggested = (RECT*)l;
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        layout();
        return 0;
    }
    case WM_CLOSE:
        if (g_installing) {
            if (ask(L"Stop the installation?", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            g_cancel_install = true;
        }
        if (!g_game.empty() && settings_dirty()) {
            int choice = ask(L"Save the changed settings?", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (choice == IDCANCEL || (choice == IDYES && !save_settings())) return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}

// The launcher's own rendering of its own window, for checking the layout
// without capturing the desktop: `--capture <dir> --game <folder>`.
bool save_png(HBITMAP bitmap, int w, int h, const fs::path& path)
{
    IWICImagingFactory* factory = nullptr;
    IWICBitmap* source = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
              SUCCEEDED(factory->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapIgnoreAlpha, &source)) &&
              SUCCEEDED(factory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
              SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
              SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
              SUCCEEDED(frame->Initialize(nullptr)) &&
              SUCCEEDED(frame->SetSize(w, h)) &&
              SUCCEEDED(frame->WriteSource(source, nullptr)) &&
              SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    for (IUnknown* p : {(IUnknown*)frame, (IUnknown*)encoder, (IUnknown*)stream, (IUnknown*)source, (IUnknown*)factory})
        if (p) p->Release();
    return ok;
}

int capture_pages()
{
    fs::create_directories(g_capture);
    const wchar_t* names[kPages] = {L"play", L"settings", L"mods", L"install"};
    int failed = 0;
    for (int page = 0; page < kPages; ++page) {
        show_page((Page)page);
        if (page == kMods && !g_mods.empty()) ListView_SetItemState(item(IDC_MOD_LIST), 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        RedrawWindow(g_main, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        MSG msg;
        for (DWORD until = GetTickCount() + 300; GetTickCount() < until;) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            Sleep(10);
        }
        RECT r;
        GetWindowRect(g_main, &r);
        int w = r.right - r.left, h = r.bottom - r.top;
        HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
        HGDIOBJ old = SelectObject(dc, bitmap);
        bool ok = PrintWindow(g_main, dc, PW_RENDERFULLCONTENT) != 0;
        SelectObject(dc, old);
        ok = ok && save_png(bitmap, w, h, g_capture / (std::wstring(names[page]) + L".png"));
        if (!ok) ++failed;
        DeleteObject(bitmap);
        DeleteDC(dc);
        ReleaseDC(nullptr, screen);
    }
    return failed;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    fs::path capture_game;
    {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i + 1 < argc; ++i) {
            if (!wcscmp(argv[i], L"--capture")) g_capture = argv[++i];
            else if (!wcscmp(argv[i], L"--game")) capture_game = argv[++i];
        }
        LocalFree(argv);
    }
    g_instance = instance;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW cls{sizeof(cls)};
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    cls.lpszClassName = L"OpenXML1Launcher";
    RegisterClassExW(&cls);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    g_main = CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, L"X-Men Legends Launcher", style,
                             CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, nullptr, nullptr, instance, nullptr);
    g_dpi = GetDpiForWindow(g_main);
    RECT frame{0, 0, px(kClientW), px(kClientH)};
    AdjustWindowRectExForDpi(&frame, style, FALSE, WS_EX_CONTROLPARENT, g_dpi);
    SetWindowPos(g_main, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top, SWP_NOMOVE | SWP_NOZORDER);
    make_fonts();
    build();
    layout();
    show_page(kPlay);
    check(IDC_CLOSE_ON_START, preference("CloseOnStart") == "1");
    // Install tab defaults: what was used last, else a Games folder.
    check(IDC_DOWNLOAD, true);
    SetWindowTextW(item(IDC_IMAGE), widen(preference("DiscImage")).c_str());
    std::wstring target = widen(preference("InstallFolder"));
    SetWindowTextW(item(IDC_TARGET), (target.empty() ? default_install_folder().wstring() : target).c_str());
    install_controls_enabled(true);

    if (!g_capture.empty()) {
        // Off screen, and never activated: nothing appears on the desktop.
        SetWindowPos(g_main, nullptr, -20000, -20000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(g_main, SW_SHOWNOACTIVATE);
        if (!is_game_folder(capture_game)) return 2;
        use_game_folder(capture_game);
        int failed = capture_pages();
        DestroyWindow(g_main);
        CoUninitialize();
        return failed ? 1 : 0;
    }
    ShowWindow(g_main, show);

    fs::path folder = find_game_folder();
    if (folder.empty()) {
        // Nothing installed yet: start on Install. Play still offers Change...
        // for a game that is already set up somewhere.
        show_page(kInstall);
        status(L"Install the game from your disc image, or choose an existing game folder on the Play tab.");
        EnableWindow(item(IDC_SAVE), FALSE);
    } else {
        use_game_folder(folder);
    }
    SetFocus(item(IDC_PLAY));
    // Tidy up after an update, then look for the next one in the background.
    remove_previous_launcher(own_exe());
    check_for_update(false);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(g_main, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
