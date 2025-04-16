from typing import Mapping
import drjit as dr
import mitsuba as mi

# The built-in Mitsuba optimizers have been replaced with an improved and
# general implementation in Dr.Jit. More information is available at the
# following URL:
# https://drjit.readthedocs.io/en/latest/reference.html#module-drjit.opt

from drjit.opt import Optimizer, Adam, SGD, RMSProp, GradScaler

# Patch the Dr.Jit optimizers to ignore non-differentiable scene parameters
def _filter_func(self, params: Mapping[str, dr.ArrayBase]) -> Mapping[str, dr.ArrayBase]:
    if type(params) is mi.SceneParameters:
        filtered = { }
        for k, v in params.items():
            if params.flags(k) & mi.ParamFlags.NonDifferentiable.value:
                continue
            filtered[k] = v
        return filtered
    else:
        return params

Optimizer._filter = _filter_func
