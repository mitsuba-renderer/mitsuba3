#pragma once

// Blender Mesh format types for the exporter
NAMESPACE_BEGIN(blender)
    static const int ME_SMOOTH = 1 << 0; // smooth shading flag

    /// Triangle tesselation of the mesh, contains references to 3 MLoop and the "real" face
    struct MLoopTri {
        unsigned int tri[3];
        unsigned int poly;
    };

    struct MLoopUV {
        float uv[2];
        int flag;
    };

    struct MLoopCol {
        unsigned char r, g, b, a;
    };

    struct MLoop {
        /// Vertex index.
        unsigned int v;
        /// Edge index.
        unsigned int e;
    };

    /// Contains info about the face, like material ID and smooth shading flag
    struct MPoly {
        /// Offset into loop array and number of loops in the face
        int loopstart;
        int totloop;
        short mat_nr;
        char flag, _pad;
    };

    struct MVert {
        float co[3]; // position
        short no[3]; // normal
        char flag, bweight;
    };
NAMESPACE_END(blender)
