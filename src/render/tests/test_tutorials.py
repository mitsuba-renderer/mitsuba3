# Script based on https://www.blog.pythonlibrary.org/2018/10/16/testing-jupyter-notebooks/

import pytest
import drjit as dr
import mitsuba as mi
import glob
import re
import os

from os.path import join, realpath, dirname, basename, splitext, exists

has_no_cuda = not dr.has_backend(dr.JitBackend.CUDA)

def variant_not_available(variant_name):
    return variant_name not in mi.variants()

# Skip conditions: list of (condition_function, test_pattern) tuples
# The condition function should return True if the test should be skipped
SKIPPED_TESTS = [
    # Skip tests that are very slow on the LLVM backend
    (has_no_cuda, 'projective_sampling_integrators'),
    (has_no_cuda, 'shape_optimization'),

    # Skip tests that require specific variants
    (variant_not_available('llvm_ad_rgb_polarized'), 'polarizer_optimization'),
    (variant_not_available('llvm_ad_specral_polarized'), 'polarized_rendering'),
]


def run_notebook(notebook_path, tmp_dir=None):
    """
    Execute a jupyter notebook (output notebook in tmp_dir)
    """
    import nbformat
    from nbconvert.preprocessors import ExecutePreprocessor

    for skip_condition, pattern in SKIPPED_TESTS:
        if pattern in notebook_path and skip_condition:
            pytest.skip(f'Skipped: {pattern}')

    nb_name, _ = splitext(basename(notebook_path))
    if tmp_dir is None:
        tmp_dir = dirname(notebook_path)

    with open(notebook_path, encoding='utf-8') as f:
        nb = nbformat.read(f, as_version=4)

    proc = ExecutePreprocessor(timeout=900, kernel_name='python3')
    proc.allow_errors = True

    proc.preprocess(nb, {'metadata': {'path': dirname(notebook_path)}})
    output_path = join(tmp_dir, '{}_all_output.ipynb'.format(nb_name))

    with open(output_path, mode='wt', encoding='utf-8') as f:
        nbformat.write(nb, f)

    return nb


tutorials_dir = realpath(join(dirname(__file__), '../../../tutorials'))
tutorials = glob.glob(join(tutorials_dir, '*', './**/*.ipynb'), recursive=True)


@pytest.mark.slow
@pytest.mark.parametrize("notebook", tutorials)
def test_tutorials(notebook, tmp_path):
    try:
        # Check if various packages needed by the tutorial tests are present
        import nbformat, nbconvert, ipywidgets
    except ImportError:
        pytest.skip('Skipping tutorial testcase (missing dependencies)')

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
