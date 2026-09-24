"""Writes the Danger Room: More Characters mod.

  python tools/make-danger-room-roster.py <game folder> mods/danger-room-roster

Each character is a copy of one of the game's own npcstat entries under a new
name, with playable="true" (what puts a character in the main-menu Danger
Room's Sparring and Skirmish rosters) and any Danger Room adjustments. The
story entries are left untouched. The game finds a character's packages by
<name>_<skin>.pkgb, so [Copy] makes the original's two packages available
under the new name.

Bosses whose powers summon other characters get a Danger Room powerstyle of
their own, made from their story one with only those moves replaced by their
own attacks, and packages (as text, decoded from the originals) that load it.
"""
from pathlib import Path
import re
import subprocess
import sys

GAME = Path(sys.argv[1])
M = Path(sys.argv[2])
DECODER = Path(__file__).resolve().parents[1] / 'build/Release/launcher-test.exe'


def move(text, name):
    match = re.search(r'<FightMove Name="%s".*?</FightMove>' % re.escape(name), text, re.S)
    if not match:
        sys.exit('no move ' + name)
    return match.group(0)


def must(text, old, new, count=1):
    if text.count(old) < count:
        sys.exit('expected %r' % old)
    return text.replace(old, new)


def shadow_king_one(text):
    # Boost summoned six shades and made him immune to damage for 31 seconds;
    # the Xtreme ended that vortex. Boost is now a three-spear throw and the
    # Xtreme a wider, stronger spear spin, both his own attacks.
    attack, smash = move(text, 'power_attack'), move(text, 'power_smash')
    boost = must(attack, 'Name="power_attack"', 'Name="power_boost"')
    boost = must(boost, 'aireusetime="5"', 'aireusetime="8"')
    boost = must(boost, 'count="1"', 'count="3" Spread="25"')
    boost = must(boost, 'PowerUsage="P3"', 'PowerUsage="P5"')
    xtreme = must(smash, 'Name="power_smash"', 'Name="power_xtreme"')
    xtreme = must(xtreme, 'damage="M3"', 'damage="H1"')
    xtreme = must(xtreme, 'maxrange="168"', 'maxrange="224"')
    xtreme = must(xtreme, 'maxrange="192"', 'maxrange="256"')
    text = text.replace(move(text, 'power_boost'), boost)
    return text.replace(move(text, 'power_xtreme'), xtreme)


def shadow_king_two(text):
    # Boost summoned six shades and the Xtreme two copies of himself. Boost is
    # now a stronger, longer fire breath; the Xtreme keeps its mirror-image
    # effects and releases a radial blast instead of the copies.
    attack = move(text, 'power_attack')
    boost = must(attack, 'Name="power_attack"', 'Name="power_boost"')
    boost = must(boost, 'aireusetime="4"', 'aireusetime="8"')
    first = boost.index('\n') + 1
    boost = boost[:first] + '<event name="breath_big" inherit="breath_dmg" damage="L5" MaxRange="260"/>\n' + boost[first:]
    boost = must(boost, 'name="breath_dmg"/>', 'name="breath_big"/>')
    old = move(text, 'power_xtreme')
    xtreme = '\n'.join(line for line in old.split('\n') if 'spawn_actor' not in line)
    xtreme = must(xtreme, ' nodamage="true"', '')
    first = xtreme.index('\n') + 1
    xtreme = (xtreme[:first]
              + '<event name="mirror_blast" inherit="punch" AttackType="blast" PowerAttack="true" Arc="360" '
                'maxrange="260" damage="M4" knockback="K8">\n<damageMod name="dmgmod_auto_knockback"/>\n</event>\n'
              + xtreme[first:])
    xtreme = must(xtreme, '<chain action="idle"', '<trigger time="0.46" name="mirror_blast"/>\n<chain action="idle"')
    text = text.replace(move(text, 'power_boost'), boost)
    return text.replace(old, xtreme)


def master_mold(text):
    # The Xtreme threw spider bombs that hatch into spider mines. It is now a
    # barrage of his Boost missiles, eight of them, alternating arms.
    boost, old = move(text, 'power_boost'), move(text, 'power_xtreme')
    events = [line for line in boost.split('\n') if line.startswith('<event ')]
    triggers = []
    for i in range(8):
        t = 0.3 + i * 0.08
        triggers.append('<trigger time="%.2f" name="sound" sound="character/master_m/rocket_launch"/>' % t)
        triggers.append('<trigger time="%.2f" name="mm_missile_%s"/>' % (t, 'lr'[i % 2]))
    xtreme = '\n'.join([old.split('\n')[0]] + events + [old.split('\n')[1]] + triggers
                       + ['<chain action="Idle" result="idle"/>', '</FightMove>'])
    return text.replace(old, xtreme)


# new name: (game entry, attributes to set, (story powerstyle, Danger Room powerstyle, change) or None)
ROSTER = {
    # The final-act boss, shrunk to about a Sentinel's height so he fits the
    # arenas and the camera; his story health scale of 30 is far too much here.
    'MasterMoldDR': ('mastermoldone', {'scale_factor': '0.55', 'size': '53 53 158', 'npchealthscale': '3'},
                     ('ps_mastermold_one', 'ps_mastermolddr', master_mold)),
    # The Muir Island boss. The roster's own Juggernaut is the level-5 flashback one.
    'JuggernautDR': ('JuggernautAct3', {'charactername': 'Juggernaut (Boss)'}, None),
    # The Hive's tougher spider platform.
    'SentinelSpiderDR': ('SentinelSpider_b', {}, None),
    # The two phases of the final Astral Plane fight.
    'ShadowKingDR': ('shadowking', {}, ('ps_shadowking_one', 'ps_shadowkingdr_one', shadow_king_one)),
    'ShadowKingTwoDR': ('shadowkingtwo', {'charactername': 'Shadow King (Final Form)'},
                        ('ps_shadowking_two', 'ps_shadowkingdr_two', shadow_king_two)),
}


def set_attribute(head, key, value):
    if re.search(r' %s="[^"]*"' % key, head):
        return re.sub(r' %s="[^"]*"' % key, ' %s="%s"' % (key, value), head)
    return head[:-1] + ' %s="%s">' % (key, value)


def read_text(path):
    return path.read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


def write(relative, text):
    path = M / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


source = read_text(GAME / 'data/npcstat.eng')
entries, copies = [], []
for name, (original, changes, style) in ROSTER.items():
    match = re.search(r'<stats name="%s"[\s>].*?</stats>' % re.escape(original), source, re.S)
    if not match:
        sys.exit('no npcstat entry ' + original)
    block = match.group(0)
    head = re.match(r'<stats [^>]*>', block).group(0)
    skin = re.search(r' skin="([^"]*)"', head).group(1)
    new = head.replace('name="%s"' % original, 'name="%s"' % name, 1)
    attributes = dict(changes, playable='true')
    if style:
        attributes['powerstyle'] = style[1]
    for key, value in attributes.items():
        new = set_attribute(new, key, value)
    entries.append(block.replace(head, new, 1))
    for suffix in ('', '_nc'):
        game_package = 'packages/generated/characters/%s_%s%s.pkgb' % (original.lower(), skin, suffix)
        mod_package = 'packages/generated/characters/%s_%s%s.pkgb' % (name.lower(), skin, suffix)
        if not style:
            copies.append('%s = %s' % (game_package, mod_package))
            continue
        package = subprocess.run([str(DECODER), '--decode', str(GAME / game_package)],
                                 capture_output=True, text=True, check=True).stdout
        package = package.replace('data/powerstyles/%s"' % style[0], 'data/powerstyles/%s"' % style[1])
        write('files/' + mod_package, package if package.endswith('\n') else package + '\n')
    if style:
        story = next(p for p in (GAME / 'data/powerstyles').glob(style[0] + '.*') if p.suffix in ('.xml', '.eng'))
        write('files/data/powerstyles/%s.xml' % style[1], style[2](read_text(story)))

write('merge/data/npcstat.eng', '<characters>\n' + '\n'.join(entries) + '\n</characters>\n')
write('mod.ini',
      '[Mod]\nName = Danger Room: More Characters\nType = other\nAuthor = KaikoClanworth1\nVersion = 0.3\n'
      "Description = Adds bosses to the main-menu Danger Room's Sparring and Skirmish rosters: Master Mold, "
      'Juggernaut (Boss), Sentinel Platform Mark II and both Shadow King forms. Their summoning powers are '
      'replaced by their own attacks. Use with Danger Room: Unlock Everything, since no training course '
      'unlocks them.\n\n[Copy]\n' + '\n'.join(copies) + '\n')
print('written', sum(1 for f in M.rglob('*') if f.is_file()), 'files')
