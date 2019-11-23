#include <mitsuba/python/python.h>

#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/texture.h>


py::object py_cast(Object *o) {
    // Try casting, starting from the most precise types
    PY_CAST_VARIANTS(Scene);
    PY_CAST_VARIANTS(Mesh);
    PY_CAST_VARIANTS(Shape);
    PY_CAST_VARIANTS(Texture);
    PY_CAST_VARIANTS(Texture3D);
    PY_CAST_VARIANTS(ReconstructionFilter);

    PY_CAST_VARIANTS(ProjectiveCamera);
    PY_CAST_VARIANTS(Sensor);

    PY_CAST_VARIANTS(Emitter);
    PY_CAST_VARIANTS(Endpoint);

    PY_CAST_VARIANTS(BSDF);

    PY_CAST_VARIANTS(ImageBlock);
    PY_CAST_VARIANTS(Film);

    PY_CAST_VARIANTS(MonteCarloIntegrator);
    PY_CAST_VARIANTS(SamplingIntegrator);
    PY_CAST_VARIANTS(Integrator);

    PY_CAST_VARIANTS(Sampler);

    Log(Warn, "Unable to cast object pointer. Is your type registered in py_cast()?");
    return py::none();
}

