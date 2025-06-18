# Import/re-import all files in this folder to register AD integrators
import mitsuba as mi
import sys

if mi.variant() is not None and not mi.variant().startswith('scalar'):
    # List of submodules to import
    submodules = [
        'common',
        'prb_basic',
        'prb',
        'prbvolpath',
        'direct_projective',
        'prb_projective',
        'volprim_rf_basic'
    ]

    # Are we importing the submodules for the first time or reloading them?
    reload = submodules[0] in globals()

    import importlib
    module, name = None, None

    for name in submodules:
        module = importlib.import_module(f'.{name}', package=__name__)
        if reload:
            importlib.reload(module)

        # Make the submodule available at package level
        globals()[name] = module

    del importlib, name, submodules, module, reload

del mi, sys
