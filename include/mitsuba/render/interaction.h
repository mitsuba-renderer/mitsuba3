#pragma once

#include <mitsuba/core/frame.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/mueller.h>

NAMESPACE_BEGIN(mitsuba)

/// Generic surface interaction data structure
template <typename Float_, typename Spectrum_>
struct Interaction {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Float      = Float_;
    using Spectrum   = Spectrum_;
    using Mask       = mask_t<Float>;
    using Point3f    = Point<Float, 3>;
    using Vector3f   = Vector<Float, 3>;
    using Ray3f      = Ray<Point3f, Spectrum>;
    using Wavelength = wavelength_t<Spectrum>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Distance traveled along the ray
    Float t = math::Infinity<Float>;

    /// Time value associated with the interaction
    Float time;

    /// Wavelengths associated with the ray that produced this interaction
    Wavelength wavelengths;

    /// Position of the interaction in world coordinates
    Point3f p;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Methods
    // =============================================================

    /// Is the current interaction valid?
    Mask is_valid() const {
        return neq(t, math::Infinity<Float>);
    }

    /// Spawn a semi-infinite ray towards the given direction
    Ray3f spawn_ray(const Vector3f &d) const {
        return Ray3f(p, d, (1.f + hmax(abs(p))) * math::RayEpsilon<Float>,
                     math::Infinity<Float>, time, wavelengths);
    }

    /// Spawn a finite ray towards the given position
    Ray3f spawn_ray_to(const Point3f &t) const {
        Vector3f d = t - p;
        Float dist = norm(d);
        d /= dist;

        return Ray3f(p, d, (1.f + hmax(abs(p))) * math::RayEpsilon<Float>,
                     dist * (1.f - math::ShadowEpsilon<Float>), time, wavelengths);
    }

    //! @}
    // =============================================================

    ENOKI_STRUCT(Interaction, t, time, wavelengths, p);
};

/// Stores information related to a surface scattering interaction
template <typename Float_, typename Spectrum_>
struct SurfaceInteraction : Interaction<Float_, Spectrum_> {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================
    using Float    = Float_;
    using Spectrum = Spectrum_;
    MTS_IMPORT_RENDER_BASIC_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using Index            = typename CoreAliases::UInt32;
    using PositionSample3f = typename RenderAliases::PositionSample3f;
    // Make parent fields/functions visible
    MTS_IMPORT_BASE(Interaction, t, time, wavelengths, p, is_valid)
    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Pointer to the associated shape
    ShapePtr shape = nullptr;

    /// UV surface coordinates
    Point2f uv;

    /// Geometric normal
    Normal3f n;

    /// Shading frame
    Frame3f sh_frame;

    /// Position partials wrt. the UV parameterization
    Vector3f dp_du, dp_dv;

    /// UV partials wrt. changes in screen-space
    Vector2f duv_dx, duv_dy;

    /// Incident direction in the local shading frame
    Vector3f wi;

    /// Primitive index, e.g. the triangle ID (if applicable)
    Index prim_index;

    /// Stores a pointer to the parent instance (if applicable)
    ShapePtr instance = nullptr;

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
        : Base(0.f, ps.time, wavelengths, ps.p), uv(ps.uv), n(ps.n),
          sh_frame(Frame3f(ps.n)) { }

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
        return target_medium(dot(d, n));
    }

    /**
     * \brief Determine the target medium based on the cosine
     * of the angle between the geometric normal and a direction
     *
     * Returns the exterior medium when ``cos_theta > 0`` and
     * the interior medium when ``cos_theta <= 0``.
     */
    MediumPtr target_medium(const Float &cos_theta) const {
        return select(cos_theta > 0, shape->exterior_medium(),
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

    /// Calls the suitable implementation of \ref Shape::normal_derivative()
    std::pair<Vector3f, Vector3f> normal_derivative(
            bool shading_frame = true, Mask active = true) const {
        ShapePtr target = select(neq(instance, nullptr), instance, shape);
        return target->normal_derivative(*this, shading_frame, active);
    }

    /// Computes texture coordinate partials
    void compute_partials(const RayDifferential3f &ray) {
        if (!ray.has_differentials)
            return;

        // Compute interaction with the two offset rays
        Float d   = dot(n, p),
              t_x = (d - dot(n, ray.o_x)) / dot(n, ray.d_x),
              t_y = (d - dot(n, ray.o_y)) / dot(n, ray.d_y);

        // Corresponding positions near the surface
        Vector3f dp_dx = fmadd(ray.d_x, t_x, ray.o_x) - p,
                 dp_dy = fmadd(ray.d_y, t_y, ray.o_y) - p;

        // Solve a least squares problem to turn this into UV coordinates
        Float a00 = dot(dp_du, dp_du),
              a01 = dot(dp_du, dp_dv),
              a11 = dot(dp_dv, dp_dv),
              inv_det = rcp(a00*a11 - a01*a01);

        Float b0x = dot(dp_du, dp_dx),
              b1x = dot(dp_dv, dp_dx),
              b0y = dot(dp_du, dp_dy),
              b1y = dot(dp_dv, dp_dy);

        /* Set the UV partials to zero if dpdu and/or dpdv == 0 */
        inv_det = select(enoki::isfinite(inv_det), inv_det, 0.f);

        duv_dx = Vector2f(fmsub(a11, b0x, a01 * b1x) * inv_det,
                          fmsub(a00, b1x, a01 * b0x) * inv_det);

        duv_dy = Vector2f(fmsub(a11, b0y, a01 * b1y) * inv_det,
                          fmsub(a00, b1y, a01 * b0y) * inv_det);
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
            ENOKI_MARK_USED(in_forward_local);
            ENOKI_MARK_USED(out_forward_local);
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
        return any_nested(neq(duv_dx, 0.f) || neq(duv_dy, 0.f));
    }

    //! @}
    // =============================================================

    ENOKI_DERIVED_STRUCT(SurfaceInteraction, Base,
        ENOKI_BASE_FIELDS(t, time, wavelengths, p),
        ENOKI_DERIVED_FIELDS(shape, uv, n, sh_frame, dp_du, dp_dv,
                             duv_dx, duv_dy, wi, prim_index, instance)
    )
};


/// Stores information related to a medium scattering interaction
template <typename Float_, typename Spectrum_>
struct MediumInteraction : Interaction<Float_, Spectrum_> {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================
    using Float    = Float_;
    using Spectrum = Spectrum_;
    MTS_IMPORT_RENDER_BASIC_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using Index = typename CoreAliases::UInt32;

    // Make parent fields/functions visible
    MTS_IMPORT_BASE(Interaction, t, time, wavelengths, p, is_valid)
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

    /// mint used when sampling the given distance "t".
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

    ENOKI_DERIVED_STRUCT(MediumInteraction, Base,
        ENOKI_BASE_FIELDS(t, time, wavelengths, p),
        ENOKI_DERIVED_FIELDS(medium, sh_frame, wi, sigma_s, sigma_n, sigma_t, combined_extinction, mint)
    )

};


// -----------------------------------------------------------------------------

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const Interaction<Float, Spectrum> &it) {
    if (none(it.is_valid())) {
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
    if (none(it.is_valid())) {
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
    if (none(it.is_valid())) {
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

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_SUPPORT(mitsuba::Interaction, t, time, wavelengths, p)

ENOKI_STRUCT_SUPPORT(mitsuba::SurfaceInteraction, t, time, wavelengths, p,
                     shape, uv, n, sh_frame, dp_du, dp_dv, duv_dx, duv_dy, wi,
                     prim_index, instance)

ENOKI_STRUCT_SUPPORT(mitsuba::MediumInteraction, t, time, wavelengths, p,
                     medium, sh_frame, wi, sigma_s, sigma_n, sigma_t, combined_extinction, mint)

//! @}
// -----------------------------------------------------------------------
