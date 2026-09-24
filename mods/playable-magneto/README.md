# Playable Magneto (template character mod)

Magneto as a playable hero from level 1, with the **final boss's powers**. He is built from what the game
already has (the boss's animations and power effects, his model, cape, voice and HUD portraits), plus a
roster portrait, power icons and an animated-series (TAS) costume made for the mod. It needs no other
game.

It is also the template for character mods. Each file below is the minimum a new playable hero needs, and
it is short enough to copy.

## The files, and what each one does

| File | How | What it is |
|---|---|---|
| `mod.ini` `[Copy]` | copy inside the game | `actors/25_magnetoboss.igb` is copied to `actors/25_magnetohero.igb`, his own animation set. `hud/hud_head_2501.igb` is copied to `hud_head_2502.igb` for the TAS costume. |
| `merge/data/herostat.eng` | merge (new entry) | The hero: name, model (`skin="2501"`), animations (`characteranims="25_magnetohero"`), voice (`sounddir`), powerstyle, level-1 stats, the cape (`BoltOn`), his costumes (`skin_90s="02"`: the 90s category, skin 2502), and **talents**. Each power is a `Talent` with `power="0"` to `"3"` (its slot) and one `<level>` per rank. `fightstyle_psionic` gives the basic moves (combos, jump, grab). |
| `files/data/powerstyles/ps_magnetohero.xml` | new file | The **powers**. Each rank is a `FightMove` (`magbolt1` to `magbolt5`) requiring `<require cat="skill" item="magneto_bolt" level="N"/>`, with an animation (`ea_power1` plays `power_1`), effects and damage. **The buttons call fixed move names**: `power_attack`, `power_smash`, `power_boost` and `power_xtreme`. Each one inherits the top rank and falls back down to the rank learned. Each also requires a rank one above the top, so it never runs by itself. The Xtreme ranks are `xtreme1` to `xtreme5`, as Storm's are. |
| `files/packages/generated/characters/magneto_2501.pkgb`, `_nc.pkgb`, the same pair for `2502`, and `magneto_xml.pkgb` | new files, written as text | One pair per costume, as the game's own alternate skins have. What loads with him: the skin, animations, HUD portrait, the boss's power effects, metal-prison skin (`2504`), spheres, debris entities, powerstyle and cape. The `_nc` package is the same without powers, for menus. The launcher compiles them to PKGB on install. |
| `files/ui/models/characters/2501.igb` and `merge/.../menus/characters_heads.pkgb` | new file, and merge | His roster portrait, made for this mod, and its entry in the roster screen's package. |
| `files/textures/ui/magneto_all.igb` | new file | His power icons, made for this mod: a 2 x 2 sheet like every XML1 hero's (`IconColumns`/`IconRows` in the powerstyle). Each talent and first-rank move names its cell with `icon`: 0 spheres (Shield), 1 burst (Xtreme), 2 girder (Crush), 3 beam (Bolt). Both character packages load it. |
| `files/actors/2502.igb`, `files/ui/hud/characters/2502.igb` | new files | His TAS costume and its HUD head, made for this mod. |
| `append/scripts/missions/*.py` | append | `setInCampaign("magneto", "TRUE")` unlocks him when a mission starts. |

**Never name a missing animation set.** Naming an animation set that is not in the game ends the game
when the level loads.

The generator that wrote this mod is in the repository as `tools/make-playable-magneto.py`.

## Menu animations

The boss's animation set has no `menu_idle` / `menu_action` animations, so his pad on the Change Team
screen is empty. No available IGB editor handles XML1 animation sets yet. When one does, put the edited
set at:

    mods/playable-magneto/files/actors/25_magnetohero.igb

A file under `files\` replaces the `[Copy]` of the same name. The game's own `25_magnetoboss.igb` is
never changed.

## Powers

| Button | Power | Unlocks | From the boss |
|---|---|---|---|
| A | **Magnetic Bolt** | level 1 | His beam. A target it hits is encased in metal for 1.5 to 3.5 seconds. Damage L2 to M1. |
| B | **Magnetic Crush** | level 2 | His two-handed crush. Damage L3 to M2. |
| Boost | **Sphere Shield** | level 5 (costs 2 points) | His metal spheres orbit him for as long as the shield lasts: a timed damage shield (`BST` time, `A` armour, as Storm's shield) that flashes his shield-hit effect when struck. The spheres ride on the shield itself (`bolton=` and `fx_bolt=` on the powerup, as Iceman's ice blades do), so they leave when it ends. |
| Xtreme | **Magnetic Shockwave** | level 15 (costs 2 points) | His four expanding rings of force, with flying debris. Damage M1 to H1, knockback. |

He also has **Magnetic Glide** from level 1, a passive talent: +75% movement speed. The boss's animation
set has only a slow boss walk and no run, so the glide is a permanent `move` powerup (`activepowerup`
with `affect_type="scale"` and `life="-1"`), the same way the game's speed items work. The value is
`MOVE_SCALE` in the generator.

The boss's immunities are left out.

## Costumes

| Skin | Costume | Category |
|---|---|---|
| 2501 | His XML1 look | base |
| 2502 | The animated series (TAS) | `90s` |

Press **Skin** (X) on his pad in the Change Team screen. Costumes follow the game's own rules: all of
them become available when the game is finished, or with the launcher's **modder mode**.

## Tested in the game

These were run hidden and muted with OpenXML1 0.9b, from a new game with Early X-Men Xtraction Point. For
the test only, Crush and Shield started at rank 1.

- His roster slot shows his portrait.
- His powers screen (Details, then the powers tab) shows all four of his icons.
- He joins at level 1 (70 HP, 83 EP) with his cape.
- Magnetic Bolt, Magnetic Crush and Sphere Shield all play their effects and cost energy.
- The spheres orbit him for the whole shield (seen at 1, 3, 8 and 15 seconds) and are gone once it ends
  (30 seconds).
- The level plays normally with no crash.
- With costumes unlocked, Skin on his pad switches him to the TAS costume. He plays in it, and the HUD
  shows its head.
- Magnetic Glide: the same 0.6-second push moved him about 1.5 times as far as without it (a lower bound:
  the push includes starting up).

## Known limits

- **Not yet seen in play:**
  - the metal prison on an enemy the bolt hits
  - the Xtreme, which needs level 15 and a full Xtreme meter
- **His walk animation plays at its normal pace** while the glide moves him faster, so his feet may slide.
- **The TAS costume's pause-menu head** (`hud/hud_head_2502`) is a copy of his regular one. Put your own at
  `files/hud/hud_head_2502.igb` to replace it.
- **No flight.** His XML1 model has no flying animations.
