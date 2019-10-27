#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/records.h>
#include <mutex>

#if defined(MTS_USE_EMBREE)
    #include <embree3/rtcore.h>
#endif

#if defined(MTS_ENABLE_AUTODIFF)
    #include <optix.h>
    #include <optix_cuda_interop.h>
#endif


NAMESPACE_BEGIN(mitsuba)

Mesh::Mesh(const Properties &props) : Shape(props) {
    /* When set to ``true``, Mitsuba will use per-face instead of per-vertex
       normals when rendering the object, which will give it a faceted
       appearance. Default: ``false`` */
    if (props.bool_("face_normals", false))
        m_disable_vertex_normals = true;
    m_to_world = props.transform("to_world", Transform4f());
    m_mesh = true;
}

Mesh::Mesh(const std::string &name,
           Struct *vertex_struct, Size vertex_count,
           Struct *face_struct, Size face_count)
    : m_name(name), m_vertex_count(vertex_count),
      m_face_count(face_count), m_vertex_struct(vertex_struct),
      m_face_struct(face_struct) {
    /* Helper lambda function to determine compatibility (offset/type) of a 'Struct' field */
    auto check_field = [](const Struct *s, size_t idx,
                          const std::string &suffix_exp,
                          Struct::EType type_exp) {
        if (idx >= s->field_count())
            Throw("Mesh::Mesh(): Incompatible data structure %s", s->to_string());
        auto field = s->operator[](idx);
        std::string suffix = field.name;
        auto it = suffix.rfind(".");
        if (it != std::string::npos)
            suffix = suffix.substr(it + 1);
        if (suffix != suffix_exp || field.type != type_exp)
            Throw("Mesh::Mesh(): Incompatible data structure %s", s->to_string());
    };

    check_field(vertex_struct, 0, "x",  Struct::EFloat);
    check_field(vertex_struct, 1, "y",  Struct::EFloat);
    check_field(vertex_struct, 2, "z",  Struct::EFloat);

    check_field(face_struct,   0, "i0", struct_traits<Index>::value);
    check_field(face_struct,   1, "i1", struct_traits<Index>::value);
    check_field(face_struct,   2, "i2", struct_traits<Index>::value);

    if (vertex_struct->has_field("nx") &&
        vertex_struct->has_field("ny") &&
        vertex_struct->has_field("nz")) {
        check_field(vertex_struct, 3, "nx", Struct::EFloat);
        check_field(vertex_struct, 4, "ny", Struct::EFloat);
        check_field(vertex_struct, 5, "nz", Struct::EFloat);
        m_normal_offset = (Index) vertex_struct->field("nx").offset;
    }

    if (vertex_struct->has_field("u") && vertex_struct->has_field("v")) {
        if (m_normal_offset == 0) {
            check_field(vertex_struct, 3, "u", Struct::EFloat);
            check_field(vertex_struct, 4, "v", Struct::EFloat);
        } else {
            check_field(vertex_struct, 6, "u", Struct::EFloat);
            check_field(vertex_struct, 7, "v", Struct::EFloat);
        }
        m_texcoord_offset = (Index) vertex_struct->field("u").offset;
    }

    m_vertex_size = (Size) m_vertex_struct->size();
    m_face_size = (Size) m_face_struct->size();

    m_vertices = VertexHolder(new uint8_t[(vertex_count + 1) * m_vertex_size]);
    m_faces = VertexHolder(new uint8_t[(face_count + 1) * m_face_size]);

    m_mesh = true;
}

Mesh::~Mesh() { }

void Mesh::write(Stream *) const {
    NotImplementedError("write");
}

BoundingBox3f Mesh::bbox() const {
    return m_bbox;
}

BoundingBox3f Mesh::bbox(Index index) const {
    Assert(index <= m_face_count);

    auto idx = (const Index *) face(index);
    Assert(idx[0] < m_vertex_count &&
           idx[1] < m_vertex_count &&
           idx[2] < m_vertex_count);

    Point3f v0 = vertex_position(idx[0]),
            v1 = vertex_position(idx[1]),
            v2 = vertex_position(idx[2]);

    return BoundingBox3f(
        min(min(v0, v1), v2),
        max(max(v0, v1), v2)
    );
}

void Mesh::recompute_vertex_normals() {
    if (!has_vertex_normals())
        Throw("Storing new normals in a Mesh that didn't have normals at "
              "construction time is not implemented yet.");

    std::vector<Normal3f> normals(m_vertex_count, zero<Normal3f>());
    size_t invalid_counter = 0;
    Timer timer;

    /* Weighting scheme based on "Computing Vertex Normals from Polygonal Facets"
       by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
    for (Size i = 0; i < m_face_count; ++i) {
        const Index *idx = (const Index *) face(i);
        Assert(idx[0] < m_vertex_count && idx[1] < m_vertex_count && idx[2] < m_vertex_count);
        Point3f v[3]{ vertex_position(idx[0]),
                      vertex_position(idx[1]),
                      vertex_position(idx[2]) };

        Vector3f side_0 = v[1] - v[0], side_1 = v[2] - v[0];
        Normal3f n = cross(side_0, side_1);
        Float length_sqr = squared_norm(n);
        if (likely(length_sqr > 0)) {
            n *= rsqrt<Vector3f::Approx>(length_sqr);

            // Use Enoki to compute the face angles at the same time
            auto side1 = transpose(Array<Packet<float, 3>, 3>{ side_0, v[2] - v[1], v[0] - v[2] });
            auto side2 = transpose(Array<Packet<float, 3>, 3>{ side_1, v[0] - v[1], v[1] - v[2] });
            Vector3f face_angles = unit_angle(normalize(side1), normalize(side2));

            for (size_t j = 0; j < 3; ++j)
                normals[idx[j]] += n * face_angles[j];
        }
    }

    for (Size i = 0; i < m_vertex_count; i++) {
        Normal3f n = normals[i];
        Float length = norm(n);
        if (likely(length != 0.f)) {
            n /= length;
        } else {
            n = Normal3f(1, 0, 0); // Choose some bogus value
            invalid_counter++;
        }

        store(vertex(i) + m_normal_offset, n);
    }

    if (invalid_counter == 0)
        Log(Debug, "\"%s\": computed vertex normals (took %s)", m_name,
            util::time_string(timer.value()));
    else
        Log(Warn, "\"%s\": computed vertex normals (took %s, %i invalid vertices!)",
            m_name, util::time_string(timer.value()), invalid_counter);
}

void Mesh::recompute_bbox() {
    m_bbox.reset();
    for (Size i = 0; i < m_vertex_count; ++i)
        m_bbox.expand(vertex_position(i));
}

void Mesh::prepare_sampling_table() {
    if (m_face_count == 0)
        Throw("Cannot create sampling table for an empty mesh: %s", to_string());

    std::lock_guard<tbb::spin_mutex> lock(m_mutex);
    if (m_surface_area < 0) {
        // Generate a PDF for sampling wrt. the area of each face.
        m_area_distribution.reserve(m_face_count);
        for (Index i = 0; i < m_face_count; i++) {
            Float area = face_area(i);
            Assert(area >= 0);
            m_area_distribution.append(area);
        }
        m_surface_area = m_area_distribution.normalize();
        m_inv_surface_area = 1.f / m_surface_area;
    }
}

Mesh::Size Mesh::primitive_count() const {
    return face_count();
}

Float Mesh::surface_area() const {
    ensure_table_ready();
    return m_surface_area;
}

template <typename Value, typename Point2, typename Point3,
          typename PositionSample, typename Mask>
PositionSample Mesh::sample_position_impl(Value time, Point2 sample,
                                          Mask active) const {
    using Index   = replace_scalar_t<Value, Shape::Index>;
    using Index3  = Array<Index, 3>;
    using Normal3 = Normal<Value, 3>;
    Index face_idx;

    ensure_table_ready();
    std::tie(face_idx, sample.y()) =
        m_area_distribution.sample_reuse(sample.y(), active);

    Index3 fi = face_indices(face_idx, active);

    Point3 p0 = vertex_position(fi[0], active),
           p1 = vertex_position(fi[1], active),
           p2 = vertex_position(fi[2], active);

    vector3_t<Point3> e0 = p1 - p0, e1 = p2 - p0;
    Point2 b = warp::square_to_uniform_triangle(sample);

    PositionSample ps;
    ps.p     = p0 + e0 * b.x() + e1 * b.y();
    ps.time  = time;
    ps.pdf   = m_inv_surface_area;
    ps.delta = false;

    if (has_vertex_texcoords()) {
        Point2 uv0 = vertex_texcoord(fi[0], active),
               uv1 = vertex_texcoord(fi[1], active),
               uv2 = vertex_texcoord(fi[2], active);
        ps.uv = uv0 * (1.f - b.x() - b.y())
              + uv1 * b.x() + uv2 * b.y();
    } else {
        ps.uv = b;
    }

    if (has_vertex_normals()) {
        Normal3 n0 = vertex_normal(fi[0], active),
                n1 = vertex_normal(fi[1], active),
                n2 = vertex_normal(fi[2], active);
        ps.n = normalize(n0 * (1.f - b.x() - b.y())
                       + n1 * b.x() + n2 * b.y());
    } else {
        ps.n = normalize(cross(e0, e1));
    }

    return ps;
}

PositionSample3f Mesh::sample_position(Float time, const Point2f &sample) const {
    return sample_position_impl(time, sample, true);
}

PositionSample3fP Mesh::sample_position(FloatP time,
                                        const Point2fP &sample,
                                        MaskP active) const {
    return sample_position_impl(time, sample, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
PositionSample3fD Mesh::sample_position(FloatD time,
                                        const Point2fD &sample,
                                        MaskD active) const {
    return sample_position_impl(time, sample, active);
}
#endif

template <typename PositionSample, typename Value, typename Mask>
Value Mesh::pdf_position_impl(const PositionSample &, Mask) const {
    ensure_table_ready();
    return m_inv_surface_area;
}

Float Mesh::pdf_position(const PositionSample3f &ps) const {
    return pdf_position_impl(ps, true);
}

FloatP Mesh::pdf_position(const PositionSample3fP &ps, MaskP active) const {
    return pdf_position_impl(ps, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
FloatD Mesh::pdf_position(const PositionSample3fD &ps, MaskD active) const {
    return pdf_position_impl(ps, active);
}
#endif

template <typename Ray, typename Value, typename Point3,
          typename SurfaceInteraction, typename Mask>
void Mesh::fill_surface_interaction_impl(const Ray &ray, const Value *cache,
                                         SurfaceInteraction &si,
                                         Mask active) const {
    using Point2 = point2_t<Point3>;
    using Normal3 = normal3_t<Point3>;
    using Vector3 = vector3_t<Point3>;
    using Vector2 = vector2_t<Point3>;

    ENOKI_MARK_USED(ray);

    // Barycentric coordinates within triangle
    Value b1 = cache[0],
          b2 = cache[1];

    Value b0 = 1.f - b1 - b2;

    auto fi = face_indices(si.prim_index, active);

    Point3 p0 = vertex_position(fi[0], active),
           p1 = vertex_position(fi[1], active),
           p2 = vertex_position(fi[2], active);

    Vector3 dp0 = p1 - p0,
            dp1 = p2 - p0;

    // Re-interpolate intersection using barycentric coordinates
    si.p[active] = p0 * b0 + p1 * b1 + p2 * b2;

    // Face normal
    Normal3 n = normalize(cross(dp0, dp1));
    si.n[active] = n;

    // Texture coordinates (if available)
    auto [dp_du, dp_dv] = coordinate_system(n);
    Point2 uv(b1, b2);
    if (has_vertex_texcoords()) {
        Point2 uv0 = vertex_texcoord(fi[0], active),
               uv1 = vertex_texcoord(fi[1], active),
               uv2 = vertex_texcoord(fi[2], active);

        uv = uv0 * b0 + uv1 * b1 + uv2 * b2;

        Vector2 duv0 = uv1 - uv0,
                duv1 = uv2 - uv0;

        Value det     = fmsub(duv0.x(), duv1.y(), duv0.y() * duv1.x()),
              inv_det = rcp(det);

        Mask valid = neq(det, 0.f);

        dp_du[valid] = fmsub( duv1.y(), dp0, duv0.y() * dp1) * inv_det;
        dp_dv[valid] = fnmadd(duv1.x(), dp0, duv0.x() * dp1) * inv_det;
    }
    si.uv[active] = uv;

    // Shading normal (if available)
    if (has_vertex_normals()) {
        Normal3 n0 = vertex_normal(fi[0], active),
                n1 = vertex_normal(fi[1], active),
                n2 = vertex_normal(fi[2], active);

        n = normalize(n0 * b0 + n1 * b1 + n2 * b2);
    }

    si.sh_frame.n[active] = n;

    // Tangents
    si.dp_du[active] = dp_du;
    si.dp_dv[active] = dp_dv;
}

void Mesh::fill_surface_interaction(const Ray3f &ray,
                                    const Float *cache,
                                    SurfaceInteraction3f &si) const {
    fill_surface_interaction_impl(ray, cache, si, true);
}

void Mesh::fill_surface_interaction(const Ray3fP &ray,
                                    const FloatP *cache,
                                    SurfaceInteraction3fP &si,
                                    MaskP active) const {
    fill_surface_interaction_impl(ray, cache, si, active);
}


template <typename SurfaceInteraction, typename Value, typename Vector3,
          typename Mask>
std::pair<Vector3, Vector3>
Mesh::normal_derivative_impl(const SurfaceInteraction &si,
                             bool shading_frame, Mask active) const {
    using Point3  = typename SurfaceInteraction::Point3;
    using Normal3 = typename SurfaceInteraction::Normal3;

    Assert(has_vertex_normals());

    if (!shading_frame)
        return { zero<Vector3>(), zero<Vector3>() };

    auto fi = face_indices(si.prim_index, active);

    Point3 p0 = vertex_position(fi[0], active),
           p1 = vertex_position(fi[1], active),
           p2 = vertex_position(fi[2], active);

    Normal3 n0 = vertex_normal(fi[0], active),
            n1 = vertex_normal(fi[1], active),
            n2 = vertex_normal(fi[2], active);

    Vector3 rel = si.p - p0,
            du  = p1 - p0,
            dv  = p2 - p0;

    /* Solve a least squares problem to determine
       the UV coordinates within the current triangle */
    Value b1  = dot(du, rel), b2 = dot(dv, rel),
          a11 = dot(du, du), a12 = dot(du, dv),
          a22 = dot(dv, dv),
          inv_det = rcp(a11 * a22 - a12 * a12);

    Value u = fmsub (a22, b1, a12 * b2) * inv_det,
          v = fnmadd(a12, b1, a11 * b2) * inv_det,
          w = 1.f - u - v;

    /* Now compute the derivative of "normalize(u*n1 + v*n2 + (1-u-v)*n0)"
       with respect to [u, v] in the local triangle parameterization.

       Since d/du [f(u)/|f(u)|] = [d/du f(u)]/|f(u)|
         - f(u)/|f(u)|^3 <f(u), d/du f(u)>, this results in
    */

    Normal3 N(u * n1 + v * n2 + w * n0);
    Value il = rsqrt(squared_norm(N));
    N *= il;

    Vector3 dndu = (n1 - n0) * il;
    Vector3 dndv = (n2 - n0) * il;

    dndu = fnmadd(N, dot(N, dndu), dndu);
    dndv = fnmadd(N, dot(N, dndv), dndv);

    return { dndu, dndv };
}

std::pair<Vector3f, Vector3f>
Mesh::normal_derivative(const SurfaceInteraction3f &si, bool shading_frame) const {
    return normal_derivative_impl(si, shading_frame, true);
}

std::pair<Vector3fP, Vector3fP>
Mesh::normal_derivative(const SurfaceInteraction3fP &si, bool shading_frame,
                        MaskP active) const {
    return normal_derivative_impl(si, shading_frame, active);
}

namespace {
constexpr size_t max_vertices = 10;

size_t sutherland_hodgman(Point3d *input, size_t in_count, Point3d *output,
                          int axis, double split_pos, bool is_minimum) {
    if (in_count < 3)
        return 0;

    Point3d cur        = input[0];
    double sign        = is_minimum ? 1.0 : -1.0;
    double distance    = sign * (cur[axis] - split_pos);
    bool  cur_is_inside = (distance >= 0);
    size_t out_count    = 0;

    for (size_t i = 0; i < in_count; ++i) {
        size_t next_idx = i + 1;
        if (next_idx == in_count)
            next_idx = 0;

        Point3d next = input[next_idx];
        distance = sign * (next[axis] - split_pos);
        bool next_is_inside = (distance >= 0);

        if (cur_is_inside && next_is_inside) {
            /* Both this and the next vertex are inside, add to the list */
            Assert(out_count + 1 < max_vertices);
            output[out_count++] = next;
        } else if (cur_is_inside && !next_is_inside) {
            /* Going outside -- add the intersection */
            double t = (split_pos - cur[axis]) / (next[axis] - cur[axis]);
            Assert(out_count + 1 < max_vertices);
            Point3d p = cur + (next - cur) * t;
            p[axis] = split_pos; // Avoid roundoff errors
            output[out_count++] = p;
        } else if (!cur_is_inside && next_is_inside) {
            /* Coming back inside -- add the intersection + next vertex */
            double t = (split_pos - cur[axis]) / (next[axis] - cur[axis]);
            Assert(out_count + 2 < max_vertices);
            Point3d p = cur + (next - cur) * t;
            p[axis] = split_pos; // Avoid roundoff errors
            output[out_count++] = p;
            output[out_count++] = next;
        } else {
            /* Entirely outside - do not add anything */
        }
        cur = next;
        cur_is_inside = next_is_inside;
    }
    return out_count;
}
}  // end namespace

BoundingBox3f Mesh::bbox(Index index, const BoundingBox3f &clip) const {
    // Reserve room for some additional vertices
    Point3d vertices1[max_vertices], vertices2[max_vertices];
    size_t nVertices = 3;

    Assert(index <= m_face_count);

    auto idx = (const Index *) face(index);
    Assert(idx[0] < m_vertex_count);
    Assert(idx[1] < m_vertex_count);
    Assert(idx[2] < m_vertex_count);

    Point3f v0 = vertex_position(idx[0]),
            v1 = vertex_position(idx[1]),
            v2 = vertex_position(idx[2]);

    /* The kd-tree code will frequently call this function with
       almost-collapsed bounding boxes. It's extremely important not to
       introduce errors in such cases, otherwise the resulting tree will
       incorrectly remove triangles from the associated nodes. Hence, do
       the following computation in double precision! */

    vertices1[0] = Point3d(v0);
    vertices1[1] = Point3d(v1);
    vertices1[2] = Point3d(v2);

    for (int axis = 0; axis < 3; ++axis) {
        nVertices = sutherland_hodgman(vertices1, nVertices, vertices2, axis,
                                      (double) clip.min[axis], true);
        nVertices = sutherland_hodgman(vertices2, nVertices, vertices1, axis,
                                      (double) clip.max[axis], false);
    }

    BoundingBox3f result;
    for (size_t i = 0; i < nVertices; ++i) {
        if (std::is_same<Float, float>::value) {
            /* Convert back into floats (with conservative custom rounding modes) */
            Array<double, 3, false, RoundingMode::Up>   p1_up  (vertices1[i]);
            Array<double, 3, false, RoundingMode::Down> p1_down(vertices1[i]);
            result.min = min(result.min, Point3f(p1_down));
            result.max = max(result.max, Point3f(p1_up));
        } else {
            result.min = min(result.min, Point3f(prev_float(vertices1[i])));
            result.max = max(result.max, Point3f(next_float(vertices1[i])));
        }
    }
    result.clip(clip);

    return result;
}

std::string Mesh::to_string() const {
    std::ostringstream oss;
    oss << class_()->name() << "[" << std::endl
        << "  name = \"" << m_name << "\"," << std::endl
        << "  bbox = " << string::indent(m_bbox) << "," << std::endl
        << "  vertex_struct = " << string::indent(m_vertex_struct) << "," << std::endl
        << "  vertex_count = " << m_vertex_count << "," << std::endl
        << "  vertices = [" << util::mem_string(m_vertex_size * m_vertex_count) << " of vertex data]," << std::endl
        << "  face_struct = " << string::indent(m_face_struct) << "," << std::endl
        << "  face_count = " << m_face_count << "," << std::endl
        << "  faces = [" << util::mem_string(m_face_size * m_face_count) << " of face data]," << std::endl
        << "  disable_vertex_normals = " << m_disable_vertex_normals << "," << std::endl
        << "  surface_area = " << m_surface_area << std::endl
        << "]";
    return oss.str();
}

#if defined(MTS_USE_EMBREE)
RTCGeometry Mesh::embree_geometry(RTCDevice device) const {
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, m_vertices.get(),
            0, m_vertex_size, m_vertex_count);
    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, m_faces.get(),
            0, m_face_size, m_face_count);

    rtcCommitGeometry(geom);
    return geom;
}
#endif

#if defined(MTS_ENABLE_AUTODIFF)
#define rt_check(err)  __rt_check (m_optix_context, err, __FILE__, __LINE__)

static void __rt_check(RTcontext context, RTresult errval, const char *file,
                       const int line) {
    if (errval != RT_SUCCESS) {
        const char *message;
        rtContextGetErrorString(context, errval, &message);
        fprintf(stderr,
                "rt_check(): OptiX API error = %04d (%s) in "
                "%s:%i.\n", (int) errval, message, file, line);
        exit(EXIT_FAILURE);
    }
}

void Mesh::put_parameters(DifferentiableParameters &dp) {
    dp.put(this, "vertex_positions", m_vertex_positions_d);
    if (has_vertex_texcoords())
        dp.put(this, "vertex_texcoords", m_vertex_texcoords_d);
}

void Mesh::parameters_changed() {
    UInt32D vertex_range   = arange<UInt32D>(m_vertex_count),
            vertex_range_2 = vertex_range * 2,
            vertex_range_3 = vertex_range * 3,
            face_range_3   = arange<UInt32D>(m_face_count) * 3;

    Index *faces_ptr = nullptr;
    rt_check(rtBufferGetDevicePointer(m_optix_faces_buf, 0, (void **) &faces_ptr));
    for (size_t i = 0; i < 3; ++i)
        scatter(faces_ptr + i, m_faces_d[i], face_range_3);

    float *vertex_positions_ptr = nullptr;
    rt_check(rtBufferGetDevicePointer(m_optix_vertex_positions_buf, 0,
                                      (void **) &vertex_positions_ptr));

    for (size_t i = 0; i < 3; ++i)
        scatter(vertex_positions_ptr + i, m_vertex_positions_d[i], vertex_range_3);

    if (has_vertex_texcoords()) {
        float *vertex_texcoords_ptr = nullptr;
        rt_check(rtBufferGetDevicePointer(m_optix_vertex_texcoords_buf, 0,
                                          (void **) &vertex_texcoords_ptr));
        for (size_t i = 0; i < 2; ++i)
            scatter(vertex_texcoords_ptr + i, m_vertex_texcoords_d[i], vertex_range_2);
    }

    if (has_vertex_normals()) {
        if (requires_gradient(m_vertex_positions_d)) {
            m_vertex_normals_d = zero<Vector3fD>(m_vertex_count);
            Vector3fD v[3] = {
                gather<Vector3fD>(m_vertex_positions_d, m_faces_d.x()),
                gather<Vector3fD>(m_vertex_positions_d, m_faces_d.y()),
                gather<Vector3fD>(m_vertex_positions_d, m_faces_d.z())
            };
            Normal3fD n = normalize(cross(v[1] - v[0], v[2] - v[0]));

            /* Weighting scheme based on "Computing Vertex Normals from Polygonal Facets"
               by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
            for (int i = 0; i < 3; ++i) {
                Vector3fD d0 = normalize(v[(i + 1) % 3] - v[i]);
                Vector3fD d1 = normalize(v[(i + 2) % 3] - v[i]);
                FloatD face_angle = safe_acos(dot(d0, d1));
                scatter_add(m_vertex_normals_d, n * face_angle, m_faces_d[i]);
            }

            m_vertex_normals_d = normalize(m_vertex_normals_d);
        }

        float *vertex_normals_ptr = nullptr;
        rt_check(rtBufferGetDevicePointer(m_optix_vertex_normals_buf, 0,
                                          (void **) &vertex_normals_ptr));
        for (size_t i = 0; i < 3; ++i)
            scatter(vertex_normals_ptr + i, m_vertex_normals_d[i], vertex_range_3);
    }

    if (m_optix_geometry_ready) {
        // Mark acceleration data structure dirty
        RTcontext context;
        RTvariable accel_var;
        RTacceleration accel;
        rt_check(rtGeometryTrianglesGetContext(m_optix_geometry, &context));
        rt_check(rtContextQueryVariable(context, "accel", &accel_var));
        rt_check(rtVariableGetUserData(accel_var, sizeof(void *), (void *) &accel));
        rt_check(rtAccelerationMarkDirty(accel));
    }
}

template <typename Value, size_t Dim, typename Func,
          typename Result = Array<DiffArray<CUDAArray<Value>>, Dim>>
Result cuda_upload(size_t size, Func func) {
    Value *tmp = (Value *) cuda_host_malloc(size * Dim * sizeof(Value));

    for (size_t i = 0; i < size; ++i) {
        auto value = func(i);
        for (size_t j = 0; j < Dim; ++j)
            tmp[i + j * size] = value[j];
    }

    Result result;
    for (size_t j = 0; j < Dim; ++j) {
        void *dst = cuda_malloc(size * sizeof(Value));
        cuda_memcpy_to_device_async(dst, tmp + j * size,
                                    size * sizeof(Value));
        result[j] = CUDAArray<Value>::map(dst, size, true);
    }

    cuda_host_free(tmp);

    return result;
}

RTgeometrytriangles Mesh::optix_geometry(RTcontext context) {
    if (m_optix_geometry != nullptr)
        throw std::runtime_error("OptiX geometry was already created!");
    m_optix_context = context;

    /// Face indices
    rt_check(rtBufferCreate(context, RT_BUFFER_INPUT, &m_optix_faces_buf));
    rt_check(rtBufferSetFormat(m_optix_faces_buf, RT_FORMAT_UNSIGNED_INT3));
    rt_check(rtBufferSetSize1D(m_optix_faces_buf, m_face_count));
    m_faces_d = cuda_upload<Index, 3>(
        m_face_count, [this](size_t i) { return face_indices(i); });

    // Vertex positions
    rt_check(rtBufferCreate(context, RT_BUFFER_INPUT, &m_optix_vertex_positions_buf));
    rt_check(rtBufferSetFormat(m_optix_vertex_positions_buf, RT_FORMAT_FLOAT3));
    rt_check(rtBufferSetSize1D(m_optix_vertex_positions_buf, m_vertex_count));
    m_vertex_positions_d = cuda_upload<float, 3>(
        m_vertex_count, [this](size_t i) { return vertex_position(i); });

    // Vertex texture coordinates
    rt_check(rtBufferCreate(context, RT_BUFFER_INPUT, &m_optix_vertex_texcoords_buf));
    rt_check(rtBufferSetFormat(m_optix_vertex_texcoords_buf, RT_FORMAT_FLOAT2));
    rt_check(rtBufferSetSize1D(
        m_optix_vertex_texcoords_buf, has_vertex_texcoords() ? m_vertex_count : 0));
    if (has_vertex_texcoords())
        m_vertex_texcoords_d = cuda_upload<float, 2>(
            m_vertex_count, [this](size_t i) { return vertex_texcoord(i); });

    // Vertex normals
    rt_check(rtBufferCreate(context, RT_BUFFER_INPUT, &m_optix_vertex_normals_buf));
    rt_check(rtBufferSetFormat(m_optix_vertex_normals_buf, RT_FORMAT_FLOAT3));
    rt_check(rtBufferSetSize1D(
        m_optix_vertex_normals_buf, has_vertex_normals() ? m_vertex_count : 0));
    if (has_vertex_normals())
        m_vertex_normals_d = cuda_upload<float, 3>(
            m_vertex_count, [this](size_t i) { return vertex_normal(i); });

    rt_check(rtGeometryTrianglesCreate(context, &m_optix_geometry));
    rt_check(rtGeometryTrianglesSetPrimitiveCount(m_optix_geometry, m_face_count));

    parameters_changed();

    rt_check(rtGeometryTrianglesSetTriangleIndices(
        m_optix_geometry, m_optix_faces_buf, 0, 3 * sizeof(unsigned int),
        RT_FORMAT_UNSIGNED_INT3));

    rt_check(rtGeometryTrianglesSetVertices(
        m_optix_geometry, m_vertex_count, m_optix_vertex_positions_buf, 0,
        3 * sizeof(float), RT_FORMAT_FLOAT3));

    RTvariable faces_var, vertex_positions_var, vertex_normals_var, vertex_texcoords_var;
    rt_check(rtGeometryTrianglesDeclareVariable(m_optix_geometry, "faces", &faces_var));
    rt_check(rtGeometryTrianglesDeclareVariable(m_optix_geometry, "vertex_positions", &vertex_positions_var));
    rt_check(rtGeometryTrianglesDeclareVariable(m_optix_geometry, "vertex_normals", &vertex_normals_var));
    rt_check(rtGeometryTrianglesDeclareVariable(m_optix_geometry, "vertex_texcoords", &vertex_texcoords_var));

    rt_check(rtVariableSetObject(faces_var, m_optix_faces_buf));
    rt_check(rtVariableSetObject(vertex_positions_var, m_optix_vertex_positions_buf));
    rt_check(rtVariableSetObject(vertex_normals_var, m_optix_vertex_normals_buf));
    rt_check(rtVariableSetObject(vertex_texcoords_var, m_optix_vertex_texcoords_buf));

    rt_check(rtGeometryTrianglesSetMaterialCount(m_optix_geometry, 1));
    rt_check(rtGeometryTrianglesSetFlagsPerMaterial(m_optix_geometry, 0, RT_GEOMETRY_FLAG_DISABLE_ANYHIT));

    m_optix_geometry_ready = true;

    return m_optix_geometry;
}
#endif

MTS_IMPLEMENT_CLASS(Mesh, Shape)
NAMESPACE_END(mitsuba)
