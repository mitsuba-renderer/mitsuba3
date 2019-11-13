""" Mitsuba Python extension library """

import sys

try:
    import numpy
except ImportError:
    raise ImportError("NumPy could not be found! (To install it, run %s -m pip install numpy)" % sys.executable)

try:
    from . import core, render
except ImportError as e:
    from .config import PYTHON_EXECUTABLE

    if PYTHON_EXECUTABLE != sys.executable:
        extra_msg = ("You're likely trying to use Mitsuba within a Python "
        "binary (%s) that is different from the one for which the native "
        "module was compiled (%s).") % (sys.executable, PYTHON_EXECUTABLE)
    else:
        extra_msg = ""

    exc = ImportError("The 'mitsuba' native modules could not be "
                      "imported. %s" % extra_msg)
    exc.__cause__ = e

    raise exc
