#include "mods.h"
#include "text.h"
#include "xmlb.h"
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>

namespace launcher {
namespace {

const wchar_t* const kLanguages[] = {L".eng", L".fre", L".ger", L".ita", L".spa", L".pol", L".rus"};

fs::path state_dir(const fs::path& game) { return mods_dir(game) / L".launcher"; }
fs::path ledger_path(const fs::path& game) { return state_dir(game) / L"installed.txt"; }
fs::path enabled_path(const fs::path& game) { return state_dir(game) / L"enabled.txt"; }
fs::path backup_dir(const fs::path& game) { return state_dir(game) / L"backup"; }

std::wstring key_of(const fs::path& relative) { return lower(relative.lexically_normal().wstring()); }

// A mod may only write game data. Saves, settings, the executable and the
// launcher's own state are off limits, as is anything outside the folder.
std::wstring unsafe_path(const fs::path& relative)
{
    if (relative.empty() || relative.is_absolute() || relative.has_root_name())
        return L"absolute path";
    auto first = lower(relative.begin()->wstring());
    static const wchar_t* const kReserved[] = {L"mods", L"udata", L"tdata", L"build", L"logs", L"captures", L"z", L"runtime"};
    for (auto reserved : kReserved)
        if (first == reserved) return L"writes into " + relative.begin()->wstring() + L"\\";
    for (const auto& part : relative) {
        auto name = part.wstring();
        if (name == L".." || name == L".") return L"leaves the game folder";
        if (!name.empty() && name[0] == L'.') return L"hidden path";
    }
    auto file = lower(relative.filename().wstring());
    static const wchar_t* const kProtected[] = {L"default.xbe", L"x-men legends.exe", L"build.ini", L"pc-settings.ini"};
    for (auto name : kProtected)
        if (relative == relative.filename() && file == name) return L"replaces " + relative.wstring();
    auto ext = lower(relative.extension().wstring());
    if (ext == L".exe" || ext == L".dll") return L"installs a program";
    return {};
}

bool is_link(const fs::path& path)
{
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

std::vector<fs::path> files_under(const fs::path& root)
{
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return out;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (is_link(it->path())) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file(ec)) out.push_back(fs::relative(it->path(), root));
    }
    std::sort(out.begin(), out.end());
    return out;
}

// The game's first-run setup compiles every XML text file under the game
// folder, mods\ included, into a binary copy beside it. In merge\ those copies
// are never the mod's own, so they are left out.
std::vector<fs::path> fragments_under(const fs::path& root)
{
    auto files = files_under(root);
    std::vector<fs::path> out;
    for (const auto& file : files) {
        auto ext = file.extension().u8string();
        if (ext.size() > 1 && (ext.back() == 'b' || ext.back() == 'B')) {
            fs::path source = file;
            source.replace_extension(fs::u8path(ext.substr(0, ext.size() - 1)));
            if (xml1::xml_text_extension(source.extension().u8string()) && std::binary_search(files.begin(), files.end(), source))
                continue;
        }
        out.push_back(file);
    }
    return out;
}

bool language_file(const fs::path& relative)
{
    auto ext = lower(relative.extension().wstring());
    for (auto language : kLanguages) if (ext == language) return true;
    return false;
}

bool text_data(const fs::path& relative) { return xml1::xml_text_extension(relative.extension().u8string()); }

// PKGB packages are the same binary XML (XMLB) as the compiled data, so they
// can be merged through their decoded text and compiled back.
bool package(const fs::path& relative) { return lower(relative.extension().wstring()) == L".pkgb"; }

fs::path binary_of(const fs::path& relative) { fs::path b = relative; b += L"b"; return b; }

void write_atomic(const fs::path& target, const std::string& bytes)
{
    fs::create_directories(target.parent_path());
    fs::path temp = target; temp += L".launcher-tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), (std::streamsize)bytes.size());
        out.close();
        if (!out) throw std::runtime_error("cannot write " + target.u8string());
    }
    if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(temp);
        throw std::runtime_error("cannot replace " + target.u8string() + " (is the game running?)");
    }
}

// Keeps the ledger of every file touched, so that restore can undo exactly
// what was done even if the launcher is closed halfway through.
struct Ledger {
    fs::path game;
    std::set<std::wstring> seen;

    void touch(const fs::path& relative)
    {
        if (!seen.insert(key_of(relative)).second) return;
        fs::path target = game / relative;
        bool existed = fs::exists(target);
        if (existed) {
            fs::path backup = backup_dir(game) / relative;
            fs::create_directories(backup.parent_path());
            fs::copy_file(target, backup, fs::copy_options::overwrite_existing);
        }
        std::ofstream out(ledger_path(game), std::ios::binary | std::ios::app);
        out << (existed ? "B\t" : "N\t") << relative.u8string() << "\n";
        out.close();
        if (!out) throw std::runtime_error("cannot record " + relative.u8string());
    }
};

void compile_into(const fs::path& game, const fs::path& relative, const std::string& text, Ledger& ledger)
{
    xml1::BinaryXml binary;
    try {
        binary = xml1::compile_xmlb(text);
        auto decoded = xml1::decode_xmlb(binary.data(), (unsigned)binary.size());
        if (xml1::compile_xmlb(decoded) != binary) throw std::runtime_error("round-trip check failed");
    } catch (const std::exception& e) {
        throw std::runtime_error(relative.u8string() + " does not compile: " + e.what());
    }
    fs::path target = binary_of(relative);
    ledger.touch(target);
    write_atomic(game / target, std::string(binary.begin(), binary.end()));
}

// Put back every original the ledger recorded, newest first.
bool restore(const fs::path& game, std::wstring& error)
{
    fs::path ledger = ledger_path(game);
    std::error_code ec;
    if (fs::exists(ledger)) {
        std::vector<std::string> lines;
        {
            std::ifstream in(ledger, std::ios::binary);
            for (std::string line; std::getline(in, line);) if (line.size() > 2) lines.push_back(line);
        }
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            fs::path relative = fs::u8path(it->substr(2));
            fs::path target = game / relative;
            if ((*it)[0] == 'B') {
                fs::path backup = backup_dir(game) / relative;
                if (!fs::exists(backup)) { error = L"The backup of " + relative.wstring() + L" is missing."; return false; }
                if (!CopyFileW(backup.c_str(), target.c_str(), FALSE)) {
                    error = L"Could not restore " + relative.wstring() + L". Is the game running?";
                    return false;
                }
            } else if (fs::exists(target) && !fs::remove(target, ec)) {
                error = L"Could not remove " + relative.wstring() + L". Is the game running?";
                return false;
            }
        }
        fs::remove(ledger, ec);
    }
    fs::remove_all(backup_dir(game), ec);
    fs::remove(enabled_path(game), ec);
    return true;
}

// ---- top-level XML entries -------------------------------------------------

struct Entry { size_t begin, end; std::string tag, name; };
struct Document { std::string root; size_t root_begin = 0, content_begin = 0, content_end = 0; std::vector<Entry> entries; };

size_t tag_end(const std::string& text, size_t at)
{
    char quote = 0;
    for (size_t i = at + 1; i < text.size(); ++i) {
        char c = text[i];
        if (quote) { if (c == quote) quote = 0; }
        else if (c == '"' || c == '\'') quote = c;
        else if (c == '>') return i + 1;
    }
    return std::string::npos;
}

std::string tag_name(const std::string& text, size_t at)
{
    size_t i = at + 1, j = i;
    while (j < text.size() && !isspace((unsigned char)text[j]) && text[j] != '>' && text[j] != '/') ++j;
    return text.substr(i, j - i);
}

std::string attribute_value(const std::string& tag, const std::string& attr)
{
    for (size_t at = tag.find(attr); at != std::string::npos; at = tag.find(attr, at + 1)) {
        if (at == 0 || !isspace((unsigned char)tag[at - 1])) continue;
        size_t i = at + attr.size();
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size() || tag[i] != '=') continue;
        ++i;
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size() || (tag[i] != '"' && tag[i] != '\'')) continue;
        size_t close = tag.find(tag[i], i + 1);
        if (close == std::string::npos) return {};
        return tag.substr(i + 1, close - i - 1);
    }
    return {};
}

bool skip_markup(const std::string& text, size_t& at)
{
    auto skip_to = [&](const char* end) {
        size_t found = text.find(end, at);
        at = found == std::string::npos ? text.size() : found + strlen(end);
        return true;
    };
    if (text.compare(at, 4, "<!--") == 0) return skip_to("-->");
    if (text.compare(at, 2, "<?") == 0) return skip_to("?>");
    if (text.compare(at, 2, "<!") == 0) return skip_to(">");
    return false;
}

bool parse(const std::string& text, Document& doc, std::string& error)
{
    size_t at = 0;
    for (;;) {
        at = text.find('<', at);
        if (at == std::string::npos) { error = "no root element"; return false; }
        if (!skip_markup(text, at)) break;
    }
    size_t end = tag_end(text, at);
    if (end == std::string::npos) { error = "unterminated root tag"; return false; }
    if (text[end - 2] == '/') { error = "the root element is empty"; return false; }
    doc.root = tag_name(text, at);
    doc.root_begin = at;
    doc.content_begin = end;
    int depth = 0;
    Entry current{};
    at = end;
    for (;;) {
        at = text.find('<', at);
        if (at == std::string::npos) { error = "<" + doc.root + "> is never closed"; return false; }
        if (skip_markup(text, at)) continue;
        end = tag_end(text, at);
        if (end == std::string::npos) { error = "unterminated tag"; return false; }
        if (text[at + 1] == '/') {
            if (depth == 0) { doc.content_end = at; return true; }
            if (--depth == 0) { current.end = end; doc.entries.push_back(current); }
        } else {
            bool closed = text[end - 2] == '/';
            if (depth == 0) {
                std::string open = text.substr(at, end - at);
                std::string key = attribute_value(open, "name");
                // A placed-object group is identified by the entity type it places.
                if (key.empty() && _stricmp(tag_name(text, at).c_str(), "entinst") == 0) key = attribute_value(open, "type");
                current = {at, 0, tag_name(text, at), key};
                if (closed) { current.end = end; doc.entries.push_back(current); }
                else depth = 1;
            } else if (!closed) {
                ++depth;
            }
        }
        at = end;
    }
}

bool same(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return tolower((unsigned char)x) == tolower((unsigned char)y); });
}

struct Attribute { std::string name, value; size_t value_begin, value_end; };

// The name="value" pairs of a start tag, with where each value sits in it.
std::vector<Attribute> attributes(const std::string& tag)
{
    std::vector<Attribute> out;
    size_t i = 1;
    while (i < tag.size() && !isspace((unsigned char)tag[i]) && tag[i] != '>' && tag[i] != '/') ++i;  // tag name
    while (i < tag.size()) {
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        size_t name_begin = i;
        while (i < tag.size() && !isspace((unsigned char)tag[i]) && tag[i] != '=' && tag[i] != '>' && tag[i] != '/') ++i;
        if (i == name_begin) break;
        std::string name = tag.substr(name_begin, i - name_begin);
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size() || tag[i] != '=') continue;
        ++i;
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size() || (tag[i] != '"' && tag[i] != '\'')) break;
        char quote = tag[i];
        size_t close = tag.find(quote, i + 1);
        if (close == std::string::npos) break;
        out.push_back({name, tag.substr(i + 1, close - i - 1), i + 1, close});
        i = close + 1;
    }
    return out;
}

// Sets each of `changes` on the start tag: replacing a value the tag already
// has (names compared without case), or adding the attribute at its end.
std::string with_attributes(std::string tag, const std::vector<Attribute>& changes)
{
    for (const auto& change : changes) {
        bool done = false;
        for (const auto& existing : attributes(tag))
            if (same(existing.name, change.name)) {
                tag.replace(existing.value_begin, existing.value_end - existing.value_begin, change.value);
                done = true;
                break;
            }
        if (!done) {
            size_t close = tag.size() - 1;
            if (close > 0 && tag[close - 1] == '/') --close;
            tag.insert(close, " " + change.name + "=\"" + change.value + "\"");
        }
    }
    return tag;
}

std::string read_bytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.u8string());
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

struct Plan {
    std::map<std::wstring, std::pair<fs::path, std::wstring>> copies;  // key -> (relative, source file)
    std::map<std::wstring, std::vector<fs::path>> merges;              // key -> fragment files, in order
    std::map<std::wstring, fs::path> merge_targets;                    // key -> relative
    std::map<std::wstring, std::vector<fs::path>> appends;             // key -> text files, in order
    std::map<std::wstring, fs::path> append_targets;                   // key -> relative
    std::map<std::wstring, std::wstring> copy_owner;
    std::vector<std::wstring> conflicts;
};

Plan make_plan(const fs::path& game, const std::vector<std::wstring>& enabled)
{
    Plan plan;
    for (const auto& folder : enabled) {
        fs::path mod = mods_dir(game) / folder;
        for (const auto& relative : files_under(mod / L"files")) {
            auto key = key_of(relative);
            auto owner = plan.copy_owner.find(key);
            if (owner != plan.copy_owner.end() && owner->second != folder)
                plan.conflicts.push_back(relative.wstring() + L": " + owner->second + L", then " + folder + L" (" + folder + L" wins)");
            plan.copy_owner[key] = folder;
            plan.copies[key] = {relative, (mod / L"files" / relative).wstring()};
        }
        auto fragments = fragments_under(mod / L"merge");
        for (const auto& relative : fragments) {
            auto add = [&](const fs::path& target) {
                auto key = key_of(target);
                plan.merge_targets[key] = target;
                plan.merges[key].push_back(mod / L"merge" / relative);
            };
            add(relative);
            // An English-only fragment also goes into the other languages the
            // game has, so a new character appears whatever the text language.
            if (lower(relative.extension().wstring()) != L".eng") continue;
            for (auto language : kLanguages) {
                fs::path other = relative; other.replace_extension(language);
                if (other == relative) continue;
                bool own = std::any_of(fragments.begin(), fragments.end(), [&](const fs::path& f) { return key_of(f) == key_of(other); });
                if (!own && fs::exists(game / other)) add(other);
            }
        }
        for (const auto& relative : files_under(mod / L"append")) {
            auto key = key_of(relative);
            plan.append_targets[key] = relative;
            plan.appends[key].push_back(mod / L"append" / relative);
        }
    }
    return plan;
}

// Only text the game reads as text can be appended to.
bool appendable(const fs::path& relative)
{
    auto ext = lower(relative.extension().wstring());
    return text_data(relative) || ext == L".py" || ext == L".txt";
}

// `addition` added at the end of `text`, in the file's own line endings.
std::string append_text(std::string text, std::string addition)
{
    const bool crlf = text.find("\r\n") != std::string::npos;
    std::string plain;
    for (size_t i = 0; i < addition.size(); ++i)
        if (!(addition[i] == '\r' && i + 1 < addition.size() && addition[i + 1] == '\n')) plain += addition[i];
    std::string converted;
    for (char c : plain) { if (c == '\n' && crlf) converted += '\r'; converted += c; }
    const char* newline = crlf ? "\r\n" : "\n";
    if (!text.empty() && text.back() != '\n') text += newline;
    text += converted;
    if (!text.empty() && text.back() != '\n') text += newline;
    return text;
}

} // namespace

fs::path mods_dir(const fs::path& game) { return game / L"mods"; }

bool merge_xml(const std::string& base, const std::string& fragment, std::string& out, std::string& error)
{
    Document b, f;
    if (!parse(base, b, error)) { error = "game file: " + error; return false; }
    if (!parse(fragment, f, error)) { error = "mod file: " + error; return false; }
    if (!same(b.root, f.root)) { error = "mod file's root is <" + f.root + ">, the game's is <" + b.root + ">"; return false; }
    const char* newline = base.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::vector<std::string> replaced(b.entries.size());
    std::vector<bool> has(b.entries.size(), false), removed(b.entries.size(), false);
    std::vector<std::pair<Entry, std::string>> appended;
    for (const auto& entry : f.entries) {
        std::string text = fragment.substr(entry.begin, entry.end - entry.begin);
        bool done = false;
        if (attribute_value(text.substr(0, text.find('>') + 1), "mod-remove") == "true") {
            for (size_t i = 0; i < b.entries.size(); ++i)
                if (same(b.entries[i].tag, entry.tag) && same(b.entries[i].name, entry.name)) removed[i] = true;
            continue;
        }
        if (!entry.name.empty()) {
            for (size_t i = 0; i < b.entries.size() && !done; ++i)
                if (same(b.entries[i].tag, entry.tag) && same(b.entries[i].name, entry.name)) {
                    replaced[i] = text; has[i] = true; done = true;
                }
            for (auto& added : appended)
                if (!done && same(added.first.tag, entry.tag) && same(added.first.name, entry.name)) { added.second = text; done = true; }
        }
        if (!done && entry.name.empty())
            for (const auto& existing : b.entries)
                if (same(existing.tag, entry.tag) && same(base.substr(existing.begin, existing.end - existing.begin), text)) {
                    done = true;  // already there, word for word
                    break;
                }
        if (!done) appended.push_back({entry, text});
    }
    // A new entry goes after the game's last entry with the same tag, so it
    // lands where the game keeps that kind (map precaches sit at the top,
    // before the entities, and are only honoured there). Otherwise at the end.
    std::vector<std::vector<std::string>> after(b.entries.size());
    std::vector<std::string> at_end;
    for (const auto& added : appended) {
        size_t last = b.entries.size();
        for (size_t i = 0; i < b.entries.size(); ++i)
            if (same(b.entries[i].tag, added.first.tag)) last = i;
        if (last < b.entries.size()) after[last].push_back(added.second);
        else at_end.push_back(added.second);
    }
    out.clear();
    size_t cursor = 0;
    for (size_t i = 0; i < b.entries.size(); ++i) {
        if (removed[i]) {
            // Drop the entry and the line break that ended it.
            out.append(base, cursor, b.entries[i].begin - cursor);
            while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
            cursor = b.entries[i].end;
            if (cursor < base.size() && base[cursor] == '\r') ++cursor;
            if (cursor < base.size() && base[cursor] == '\n') ++cursor;
            continue;
        }
        out.append(base, cursor, b.entries[i].begin - cursor);
        out += has[i] ? replaced[i] : base.substr(b.entries[i].begin, b.entries[i].end - b.entries[i].begin);
        for (const auto& text : after[i]) { out += newline; out += text; }
        cursor = b.entries[i].end;
    }
    out.append(base, cursor, b.content_end - cursor);
    if (!at_end.empty() && !out.empty() && out.back() != '\n') out += newline;
    for (const auto& text : at_end) { out += text; out += newline; }
    out.append(base, b.content_end, std::string::npos);
    // Attributes on the fragment's root set the same attributes on the game's
    // root; the root tag is unchanged in `out` up to this point.
    auto changes = attributes(fragment.substr(f.root_begin, f.content_begin - f.root_begin));
    if (!changes.empty()) {
        std::string root = out.substr(b.root_begin, b.content_begin - b.root_begin);
        out.replace(b.root_begin, root.size(), with_attributes(root, changes));
    }
    return true;
}

std::vector<ModInfo> find_mods(const fs::path& game)
{
    std::vector<ModInfo> mods;
    std::error_code ec;
    if (!fs::is_directory(mods_dir(game), ec)) return mods;
    for (const auto& entry : fs::directory_iterator(mods_dir(game), ec)) {
        if (!entry.is_directory() || is_link(entry.path())) continue;
        ModInfo mod;
        mod.folder = entry.path().filename().wstring();
        if (mod.folder.empty() || mod.folder[0] == L'.') continue;
        mod.name = mod.folder;
        Ini ini;
        if (ini.load(entry.path() / L"mod.ini")) {
            auto value = [&](const char* key) { return widen(ini.get("Mod", key)); };
            if (!value("Name").empty()) mod.name = value("Name");
            mod.author = value("Author");
            mod.version = value("Version");
            mod.description = value("Description");
            mod.character = lower(value("Type")) == L"character";
        }
        auto files = files_under(entry.path() / L"files");
        auto merges = fragments_under(entry.path() / L"merge");
        auto appends = files_under(entry.path() / L"append");
        mod.files = (unsigned)files.size();
        mod.merges = (unsigned)merges.size();
        mod.appends = (unsigned)appends.size();
        for (const auto& relative : files) {
            auto why = unsafe_path(relative);
            if (!why.empty()) { mod.problem = relative.wstring() + L": " + why; break; }
        }
        for (const auto& relative : merges) {
            if (!mod.problem.empty()) break;
            auto why = unsafe_path(relative);
            if (why.empty() && !text_data(relative) && !package(relative)) why = L"only XML data and packages can be merged";
            if (why.empty() && !fs::exists(game / relative)) why = L"the game has no such file to merge into";
            if (!why.empty()) mod.problem = L"merge\\" + relative.wstring() + L": " + why;
        }
        for (const auto& relative : appends) {
            if (!mod.problem.empty()) break;
            auto why = unsafe_path(relative);
            if (why.empty() && !appendable(relative)) why = L"only text files can be appended to";
            if (why.empty() && !fs::exists(game / relative)) why = L"the game has no such file to append to";
            if (!why.empty()) mod.problem = L"append\\" + relative.wstring() + L": " + why;
        }
        if (mod.problem.empty() && !mod.files && !mod.merges && !mod.appends) mod.problem = L"has no files\\, merge\\ or append\\ folder";
        mods.push_back(mod);
    }
    std::sort(mods.begin(), mods.end(), [](const ModInfo& a, const ModInfo& b) { return lower(a.name) < lower(b.name); });
    return mods;
}

std::vector<std::wstring> installed_mods(const fs::path& game)
{
    std::vector<std::wstring> out;
    std::ifstream in(enabled_path(game), std::ios::binary);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(widen(line));
    }
    return out;
}

std::vector<std::wstring> find_conflicts(const fs::path& game, const std::vector<std::wstring>& enabled)
{
    return make_plan(game, enabled).conflicts;
}

ApplyResult apply_mods(const fs::path& game, const std::vector<std::wstring>& enabled)
{
    ApplyResult result;
    if (!restore(game, result.error)) return result;
    if (enabled.empty()) { result.ok = true; return result; }
    Plan plan = make_plan(game, enabled);
    result.conflicts = plan.conflicts;
    Ledger ledger{game, {}};
    try {
        fs::create_directories(state_dir(game));
        for (const auto& [key, copy] : plan.copies) {
            const auto& [relative, source] = copy;
            auto why = unsafe_path(relative);
            if (!why.empty()) throw std::runtime_error(relative.u8string() + ": " + narrow(why));
            ledger.touch(relative);
            fs::create_directories((game / relative).parent_path());
            if (!CopyFileW(source.c_str(), (game / relative).c_str(), FALSE))
                throw std::runtime_error("cannot install " + relative.u8string() + " (is the game running?)");
            ++result.written;
        }
        std::set<std::wstring> merged;
        for (const auto& [key, fragments] : plan.merges) {
            const fs::path& relative = plan.merge_targets[key];
            auto why = unsafe_path(relative);
            if (!why.empty()) throw std::runtime_error(relative.u8string() + ": " + narrow(why));
            std::string text = read_bytes(game / relative);
            const bool binary = package(relative);
            if (binary) {
                try { text = xml1::decode_xmlb(text.data(), (unsigned)text.size()); }
                catch (const std::exception& e) { throw std::runtime_error(relative.u8string() + " is not a readable package: " + e.what()); }
            }
            for (const auto& fragment : fragments) {
                std::string out, error;
                if (!merge_xml(text, read_bytes(fragment), out, error))
                    throw std::runtime_error(fragment.u8string() + ": " + error);
                text.swap(out);
            }
            ledger.touch(relative);
            if (binary) {
                xml1::BinaryXml compiled;
                try {
                    compiled = xml1::compile_xmlb(text);
                    if (xml1::compile_xmlb(xml1::decode_xmlb(compiled.data(), (unsigned)compiled.size())) != compiled)
                        throw std::runtime_error("round-trip check failed");
                } catch (const std::exception& e) {
                    throw std::runtime_error(relative.u8string() + " does not compile: " + e.what());
                }
                write_atomic(game / relative, std::string(compiled.begin(), compiled.end()));
            } else {
                write_atomic(game / relative, text);
                merged.insert(key);
            }
            ++result.written;
        }
        for (const auto& [key, additions] : plan.appends) {
            const fs::path& relative = plan.append_targets[key];
            auto why = unsafe_path(relative);
            if (why.empty() && !appendable(relative)) why = L"only text files can be appended to";
            if (!why.empty()) throw std::runtime_error(relative.u8string() + ": " + narrow(why));
            std::string text = read_bytes(game / relative);
            for (const auto& addition : additions) text = append_text(text, read_bytes(addition));
            ledger.touch(relative);
            write_atomic(game / relative, text);
            if (text_data(relative)) { merged.insert(key); plan.merge_targets[key] = relative; }
            ++result.written;
        }
        // The game loads the compiled form, so compile what was just written
        // unless the mod shipped a compiled file of its own.
        for (const auto& [key, copy] : plan.copies) {
            const auto& relative = copy.first;
            if (!text_data(relative) || merged.count(key) || plan.copies.count(key_of(binary_of(relative)))) continue;
            compile_into(game, relative, read_bytes(game / relative), ledger);
        }
        for (const auto& key : merged) {
            const fs::path& relative = plan.merge_targets[key];
            compile_into(game, relative, read_bytes(game / relative), ledger);
        }
        std::string list;
        for (const auto& folder : enabled) list += narrow(folder) + "\n";
        write_atomic(enabled_path(game), list);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = widen(e.what());
        std::wstring undo;
        if (!restore(game, undo)) result.error += L"\n\nUndoing the partial install also failed: " + undo;
        else result.error += L"\n\nThe game's files were put back as they were.";
    }
    return result;
}

void ensure_mods_readme(const fs::path& game)
{
    fs::path readme = mods_dir(game) / L"README.txt";
    std::error_code ec;
    if (fs::exists(readme, ec)) return;
    fs::create_directories(mods_dir(game), ec);
    std::ofstream out(readme, std::ios::binary);
    out <<
        "X-Men Legends mods\r\n"
        "==================\r\n\r\n"
        "Put each mod in its own folder here, then tick it on the launcher's Mods tab.\r\n"
        "The launcher installs the ticked mods when you press Play or Apply, and puts\r\n"
        "the game's own files back when you untick them.\r\n\r\n"
        "  mods\\MyMod\\mod.ini\r\n"
        "  mods\\MyMod\\files\\...   copied over the game folder, same paths\r\n"
        "  mods\\MyMod\\merge\\...   XML data merged into the game's file of the same path\r\n"
        "  mods\\MyMod\\append\\...  text added to the end of the game's file (scripts)\r\n\r\n"
        "mod.ini:\r\n\r\n"
        "  [Mod]\r\n"
        "  Name = Deadpool\r\n"
        "  Type = character        ; character or other\r\n"
        "  Author = you\r\n"
        "  Version = 1.0\r\n"
        "  Description = Adds Deadpool to the roster.\r\n\r\n"
        "Merging: a file such as merge\\data\\herostat.eng holds just the new entries,\r\n"
        "inside the same root element as the game's file:\r\n\r\n"
        "  <characters>\r\n"
        "  <stats name=\"Deadpool\" ... >\r\n"
        "  ...\r\n"
        "  </stats>\r\n"
        "  </characters>\r\n\r\n"
        "An entry with the same tag and name as one of the game's replaces it; any\r\n"
        "other entry is added. Several character mods can all merge into herostat\r\n"
        "this way. An English (.eng) file is also merged into the other languages.\r\n"
        "Attributes on the fragment's root element set those on the game file's\r\n"
        "root, for example <MISSION maxheros=\"4\"> in merge\\data\\missions.\r\n\r\n"
        "Appending: append\\scripts\\...\\name.py holds only the new lines; they are\r\n"
        "added to the end of the game's script in its own line endings.\r\n\r\n"
        "Text data is compiled to the form the game loads (herostat.eng becomes\r\n"
        "herostat.engb) automatically.\r\n\r\n"
        "Two mods that replace the same file under files\\ conflict: the one lower in\r\n"
        "the list wins, and the launcher tells you which files are affected.\r\n";
}

} // namespace launcher
