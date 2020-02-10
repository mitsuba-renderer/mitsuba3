import mitsuba
import pytest
import enoki as ek
import numpy as np

from mitsuba.python.test import variant_scalar, variant_packet, variants_vec


@pytest.fixture(params=['Hierarchical2D', 'MarginalDiscrete2D',
                        'MarginalContinuous2D'])
def warps(request):
    return [getattr(mitsuba.core, request.param + str(i)) for i in range(4)]


def test01_sample_inverse_simple(variant_scalar, warps):
    # Spot checks of Hierarchical2D0, MarginalDiscrete2D0
    if 'Continuous' in warps[0].__name__:
        return

    # Mismatched X/Y resolution, odd number of columns
    ref = np.array([[1, 2, 5],
                    [9, 7, 2]])

    # Normalized discrete PDF corresponding to the two patches
    intg = np.array([19, 16]) / 35

    distr = warps[0](ref)

    # Check if we can reach the corners and transition between patches
    assert ek.allclose(distr.sample([0, 0]), [[0, 0], 8.0 / 35.0], atol=1e-6)
    assert ek.allclose(distr.sample([1, 1]), [[1, 1], 16.0 / 35.0], atol=1e-6)
    assert ek.allclose(distr.sample([intg[0], 0]), [[0.5, 0], 16.0 / 35.0],
                       atol=1e-6)

    assert ek.allclose(distr.invert([0, 0]), [[0, 0], 8.0 / 35.0], atol=1e-6)
    assert ek.allclose(distr.invert([1, 1]), [[1, 1], 16.0 / 35.0], atol=1e-6)
    assert ek.allclose(distr.invert([0.5, 0]), [[intg[0], 0], 16.0 / 35.0],
                       atol=1e-6)

    from mitsuba.core.warp import bilinear_to_square

    # Check if we can sample a specific position in each patch
    sample, pdf = bilinear_to_square(1, 2, 9, 7, [0.4, 0.3])
    sample.x *= intg[0]
    pdf *= 8.0 / 35.0
    assert ek.allclose(distr.sample(sample), [[0.2, 0.3], pdf])
    assert ek.allclose(distr.invert([0.2, 0.3]), [sample, pdf])
    assert ek.allclose(distr.eval([0.2, 0.3]), pdf)

    sample, pdf = bilinear_to_square(2, 5, 7, 2, [0.4, 0.3])
    sample.x = sample.x * intg[1] + intg[0]
    pdf *= 8.0 / 35.0
    assert ek.allclose(distr.sample(sample), [[0.7, 0.3], pdf])
    assert ek.allclose(distr.invert([0.7, 0.3]), [sample, pdf])
    assert ek.allclose(distr.eval([0.7, 0.3]), pdf+1)

def test01_sample_simple(variant_scalar, warps):
    if 'Continuous' in warps[0].__name__:
        return

# Test inverse<->sample, eval
# Chi2 test pow2, npow2
# both hi res and low res
# Test interpolation with and without normalization
# Test sampling with and without interpolation
#  Test combinations of multiple distributins at boundary and in the middle
# Does invert still work for non-normalized data?

