"""Compile sheet B/C aliases and merged ranges, never execute cell contents.

Input is the read-only inventory from inspect-outfit-sheet.py. Empty cells are
expanded ONLY inside their real merged range. A identifies a plugin, not a set.
D/E describe parts/variants and cannot union unrelated sets by color or slot.
"""
import argparse
import collections
import json
from pathlib import Path
import re

GENERIC = {'', '-', '바슬 이름', '세트명', '남성용', '남성 전용', '여성용',
           '바슬 없음', '없음', '[bns]', 'bns'}
PARTS = {'꼬리', '날개', '머리핀', '모자', '벨트', '뿔', '소', '스커트', '신발',
         '아뮬랫', '안경', '의상', '자켓', '장갑', '총', '팔찌', '팬티', '피어싱',
         '핫팬츠', '상의', '하의', '부츠', '투구', '갑옷', '망토', '목걸이',
         'armor', 'boots', 'gloves', 'skirt', 'panty', 'belt', 'amulet', 'jacket',
         'arm', 'dec', 'gun', 'hat', 'stockings', 'shoes'}


def clean(value):
    return re.sub(r'\s+', ' ', value).strip() if isinstance(value, str) else ''


def key(value):
    return ' '.join(re.findall(r'[^\W_]+', value.lower(), re.UNICODE))


def meaningful(value):
    if re.fullmatch(r'[0-9]{2,}\+', value):
        return True  # Explicit numeric set label, e.g. the sheet's "300+".
    compact = key(value).replace(' ', '')
    return (value.lower() not in GENERIC | PARTS | {'바지', '리본'} and '착용례' not in value
            and len(compact) >= (3 if compact.isascii() else 2)
            and not compact.isdigit())


def compile_families(inventory):
    nodes, skipped = [], []
    for sheet in inventory['sheets']:
        cells = {c['cell']: c['value'] for row in sheet['rows'] for c in row}
        anchors = {}
        for span in sheet['merges']:
            a, first, b, last = re.fullmatch(r'([A-Z]+)(\d+):([A-Z]+)(\d+)', span).groups()
            if a == b and a in 'ABC':
                for row in range(int(first), int(last) + 1):
                    anchors[f'{a}{row}'] = f'{a}{first}'
        for row in range(2, sheet['maxRow'] + 1):
            coords = [anchors.get(f'{col}{row}', f'{col}{row}') for col in 'ABC']
            a, b, c = (clean(cells.get(coord)) for coord in coords)
            plugins = re.findall(r'[^\r\n]+\.(?:esp|esl|esm)\s*(?=\n|$)',
                                 str(cells.get(coords[0], '')), re.I)
            plugins = [clean(p) for p in plugins]
            if not plugins or '착용례' in c or not (meaningful(b) or meaningful(c)):
                if (b or c) and coords[0] == f'A{row}':
                    skipped.append({'sheet': sheet['name'], 'row': row, 'A': a, 'B': b, 'C': c})
                continue
            if len(plugins) != 1:
                raise ValueError(f'Ambiguous plugin: {sheet["name"]}!A{row}')
            nodes.append(dict(plugin=plugins[0].lower(), sheet=sheet['name'], row=row,
                              b=b, c=c, bCell=coords[1], cCell=coords[2]))
    parent = list(range(len(nodes)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def union(i, j):
        parent[find(j)] = find(i)

    seen = {}
    part_roots = collections.defaultdict(list)
    for i, node in enumerate(nodes):
        # Repeated explicit set labels (e.g. Hepsy on ten separate rows) and
        # one merged BodySlide label connecting two translations are evidence.
        bindings = []
        if meaningful(node['c']):
            bindings.append(('set', node['plugin'], key(node['c'])))
        if meaningful(node['b']):
            bindings.append(('bspan', node['plugin'], node['sheet'], node['bCell']))
        for binding in bindings:
            if binding in seen:
                union(i, seen[binding])
            seen[binding] = i
        # Some tabs list every part as a "set" (Honoka Cosplay). Consolidate
        # an explicit multiword family prefix only when multiple distinct part
        # labels corroborate it. A single maker prefix such as TGO cannot qualify.
        words = key(node['c']).split()
        if words and len(words[-1]) == 1 and words[-1].isascii():
            words.pop()  # accessory letter variant, e.g. glasses B
        if len(words) >= 3 and words[-1] in PARTS:
            part_roots[node['plugin'], ' '.join(words[:-1])].append((i, words[-1]))
    derived = []
    for (plugin, root), entries in part_roots.items():
        if len({part for _, part in entries}) >= 2:
            for i, _ in entries[1:]:
                union(entries[0][0], i)
            derived.append((entries[0][0], root))
    groups = collections.defaultdict(list)
    for i, node in enumerate(nodes):
        groups[find(i)].append(node)
    root_aliases = collections.defaultdict(set)
    for i, root in derived:
        root_aliases[find(i)].add(root)
    families = []
    for root, members in groups.items():
        aliases = {node[col] for node in members for col in ('b', 'c') if meaningful(node[col])}
        for alias in list(aliases):
            if ' / ' in alias:
                aliases.update(part.strip() for part in alias.split(' / ') if meaningful(part.strip()))
        aliases.update(root_aliases[root])
        labels = root_aliases[root] or {node['c'] for node in members if meaningful(node['c'])} or aliases
        # A merged BodySlide set can span category-like C labels (Rayne's
        # jeans/top/sandals). Use its common B label instead of naming the
        # entire outfit after whichever short part label happens to sort first.
        b_labels = {node['b'] for node in members if meaningful(node['b'])}
        if len(labels) > 1 and len(b_labels) == 1 and labels & {'청바지', '수영복', '비키니', '뷰지', '크리스마스'}:
            labels = b_labels
        name = min(labels, key=lambda s: (len(key(s)), s))
        families.append(dict(plugin=members[0]['plugin'], name=name,
                             aliases=sorted(aliases),
                             cells=sorted({node['sheet'] + '!' + node[col] for node in members for col in ('bCell', 'cCell')})))
    families.sort(key=lambda f: (f['plugin'], f['name']))
    # An alias used by two distinct explicit families must not guess a winner.
    owners = collections.defaultdict(set)
    for i, family in enumerate(families):
        for alias in family['aliases']:
            owners[family['plugin'], key(alias)].add(i)
    ambiguous = [[plugin, alias] for (plugin, alias), ids in owners.items() if len(ids) > 1]
    return dict(sourceSha256=inventory['sha256'], worksheets=[s['name'] for s in inventory['sheets']],
                families=families, ambiguousAliases=ambiguous, skipped=skipped,
                policy='Community membership > explicit spreadsheet family > legacy inference')


def render(data):
    lines = ['// Generated by scripts/generate-sheet-grouping.py; do not edit.',
             '// Workbook SHA256: ' + data['sourceSha256'],
             'constexpr SheetAlias kSheetAliases[]{']
    for family in data['families']:
        for alias in family['aliases']:
            lines.append('  {' + json.dumps(alias, ensure_ascii=False) + '},')
    lines += ['};', 'constexpr SheetFamily kSheetFamilies[]{']
    offset = 0
    for family in data['families']:
        lines.append('  {' + ', '.join([json.dumps(family['plugin'], ensure_ascii=False),
                     json.dumps(family['name'], ensure_ascii=False), str(offset), str(len(family['aliases']))]) + '},')
        offset += len(family['aliases'])
    lines += ['};', '']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory', type=Path)
    parser.add_argument('rules', type=Path)
    parser.add_argument('include', type=Path)
    args = parser.parse_args()
    data = compile_families(json.loads(args.inventory.read_text(encoding='utf-8')))
    args.rules.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
    args.include.write_text(render(data), encoding='utf-8')
    print(f'{len(data["families"])} sheet families, {sum(len(f["aliases"]) for f in data["families"])} aliases')


if __name__ == '__main__':
    main()
