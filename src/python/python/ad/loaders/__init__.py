import importlib
import mitsuba as mi

if mi.variant() is not None and not mi.variant().startswith('scalar'):
    from . import ray_loader
    importlib.reload(ray_loader)
    from .ray_loader import *
