import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import run_research as report


def row(dataset, split, policy, hits, workload='uniform', capacity='8'):
    return {'dataset': dataset, 'split': split, 'workload': workload, 'capacity': capacity,
            'policies': policy, 'requests': '100', 'hits': str(hits), 'ideal_hits': '90'}


class ReportTest(unittest.TestCase):
    def test_aggregates_seeds_and_sample_deviation(self):
        cells = report.aggregate([row('a', 'train', 'LRU', 20), row('b', 'train', 'LRU', 40)])
        cell = cells[('train', 'uniform', 8, 'LRU')]
        self.assertEqual(cell['runs'], 2)
        self.assertAlmostEqual(cell['mean_hit_ratio'], 0.3)
        self.assertAlmostEqual(cell['sd_hit_ratio'], 0.02 ** 0.5)

    def test_rejects_duplicates_and_impossible_hits(self):
        item = row('a', 'train', 'LRU', 20)
        with self.assertRaises(ValueError):
            report.aggregate([item, item])
        with self.assertRaises(ValueError):
            report.aggregate([row('a', 'train', 'LRU', 99)])

    def test_validation_does_not_select_winner(self):
        cells = report.aggregate([
            row('a', 'train', 'LRU', 80), row('a', 'train', 'ARC', 70),
            row('b', 'validation', 'LRU', 10), row('b', 'validation', 'ARC', 90),
        ])
        rankings, selected = report.summaries(cells)
        self.assertEqual(rankings[0]['policies'], 'LRU')
        self.assertEqual(selected[0]['selected'], 'LRU')
        self.assertAlmostEqual(selected[0]['validation_hit_ratio'], 0.1)

    def test_tie_prefers_fewer_levels(self):
        cells = report.aggregate([
            row('a', 'train', 'ARC|ARC', 80), row('a', 'train', 'LRU', 80),
        ])
        self.assertEqual(report.choose(cells, 'uniform', 8), 'LRU')

    def test_macro_average_does_not_overweight_many_seeds(self):
        rows = []
        for split in ['train', 'validation']:
            rows.append(row(split + '_fixed', split, 'LRU', 0, 'cycle'))
            for seed in range(3):
                rows.append(row(split + str(seed), split, 'LRU', 90))
        rankings, selected = report.summaries(report.aggregate(rows))
        self.assertAlmostEqual(rankings[0]['train_score'], 0.45)
        self.assertEqual(len(selected), 2)


if __name__ == '__main__':
    unittest.main()
