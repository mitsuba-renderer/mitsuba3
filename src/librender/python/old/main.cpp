#include <mitsuba/python/python.h>

MTS_PY_DECLARE(autodiff);
MTS_PY_DECLARE(Scene);
MTS_PY_DECLARE(Shape);
MTS_PY_DECLARE(ShapeKDTree);
MTS_PY_DECLARE(Interaction);
MTS_PY_DECLARE(SurfaceInteraction);
MTS_PY_DECLARE(Endpoint);
MTS_PY_DECLARE(Emitter);
MTS_PY_DECLARE(Sensor);
MTS_PY_DECLARE(BSDFSample);
MTS_PY_DECLARE(BSDF);
MTS_PY_DECLARE(ImageBlock);
MTS_PY_DECLARE(Film);
MTS_PY_DECLARE(Spiral);
MTS_PY_DECLARE(Integrator);
MTS_PY_DECLARE(Sampler);
MTS_PY_DECLARE(Texture);
MTS_PY_DECLARE(MicrofacetDistribution);
MTS_PY_DECLARE(PositionSample);
MTS_PY_DECLARE(DirectionSample);
MTS_PY_DECLARE(fresnel);
MTS_PY_DECLARE(srgb);
MTS_PY_DECLARE(mueller);
MTS_PY_DECLARE(Volume);

PYBIND11_MODULE(mitsuba_render_ext, m_) {
    (void) m_; /* unused */

    py::module m = py::module::import("mitsuba");
    std::vector<py::module> submodule_list;
    MTS_PY_DEF_SUBMODULE(submodule_list, render)

    // MTS_PY_IMPORT(autodiff);
    MTS_PY_IMPORT(Scene);
    MTS_PY_IMPORT(Shape);
    MTS_PY_IMPORT(ShapeKDTree);
    MTS_PY_IMPORT(Interaction);
    MTS_PY_IMPORT(SurfaceInteraction);
    MTS_PY_IMPORT(Endpoint);
    MTS_PY_IMPORT(Emitter);
    MTS_PY_IMPORT(Sensor);
    MTS_PY_IMPORT(BSDFSample);
    MTS_PY_IMPORT(BSDF);
    MTS_PY_IMPORT(ImageBlock);
    MTS_PY_IMPORT(Film);
    MTS_PY_IMPORT(Spiral);
    MTS_PY_IMPORT(Integrator);
    MTS_PY_IMPORT(Sampler);
    MTS_PY_IMPORT(Texture);
    MTS_PY_IMPORT(MicrofacetDistribution);
    MTS_PY_IMPORT(PositionSample);
    MTS_PY_IMPORT(DirectionSample);
    MTS_PY_IMPORT(fresnel);
    MTS_PY_IMPORT(srgb);
    MTS_PY_IMPORT(mueller);
    // MTS_PY_IMPORT(Volume);
}
