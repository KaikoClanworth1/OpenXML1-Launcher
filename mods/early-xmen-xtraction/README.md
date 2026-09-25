# Early X-Men Xtraction Point

Tested with OpenXML1 0.9b and the World Xbox disc, from a new game.

- The first mission starts with a full team of four: **Wolverine, Cyclops, Storm and Jean Grey**. The
  unmodded game starts with Wolverine alone.
- An **Xtraction Point** stands a few steps in front of Wolverine's start. Its menu offers Change Team,
  Save Game, Load Game and Visit Danger Room.
- **Every hero can be swapped**, Wolverine and Cyclops included. At Change Team, pick a pad and press
  A. A roster bar opens with the unlocked heroes not on the team; press A again to replace. Beast and
  Iceman are unlocked at the start, and modder mode offers everyone.
- **The mansion takes a team of four too.** Every visit used to be Magma alone. Magma stays on the team
  and cannot be replaced; the team starts as Magma, Wolverine, Cyclops and Storm. The other three can be
  swapped freely at the subbasement's Xtraction Point, which now offers Change Team.

## How it works

The team comes from the map's player starts, the same way the game places a team anywhere else. Every map
of the first mission gets start positions for four heroes; the game only gave them room for one or two.
Nothing is spawned by script.

| File | How | Change |
|---|---|---|
| `data/missions/alison.eng` | merge | `maxheros` 2 to 4, `minheros` 2 to 1; Wolverine and Cyclops changed from required to recommended heroes; Storm, Jean Grey, Beast and Iceman added as recommended |
| `maps/nyc/alison/nyc1_1_1.eng` ... `nyc1_1_5.eng` (all six areas) | merge | each player start keeps the game's own position(s) and gains new ones beside them, facing the same way, up to four |
| `maps/nyc/alison/nyc1_1_1.eng` | merge | the game's standard `xtraction_point` entity, 110 units in front of Wolverine's start |
| `scripts/nyc/alison/nyc1_1_1.py` | append | `setInCampaign` for the six heroes, so they are unlocked |
| `scripts/nyc/alison/subway*.py` (all eight subway entrances) | files | move every living hero through the subway, not only party slot 1 |
| `maps/nyc/alison/nyc1_1_1.eng`, `nyc1_1_2.eng` | merge | three spots beside each subway exit for heroes 2 to 4 |
| `data/missions/mansion*.eng` (the eleven mansion visits) | merge | `maxheros` 1 to 4; Magma stays required; the six early X-Men recommended |
| `maps/mansion/man*/*` (34 maps) | merge | every player start gets four positions, on walkable floor |
| the eight subbasements | merge | the Xtraction Point's team-change flag turned on, with the standard beacon |

## Why each piece is needed

Each was found by testing on the real game:

- **Team size.** `maxheros="2"` in the mission file held the team at two, even with modder mode.
- **Recommended, not required.** The mission loads only the heroes it names. A *required* hero is loaded
  but locked on the team. A *recommended* hero is loaded and can be swapped. That is also what frees
  Wolverine and Cyclops.
- **Start positions.** When a map loads, including the reload after every team change, the team is placed
  at the map's player start, one position per hero. With too few positions the heroes without one were
  left out of the map: changing team while playing as anyone but Wolverine put the player in the void.
- **The Xtraction Point's own map reload** uses the start with an empty `prevzone`. The first area had
  none (its only start is for arriving from the second area), so the game fell back on that one.

- **Subways.** Each subway entrance fades to black and moved only `_HERO1_`, the whole party when the
  mission had one hero. Now it moves every living hero, the way the game's own party moves do
  (`alive()` then `copyOriginAndAngles` per hero): hero 1 to the original exit, heroes 2 to 4 to spots
  beside it and one step ahead. A merge replaces a placed-object group by its type, so the maps' whole
  `null` group is carried with the new spots added. `tools/make-early-xmen-subways.py` writes the scripts
  and spots from the game's own files.

- **Mansion.** Each visit's mission file held the team at one (`maxheros="1"`, Magma required). The
  maps gave most player starts one position, so every start gets four. New positions are checked
  against the maps' navigation grids (40-unit cells at `floor(position / 40)`, which put 135 of the
  game's 138 mansion starts on walkable cells). The later visits reuse the first visit's building with
  empty grids, and every one of their starts matches the first visit's grids, so those are used.
  Fifteen positions cannot be checked (two back-yard maps and one elevator start) and sit just beside
  their start. The subbasement Xtraction Point calls `extractionPointLite` with its first flag off (the
  `noteamchange` beacon); with it on, its menu gains Change Team, as at the final Master Mold fight.
  The Danger Room lessons, the E3 demo mansion and the GRSO attack are left as they were.
  `tools/make-early-xmen-mansion.py` writes all of this from the game's own files.

## Tested in the game

- **New game:** Wolverine, Cyclops, Storm and Jean Grey start together, with four HUD portraits.
- **Swapping:** Wolverine swapped for Beast and Cyclops for Iceman, leaving neither on the team. Then a
  second team change while playing as Storm. No void, all four present, all four portraits.
- **Subways:** all eight entrances on both subway maps, fired in turn. All four heroes arrived together
  at each exit, including while controlling a hero other than party slot 1 and during a fight.
- **Mansion, first visit:** Magma, Wolverine, Cyclops and Storm in the office after Professor X's
  conversation. At the subbasement Xtraction Point, Change Team replaced Wolverine with Beast; after the
  reload all four stood together. Magma's pad offers no Replace.
- **Mansion, fifth and seventh visits:** a team of four with four portraits in the entrance hall, and in
  the War Room after the status-meeting cutscene.
- **All six areas:** each was loaded directly. All four heroes were placed on solid ground at every start.

## Known limits

- **A second Cyclops.** In the second, third and fourth areas the story's own Cyclops stands nearby as a
  separate character until he joins in the third area. With Cyclops already on the team there are two for
  a while.
- **Mansion X-Men.** The mansion's own X-Men stand around as story characters, so a hero on your team
  can appear twice.
- **Mansion visits not all played.** Visits 1, 5 and 7 were tested; the others use the same
  changes on the same building.
- **Story not played through.** Areas were loaded directly, and the story's Cyclops scene was not reached
  in play.
