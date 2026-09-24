"""Writes the Playable Magneto template mod.

  python tools/make-playable-magneto.py mods/playable-magneto <mission start scripts...>

The mission start scripts are the BehavEd-format scripts/missions/*.py that
get the unlock line (the same list as mods/playable-professor-x/append).

His powers are the final boss's (ps_magneto_boss), made into ranked hero
powers. His animation set is the boss's, duplicated by the launcher to
actors/25_magnetohero.igb so it can be edited (for example, to add the
menu_idle / menu_action / menu_goodbye animations the team screens play).
"""
from pathlib import Path
import sys

M = Path(sys.argv[1])
MISSION_SCRIPTS = sys.argv[2:]
ANIMS = '25_magnetohero'


def w(rel, text):
    path = M / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


w('mod.ini', f"""[Mod]
Name = Playable Magneto
Type = character
Author = KaikoClanworth1
Version = 0.3
Description = Magneto as a playable hero from level 1, with the final boss's powers: Magnetic Bolt, Magnetic Crush, Sphere Shield and the Magnetic Shockwave Xtreme. With your own X-Men Legends II installed, he also gets its roster portrait and power icons.

[Copy]
actors/25_magnetoboss.igb = actors/{ANIMS}.igb

[Import]
Game = X-Men Legends II
Detect = XMen2.exe
ui/models/characters/2501.igb = ui/models/characters/2501.igb
textures/ui/magneto_icons1.igb = textures/ui/magneto_icons1.igb
""")

# ---- powers ---------------------------------------------------------------------
USAGE = ['P1', 'P1+', 'P2', 'P2+', 'P3']
BOLT = ['L2', 'L3', 'L4', 'L5', 'M1']            # damage per rank
HOLD = ['1.5', '2', '2.5', '3', '3.5']           # seconds encased in metal
CRUSH = ['L3', 'L4', 'L5', 'M1', 'M2']
SHIELD_USE = ['P6', 'P7', 'P8', 'P9', 'P10']
XTREME = ['M1', 'M2', 'M3', 'M4', 'H1']
# XML2's icon sheet: Magnetic Blast, Magnetic Shell, Polarized Shield, METALLIC MAYHEM
ICON = {'bolt': 6, 'crush': 1, 'shield': 0, 'xtreme': 8}


def rank_moves(prefix, talent, count, body):
    out = []
    for n in range(2, count + 1):
        out.append(f'<FightMove Name="{prefix}{n}" fallback="{prefix}{n-1}" inherit="{prefix}{n-1}">\n'
                   f'<require cat="skill" item="{talent}" level="{n}"/>\n{body(n - 1)}\n</FightMove>')
    return '\n'.join(out)


def powerstyle(icons):
    head = ('<PowerStyle IconFile="textures/ui/magneto_icons1.png" IconColumns="4" IconRows="4" CanSteal="true" exclusive="magneto">'
            if icons else '<PowerStyle CanSteal="true" exclusive="magneto">')
    return head + f'''
<FightMove Name="magbolt1" lockangles="true" animenum="ea_power1" priority="5" aitype="projectile" aireusetime="5" icon="{ICON['bolt']}" comboTextStarter="Magnetic" comboTextFinisher="Prison">
<require cat="skill" item="magneto_bolt" level="1"/>
<trigger time="-1" name="sound" sound="character/magnet_m/shieldb"/>
<trigger time="0.4" name="sound" sound="character/magnet_m/power1"/>
<trigger time="0" name="effect" effect="powers/magneto_pow1_charge" bolt="Bip01 L Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow1_arc"/>
<trigger time="-1" tag="10" name="powerup" life="{HOLD[0]}" powerup="move" level="0" affect_type="scale" skin="2504" func_activate="immobolizeactivate" func_deactivate="frozendeactivate"/>
<trigger tag="1" time="0.55" name="metal_balls" inherit="beam" spawneffect="powers/magneto_pow1_beamatk" beameffect="powers/magneto_pow1_beam" HitEffect="powers/magneto_pow1_hit" VictimEventTag="10" DamageType="dmg_magnetic" Damage="{BOLT[0]}" beamBolt="Bip01 L Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="3" radius="10"/>
<chain action="idle" result="idle"/>
</FightMove>
{rank_moves("magbolt", "magneto_bolt", 5, lambda i: f'<trigger tag="10" life="{HOLD[i]}"/>' + chr(10) + f'<trigger tag="1" Damage="{BOLT[i]}" PowerUsage="{USAGE[i]}"/>')}
<FightMove Name="magcrush1" lockangles="true" animenum="ea_power8" priority="5" aitype="projectile" aireusetime="10" icon="{ICON['crush']}" comboTextStarter="Magnetic" comboTextFinisher="Crush">
<require cat="skill" item="magneto_crush" level="1"/>
<trigger time="0" name="sound" sound="character/magnet_m/power2"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_charge" bolt="Bip01 L Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_charge" bolt="Bip01 R Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_grow"/>
<trigger tag="1" time="0.55" name="metal_balls" inherit="beam" spawneffect="powers/magneto_pow2_sentatk" beameffect="powers/magneto_pow2_beam" HitEffect="powers/magneto_pow2_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{CRUSH[0]}" beamBolt="Bip01 L Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="5" radius="10"/>
<trigger tag="2" time="0.55" name="metal_balls" inherit="beam" beameffect="powers/magneto_pow2_beam" hiteffect="powers/magneto_pow2_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{CRUSH[0]}" beamBolt="Bip01 R Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="5" radius="10"/>
<chain action="idle" result="idle"/>
</FightMove>
{rank_moves("magcrush", "magneto_crush", 5, lambda i: f'<trigger tag="1" Damage="{CRUSH[i]}" PowerUsage="{USAGE[i]}"/>' + chr(10) + f'<trigger tag="2" Damage="{CRUSH[i]}" PowerUsage="{USAGE[i]}"/>')}
<FightMove Name="magshield1" lockangles="true" animenum="ea_power3" priority="5" aitype="buffself" aireusetime="20" icon="{ICON['shield']}">
<require cat="skill" item="magneto_shield" level="1"/>
<trigger time="0" name="sound" sound="character/magnet_m/shieldb"/>
<trigger time="0.3" name="bolton" type="ce_bolton" model="models/bolton/magneto_spheres" bolt="Bip01 Neck" replacecurrent="true" boltslot="ebolton_weapon"/>
<trigger time="0.95" name="removebolton" type="ce_bolton" removebolton="true" boltslot="ebolton_weapon"/>
<trigger time="0.5" tag="1" name="powerup" powerusage="{SHIELD_USE[0]}" life="BST1" powerup="def_damage" level="A2" no_shadow="true" effect_cust1="powers/magneto_shield_hit" effect="powers/magneto_pow1_arc"/>
<chain action="idle" result="idle"/>
</FightMove>
{rank_moves("magshield", "magneto_shield", 5, lambda i: f'<trigger tag="1" life="BST{i + 1}" level="A{i + 2}" powerusage="{SHIELD_USE[i]}"/>')}
<FightMove Name="xtreme1" lockangles="true" animenum="ea_power4" priority="uninterruptable" icon="{ICON['xtreme']}" comboTextStarter="" comboTextFinisher="Shockwave">
<require cat="skill" item="magneto_xtreme" level="1"/>
<require cat="xtreme" level="1"/>
<event name="mag_radial" inherit="punch" AttackType="blast" PowerAttack="true" DamageType="dmg_magnetic" Arc="180" damage="{XTREME[0]}" knockback="K8">
<damageMod name="dmgmod_auto_knockback"/>
</event>
<trigger time="0" name="xtreme_start"/>
<trigger time="0" name="stop"/>
<trigger time="0" name="effect" effect="powers/magneto_pow4_chaos"/>
<trigger time="0" name="relative_spawn" filename="magnetoboss_ents" entity="debris" offset="0 0 60" attackerheight="false" absolute="false"/>
<trigger time="0" name="sound" sound="character/magnet_m/power4"/>
<trigger time="0.43" name="mag_radial" MaxRange="70"/>
<trigger time="0.46" name="mag_radial" MaxRange="140"/>
<trigger time="0.49" name="mag_radial" MaxRange="210"/>
<trigger time="0.52" name="mag_radial" MaxRange="280"/>
<chain action="idle" result="idle"/>
</FightMove>
{chr(10).join(f'<FightMove name="xtreme{n}" inherit="xtreme{n-1}" fallback="xtreme{n-1}">{chr(10)}<require cat="level" level="XLT{n}"/>{chr(10)}<event name="mag_radial" damage="{XTREME[n-1]}"/>{chr(10)}</FightMove>' for n in range(2, 6))}
<FightMove Name="power_attack" fallback="magbolt5" inherit="magbolt5">
<require cat="skill" item="magneto_bolt" level="6"/>
</FightMove>
<FightMove Name="power_smash" fallback="magcrush5" inherit="magcrush5">
<require cat="skill" item="magneto_crush" level="6"/>
</FightMove>
<FightMove Name="power_boost" fallback="magshield5" inherit="magshield5">
<require cat="skill" item="magneto_shield" level="6"/>
</FightMove>
<FightMove Name="power_xtreme" fallback="xtreme5" inherit="xtreme5">
<require cat="level" level="XLT6"/>
</FightMove>
</PowerStyle>
'''


# ---- the hero -------------------------------------------------------------------
def talent(name, descname, description, icon_attr, icon, power, levels, first_cost=None):
    rows = []
    for i, (text, need) in enumerate(levels):
        cost = f' cost="{first_cost}"' if (i == 0 and first_cost) else ''
        rows.append(f'<level description="{text}"{cost}/>' if need is None else
                    f'<level description="{text}"{cost}>\n<require cat="level" level="{need}"/>\n</level>')
    return (f'<Talent name="{name}" level="{1 if name == "magneto_bolt" else 0}" descname="{descname}" description="{description}"'
            f'{icon_attr} icon="{icon}" power="{power}">\n' + '\n'.join(rows) + '\n</Talent>')


def hero(imported):
    icon = ' icon_texture="textures/ui/magneto_icons1.png"' if imported else ''
    bolt = talent('magneto_bolt', 'Magnetic Bolt', 'A beam of magnetic force that encases its target in metal.', icon, ICON['bolt'], 0,
                  [(f'^{BOLT[i]} Magnetic Damage. Held {HOLD[i]} seconds. ^{USAGE[i]} Energy.', [None, 3, 5, 7, 9][i]) for i in range(5)])
    crush = talent('magneto_crush', 'Magnetic Crush', 'Two beams of crushing magnetic force.', icon, ICON['crush'], 1,
                   [(f'^{CRUSH[i]} Magnetic Damage. ^{USAGE[i]} Energy.', [2, 4, 6, 8, 10][i]) for i in range(5)])
    shield = talent('magneto_shield', 'Sphere Shield', 'A magnetic field that turns aside incoming attacks.', icon, ICON['shield'], 2,
                    [(f'^BST{i + 1} Seconds. -^A{i + 2} Damage. ^{SHIELD_USE[i]} Energy.', [5, 7, 9, 11, 13][i]) for i in range(5)], first_cost=2)
    xtreme = talent('magneto_xtreme', 'MAGNETIC SHOCKWAVE', 'XTreme wave of magnetic force that hurls enemies away.', icon, ICON['xtreme'], 3,
                    [('Unlocks this ability', 15)], first_cost=2)
    return f'''<characters>
<stats name="Magneto" charactername="Magneto" skin="2501" sounddir="magnet_m" powerstyle="ps_magnetohero" characteranims="{ANIMS}" level="1" strength="3" speed="4" body="4" mind="7" team="hero" RatingMelee="0.3" RatingRanged="0.5" RatingSupport="0.2" RatingDurability="0.4" scriptlevel="3" ailevel="2" canSeeStealthed="true" playable="true">
<BoltOn slot="ebolton_cape" bolt="Bip01 Spine2" model="9901" anim="99_cape"/>
<Race name="Mutant"/>
{bolt}
{crush}
{shield}
{xtreme}
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
           'magneto_pow2_charge', 'magneto_pow2_grow', 'magneto_pow2_sentatk', 'magneto_pow2_beam', 'magneto_pow2_hit',
           'magneto_pow4_chaos', 'magneto_shield_hit']
SKIN = (f'<actorskin filename="2501"/>\n<actoranimdb filename="{ANIMS}"/>\n'
        '<model filename="hud/hud_head_2501"/>\n<model filename="ui/hud/characters/2501"/>\n')
CAPE = '<actoranimdb filename="99_cape"/>\n<actoranimdb filename="9901"/>\n'
POWERS = (''.join(f'<effect filename="powers/{e}"/>\n' for e in EFFECTS) +
          '<actorskin filename="2504"/>\n<model filename="models/effects/magneto_blast"/>\n'
          '<xml filename="data/entities/magnetoboss_ents"/>\n'
          '<model filename="models/bolton/magneto_spheres"/>\n<model filename="models/bolton/magneto_hiteffect"/>\n')

w('merge/data/herostat.eng', hero(False))
w('files/data/powerstyles/ps_magnetohero.xml', powerstyle(False))
w('files/packages/generated/characters/magneto_2501.pkgb', '<packagedef>\n' + SKIN + POWERS +
  '<fightstyle filename="data/powerstyles/ps_magnetohero"/>\n' + CAPE + '</packagedef>\n')
w('files/packages/generated/characters/magneto_2501_nc.pkgb', '<packagedef>\n' + SKIN + CAPE + '</packagedef>\n')
w('files/packages/generated/characters/magneto_xml.pkgb',
  '<packagedef>\n<xml filename="data/entities/magnetoboss_ents"/>\n<fightstyle filename="data/powerstyles/ps_magnetohero"/>\n</packagedef>\n')
for name in MISSION_SCRIPTS:
    w(f'append/scripts/missions/{name}', 'setInCampaign("magneto", "TRUE" )\n')

# with-imports: used only when every [Import] file was found: XML2's power
# icons and roster portrait.
w('with-imports/merge/data/herostat.eng', hero(True))
w('with-imports/files/data/powerstyles/ps_magnetohero.xml', powerstyle(True))
w('with-imports/merge/packages/generated/characters/magneto_2501.pkgb',
  '<packagedef>\n<texture filename="textures/ui/magneto_icons1"/>\n</packagedef>\n')
w('with-imports/merge/packages/generated/maps/package/menus/characters_heads.pkgb',
  '<packagedef>\n<model filename="ui/models/characters/2501"/>\n</packagedef>\n')
print('written', sum(1 for f in M.rglob('*') if f.is_file()), 'files')
