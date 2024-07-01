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