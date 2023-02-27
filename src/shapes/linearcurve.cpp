#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#include <drjit/texture.h>

#if defined(MI_ENABLE_EMBREE)
#include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class LinearCurve final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    using InputFloat = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector3f = Vector<InputFloat, 3>;
    using FloatStorage = DynamicBuffer<dr::replace_scalar_t<Float, InputFloat>>;
    using UInt32Storage = DynamicBuffer<UInt32>;
    using Index = typename CoreAliases::UInt32;

    LinearCurve(const Properties &props) : Base(props) {

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        std::string m_name = file_path.filename().string();

        // used for throwing an error later
        auto fail = [&](const char *descr, auto... args) {
            Throw(("Error while loading linear curve file \"%s\": " + std::string(descr))
                      .c_str(), m_name, args...);
        };

        Log(Debug, "Loading a linear curve file from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);
        ScopedPhase phase(ProfilerPhase::LoadGeometry);

        // temporary buggers for vertices and per-vertex radius
        std::vector<InputVector3f> vertices;
        std::vector<InputFloat> radius;
        ScalarSize vertex_guess = mmap->size() / 100;
        vertices.reserve(vertex_guess);

        // load data from the given .txt file
        const char *ptr = (const char *) mmap->data();
        const char *eof = ptr + mmap->size();
        char buf[1025];
        Timer timer;

        while (ptr < eof) {
            // Determine the offset of the next newline
            const char *next = ptr;
            advance<false>(&next, eof, "\n");

            // Copy buf into a 0-terminated buffer
            ScalarSize size = next - ptr;
            if (size >= sizeof(buf) - 1)
                fail("file contains an excessively long line! (%i characters)", size);
            memcpy(buf, ptr, size);
            buf[size] = '\0';

            // handle current line: v.x v.y v.z radius
            // skip whitespace
            const char *cur = buf, *eol = buf + size;
            advance<true>(&cur, eol, " \t\r");
            bool parse_error = false;

            // Vertex position
            InputPoint3f p;
            InputFloat r;
            for (ScalarSize i = 0; i < 3; ++i) {
                const char *orig = cur;
                p[i] = string::strtof<InputFloat>(cur, (char **) &cur);
                parse_error |= cur == orig;
            }

            // parse per-vertex radius
            const char *orig = cur;
            r = string::strtof<InputFloat>(cur, (char **) &cur);
            parse_error |= cur == orig;

            p = m_to_world.scalar().transform_affine(p);

            if (unlikely(!all(dr::isfinite(p))))
                fail("linear curve control point contains invalid vertex position data");
            if (unlikely(!dr::isfinite(r)))
                fail("linear curve control point contains invalid radius data");

            // TODO: how to calculate the bbox
            // just expand using control points for now
            m_bbox.expand(p);

            vertices.push_back(p);
            radius.push_back(r);

            if (unlikely(parse_error))
                fail("could not parse line \"%s\"", buf);
            ptr = next + 1;
        }

        m_control_point_count = vertices.size();
        m_segment_count = m_control_point_count - 1;
        if (unlikely(m_control_point_count < 2))
            fail("linear curve must have at least two control points");


        for (int i = 0; i < m_control_point_count; i++)
            Log(Debug, "Loaded a control point %s with radius %f",
                vertices[i], radius[i]);

        // store the data from the previous temporary buffer
        std::unique_ptr<float[]> vertex_positions_radius(new float[m_control_point_count * 4]);
        std::unique_ptr<ScalarIndex[]> indices(new ScalarIndex[m_segment_count]);
        
        // for OptiX
        std::unique_ptr<float[]> vertex_position(new float[m_control_point_count * 3]);
        std::unique_ptr<float[]> vertex_radius(new float[m_control_point_count * 1]);


        for (ScalarIndex i = 0; i < vertices.size(); i++) {
            InputFloat* position_ptr = vertex_positions_radius.get() + i * 4;
            InputFloat* radius_ptr   = vertex_positions_radius.get() + i * 4 + 3;

            dr::store(position_ptr, vertices[i]);
            dr::store(radius_ptr, radius[i]);
            
            // OptiX
            position_ptr = vertex_position.get() + i * 3;
            radius_ptr = vertex_radius.get() + i;
            dr::store(position_ptr, vertices[i]);
            dr::store(radius_ptr, radius[i]);
        }

        for (ScalarIndex i = 0; i < m_segment_count; i++) {
            u_int32_t* index_ptr = indices.get() + i;
            dr::store(index_ptr, i);
        }

        m_vertex_with_radius = dr::load<FloatStorage>(vertex_positions_radius.get(), m_control_point_count * 4);
        m_indices = dr::load<UInt32Storage>(indices.get(), m_segment_count);

        // OptiX
        m_vertex = dr::load<FloatStorage>(vertex_position.get(), m_control_point_count * 3);
        m_radius = dr::load<FloatStorage>(vertex_radius.get(), m_control_point_count * 1);


        ScalarSize vertex_data_bytes = 4 * sizeof(InputFloat);
        Log(Debug, "\"%s\": read %i control points (%s in %s)",
            m_name, m_control_point_count,
            util::mem_string(m_control_point_count * vertex_data_bytes),
            util::time_string((float) timer.value())
        );

        // size_t m_shape[1] = { m_control_point_count };
        // m_tex = dr::Texture<Float, 1>{m_shape, 3};
        // m_tex.set_value(m_vertex);
        // m_tex_r = dr::Texture<Float, 1>{m_shape, 1};
        // m_tex_r.set_value(m_radius);

        // update();
        initialize();
    }

    template <bool Negate, ScalarSize N>
    void advance(const char **start_, const char *end, const char (&delim)[N]) {
        const char *start = *start_;

        while (true) {
            bool is_delim = false;
            for (ScalarSize i = 0; i < N; ++i)
                if (*start == delim[i])
                    is_delim = true;
            if ((is_delim ^ Negate) || start == end)
                break;
            ++start;
        }

        *start_ = start;
    }

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t /* ray_flags */,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        Float t = pi.t;
        
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t = dr::select(active, t, dr::Infinity<Float>);
        si.p = ray(t);

        Float u_local = pi.prim_uv.x();
        Index idx = pi.prim_index;

        // It seems u_local given by Embree and OptiX has already taken into account the influence of radius
        // My guess is u_local is shifted a bit so that the normal can be easily computed as si.p - pos, 
        // where pos = (1 - u_local) * cp1 + u_local * cp2
        // The shift of u_local is not documented anywhere, but normals computed in this way are consistent with Embree's result
        Point4f q0 = dr::gather<Point4f>(m_vertex_with_radius, idx + 0),
                q1 = dr::gather<Point4f>(m_vertex_with_radius, idx + 1);
        
        Point3f p0(q0.x(), q0.y(), q0.z()),
                p1(q1.x(), q1.y(), q1.z());
        
        Point3f pos = p0 * (1.f - u_local) + p1 * u_local;
        si.sh_frame.n = dr::normalize(si.p - pos);

        // Convert segment-local u to curve-global u
        // Float u_global = (u_local + idx + 0.5) / (m_control_point_count);
        // Use Mitsuba's Texture to interpolate the point position that lies on the curve center and the radius
        // Point3f pos, r;
        // m_tex.eval(u_global, pos.data(), true);
        // m_tex_r.eval(u_global, r.data(), true);
        // si.sh_frame.n = dr::normalize(pi.normal);
        // si.sh_frame.n = dr::normalize(si.p - pos);
        // Point4f q0 = dr::gather<Point4f>(m_vertex_with_radius, idx + 0);
        // Point4f q1 = dr::gather<Point4f>(m_vertex_with_radius, idx + 1);
        // Vector4f d4 = q1 - q0;
        // Vector3f d(d4.x(), d4.y(), d4.z());
        // Float dr = d4.w();
        // Float dd = dr::dot(d, d);
        // Vector3f o1 = si.p - pos;
        // Vector3f normal = dd * o1 - (dr * r.x()) * d;
        // si.sh_frame.n = dr::normalize(normal);

        si.n = si.sh_frame.n;
    
        // Debug
        //si.uv = Point2f(u_local, dr::norm(si.n - dr::normalize(pi.normal)));

        si.shape    = this;
        si.instance = nullptr;

        return si;
    }


    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("control_point_count", m_control_point_count, +ParamFlags::NonDifferentiable);
        callback->put_parameter("vertex",              m_vertex, +ParamFlags::NonDifferentiable);
        callback->put_parameter("radius",              m_radius, +ParamFlags::NonDifferentiable);
        Base::traverse(callback);
    }


    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "vertex") || string::contains(keys, "radius")) {
            
            // update m_vertex_with_radius to match new m_vertex and m_radius
            update();

            mark_dirty();
        }

        Base::parameters_changed();
    }


#if defined(MI_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) override {
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE);

        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4,
                                   m_vertex_with_radius.data(), 0, 4 * sizeof(InputFloat),
                                   m_control_point_count);
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT,
                                   m_indices.data(), 0, 1 * sizeof(ScalarIndex),
                                   m_control_point_count - 1);
        rtcCommitGeometry(geom);
        return geom;
    }
#endif


#if defined(MI_ENABLE_CUDA)
    void optix_prepare_geometry() override { }

    void optix_build_input(OptixBuildInput &build_input) const override {
        m_vertex_buffer_ptr = (void*) m_vertex.data(); // triggers dr::eval()
        m_radius_buffer_ptr = (void*) m_radius.data(); // triggers dr::eval()
        m_index_buffer_ptr = (void*) m_indices.data(); // triggers dr::eval()

        build_input.type = OPTIX_BUILD_INPUT_TYPE_CURVES;
        build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR;
        build_input.curveArray.numPrimitives        = m_segment_count;

        build_input.curveArray.vertexBuffers        = (CUdeviceptr*) &m_vertex_buffer_ptr;
        build_input.curveArray.numVertices          = m_control_point_count;
        build_input.curveArray.vertexStrideInBytes  = sizeof( InputFloat ) * 3;

        build_input.curveArray.widthBuffers         = (CUdeviceptr*) &m_radius_buffer_ptr;
        build_input.curveArray.widthStrideInBytes   = sizeof( InputFloat );

        build_input.curveArray.indexBuffer          = (CUdeviceptr) m_index_buffer_ptr;
        build_input.curveArray.indexStrideInBytes   = sizeof( ScalarIndex );

        build_input.curveArray.normalBuffers        = 0;
        build_input.curveArray.normalStrideInBytes  = 0;
        build_input.curveArray.flag                 = OPTIX_GEOMETRY_FLAG_NONE;
        build_input.curveArray.primitiveIndexOffset = 0;
        build_input.curveArray.endcapFlags          = OPTIX_CURVE_ENDCAP_DEFAULT;

        Log(Debug, "Optix_build_input done for one linear curve, numVertices %d, numPrimitives %d",
                    m_control_point_count, m_segment_count);
    }
#endif

    void update() {
        for (ScalarIndex i = 0; i < m_control_point_count; i++) {
            Float x = dr::gather<Float>(m_vertex, i * 3 + 0);
            Float y = dr::gather<Float>(m_vertex, i * 3 + 1);
            Float z = dr::gather<Float>(m_vertex, i * 3 + 2);
            Float r = dr::gather<Float>(m_radius, i);

            dr::scatter(m_vertex_with_radius, x, UInt32(i) * 4 + 0);
            dr::scatter(m_vertex_with_radius, y, UInt32(i) * 4 + 1);
            dr::scatter(m_vertex_with_radius, z, UInt32(i) * 4 + 2);
            dr::scatter(m_vertex_with_radius, r, UInt32(i) * 4 + 3);
            dr::eval(m_vertex_with_radius);
        }
    }

    bool is_curve() const override {
        return true;
    }

    bool is_linear_curve() const override {
        return true;
    }

    ScalarBoundingBox3f bbox() const override {
        return m_bbox;
    }


    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LinearCurve[" << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    ScalarBoundingBox3f m_bbox;

    ScalarSize m_control_point_count = 0;
    ScalarSize m_segment_count = 0;

    mutable FloatStorage m_vertex_with_radius;
    mutable UInt32Storage m_indices;

    // separate storage of control points and per-vertex radius for OptiX
    mutable FloatStorage m_vertex;
    mutable FloatStorage m_radius;

    // for OptiX build input
    mutable void* m_vertex_buffer_ptr = nullptr;
    mutable void* m_radius_buffer_ptr = nullptr;
    mutable void* m_index_buffer_ptr = nullptr;

    // texture used to compute surface normal
    // dr::Texture<Float, 1> m_tex;
    // dr::Texture<Float, 1> m_tex_r;
};

MI_IMPLEMENT_CLASS_VARIANT(LinearCurve, Shape)
MI_EXPORT_PLUGIN(LinearCurve, "Linear curve intersection primitive");
NAMESPACE_END(mitsuba)

