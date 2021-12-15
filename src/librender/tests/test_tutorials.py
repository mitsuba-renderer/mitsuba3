# Script based on https://www.blog.pythonlibrary.org/2018/10/16/testing-jupyter-notebooks/

import os
from os.path import join, realpath, dirname, basename, splitext, exists
import glob
import pytest
import re
import mitsuba


def run_notebook(notebook_path, tmp_dir=None):
    """
    Execute a jupyter notebook (output notebook in tmp_dir)
    """
    import nbformat
    from nbconvert.preprocessors import ExecutePreprocessor

    nb_name, _ = splitext(basename(notebook_path))
    if tmp_dir is None:
        tmp_dir = dirname(notebook_path)

    with open(notebook_path) as f:
        nb = nbformat.read(f, as_version=4)

    # Check the variants required in this notebook are enabled, otherwise skip
    regex = re.compile(r'mitsuba.set_variant\(\'([a-zA-Z_]+)\'\)', re.MULTILINE)
    for c in filter(lambda c: c['cell_type'] == 'code', nb['cells']):
        for v in re.findall(regex, c['source']):
            if v not in mitsuba.variants():
                pytest.skip(f'variant {v} is not enabled')

    proc = ExecutePreprocessor(timeout=600, kernel_name='python3')
    proc.allow_errors = True

    proc.preprocess(nb, {'metadata': {'path': dirname(notebook_path)}})
    output_path = join(tmp_dir, '{}_all_output.ipynb'.format(nb_name))

    with open(output_path, mode='wt') as f:
        nbformat.write(nb, f)

    return nb


tutorials_dir = realpath(join(dirname(__file__), '../../../tutorials'))
tutorials = glob.glob(join(tutorials_dir, '*', '**.ipynb'))


@pytest.mark.slow
@pytest.mark.parametrize("notebook", tutorials)
def test_tutorials(notebook, tmp_path):
    nb = run_notebook(notebook, tmp_path)
    errors = []
    for cell in nb.cells:
        if 'outputs' in cell:
            execution_count = cell['execution_count']
            msg = ''
            for output in cell['outputs']:
                if output.output_type == 'error':
                    ename = output['ename']
                    evalue = output['evalue']
                    msg += f'\nError in cell #{execution_count}: {ename}: {evalue}\n'
                    msg += '\n'.join(output['traceback'])
            if msg != '':
                pytest.fail(msg)
