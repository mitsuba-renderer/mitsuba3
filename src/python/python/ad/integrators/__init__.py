# Import/re-import all files in this folder to register AD integrators

import os
import importlib
import glob

# Make sure mitsuba.python.util is imported before the integrators
import mitsuba.python.util

do_reload = 'common' in globals()

for f in glob.glob(os.path.join(os.path.dirname(__file__), "*.py")):
    if not os.path.isfile(f) or f.endswith('__init__.py'):
        continue
    name = os.path.basename(f)[:-3]
    if do_reload:
        importlib.reload(globals()[name])
    else:
        importlib.import_module('mitsuba.python.ad.integrators.' + name)

from .common import render

del os, glob, importlib, do_reload
