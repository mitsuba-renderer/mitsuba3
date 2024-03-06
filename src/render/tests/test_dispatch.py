import drjit as dr
import mitsuba as mi
import pytest

@pytest.mark.parametrize("recorded", [True, False])
def test01_dispatch(variants_vec_rgb, recorded):
    dr.set_flag(dr.JitFlag.VCallRecord, recorded)

    bsdf1 = mi.load_dict({'type': 'diffuse'})
    bsdf2 = mi.load_dict({'type': 'conductor'})

    mask = mi.Bool([True, True, False, False])

    bsdf_ptr = dr.select(mask, mi.BSDFPtr(bsdf1), mi.BSDFPtr(bsdf2))

    def func(self, si, wo):
        print(f'Tracing -> {self.class_().name()}')
        return self.eval(mi.BSDFContext(), si, wo)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]
    wo = mi.Vector3f(0, 0, 1)
    ctx = mi.BSDFContext()

    res = dr.dispatch(bsdf_ptr, func, si, wo)
    dr.eval(res)

    assert dr.allclose(res, bsdf_ptr.eval(ctx, si, wo))


@pytest.mark.parametrize("recorded", [True, False])
def test02_dispatch_sparse_registry(variants_vec_rgb, recorded):
    dr.set_flag(dr.JitFlag.VCallRecord, recorded)

    bsdf1 = mi.load_dict({'type': 'diffuse'})
    bsdf2 = mi.load_dict({'type': 'plastic'})
    bsdf3 = mi.load_dict({'type': 'conductor'})

    del bsdf2

    mask = mi.Bool([True, True, False, False])

    bsdf_ptr = dr.select(mask, mi.BSDFPtr(bsdf1), mi.BSDFPtr(bsdf3))

    def func(self, si, wo):
        return self.eval(mi.BSDFContext(), si, wo)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]
    wo = mi.Vector3f(0, 0, 1)
    ctx = mi.BSDFContext()

    res = dr.dispatch(bsdf_ptr, func, si, wo)
    dr.eval(res)

    assert dr.allclose(res, bsdf_ptr.eval(ctx, si, wo))

    res = dr.dispatch(bsdf_ptr, func, si, wo)
    dr.eval(res)

    assert dr.allclose(res, bsdf_ptr.eval(ctx, si, wo))


