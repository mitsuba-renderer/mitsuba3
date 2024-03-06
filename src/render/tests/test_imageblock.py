import pytest
import drjit as dr
import mitsuba as mi
import math
import os


def test01_construct(variant_scalar_rgb):
    im = mi.ImageBlock([33, 12], [2, 3], 4)
    assert im is not None
    assert dr.all(im.offset() == [2, 3])
    im.set_offset([10, 20])
    assert dr.all(im.offset() == [10, 20])

    assert dr.all(im.size() == [33, 12])
    assert im.warn_invalid()
    assert im.border_size() == 0  # Since there's no reconstruction filter
    assert im.channel_count() == 4
    assert im.tensor() is not None

    rfilter = mi.load_dict({
        "type" : "gaussian",
        "stddev" : 15
    })
    im = mi.ImageBlock([10, 11], [0, 0], 2, rfilter=rfilter, warn_invalid=False)
    assert im.border_size() == rfilter.border_size()
    assert im.channel_count() == 2
    assert not im.warn_invalid()


@pytest.mark.parametrize("filter_name", ['gaussian', 'tent', 'box'])
@pytest.mark.parametrize("border", [ False, True ])
@pytest.mark.parametrize("offset", [ [0, 0], [1, 2] ])
@pytest.mark.parametrize("normalize", [ False, True ])
@pytest.mark.parametrize("coalesce", [ False, True ])
@pytest.mark.parametrize("compensate", [ False, True ])
def test02_put(variants_all, filter_name, border, offset, normalize, coalesce, compensate):
    # Checks the result of the ImageBlock::put() method
    # against a brute force reference
    scalar = 'scalar' in mi.variant()

    if compensate and scalar:
        pytest.skip('for now, error compensation is just enabled in JIT modes')

    rfilter = mi.load_dict({ 'type' : filter_name })

    for j in range(5):
        for i in range(5):
            # Intentional: one of the points is exactly on a boundary
            pos = mi.Point2f(3.3+0.25*i, 3+0.25*j)
            dr.make_opaque(pos)

            size = mi.ScalarVector2u(6, 6)

            block = mi.ImageBlock(size=size,
                                  offset=offset,
                                  channel_count=1,
                                  rfilter=rfilter,
                                  border=border,
                                  compensate=compensate,
                                  normalize=normalize,
                                  coalesce=coalesce)

            block.put(pos=pos, values=[1])

            size += 2 * block.border_size()
            Array1f = mi.Float if not scalar else dr.scalar.ArrayXf

            shift = 0.5 - pos - block.border_size()
            p = dr.meshgrid(
                dr.arange(Array1f, size[0]) + shift[0],
                dr.arange(Array1f, size[1]) + shift[1]
            )

            shift += block.offset()
            p_offset = dr.meshgrid(
                dr.arange(Array1f, size[0]) + shift[0],
                dr.arange(Array1f, size[1]) + shift[1]
            )

            import numpy as np
            if scalar:
                ref = dr.zeros(mi.TensorXf, block.tensor().shape)
                ref_norm = dr.zeros(mi.TensorXf, block.tensor().shape)
                if filter_name == 'box':
                    eval_method = rfilter.eval
                else:
                    eval_method = rfilter.eval_discretized

                out = ref.array
                out_norm = ref_norm.array
                for i in range(dr.prod(size)):
                    out[i] = eval_method(-p_offset[0][i]) * \
                             eval_method(-p_offset[1][i])
                    out_norm[i] = eval_method(-p[0][i]) * \
                                  eval_method(-p[1][i])
            else:
                ref = mi.TensorXf(rfilter.eval(-p_offset[0]) *
                                  rfilter.eval(-p_offset[1]),
                                  block.tensor().shape)
                ref_norm = mi.TensorXf(rfilter.eval(-p[0]) *
                                       rfilter.eval(-p[1]),
                                       block.tensor().shape)

            if normalize:
                value = dr.sum(ref_norm, axis=None)
                if dr.all(value > 0):
                    ref /= value
            match = dr.allclose(block.tensor(), ref, atol=1e-5)

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
    rfilter = mi.load_dict({'type': filter_name})
    im = mi.ImageBlock([3, 3], [0, 0], 1, rfilter=rfilter, warn_negative=False, border=False)
    im.clear()
    im.put([1.5, 1.5], [1.0])
    if dr.is_jit_v(mi.Float):
        a = dr.slice(rfilter.eval(0), 0)
        b = dr.slice(rfilter.eval(1), 0)
        c = b**2
        assert dr.allclose(im.tensor().array, [c, b, c,
                                               b, a, b,
                                               c, b, c], atol=1e-3)
    else:
        a = rfilter.eval_discretized(0)
        b = rfilter.eval_discretized(1)
        c = b**2
        assert dr.allclose(im.tensor().array, [c, b, c,
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
    import numpy as np

    scalar = 'scalar' in mi.variant()

    rfilter = mi.load_dict({ 'type' : filter_name })
    size = mi.ScalarVector2u(6, 6)
    size_b = size

    if border:
        size_b += rfilter.border_size() * 2

    Array1f = mi.Float if not scalar else dr.scalar.ArrayXf

    src = Array1f(np.float32(np.random.rand(size_b[0]*size_b[1])))

    if dr.is_diff_v(mi.Float) and enable_ad:
        dr.enable_grad(src)

    source_tensor = mi.TensorXf(array=src, shape=(size_b[0], size_b[1], 1))

    block = mi.ImageBlock(source_tensor, offset,
                       rfilter=rfilter,
                       border=border,
                       normalize=normalize)

    for j in range(5):
        for i in range(5):
            # Intentional: one of the points is exactly on a boundary
            pos = mi.Point2f(3.3+0.25*i, 3+0.25*j)
            dr.make_opaque(pos)

            shift = 0.5 - pos - block.border_size() + block.offset()
            p = dr.meshgrid(
                dr.arange(Array1f, size_b[0]) + shift[0],
                dr.arange(Array1f, size_b[1]) + shift[1]
            )

            value = block.read(pos)[0]

            import numpy as np
            if scalar:
                if filter_name == 'box':
                    eval_method = rfilter.eval
                else:
                    eval_method = rfilter.eval_discretized

                weights = ref = 0
                for i in range(dr.prod(size)):
                    weight = eval_method(-p[0][i]) * eval_method(-p[1][i])
                    ref = dr.fma(src[i], weight, ref)
                    weights += weight

                if weights > 0 and normalize:
                    ref /= weights
            else:
                weight = rfilter.eval(-p[0]) * rfilter.eval(-p[1])
                ref = dr.sum(weight * dr.detach(source_tensor.array), axis=None)

                if normalize:
                    ref /= dr.sum(weight, axis=None)

            assert dr.allclose(value, ref, atol=1e-5)


@pytest.mark.parametrize("coalesce", [ False, True ])
@pytest.mark.parametrize("normalize", [ False, True ])
def test05_boundary_effects(variants_vec_rgb, coalesce, normalize):
    # Check that everything works correctly even when the image block
    # is smaller than the filter kernel
    rfilter = mi.load_dict({'type':'gaussian'})
    ib = mi.ImageBlock(
        size=(1, 1),
        offset=(0, 0),
        channel_count=1,
        rfilter=rfilter,
        normalize=normalize,
        coalesce=coalesce
    )
    ib.put(pos=(0.49, 0.49), values=(dr.ones(mi.Float, 1),))

    v1 = 0.9996645373720975
    v2 = 0.13499982060871019
    if normalize:
        ref = v1 / (v1 + 2*v2)**2
    else:
        ref = v1
    assert dr.allclose(ib.tensor().array, ref, rtol=1e-2)

@pytest.mark.parametrize("compensate", [ False, True ])
def test06_error_compensation(variants_any_cuda, compensate):
    with dr.scoped_set_flag(dr.JitFlag.ScatterReduceLocal, False):
        ib = mi.ImageBlock(
            size=(1, 1),
            offset=(0, 0),
            channel_count=1,
            rfilter=None,
            normalize=False,
            coalesce=False,
            compensate=compensate
        )
        ib.put(
            pos=(0.5, 0.5),
            values = (dr.ones(mi.Float, 2**24+1024),)
        )
        print(ib.tensor().array[0])
        print("vs")
        print(2**24 + 1024)
        print(2**24)
        assert ib.tensor().array[0] ==  2**24 + (1024 if compensate else 0)
