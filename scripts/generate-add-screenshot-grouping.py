"""Compile one ADD kit identity per Screenshot PNG into verified record memberships.

Names are evidence, never commands. Each PNG keeps its own kit name. Photo
sequence numbers do not select armor/color numbers: the user requested the
whole outfit's parts and variations. Reviewed selectors separate collections
whose numbers really identify different outfits. Candidate generation resolves
slots and coordinates variations after this membership stage.
"""
import argparse
import collections
import json
from pathlib import Path
import re

IGNORE = {'se', 'sse', 'cbbe', '3ba', 'armor', 'armour', 'outfit', 'outfits', 'set', 'standalone'}


def words(text):
    text = re.sub(r'([a-z])([A-Z])', r'\1 \2', text)
    text = re.sub(r'([A-Z])([A-Z][a-z])', r'\1 \2', text)
    return re.findall(r'[a-z]+|[0-9]+|[^\W\d_a-z]+', text.lower(), re.UNICODE)


def significant(text):
    text = re.sub(r'^(?:\s*\[[^]]+\])+', ' ', text)
    text = re.sub(r'\b(?:SE|SSE|CBBE|3BA)\b', ' ', text, flags=re.I)
    text = re.sub(r'\bby Team TAL\b', '', text, flags=re.I)
    return [w for w in words(text) if w not in IGNORE]


def compact(text):
    return ''.join(words(text))


def title_parts(title):
    match = re.fullmatch(r'(.+?) (\d{2})(?: \[([^]]+)\])?', title)
    return (match[1], int(match[2]), match[3] or '') if match else (title, None, '')


def score_root(alias, armor):
    root = significant(alias)
    if not root:
        return 0
    name = words(re.sub(r'^(?:\s*\[[^]]+\])+', ' ', armor['name']))
    value = ''.join(root)
    # Token-sequence with part words between title and variant (Dark Knight
    # ... [Gold] [Sin Terrna]), plus spacing-insensitive localized prefixes.
    pos = 0
    for token in name:
        if pos < len(root) and token == root[pos]:
            pos += 1
    if (pos == len(root) and name and name[0] == root[0] and
            (len(root) < 2 or name[:2] == root[:2])):
        return 1000 + len(value)
    name_value = ''.join(name)
    ends = {len(''.join(name[:i])) for i in range(1, len(name) + 1)}
    if name_value.startswith(value) and len(value) in ends:
        return 1000 + len(value)
    # English filename titles often survive translation only in the EDID.
    # Require a substantial identity; short generic fragments cannot qualify.
    editor = compact(armor['editorID'])
    if value.isascii() and len(value) >= 5 and value in editor:
        at = editor.index(value) + len(value)
        if not value[-1].isdigit() or at == len(editor) or not editor[at].isdigit():
            return len(value)
    return 0


def compile_rules(inventory, corpus, overrides):
    plugins = {p['name'].lower(): p for p in corpus['plugins']}
    rules, report = [], []
    for pack in inventory['packs']:
        plugin = plugins[pack['plugin'].lower()]
        if Path(plugin['path']) != Path(pack['pluginPath']):
            raise ValueError('The inspected plugin must come from its own screenshot mod')
        families = collections.defaultdict(list)
        for photo in pack['screenshots']:
            base, number, variant = title_parts(photo['name'])
            families[base].append((photo, number, variant))
        pools = collections.defaultdict(list)
        for armor in plugin['armors']:
            scores = {}
            for base in families:
                aliases = overrides.get('aliases', {}).get(pack['plugin'] + '|' + base, [base])
                scores[base] = max((score_root(alias, armor) for alias in aliases), default=0)
            best = max(scores.values(), default=0)
            winners = [base for base, score in scores.items() if score == best and score]
            if len(winners) == 1:
                pools[winners[0]].append(armor)
        for base, photos in families.items():
            pool = pools[base]
            for photo, number, variant in photos:
                override = overrides.get('photos', {}).get(pack['plugin'] + '|' + photo['name'])
                mode = 'outfit-family-all-variations'
                selected = pool
                if override is not None:
                    mode = 'reviewed-selector'
                    selected = [a for a in plugin['armors'] if
                                any(re.search(pattern, a['editorID'] + '|' + a['name'], re.I) for pattern in override['include']) and
                                not any(re.search(pattern, a['editorID'] + '|' + a['name'], re.I) for pattern in override.get('exclude', []))]
                entries = [dict(editorID=a['editorID'], localFormID=a['formID'], name=a['name'], anchor=bool(a['slots'] & 4)) for a in selected]
                if entries and not any(e['anchor'] for e in entries):
                    for e in entries:
                        e['anchor'] = True
                rules.append(dict(plugin=pack['plugin'].lower(), name=photo['name'], file=photo['file'],
                                  sourceSha256=photo['sha256'], members=entries, method=mode))
                if not entries:
                    report.append(dict(plugin=pack['plugin'], photo=photo['name'], base=base, poolCount=len(pool),
                                       examples=[a['editorID'] + ' | ' + a['name'] for a in pool[:12]]))
    return dict(pngCount=len(rules), groups=rules, unresolved=report)


def render(rules):
    lines = ['// Generated by scripts/generate-add-screenshot-grouping.py; do not edit.',
             'constexpr ScreenshotMember kScreenshotMembers[]{']
    for group in rules['groups']:
        for member in group['members']:
            lines.append('  {' + json.dumps(member['editorID'].lower()) + ', ' + str(member['localFormID']) + ', ' + str(member['anchor']).lower() + '},')
    lines += ['};', 'constexpr ScreenshotReference kScreenshotReferences[]{']
    offset = 0
    for group in rules['groups']:
        lines.append('  {' + ', '.join([json.dumps(group['plugin']), json.dumps(group['name'], ensure_ascii=False), str(offset), str(len(group['members']))]) + '},')
        offset += len(group['members'])
    return '\n'.join(lines + ['};', ''])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['inventory', 'corpus', 'overrides', 'output', 'include']:
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    data = compile_rules(*(json.loads(p.read_text(encoding='utf-8')) for p in [args.inventory, args.corpus, args.overrides]))
    args.output.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
    args.include.write_text(render(data), encoding='utf-8')
    print(f'{data["pngCount"]} PNG identities, {len(data["unresolved"])} unresolved memberships')
