"""Adds the subway moves of the Early X-Men Xtraction Point mod.

  python tools/make-early-xmen-subways.py <game folder> mods/early-xmen-xtraction

The first mission's subway entrances fade to black and move only _HERO1_ to
the far side, which was the whole party when the mission had one hero. With
four heroes the other three were left behind, and when the player controlled
someone other than party slot 1, slot 1 moved without them.

Each subway script now moves every living hero, as the game's own party moves
do (asteroid_m/2_1_meet_magneto.py): _HERO1_ to the original spot and
_HERO2_-_HERO4_ to spots added beside it, to either side and one step ahead
in the direction it faces. The map's null group is carried whole with the new
spots appended, because a merge replaces a placed-object group by its type.
"""
from pathlib import Path
import math
import re
import sys

GAME = Path(sys.argv[1])
M = Path(sys.argv[2])
SIDE, AHEAD = 45.0, 55.0

# script: (map, destination)
SUBWAYS = {
    'subwaydowna': ('nyc1_1_1', 'subway_downA'),
    'subwaydownb': ('nyc1_1_1', 'subway_downB'),
    'subwayupa': ('nyc1_1_1', 'subway_upA'),
    'subwayupb': ('nyc1_1_1', 'subway_upb'),
    'subway1': ('nyc1_1_2', 'null05'),
    'subway2': ('nyc1_1_2', 'null01'),
    'subway3': ('nyc1_1_2', 'null13'),
    'subway4': ('nyc1_1_2', 'null09'),
}


def fmt(v):
    return ('%.3f' % v).rstrip('0').rstrip('.')


def spots(inst):
    name = re.search(r'name="([^"]*)"', inst).group(1)
    x, y, z = map(float, re.search(r'pos="([^"]*)"', inst).group(1).split())
    orient = re.search(r'orient="([^"]*)"', inst)
    yaw = float(orient.group(1).split()[2]) if orient else 0.0
    forward = (math.cos(yaw), math.sin(yaw))
    side = (-forward[1], forward[0])
    out = []
    for n, (s, a) in zip((2, 3, 4), ((SIDE, 0), (-SIDE, 0), (0, AHEAD))):
        px, py = x + side[0] * s + forward[0] * a, y + side[1] * s + forward[1] * a
        extents = ' '.join(fmt(v) for v in (px - 6, py - 6, z, px + 6, py + 6, z + 12))
        out.append('<inst name="%s_p%d" pos="%s %s %s" extents="%s"%s/>'
                   % (name.lower(), n, fmt(px), fmt(py), fmt(z), extents,
                      ' orient="%s"' % orient.group(1) if orient else ''))
    return out


maps = {}
for script, (map_name, destination) in SUBWAYS.items():
    maps.setdefault(map_name, []).append(destination)

for map_name, destinations in maps.items():
    game_map = (GAME / 'maps/nyc/alison' / (map_name + '.eng')).read_text(encoding='utf-8', errors='replace')
    groups = re.findall(r'<entinst type="null">.*?</entinst>', game_map, re.S)
    if len(groups) != 1:
        sys.exit('%s: expected one null group' % map_name)
    group = groups[0].replace('\r\n', '\n')
    added = []
    for destination in destinations:
        inst = re.search(r'<inst name="%s"[^>]*>' % re.escape(destination), group)
        if not inst:
            sys.exit('%s: no %s' % (map_name, destination))
        added += spots(inst.group(0))
    group = group.replace('</entinst>', '\n'.join(added) + '\n</entinst>')
    fragment_path = M / 'merge/maps/nyc/alison' / (map_name + '.eng')
    fragment = fragment_path.read_text(encoding='utf-8')
    fragment = re.sub(r'<entinst type="null">.*?</entinst>\n', '', fragment, flags=re.S)
    fragment = fragment.replace('</world>', group + '\n</world>')
    fragment_path.write_text(fragment, encoding='utf-8', newline='\n')

for script, (map_name, destination) in SUBWAYS.items():
    source = (GAME / 'scripts/nyc/alison' / (script + '.py')).read_text(encoding='utf-8')
    lines = source.replace('\r\n', '\n').split('\n')
    out = []
    for line in lines:
        if line.startswith('setCombatNode("_HERO1_", "idle" )'):
            out += ['setCombatNode("_HERO%d_", "idle" )' % n for n in range(1, 5)]
        elif line.startswith('copyOriginAndAngles("_HERO1_",'):
            for n in range(1, 5):
                spot = destination.lower() if n == 1 else '%s_p%d' % (destination.lower(), n)
                out += ['hero%d = alive("_HERO%d_" )' % (n, n), 'if hero%d > 0' % n,
                        '     copyOriginAndAngles("_HERO%d_", "%s" )' % (n, spot), 'endif']
        else:
            out.append(line)
    if out == lines:
        sys.exit(script + ': nothing to change')
    target = M / 'files/scripts/nyc/alison' / (script + '.py')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes('\r\n'.join(out).encode('utf-8'))
print('subway moves written for', ', '.join(SUBWAYS))
