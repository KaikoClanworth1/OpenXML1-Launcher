# Playable Magneto (template character mod)

Magneto as a playable hero from level 1, built only from what the game already has: his model, animations,
cape, voice, HUD portraits and power effects. It is also the template for character mods. Each file below
is the minimum a new playable hero needs, and it is short enough to copy.

Tested with OpenXML1 0.9b from a new game, with Early X-Men Xtraction Point: Magneto appears in the Change
Team roster, joins at level 1 (70 HP, 83 EP), plays in New York with his helmeted HUD portrait, and his
Magnetic Bolt fires. Its effects play, it knocks a troop into the air, and it costs energy.

## The files, and what each one does

| File | How | What it is |
|---|---|---|
| `merge/data/herostat.eng` | merge (new entry) | The hero: name, model (`skin="2501"`), animations (`characteranims`), voice (`sounddir`), powerstyle, level-1 stats, and **talents**. Each power is a `Talent` with `power="0"` to `"3"` (its button) and one `<level>` per rank. `fightstyle_psionic` gives the basic moves: combos, jump and grab. Their animations are loaded for every level. Merged into all languages. |
| `files/data/powerstyles/ps_magnetohero.xml` | new file | The **powers**. Each rank is a `FightMove` (`magbolt1` to `magbolt5`) with `<require cat="skill" item="magneto_bolt" level="N"/>`, an animation (`animenum="ea_power1"` plays his `power_1`), effects and damage. **The buttons call fixed move names**: `power_attack` (A), `power_smash` (B), `power_boost` and `power_xtreme`. Each one inherits the top rank and falls back down to the rank the hero has learned. It requires a rank one above the top, so it never runs itself. The launcher compiles the file on install. |
| `files/packages/generated/characters/magneto_2501.pkgb` | new file (text) | What loads with him in a level: skin, animations, HUD portrait, power effects, powerstyle, cape. Named `<hero name>_<skin>`. |
| `files/.../magneto_2501_nc.pkgb` | new file (text) | The same without powers, for menus. |
| `files/.../magneto_xml.pkgb` | new file (text) | His powerstyle, loaded with the character data. |
| `append/scripts/missions/*.py` | append | `setInCampaign("magneto", "TRUE")`: unlocks him when a mission starts. |

The packages are written as plain XML inside `<packagedef>`, and the launcher compiles them to the game's
binary PKGB form on install. The generator that wrote this mod is kept beside it as
`tools/make-playable-magneto.py`.

## Powers

- **Magnetic Bolt** (A): the ally Magneto's beam. Five ranks, from L2 damage for P1 energy up to M1 for P3.
- **Magnetic Crush** (B): his two-handed double beam. Starts locked (rank 0); spend a skill point on it.

## Known limits

- **No roster portrait.** He shows as a black square in the team screen's hero bar (between Magma and
  Nightcrawler), and his pad has no model preview. The game has no selection portrait for him
  (`ui/models/characters/2501`), and no menu animations (`menu_idle`). Upstream OpenXML1 makes these with
  its asset scripts, which is a separate job.
- **No power icons.** The game has no Magneto icon sheet, so his power slots have no pictures.
- **Two powers.** The game's own Magneto moves give two; `power_boost` and `power_xtreme` are not defined.
- **No flight.** His model has no flying animations.
- **Only Magnetic Bolt was tested firing**; Magnetic Crush needs a skill point first.
