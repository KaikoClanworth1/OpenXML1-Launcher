// Headless checks for the launcher: INI editing, XML merging, the mod
// install/uninstall cycle, the camera flip, and the disc and release readers.
//
//   launcher-test [--game <folder>] [--image <iso>] [--install <iso> <release zip> <target>]
//
// --game merges and compiles a copy of that game's data/herostat.eng (the
// folder is only read). --image reads a real disc image. --install runs the
// whole installation into <target>, which must not exist yet.
#include "bundled.h"
#include "camera.h"
#include "install.h"
#include "mods.h"
#include "release.h"
#include "version.h"
#include "xiso.h"
#include "text.h"
#include "xmlb.h"
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <map>

using namespace launcher;

static int failures;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static std::string read(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
static void write(const fs::path& p, const std::string& s)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}
static bool contains(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }

static std::map<fs::path, std::string> snapshot(const fs::path& root)
{
    std::map<fs::path, std::string> out;
    for (auto& e : fs::recursive_directory_iterator(root)) {
        auto rel = fs::relative(e.path(), root);
        if (*rel.begin() == L"mods") continue;
        if (e.is_regular_file()) out[rel] = read(e.path());
    }
    return out;
}

static void ini_test(const fs::path& dir)
{
    fs::path path = dir / L"build.ini";
    write(path, "[BUILD]\nbuild = 1\nPreferFilesLoose = 1 ; keep loose\n\n[OTHER]\nx = 2\n");
    Ini ini;
    CHECK(ini.load(path));
    CHECK(ini.get("build", "preferfilesloose") == "1");
    ini.set("BUILD", "PreferFilesLoose", "0");
    ini.set("BUILD", "modderMode", "1");
    ini.set("NEW", "a", "b");
    CHECK(ini.save(path));
    std::string text = read(path);
    CHECK(contains(text, "PreferFilesLoose = 0 ; keep loose\n"));
    CHECK(text.find("modderMode = 1") < text.find("[OTHER]"));
    CHECK(contains(text, "[OTHER]\nx = 2\n"));
    CHECK(contains(text, "[NEW]\na = b\n"));
    CHECK(!contains(text, "\r"));  // line endings kept as they were
}

static void merge_test()
{
    std::string base =
        "<characters>\n"
        "<stats name=\"default\" charactername=\"defaultman\">\n<Race name=\"Mutant\"/>\n</stats>\n"
        "<!-- <stats name=\"commented\"> -->\n"
        "<stats name=\"Beast\" skin=\"0501\">\n<Talent name=\"x\">\n<level description=\"a > b\"/>\n</Talent>\n</stats>\n"
        "</characters>\n";
    std::string fragment =
        "<characters>\n<stats name=\"beast\" skin=\"0599\">\n</stats>\n"
        "<stats name=\"Deadpool\" charactername=\"Deadpool\"/>\n</characters>\n";
    std::string out, error;
    CHECK(merge_xml(base, fragment, out, error));
    CHECK(contains(out, "skin=\"0599\""));
    CHECK(!contains(out, "skin=\"0501\""));
    CHECK(contains(out, "charactername=\"defaultman\""));  // name= found, not charactername=
    CHECK(contains(out, "<!-- <stats name=\"commented\"> -->"));
    CHECK(out.find("Deadpool") < out.find("</characters>"));
    CHECK(!merge_xml(base, "<npcs><stats name=\"a\"/></npcs>", out, error));

    // Attributes on the fragment's root set those on the game file's root.
    std::string mission = "<MISSION descname=\"Alison in NYC\" minheros=\"2\" maxheros=\"2\">\r\n<REQUIREDHERO name=\"wolverine\"/>\r\n</MISSION>\r\n";
    CHECK(merge_xml(mission, "<MISSION MAXHEROS=\"4\" teamselect=\"true\">\n</MISSION>\n", out, error));
    CHECK(contains(out, "maxheros=\"4\"") && contains(out, "minheros=\"2\"") && contains(out, "descname=\"Alison in NYC\""));
    CHECK(contains(out, " teamselect=\"true\">") && contains(out, "<REQUIREDHERO name=\"wolverine\"/>"));
    CHECK(!contains(out, "maxheros=\"2\""));
}

static void install_test(const fs::path& game)
{
    std::string herostat =
        "<characters>\n<stats name=\"Beast\" skin=\"0501\">\n<Race name=\"Mutant\"/>\n</stats>\n</characters>\n";
    write(game / L"default.xbe", "xbe");
    write(game / L"X-Men Legends.exe", "exe");
    write(game / L"data/herostat.eng", herostat);
    write(game / L"data/herostat.fre", herostat);
    auto binary = xml1::compile_xmlb(herostat);
    write(game / L"data/herostat.engb", std::string(binary.begin(), binary.end()));
    write(game / L"textures/a.png", "original");
    write(game / L"scripts/test/start.py", "    first()\r\n    second()");
    auto before = snapshot(game);

    fs::path mods = game / L"mods";
    write(mods / L"A/mod.ini", "[Mod]\nName = Deadpool\nType = character\nVersion = 1.0\n");
    write(mods / L"A/merge/data/herostat.eng", "<characters>\n<stats name=\"Deadpool\" skin=\"9901\"/>\n</characters>\n");
    write(mods / L"A/files/actors/9901.igb", "model");
    write(mods / L"B/mod.ini", "[Mod]\nName = Gambit\nType = character\n");
    write(mods / L"B/merge/data/herostat.eng", "<characters>\n<stats name=\"Gambit\"/>\n<stats name=\"Beast\" skin=\"0502\"/>\n</characters>\n");
    write(mods / L"B/files/textures/a.png", "from B");
    write(mods / L"C/mod.ini", "[Mod]\nName = Textures\n");
    write(mods / L"C/files/textures/a.png", "from C");
    write(mods / L"Bad/files/default.xbe", "nope");
    // append\: lines added to the end of a game script, in its own line endings.
    write(game / L"scripts/test/start.py", "    first()\r\n    second()");
    write(mods / L"Lines/mod.ini", "[Mod]\nName = Lines\n");
    write(mods / L"Lines/append/scripts/test/start.py", "    added(\"one\")\n    added(\"two\")\n");
    write(mods / L"BadAppend/append/scripts/test/missing.py", "x");
    // What the game's first-run setup leaves beside a fragment is not the mod's.
    write(mods / L"A/merge/data/herostat.engb", "compiled by the game");

    auto found = find_mods(game);
    CHECK(found.size() == 6);
    for (const auto& m : found) {
        if (m.folder == L"Bad" || m.folder == L"BadAppend") CHECK(!m.problem.empty());
        else CHECK(m.problem.empty());
        if (m.folder == L"A") CHECK(m.character && m.name == L"Deadpool" && m.version == L"1.0" && m.merges == 1);
        if (m.folder == L"C") CHECK(!m.character);
    }
    ensure_mods_readme(game);
    CHECK(fs::exists(mods / L"README.txt"));

    std::vector<std::wstring> all{L"A", L"B", L"C"};
    CHECK(find_conflicts(game, all).size() == 1);
    auto result = apply_mods(game, all);
    CHECK(result.ok);
    if (!result.ok) std::printf("  %ls\n", result.error.c_str());
    std::string eng = read(game / L"data/herostat.eng"), fre = read(game / L"data/herostat.fre");
    CHECK(contains(eng, "Deadpool") && contains(eng, "Gambit") && contains(eng, "0502") && !contains(eng, "0501"));
    CHECK(contains(fre, "Deadpool"));  // English fragment reaches the other languages
    std::string engb = read(game / L"data/herostat.engb");
    CHECK(contains(xml1::decode_xmlb(engb.data(), (unsigned)engb.size()), "Deadpool"));
    CHECK(fs::exists(game / L"data/herostat.freb"));
    CHECK(read(game / L"textures/a.png") == "from C");
    CHECK(read(game / L"actors/9901.igb") == "model");
    CHECK(installed_mods(game) == all);

    // Appending, then taking it out again.
    result = apply_mods(game, {L"Lines"});
    CHECK(result.ok);
    CHECK(read(game / L"scripts/test/start.py") == "    first()\r\n    second()\r\n    added(\"one\")\r\n    added(\"two\")\r\n");
    result = apply_mods(game, all);
    CHECK(result.ok);

    // A different selection starts from the originals, not from A+B+C.
    result = apply_mods(game, {L"C"});
    CHECK(result.ok);
    CHECK(!contains(read(game / L"data/herostat.eng"), "Deadpool"));
    CHECK(!fs::exists(game / L"actors/9901.igb"));
    CHECK(read(game / L"textures/a.png") == "from C");

    result = apply_mods(game, {});
    CHECK(result.ok);
    CHECK(snapshot(game) == before);  // byte for byte, and nothing left over
    CHECK(installed_mods(game).empty());

    // A mod that cannot compile is undone completely.
    write(mods / L"Broken/merge/data/herostat.eng", "<characters>\n<stats name=\"X\">\n</characters>\n");
    result = apply_mods(game, {L"A", L"Broken"});
    CHECK(!result.ok);
    CHECK(snapshot(game) == before);
}

static void real_data_test(const fs::path& real, const fs::path& scratch)
{
    fs::path source = real / L"data/herostat.eng";
    if (!fs::exists(source)) { std::printf("skip real data: no %ls\n", source.c_str()); return; }
    std::string base = read(source);
    size_t at = base.find("<stats name=\"Wolverine\"");
    size_t end = base.find("</stats>", at);
    CHECK(at != std::string::npos && end != std::string::npos);
    std::string entry = base.substr(at, end + 8 - at);
    entry.replace(entry.find("Wolverine"), 9, "Launcher Test");
    std::string merged, error;
    CHECK(merge_xml(base, "<characters>\n" + entry + "\n</characters>\n", merged, error));
    CHECK(merged.size() > base.size());
    auto binary = xml1::compile_xmlb(merged);
    std::string decoded = xml1::decode_xmlb(binary.data(), (unsigned)binary.size());
    CHECK(contains(decoded, "Launcher Test") && contains(decoded, "Wolverine"));
    CHECK(xml1::compile_xmlb(decoded) == binary);
    // Merging an unchanged game file into itself changes nothing it compiles to.
    CHECK(merge_xml(base, base, merged, error));
    CHECK(xml1::compile_xmlb(merged) == xml1::compile_xmlb(base));
    std::printf("real herostat: %zu bytes merged and compiled\n", base.size());
    (void)scratch;
}

static void camera_test()
{
    Xml1PcSettings s;
    xml1_pc_settings_defaults(&s);
    const Xml1PcSettings defaults = s;
    char error[160];
    for (unsigned p = 0; p < 4; ++p) CHECK(!camera_flipped(s, p));
    set_camera_flipped(s, true);
    for (unsigned p = 0; p < 4; ++p) {
        CHECK(camera_flipped(s, p));
        CHECK(s.pad_bindings[p][XML1_PC_CAMERA_LEFT] == XML1_PAD_RX_POS);
        CHECK(s.keys[p][XML1_PC_CAMERA_LEFT] == 'L' && s.keys[p][XML1_PC_CAMERA_RIGHT] == 'J');
    }
    CHECK(xml1_pc_settings_validate(&s, error, sizeof(error)));  // the game accepts it
    set_camera_flipped(s, true);                                  // idempotent
    CHECK(camera_flipped(s, 0));
    set_camera_flipped(s, false);
    CHECK(!std::memcmp(&s, &defaults, sizeof(s)));
}

static void updater_test(const fs::path& scratch)
{
    CHECK(compare_versions(L"v0.2.0", L"0.1.0") > 0);
    CHECK(compare_versions(L"v0.1.0", L"0.1.0") == 0);
    CHECK(compare_versions(L"0.1.9", L"v0.1.10") < 0);
    CHECK(compare_versions(L"v1", L"0.9.9") > 0);
    CHECK(compare_versions(L"v0.1.0-beta", L"0.1.0") == 0);

    // Swap the exe of a program that is running, as the updater does to itself.
    fs::path dir = scratch / L"updater";
    fs::create_directories(dir);
    fs::path exe = dir / L"running.exe";
    wchar_t system[MAX_PATH];
    GetSystemDirectoryW(system, MAX_PATH);
    fs::copy_file(fs::path(system) / L"ping.exe", exe);
    std::wstring command = L"\"" + exe.wstring() + L"\" -n 6 127.0.0.1";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    CHECK(CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process));
    Sleep(300);
    CHECK(WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT);  // still running
    fs::path incoming = dir / L"running.exe.new";
    write(incoming, "MZ new launcher");
    std::wstring error;
    CHECK(replace_running_exe(exe, incoming, error));
    if (!error.empty()) std::printf("  %ls\n", error.c_str());
    CHECK(read(exe) == "MZ new launcher");
    CHECK(fs::exists(dir / L"running.exe.old") && !fs::exists(incoming));
    TerminateProcess(process.hProcess, 0);
    WaitForSingleObject(process.hProcess, 5000);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    remove_previous_launcher(exe);
    CHECK(!fs::exists(dir / L"running.exe.old"));

    // A failed swap leaves the working program where it was.
    CHECK(!replace_running_exe(exe, dir / L"missing.exe", error));
    CHECK(read(exe) == "MZ new launcher");
}

static void bundled_test(const fs::path& scratch)
{
    fs::path game = scratch / L"bundled-game";
    fs::create_directories(game);
    CHECK(!bundled_files().empty());
    auto written = install_bundled_mods(game);
    CHECK(written.size() == 1 && written[0] == L"early-xmen-xtraction");
    fs::path mod = game / L"mods/early-xmen-xtraction";
    CHECK(fs::exists(mod / L"mod.ini") && fs::exists(mod / L"append/scripts/nyc/alison/nyc1_1_1.py"));
    CHECK(fs::exists(mod / L"merge/data/missions/alison.eng") && fs::exists(mod / L"merge/maps/nyc/alison/nyc1_1_1.eng"));
    // The same bytes as the repository's copy, which is the one tested in the game.
    fs::path source = fs::path(__FILE__).parent_path().parent_path() / L"mods/early-xmen-xtraction";
    for (const wchar_t* rel : {L"mod.ini", L"merge/data/missions/alison.eng", L"append/scripts/nyc/alison/nyc1_1_1.py"}) {
        std::string a = read(source / rel), b = read(mod / rel);
        a.erase(std::remove(a.begin(), a.end(), '\r'), a.end());  // git may check files out with CRLF
        CHECK(a == b);
    }
    CHECK(install_bundled_mods(game).empty());  // same version: left alone
    write(mod / L"mod.ini", "[Mod]\nName = old\nVersion = 0.0\n");
    CHECK(install_bundled_mods(game).size() == 1);  // different version: refreshed
    CHECK(contains(read(mod / L"mod.ini"), "Early X-Men"));
}

static void sound_repair_test(const fs::path& scratch)
{
    fs::path game = scratch / L"sounds-game";
    write(game / L"sounds/zsds/b/i/bishop_m.zsm", "disc bishop");
    write(game / L"sounds/zsds/m/e/menu_a.zsm", "disc menu");
    write(game / L"sounds/eng/b/i/bishop_m.zsm", "release bishop");
    write(game / L"sounds/eng/s/u/sun_m.zsm", "release sunfire");
    CHECK(sounds_need_repair(game));
    std::wstring error;
    CHECK(repair_sounds(game, error) == 2 && error.empty());
    CHECK(!fs::exists(game / L"sounds/eng"));  // the game stops routing to it
    CHECK(read(game / L"sounds/zsds/b/i/bishop_m.zsm") == "release bishop");
    CHECK(read(game / L"sounds/zsds/s/u/sun_m.zsm") == "release sunfire");
    CHECK(read(game / L"sounds/zsds/m/e/menu_a.zsm") == "disc menu");
    CHECK(!sounds_need_repair(game));
    // A complete language folder is a real translation and is left alone.
    write(game / L"sounds/eng/b/i/bishop_m.zsm", "x");
    write(game / L"sounds/eng/m/e/menu_a.zsm", "x");
    write(game / L"sounds/eng/s/u/sun_m.zsm", "x");
    CHECK(!sounds_need_repair(game));
}

// Only the disc's exact leftover bytes count as stale; anything else is the
// player's and is left alone. The replacement itself needs a real game's
// z/assetsfb.zip: see --repair-disc.
static void disc_data_test(const fs::path& scratch)
{
    fs::path game = scratch / L"disc-data-game";
    write(game / L"data/herostat.eng", "<characters><stats name=\"Wolverine\" skin=\"0301\"/></characters>");
    write(game / L"data/stat_rules.xml", "<STAT_RULES/>");
    CHECK(stale_disc_data(game).empty());
    std::wstring error;
    CHECK(repair_disc_data(game, error) == 0 && error.empty());
    CHECK(read(game / L"data/herostat.eng") == "<characters><stats name=\"Wolverine\" skin=\"0301\"/></characters>");
}

static void image_test(const fs::path& image)
{
    Xiso disc;
    std::wstring error;
    CHECK(disc.open(image, error));
    if (!error.empty()) std::printf("  %ls\n", error.c_str());
    CHECK(disc.title_id() == kXml1TitleId);
    CHECK(disc.find("default.xbe") && disc.find("z/assetsfb.zip") && disc.find("DEFAULT.XBE"));
    std::vector<unsigned char> xbe;
    CHECK(disc.read(*disc.find("default.xbe"), xbe, error) && xbe.size() > 4 && !std::memcmp(xbe.data(), "XBEH", 4));
    CHECK(check_image(image, nullptr).empty());
    std::printf("disc image: %zu files, %.1f MB, title %08X\n", disc.files().size(), disc.total_bytes() / 1048576.0, disc.title_id());
    // Not a disc image at all.
    Xiso wrong;
    CHECK(!wrong.open(fs::u8path(__FILE__), error));
}

static void full_install_test(const fs::path& image, const fs::path& zip, const fs::path& target)
{
    if (fs::exists(target)) { std::printf("FAIL: %ls already exists\n", target.c_str()); ++failures; return; }
    std::wstring error;
    CHECK(is_release_zip(zip, error));
    int last = -1;
    bool ok = install_game({image, zip, target}, [&](int permille, const std::wstring& what) {
        if (permille / 100 != last / 100) std::printf("  %3d%%  %ls\n", permille / 10, what.c_str());
        last = permille;
        return true;
    }, error);
    CHECK(ok);
    if (!ok) std::printf("  %ls\n", error.c_str());
    CHECK(has_install(target));
    CHECK(fs::exists(target / L"z/assetsfb.zip") && fs::is_directory(target / L"movies"));
    // The disc's files arrive intact: default.xbe as read from the image.
    Xiso disc;
    disc.open(image, error);
    std::vector<unsigned char> xbe;
    disc.read(*disc.find("default.xbe"), xbe, error);
    std::string installed = read(target / L"default.xbe");
    CHECK(installed.size() == xbe.size() && !std::memcmp(installed.data(), xbe.data(), xbe.size()));
    size_t count = 0;
    for (auto& e : fs::recursive_directory_iterator(target)) count += e.is_regular_file();
    std::printf("installed %zu files into %ls\n", count, target.c_str());
}

// Against the published release: what the updater sees, and that it can
// swap the release's exe into place, byte-identical to `built`.
static void release_check(const fs::path& scratch, const fs::path& built)
{
    LauncherRelease release;
    std::wstring error;
    CHECK(latest_launcher(release, error));
    if (!error.empty()) { std::printf("  %ls\n", error.c_str()); return; }
    std::printf("latest release %ls, %llu bytes, %ls\n", release.tag.c_str(), (unsigned long long)release.size, release.url.c_str());
    CHECK(compare_versions(release.tag, L"" LAUNCHER_VERSION_TEXT) >= 0);
    fs::path dir = scratch / L"release-check";
    fs::create_directories(dir);
    fs::path exe = dir / L"X-Men Legends Launcher.exe";
    write(exe, "MZ the previous launcher");
    CHECK(install_launcher_update(release, exe, [](uint64_t, uint64_t) { return true; }, error));
    if (!error.empty()) std::printf("  %ls\n", error.c_str());
    CHECK(read(exe) == read(built));
    CHECK(read(dir / L"X-Men Legends Launcher.exe.old") == "MZ the previous launcher");
    remove_previous_launcher(exe);
    CHECK(!fs::exists(dir / L"X-Men Legends Launcher.exe.old"));
}

int main(int argc, char** argv)
{
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    fs::path scratch = fs::path(temp) / (L"xml1-launcher-test-" + std::to_wstring(GetCurrentProcessId()));
    fs::create_directories(scratch);
    ini_test(scratch);
    merge_test();
    install_test(scratch / L"game");
    camera_test();
    sound_repair_test(scratch);
    disc_data_test(scratch);
    bundled_test(scratch);
    updater_test(scratch);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--game" && i + 1 < argc) real_data_test(fs::u8path(argv[++i]), scratch);
        else if (arg == "--image" && i + 1 < argc) image_test(fs::u8path(argv[++i]));
        else if (arg == "--release-check" && i + 1 < argc) release_check(scratch, fs::u8path(argv[++i]));
        else if (arg == "--apply" && i + 1 < argc) {
            // --apply <game folder> [mod folder names...]: the launcher's own
            // mod install, on a real game folder. No names restores the game.
            fs::path game = fs::u8path(argv[++i]);
            std::vector<std::wstring> mods;
            while (i + 1 < argc && argv[i + 1][0] != '-') mods.push_back(widen(argv[++i]));
            for (const auto& m : find_mods(game))
                std::printf("mod %ls: %ls, %u files, %u merged%s%ls\n", m.folder.c_str(), m.name.c_str(), m.files, m.merges,
                            m.problem.empty() ? "" : ", PROBLEM: ", m.problem.c_str());
            auto result = apply_mods(game, mods);
            CHECK(result.ok);
            std::printf("applied %zu mod(s): %s %ls\n", mods.size(), result.ok ? "ok" : "FAILED", result.error.c_str());
        }
        else if (arg == "--repair-disc" && i + 1 < argc) {
            // --repair-disc <game folder>: replace the disc's stale data with
            // the shipped copies from its z/assetsfb.zip, mods kept on top.
            fs::path game = fs::u8path(argv[++i]);
            auto mods = installed_mods(game);
            for (const auto& file : stale_disc_data(game)) std::printf("stale: %ls\n", file.c_str());
            std::wstring error;
            unsigned replaced = repair_disc_data(game, error);
            CHECK(error.empty());
            CHECK(stale_disc_data(game).empty());
            CHECK(installed_mods(game) == mods);
            std::printf("replaced %u file(s) %ls\n", replaced, error.c_str());
        }
        else if (arg == "--install" && i + 3 < argc) {
            full_install_test(fs::u8path(argv[i + 1]), fs::u8path(argv[i + 2]), fs::u8path(argv[i + 3]));
            i += 3;
        }
    }
    std::error_code ec;
    fs::remove_all(scratch, ec);
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
