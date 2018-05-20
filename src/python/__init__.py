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

    raise ImportError("The 'mitsuba' native modules could not be imported. You're likely trying "
                      "to use Mitsuba within a Python binary (%s) that is different from the "
                      " one for which the native module was compiled (%s)."
                      % (sys.executable, PYTHON_EXECUTABLE))
