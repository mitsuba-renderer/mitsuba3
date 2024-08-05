import mitsuba as mi
import sys

def stub_variant() -> str:
    tokens = ['rgb', 'spectral', 'polarized', 'ad', 'llvm', 'cuda']
    d = {}

    variants = mi.variants()
    for t in tokens:
        d[t] = any(t in v for v in variants)

    variant = ''
    if d['llvm']:
        variant += 'llvm_'
    elif d['cuda']:
        variant += 'cuda_'
    else:
        variant += 'scalar_'

    if d['ad']:
        variant += 'ad_'

    if d['spectral']:
        variant += 'spectral'
    elif d['rgb']:
        variant += 'rgb'
    else:
        variant += 'mono'

    if d['polarized']:
        variant += '_polarized'

    return variant

v = stub_variant()

try:
    mi.set_variant(v)
except ImportError:
    raise ImportError(f'Based on chosen set of Mitsuba variants, variant {v} ' 
                       'is required for stub generation. Please modify your '
                       'mitsuba.conf file to include this variant and recompile '
                       'Mitsuba.')

sys.modules[__name__] = sys.modules['mitsuba']
sys.modules[__name__ +'.math']      = mi.math
sys.modules[__name__ +'.spline']    = mi.spline
sys.modules[__name__ +'.warp']      = mi.warp
sys.modules[__name__ +'.quad']      = mi.quad
sys.modules[__name__ +'.mueller']   = mi.mueller
sys.modules[__name__ +'.misc']      = mi.misc
sys.modules[__name__ +'.python']    = mi.python

