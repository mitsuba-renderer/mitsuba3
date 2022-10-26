#pragma once

#include <mitsuba/core/frame.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/mueller.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief This list of flags is used to determine which members of \ref SurfaceInteraction
 * should be computed when calling \ref compute_surface_interaction().
 *
 * It also specifies whether the \ref SurfaceInteraction should be differentiable with
 * respect to the shapes parameters.
 */
enum class RayFlags : uint32_t {

    // =============================================================
    //             Surface interaction compute flags
    // =============================================================

    /// No flags set
    Empty = 0x0,

    /// Compute position and geometric normal
    Minimal = 0x1,

    /// Compute UV coordinates
    UV = 0x2,

    /// Compute position partials wrt. UV coordinates
    dPdUV = 0x4,

    /// Compute shading normal and shading frame
    ShadingFrame = 0x8,

    /// Compute the geometric normal partials wrt. the UV coordinates
    dNGdUV = 0x10,

    /// Compute the shading normal partials wrt. the UV coordinates
    dNSdUV = 0x20,

    /// Compute the boundary-test used in reparameterized integrators
    BoundaryTest = 0x40,

    // =============================================================
    //!              Differentiability compute flags
    // =============================================================

    /// Derivatives of the SurfaceInteraction fields follow shape's motion
    FollowShape = 0x80,

    /// Derivatives of the SurfaceInteraction fields ignore shape's motion
    DetachShape = 0x100,

    // =============================================================
    //!                 Compound compute flags
    // =============================================================

    /* \brief Default: compute all fields of the surface interaction data
       structure except shading/geometric normal derivatives */
    All = UV | dPdUV | ShadingFrame,

    /// Compute all fields of the surface interaction ignoring shape's motion
    AllNonDifferentiable = All | DetachShape,
};

MI_DECLARE_ENUM_OPERATORS(RayFlags)

// -----------------------------------------------------------------------------

/// Generic surface interaction data structure
template <typename Float_, typename Spectrum_>
struct Interaction {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Float    = Float_;
    using Spectrum = Spectrum_;
    MI_IMPORT_RENDER_BASIC_TYPES()
    MI_IMPORT_OBJECT_TYPES()

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Distance traveled along the ray
    Float t = dr::Infinity<Float>;

    /// Time value associated with the interaction
    Float time;

    /// Wavelengths associated with the ray that produced this interaction
    Wavelength wavelengths;

    /// Position of the interaction in world coordinates
    Point3f p;

    /// Geometric normal (only valid for \c SurfaceInteraction)
    Normal3f n;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /// Constructor
    Interaction(Float t, Float time, const Wavelength &wavelengths,
                const Point3f &p, const Normal3f &n = 0.f)
        : t(t), time(time), wavelengths(wavelengths), p(p), n(n) { }

    /**
     * This callback method is invoked by dr::zeros<>, and takes care of fields that deviate
     * from the standard zero-initialization convention. In this particular class, the ``t``
     * field should be set to an infinite value to mark invalid intersection records.
     */
    void zero_(size_t size = 1) {
        t = dr::full<Float>(dr::Infinity<Float>, size);
    }

    /// Is the current interaction valid?
    Mask is_valid() const {
        return dr::neq(t, dr::Infinity<Float>);
    }

    /// Spawn a semi-infinite ray towards the given direction
    Ray3f spawn_ray(const Vector3f &d) const {
        return Ray3f(offset_p(d), d, dr::Largest<Float>, time, wavelengths);
    }

    /// Spawn a finite ray towards the given position
    Ray3f spawn_ray_to(const Point3f &t) const {
        Point3f o = offset_p(t - p);
        Vector3f d = t - o;
        Float dist = dr::norm(d);
        d /= dist;
        return Ray3f(o, d, dist * (1.f - math::ShadowEpsilon<Float>), time,
                     wavelengths);
    }

    //! @}
    // =============================================================

    DRJIT_STRUCT(Interaction, t, time, wavelengths, p, n);

private:
    /**
     * Compute an offset position, used when spawning a ray from this
     * interaction. When the interaction is on the surface of a shape, the
     * position is offset along the surface normal to prevent self intersection.
     */
    Point3f offset_p(const Vector3f &d) const {
        Float mag = (1.f + dr::max(dr::abs(p))) * math::RayEpsilon<Float>;
        mag = dr::detach(dr::mulsign(mag, dr::dot(n, d)));
        return dr::fmadd(mag, dr::detach(n), p);
    }
};

// -----------------------------------------------------------------------------

template <typename Float_, typename Shape_>
struct PreliminaryIntersection;

/// Stores information related to a surface scattering interaction
template <typename Float_, typename Spectrum_>
struct SurfaceInteraction : Interaction<Float_, Spectrum_> {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Float    = Float_;
    using Spectrum = Spectrum_;

    // Make parent fields/functions visible
    MI_IMPORT_BASE(Interaction, t, time, wavelengths, p, n, is_valid)

    MI_IMPORT_RENDER_BASIC_TYPES()
    MI_IMPORT_OBJECT_TYPES()

    using Index            = typename CoreAliases::UInt32;
    using PositionSample3f = typename RenderAliases::PositionSample3f;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Pointer to the associated shape
    ShapePtr shape = nullptr;

    /// UV surface coordinates
    Point2f uv;

    /// Shading frame
    Frame3f sh_frame;

    /// Position partials wrt. the UV parameterization
    Vector3f dp_du, dp_dv;

    /// Normal partials wrt. the UV parameterization
    Vector3f dn_du, dn_dv;

    /// UV partials wrt. changes in screen-space
    Vector2f duv_dx, duv_dy;

    /// Incident direction in the local shading frame
    Vector3f wi;

    /// Primitive index, e.g. the triangle ID (if applicable)
    Index prim_index;

    /// Stores a pointer to the parent instance (if applicable)
    ShapePtr instance = nullptr;

    /**
     * Boundary-test value used in reparameterized integrators, a soft indicator
     * function which returns a zero value at the silhouette of the shape from
     * the perspective of a given ray. Everywhere else this function will return
     * non-negative values reflecting the distance of the surface interaction to
     * this closest point on the silhouette.
     */
    Float boundary_test;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /**
     * Construct from a position sample.
     * Unavailable fields such as `wi` and the partial derivatives are left
     * uninitialized.
     * The `shape` pointer is left uninitialized because we can't guarantee that
     * the given \ref PositionSample::object points to a Shape instance.
     */
    explicit SurfaceInteraction(const PositionSample3f &ps,
                                const Wavelength &wavelengths)
        : Base(0.f, ps.time, wavelengths, ps.p, ps.n), uv(ps.uv),
          sh_frame(Frame3f(ps.n)), dp_du(0), dp_dv(0), dn_du(0), dn_dv(0),
          duv_dx(0), duv_dy(0), wi(0), prim_index(0), boundary_test(0) {}

    /// Initialize local shading frame using Gram-schmidt orthogonalization
    void initialize_sh_frame() {
        sh_frame.s = dr::normalize(
            dr::fmadd(sh_frame.n, -dr::dot(sh_frame.n, dp_du), dp_du));

        // When dp_du is invalid, use an orthonormal basis
        Mask singularity_mask = dr::all(dr::eq(dp_du, 0.f));
        if (unlikely(dr::any_or<true>(singularity_mask)))
            sh_frame.s[singularity_mask] = coordinate_system(sh_frame.n).first;

        sh_frame.t = dr::cross(sh_frame.n, sh_frame.s);
    }

    /// Convert a local shading-space vector into world space
    Vector3f to_world(const Vector3f &v) const {
        return sh_frame.to_world(v);
    }

    /// Convert a world-space vector into local shading coordinates
    Vector3f to_local(const Vector3f &v) const {
        return sh_frame.to_local(v);
    }

    /**
     * Return the emitter associated with the intersection (if any)
     * \note Defined in scene.h
     */
    EmitterPtr emitter(const Scene *scene, Mask active = true) const;

    /// Is the intersected shape also a sensor?
    Mask is_sensor() const { return shape->is_sensor(); }

    /// Does the surface mark a transition between two media?
    Mask is_medium_transition() const { return shape->is_medium_transition(); }

    /**
     * \brief Determine the target medium
     *
     * When ``is_medium_transition()`` = \c true, determine the medium that
     * contains the ``ray(this->p, d)``
     */
    MediumPtr target_medium(const Vector3f &d) const {
        return target_medium(dr::dot(d, n));
    }

    /**
     * \brief Determine the target medium based on the cosine
     * of the angle between the geometric normal and a direction
     *
     * Returns the exterior medium when ``cos_theta > 0`` and
     * the interior medium when ``cos_theta <= 0``.
     */
    MediumPtr target_medium(const Float &cos_theta) const {
        return dr::select(cos_theta > 0, shape->exterior_medium(),
                                         shape->interior_medium());
    }

    /**
     * \brief Returns the BSDF of the intersected shape.
     *
     * The parameter \c ray must match the one used to create the interaction
     * record. This function computes texture coordinate partials if this is
     * required by the BSDF (e.g. for texture filtering).
     *
     * Implementation in 'bsdf.h'
     */
    BSDFPtr bsdf(const RayDifferential3f &ray);

    // Returns the BSDF of the intersected shape
    BSDFPtr bsdf() const { return shape->bsdf(); }

    /// Computes texture coordinate partials
    void compute_uv_partials(const RayDifferential3f &ray) {
        if (!ray.has_differentials)
            return;

        // Compute interaction with the two offset rays
        Float d   = dr::dot(n, p),
              t_x = (d - dr::dot(n, ray.o_x)) / dr::dot(n, ray.d_x),
              t_y = (d - dr::dot(n, ray.o_y)) / dr::dot(n, ray.d_y);

        // Corresponding positions near the surface
        Vector3f dp_dx = dr::fmadd(ray.d_x, t_x, ray.o_x) - p,
                 dp_dy = dr::fmadd(ray.d_y, t_y, ray.o_y) - p;

        // Solve a least squares problem to turn this into UV coordinates
        Float a00 = dr::dot(dp_du, dp_du),
              a01 = dr::dot(dp_du, dp_dv),
              a11 = dr::dot(dp_dv, dp_dv),
              inv_det = dr::rcp(dr::fmsub(a00, a11, a01*a01));

        Float b0x = dr::dot(dp_du, dp_dx),
              b1x = dr::dot(dp_dv, dp_dx),
              b0y = dr::dot(dp_du, dp_dy),
              b1y = dr::dot(dp_dv, dp_dy);

        // Set the UV partials to zero if dpdu and/or dpdv == 0
        inv_det = dr::select(dr::isfinite(inv_det), inv_det, 0.f);

        duv_dx = Vector2f(dr::fmsub(a11, b0x, a01 * b1x),
                          dr::fmsub(a00, b1x, a01 * b0x)) * inv_det;

        duv_dy = Vector2f(dr::fmsub(a11, b0y, a01 * b1y),
                          dr::fmsub(a00, b1y, a01 * b0y)) * inv_det;
    }

    /**
     * \brief Converts a Mueller matrix defined in a local frame to world space
     *
     * A Mueller matrix operates from the (implicitly) defined frame
     * stokes_basis(in_forward) to the frame stokes_basis(out_forward).
     * This method converts a Mueller matrix defined on directions in the local
     * frame to a Mueller matrix defined on world-space directions.
     *
     * This expands to a no-op in non-polarized modes.
     *
     * \param M_local
     *      The Mueller matrix in local space, e.g. returned by a BSDF.
     *
     * \param in_forward_local
     *      Incident direction (along propagation direction of light),
     *      given in local frame coordinates.
     *
     * \param wo_local
     *      Outgoing direction (along propagation direction of light),
     *      given in local frame coordinates.
     *
     * \return
     *      Equivalent Mueller matrix that operates in world-space coordinates.
     */
    Spectrum to_world_mueller(const Spectrum &M_local,
                              const Vector3f &in_forward_local,
                              const Vector3f &out_forward_local) const {
        if constexpr (is_polarized_v<Spectrum>) {
            Vector3f in_forward_world  = to_world(in_forward_local),
                     out_forward_world = to_world(out_forward_local);

            Vector3f in_basis_current = to_world(mueller::stokes_basis(in_forward_local)),
                     in_basis_target  = mueller::stokes_basis(in_forward_world);

            Vector3f out_basis_current = to_world(mueller::stokes_basis(out_forward_local)),
                     out_basis_target  = mueller::stokes_basis(out_forward_world);

            return mueller::rotate_mueller_basis(M_local,
                                                 in_forward_world, in_basis_current, in_basis_target,
                                                 out_forward_world, out_basis_current, out_basis_target);
        } else {
            DRJIT_MARK_USED(in_forward_local);
            DRJIT_MARK_USED(out_forward_local);
            return M_local;
        }
    }

    /**
     * \brief Converts a Mueller matrix defined in world space to a local frame
     *
     * A Mueller matrix operates from the (implicitly) defined frame
     * stokes_basis(in_forward) to the frame stokes_basis(out_forward).
     * This method converts a Mueller matrix defined on directions in
     * world-space to a Mueller matrix defined in the local frame.
     *
     * This expands to a no-op in non-polarized modes.
     *
     * \param in_forward_local
     *      Incident direction (along propagation direction of light),
     *      given in world-space coordinates.
     *
     * \param wo_local
     *      Outgoing direction (along propagation direction of light),
     *      given in world-space coordinates.
     *
     * \return
     *      Equivalent Mueller matrix that operates in local frame coordinates.
     */
    Spectrum to_local_mueller(const Spectrum &M_world,
                              const Vector3f &in_forward_world,
                              const Vector3f &out_forward_world) const {
        if constexpr (is_polarized_v<Spectrum>) {
            Vector3f in_forward_local = to_local(in_forward_world),
                     out_forward_local = to_local(out_forward_world);

            Vector3f in_basis_current = to_local(mueller::stokes_basis(in_forward_world)),
                     in_basis_target  = mueller::stokes_basis(in_forward_local);

            Vector3f out_basis_current = to_local(mueller::stokes_basis(out_forward_world)),
                     out_basis_target  = mueller::stokes_basis(out_forward_local);

            return mueller::rotate_mueller_basis(M_world,
                                                 in_forward_local, in_basis_current, in_basis_target,
                                                 out_forward_local, out_basis_current, out_basis_target);
        } else {
            return M_world;
        }
    }

    bool has_uv_partials() const {
        if constexpr (dr::is_dynamic_v<Float>)
            return dr::width(duv_dx) > 0 || dr::width(duv_dy) > 0;
        else
            return dr::any_nested(dr::neq(duv_dx, 0.f) || dr::neq(duv_dy, 0.f));
    }

    bool has_n_partials() const {
        if constexpr (dr::is_dynamic_v<Float>)
            return dr::width(dn_du) > 0 || dr::width(dn_dv) > 0;
        else
            return dr::any_nested(dr::neq(dn_du, 0.f) || dr::neq(dn_dv, 0.f));
    }

    /**
     * \brief Fills uninitialized fields after a call to \ref Shape::compute_surface_interaction()
     *
     * \param pi
     *      Preliminary intersection which was used to during the call to
     *      \ref Shape::compute_surface_interaction()
     * \param ray
     *      Ray associated with the ray intersection
     * \param ray_flags
     *      Flags specifying which information should be computed
     */
    void finalize_surface_interaction(
        const PreliminaryIntersection<Float, Shape> &pi, const Ray3f &ray,
        uint32_t ray_flags, Mask active) {
        dr::masked(t, !active) = dr::Infinity<Float>;
        active &= is_valid();

        dr::masked(shape, !active)    = nullptr;
        dr::masked(instance, !active) = nullptr;

        prim_index  = pi.prim_index;
        time        = ray.time;
        wavelengths = ray.wavelengths;

        if (has_flag(ray_flags, RayFlags::ShadingFrame))
            initialize_sh_frame();

        // Incident direction in local coordinates
        wi = dr::select(active, to_local(-ray.d), -ray.d);

        duv_dx = duv_dy = dr::zeros<Point2f>();

        if (has_flag(ray_flags, RayFlags::BoundaryTest))
            boundary_test = dr::select(active, dr::detach(boundary_test), 1e8f);
    }

    //! @}
    // =============================================================

    DRJIT_STRUCT(SurfaceInteraction, t, time, wavelengths, p, n, shape, uv,
                 sh_frame, dp_du, dp_dv, dn_du, dn_dv, duv_dx,
                 duv_dy, wi, prim_index, instance, boundary_test)
};

// -----------------------------------------------------------------------------

/// Stores information related to a medium scattering interaction
template <typename Float_, typename Spectrum_>
struct MediumInteraction : Interaction<Float_, Spectrum_> {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================
    using Float    = Float_;
    using Spectrum = Spectrum_;
    MI_IMPORT_RENDER_BASIC_TYPES()
    MI_IMPORT_OBJECT_TYPES()
    using Index = typename CoreAliases::UInt32;

    // Make parent fields/functions visible
    MI_IMPORT_BASE(Interaction, t, time, wavelengths, p, n, is_valid)
    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Pointer to the associated medium
    MediumPtr medium = nullptr;

    /// Shading frame
    Frame3f sh_frame;

    /// Incident direction in the local shading frame
    Vector3f wi;

    UnpolarizedSpectrum sigma_s, sigma_n, sigma_t, combined_extinction;

    /// mint used when sampling the given distance ``t``
    Float mint;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================


    /// Convert a local shading-space vector into world space
    Vector3f to_world(const Vector3f &v) const {
        return sh_frame.to_world(v);
    }

    /// Convert a world-space vector into local shading coordinates
    Vector3f to_local(const Vector3f &v) const {
        return sh_frame.to_local(v);
    }

    //! @}
    // =============================================================

    DRJIT_STRUCT(MediumInteraction, t, time, wavelengths, p, n, medium,
                 sh_frame, wi, sigma_s, sigma_n, sigma_t,
                 combined_extinction, mint)
};

// -----------------------------------------------------------------------------

/**
 * \brief Stores preliminary information related to a ray intersection
 *
 * This data structure is used as return type for the \ref Shape::ray_intersect_preliminary
 * efficient ray intersection routine. It stores whether the shape is intersected by a
 * given ray, and cache preliminary information about the intersection if that is the case.
 *
 * If the intersection is deemed relevant, detailed intersection information can later be
 * obtained via the  \ref create_surface_interaction() method.
 */
template <typename Float_, typename Shape_>
struct PreliminaryIntersection {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Float    = Float_;
    using ShapePtr = dr::replace_scalar_t<Float, const Shape_ *>;

    MI_IMPORT_CORE_TYPES()

    using Index = typename CoreAliases::UInt32;
    using Ray3f = typename Shape_::Ray3f;
    using Spectrum = typename Ray3f::Spectrum;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Distance traveled along the ray
    Float t = dr::Infinity<Float>;

    /// 2D coordinates on the primitive surface parameterization
    Point2f prim_uv;

    /// Primitive index, e.g. the triangle ID (if applicable)
    Index prim_index;

    /// Shape index, e.g. the shape ID in shapegroup (if applicable)
    Index shape_index;

    /// Pointer to the associated shape
    ShapePtr shape = nullptr;

    /// Stores a pointer to the parent instance (if applicable)
    ShapePtr instance = nullptr;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /**
     * This callback method is invoked by dr::zeros<>, and takes care of fields that deviate
     * from the standard zero-initialization convention. In this particular class, the ``t``
     * field should be set to an infinite value to mark invalid intersection records.
     */
    void zero_(size_t size = 1) {
        t = dr::full<Float>(dr::Infinity<Float>, size);
    }

    /// Is the current interaction valid?
    Mask is_valid() const {
        return dr::neq(t, dr::Infinity<Float>);
    }

    /**
     * \brief Compute and return detailed information related to a surface interaction
     *
     * \param ray
     *      Ray associated with the ray intersection
     * \param ray_flags
     *      Flags specifying which information should be computed
     * \return
     *      A data structure containing the detailed information
     */
    auto compute_surface_interaction(const Ray3f &ray,
                                     uint32_t ray_flags = +RayFlags::All,
                                     Mask active = true) {
        if constexpr (!std::is_same_v<Shape_, Shape<Float, Spectrum>>) {
            Throw("PreliminaryIntersection::compute_surface_interaction(): not implemented!");
        } else {
            using SurfaceInteraction3f = SurfaceInteraction<Float, Spectrum>;
            using ShapePtr = typename SurfaceInteraction3f::ShapePtr;

            active &= is_valid();
            if (dr::none_or<false>(active)) {
                SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
                si.wi = -ray.d;
                si.wavelengths = ray.wavelengths;
                return si;
            }

            ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);

            ShapePtr target = dr::select(dr::eq(instance, nullptr), shape, instance);
            SurfaceInteraction3f si =
                target->compute_surface_interaction(ray, *this, ray_flags, 0u, active);
            si.finalize_surface_interaction(*this, ray, ray_flags, active);

            return si;
        }
    }

    //! @}
    // =============================================================

    DRJIT_STRUCT(PreliminaryIntersection, t, prim_uv, prim_index, shape_index,
                 shape, instance);
};

// -----------------------------------------------------------------------------

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const Interaction<Float, Spectrum> &it) {
    if (dr::none(it.is_valid())) {
        os << "Interaction[invalid]";
    } else {
        os << "Interaction[" << std::endl
           << "  t = " << it.t << "," << std::endl
           << "  time = " << it.time << "," << std::endl
           << "  wavelengths = " << it.wavelengths << "," << std::endl
           << "  p = " << string::indent(it.p, 6) << std::endl
           << "]";
    }
    return os;
}

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const SurfaceInteraction<Float, Spectrum> &it) {
    if (dr::none(it.is_valid())) {
        os << "SurfaceInteraction[invalid]";
    } else {
        os << "SurfaceInteraction[" << std::endl
           << "  t = " << it.t << "," << std::endl
           << "  time = " << it.time << "," << std::endl
           << "  wavelengths = " << string::indent(it.wavelengths, 16) << "," << std::endl
           << "  p = " << string::indent(it.p, 6) << "," << std::endl
           << "  shape = " << string::indent(it.shape, 2) << "," << std::endl
           << "  uv = " << string::indent(it.uv, 7) << "," << std::endl
           << "  n = " << string::indent(it.n, 6) << "," << std::endl
           << "  sh_frame = " << string::indent(it.sh_frame, 2) << "," << std::endl
           << "  dp_du = " << string::indent(it.dp_du, 10) << "," << std::endl
           << "  dp_dv = " << string::indent(it.dp_dv, 10) << "," << std::endl;

        if (it.has_n_partials())
            os << "  dn_du = " << string::indent(it.dn_du, 11) << "," << std::endl
               << "  dn_dv = " << string::indent(it.dn_dv, 11) << "," << std::endl;

        if (it.has_uv_partials())
            os << "  duv_dx = " << string::indent(it.duv_dx, 11) << "," << std::endl
               << "  duv_dy = " << string::indent(it.duv_dy, 11) << "," << std::endl;

        os << "  wi = " << string::indent(it.wi, 7) << "," << std::endl
           << "  prim_index = " << it.prim_index << "," << std::endl
           << "  instance = " << string::indent(it.instance, 13) << std::endl
           << "]";
    }
    return os;
}

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const MediumInteraction<Float, Spectrum> &it) {
    if (dr::none(it.is_valid())) {
        os << "MediumInteraction[invalid]";
    } else {
        os << "MediumInteraction[" << std::endl
           << "  t = " << it.t << "," << std::endl
           << "  time = " << it.time << "," << std::endl
           << "  wavelengths = " << it.wavelengths << "," << std::endl
           << "  p = " << string::indent(it.p, 6) << "," << std::endl
           << "  medium = " << string::indent(it.medium, 2) << "," << std::endl
           << "  sh_frame = " << string::indent(it.sh_frame, 2) << "," << std::endl
           << "  wi = " << string::indent(it.wi, 7) << "," << std::endl
           << "]";
    }
    return os;
}

template <typename Float, typename Shape>
std::ostream &operator<<(std::ostream &os, const PreliminaryIntersection<Float, Shape> &pi) {
    if (dr::none(pi.is_valid())) {
        os << "PreliminaryIntersection[invalid]";
    } else {
        os << "PreliminaryIntersection[" << std::endl
           << "  t = " << pi.t << "," << std::endl
           << "  prim_uv = " << pi.prim_uv << "," << std::endl
           << "  prim_index = " << pi.prim_index << "," << std::endl
           << "  shape_index = " << pi.shape_index << "," << std::endl
           << "  shape = " << string::indent(pi.shape, 6) << "," << std::endl
           << "  instance = " << string::indent(pi.instance, 6) << "," << std::endl
           << "]";
    }
    return os;
}

NAMESPACE_END(mitsuba)
