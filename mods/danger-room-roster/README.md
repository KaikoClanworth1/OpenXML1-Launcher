# Danger Room: More Characters

Adds bosses to the **main-menu** Danger Room's Sparring and Skirmish rosters. They are listed at the end,
after Toad:

| In the roster | From the story | Changes for the Danger Room |
|---|---|---|
| **Master Mold** | the final-act boss (`mastermoldone`) | 0.55 scale, so he fits the arenas and the camera; health scale 3 instead of 30 |
| **Juggernaut (Boss)** | the Muir Island boss (`JuggernautAct3`, level 28) | named apart from the roster's own Juggernaut, the level-5 flashback one |
| **Sentinel Platform Mark II** | the Hive's tougher spider platform (`SentinelSpider_b`, level 36) | none. The ordinary spider, "Sentinel Weapons Platform", was already in the roster |
| **Shadow King** | the final Astral Plane fight, first phase with shield and spear (`shadowking`) | none |
| **Shadow King (Final Form)** | the same fight's second phase (`shadowkingtwo`) | named apart from the first phase |

Each has the boss's own moves, powers and immunities. No training course unlocks them, so use this with
**Danger Room: Unlock Everything**.

## How it works

The story bosses are left exactly as they are. Each Danger Room version is a separate character added to
`data/npcstat.eng`. It is a copy of the boss's entry under a new name (`MasterMoldDR`, `JuggernautDR`,
and so on), with:
- `playable="true"`, which is what puts a character in the Danger Room roster
- the adjustments above

The game finds a character's packages by name and skin (`<name>_<skin>.pkgb`). `mod.ini` `[Copy]` makes
each boss's two packages available under its new name, for example:

    packages/generated/characters/mastermoldone_5901.pkgb    -> mastermolddr_5901.pkgb
    packages/generated/characters/mastermoldone_5901_nc.pkgb -> mastermolddr_5901_nc.pkgb

The mod is written by `tools/make-danger-room-roster.py` from the game's own `npcstat.eng`. To add a
character, add a line to its `ROSTER` table: the new name, the game entry to copy, and any attributes to
change.

## Tested in the game

Hidden and muted, with Danger Room: Unlock Everything, on OpenXML1's `dangerRoomUnlockAll` build. All
five appear in the Sparring roster. Each was played against Toad in the Danger Room arena, with no crash:

| Character | As the player |
|---|---|
| Master Mold | whole body on screen; moves, arm sweep hits, A/B/Boost fire the arm-cannon beam, stun beam and missiles. Also loads and fights as an AI champion |
| Juggernaut (Boss) | moves, attacks and uses his powers |
| Sentinel Platform Mark II | loads and fires its powers |
| Shadow King | shield and spear; attacks and fires his powers |
| Shadow King (Final Form) | attacks and fires his powers |

Not yet tested:
- their Xtremes
- full matches to the end
- Skirmish with them
- each boss as an AI champion, except Master Mold
