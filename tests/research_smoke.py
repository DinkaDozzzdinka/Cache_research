import csv
import subprocess
import sys
import tempfile
from pathlib import Path


binary, simulator, script = map(Path, sys.argv[1:4])
with tempfile.TemporaryDirectory(prefix='cache-research-test-') as directory:
    output = Path(directory) / 'first'
    options = ['--requests', '80', '--max-levels', '1', '--capacities', '0,3',
               '--train-seeds', '1', '--validation-seeds', '2']
    subprocess.run([str(binary), '--output', str(output), *options], capture_output=True, check=True)
    with (output / 'raw.csv').open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 120
    for item in rows:
        assert 0 <= int(item['hits']) <= int(item['ideal_hits']) <= 80
        if item['capacity'] == '0':
            assert item['hits'] == '0'
    data = (output / 'inputs/train_hot_set.in').read_text().split()
    data[0] = '3'
    check = subprocess.run([str(simulator), str(output / 'configs/ARC.cfg'), '--compare'],
                           input=' '.join(data), text=True, capture_output=True, check=True)
    values = dict(line.split(': ') for line in check.stdout.splitlines())
    expected = next(item for item in rows if item['dataset'] == 'train_hot_set'
                    and item['capacity'] == '3' and item['policies'] == 'ARC')
    assert values['hierarchy_hits'] == expected['hits']
    assert values['ideal_hits'] == expected['ideal_hits']
    second = Path(directory) / 'second'
    subprocess.run([str(binary), '--output', str(second), *options], capture_output=True, check=True)
    assert (output / 'raw.csv').read_bytes() == (second / 'raw.csv').read_bytes()
    subprocess.run([sys.executable, str(script), '--binary', str(binary), '--output', str(output),
                    '--skip-run', '--no-plots'], capture_output=True, check=True)
    assert (output / 'report.md').exists()
    assert (output / 'selected.csv').exists()
    for invalid in [['--requests', '0'], ['--max-levels', '0'], ['--capacities', '0,0'],
                    ['--train-seeds', '1', '--validation-seeds', '1'], ['--requests', '-1'],
                    ['--output'], ['--unknown', 'value']]:
        failed = subprocess.run([str(binary), '--output', str(Path(directory) / 'invalid'), *invalid],
                                capture_output=True)
        assert failed.returncode != 0, invalid
print('PASS: research CLI, replay, deterministic CSV, report generation and invalid options')
