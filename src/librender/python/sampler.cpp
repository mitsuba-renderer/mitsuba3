#include <mitsuba/render/sampler.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT(Sampler) {
    MTS_PY_CLASS(Sampler, Object)
        .mdef(Sampler, clone)
        .mdef(Sampler, generate, "offset"_a)
        .mdef(Sampler, advance)
        .mdef(Sampler, next_1d)
        .mdef(Sampler, next_1d_p, "active"_a = mask_t<FloatP>(true))
        .mdef(Sampler, next_2d)
        .mdef(Sampler, next_2d_p, "active"_a = mask_t<FloatP>(true))
        .mdef(Sampler, request_1d_array, "size"_a)
        .mdef(Sampler, request_2d_array, "size"_a)
        .def("next_1d_array", [](Sampler &sampler, size_t size) {
            auto *array = sampler.next_1d_array(size);
            py::array_t<Float> result(size);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = array[i];
            return result;
        }, D(Sampler, next_1d_array), "size"_a)
        .def("next_2d_array", [](Sampler &sampler, size_t size) {
            auto *array = sampler.next_2d_array(size);

            std::vector<size_t> shape = {size, 2};
            py::array_t<Float> result(shape);
            for (size_t i = 0; i < size; ++i) {
                result.mutable_data()[2*i]   = array[i].x();
                result.mutable_data()[2*i+1] = array[i].y();
            }
            return result;
        }, D(Sampler, next_2d_array), "size"_a)
        .mdef(Sampler, sample_count)
        .mdef(Sampler, sample_index)
        .mdef(Sampler, set_sample_index, "sample_index"_a)
        .mdef(Sampler, set_film_resolution, "res"_a, "blocked"_a)
        .def("__repr__", &Sampler::to_string)
        ;
}
