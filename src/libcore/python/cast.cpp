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


py::object py_cast_object(Object *o) {
    // Try casting, starting from the most precise types
    PY_CAST_OBJECT_VARIANTS(Scene);
    PY_CAST_OBJECT_VARIANTS(Mesh);
    PY_CAST_OBJECT_VARIANTS(Shape);
    PY_CAST_OBJECT_VARIANTS(Texture);
    PY_CAST_OBJECT_VARIANTS(Texture3D);
    PY_CAST_OBJECT_VARIANTS(ReconstructionFilter);

    PY_CAST_OBJECT_VARIANTS(ProjectiveCamera);
    PY_CAST_OBJECT_VARIANTS(Sensor);

    PY_CAST_OBJECT_VARIANTS(Emitter);
    PY_CAST_OBJECT_VARIANTS(Endpoint);

    PY_CAST_OBJECT_VARIANTS(BSDF);

    PY_CAST_OBJECT_VARIANTS(ImageBlock);
    PY_CAST_OBJECT_VARIANTS(Film);

    PY_CAST_OBJECT_VARIANTS(MonteCarloIntegrator);
    PY_CAST_OBJECT_VARIANTS(SamplingIntegrator);
    PY_CAST_OBJECT_VARIANTS(Integrator);

    PY_CAST_OBJECT_VARIANTS(Sampler);

    Log(Warn, "Unable to cast object pointer. Is your type registered in py_cast_object()?");
    return py::none();
}

py::object py_cast(const std::type_info &type, void *ptr) {
    PY_CAST_VARIANTS(Float)
    PY_CAST_VARIANTS(Int32)
    PY_CAST_VARIANTS(Mask)
    PY_CAST_VARIANTS(Array1f)
    PY_CAST_VARIANTS(Array3f)
    PY_CAST_VARIANTS(Color3f)
    PY_CAST_VARIANTS(Point3f)
    PY_CAST_VARIANTS(Vector3f)
    PY_CAST_VARIANTS(Normal3f)
    PY_CAST_VARIANTS(Frame3f)
    PY_CAST_VARIANTS(Matrix3f)
    PY_CAST_VARIANTS(Matrix4f)
    PY_CAST_VARIANTS(Transform3f)
    PY_CAST_VARIANTS(Transform4f)
    PY_CAST(AnimatedTransform)

    PY_CAST_VARIANTS(DynamicBuffer);
    PY_CAST_VARIANTS(DataBuffer1);
    PY_CAST_VARIANTS(DataBuffer3);

    Log(Warn, "Unable to cast void pointer. Is your type registered in py_cast()? (%s)", type.name());
    return py::none();
}
