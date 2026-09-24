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
| `merge/data/herostat.eng` | merge (new entry) | The hero: name, model (`skin="2501"`), animations (`characteranims`), voice (`sounddir`), powerstyle, level-1 stats, the cape (`BoltOn`, as the game's own Magneto has it), and **talents**. Each power is a `Talent` with `power="0"` to `"3"` (its button) and one `<level>` per rank. `fightstyle_psionic` gives the basic moves (combos, jump, grab), whose animations every level loads. |
| `files/data/powerstyles/ps_magnetohero.xml` | new file | The **powers**. Each rank is a `FightMove` (`magbolt1` to `magbolt5`) requiring `<require cat="skill" item="magneto_bolt" level="N"/>`, with an animation (`ea_power1` plays `power_1`), effects and damage. **The buttons call fixed move names**: `power_attack` (A) and `power_smash` (B); `power_boost` and `power_xtreme` also exist. Each one inherits the top rank and falls back down to the rank learned, and requires a rank one above the top, so it never runs by itself. |
| `files/packages/generated/characters/magneto_2501.pkgb`, `_nc.pkgb`, `magneto_xml.pkgb` | new files, written as text | What loads with him: skin, animations, HUD portrait, power effects, powerstyle and cape. The `_nc` package is the same without powers, for menus. The launcher compiles them to PKGB on install. |
| `append/scripts/missions/*.py` | append | `setInCampaign("magneto", "TRUE")` unlocks him when a mission starts. |
| `mod.ini` `[Import]` | import | Three files from your own **X-Men Legends II**: roster portrait, power-icon sheet, and animation set (with team-screen poses). The launcher asks where XML2 is installed the first time. Without it, these are skipped. |
| `with-imports/...` | only when every import was found | Uses the imported files: XML2's animation set as his main one, icons on his powers, and his portrait in the roster package. |

**Keep an imported animation set out of the base.** Naming an animation set that is missing ends the game
when the level loads. That is why the XML2 set is only used from `with-imports`.

The generator that wrote this mod is in the repository as `tools/make-playable-magneto.py`.

## Powers

- **Magnetic Bolt** (A): the ally Magneto's beam. Five ranks, from L2 damage for P1 energy up to M1 for P3.
- **Magnetic Crush** (B): his two-handed double beam. Starts locked (rank 0); spend a skill point on it.

## Tested in the game

- **Without XML2:** he joins at level 1 and wears his cape. Magnetic Bolt fires, hits and costs energy. The
  level loads and plays normally.
- **With XML2:**
  - He stands on his team-screen pad in XML2's pose, and wears his cape in play.
  - Magnetic Bolt still fires.
  - His power menu shows XML2's Magnetic Blast and Magnetic Shell icons.
  - His roster slot shows XML2's portrait.

## Known limits

- **Roster portrait framing.** XML2's portrait is framed larger than XML1's slot, so it overflows the square.
- **Without XML2**, his roster slot is a black square and his team-screen pad is empty. He is still
  selectable: pick the black square between Magma and Nightcrawler.
- **Two powers.** The game's own Magneto moves give two.
- **No flight.** His XML1 model has no flying animations.
- **Magnetic Crush** was not tested firing.
