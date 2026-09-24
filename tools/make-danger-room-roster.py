"""Writes the Danger Room: More Characters mod.

  python tools/make-danger-room-roster.py <game data/npcstat.eng> mods/danger-room-roster

Each character is a copy of one of the game's own npcstat entries under a new
name, with playable="true" (what puts a character in the main-menu Danger
Room's Sparring and Skirmish rosters) and any Danger Room adjustments. The
story entries are left untouched. The game finds a character's packages by
<name>_<skin>.pkgb, so [Copy] makes the original's two packages available
under the new name.
"""
from pathlib import Path
import re
import sys

NPCSTAT = Path(sys.argv[1])
M = Path(sys.argv[2])

# new name: (game entry, attributes to set)
ROSTER = {
    # The final-act boss, shrunk to about a Sentinel's height so he fits the
    # arenas and the camera; his story health scale of 30 is far too much here.
    'MasterMoldDR': ('mastermoldone', {'scale_factor': '0.55', 'size': '53 53 158', 'npchealthscale': '3'}),
    # The Muir Island boss. The roster's own Juggernaut is the level-5 flashback one.
    'JuggernautDR': ('JuggernautAct3', {'charactername': 'Juggernaut (Boss)'}),
    # The Hive's tougher spider platform.
    'SentinelSpiderDR': ('SentinelSpider_b', {}),
    # The two phases of the final Astral Plane fight.
    'ShadowKingDR': ('shadowking', {}),
    'ShadowKingTwoDR': ('shadowkingtwo', {'charactername': 'Shadow King (Final Form)'}),
}


def set_attribute(head, key, value):
    if re.search(r' %s="[^"]*"' % key, head):
        return re.sub(r' %s="[^"]*"' % key, ' %s="%s"' % (key, value), head)
    return head[:-1] + ' %s="%s">' % (key, value)


source = NPCSTAT.read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')
entries, copies = [], []
for name, (original, changes) in ROSTER.items():
    match = re.search(r'<stats name="%s"[\s>].*?</stats>' % re.escape(original), source, re.S)
    if not match:
        sys.exit('no npcstat entry ' + original)
    block = match.group(0)
    head = re.match(r'<stats [^>]*>', block).group(0)
    skin = re.search(r' skin="([^"]*)"', head).group(1)
    new = head.replace('name="%s"' % original, 'name="%s"' % name, 1)
    for key, value in dict(changes, playable='true').items():
        new = set_attribute(new, key, value)
    entries.append(block.replace(head, new, 1))
    for suffix in ('', '_nc'):
        copies.append('packages/generated/characters/%s_%s%s.pkgb = packages/generated/characters/%s_%s%s.pkgb'
                      % (original.lower(), skin, suffix, name.lower(), skin, suffix))

(M / 'merge/data').mkdir(parents=True, exist_ok=True)
(M / 'merge/data/npcstat.eng').write_text('<characters>\n' + '\n'.join(entries) + '\n</characters>\n',
                                          encoding='utf-8', newline='\n')
(M / 'mod.ini').write_text(
    '[Mod]\nName = Danger Room: More Characters\nType = other\nAuthor = KaikoClanworth1\nVersion = 0.2\n'
    "Description = Adds bosses to the main-menu Danger Room's Sparring and Skirmish rosters: Master Mold, "
    'Juggernaut (Boss), Sentinel Platform Mark II and both Shadow King forms. Use with Danger Room: Unlock '
    'Everything, since no training course unlocks them.\n\n[Copy]\n' + '\n'.join(copies) + '\n',
    encoding='utf-8', newline='\n')
print('\n'.join(copies))
