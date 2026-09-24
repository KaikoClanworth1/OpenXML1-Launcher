# Danger Room: Unlock Everything

Opens the whole **main-menu** Danger Room (the one on the title screen, not the in-game training room):

- **Sparring**, **Records** and **Skirmish**
- all four skirmish game types: Brawl, King Of The Hill, Last Man Standing, Last Man Ladder
- every arena the modes use: Danger Room, Mansion, NYC Rooftops, NYC Courtyard, Snow Courtyard,
  Research Facility, Sewers, Engine Room, Power Plant
- every playable hero and villain: the X-Men, installed character mods, the story villains (Blob,
  Avalanche, Havok, Juggernaut, Magneto, Marrow, Mystique, Pyro, Sabretooth, Toad), Acolytes,
  Brotherhood, GRSO, Morlocks, Sentinels and Astral Shades

## How it works

The mod is one `build.ini` setting:

    [Settings]
    dangerRoomUnlockAll = 1

**This needs an OpenXML1 build that has the `dangerRoomUnlockAll` option.** It is not in OpenXML1 0.9b,
which ignores the setting. The option is on a local OpenXML1 branch while it is tested.

Normally the menu unlocks by progress:
- Sparring needs a hero above level 5.
- Skirmish needs one above level 15; its game types need 15, 18, 23 and 30.
- Characters unlock by finishing training courses.
- Arenas unlock one after another.

With the option on, those checks read as unlocked where the menu makes them. Nothing is written to your
saves, so they keep their real progress, and unticking the mod brings the normal rules back.

## Tested in the game

Hidden and muted on a fresh profile with no saves:
- Sparring lists the nine arenas and every character above. An Astral Shade versus Toad match starts
  and plays.
- Skirmish lists the arenas and all four game types, and reaches character selection.
- With the mod unticked, the same game shows "There are no unlocked characters for sparring" again.
