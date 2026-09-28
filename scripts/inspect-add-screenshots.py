"""Inventory ADD 01..12 screenshot names without editing mods or reading image text."""
import argparse
import hashlib
import json
from pathlib import Path


def inspect(mods):
    result = {'modsRoot': str(mods), 'packs': []}
    for number in range(1, 13):
        directory = mods / f'[의상] ADD {number:02}'
        plugin = directory / f'ADD {number:02}.esp'
        screenshots = directory / 'Screenshot'
        if not plugin.is_file() or not screenshots.is_dir():
            raise ValueError(f'Missing expected ADD plugin/screenshot directory: {directory}')
        images = [dict(name=path.stem, file=str(path.relative_to(directory)),
                       sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                  for path in sorted(screenshots.rglob('*')) if path.is_file() and path.suffix.lower() == '.png']
        result['packs'].append(dict(mod=directory.name, plugin=plugin.name, pluginPath=str(plugin), screenshots=images))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mods', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    inventory = inspect(args.mods)
    args.output.write_text(json.dumps(inventory, ensure_ascii=False, indent=2), encoding='utf-8')
    for pack in inventory['packs']:
        print(f'{pack["plugin"]}: {len(pack["screenshots"])} PNG kit identities')
