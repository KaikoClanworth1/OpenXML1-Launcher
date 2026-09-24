# Danger Room: More Characters

Adds characters to the **main-menu** Danger Room's Sparring and Skirmish rosters. So far:

| Character | From | Changes for the Danger Room |
|---|---|---|
| **Master Mold** | the final-act boss (`mastermoldone`) | 0.55 scale, so he fits the arenas and the camera; health scale 3 instead of 30 |

He has the boss's own moves: arm sweep, stomp, arm-cannon beam (A), stun beam (B), missiles (Boost) and
his Xtreme. He is listed at the end of the roster, after Toad.

No training course unlocks him, so use this with **Danger Room: Unlock Everything**.

## How it works

The story boss is left exactly as it is. The Danger Room version is a separate character, `MasterMoldDR`,
added to `data/npcstat.eng`:
- a copy of the boss's entry with `playable="true"`, which is what puts a character in the Danger Room
  roster
- `scale_factor` and `size` shrunk to about a Sentinel's height, and a lower `npchealthscale`

The game finds a character's packages by name and skin (`<name>_<skin>.pkgb`). `mod.ini` `[Copy]` makes
the boss's two packages available under the new name:

    packages/generated/characters/mastermoldone_5901.pkgb    -> mastermolddr_5901.pkgb
    packages/generated/characters/mastermoldone_5901_nc.pkgb -> mastermolddr_5901_nc.pkgb

To add another character, copy its `npcstat.eng` entry under a new name with `playable="true"`, adjust
what it needs, and add the same two `[Copy]` lines for its packages.

## Tested in the game

Hidden and muted, with Danger Room: Unlock Everything, on OpenXML1's `dangerRoomUnlockAll` build:
- Master Mold appears in the Sparring roster.
- **As the player** (challenger) against Toad, his whole body stays on screen. He moves, his arm sweep
  hits, and A, B and Boost fire the arm-cannon beam, stun beam and missiles.
- **As an AI champion** against a Morlock Claw, he loads and fights.

Not yet tested:
- his Xtreme
- a full match to the end
- Skirmish with him
