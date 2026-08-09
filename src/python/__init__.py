""" Mitsuba Python extension library """

import sys as _sys
import os as _os
import drjit as _dr
import logging

if _sys.version_info < (3, 9):
    raise ImportError("Mitsuba requires Python 3.9 or greater.")

mi_dir = _os.path.dirname(_os.path.realpath(__file__))
drjit_expected_loc = _os.path.realpath(_os.path.join(mi_dir, "..", "drjit"))
drjit_loc = _os.path.realpath(_dr.__path__[0])
if _os.name != 'nt' and drjit_expected_loc != drjit_loc:
    logging.warning("The `mitsuba` package relies on `drjit` and needs it "
                    "to be installed at a specific location. Currently, "
                    "`drjit` is located at \"%s\" when it is expected to be "
                    "at \"%s\". This can happen when both packages are not "
                    "installed in the same Python environment. You will very "
                    "likely experience linking issues if you do not fix this."
                    % (drjit_loc, drjit_expected_loc))
del mi_dir, drjit_expected_loc, drjit_loc

from .config import DRJIT_VERSION_REQUIREMENT
if _dr.__version__ != DRJIT_VERSION_REQUIREMENT:
    raise ImportError("You are using an incompatible version of `drjit`. "
                      "Only version \"%s\" is guaranteed to be compatible with "
                      "your current Mitsuba installation. Please update your "
                      "Python packages for `drjit` and/or `mitsuba`."
                      % (DRJIT_VERSION_REQUIREMENT))
del DRJIT_VERSION_REQUIREMENT

# Import detail module before native extensions
from . import detail

with _dr.detail.scoped_rtld_deepbind():
    # Replaces 'mitsuba' in sys.modules with itself (mitsuba_alias)
    from . import mitsuba_alias

_ = mitsuba_alias # Removes unused variable warnings


def _stub_variant() -> str:
    """Pick the variant whose bindings expose the largest part of the API.

    The stubs are generated once and shared between all variants, so this
    prefers the color representations that add the most (polarization,
    spectral rendering) over a plain RGB build.
    """
    scores = { 'scalar': 1, 'llvm': 200, 'cuda': 300, 'mono': 10, 'rgb': 20,
               'spectral': 30, 'polarized': 100 }

    def score(variant: str) -> int:
        return sum(v for feature, v in scores.items() if feature in variant)

    import mitsuba
    return max(mitsuba.variants(), key=score)


if _os.environ.get('NB_STUBGEN'):
    # Automatically set a variant so that Mitsuba is ready for stub generation
    import mitsuba
    mitsuba.set_variant(_stub_variant())
