import os
import importlib
import glob

for f in glob.glob(os.path.join(os.path.dirname(__file__), "*.py")):
    if not os.path.isfile(f) or f.endswith('__init__.py'):
        continue

    # TODO remove this
    if f.endswith('polvis.py') or f.endswith('tonemap.py'):
        continue

    name = os.path.basename(f)[:-3]
    importlib.import_module(f'mitsuba.python.{name}')

del os, glob, importlib