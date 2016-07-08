#include <mitsuba/render/mesh.h>
#include <mitsuba/core/stream.h>
#include "python.h"

MTS_PY_EXPORT(Shape) {
    MTS_PY_CLASS(Shape, Object)
        .mdef(Shape, bbox);

    MTS_PY_CLASS(Mesh, Shape)
        .mdef(Mesh, vertexStruct)
        .mdef(Mesh, faceStruct)
        .mdef(Mesh, write)
        .def("faces", [](Mesh &) {
                //auto f = m.faces();
                return nullptr;
            },
            DM(Mesh, faces));
}
