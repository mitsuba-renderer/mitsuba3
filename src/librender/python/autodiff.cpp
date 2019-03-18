#include <mitsuba/render/autodiff.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/transform.h>
#include <map>

#if defined(MTS_ENABLE_AUTODIFF)
namespace mitsuba {
    struct DifferentiableParameters::Details {
        std::string m_prefix;
        std::map<std::string, std::tuple<DifferentiableObject *, void *, size_t>> entries;
    };

    class PyDifferentiableParameters : public DifferentiableParameters {
    public:
        size_t size() { return d->entries.size() == 0; }
        py::object getitem(const std::string &key) {
            auto it = d->entries.find(key);
            if (it == d->entries.end())
                throw py::key_error(key);
            auto value = it->second;
            switch (std::get<2>(value)) {
                case 1 : return py::cast((FloatD *) std::get<1>(value), py::return_value_policy::reference);
                case 2 : return py::cast((Vector2fD *) std::get<1>(value), py::return_value_policy::reference);
                case 3 : return py::cast((Vector3fD *) std::get<1>(value), py::return_value_policy::reference);
                case 4 : return py::cast((Vector4fD *) std::get<1>(value), py::return_value_policy::reference);
                case 16 : return py::cast((Matrix4fD *) std::get<1>(value), py::return_value_policy::reference);
                default: throw std::runtime_error("DifferentiableParameters::getitem:Unsupported type!");
            }
        }

        void setitem(const std::string &key, py::object obj) {
            auto it = d->entries.find(key);
            if (it == d->entries.end())
                throw py::key_error(key);
            auto value = it->second;
            switch (std::get<2>(value)) {
                case 1 : *((FloatD *) std::get<1>(value)) = py::cast<FloatD>(obj); break;
                case 2 : *((Vector2fD *) std::get<1>(value)) = py::cast<Vector2fD>(obj); break;
                case 3 : *((Vector3fD *) std::get<1>(value)) = py::cast<Vector3fD>(obj); break;
                case 4 : *((Vector4fD *) std::get<1>(value)) = py::cast<Vector4fD>(obj); break;
                case 16 : *((Matrix4fD *) std::get<1>(value)) = py::cast<Matrix4fD>(obj); break;
                default: throw std::runtime_error("DifferentiableParameters::setitem: unsupported type!");
            }
            DifferentiableObject *diff_obj = std::get<0>(value);
            if (diff_obj)
                diff_obj->parameters_changed();
        }

        std::vector<std::string> keys() {
            std::vector<std::string> result;
            result.reserve(d->entries.size());
            for (auto &kv: d->entries)
                result.push_back(kv.first);
            return result;
        }

        auto items() {
            std::vector<std::pair<std::string, py::object>> result;
            result.reserve(d->entries.size());
            for (auto &kv: d->entries)
                result.emplace_back(kv.first, getitem(kv.first));
            return result;
        }

        void keep(const std::vector<std::string> &keys) {
            std::map<std::string, std::tuple<DifferentiableObject *, void *, size_t>> entries;
            for (const std::string &k : keys) {
                auto it = d->entries.find(k);
                if (it == d->entries.end())
                    throw std::runtime_error("DifferentiableParameters::keep():"
                                             " could not find parameter \"" +
                                             k + "\"");
                entries.emplace(*it);
            }
            d->entries = entries;
        }
    protected:
        virtual ~PyDifferentiableParameters() = default;
    };

};
#endif

MTS_PY_EXPORT(autodiff) {
    ENOKI_MARK_USED(m);

    auto dc = MTS_PY_CLASS(DifferentiableObject, Object);

#if defined(MTS_ENABLE_AUTODIFF)
    dc.def("put_parameters", &DifferentiableObject::put_parameters);

    py::class_<DifferentiableParameters, Object, ref<DifferentiableParameters>>(m, "DifferentiableParametersBase");
    py::class_<PyDifferentiableParameters, DifferentiableParameters,
               ref<PyDifferentiableParameters>>(m, "DifferentiableParameters")
        .def(py::init<>())
        .def("set_prefix", &DifferentiableParameters::set_prefix)
        .def("keys", &PyDifferentiableParameters::keys)
        .def("items", &PyDifferentiableParameters::items)
        .def("keep", &PyDifferentiableParameters::keep)
        .def("put",
             [](PyDifferentiableParameters &dp, const std::string &name, FloatD &v) {
                 dp.put(nullptr, name, v);
             },
             py::keep_alive<1, 2>())
        .def("put",
             [](PyDifferentiableParameters &dp, const std::string &name, Vector2fD &v) {
                 dp.put(nullptr, name, v);
             },
             py::keep_alive<1, 2>())
        .def("put",
             [](PyDifferentiableParameters &dp, const std::string &name, Vector3fD &v) {
                 dp.put(nullptr, name, v);
             },
             py::keep_alive<1, 2>())
        .def("put",
             [](PyDifferentiableParameters &dp, const std::string &name, Vector4fD &v) {
                 dp.put(nullptr, name, v);
             },
             py::keep_alive<1, 2>())
        .def("put",
             [](PyDifferentiableParameters &dp, const std::string &name, Matrix4fD &v) {
                 dp.put(nullptr, name, v);
             },
             py::keep_alive<1, 2>())
        .def("__len__", &PyDifferentiableParameters::size)
        .def("__getitem__", &PyDifferentiableParameters::getitem)
        .def("__setitem__", &PyDifferentiableParameters::setitem);
#endif
}
