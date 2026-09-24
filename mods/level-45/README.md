# Level 45

Every hero at level 45, the game's highest level, from the first mission on. They get the skill points
of every level up to 45 to spend as you like.

## How it works

Each mission's start script (`append/scripts/missions/*.py`) gets one line:

    awardXPToPlayable(589255846 )

`awardXPToPlayable` is the game's own command that gives XP to every playable hero, benched ones
included. 589,255,846 is the XP the game stops at. Measured in the game:

| XP given | Level reached |
|---|---|
| 1,125,000 | 25 (next level at 1,543,300) |
| 50,000,000 | 37 (next level at 167,642,160) |
| 2,000,000,000 | 45, with experience held at 589,255,846 and no next level |

Once a hero is at 45, the line does nothing more. It runs at every mission start rather than once,
so heroes who join later in the story also reach 45 at their next mission.

## Tested in the game

Run hidden and muted with OpenXML1 0.9b, from a new game with Early X-Men Xtraction Point and Playable
Magneto. At the first Xtraction Point, the team and benched heroes were all at level 45, with their
skill points waiting.

## Notes

- Unticking the mod stops the XP. Heroes keep any levels they already have in a save.
- The game's own XP awards still happen, but a hero at 45 cannot go higher.
