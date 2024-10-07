import mitsuba
import pytest
import drjit as dr
import numpy as np

def check_uniform_scalar_sampler(sampler, res=16, atol=0.5):
    sampler.seed(0)
    sample_count = sampler.sample_count()

    hist_1d = np.zeros(res)
    hist_2d = np.zeros((res, res))

    for i in range(sample_count):
        v_1d = sampler.next_1d()
        hist_1d[int(v_1d * res)] += 1
        v_2d = sampler.next_2d()
        hist_2d[int(v_2d.x * res), int(v_2d.y * res)] += 1
        sampler.advance()

    # print(hist_1d)
    # print(hist_2d)

    assert dr.allclose(hist_1d, float(sample_count) / res, atol=atol)
    assert dr.allclose(hist_2d, float(sample_count) / (res * res), atol=atol)


def check_uniform_wavefront_sampler(sampler, res=16, atol=0.5):
    import mitsuba as mi

    sample_count = sampler.sample_count()
    sampler.set_samples_per_wavefront(sample_count)

    sampler.seed(0, sample_count)

    hist_1d = dr.zeros(mi.UInt32, res)
    hist_2d = dr.zeros(mi.UInt32, res * res)

    v_1d = dr.clip(sampler.next_1d() * res, 0, res)
    dr.scatter_reduce(
        dr.ReduceOp.Add,
        hist_1d,
        mi.UInt32(1),
        mi.UInt32(v_1d)
    )

    v_2d = mi.Vector2u(dr.clip(sampler.next_2d() * res, 0, res))
    dr.scatter_reduce(
        dr.ReduceOp.Add,
        hist_2d,
        mi.UInt32(1),
        mi.UInt32(v_2d.x * res + v_2d.y)
    )

    assert dr.allclose(mi.Float(hist_1d), float(sample_count) / res, atol=atol)
    assert dr.allclose(mi.Float(hist_2d), float(sample_count) / (res * res), atol=atol)


def check_deep_copy_sampler_scalar(sampler1):
    sampler1.seed(0)

    assert sampler1.wavefront_size() == 1

    for i in range(5):
        sampler1.next_1d()
        sampler1.next_2d()

    sampler2 = sampler1.clone()

    for i in range(10):
        assert dr.all(sampler1.next_1d() == sampler2.next_1d())
        assert dr.all(sampler1.next_2d() == sampler2.next_2d())


def check_deep_copy_sampler_wavefront(sampler1, factor=16):
    sample_count = sampler1.sample_count()

    sampler1.set_samples_per_wavefront(sample_count // factor)
    sampler1.seed(0, sample_count // factor)

    assert sampler1.wavefront_size() == (sample_count // factor)

    for i in range(5):
        sampler1.next_1d()
        sampler1.advance()
        sampler1.next_2d()

    sampler2 = sampler1.clone()

    for i in range(10):
        assert dr.all(sampler1.next_1d() == sampler2.next_1d())
        assert dr.all(sampler1.next_2d() == sampler2.next_2d(), axis=None)

def check_sampler_kernel_hash_wavefront(t, sampler):
    """
    Checks wether re-seeding the sampler causes recompilation of the kernel, sampling from it.
    """
    with dr.scoped_set_flag(dr.JitFlag.KernelHistory, True):
        kernel_hash = None
        for i in range(4):
            seed = t(i)

            sampler.seed(seed, 64)
            
            dr.eval(sampler.next_1d())

            history = dr.kernel_history([dr.KernelType.JIT])
            if kernel_hash is None:
                kernel_hash = history[-1]["hash"]
            else:
                assert kernel_hash ==  history[-1]["hash"]
            
            
