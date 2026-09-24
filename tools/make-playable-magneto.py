"""Writes the Playable Magneto template mod."""
from pathlib import Path
import sys

M = Path(sys.argv[1])
MISSION_SCRIPTS = sys.argv[2:]   # BehavEd-format mission start scripts, for the unlock
for sub in ['files/data/powerstyles', 'files/packages/generated/characters', 'merge/data', 'append/scripts/missions']:
    (M / sub).mkdir(parents=True, exist_ok=True)

def w(rel, text):
    (M / rel).write_text(text, encoding='utf-8', newline='\n')

w('mod.ini', """[Mod]
Name = Playable Magneto
Type = character
Author = KaikoClanworth1
Version = 0.1
Description = Magneto, from his boss model and powers, as a playable hero from level 1. A template for character mods: a new hero built only from what the game already has.
""")

# ---- powers: a hero powerstyle from the NPC Magneto's two powers ---------------
DAMAGE_BOLT = ['L2', 'L3', 'L4', 'L5', 'M1']
DAMAGE_CRUSH = ['L3', 'L4', 'L5', 'M1', 'M2']
USAGE = ['P1', 'P1+', 'P2', 'P2+', 'P3']

def levels(prefix, talent, damages, first):
    out = []
    for n in range(2, len(damages) + 1):
        out.append(f'''<FightMove Name="{prefix}{n}" fallback="{prefix}{n-1}" inherit="{prefix}{n-1}">
<require cat="skill" item="{talent}" level="{n}"/>
{first(damages[n-1], USAGE[n-1])}
</FightMove>''')
    return '\n'.join(out)

bolt_triggers = lambda dmg, use: f'<trigger tag="1" Damage="{dmg}" PowerUsage="{use}"/>'
crush_triggers = lambda dmg, use: f'<trigger tag="1" Damage="{dmg}" PowerUsage="{use}"/>\n<trigger tag="2" Damage="{dmg}" PowerUsage="{use}"/>'

w('files/data/powerstyles/ps_magnetohero.xml', f'''<PowerStyle CanSteal="true" exclusive="magneto">
<FightMove Name="magbolt1" lockangles="true" animenum="ea_power1" priority="5" aitype="projectile" aireusetime="5" icon="0" comboTextStarter="Magnetic" comboTextFinisher="Bolt">
<require cat="skill" item="magneto_bolt" level="1"/>
<trigger time="0" name="effect" effect="powers/magneto_pow1_charge" bolt="Bip01 L Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow1_arc"/>
<trigger time="0.4" name="sound" sound="character/magnet_m/power1"/>
<trigger tag="1" time="0.5" name="metal_balls" inherit="beam" spawneffect="powers/magneto_pow1_beamatk" beameffect="powers/magneto_pow1_beam" HitEffect="powers/magneto_pow1_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{DAMAGE_BOLT[0]}" beamBolt="Bip01 L Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="3" radius="10"/>
<chain action="idle" result="idle"/>
</FightMove>
{levels("magbolt", "magneto_bolt", DAMAGE_BOLT, bolt_triggers)}
<FightMove Name="magcrush1" lockangles="true" animenum="ea_power8" priority="5" aitype="projectile" aireusetime="10" icon="1" comboTextStarter="Magnetic" comboTextFinisher="Crush">
<require cat="skill" item="magneto_crush" level="1"/>
<trigger time="0" name="sound" sound="character/magnet_m/power2"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_charge" bolt="Bip01 L Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_charge" bolt="Bip01 R Hand"/>
<trigger time="0" name="effect" effect="powers/magneto_pow2_grow"/>
<trigger tag="1" time="0.55" name="metal_balls" inherit="beam" spawneffect="powers/magneto_pow2_sentatk" beameffect="powers/magneto_pow2_beam" HitEffect="powers/magneto_pow2_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{DAMAGE_CRUSH[0]}" beamBolt="Bip01 L Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="5" radius="10"/>
<trigger tag="2" time="0.55" name="metal_balls" inherit="beam" beameffect="powers/magneto_pow2_beam" hiteffect="powers/magneto_pow2_hit" VictimEventTag="0" DamageType="dmg_magnetic" Damage="{DAMAGE_CRUSH[0]}" beamBolt="Bip01 R Hand" PowerUsage="{USAGE[0]}" PowerAttack="true" maxrange="800" damagelevel="5" radius="10"/>
<chain action="idle" result="idle"/>
</FightMove>
{levels("magcrush", "magneto_crush", DAMAGE_CRUSH, crush_triggers)}
<FightMove Name="power_attack" fallback="magbolt5" inherit="magbolt5">
<require cat="skill" item="magneto_bolt" level="6"/>
</FightMove>
<FightMove Name="power_smash" fallback="magcrush5" inherit="magcrush5">
<require cat="skill" item="magneto_crush" level="6"/>
</FightMove>
</PowerStyle>
''')

# ---- the hero -------------------------------------------------------------------
def talent_levels(dmg, first_level_req):
    out = []
    reqs = [None, 3, 5, 7, 9]
    for i, d in enumerate(dmg):
        text = f'^{d} Magnetic Damage. ^{USAGE[i]} Energy.'
        if reqs[i] is None:
            out.append(f'<level description="{text}"/>')
        else:
            out.append(f'<level description="{text}">\n<require cat="level" level="{reqs[i]}"/>\n</level>')
    return '\n'.join(out)

w('merge/data/herostat.eng', f'''<characters>
<stats name="Magneto" charactername="Magneto" skin="2501" sounddir="magnet_m" powerstyle="ps_magnetohero" characteranims="25_magneto" level="1" strength="3" speed="4" body="4" mind="7" team="hero" RatingMelee="0.3" RatingRanged="0.5" RatingSupport="0.1" RatingDurability="0.4" scriptlevel="3" ailevel="2" canSeeStealthed="true" playable="true">
<Race name="Mutant"/>
<Talent name="magneto_bolt" level="1" descname="Magnetic Bolt" description="Hurls a beam of magnetic force at an enemy." icon="0" power="0">
{talent_levels(DAMAGE_BOLT, None)}
</Talent>
<Talent name="magneto_crush" level="0" descname="Magnetic Crush" description="Two beams of crushing magnetic force." icon="1" power="1">
{talent_levels(DAMAGE_CRUSH, 2)}
</Talent>
<Talent name="fightstyle_psionic" level="1"/>
<Talent name="critical" level="0"/>
<Talent name="toughness" level="0"/>
<Talent name="leadership" level="0"/>
<Talent name="mutantmastery" level="0"/>
</stats>
</characters>
''')

# ---- packages: what loads with him, as text; the launcher compiles them ----------
effects = ['magneto_pow1_charge', 'magneto_pow1_hit', 'magneto_pow1_arc', 'magneto_pow1_beamatk', 'magneto_pow1_beam',
           'magneto_pow2_charge', 'magneto_pow2_grow', 'magneto_pow2_sentatk', 'magneto_pow2_beam', 'magneto_pow2_hit']
skin_common = '''<actorskin filename="2501"/>
<actoranimdb filename="25_magneto"/>
<model filename="hud/hud_head_2501"/>
<model filename="ui/hud/characters/2501"/>
'''
cape = '<actoranimdb filename="99_cape"/>\n<actoranimdb filename="9901"/>\n'
w('files/packages/generated/characters/magneto_2501.pkgb', '<packagedef>\n' + skin_common +
  ''.join(f'<effect filename="powers/{e}"/>\n' for e in effects) +
  '<fightstyle filename="data/powerstyles/ps_magnetohero"/>\n' + cape + '</packagedef>\n')
w('files/packages/generated/characters/magneto_2501_nc.pkgb', '<packagedef>\n' + skin_common + cape + '</packagedef>\n')
w('files/packages/generated/characters/magneto_xml.pkgb', '<packagedef>\n<fightstyle filename="data/powerstyles/ps_magnetohero"/>\n</packagedef>\n')

for name in MISSION_SCRIPTS:
    w(f'append/scripts/missions/{name}', 'setInCampaign("magneto", "TRUE" )\n')
print('written', sum(1 for _ in M.rglob('*') if _.is_file()), 'files')
