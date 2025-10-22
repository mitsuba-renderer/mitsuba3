import mitsuba as mi
import sys
import os

# Assign scores to different features to pick a variant that will produce the
# most useful stubs (which are generate once and shared between all variants).
# Variants impact the API, especially when using fancy color representations
# (polarization, spectral rendering), hence those are scored higher.
FEATURE_SCORES = {
    'scalar': 1,
    'llvm': 2,
    'cuda': 3,
    'mono': 10,
    'rgb': 20,
    'spectral': 30,
    'polarized': 100
}

def stub_variant() -> str:
    def score(variant_name):
        r = 0
        for feature, value in FEATURE_SCORES.items():
            if feature in variant_name:
                r += value
        return r

    return max(mi.variants(), key=score)

v = stub_variant()

if v not in mi.variants():
    raise ImportError(f'Based on chosen set of Mitsuba variants, variant {v} '
                       'is required for stub generation. Please modify your '
                       'mitsuba.conf file to include this variant and recompile '
                       'Mitsuba.')

# Mitsuba variant has static initialization that requires JIT to initialize
# For generating stubs we don't actually care if JIT init is successful
# e.g. if LLVM/CUDA wasn't installed
# We set an envvar to signal to the module that it's only being loaded for stub
# generation and can therefore skip any static initializations.
os.environ["MI_STUB_GENERATION"] = "True"
try:
    mi.set_variant(v)
finally:
    del os.environ["MI_STUB_GENERATION"]

sys.modules[__name__] = sys.modules['mitsuba']
sys.modules[__name__ +'.math']       = mi.math
sys.modules[__name__ +'.spline']     = mi.spline
sys.modules[__name__ +'.warp']       = mi.warp
sys.modules[__name__ +'.quad']       = mi.quad
sys.modules[__name__ +'.mueller']    = mi.mueller
sys.modules[__name__ +'.misc']       = mi.misc
sys.modules[__name__ +'.python']     = mi.python
sys.modules[__name__ +'.detail']     = mi.detail
sys.modules[__name__ +'.filesystem'] = mi.filesystem
sys.modules[__name__ +'.parser']     = mi.parser

