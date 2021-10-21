"""Import/re-import all files in this folder (e.g. register AD integrators)"""
from os.path import dirname, basename, isfile, join
import glob
from importlib import import_module as _import, reload as _reload

files = glob.glob(join(dirname(__file__), "*.py"))
modules = [ basename(f)[:-3] for f in files if isfile(f) and not f.endswith('__init__.py')]

should_reload = 'integrator' in globals()
for m in modules:
    if should_reload:
        _reload(globals()[m])
    else:
        _import('mitsuba.python.ad.integrators.' + m)

from .integrator import render