#include <mitsuba/render/emitter.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Emitter) {
    py::enum_<Emitter::EEmitterType>(m, "EEmitterType",
        "Flags used to classify the emission profile of different types of"
        " emitters. \n"
        "\n    EDeltaDirection: Emission profile contains a Dirac delta term with respect to direction."
        "\n    EDeltaPosition: Emission profile contains a Dirac delta term with respect to position."
        "\n    EOnSurface: Is the emitter associated with a surface in the scene?",
        py::arithmetic())
        .value("EDeltaDirection", Emitter::EDeltaDirection)
        .value("EDeltaPosition", Emitter::EDeltaPosition)
        .value("EOnSurface", Emitter::EOnSurface)
        .export_values();

    MTS_PY_CLASS(Emitter, Object)
        .mdef(Emitter, world_transform)
        .def("set_world_transform", &Emitter::set_world_transform,
             D(Emitter, set_world_transform))
        ;
}
