# Import/re-import all files in this folder to register AD integrators
import mitsuba as mi

def integrators_variants_cb(old, new):
    if new is None or new.startswith("scalar"):
        return

    import importlib
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


mi.detail.add_variant_callback(integrators_variants_cb)
