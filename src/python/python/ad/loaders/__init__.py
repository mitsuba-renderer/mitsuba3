import importlib
import mitsuba as mi

if mi.variant() is not None and not mi.variant().startswith('scalar'):
    from .ray_loader import *
    importlib.reload(ray_loader)
