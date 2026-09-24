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

Each has the boss's own moves, powers and immunities, except the powers that summon other characters.
No training course unlocks them, so use this with **Danger Room: Unlock Everything**.

## Summoning powers, replaced

In the story these powers fill the fight with extra characters: shades, copies of the Shadow King, and
Master Mold's spider mines. The Danger Room versions use their own powerstyles, with only these moves
replaced by the boss's own attacks:

| Boss | In the story | In the Danger Room |
|---|---|---|
| Shadow King | Boost: six Spectral Tyrant shades, and immune to damage for 31 seconds | throws three spears at once |
| Shadow King | Xtreme: ends that immunity | a wider, stronger version of his spear spin |
| Shadow King (Final Form) | Boost: six Spectral Tyrant shades | a stronger, longer fire breath |
| Shadow King (Final Form) | Xtreme: two copies of himself | the same mirror-image effect, with a blast all around him |
| Master Mold | Xtreme: spider bombs that hatch into spider mines | a barrage of eight of his Boost missiles |

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

For the three bosses above, the mod ships its own powerstyles (`files/data/powerstyles/ps_*dr*.xml`) and
packages (`files/packages/generated/characters/*dr_*.pkgb`, written as text). Each package is the story one
with its powerstyle line pointing to the new file.

The mod is written by `tools/make-danger-room-roster.py` from the game's own data. The generator decodes
the packages and edits the story powerstyles move by move. To add a character, add a line to its
`ROSTER` table: the new name, the game entry to copy, any attributes to change and, if needed, a
powerstyle change.

## Tested in the game

Hidden and muted, with Danger Room: Unlock Everything, on OpenXML1's `dangerRoomUnlockAll` build. All
five appear in the Sparring roster.

Each was played **as the player** against Toad in the Danger Room arena, with no crash:

| Character | Seen |
|---|---|
| Master Mold | whole body on screen; moves, arm sweep hits, A/B/Boost fire the arm-cannon beam, stun beam and missiles |
| Juggernaut (Boss) | moves, attacks and uses his powers |
| Sentinel Platform Mark II | loads and fires its powers |
| Shadow King | shield and spear; attacks and fires his powers |
| Shadow King (Final Form) | attacks and fires his powers |

The three bosses with replaced powers were also watched **as the AI opponent** against an idle Beast
for 40 seconds each:
- **Shadow King:** fought with no shades.
- **Shadow King (Final Form):** stayed a single Shadow King and defeated Beast.
- **Master Mold:** used his beam, stomp and missiles with no spider mines, and defeated Beast.

Not yet tested:
- the replacement Xtremes, which were not seen firing
- the new Boost moves when you are the one playing the boss
- full matches to the end
- Skirmish with them
- Juggernaut (Boss) and the Mark II as AI opponents
