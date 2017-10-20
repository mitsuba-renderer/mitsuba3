#include <mitsuba/python/python.h>

MTS_PY_DECLARE(ETransportMode);
MTS_PY_DECLARE(EMeasure);
MTS_PY_DECLARE(Scene);
MTS_PY_DECLARE(Shape);
MTS_PY_DECLARE(ShapeKDTree);
MTS_PY_DECLARE(PositionSample);
MTS_PY_DECLARE(DirectionSample);
MTS_PY_DECLARE(DirectSample);
MTS_PY_DECLARE(SurfaceInteraction);
MTS_PY_DECLARE(Endpoint);
MTS_PY_DECLARE(Emitter);
MTS_PY_DECLARE(Sensor);
MTS_PY_DECLARE(BSDF);
MTS_PY_DECLARE(BSDFSample);
MTS_PY_DECLARE(rt);
MTS_PY_DECLARE(ImageBlock);
MTS_PY_DECLARE(Film);

PYBIND11_MODULE(mitsuba_render_ext, m_) {
    (void) m_; /* unused */

    py::module m = py::module::import("mitsuba.render");

    MTS_PY_IMPORT(ETransportMode);
    MTS_PY_IMPORT(EMeasure);
    MTS_PY_IMPORT(Scene);
    MTS_PY_IMPORT(Shape);
    MTS_PY_IMPORT(ShapeKDTree);
    MTS_PY_IMPORT(PositionSample);
    MTS_PY_IMPORT(DirectionSample);
    MTS_PY_IMPORT(DirectSample);
    MTS_PY_IMPORT(SurfaceInteraction);
    MTS_PY_IMPORT(Endpoint);
    MTS_PY_IMPORT(Emitter);
    MTS_PY_IMPORT(Sensor);
    MTS_PY_IMPORT(BSDF);
    MTS_PY_IMPORT(BSDFSample);
    MTS_PY_IMPORT(rt);
    MTS_PY_IMPORT(ImageBlock);
    MTS_PY_IMPORT(Film);
}
