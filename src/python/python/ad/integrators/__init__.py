# Import/re-import all files in this folder to register AD integrators

import os
import importlib
import glob

# Make sure mitsuba.python.util is imported before the integrators
import mitsuba.util

do_reload = 'common' in globals()

if mitsuba.variant() is not None and not mitsuba.variant().startswith('scalar'):
    # Make sure `common.py` is reloaded before the integrators
    if do_reload:
        importlib.reload(globals()['common'])

    for f in glob.glob(os.path.join(os.path.dirname(__file__), "*.py")):
        if not os.path.isfile(f) or f.endswith('__init__.py'):
            continue

        name = os.path.basename(f)[:-3]
        if do_reload and not name == 'common':
            importlib.reload(globals()[name])
        else:
            importlib.import_module('mitsuba.ad.integrators.' + name)

        del name
    del f

del os, glob, importlib, do_reload
