"""
Miscellaneous utility functions for tests (common fixtures,
test decorators, etc).
"""

import os
from functools import wraps
from inspect import getframeinfo, stack, signature, _empty

import pytest
import drjit as dr

def find_resource(fname):
    path = os.path.dirname(os.path.realpath(__file__))
    while True:
        full = os.path.join(path, fname)
        if os.path.exists(full):
            return full
        if path == '' or path == '/':
            raise Exception("find_resource(): could not find \"%s\"" % fname)
        path = os.path.dirname(path)

def fresolver_append_path(func):
    """
    Function decorator that adds the mitsuba project root
    to the mitsuba.FileResolver's search path. This is useful in particular
    for tests that e.g. load scenes, and need to specify paths to resources.

    The file resolver is restored to its previous state once the test's
    execution has finished.
    """

    import mitsuba.scalar_rgb as mi

    par = os.path.dirname

    # Get the path to the source file from which this function is
    # being called.
    # Source: https://stackoverflow.com/a/24439444/3792942
    caller = getframeinfo(stack()[1][0])
    caller_path = par(os.path.realpath(caller.filename))

    # Heuristic to find the project's root directory
    def is_root(path):
        if not path:
            return False
        children = os.listdir(path)
        return ('ext' in children) and ('include' in children) \
               and ('src' in children) and ('resources' in children)
    root_path = caller_path
    while not is_root(root_path) and (par(root_path) != root_path):
        root_path = par(root_path)

    # The @wraps decorator properly sets __name__ and other properties, so that
    # pytest-xdist can keep track of the original test function.
    @wraps(func)
    def f(*args, **kwargs):
        # New file resolver
        thread = mi.Thread.thread()
        fres_old = thread.file_resolver()
        fres = mi.FileResolver(fres_old)

        # Append current test directory and project root to the
        # search path.
        fres.append(caller_path)
        fres.append(root_path)

        thread.set_file_resolver(fres)

        # Run actual function
        res = func(*args, **kwargs)

        # Restore previous file resolver
        thread.set_file_resolver(fres_old)

        return res

    return f


@pytest.fixture
def tmpfile(request, tmpdir_factory):
    """Fixture to create a temporary file"""
    return make_tmpfile(request, tmpdir_factory)

def make_tmpfile(request, tmpdir_factory):
    my_dir = tmpdir_factory.mktemp('tmpdir')
    request.addfinalizer(lambda: my_dir.remove(rec=1))
    path_value = str(my_dir.join('tmpfile'))
    open(path_value, 'a').close()
    return path_value


def check_vectorization(kernel, arg_dims = [], width = 125, atol=1e-6,
                        modes=['llvm', 'cuda', 'llvm_ad', 'cuda_ad']):
    """
    Helper routine which compares evaluations of the vectorized and
    non-vectorized version of a kernel using available variants (e.g. LLVM, CUDA).

    Parameter ``kernel`` (function):
        Function to be evaluated. It's arguments should be annotated if
        ``arg_dims`` is not specified. A kernel can return any drjit supported array
        types (e.g. Float, Vector3f, ...) or a tuple of such arrays.

    Parameter ``arg_dims`` (list(int)):
        Dimensionalities of the function arguments. If not specified, those will be
        deduced from the function argument annotations (if available).

    Parameter ``width`` (int):
       Number of elements to be evaluated at a time for the vectorized call.

    Parameter ``atol`` (float):
       Absolute tolerance for the comparison of the returned values.
    """
    import numpy as np
    import mitsuba as mi

    # Ensure scalar variant is enabled when calling this kernel
    assert mi.variant().startswith('scalar_')

    # List available variants with similar spectral variant
    spectral_variant = mi.variant().replace("scalar", "")
    variants = list(set(mi.variants()) & set([m + spectral_variant for m in modes]))

    if not variants:
        pytest.skip(f"No vectorized variants available")

    # If argument dimensions not provided, look at kernel argument annotations
    if arg_dims == []:
        params = signature(kernel).parameters
        args_types = [params[name].annotation for name in params]
        assert not _empty in args_types, \
               "`kernel` arguments should be annotated, or `arg_dims` should be set."
        arg_dims = [1 if t == float else t.Size for t in args_types]

    # Construct random argument arrays
    rng = np.random.default_rng(seed=0)
    args_np = [rng.random(width) if d == 1 else rng.random((width, d)) for d in arg_dims]

    # Evaluate non-vectorized kernel
    from mitsuba import Float, Vector2f, Vector3f
    types = [Float, Vector2f, Vector3f]
    results_scalar = []
    for i in range(width):
        args = [types[arg_dims[j]-1](args_np[j][i]) for j in range(len(args_np))]
        res = kernel(*args)

        if not type(res) in [list, tuple]:
            res = [res]

        if results_scalar == []:
            results_scalar = [[] for i in range(len(res))]

        for i in range(len(res)):
            results_scalar[i].append(res[i])

    results_scalar = [np.array(res) for res in results_scalar]

    # Evaluate and compare vectorized kernel
    for variant in variants:
        # Set variant
        mi.set_variant(variant)
        types = [mi.Float, mi.Vector2f, mi.Vector3f]

        # Cast arguments
        args = [types[arg_dims[i]-1](args_np[i]) for i in range(len(args_np))]

        # Evaluate vectorized kernel
        results_vec = kernel(*args)
        if not type(results_vec) in [list, tuple]:
            results_vec = [results_vec]

        # Compare results
        for i in range(len(results_scalar)):
            assert dr.allclose(results_vec[i], results_scalar[i], atol=atol)
