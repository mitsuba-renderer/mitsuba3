#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/pair.h>
#include <drjit/python.h>

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#if !defined(MI_ENABLE_EMBREE)
#  include <mitsuba/render/kdtree.h>
#endif

#include "signal.h"

// Helper function to register Python plugins with specific type
static void register_typed_plugin(std::string_view name, nb::handle constructor, ObjectType type) {
    const char *variant = STRINGIFY(MI_VARIANT_NAME);

    auto instantiate = [](void *payload, const Properties &props) -> ref<Object> {
        nb::gil_scoped_acquire gil;
        nb::handle ctor((PyObject *) payload);
        nb::object props_o = nb::cast(props, nb::rv_policy::reference);
        return nb::cast<ref<Object>>(ctor(props_o));
    };

    auto release = [](void *payload) {
        nb::gil_scoped_acquire gil;
        nb::handle((PyObject *) payload).dec_ref();
    };

    PluginManager::instance()->register_plugin(
        name, variant, type, instantiate, release, constructor.inc_ref().ptr());
};


MI_PY_EXPORT(Scene) {
    MI_PY_IMPORT_TYPES(Scene, Integrator, SamplingIntegrator, MonteCarloIntegrator, Sensor)
    auto scene = MI_PY_CLASS(Scene, Object, nb::is_final())
        .def(nb::init<const Properties>())
        .def("ray_intersect_preliminary",
             nb::overload_cast<const Ray3f &, Mask, Mask>(&Scene::ray_intersect_preliminary, nb::const_),
             "ray"_a, "coherent"_a = false, "active"_a = true, D(Scene, ray_intersect_preliminary))
        .def("ray_intersect_preliminary",
             nb::overload_cast<const Ray3f &, Mask, bool, UInt32, uint32_t, Mask>(&Scene::ray_intersect_preliminary, nb::const_),
             "ray"_a, "coherent"_a, "reorder"_a = false,
             "reorder_hint"_a = 0, "reorder_hint_bits"_a = 0, "active"_a = true, D(Scene, ray_intersect_preliminary, 2))
        .def("ray_intersect",
             nb::overload_cast<const Ray3f &, Mask>(&Scene::ray_intersect, nb::const_),
             "ray"_a, "active"_a = true, D(Scene, ray_intersect))
        .def("ray_intersect",
             nb::overload_cast<const Ray3f &, uint32_t, Mask, Mask>(&Scene::ray_intersect, nb::const_),
             "ray"_a, "ray_flags"_a, "coherent"_a, "active"_a = true, D(Scene, ray_intersect, 2))
        .def("ray_intersect",
             nb::overload_cast<const Ray3f &, uint32_t, Mask, bool, UInt32, uint32_t, Mask>(&Scene::ray_intersect, nb::const_),
             "ray"_a, "ray_flags"_a, "coherent"_a, "reorder"_a = false,
             "reorder_hint"_a = 0, "reorder_hint_bits"_a = 0, "active"_a = true, D(Scene, ray_intersect, 3))
        .def("ray_test",
             nb::overload_cast<const Ray3f &, Mask>(&Scene::ray_test, nb::const_),
             "ray"_a, "active"_a = true, D(Scene, ray_test))
        .def("ray_test",
             nb::overload_cast<const Ray3f &, Mask, Mask>(&Scene::ray_test, nb::const_),
             "ray"_a, "coherent"_a, "active"_a = true, D(Scene, ray_test, 2))
#if !defined(MI_ENABLE_EMBREE)
        .def("ray_intersect_naive",
            &Scene::ray_intersect_naive,
            "ray"_a, "active"_a = true)
#endif
        .def("sample_emitter", &Scene::sample_emitter,
             "sample"_a, "active"_a = true, D(Scene, sample_emitter))
        .def("pdf_emitter", &Scene::pdf_emitter,
             "index"_a, "active"_a = true, D(Scene, pdf_emitter))
        .def("sample_emitter_direction", &Scene::sample_emitter_direction,
             "ref"_a, "sample"_a, "test_visibility"_a = true, "active"_a = true,
             D(Scene, sample_emitter_direction))
        .def("pdf_emitter_direction", &Scene::pdf_emitter_direction,
             "ref"_a, "ds"_a, "active"_a = true, D(Scene, pdf_emitter_direction))
        .def("eval_emitter_direction", &Scene::eval_emitter_direction,
             "ref"_a, "ds"_a, "active"_a = true, D(Scene, eval_emitter_direction))
        .def("sample_emitter_ray", &Scene::sample_emitter_ray,
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a,
             D(Scene, sample_emitter_ray))
        .def("sample_silhouette", &Scene::sample_silhouette,
             "sample"_a, "flags"_a, "active"_a = true,
             D(Scene, sample_silhouette))
        .def("invert_silhouette_sample", &Scene::invert_silhouette_sample,
             "ss"_a, "active"_a = true,
             D(Scene, invert_silhouette_sample))
        .def("shape_types", &Scene::shape_types, D(Scene, shape_types))
        // Accessors
        .def_method(Scene, bbox)
        .def("sensors",
             [](const Scene &scene) {
                 nb::list result;
                 for (const Sensor *s : scene.sensors()) {
                     const ProjectiveCamera *p =
                         dynamic_cast<const ProjectiveCamera *>(s);
                     if (p)
                         result.append(nb::cast(ref<const ProjectiveCamera>(p)));
                     else
                         result.append(nb::cast(ref<const Sensor>(s)));
                 }
                 return result;
             },
             D(Scene, sensors))
        .def("sensors_dr", &Scene::sensors_dr, D(Scene, sensors_dr))
        .def("emitters", nb::overload_cast<>(&Scene::emitters), D(Scene, emitters))
        .def("emitters_dr", &Scene::emitters_dr, D(Scene, emitters_dr))
        .def_method(Scene, environment)
        .def("shapes",
             [](const Scene &scene) {
                 nb::list result;
                 for (const Shape *s : scene.shapes()) {
                     const Mesh *m = dynamic_cast<const Mesh *>(s);
                     if (m)
                         result.append(nb::cast(m));
                     else
                         result.append(nb::cast(s));
                 }
                 return result;
             },
             D(Scene, shapes))
        .def("shapes_dr", &Scene::shapes_dr, D(Scene, shapes_dr))
        .def("silhouette_shapes",
             [](const Scene &scene) {
                 nb::list result;
                 for (const Shape *s : scene.silhouette_shapes()) {
                     const Mesh *m = dynamic_cast<const Mesh *>(s);
                     if (m)
                         result.append(nb::cast(m));
                     else
                         result.append(nb::cast(s));
                 }
                 return result;
             },
             D(Scene, silhouette_shapes))
        .def("integrator",
             [](Scene &scene) -> nb::object {
                 Integrator *o = scene.integrator();
                 if (!o)
                     return nb::none();
                 if (auto tmp = dynamic_cast<MonteCarloIntegrator *>(o); tmp)
                     return nb::cast(tmp);
                 if (auto tmp = dynamic_cast<SamplingIntegrator *>(o); tmp)
                     return nb::cast(tmp);
                 if (auto tmp = dynamic_cast<AdjointIntegrator *>(o); tmp)
                     return nb::cast(tmp);
                 return nb::cast(o);
             },
             D(Scene, integrator))
        .def_method(Scene, shapes_grad_enabled)
        .def("__repr__", &Scene::to_string);

    dr::bind_traverse(scene);

    // Type-specific registration functions
    m.def("register_integrator",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Integrator);
          },
          "name"_a, "constructor"_a,
          "Register a Python integrator plugin");

    m.def("register_bsdf",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::BSDF);
          },
          "name"_a, "constructor"_a,
          "Register a Python BSDF plugin");

    m.def("register_emitter",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Emitter);
          },
          "name"_a, "constructor"_a,
          "Register a Python emitter plugin");

    m.def("register_sensor",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Sensor);
          },
          "name"_a, "constructor"_a,
          "Register a Python sensor plugin");

    m.def("register_sampler",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Sampler);
          },
          "name"_a, "constructor"_a,
          "Register a Python sampler plugin");

    m.def("register_texture",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Texture);
          },
          "name"_a, "constructor"_a,
          "Register a Python texture plugin");

    m.def("register_shape",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Shape);
          },
          "name"_a, "constructor"_a,
          "Register a Python shape plugin");

    m.def("register_film",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Film);
          },
          "name"_a, "constructor"_a,
          "Register a Python film plugin");

    m.def("register_rfilter",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::ReconstructionFilter);
          },
          "name"_a, "constructor"_a,
          "Register a Python reconstruction filter plugin");

    m.def("register_medium",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::Medium);
          },
          "name"_a, "constructor"_a,
          "Register a Python medium plugin");

    m.def("register_phase",
          [](const std::string &name, nb::object constructor) {
              register_typed_plugin(name, constructor, ObjectType::PhaseFunction);
          },
          "name"_a, "constructor"_a,
          "Register a Python phase function plugin");
}
