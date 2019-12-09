#include <mitsuba/python/python.h>
#include <mitsuba/render/emitter.h>

MTS_PY_EXPORT(Emitter) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    MTS_PY_CHECK_ALIAS(Emitter, m) {
        auto emitter = MTS_PY_CLASS(Emitter, Endpoint)
            .def_method(Emitter, is_environment);

        if constexpr (is_array_v<Float>) {
            emitter.def_static(
                "sample_ray_vec",
                vectorize<Float>([](const EmitterPtr &ptr, Float time, Float sample1,
                                    const Point2f &sample2, const Point2f &sample3, Mask active) {
                    return ptr->sample_ray(time, sample1, sample2, sample3, active);
                }),
                "ptr"_a, "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
                D(Emitter, sample_ray));
            emitter.def_static("sample_direction_vec",
                                vectorize<Float>([](const EmitterPtr &ptr, const Interaction3f &it,
                                                    const Point2f &sample, Mask active) {
                                    return ptr->sample_direction(it, sample, active);
                                }),
                                "ptr"_a, "it"_a, "sample"_a, "active"_a = true,
                                D(Emitter, sample_direction));
            emitter.def_static("pdf_direction_vec",
                                vectorize<Float>([](const EmitterPtr &ptr, const Interaction3f &it,
                                                    const DirectionSample3f &ds, Mask active) {
                                    return ptr->pdf_direction(it, ds, active);
                                }),
                                "ptr"_a, "it"_a, "ds"_a, "active"_a = true,
                                D(Emitter, pdf_direction));
            emitter.def_static(
                "eval_vec",
                vectorize<Float>([](const EmitterPtr &ptr, const SurfaceInteraction3f &si,
                                    Mask active) { return ptr->eval(si, active); }),
                "ptr"_a, "si"_a, "active"_a = true, D(Emitter, eval));
        }
    }
}
