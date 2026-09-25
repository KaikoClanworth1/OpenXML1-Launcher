"""Adds the mansion visits to the Early X-Men Xtraction Point mod.

  python tools/make-early-xmen-mansion.py <game folder> mods/early-xmen-xtraction

Every mansion visit holds the team to Magma alone (maxheros="1", Magma
required). Here each one takes four heroes: Magma stays required, and the
early X-Men are recommended, so the team starts as Magma, Wolverine, Cyclops
and Storm and can be changed at the subbasement's Xtraction Point.

A team is placed at a map's player start, one position per hero, and heroes
without a position are left out of the map, so every player start in the
mansion maps gets up to four positions. New positions are chosen on walkable
floor: the maps' own navigation grids (cellSize 40, cell = floor(pos / 40),
which puts 135 of the game's 138 mansion starts on walkable cells). The later
visits reuse the first visit's building and have empty grids; they are checked
against the first visit's grids, which match all of their starts. The two
cutscene maps have no grid, and a few starts sit off their grid (elevators);
their positions sit just beside and ahead of the start.

The Danger Room lessons (mansion1b, mansion3b, mansion3_dangerroom,
dr_mansion1b), the E3 demo mansion and the GRSO attack (already four heroes)
are left alone.
"""
from pathlib import Path
import math
import re
import sys

GAME = Path(sys.argv[1])
M = Path(sys.argv[2])
MISSIONS = ['mansion1', 'mansion1a', 'mansion2', 'mansion3', 'mansion3_uniform', 'mansion4',
            'mansion4_grso_done', 'mansion5', 'mansion6', 'mansion7', 'mansion8']
RECOMMENDED = ['wolverine', 'cyclops', 'storm', 'phoenix', 'beast', 'iceman']
SKIP_MAPS = {'danger_room', 'mansion_back4_grso', 'mansion_front4_grso'}
# (side, ahead) offsets tried in order, in units along the start's facing.
CANDIDATES = [(-45, 45), (45, 45), (0, 90), (-45, 0), (45, 0), (-90, 45), (90, 45), (-45, 90), (45, 90),
              (0, 135), (-90, 0), (90, 0), (0, -50), (-45, -50), (45, -50)]


def grid(nav):
    try:
        text = nav.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return None
    cells = {tuple(map(int, c.split()[:2])) for c in re.findall(r'<c p="([^"]*)"', text)}
    return cells or None


def fmt(v):
    return ('%.3f' % v).rstrip('0').rstrip('.')


def cell(x, y):
    return (math.floor(x / 40), math.floor(y / 40))


map_files = sorted(p for p in (GAME / 'maps/mansion').glob('man*/*')
                   if p.suffix in ('.eng', '.xml') and p.stem not in SKIP_MAPS)
grids = {p: grid(p.with_suffix('.nav')) for p in map_files}
starts_of = {}
for path in map_files:
    text = path.read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')
    groups = re.findall(r'<entinst type="(player_start[^"]*)">.*?</entinst>', text, re.S)
    if groups:
        starts_of[path] = text

report = []
for path, text in starts_of.items():
    cells = grids[path]
    points = [tuple(map(float, p.split())) for p in re.findall(r'<inst name="player_start[^"]*" pos="([^"]*)"', text)]
    source = 'own grid'
    if not cells:
        best, hits = None, -1
        for other, other_cells in grids.items():
            if other_cells:
                h = sum(cell(x, y) in other_cells for x, y, z in points)
                if h > hits:
                    best, hits = other, h
        if best is not None and hits == len(points):
            cells, source = grids[best], 'grid of ' + best.parent.name + '/' + best.stem
        else:
            cells, source = None, 'no grid'
    added = unchecked = 0
    out = []
    for group in re.finditer(r'<entinst type="(player_start[^"]*)">(.*?)</entinst>', text, re.S):
        kind, body = group.group(1), group.group(2)
        insts = re.findall(r'<inst [^>]*/>', body)
        if len(insts) >= 4:
            continue
        first = insts[0]
        x, y, z = map(float, re.search(r'pos="([^"]*)"', first).group(1).split())
        orient = re.search(r'orient="([^"]*)"', first)
        yaw = float(orient.group(1).split()[2]) if orient else 0.0
        fwd = (math.cos(yaw), math.sin(yaw))
        side = (-fwd[1], fwd[0])
        # A start off the grid (an elevator the AI never walks) cannot be checked by it.
        group_cells = cells if cells is not None and cell(x, y) in cells else None
        taken = [(float(a), float(b)) for a, b, _ in (re.search(r'pos="([^"]*)"', i).group(1).split() for i in insts)]
        new = []
        for s, a in CANDIDATES:
            if len(taken) >= 4:
                break
            px, py = x + side[0] * s + fwd[0] * a, y + side[1] * s + fwd[1] * a
            if group_cells is not None and cell(px, py) not in group_cells:
                continue
            if any(math.hypot(px - tx, py - ty) < 40 for tx, ty in taken):
                continue
            taken.append((px, py))
            ext = ' '.join(fmt(v) for v in (px - 18, py - 22, z, px + 18, py + 28, z + 84))
            new.append('<inst name="%s" pos="%s %s %s" extents="%s"%s/>'
                       % (kind, fmt(px), fmt(py), fmt(z), ext, ' orient="%s"' % orient.group(1) if orient else ''))
        if len(taken) < 4:
            sys.exit('%s %s: only %d positions on the floor' % (path, kind, len(taken)))
        added += len(new)
        unchecked += 0 if group_cells is not None else len(new)
        out.append('<entinst type="%s">\n%s\n</entinst>' % (kind, '\n'.join(i for i in insts + new)))
    # The subbasement's Xtraction Point offers no team change: its first
    # extractionPointLite flag is off, as it is on the "noteamchange" beacon.
    # The final Master Mold fight's point, which does change teams, has it on.
    lite = re.search(r'<entity name="(xtraction_point_mansion)"[^>]*actscript="extractionPointLite\(([^)]*)\)"', text)
    if lite:
        flags = lite.group(2).split(',')
        flags[1] = "'true'"
        out.append('<entity name="%s" model="puzzles/beacon_xtraction" actscript="extractionPointLite(%s)" mod-attributes="true"/>'
                   % (lite.group(1), ','.join(flags)))
    if out:
        rel = path.relative_to(GAME)
        target = M / 'merge' / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('<world>\n' + '\n'.join(out) + '\n</world>\n', encoding='utf-8', newline='\n')
        report.append('%s: %d positions added (%s, %d unchecked)' % (rel, added, source, unchecked))

for name in MISSIONS:
    game_file = (GAME / 'data/missions' / (name + '.eng')).read_text(encoding='utf-8', errors='replace')
    if not re.search(r'<REQUIREDHERO name="magma"', game_file, re.I):
        sys.exit(name + ': Magma is not its required hero')
    lines = ['<MISSION maxheros="4">'] + ['<RECOMMENDEDHERO name="%s"/>' % h for h in RECOMMENDED] + ['</MISSION>']
    target = M / 'merge/data/missions' / (name + '.eng')
    target.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
print('\n'.join(report))
print(len(MISSIONS), 'missions opened to four heroes')
