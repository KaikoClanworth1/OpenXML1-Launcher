# Playable Professor X

Professor X in his astral form, the hero the game uses on the Astral Plane, is playable throughout the
game. Pick him at any team screen: the Blackbird, or an Xtraction Point.

Tested with OpenXML1 0.9b from a new game, together with Early X-Men Xtraction Point: he appears in the
Change Team roster, joins the team at level 1 with 60 HP and 90 EP, plays in New York with his own HUD portrait, and fights.

## What it changes

| File | How | Change |
|---|---|---|
| `data/herostat.eng` (and the other languages) | merge, attributes only | `ProfXAstral`: playable; level 1 and gains experience (`xpexempt` off); strength 2, speed 4, body 3, mind 8, a level-1 psychic profile like Jean Grey's. His powers and talents are unchanged |
| `data/missions/` astral1b, astral2, astral2_col, astral3, sent_fb, wx_fb_start | merge | the `RESTRICTEDHERO PROFXASTRAL` entries removed |
| `scripts/missions/*.py` (the 38 in BehavEd form) | append | `setInCampaign("profxastral", "TRUE")` |

**Unlocking.** A hero added to the campaign stays in the save. The unlock runs when any of those 38 missions
starts, so a new game has him from the first mission, and an existing save gets him at its next mission
start. The 47 mission scripts written as Python (`import game`) are left alone.

**Other modder-mode note.** OpenXML1's modder mode does not unlock him: it skips `ProfXAstral` by name.

## Known limits

- **Balance.** The game's astral Professor X is level 40 with base stats about ten times a normal hero's,
  and never gains experience. The mod starts him at level 1 (60 HP, 90 EP, tested) and lets him level up
  like the others. His powers are the game's own and were not rebalanced.
- **Astral form only.** The physical Professor X in the game is a wheelchair NPC with no powers or
  animations for fighting, so this mod uses the astral form.
- **Story not played through.** Scenes written for a team without him (the astral missions' own Professor X
  included) were not checked.
