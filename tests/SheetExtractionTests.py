"""Regression cases for workbook boundaries; no workbook or external package needed."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('sheet_rules', Path(__file__).parents[1] / 'scripts/generate-sheet-grouping.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def compile_rows(rows, merges=()):
    inventory = dict(sha256='fixture', sheets=[dict(name='Fixture', maxRow=max(rows), merges=list(merges),
                     rows=[[dict(cell=col + str(row), value=value) for col, value in cells.items()] for row, cells in rows.items()])])
    return module.compile_families(inventory)['families']


class SheetExtractionTests(unittest.TestCase):
    def test_merged_body_label_links_aliases_without_merging_entire_plugin(self):
        families = compile_rows({2: dict(A='Pack.esp', B='Shared Set', C='Red Shared'),
                                 3: dict(C='Blue Shared'), 4: dict(B='Other Set', C='Other')},
                                ['A2:A4', 'B2:B3'])
        self.assertEqual(len(families), 2)
        self.assertEqual(next(f['aliases'] for f in families if 'Shared Set' in f['aliases']),
                         ['Blue Shared', 'Red Shared', 'Shared Set'])

    def test_blank_plugin_does_not_forward_fill(self):
        families = compile_rows({2: dict(A='Pack.esp', B='Alpha Set', C='Alpha'),
                                 3: dict(B='Do Not Include', C='Unknown')})
        self.assertEqual(len(families), 1)
        self.assertNotIn('Unknown', families[0]['aliases'])

    def test_generic_merged_body_label_cannot_union_outfits(self):
        families = compile_rows({2: dict(A='Pack.esp', B='남성용', C='First Set'),
                                 3: dict(C='Second Set')}, ['A2:A3', 'B2:B3'])
        self.assertEqual(len(families), 2)
        self.assertTrue(all('남성용' not in f['aliases'] for f in families))

    def test_plugin_and_example_boundaries(self):
        families = compile_rows({2: dict(A='One.esp', B='Alpha Set', C='Alpha'),
                                 3: dict(A='Two.esp', B='Alpha Set', C='Alpha'),
                                 4: dict(A='One.esp', B='Wrong Alias', C='(Alpha 착용례)')})
        self.assertEqual({f['plugin'] for f in families}, {'one.esp', 'two.esp'})
        self.assertTrue(all('Wrong Alias' not in f['aliases'] for f in families))

    def test_part_rows_need_a_corroborated_multiword_root(self):
        families = compile_rows({2: dict(A='Pack.esp', C='Honoka Cosplay Armor'),
                                 3: dict(C='Honoka Cosplay Boots'),
                                 4: dict(C='TGO 의상'), 5: dict(C='TGO 장갑')}, ['A2:A5'])
        self.assertEqual(len(families), 3)
        self.assertTrue(any(f['name'] == 'honoka cosplay' for f in families))


if __name__ == '__main__':
    unittest.main()
