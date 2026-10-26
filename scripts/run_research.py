#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import platform
import shutil
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LABELS = {
    'sequential_unique': 'Новые ключи без повторений',
    'hot_set': 'Небольшой горячий набор',
    'cycle': 'Циклический рабочий набор',
    'hot_scan': 'Горячий набор и сканирование',
    'phase_change': 'Смена горячего набора',
    'uniform': 'Равномерные случайные запросы',
}
SINGLES = ['LRU', 'LFU', '2Q', 'ARC', 'LIRS']


def read_csv(path):
    with path.open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def write_csv(path, rows):
    with path.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def aggregate(rows):
    groups = defaultdict(list)
    seen = set()
    for row in rows:
        key = (row['dataset'], int(row['capacity']), row['policies'])
        if key in seen:
            raise ValueError('Duplicate experiment: ' + str(key))
        seen.add(key)
        count, hits, ideal = int(row['requests']), int(row['hits']), int(row['ideal_hits'])
        if count <= 0 or not 0 <= hits <= ideal <= count:
            raise ValueError('Invalid hit counts: ' + str(key))
        cell = (row['split'], row['workload'], int(row['capacity']), row['policies'])
        groups[cell].append((hits, ideal, count))
    result = {}
    for key, values in groups.items():
        ratios = [hits / count for hits, ideal, count in values]
        ideals = [ideal / count for hits, ideal, count in values]
        result[key] = {
            'runs': len(values),
            'mean_hit_ratio': statistics.fmean(ratios),
            'sd_hit_ratio': statistics.stdev(ratios) if len(ratios) > 1 else 0.0,
            'mean_ideal_ratio': statistics.fmean(ideals),
            'gap_pp': 100 * (statistics.fmean(ideals) - statistics.fmean(ratios)),
        }
    return result


def choose(cells, workload, capacity, singles_only=False):
    candidates = []
    for (split, name, size, policy), stats in cells.items():
        if split == 'train' and name == workload and size == capacity:
            levels = len(policy.split('|'))
            if not singles_only or levels == 1:
                candidates.append((-stats['mean_hit_ratio'], levels, policy))
    if not candidates:
        raise ValueError('Missing training candidates')
    return min(candidates)[2]


def summaries(cells):
    pairs = sorted({(workload, capacity) for split, workload, capacity, policy in cells if split == 'train'})
    policies = sorted({policy for split, workload, capacity, policy in cells})
    rankings = []
    for policy in policies:
        training = [cells[('train', workload, capacity, policy)] for workload, capacity in pairs]
        validation = [cells[('validation', workload, capacity, policy)] for workload, capacity in pairs]
        rankings.append({
            'policies': policy,
            'levels': len(policy.split('|')),
            'train_score': statistics.fmean(row['mean_hit_ratio'] for row in training),
            'validation_score': statistics.fmean(row['mean_hit_ratio'] for row in validation),
            'train_gap_pp': statistics.fmean(row['gap_pp'] for row in training),
            'validation_gap_pp': statistics.fmean(row['gap_pp'] for row in validation),
        })
    order = sorted((-row['train_score'], row['levels'], row['policies']) for row in rankings)
    by_name = {row['policies']: row for row in rankings}
    rankings = [by_name[name] for score, levels, name in order]
    selected = []
    for workload, capacity in pairs:
        winner = choose(cells, workload, capacity)
        single = choose(cells, workload, capacity, singles_only=True)
        training = cells[('train', workload, capacity, winner)]
        validation = cells[('validation', workload, capacity, winner)]
        single_validation = cells[('validation', workload, capacity, single)]
        selected.append({
            'workload': workload, 'capacity': capacity, 'selected': winner,
            'levels': len(winner.split('|')), 'train_hit_ratio': training['mean_hit_ratio'],
            'validation_hit_ratio': validation['mean_hit_ratio'],
            'validation_ideal_ratio': validation['mean_ideal_ratio'],
            'validation_gap_pp': validation['gap_pp'], 'best_single': single,
            'best_single_validation_ratio': single_validation['mean_hit_ratio'],
            'delta_validation_pp': 100 * (validation['mean_hit_ratio'] - single_validation['mean_hit_ratio']),
        })
    return rankings, selected


def validate_grid(rows, datasets, configurations):
    expected = {(dataset['dataset'], row['capacity'], row['policies'])
                for dataset in datasets for row in configurations}
    actual = {(row['dataset'], row['capacity'], row['policies']) for row in rows}
    if len(rows) != len(expected) or actual != expected:
        raise ValueError('Raw CSV is not a complete experiment grid')
    data_by_name = {row['dataset']: row for row in datasets}
    config_by_key = {(row['capacity'], row['policies']): row for row in configurations}
    ideals = {}
    for row in rows:
        dataset = data_by_name[row['dataset']]
        config = config_by_key[(row['capacity'], row['policies'])]
        if any(row[name] != dataset[name] for name in ['workload', 'split', 'seed', 'requests']):
            raise ValueError('Dataset metadata mismatch')
        if row['levels'] != config['levels'] or row['level_capacities'] != config['level_capacities']:
            raise ValueError('Configuration metadata mismatch')
        count, hits, ideal = int(row['requests']), int(row['hits']), int(row['ideal_hits'])
        if count <= 0 or not 0 <= hits <= ideal <= count:
            raise ValueError('Invalid raw hit counts')
        expected_metrics = {
            'hit_ratio': hits / count, 'ideal_hit_ratio': ideal / count,
            'hit_gap': ideal - hits, 'gap_pp': 100 * (ideal - hits) / count,
        }
        if any(abs(float(row[name]) - value) > 1e-8 for name, value in expected_metrics.items()):
            raise ValueError('Derived metric mismatch')
        key = (row['dataset'], row['capacity'])
        if key in ideals and ideals[key] != ideal:
            raise ValueError('Policies do not share the same ideal baseline')
        ideals[key] = ideal


def export_aggregates(output, cells):
    rows = []
    for (split, workload, capacity, policy), stats in sorted(cells.items()):
        rows.append({'split': split, 'workload': workload, 'capacity': capacity,
                     'policies': policy, **stats})
    write_csv(output / 'aggregate.csv', rows)


def plot_body(data, workload, capacities):
    title = workload.replace('_', ' ')
    commands = [
        "set encoding utf8", "set grid ytics", "set yrange [0:100]",
        "set xlabel 'Total capacity (pages)'", "set ylabel 'Hit ratio (%)'",
        'set xtics (' + ','.join(f'"{value}" {value}' for value in capacities) + ')',
        f'set xrange [{min(capacities) - 1}:{max(capacities) + 1}]',
        'set key outside bottom center horizontal maxrows 3',
        f"set multiplot layout 1,2 title '{title}'",
    ]
    for split in ['train', 'validation']:
        commands.append(f"set title '{split}'")
        path = f'plot_data/{workload}_{split}.tsv'
        commands.append(
            f"plot for [col=2:6] '{path}' using 1:col with linespoints lw 2 title columnhead(col), "
            f"'{path}' using 1:7 with linespoints lw 3 dt 2 title 'Selected on train', "
            f"'{path}' using 1:8 with linespoints lw 2 dt 3 lc rgb 'black' title 'Ideal'")
    commands.extend(['unset multiplot', 'unset output'])
    return '\n'.join(commands)


def render_plots(output, cells, rankings, selected):
    executable = shutil.which('gnuplot')
    if executable is None:
        raise RuntimeError('gnuplot is required for figures; install it or use --no-plots')
    (output / 'plots').mkdir(exist_ok=True)
    (output / 'plot_data').mkdir(exist_ok=True)
    selected_index = {(row['workload'], row['capacity']): row['selected'] for row in selected}
    capacities = sorted({row['capacity'] for row in selected})
    for workload in LABELS:
        for split in ['train', 'validation']:
            path = output / 'plot_data' / f'{workload}_{split}.tsv'
            with path.open('w') as stream:
                stream.write('Capacity\tLRU\tLFU\t2Q\tARC\tLIRS\tSelected\tIdeal\n')
                for capacity in capacities:
                    winner = selected_index[(workload, capacity)]
                    values = [100 * cells[(split, workload, capacity, policy)]['mean_hit_ratio']
                              for policy in SINGLES + [winner]]
                    ideal = 100 * cells[(split, workload, capacity, winner)]['mean_ideal_ratio']
                    stream.write('\t'.join(map(str, [capacity, *values, ideal])) + '\n')
        script = []
        for terminal, extension in [('svg size 1300,600 dynamic', 'svg'), ('pngcairo size 1300,600', 'png')]:
            script.extend([f'set terminal {terminal}', f"set output 'plots/{workload}.{extension}'",
                           plot_body(output, workload, capacities)])
        script_path = output / 'plots' / f'{workload}.gp'
        script_path.write_text('\n'.join(script) + '\n')
        subprocess.run([executable, str(script_path.resolve())], cwd=output, check=True)
    path = output / 'plot_data/global_top10.tsv'
    with path.open('w') as stream:
        stream.write('Configuration\tTrain\tValidation\n')
        for row in rankings[:10]:
            stream.write(f"{row['policies']}\t{100 * row['train_score']}\t{100 * row['validation_score']}\n")
    last = min(10, len(rankings)) - 0.5
    body = f"""set xrange [0:100]
set yrange [{last}:-0.5]
set grid xtics
set xlabel 'Macro-average hit ratio (%)'
unset ylabel
set ytics nomirror
set lmargin 24
set rmargin 4
set bmargin 4
set title 'Top 10 configurations selected by training score'
set key outside top center horizontal
plot 'plot_data/global_top10.tsv' using 2:0:ytic(1) with points pt 7 ps 1.6 title 'Train', '' using 3:0 with points pt 5 ps 1.6 title 'Validation'
unset output
"""
    script = []
    for terminal, extension in [('svg size 1200,650 dynamic', 'svg'), ('pngcairo size 1200,650', 'png')]:
        script.extend([f'set terminal {terminal}', f"set output 'plots/global_top10.{extension}'", body])
    script_path = output / 'plots/global_top10.gp'
    script_path.write_text('\n'.join(script))
    subprocess.run([executable, str(script_path.resolve())], cwd=output, check=True)


def percent(value):
    return f'{100 * value:.2f}%'.replace('.', ',')


def report(output, rows, datasets, cells, rankings, selected, plots):
    best = rankings[0]
    best_single = next(row for row in rankings if row['levels'] == 1)
    multi_wins = sum(row['levels'] > 1 for row in selected)
    better = sum(row['delta_validation_pp'] > 1e-9 for row in selected)
    worse = sum(row['delta_validation_pp'] < -1e-9 for row in selected)
    lines = [
        '# Исследование политик кеширования', '',
        f"Выполнено **{len(rows)} прогонов**, {len(rankings)} конфигураций, {len(datasets)} последовательностей.",
        'Основная метрика — доля попаданий. Измерения времени выполнения не проводились.', '',
        '## Условия эксперимента', '',
        '- Политики: LRU, LFU, 2Q, ARC, LIRS; от одного до трёх уровней, порядок учитывается.',
        '- Каждый прогон начинается с пустых кешей; первоначальные загрузки входят в статистику.',
        '- Сравнивается одинаковая общая ёмкость в страницах. Она делится поровну, остаток получают верхние уровни.',
        '- Одна и та же последовательность подаётся всем кандидатам. Идеальный результат рассчитывается один раз на последовательность и ёмкость.',
        '- Параметры каждого набора и сохранённые входы перечислены в [datasets.csv](datasets.csv).', '',
        'Train — набор для выбора конфигурации, validation — отдельная проверка выбранного решения. Это подбор конфигураций, а не обучение нейросети.',
        'На validation не перевыбираем победителя. Для случайной нагрузки используются другие seed, для четырёх детерминированных нагрузок — другие параметры. Уникальные ключи остаются контрольным сценарием.',
        'Сначала усредняем seed внутри каждой нагрузки и ёмкости, затем считаем обычное среднее по этим ячейкам. Так случайная нагрузка не получает больший вес из-за числа seed.',
        'При равном результате предпочитаем меньше уровней, затем лексикографически первое название.', '',
        '## Общий рейтинг по train', '',
        f"Первое место: **{best['policies']}**, train {percent(best['train_score'])}, validation {percent(best['validation_score'])}.",
        f"Лучшая одноуровневая конфигурация по train: **{best_single['policies']}**, validation {percent(best_single['validation_score'])}.",
        'Эти оценки — средние по выбранным сценариям и ёмкостям, а не универсальная характеристика алгоритма.', '',
        '| Конфигурация | Уровни | Train | Validation | Отставание от идеального на validation, п.п. |',
        '|---|---:|---:|---:|---:|',
    ]
    for row in rankings[:10]:
        name = row['policies'].replace('|', ' → ')
        lines.append(f"| {name} | {row['levels']} | {percent(row['train_score'])} | {percent(row['validation_score'])} | {row['validation_gap_pp']:.2f} |")
    if plots:
        lines.extend(['', '![Общий рейтинг](plots/global_top10.svg)'])
    lines.extend([
        '', '## Проверка выбранных конфигураций на новых данных', '',
        f'Из {len(selected)} пар «нагрузка, ёмкость» на train выбраны многоуровневые решения в {multi_wins} случаях.',
        f'На validation выбранное решение лучше отдельно выбранного одноуровневого в {better} случаях, хуже в {worse}; в остальных — равенство.',
        'В таблице «Один уровень» тоже выбран только по train. Δ — разница validation-результатов в процентных пунктах.', '',
        '| Нагрузка | C | Выбрано на train | Validation | Идеальный | Один уровень | Δ, п.п. |',
        '|---|---:|---|---:|---:|---|---:|',
    ])
    for row in selected:
        name = row['selected'].replace('|', ' → ')
        lines.append(f"| {LABELS[row['workload']]} | {row['capacity']} | {name} | {percent(row['validation_hit_ratio'])} | {percent(row['validation_ideal_ratio'])} | {row['best_single']} | {row['delta_validation_pp']:+.2f} |")
    lines.extend(['', '## Графики по нагрузкам', ''])
    for workload, label in LABELS.items():
        lines.extend([f'### {label}', ''])
        if plots:
            lines.extend([f'![{label}](plots/{workload}.svg)', ''])
        lines.append('Левая панель — train, правая — validation. Selected on train — фиксированный выбор для данной нагрузки и ёмкости, сделанный на train.')
        lines.append('')
    lines.extend([
        '## Наблюдения по результатам', '',
        f"Одноуровневый LIRS получил максимальный средний train-результат среди перебранных кандидатов; следующий в рейтинге — {rankings[1]['policies'].replace('|', ' → ')}." if rankings[0]['policies'] == 'LIRS' and len(rankings) > 1 else f"Первое место по train занял {best['policies'].replace('|', ' → ')}.",
        'На циклической нагрузке защищённые страницы LIRS сохраняют попадания, тогда как LRU при рабочем наборе больше ёмкости последовательно вытесняет нужные страницы.',
        'На горячем наборе многие политики равны; название выбранного победителя здесь часто определяется правилом разрешения равенства и не доказывает превосходства политики.',
    ])
    phase_capacities = sorted({capacity for split, workload, capacity, policy in cells if workload == 'phase_change'})
    if phase_capacities:
        capacity = phase_capacities[0]
        selected_phase = next(row for row in selected if row['workload'] == 'phase_change' and row['capacity'] == capacity)
        lines.append(f"Смена фаз показывает риск переноса выбора: при C={capacity} выбранный на train {selected_phase['selected']} имеет {percent(selected_phase['train_hit_ratio'])} на train и {percent(selected_phase['validation_hit_ratio'])} на validation. На validation изменился размер рабочего набора; сравнивать эти два процента как измерение одной и той же нагрузки нельзя.")
    for row in selected:
        if row['levels'] > 1:
            lines.append(f"Для {row['workload']}, C={row['capacity']} сочетание {row['selected'].replace('|', ' → ')} изменило validation-результат относительно выбранного одноуровневого решения на {row['delta_validation_pp']:+.3f} п.п.")
    lines.extend([
        'Малые преимущества на двух validation-seed следует считать наблюдением этого эксперимента: статистическая значимость и универсальное превосходство не установлены.', '',
        '## Интерпретация и границы выводов', '',
        '- Уникальные ключи дают нулевую долю попаданий и служат проверкой методики.',
        '- Маленький набор, помещающийся в кеш, после первоначальных загрузок обслуживается без вытеснений.',
        '- Цикл, превышающий ёмкость, проверяет устойчивость к циклическому вытеснению. LIRS может сохранять часть защищённого набора.',
        '- Сканирование проверяет сохранение горячих страниц, смена фаз — способность перестроиться.',
        '- Наш LFU не старит частоты; накопленные частоты старого набора могут мешать после смены фазы.',
        '- В эксклюзивной иерархии найденная ниже L1 страница сразу продвигается. LFU нижних уровней не получает touch и начинает вставки с частоты 1, поэтому там решение часто определяется LRU-порядком равных частот.',
        '- У LIRS HIR-резерв равен max(1, C/100), стек ограничен 2C; у 2Q Kin=max(1,C/4), Kout=max(1,C/2) при C>0. Ghost-история не входит в ёмкость реальных страниц.',
        '- Сравниваются слоты страниц, а не одинаковое число байтов: объём метаданных у политик различается.',
        '- Случайные результаты — средние нескольких seed. sd_hit_ratio в aggregate.csv — выборочное стандартное отклонение между seed, а не доверительный интервал.',
        '- Полный перебор выполнен только внутри заявленного пространства: пять политик, максимум три уровня, равное деление ёмкости. Конфигурации с другим распределением ёмкости и другими нагрузками могут дать другой результат.',
        '- Универсального победителя эти эксперименты не устанавливают. Дополнительные уровни делят ту же ёмкость и не гарантируют улучшения.', '',
        '## Данные и повторение', '',
        '- [raw.csv](raw.csv) — все отдельные прогоны.',
        '- [aggregate.csv](aggregate.csv) — средние и стандартные отклонения по seed.',
        '- [ranking.csv](ranking.csv) — общий рейтинг, упорядоченный по train.',
        '- [selected.csv](selected.csv) — выбранные решения и проверка на validation.',
        '- [configurations.csv](configurations.csv) — политики и фактическое распределение ёмкости.',
        '- [manifest.json](manifest.json) — команда, версии и контрольные суммы.', '',
        'Из корня проекта:', '',
        '```bash', 'python3 scripts/run_research.py', '```', '',
        'Алгоритмы основаны на [ARC, Figure 4](https://www.usenix.org/legacy/events/fast03/tech/full_papers/megiddo/megiddo.pdf),',
        '[LIRS, разделы 3 и 5.2](https://xiaodongzhang1911.github.io/Zhang-papers/TR-05-11.pdf)',
        'и [полной версии 2Q](https://www.vldb.org/conf/1994/P439.PDF).', '',
    ])
    (output / 'report.md').write_text('\n'.join(lines), encoding='utf-8')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description='Run and explain reproducible cache experiments')
    parser.add_argument('--binary', default='build/cache_research')
    parser.add_argument('--output', default='results/full')
    parser.add_argument('--requests', type=int, default=10000)
    parser.add_argument('--max-levels', type=int, default=3)
    parser.add_argument('--capacities', default='8,16,32')
    parser.add_argument('--train-seeds', default='42,43,44')
    parser.add_argument('--validation-seeds', default='101,102')
    parser.add_argument('--skip-run', action='store_true')
    parser.add_argument('--no-plots', action='store_true')
    args = parser.parse_args()
    output = (ROOT / args.output).resolve()
    binary = (ROOT / args.binary).resolve()
    command = [str(binary), '--output', str(output), '--requests', str(args.requests),
               '--max-levels', str(args.max_levels), '--capacities', args.capacities,
               '--train-seeds', args.train_seeds, '--validation-seeds', args.validation_seeds]
    sources = sorted(list((ROOT / 'include').glob('*.hpp')) + list((ROOT / 'src').glob('*.cpp')) +
                     list((ROOT / 'scripts').glob('*.py')) + [ROOT / 'CMakeLists.txt'])
    source_hashes = {str(path.relative_to(ROOT)): sha256(path) for path in sources}
    binary_hash = sha256(binary) if binary.exists() else None
    previous_manifest = None
    if not args.skip_run:
        subprocess.run(command, cwd=ROOT, check=True)
    elif (output / 'manifest.json').exists():
        previous_manifest = json.loads((output / 'manifest.json').read_text())
        command = previous_manifest['runner_command']
        if sha256(output / 'raw.csv') != previous_manifest['generated_sha256']['raw.csv']:
            raise ValueError('Raw data changed since the recorded run; rerun the experiment')
        source_hashes = previous_manifest['source_sha256']
        binary_hash = previous_manifest['binary_sha256']
    else:
        command = None
    rows = read_csv(output / 'raw.csv')
    datasets = read_csv(output / 'datasets.csv')
    configurations = read_csv(output / 'configurations.csv')
    validate_grid(rows, datasets, configurations)
    cells = aggregate(rows)
    rankings, selected = summaries(cells)
    export_aggregates(output, cells)
    write_csv(output / 'ranking.csv', rankings)
    write_csv(output / 'selected.csv', selected)
    if not args.no_plots:
        render_plots(output, cells, rankings, selected)
    report(output, rows, datasets, cells, rankings, selected, not args.no_plots)
    generated = sorted(path for path in output.rglob('*') if path.is_file() and path.name != 'manifest.json')
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                              capture_output=True, check=True).stdout.strip()
    manifest = {
        'runner_command': command, 'git_revision': revision, 'python_version': platform.python_version(),
        'run_parameters': (output / 'run_parameters.txt').read_text(),
        'source_sha256': source_hashes,
        'report_source_sha256': {str(path.relative_to(ROOT)): sha256(path) for path in sources},
        'generated_sha256': {str(path.relative_to(output)): sha256(path) for path in generated},
        'binary_sha256': binary_hash,
        'gnuplot_version': subprocess.run(['gnuplot', '--version'], text=True, capture_output=True,
                                         check=True).stdout.strip() if not args.no_plots else None,
        'raw_rows': len(rows), 'configurations': len(rankings), 'datasets': len(datasets),
        'selection_rule': 'Use train only; prefer fewer levels on a tie, then lexicographic name.',
        'scope': 'Hit ratios; no runtime claims. Validation winners are not reselected.',
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print(f"Saved {len(rows)} rows; best train configuration: {rankings[0]['policies']}")
    print(output / 'report.md')


if __name__ == '__main__':
    main()
