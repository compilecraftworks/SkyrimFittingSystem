"""Behavior tests for PNG identities, family pooling and reviewed exceptions."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('add_rules', Path(__file__).parents[1] / 'scripts/generate-add-screenshot-grouping.py')
rules = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rules)


class ScreenshotGroupingTests(unittest.TestCase):
    def compile(self, photos, armors, overrides=None):
        inventory = {'packs': [{'plugin': 'ADD 01.esp', 'pluginPath': 'fixture.esp',
                     'screenshots': [{'name': name, 'file': name + '.png', 'sha256': 'fixture'} for name in photos]}]}
        corpus = {'plugins': [{'name': 'ADD 01.esp', 'path': 'fixture.esp', 'armors': armors}]}
        return rules.compile_rules(inventory, corpus, overrides or {})

    def armor(self, id, name, slots=4, editor=None):
        return dict(formID=id, name=name, slots=slots, editorID=editor or f'Outfit{id}')

    def test_photo_numbers_are_not_colors_or_component_numbers(self):
        armors = [self.armor(1, 'Example Body 01'), self.armor(2, 'Example Body 09'),
                  self.armor(3, 'Example Gloves 09', 8), self.armor(4, 'Example Boots', 128)]
        result = self.compile(['Example 01', 'Example 02'], armors)
        self.assertFalse(result['unresolved'])
        self.assertEqual([len(g['members']) for g in result['groups']], [4, 4])
        self.assertEqual([g['name'] for g in result['groups']], ['Example 01', 'Example 02'])

    def test_accessory_family_does_not_require_body_slot(self):
        result = self.compile(['Jewels 02'], [self.armor(1, 'Jewels Ring', 64)])
        self.assertTrue(result['groups'][0]['members'][0]['anchor'])

    def test_reviewed_design_selectors_keep_every_color(self):
        result = self.compile(['Malibu 06'], [self.armor(1, 'Malibu 06 Red'),
                             self.armor(2, 'Malibu 06 Blue'), self.armor(3, 'Malibu 07')],
                             {'photos': {'ADD 01.esp|Malibu 06': {'include': [r'\|Malibu 06\b']}}})
        self.assertEqual([m['localFormID'] for m in result['groups'][0]['members']], [1, 2])

    def test_similar_color_words_do_not_cross_outfit_boundary(self):
        self.assertEqual(rules.score_root('Black&White', self.armor(1, 'Black Cat Hair White')), 0)
        self.assertGreater(rules.score_root('Black&White', self.armor(1, 'Black&White Gloves Black')), 0)

    def test_unresolved_names_do_not_get_arbitrary_parts(self):
        result = self.compile(['Missing Outfit 01'], [self.armor(1, 'Another Body')])
        self.assertEqual(len(result['unresolved']), 1)
        self.assertFalse(result['groups'][0]['members'])

    def test_explicit_editor_alias_survives_translation(self):
        result = self.compile(['Torn 02'], [self.armor(1, '번역된 상의', editor='00TornUpper1')],
                              {'aliases': {'ADD 01.esp|Torn': ['00Torn']}})
        self.assertEqual(len(result['groups'][0]['members']), 1)


if __name__ == '__main__':
    unittest.main()
