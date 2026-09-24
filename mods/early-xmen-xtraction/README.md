# Early X-Men Xtraction Point

Tested with OpenXML1 0.9b and the World Xbox disc, from a new game, with modder mode off.

- The first mission starts with a full team of four: **Wolverine, Cyclops, Storm and Jean Grey**. The
  unmodded game starts with Wolverine alone.
- An **Xtraction Point** stands a few steps in front of Wolverine's start. Its menu offers Change Team,
  Save Game, Load Game and Visit Danger Room.
- **Storm, Jean Grey, Beast and Iceman can be swapped freely.** At Change Team, pick one of their pads
  and press A. A roster bar opens with the heroes not on the team; press A again to replace.
- **Wolverine and Cyclops stay.** The game's own mission file requires them, and the story's scenes
  are written for them.

## What it changes

| File | How | Change |
|---|---|---|
| `data/missions/alison.eng` | merge | `maxheros` 2 to 4; Storm, Jean Grey, Beast and Iceman added as recommended heroes |
| `maps/nyc/alison/nyc1_1_1.eng` | merge | the game's standard `xtraction_point` entity, placed at (1384.2, 1458.9, 9.6), 110 units in front of `player_start01` |
| `scripts/nyc/alison/nyc1_1_1.py` | append | `setInCampaign` for all six heroes; `addHero` for Cyclops, Storm and Jean Grey |

Nothing from the game is copied into the mod. It holds only its own lines, plus the one Xtraction Point
entity definition the game itself uses on other maps. The beacon model loads without any package change.

## Why each piece is needed

Each was found by testing on the real game:

- **`maxheros="2"`** in the mission file is what held the team at two. Even with modder mode's full
  unlock, the team screen offered no more than that.
- **Recommended, not required.** The mission only loads heroes it names. A *required* hero is loaded
  but locked on the team. A *recommended* hero is loaded and can be swapped. Heroes the mission does not
  name cannot join at all: an `addHero` for one gives a second Wolverine.
- **`addHero`.** In this mission the story brings heroes into play this way, as its own Cyclops scene
  does. The team screen alone only sets the roster.

## Known limits

- **No HUD portrait for swapped-in heroes.** A hero swapped in at the Xtraction Point plays and fights
  normally, and can be controlled with the D-pad, but their HUD portrait is blank. The same happens when
  the game fills a team slot by itself. Heroes the story adds with `addHero`, including the starting four,
  have portraits. Preloading the portraits (in the map's precache list and in its package) did not
  help, so the fix lies in how the engine's HUD assigns portraits, beyond what a mod can change.
- **Story not played through.** It's untested past the start of the mission, including the later scene
  where the story adds Cyclops itself.
