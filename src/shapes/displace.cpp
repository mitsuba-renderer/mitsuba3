#include <mitsuba/render/mesh.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>
#include <drjit/sphere.h>

NAMESPACE_BEGIN(mitsuba)
using namespace detail;

/**!

.. _shape-displace:

Displaced mesh (:monosp:`displace`)
-----------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - |shape|
   - The mesh to displace. Any mesh-type shape
     (e.g. :ref:`ply <shape-ply>`, :ref:`obj <shape-obj>`, or
     :ref:`packed <shape-packed>`) may be specified here.

 * - height
   - |texture|
   - Height field that is evaluated at each vertex. A spatially varying
     texture requires the nested mesh to have texture coordinates.
     (Default: 0.5)

 * - scale
   - |float|
   - Distance by which a unit height moves a vertex. (Default: 1)

 * - midlevel
   - |float|
   - Height value at which the surface stays in place. (Default: 0.5)

 * - face_normals
   - |bool|
   - When set to |true|, the displaced mesh uses face normals, which gives it
     a faceted appearance. (Default: inherited from the nested mesh)

 * - flip_normals
   - |bool|
   - Is the mesh inverted, i.e. should the normal vectors be flipped?
     (Default: |false|, i.e. the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

In addition, this plugin exposes the standard mesh state parameters
documented in :ref:`sec-shape-mesh-parameters`.

This plugin displaces every vertex of a nested mesh along its normal by

.. math::

    d = (h(u, v) - \texttt{midlevel}) \cdot \texttt{scale},

where :math:`h` is the value of the :paramtype:`height` texture at the vertex.
Displacement happens in the coordinate system of the nested mesh, i.e. before
the :paramtype:`to_world` transformation of this plugin is applied.

The plugin does not tessellate the input. The nested mesh must already be
fine enough to resolve the height field.

.. tabs::
    .. code-tab:: xml
        :name: displace

        <shape type="displace">
            <shape type="ply">
                <string name="filename" value="wall.ply"/>
            </shape>
            <texture type="bitmap" name="height">
                <string name="filename" value="bricks_height.png"/>
                <boolean name="raw" value="true"/>
            </texture>
            <float name="scale" value="0.02"/>
            <float name="midlevel" value="0"/>
            <bsdf type="diffuse"/>
        </shape>

    .. code-tab:: python

        'type': 'displace',
        'mesh': {
            'type': 'ply',
            'filename': 'wall.ply'
        },
        'height': {
            'type': 'bitmap',
            'filename': 'bricks_height.png',
            'raw': True
        },
        'scale': 0.02,
        'midlevel': 0.0,
        'material': {
            'type': 'diffuse'
        }
 */

template <typename Float, typename Spectrum>
class DisplacedMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_filename, m_to_world, m_flip_normals,
                   m_face_normals, m_bsdf, m_vertex_count, from_fields,
                   recompute_normals, remove_attribute, copy_attributes)
    MI_IMPORT_TYPES(Texture, BSDF, Shape)

    using typename Base::FloatBuffer;
    using typename Base::IndexBuffer;

    DisplacedMesh(const Properties &props) : Base(props) {
        ScopedPhase phase(ProfilerPhase::LoadGeometry);
        Timer timer;

        ref<Base> mesh;
        bool has_bsdf = false;
        for (auto &prop : props.objects()) {
            if (Base *m = prop.try_get<Base>()) {
                if (mesh)
                    Throw("displace: only a single nested mesh may be specified.");
                mesh = m;
            } else if (prop.try_get<Shape>()) {
                Throw("displace: the nested shape must be a mesh.");
            } else if (prop.try_get<BSDF>()) {
                has_bsdf = true;
            } else if (prop.name() == "height" && prop.try_get<Texture>()) {
                // The Shape constructor registered it as a texture attribute
                remove_attribute("height");
            }
        }

        if (!mesh)
            Throw("displace: a nested mesh shape is required.");

        ref<Texture> height = props.get_unbounded_texture<Texture>("height", .5f);
        bool has_texcoords = mesh->has_texcoords();
        if (!has_texcoords && height->is_spatially_varying())
            Throw("displace: the nested mesh \"%s\" has no texture coordinates.",
                  mesh->id());
        ScalarFloat scale    = props.get<ScalarFloat>("scale", 1.f),
                    midlevel = props.get<ScalarFloat>("midlevel", .5f);

        if (!props.has_property("face_normals"))
            m_face_normals = mesh->has_face_normals();
        if (!has_bsdf)
            m_bsdf = mesh->bsdf();
        if (m_filename.empty())
            m_filename = std::string(props.id().empty() ? mesh->id() : props.id());

        using Buffer = DynamicBuffer<Float>;
        size_t V = mesh->vertex_count(), F = mesh->face_count(),
               P = mesh->position_count();
        bool has_normals = mesh->has_normals();

        TensorXu32 faces     = mesh->faces();
        TensorXf32 positions = mesh->positions(),
                   texcoords = mesh->texcoords(),
                   normals   = has_normals ? mesh->normals() : TensorXf32();
        IndexBuffer pidx = mesh->position_index(),
                    nidx = has_normals ? mesh->normal_index() : IndexBuffer();

        auto position = [&](const UInt32 &v) {
            return Point3f(deinterleave<3>(positions, gather_map(pidx, v)));
        };
        auto texcoord = [&](const UInt32 &v) {
            return Point2f(deinterleave<2>(texcoords, v));
        };

        // Angle-weighted normal of each position (as in Mesh::compute_normals())
        // and the mean UV distance from each vertex to its neighbors
        Buffer dir     = dr::zeros<Buffer>(3 * P),
               spacing = dr::zeros<Buffer>(has_texcoords ? V : 0),
               valence = dr::zeros<Buffer>(has_texcoords ? V : 0);

        foreach_index<UInt32>(F, [&](const UInt32 &f) {
            Vector3u fi = deinterleave<3>(faces, f);
            Point3f p[3] = { position(fi[0]), position(fi[1]), position(fi[2]) };

            Vector3f n = dr::cross(p[1] - p[0], p[2] - p[0]);
            Float length_sqr = dr::squared_norm(n);
            Mask valid = length_sqr > 0.f;
            n *= dr::rsqrt(length_sqr);

            for (int k = 0; k < 3; ++k) {
                Float angle = dr::unit_angle(dr::normalize(p[(k + 1) % 3] - p[k]),
                                             dr::normalize(p[(k + 2) % 3] - p[k]));
                interleaved_add<3>(dir, Vector3f(n * angle),
                                   gather_map(pidx, fi[k]), valid);

                if (has_texcoords) {
                    UInt32 a = fi[k], b = fi[(k + 1) % 3];
                    Float len = dr::norm(texcoord(a) - texcoord(b));
                    for (const UInt32 &v : { a, b }) {
                        dr::scatter_reduce(ReduceOp::Add, spacing, len, v);
                        dr::scatter_reduce(ReduceOp::Add, valence, Float(1.f), v);
                    }
                }
            }
        });

        // Height of each position, averaged over its vertices
        Buffer h_sum = dr::zeros<Buffer>(P), h_count = dr::zeros<Buffer>(P);

        foreach_index<UInt32>(V, [&](const UInt32 &v) {
            UInt32 pv = gather_map(pidx, v);
            Vector3f d = deinterleave<3>(dir, pv);
            Float length_sqr = dr::squared_norm(d);

            SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>(dr::width(v));
            si.p = position(v);
            si.n = dr::select(length_sqr > 0.f, Normal3f(d * dr::rsqrt(length_sqr)),
                              Normal3f(0.f, 0.f, 1.f));
            si.sh_frame = Frame3f(si.n);
            si.footprint_scale = 1.f;
            if (has_texcoords) {
                Float s = deinterleave<1>(spacing, v)[0] /
                          dr::maximum(deinterleave<1>(valence, v)[0], 1.f);
                si.uv = texcoord(v);
                si.footprint = Matrix2f(s, 0.f, 0.f, s);
            }

            dr::scatter_reduce(ReduceOp::Add, h_sum, height->eval_1(si), pv);
            dr::scatter_reduce(ReduceOp::Add, h_count, Float(1.f), pv);
        });

        // Displace each position and bake the transform into it
        AffineTransform4f to_world = m_to_world.value();
        FloatBuffer new_positions = interleaved<3, FloatBuffer>(P, [&](const UInt32 &p) {
            Vector3f d = deinterleave<3>(dir, p);
            Float count = deinterleave<1>(h_count, p)[0],
                  dist  = (deinterleave<1>(h_sum, p)[0] / count - midlevel) * scale,
                  length_sqr = dr::squared_norm(d);
            Point3f q = Point3f(deinterleave<3>(positions, p)) +
                        dr::select(length_sqr > 0.f && count > 0.f,
                                   d * (dist * dr::rsqrt(length_sqr)), Vector3f(0.f));
            return Vector<Float32, 3>(to_world * q);
        });

        // A mirroring transform reverses the orientation of the geometry
        if (dr::det(ScalarMatrix3f(m_to_world.scalar().matrix)) < 0.f)
            m_flip_normals = !m_flip_normals;
        m_to_world = ScalarAffineTransform4f();

        from_fields(faces, TensorXf32(std::move(new_positions), { P, 3 }),
                    normals, texcoords, pidx, nidx);
        if (!m_face_normals && has_normals)
            recompute_normals();

        // Custom attributes carry over, as the vertex and face order is kept
        copy_attributes(*mesh);

        Log(Debug, "\"%s\": displaced %i vertices (%zu positions) in %s",
            m_filename, m_vertex_count, P,
            util::time_string((float) timer.value()));
    }

    MI_DECLARE_CLASS(DisplacedMesh)
};

MI_EXPORT_PLUGIN(DisplacedMesh)
NAMESPACE_END(mitsuba)
