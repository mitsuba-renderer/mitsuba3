""" Mitsuba Python extension library """

import sys as _sys
import os as _os
import drjit as _dr
import logging

if _sys.version_info < (3, 8):
    raise ImportError("Mitsuba requires Python 3.8 or greater.")

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

with _dr.detail.scoped_rtld_deepbind():
    # Replaces 'mitsuba' in sys.modules with itself (mitsuba_alias)
    from . import mitsuba_alias

_ = mitsuba_alias # Removes unused variable warnings
