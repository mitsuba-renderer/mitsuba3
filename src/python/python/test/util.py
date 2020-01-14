"""
Miscellaneous utility functions for tests (common fixtures,
test decorators, etc).
"""

import os
from functools import wraps
from inspect import getframeinfo, stack

import pytest
import mitsuba

def fresolver_append_path(func):
    """Function decorator that adds the mitsuba project root
    to the FileResolver's search path. This is useful in particular
    for tests that e.g. load scenes, and need to specify paths to resources.

    The file resolver is restored to its previous state once the test's
    execution has finished.
    """
    if mitsuba.variant() == None:
        mitsuba.set_variant('scalar_rgb')

    from mitsuba.core import Thread, FileResolver
    par = os.path.dirname

    # Get the path to the source file from which this function is
    # being called.
    # Source: https://stackoverflow.com/a/24439444/3792942
    caller = getframeinfo(stack()[1][0])
    caller_path = par(caller.filename)

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
        thread = Thread.thread()
        fres_old = thread.file_resolver()
        fres = FileResolver(fres_old)

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
