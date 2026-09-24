# Early X-Men Xtraction Point

Tested with OpenXML1 0.9b and the World Xbox disc, from a new game.

- The first mission starts with a full team of four: **Wolverine, Cyclops, Storm and Jean Grey**. The
  unmodded game starts with Wolverine alone.
- An **Xtraction Point** stands a few steps in front of Wolverine's start. Its menu offers Change Team,
  Save Game, Load Game and Visit Danger Room.
- **Beast and Iceman** are also unlocked (added to the campaign) at the start.

## What it changes

| File | How | Change |
|---|---|---|
| `data/missions/alison.eng` | merge | `maxheros` 2 to 4; Storm and Jean Grey added as required heroes |
| `maps/nyc/alison/nyc1_1_1.eng` | merge | the game's standard `xtraction_point` entity, placed at (1384.2, 1458.9, 9.6), 110 units in front of `player_start01` |
| `scripts/nyc/alison/nyc1_1_1.py` | append | `setInCampaign` for all six heroes; `addHero` for Cyclops, Storm and Jean Grey |

Nothing from the game is copied into the mod. It holds only its own lines, plus the one Xtraction Point
entity definition the game itself uses on other maps. The beacon model loads without any package change.

## Why each piece is needed

Each was found by testing on the real game, with modder mode off:

- **`maxheros="2"`** in the mission file is what held the team at two. Even with modder mode's full
  unlock, the team screen offered no more than that.
- **Storm** is available at this point in the story by default. **Jean Grey** only stays in the team
  when she is a required hero.
- **The team screen only sets the roster.** In this mission the story spawns heroes into play with
  `addHero`, as its own Cyclops scene does. Without these lines, only Wolverine appears.

## Known limits

- **Team is fixed.** All four are required, so the team screen shows them but can't swap them. Beast
  and Iceman can't be brought in during this mission. The team screen only ever offered the heroes
  above, even with every hero unlocked.
- **Story not played through.** It's untested past the start of the mission, including the later scene
  where the story adds Cyclops itself.
