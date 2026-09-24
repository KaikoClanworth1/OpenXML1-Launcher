"""Writes the Playable Magneto template mod.

  python tools/make-playable-magneto.py mods/playable-magneto <mission start scripts...>

The mission start scripts are the BehavEd-format scripts/missions/*.py that
get the unlock line (the same list as mods/playable-professor-x/append).
"""
from pathlib import Path
import sys

M = Path(sys.argv[1])
MISSION_SCRIPTS = sys.argv[2:]


def w(rel, text):
    path = M / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


w('mod.ini', """[Mod]
Name = Playable Magneto
Type = character
Author = KaikoClanworth1
Version = 0.2
Description = Magneto, with his cape, as a playable hero from level 1. With your own X-Men Legends II installed, he also gets its roster portrait, power icons and team-screen animations. A template for character mods.

[Import]
Game = X-Men Legends II
Detect = XMen2.exe
ui/models/characters/2501.igb = ui/models/characters/2501.igb
textures/ui/magneto_icons1.igb = textures/ui/magneto_icons1.igb
actors/25_magneto.igb = actors/25_magneto_menu.igb
""")

# ---- powers: a hero powerstyle from the NPC Magneto's two powers ---------------
DAMAGE_BOLT = ['L2', 'L3', 'L4', 'L5', 'M1']
DAMAGE_CRUSH = ['L3', 'L4', 'L5', 'M1', 'M2']
USAGE = ['P1', 'P1+', 'P2', 'P2+', 'P3']
ICON_BOLT, ICON_CRUSH = 6, 1   # XML2's sheet: Magnetic Blast, Magnetic Shell


def ranks(prefix, talent, damages, triggers):
    out = []
    for n in range(2, len(damages) + 1):
        out.append(f'<FightMove Name="{prefix}{n}" fallback="{prefix}{n-1}" inherit="{prefix}{n-1}">\n'
                   f'<require cat="skill" item="{talent}" level="{n}"/>\n'
                   f'{triggers(damages[n-1], USAGE[n-1])}\n</FightMove>')
    return '\n'.join(out)


def one_beam(dmg, use):
    return f'<trigger tag="1" Damage="{dmg}" PowerUsage="{use}"/>'


def two_beams(dmg, use):
    return one_beam(dmg, use) + f'\n<trigger tag="2" Damage="{dmg}" PowerUsage="{use}"/>'


def powerstyle(icons):
    head = ('<PowerStyle IconFile="textures/ui/magneto_icons1.png" IconColumns="4" IconRows="4" CanSteal="true" exclusive="magneto">'
            if icons else '<PowerStyle CanSteal="true" exclusive="magneto">')
    return head + f'''
<FightMove Name="magbolt1" lockangles="true" animenum="ea_power1" priority="5" aitype="projectile" aireusetime="5" icon="{ICON_BOLT}" comboTextStarter="Magnetic" comboTextFinisher="Bolt">
<require cat="skill" item="magneto_bolt" level="1"/>
<trigger time="0" name="effect" effect="powers/magneto_pow1_charge" bolt="Bip01 L Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow1_arc"/>
<trigger time="0.4" name="sound" sound="character/magnet_m/power1"/>
<trigger tag="1" time="0.5" name="metal_balls" inherit="beam" spawneffect="powers/magneto_pow1_beamatk" beameffect="powers/magneto_pow1_beam" HitEffect="powers/magneto_pow1_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{DAMAGE_BOLT[0]}" beamBolt="Bip01 L Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="3" radius="10"/>
<chain action="idle" result="idle"/>
</FightMove>
{ranks("magbolt", "magneto_bolt", DAMAGE_BOLT, one_beam)}
<FightMove Name="magcrush1" lockangles="true" animenum="ea_power8" priority="5" aitype="projectile" aireusetime="10" icon="{ICON_CRUSH}" comboTextStarter="Magnetic" comboTextFinisher="Crush">
<require cat="skill" item="magneto_crush" level="1"/>
<trigger time="0" name="sound" sound="character/magnet_m/power2"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_charge" bolt="Bip01 L Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_charge" bolt="Bip01 R Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_grow"/>
<trigger tag="1" time="0.55" name="metal_balls" inherit="beam" spawneffect="powers/magneto_pow2_sentatk" beameffect="powers/magneto_pow2_beam" HitEffect="powers/magneto_pow2_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{DAMAGE_CRUSH[0]}" beamBolt="Bip01 L Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="5" radius="10"/>
<trigger tag="2" time="0.55" name="metal_balls" inherit="beam" beameffect="powers/magneto_pow2_beam" hiteffect="powers/magneto_pow2_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{DAMAGE_CRUSH[0]}" beamBolt="Bip01 R Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="5" radius="10"/>
<chain action="idle" result="idle"/>
</FightMove>
{ranks("magcrush", "magneto_crush", DAMAGE_CRUSH, two_beams)}
<FightMove Name="power_attack" fallback="magbolt5" inherit="magbolt5">
<require cat="skill" item="magneto_bolt" level="6"/>
</FightMove>
<FightMove Name="power_smash" fallback="magcrush5" inherit="magcrush5">
<require cat="skill" item="magneto_crush" level="6"/>
</FightMove>
</PowerStyle>
'''


# ---- the hero -------------------------------------------------------------------
def talent_levels(damages):
    out = []
    for i, d in enumerate(damages):
        text = f'^{d} Magnetic Damage. ^{USAGE[i]} Energy.'
        need = [None, 3, 5, 7, 9][i]
        out.append(f'<level description="{text}"/>' if need is None else
                   f'<level description="{text}">\n<require cat="level" level="{need}"/>\n</level>')
    return '\n'.join(out)


def hero(imported):
    anims = '25_magneto_menu' if imported else '25_magneto'
    icon = ' icon_texture="textures/ui/magneto_icons1.png"' if imported else ''
    return f'''<characters>
<stats name="Magneto" charactername="Magneto" skin="2501" sounddir="magnet_m" powerstyle="ps_magnetohero" characteranims="{anims}" level="1" strength="3" speed="4" body="4" mind="7" team="hero" RatingMelee="0.3" RatingRanged="0.5" RatingSupport="0.1" RatingDurability="0.4" scriptlevel="3" ailevel="2" canSeeStealthed="true" playable="true">
<BoltOn slot="ebolton_cape" bolt="Bip01 Spine2" model="9901" anim="99_cape"/>
<Race name="Mutant"/>
<Talent name="magneto_bolt" level="1" descname="Magnetic Bolt" description="Hurls a beam of magnetic force at an enemy."{icon} icon="{ICON_BOLT}" power="0">
{talent_levels(DAMAGE_BOLT)}
</Talent>
<Talent name="magneto_crush" level="0" descname="Magnetic Crush" description="Two beams of crushing magnetic force."{icon} icon="{ICON_CRUSH}" power="1">
{talent_levels(DAMAGE_CRUSH)}
</Talent>
<Talent name="fightstyle_psionic" level="1"/>
<Talent name="critical" level="0"/>
<Talent name="toughness" level="0"/>
<Talent name="leadership" level="0"/>
<Talent name="mutantmastery" level="0"/>
</stats>
</characters>
'''


# ---- packages, as text; the launcher compiles them -------------------------------
EFFECTS = ['magneto_pow1_charge', 'magneto_pow1_hit', 'magneto_pow1_arc', 'magneto_pow1_beamatk', 'magneto_pow1_beam',
           'magneto_pow2_charge', 'magneto_pow2_grow', 'magneto_pow2_sentatk', 'magneto_pow2_beam', 'magneto_pow2_hit']
SKIN = ('<actorskin filename="2501"/>\n<actoranimdb filename="25_magneto"/>\n'
        '<model filename="hud/hud_head_2501"/>\n<model filename="ui/hud/characters/2501"/>\n')
CAPE = '<actoranimdb filename="99_cape"/>\n<actoranimdb filename="9901"/>\n'

# The base: only what the game itself has.
w('merge/data/herostat.eng', hero(False))
w('files/data/powerstyles/ps_magnetohero.xml', powerstyle(False))
w('files/packages/generated/characters/magneto_2501.pkgb', '<packagedef>\n' + SKIN +
  ''.join(f'<effect filename="powers/{e}"/>\n' for e in EFFECTS) +
  '<fightstyle filename="data/powerstyles/ps_magnetohero"/>\n' + CAPE + '</packagedef>\n')
w('files/packages/generated/characters/magneto_2501_nc.pkgb', '<packagedef>\n' + SKIN + CAPE + '</packagedef>\n')
w('files/packages/generated/characters/magneto_xml.pkgb', '<packagedef>\n<fightstyle filename="data/powerstyles/ps_magnetohero"/>\n</packagedef>\n')
for name in MISSION_SCRIPTS:
    w(f'append/scripts/missions/{name}', 'setInCampaign("magneto", "TRUE" )\n')

# with-imports: used only when every [Import] file was found. XML2's animation
# set becomes his main one (it adds the team-screen poses), with its power
# icons and roster portrait. Naming a missing animation set ends the game.
w('with-imports/merge/data/herostat.eng', hero(True))
w('with-imports/files/data/powerstyles/ps_magnetohero.xml', powerstyle(True))
w('with-imports/merge/packages/generated/characters/magneto_2501.pkgb',
  '<packagedef>\n<actoranimdb filename="25_magneto_menu"/>\n<texture filename="textures/ui/magneto_icons1"/>\n</packagedef>\n')
w('with-imports/merge/packages/generated/characters/magneto_2501_nc.pkgb',
  '<packagedef>\n<actoranimdb filename="25_magneto_menu"/>\n</packagedef>\n')
w('with-imports/merge/packages/generated/maps/package/menus/characters_heads.pkgb',
  '<packagedef>\n<model filename="ui/models/characters/2501"/>\n</packagedef>\n')
print('written', sum(1 for f in M.rglob('*') if f.is_file()), 'files')
