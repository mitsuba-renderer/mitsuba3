#include <mitsuba/python/python.h>

#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>


#define PY_CAST(Name) {                                                        \
        Name *temp = dynamic_cast<Name *>(o);                                  \
        if (temp)                                                              \
            return py::cast(temp);                                             \
    }

py::object py_cast(Object *o) {
    // Try casting, starting from the most precise types
    PY_CAST(Scene);
    PY_CAST(Mesh);
    PY_CAST(Shape);
    PY_CAST(ContinuousSpectrum);
    PY_CAST(ReconstructionFilter);

    PY_CAST(PerspectiveCamera);
    PY_CAST(ProjectiveCamera);
    PY_CAST(Sensor);

    PY_CAST(Emitter);
    PY_CAST(Endpoint);

    PY_CAST(BSDF);
    PY_CAST(MicrofacetDistribution<Float>);
    PY_CAST(MicrofacetDistribution<FloatP>);

    PY_CAST(ImageBlock);
    PY_CAST(Film);

    PY_CAST(MonteCarloIntegrator);
    PY_CAST(SamplingIntegrator);
    PY_CAST(Integrator);

    PY_CAST(Sampler);

    Log(EWarn, "Unable to cast object pointer. Is your type registered in"
               " py_cast()?");
    return py::none();
}

