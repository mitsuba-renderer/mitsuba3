import math
import os

import pytest
import enoki as ek
import mitsuba


def test01_construct(variant_scalar_rgb):
    from mitsuba.core import load_string
    from mitsuba.render import ImageBlock

    im = ImageBlock([2, 3], [33, 12], 4)
    assert im is not None
    assert ek.all(im.offset() == [2, 3])
    im.set_offset([10, 20])
    assert ek.all(im.offset() == [10, 20])

    assert ek.all(im.size() == [33, 12])
    assert im.warn_invalid()
    assert im.border_size() == 0  # Since there's no reconstruction filter
    assert im.channel_count() == 4
    assert im.tensor() is not None

    rfilter = load_string("""<rfilter version="2.0.0" type="gaussian">
            <float name="stddev" value="15"/>
        </rfilter>""")
    im = ImageBlock(0, [10, 11], 2, rfilter=rfilter, warn_invalid=False)
    assert im.border_size() == rfilter.border_size()
    assert im.channel_count() == 2
    assert not im.warn_invalid()


@pytest.mark.parametrize("filter_name", ['gaussian', 'tent', 'box'])
@pytest.mark.parametrize("border", [ False, True ])
@pytest.mark.parametrize("offset", [ [0, 0], [1, 2] ])
@pytest.mark.parametrize("normalize", [ False, True ])
@pytest.mark.parametrize("coalesce", [ False, True ])
def test02_put(variants_all, filter_name, border, offset, normalize, coalesce):
    # Checks the result of the ImageBlock::put() method
    # against a brute force reference

    from mitsuba.core import load_dict, TensorXf, \
        ScalarVector2u, Float, Point2f
    from mitsuba.render import ImageBlock

    scalar = 'scalar' in mitsuba.variant()

    rfilter = load_dict({ 'type' : filter_name })

    for j in range(5):
        for i in range(5):
            # Intentional: one of the points is exactly on a boundary
            pos = Point2f(3.3+0.25*i, 3+0.25*j)
            ek.make_opaque(pos)

            size = ScalarVector2u(6, 6)

            block = ImageBlock(offset=offset,
                               size=size,
                               channel_count=1,
                               rfilter=rfilter,
                               border=border,
                               normalize=normalize,
                               coalesce=coalesce)

            block.put(pos=pos, values=[1])

            size += 2 * block.border_size()
            Array1f = Float if not scalar else ek.scalar.ArrayXf

            shift = 0.5 - pos - block.border_size() + block.offset()
            p = ek.meshgrid(
                ek.arange(Array1f, size[0]) + shift[0],
                ek.arange(Array1f, size[1]) + shift[1]
            )

            import numpy as np
            if scalar:
                ref = ek.zero(TensorXf, block.tensor().shape)
                if filter_name == 'box':
                    eval_method = rfilter.eval
                else:
                    eval_method = rfilter.eval_discretized

                out = ref.array
                for i in range(ek.hprod(size)):
                    out[i] = eval_method(-p[0][i]) * \
                             eval_method(-p[1][i])
            else:
                ref = TensorXf(rfilter.eval(-p[0]) *
                               rfilter.eval(-p[1]),
                               block.tensor().shape)

            if normalize:
                value = ek.hsum(ref)
                if value > 0:
                    ref /= value
            match = ek.allclose(block.tensor(), ref, atol=1e-5)

            if not match:
                print(f'pos={pos}, rfilter={rfilter}, '
                      f'border={block.border_size()}, offset={offset}, normalize={normalize}, coalesce={coalesce}')
                print('\nref:')
                print(np.array(ref)[..., 0])
                print('\ncomputed:')
                print(np.array(block.tensor())[..., 0])

            assert match


@pytest.mark.parametrize("filter_name", ['gaussian', 'box'])
def test03_put_boundary(variants_all_rgb, filter_name):
    from mitsuba.core import load_dict, Float
    from mitsuba.render import ImageBlock

    rfilter = load_dict({'type': filter_name})
    im = ImageBlock(0, [3, 3], 1, rfilter=rfilter, warn_negative=False, border=False)
    im.clear()
    im.put([1.5, 1.5], [1.0])
    if ek.is_jit_array_v(Float):
        a = ek.slice(rfilter.eval(0))
        b = ek.slice(rfilter.eval(1))
        c = b**2
        assert ek.allclose(im.tensor().array, [c, b, c,
                                               b, a, b,
                                               c, b, c], atol=1e-3)
    else:
        a = rfilter.eval_discretized(0)
        b = rfilter.eval_discretized(1)
        c = b**2
        assert ek.allclose(im.tensor().array, [c, b, c,
                                               b, a, b,
                                               c, b, c], atol=1e-3)

@pytest.mark.parametrize("filter_name", ['gaussian', 'tent', 'box'])
@pytest.mark.parametrize("border", [ False, True ])
@pytest.mark.parametrize("offset", [ [0, 0], [1, 2] ])
@pytest.mark.parametrize("normalize", [ False, True ])
@pytest.mark.parametrize("enable_ad", [ False, True ]) # AD enables/disables loop recording in JIT modes
def test04_read(variants_all, filter_name, border, offset, normalize, enable_ad):
    # Checks the result of the ImageBlock::fetch() method
    # (reading random data) against a brute force reference

    from mitsuba.core import load_dict, TensorXf, \
        ScalarVector2u, Float, Point2f, PCG32
    from mitsuba.render import ImageBlock
    import numpy as np

    scalar = 'scalar' in mitsuba.variant()

    rfilter = load_dict({ 'type' : filter_name })
    size = ScalarVector2u(6, 6)
    size_b = size

    if border:
        size_b += rfilter.border_size() * 2

    Array1f = Float if not scalar else ek.scalar.ArrayXf

    src = Array1f(np.float32(np.random.rand(size_b[0]*size_b[1])))

    if ek.is_diff_array_v(Float) and enable_ad:
        ek.enable_grad(src)

    source_tensor = TensorXf(array=src, shape=(size_b[0], size_b[1], 1))

    block = ImageBlock(offset, source_tensor,
                       rfilter=rfilter,
                       border=border,
                       normalize=normalize)

    for j in range(5):
        for i in range(5):
            # Intentional: one of the points is exactly on a boundary
            pos = Point2f(3.3+0.25*i, 3+0.25*j)
            ek.make_opaque(pos)

            shift = 0.5 - pos - block.border_size() + block.offset()
            p = ek.meshgrid(
                ek.arange(Array1f, size_b[0]) + shift[0],
                ek.arange(Array1f, size_b[1]) + shift[1]
            )

            value = block.read(pos)[0]

            import numpy as np
            if scalar:
                if filter_name == 'box':
                    eval_method = rfilter.eval
                else:
                    eval_method = rfilter.eval_discretized

                weights = ref = 0
                for i in range(ek.hprod(size)):
                    weight = eval_method(-p[0][i]) * eval_method(-p[1][i])
                    ref = ek.fmadd(src[i], weight, ref)
                    weights += weight

                if weights > 0 and normalize:
                    ref /= weights
            else:
                weight = rfilter.eval(-p[0]) * rfilter.eval(-p[1])
                ref = ek.hsum(weight * ek.detach(source_tensor.array))

                if normalize:
                    ref /= ek.hsum(weight)

            assert ek.allclose(value, ref, atol=1e-5) 
