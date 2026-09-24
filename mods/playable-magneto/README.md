# Playable Magneto (template character mod)

Magneto as a playable hero from level 1, with the **final boss's powers**. He is built only from what the
game already has: the boss's animations and power effects, plus his model, cape, voice and HUD portraits.
It is also the template for character mods. Each file below is the minimum a new playable hero needs, and
it is short enough to copy.

## The files, and what each one does

| File | How | What it is |
|---|---|---|
| `mod.ini` `[Copy]` | copy inside the game | `actors/25_magnetoboss.igb` is copied to `actors/25_magnetohero.igb`, his own animation set. |
| `merge/data/herostat.eng` | merge (new entry) | The hero: name, model (`skin="2501"`), animations (`characteranims="25_magnetohero"`), voice (`sounddir`), powerstyle, level-1 stats, the cape (`BoltOn`), and **talents**. Each power is a `Talent` with `power="0"` to `"3"` (its slot) and one `<level>` per rank. `fightstyle_psionic` gives the basic moves (combos, jump, grab). |
| `files/data/powerstyles/ps_magnetohero.xml` | new file | The **powers**. Each rank is a `FightMove` (`magbolt1` to `magbolt5`) requiring `<require cat="skill" item="magneto_bolt" level="N"/>`, with an animation (`ea_power1` plays `power_1`), effects and damage. **The buttons call fixed move names**: `power_attack`, `power_smash`, `power_boost` and `power_xtreme`. Each one inherits the top rank and falls back down to the rank learned. Each also requires a rank one above the top, so it never runs by itself. The Xtreme ranks are `xtreme1` to `xtreme5`, as Storm's are. |
| `files/packages/generated/characters/magneto_2501.pkgb`, `_nc.pkgb`, `magneto_xml.pkgb` | new files, written as text | What loads with him: the skin, animations, HUD portrait, the boss's power effects, metal-prison skin (`2504`), spheres, debris entities, powerstyle and cape. The `_nc` package is the same without powers, for menus. The launcher compiles them to PKGB on install. |
| `files/ui/models/characters/2501.igb` and `merge/.../menus/characters_heads.pkgb` | new file, and merge | His roster portrait, made for this mod, and its entry in the roster screen's package. |
| `append/scripts/missions/*.py` | append | `setInCampaign("magneto", "TRUE")` unlocks him when a mission starts. |
| `mod.ini` `[Import]` | import | The power-icon sheet from your own **X-Men Legends II**. The launcher asks where XML2 is installed the first time. Without it, the icons are skipped. |
| `with-imports/...` | only when every import was found | Puts the imported icons on his powers. |

**Keep an imported animation set out of the base.** Naming an animation set that is missing ends the game
when the level loads.

The generator that wrote this mod is in the repository as `tools/make-playable-magneto.py`.

## Adding menu animations

The boss's animation set has no `menu_idle` / `menu_action` animations, so his pad on the Change Team
screen is empty. To add them, edit the set (for example with an IGB Blender plugin) and put the result at:

    mods/playable-magneto/files/actors/25_magnetohero.igb

A file under `files\` replaces the `[Copy]` of the same name. The game's own `25_magnetoboss.igb` is
never changed.

## Powers

| Button | Power | Unlocks | From the boss |
|---|---|---|---|
| A | **Magnetic Bolt** | level 1 | His beam. A target it hits is encased in metal for 1.5 to 3.5 seconds. Damage L2 to M1. |
| B | **Magnetic Crush** | level 2 | His two-handed crush. Damage L3 to M2. |
| Boost | **Sphere Shield** | level 5 (costs 2 points) | His metal spheres. A timed damage shield (`BST` time, `A` armour, as Storm's shield) that flashes his shield-hit effect when struck. |
| Xtreme | **Magnetic Shockwave** | level 15 (costs 2 points) | His four expanding rings of force, with flying debris. Damage M1 to H1, knockback. |

The boss's immunities are left out.

## Tested in the game

These were run hidden and muted with OpenXML1 0.9b, from a new game with Early X-Men Xtraction Point. For
the test only, Crush and Shield started at rank 1.

- **Without XML2:**
  - His roster slot shows his portrait.
  - He joins at level 1 (70 HP, 83 EP) with his cape.
  - Magnetic Bolt, Magnetic Crush and Sphere Shield all play their effects and cost energy.
  - The spheres circle him while he casts the shield.
  - The level plays normally with no crash.
- **With XML2:**
  - The same.

## Known limits

- **Not yet seen in play:**
  - the metal prison on an enemy the bolt hits
  - the Xtreme, which needs level 15 and a full Xtreme meter
  - the new icons in his power menu
- **The spheres show only while he casts.** The shield itself lasts its full time. The game's bolt-ons have
  no timer, so keeping them for the whole shield would need a removal hook the engine does not offer.
- **No flight.** His XML1 model has no flying animations.
