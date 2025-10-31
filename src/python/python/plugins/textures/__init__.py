# Import/re-import all files in this folder to register Python plugins
import mitsuba as mi
import sys

if mi.variant() is not None and not mi.variant().startswith('scalar'):
    # List of submodules to import
    submodules = [
        'brightness_contrast',
        'clamp',
        'combine_color',
        'curves',
        'hue_saturation',
        'invert_color',
        'map_range',
        'math',
        'mesh_attribute_adapter',
        'noise',
        'normalmap',
        'rgb_to_bw',
        'separate_rgb',
        'texture_coordinate',
        'udim',
        'uv_wrapper',
        'vector_math'
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
