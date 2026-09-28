"""Build an offline grouping corpus from Modex kits and installed SE/AE plugins.

Read-only against MO2. Uses only the Python standard library. Plugin selection
follows the saved MO2 left pane (first enabled entry wins); inactive fallback
sources are explicitly recorded for offline coverage, never enabled or deployed.
"""
import argparse
import collections
import hashlib
import json
import pathlib
import struct
import zlib


def records(data, start=0, end=None):
    end = len(data) if end is None else end
    pos = start
    while pos < end:
        if pos + 24 > end:
            raise ValueError('Truncated record header')
        kind = data[pos:pos + 4]
        size, flags, form = struct.unpack_from('<III', data, pos + 4)
        stop = pos + size if kind == b'GRUP' else pos + 24 + size
        if stop > end or stop < pos + 24:
            raise ValueError('Invalid record extent')
        if kind == b'GRUP':
            yield from records(data, pos + 24, stop)
        elif kind in (b'TES4', b'ARMO', b'ARMA'):
            payload = data[pos + 24:stop]
            if flags & 0x40000:
                expected, = struct.unpack_from('<I', payload)
                payload = zlib.decompress(payload[4:])
                if len(payload) != expected:
                    raise ValueError('Invalid compressed record')
            fields = collections.defaultdict(list)
            at = 0
            extended = None
            while at < len(payload):
                if at + 6 > len(payload):
                    raise ValueError('Truncated subrecord')
                tag, length = struct.unpack_from('<4sH', payload, at)
                at += 6
                if tag == b'XXXX':
                    if length != 4 or at + 4 > len(payload):
                        raise ValueError('Invalid extended subrecord')
                    extended, = struct.unpack_from('<I', payload, at)
                    at += 4
                    continue
                if extended is not None:
                    length, extended = extended, None
                if at + length > len(payload):
                    raise ValueError('Subrecord outside record')
                fields[tag].append(payload[at:at + length])
                at += length
            if extended is not None:
                raise ValueError('Dangling extended subrecord')
            yield kind, form, flags, fields
        pos = stop


def decode(value):
    value = value.rstrip(b'\0')
    try:
        return value.decode('utf-8')
    except UnicodeDecodeError:
        return value.decode('cp1252', errors='replace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mods', type=pathlib.Path, required=True)
    parser.add_argument('--profile', type=pathlib.Path, required=True)
    parser.add_argument('--kits', type=pathlib.Path, required=True)
    parser.add_argument('--data', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--sheet-rules', type=pathlib.Path)
    parser.add_argument('--screenshot-inventory', type=pathlib.Path)
    parser.add_argument('--extra-plugin', action='append', default=[],
                        help='Include an additional installed plugin for generic-family regression checks')
    args = parser.parse_args()
    references = []
    source_hashes = {}
    needed = set()
    needed.update(name.lower() for name in args.extra_plugin)
    for path in sorted(args.kits.glob('*.json')):
        raw = path.read_bytes()
        source_hashes[path.name] = hashlib.sha256(raw).hexdigest()
        for name, kit in json.loads(raw.decode('utf-8-sig')).items():
            items = []
            for editor, item in kit.get('Items', {}).items():
                plugin = item['Plugin']
                needed.add(plugin.lower())
                items.append(dict(editorID=editor, plugin=plugin,
                                  name=item.get('Name', ''),
                                  equipped=item.get('Equipped', True) is True))
            references.append(dict(name=name, items=items))
    if args.sheet_rules:
        sheet = json.loads(args.sheet_rules.read_text(encoding='utf-8'))
        needed.update(f['plugin'] for f in sheet['families'])
    if args.screenshot_inventory:
        screenshots = json.loads(args.screenshot_inventory.read_text(encoding='utf-8'))
        needed.update(p['plugin'].lower() for p in screenshots['packs'])
    paths = {}
    enabled = [line[1:] for line in args.profile.read_text(encoding='utf-8-sig').splitlines()
               if line.startswith('+')]
    directories = [(args.mods / name, True) for name in enabled]
    directories += [(args.data, True)]
    directories += [(p, False) for p in sorted(args.mods.iterdir()) if p.is_dir() and p.name not in enabled]
    for directory, active in directories:
        if not directory.is_dir():
            continue
        for path in directory.iterdir():
            key = path.name.lower()
            if key in needed and key not in paths and path.is_file():
                paths[key] = (path, active)
    corpus = dict(references=references, plugins=[], missingPlugins=sorted(needed - paths.keys()),
                  sourceHashes=source_hashes, source=str(args.kits))
    for key, (path, active) in sorted(paths.items()):
        raw = path.read_bytes()
        parsed = list(records(raw))
        localized = bool(parsed[0][2] & 0x80)
        masters = [decode(v) for v in parsed[0][3].get(b'MAST', [])]
        addon = {form: fields for kind, form, flags, fields in parsed if kind == b'ARMA' and not flags & 0x20}
        armors = []
        for kind, form, flags, fields in parsed:
            if kind != b'ARMO' or flags & 0x20 or not fields.get(b'EDID'):
                continue
            # The in-game generator snapshots the defining file, not a patch's
            # copies of records owned by its masters.
            if form >> 24 != len(masters):
                continue
            editor = decode(fields[b'EDID'][0])
            name_raw = b'' if localized else fields.get(b'FULL', [b''])[0].rstrip(b'\0')
            name = decode(name_raw)
            slot = fields.get(b'BOD2', fields.get(b'BODT', [b'\0' * 4]))[0]
            slots, = struct.unpack_from('<I', slot)
            links = [struct.unpack('<I', v)[0] for v in fields.get(b'MODL', []) if len(v) == 4]
            if not slots:
                for link in links:
                    addon_fields = addon.get(link, {})
                    addon_slot = addon_fields.get(b'BOD2', addon_fields.get(b'BODT', [b'\0' * 4]))[0]
                    slots |= struct.unpack_from('<I', addon_slot)[0]
            if not slots:
                continue
            models = sorted({decode(v).lower() for link in links for tag in (b'MOD2', b'MOD3', b'MOD4', b'MOD5')
                             for v in addon.get(link, {}).get(tag, [])})
            armors.append(dict(editorID=editor, name=name, nameBytes=list(name_raw), formID=form & 0xFFFFFF,
                               slots=slots, addons=links, models=models,
                               enchanted=b'EITM' in fields))
        corpus['plugins'].append(dict(name=path.name, path=str(path), active=active,
                                      sha256=hashlib.sha256(raw).hexdigest(), localized=localized,
                                      armors=armors))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(corpus, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(kits=len(references), plugins=len(corpus['plugins']),
                         armors=sum(len(p['armors']) for p in corpus['plugins']),
                         inactive=sum(not p['active'] for p in corpus['plugins']),
                         missing=corpus['missingPlugins']), ensure_ascii=False))


if __name__ == '__main__':
    main()
