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
| `maps/nyc/alison/nyc1_1_1.eng` | merge | the game's standard `xtraction_point` entity, placed at (1384.2, 1458.9, 9.6), 110 units in front of `player_start01`; a stock-style `player_start` (empty `prevzone`, four positions) at Wolverine's start |
| `scripts/nyc/alison/nyc1_1_1.py` | append | `setInCampaign` for all six heroes; `addHero` for Cyclops, Storm and Jean Grey, once per mission (a mission variable) |

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
- **Start positions.** A team change reloads the map and places the team at a player start with an
  empty `prevzone`, one position per hero. That is what the game's own maps have beside their Xtraction
  Points (HAARP's is about 95 units away). The first map only had `player_start01`, a single position
  for returning from the next area. Without the new start, changing team while playing as anyone but
  Wolverine left the team in the void, and a swapped-in hero got no HUD portrait.
- **Once only.** The map script runs again on every reload, including after each team change. A
  mission variable limits the `addHero` lines to the first arrival, so a reload never adds a hero the
  player has swapped out.

## Tested in the game

- **Swapping:** Jean swapped for Beast and back; Gambit swapped in, in his second outfit, with modder mode on.
- **Team changes as each hero:** changing team while playing as Storm, and twice in a row as Beast. No void, and
  every hero keeps a HUD portrait, outfits included (their packages point to the base portrait).
- **The story's Cyclops scene:** the story's own `add_cyclops.py` (from the scene after Mystique, in
  `nyc1_1_3`) was run with Cyclops already on the team, through a test-only trigger. The game carried on
  normally with one Cyclops and the same four heroes.

## Known limits

- **Wolverine and Cyclops stay** on the team, as the game's mission file requires.
- **Story not played through.** It's untested past the first area; the Cyclops scene was tested by
  running its script, not by reaching it.
