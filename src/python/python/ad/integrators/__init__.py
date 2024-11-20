# Import/re-import all files in this folder to register AD integrators
import importlib
import mitsuba as mi

if mi.variant() is not None and not mi.variant().startswith('scalar'):
    from . import common
    importlib.reload(common)

    from . import prb_basic
    importlib.reload(prb_basic)

    from . import prb
    importlib.reload(prb)

    from . import prbvolpath
    importlib.reload(prbvolpath)

    from . import direct_projective
    importlib.reload(direct_projective)

    from . import prb_projective
    importlib.reload(prb_projective)

del importlib, mi