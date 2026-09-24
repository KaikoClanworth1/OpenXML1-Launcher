# X-Men Legends Launcher

An add-on for [OpenXML1](https://github.com/GTTeancum/OpenXML1xbox), the
Windows port of the original Xbox X-Men Legends. It has four tabs:

- **Install:** sets the game up from your own disc image.
- **Play:** windowed or fullscreen, and the resolution.
- **Settings:** the PC options the game's own menus offer.
- **Mods:** tick the mods to install, and untick to put the game back. The
  launcher comes with its first mod, [Early X-Men Xtraction
  Point](mods/early-xmen-xtraction), ready in the list.

The launcher is separate from OpenXML1, and changes nothing in it. OpenXML1
runs the same with or without it.

## Download and use

1. Download `X-Men-Legends-Launcher.exe` from
   [Releases](https://github.com/KaikoClanworth1/OpenXML1-Launcher/releases/latest).
   It is a single file; no installation is needed.
2. Put it in an empty folder where you want the game, for example
   `D:\Games\X-Men Legends`, and open it.
3. On the **Install** tab, choose your own X-Men Legends (World) Xbox disc
   image, as an ISO or XISO. The launcher never includes or downloads the game.
4. Leave "Download the latest release from GitHub" selected, or choose an
   OpenXML1 release zip you already have. Press **Install**, then **Play**.

The first time the game starts, it prepares its files with its own progress
window. This takes a few minutes and happens once.

If you already have OpenXML1 installed, put the launcher beside
`X-Men Legends.exe` instead, or point it at that folder with **Change...** on
the Play tab.

**Sound fix for 0.9b.** The OpenXML1 0.9b release plays no sound when it is
installed as its README describes; see
[GTTeancum/OpenXML1xbox#6](https://github.com/GTTeancum/OpenXML1xbox/pull/6).
The launcher fixes new installs automatically, and offers the fix when it opens
an affected one.

**Costume crash fix.** The disc has old development copies of
`data/herostat` (all three languages) and `data/stat_rules.xml` beside the
versions the game shipped with, which are inside `z/assetsfb.zip`. The Xbox
reads the zip first, but OpenXML1 prefers loose files and its first-run setup
keeps them, so the old copies are used. They give heroes extra costumes whose
models are not on the disc; Wolverine's 4th costume asks for
`actors/0304.igb`, and the game ends. The launcher puts the shipped versions
in place on new installs, and offers to on existing ones. It only replaces
files that match the disc's old copies byte for byte, and keeps installed
mods.

**Updates.** The launcher checks this repository's releases when it starts, and
from **Check for updates** on the Play tab. Updating downloads the new exe,
checks it, swaps it in and restarts. A running game is not affected.

## Building

```
git submodule update --init
cmake -S . -B build -A Win32
cmake --build build --config Release
build/Release/launcher-test.exe
```

The output is `build/Release/X-Men Legends Launcher.exe`, a single file with no
DLLs. It can live anywhere. If it sits beside `X-Men Legends.exe` it uses that
game folder; otherwise it remembers the folder it was given.

**Why the build is 32-bit:** so its anti-aliasing check asks Direct3D 8 exactly as
the game's 32-bit renderer does.

**What it uses from OpenXML1:** the pinned, unmodified submodule in
`external/OpenXML1`, currently 0.9b, commit `0468023`. Only a few of its sources
are compiled in, so the launcher reads and writes the game's files exactly as
the game does:

- `pc_controls.cpp` for `pc-settings.ini`
- `xmlb.cpp` for compiled XML data
- the vendored miniz, for the release zip
- the DX8 headers

To follow a new OpenXML1 release, move the submodule to it, rebuild and run
the tests.

## Install

This does the OpenXML1 release instructions for the player:

1. **Choose your disc image.** It can be a full ISO or a trimmed XISO. The
   launcher reads the Xbox filesystem itself, and only reads the image. It
   refuses anything that is not X-Men Legends, using the title ID in
   `default.xbe`'s certificate: `4156001E`.
2. **Choose the OpenXML1 release.** Either a zip you already have, or the
   latest one, fetched from GitHub's releases API. The launcher checks the zip
   holds `X-Men Legends.exe`, then extracts it over the disc's files.
3. **Choose where to install.** The default is the launcher's own folder when
   nothing else is in it. Otherwise it is `%USERPROFILE%\Games\X-Men Legends`.

Before copying, the launcher checks free space for the disc plus room to
unpack `assetsfb.zip`. Installing over an existing install keeps saves and
settings, and takes installed mods out first.

The last setup step belongs to the game. On its first launch it unpacks
`assetsfb.zip` and compiles its data, with its own progress window. The
launcher leaves that to the game rather than copying the code, because the
setup must match the game version that was just installed.

## Play and Settings

Display and option changes go into `pc-settings.ini` through the game's own
loader, validator and atomic saver. The launcher therefore cannot write a file
the game would refuse to start with.

| Setting | Key | Notes |
|---|---|---|
| Windowed / Fullscreen | `Fullscreen` | Fullscreen is the game's borderless mode |
| Resolution | `Width`, `Height` | 1920×1080, 1280×720, 640×480: the three the game accepts |
| Anti-aliasing | `FSAA` | only levels this GPU supports are offered |
| Keyboard and mouse, keyboard player, sensitivity, controllers | as in-game | |
| Invert camera up and down | `InvertCameraY` | the game's own option: mouse-drag zoom direction |
| Flip camera left and right | the `CameraLeft` / `CameraRight` bindings | see below |
| View shake | `ViewShake` | |
| Modder mode (Mods tab) | `build.ini` `[BUILD] modderMode` | one line changed; comments and other keys kept |

**Flip camera left and right** swaps each player's CameraLeft and CameraRight
bindings: keys and controller, primary and alternate. The game turns the camera
from those bindings, so turning is reversed without changing the game.

- The swap is visible in the game's Advanced Options, and can be undone there.
- Mouse-drag turning reads the mouse directly, so it is not affected.

**Not offered:**

- **Volume, music, subtitles, vibration and camera angle.** These live in the
  game's signed save data, so they stay in-game.
- **Key bindings.** These are edited in-game, under Options, Advanced.

Before starting the game, the launcher clears `XML1_DX8_RESOLUTION` from its
own environment. Otherwise that variable would override the chosen resolution.

## Mods

**Bundled mods.** Every file under this repository's `mods/` folder is
compiled into the launcher. When it opens a game folder, it writes each one
into the game's `mods` folder if it is missing, or if its `mod.ini` Version
differs from the bundled one. A mod written this way appears in the list like
any other.

**Switch.** `kModsEnabled` in `src/main.cpp` can switch the tab off for a
release. While it is off, the launcher reads nothing from, and writes nothing
to, the game folder for mods.

The format is written for players in `mods\README.txt`, in the game folder.

```
mods\<Mod>\mod.ini       [Mod] Name, Type (character|other), Author, Version, Description
mods\<Mod>\files\...     copied over the game folder at the same paths
mods\<Mod>\merge\...     XML entries merged into the game's file of the same path
mods\<Mod>\append\...    text added to the end of the game's file (scripts, text data)
```

**Merging.** Several character mods can share `data/herostat.eng` this way.

- Each top-level entry replaces the game's entry with the same tag and `name`,
  or is appended.
- Attributes on the fragment's root element set those on the game file's root.
  An example is `<MISSION maxheros="4">` for `data/missions/alison.eng`.
- An English fragment also goes into the other languages the game has.
- Everything written is compiled to the binary form the game loads. The
  launcher uses the game's own compiler for this, with a round-trip check.

**Packages.** A fragment under `merge\packages\...\name.pkgb` is plain XML text
inside `<packagedef>`. The game's package (binary XMLB) is decoded, the entries
merged, and the result compiled back with the same round-trip check. An entry
the package already has, word for word, is not added twice.

**Placement.** A new entry goes after the game's last entry with the same tag,
so it sits with its own kind. Map precaches, for example, stay at the top.

**Appending.** A file under `append\` holds only new lines. They are added to
the end of the game's file of the same path, in that file's own line endings.
This lets a mod add script commands without shipping a copy of the game's
script.

**Example.** [`mods/early-xmen-xtraction`](mods/early-xmen-xtraction) uses
both. It is the first mod, tested in the game.

**Undoing.** Every file written is recorded, and its original backed up, in
`mods\.launcher\` first. Any new selection starts from the untouched game.
Unticking everything restores it byte for byte, and a failed install is rolled
back.

**What mods cannot touch:** saves, settings, `default.xbe`, programs, the mod
store and paths outside the game folder. A mod that tries is greyed out, with
the reason. When two mods replace the same file, the launcher lists the clash
before installing.

**The game's first-run setup** compiles every XML file under the game folder,
including those inside `mods\`. The compiled copies it leaves beside `merge\`
fragments are ignored.

**Not yet:**

- Merging the selection-screen portrait package (`characters_heads.pkgb`).
  Mods that ship their own copy conflict, and the conflict is reported.
- Checking the 48-slot character pool.

## Verification

**`launcher-test`** covers:

- the updater: version comparison, and swapping the exe of a program that is
  still running, including putting it back when the swap fails
- the 0.9b sound repair
- the costume data repair leaves files that are not the disc's old copies alone
- INI editing
- merge rules
- a three-mod install with a conflict, switching selections, restoring byte
  for byte, and rolling back a broken mod
- the camera flip: the game's validator accepts it, and flipping twice restores
  the file exactly
- ignoring setup-generated binaries in `merge\`

Optional arguments test real data:

- `--game <folder>` merges and compiles a copy of that game's `herostat.eng`.
- `--image <iso>` reads a real disc image.
- `--install <iso> <zip> <new folder>` runs a whole installation.
- `--repair-disc <game>` runs the costume data repair on a real game folder.

**`--capture <dir> --game <folder>`** makes the launcher render its own window
to one PNG per tab. It draws off screen and never activates, so no desktop
capture is involved.

## Releasing

1. Raise the version in `src/version.h`.
2. Build Release and run `launcher-test`.
3. Tag `vMAJOR.MINOR.PATCH` and publish a GitHub release with the exe attached
   as `X-Men-Legends-Launcher.exe`.

The updater takes the newest release's tag and its `.exe` asset, so every
release needs exactly one `.exe` attached.
