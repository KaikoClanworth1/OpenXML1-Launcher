# Early X-Men Xtraction Point

Tested with OpenXML1 0.9b and the World Xbox disc, from a new game.

- The first mission starts with a full team of four: **Wolverine, Cyclops, Storm and Jean Grey**. The
  unmodded game starts with Wolverine alone.
- An **Xtraction Point** stands a few steps in front of Wolverine's start. Its menu offers Change Team,
  Save Game, Load Game and Visit Danger Room.
- **Every hero can be swapped**, Wolverine and Cyclops included. At Change Team, pick a pad and press
  A. A roster bar opens with the unlocked heroes not on the team; press A again to replace. Beast and
  Iceman are unlocked at the start, and modder mode offers everyone.

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

## Tested in the game

- **New game:** Wolverine, Cyclops, Storm and Jean Grey start together, with four HUD portraits.
- **Swapping:** Wolverine swapped for Beast and Cyclops for Iceman, leaving neither on the team. Then a
  second team change while playing as Storm. No void, all four present, all four portraits.
- **All six areas:** each was loaded directly. All four heroes were placed on solid ground at every start.

## Known limits

- **A second Cyclops.** In the second, third and fourth areas the story's own Cyclops stands nearby as a
  separate character until he joins in the third area. With Cyclops already on the team there are two for
  a while.
- **Story not played through.** Areas were loaded directly, and the story's Cyclops scene was not reached
  in play.
