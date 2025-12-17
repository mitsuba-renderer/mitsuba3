from __future__ import annotations  # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi


def is_active(props, name):
    """
    Helper function to check when a lobe is active in `props`
    """
    return name in props.keys() and not (
        isinstance(props[name], (float, int)) and props[name] == 0.0
    )


# ------------------------------------------------------------------------------
# Helper routines
# ------------------------------------------------------------------------------


def safe_divide(a, b, fallback=1.0):
    """
    Safely divide a by b with a fallback value when b is near zero.
    """
    return dr.select(dr.abs(b) < 1e-6, fallback, a / b)


def is_front_side(w: mi.Vector3f) -> mi.Bool:
    """
    Return whether the direction is on the front side of the shading frame
    """
    return mi.Frame3f.cos_theta(w) > 0.0


def is_reflection(wi: mi.Vector3f, wo: mi.Vector3f) -> mi.Bool:
    """
    Return True if given configuration is a reflection
    """
    cos_theta_i = mi.Frame3f.cos_theta(wi)
    cos_theta_o = mi.Frame3f.cos_theta(wo)
    return (cos_theta_i * cos_theta_o) > 0.0


def is_refraction(wi: mi.Vector3f, wo: mi.Vector3f) -> mi.Bool:
    """
    Return True if given configuration is a refraction
    """
    cos_theta_i = mi.Frame3f.cos_theta(wi)
    cos_theta_o = mi.Frame3f.cos_theta(wo)
    return (cos_theta_i * cos_theta_o) < 0.0


def sanitize_eta(eta: mi.Float) -> mi.Float:
    """
    Ensure eta isn't too close to 1.0
    """
    diff = eta - 1.0
    return 1.0 + dr.mulsign(dr.maximum(dr.abs(diff), 0.0001), diff)


def half_vector(wi: mi.Vector3f, wo: mi.Vector3f, eta: mi.Float):
    """
    Calculate half vector for the given configuration
    """
    cos_theta_i = mi.Frame3f.cos_theta(wi)
    cos_theta_o = mi.Frame3f.cos_theta(wo)

    # Reflection and refraction masks.
    reflect = cos_theta_i * cos_theta_o > 0.0

    # Masks for the side of the incident ray (wi.z<0)
    front_side = cos_theta_i > 0.0

    # Eta value w.r.t. ray instead of the object.
    eta_path = dr.select(front_side, eta, dr.rcp(eta))

    # Halfway vector
    wh = dr.normalize(wi + wo * dr.select(reflect, 1.0, eta_path))

    # Make sure that the halfway vector points outwards the object
    wh = dr.mulsign(wh, mi.Frame3f.cos_theta(wh))

    return wh


# ------------------------------------------------------------------------------
# Bump, normal and displacement mapping routines
# ------------------------------------------------------------------------------


def compute_bump_frame(
    si: mi.SurfaceInteraction3f, bump_grad: mi.Vector2f
) -> mi.Frame3f:
    """
    Compute shading frame base on the given bump gradient
    """
    # Compute perturbed differential geometry
    dp_du = dr.fma(
        si.sh_frame.n, bump_grad.x - dr.dot(si.sh_frame.n, si.dp_du), si.dp_du
    )
    dp_dv = dr.fma(
        si.sh_frame.n, bump_grad.y - dr.dot(si.sh_frame.n, si.dp_dv), si.dp_dv
    )

    # Bump-mapped shading normal
    result = mi.Frame3f()
    result.n = dr.normalize(dr.cross(dp_du, dp_dv))

    # Flip if not aligned with geometric normal
    result.n[dr.dot(si.n, result.n) < 0.0] *= -1.0

    # Convert to small rotation from original shading frame
    result.n = si.to_local(result.n)

    # Gram-schmidt orthogonalization to compute local shading frame
    result.s = dr.normalize(
        dr.fma(-result.n, dr.dot(result.n, si.to_local(dp_du)), si.to_local(dp_du))
    )
    result.t = dr.cross(result.n, result.s)

    return result


def compute_normalmap_frame(
    si: mi.SurfaceInteraction3f, normal: mi.Normal3f, rot: mi.Float = None
) -> mi.Frame3f:
    """
    Compute shading frame base on the given normal
    """
    if normal is not None:
        dp_du = mi.Vector3f(si.dp_du)
        if rot is not None:
            dp_du = mi.Transform4f().rotate(si.n, rot * 360.0) @ dp_du
        dp_du = si.to_local(dp_du)
        frame = mi.Frame3f()
        frame.n = dr.normalize(2.0 * normal - 1.0)
        frame.s = dr.normalize(-frame.n * dr.dot(frame.n, dp_du) + dp_du)
        frame.t = dr.cross(frame.n, frame.s)
        return frame
    else:
        return mi.Frame3f([0, 0, 1])


def compute_frame(
    si: mi.SurfaceInteraction3f,
    disp: mi.Vector2f = None,
    bump: mi.Vector2f = None,
    normal: mi.Normal3f = None,
    correction: bool = True,
):
    """
    Compute shading frame base on the given displacement, bump and normal
    """
    if disp is None and bump is None and normal is None:
        return mi.Frame3f([0, 0, 1])

    t = mi.Transform4f()
    if disp is not None:
        t = t.to_frame(compute_bump_frame(si, disp))
    if bump is not None:
        t = t.to_frame(compute_bump_frame(si, bump))
    if normal is not None:
        t = t.to_frame(compute_normalmap_frame(si, normal))

    frame = mi.Frame3f(
        t @ mi.Vector3f([1, 0, 0]),
        t @ mi.Vector3f([0, 1, 0]),
        t @ mi.Vector3f([0, 0, 1]),
    )

    if correction:
        frame = correct_grazing_frame(si.wi, frame)

    return frame


def correct_grazing_shading_normal(wi, n):
    """
    Adjust shading normal to avoid reflections in the lower hemisphere. For this,
    this routine raises the shading normal towards the geometry normal so that the
    a specular reflection is just above the surface.

    Implementation from Cycles:
    https://github.com/blender/cycles/blob/main/src/kernel/closure/bsdf_util.h#L115
    """
    # Form coordinate system with Z as the local geometry normal and X such that
    # the plane X-Z contains the half-vector.
    Z = mi.Vector3f(0, 0, 1)
    X = dr.normalize(n - dr.dot(n, Z) * Z)

    wi_z = dr.dot(wi, Z)
    wi_x = dr.dot(wi, X)

    # Reflection direction may at least be as shallow as the incoming ray
    threshold = dr.minimum(0.9 * wi_z, 0.01)

    # Reflection direction
    R = mi.reflect(wi, n)

    # Check whether the half vector should be processed
    should_process = mi.Frame3f.cos_theta(R) < threshold

    # Solve reflection equation to ensure that dot(R', Z) = t
    # See Cycles implementation for detailed equations.
    a = dr.square(wi_x) + dr.square(wi_z)
    b = 2.0 * (a + wi_z * threshold)
    c = dr.square(threshold + wi_z)

    wh_z_2 = 0.25 * (b - dr.mulsign(dr.safe_sqrt(dr.square(b) - 4.0 * a * c), wi_x)) / a

    wh_x = dr.safe_sqrt(1.0 - wh_z_2)
    wh_z = dr.safe_sqrt(wh_z_2)

    # Calculate the processed half vector
    new_n = wh_x * X + wh_z * Z

    return dr.select(should_process, new_n, n)


def correct_grazing_frame(wi, frame):
    """
    Adjust local frame to avoid reflections under the horizon
    """
    new_n = correct_grazing_shading_normal(wi, frame.n)
    new_t = dr.normalize(dr.cross(new_n, frame.s))
    new_s = dr.normalize(dr.cross(new_t, new_n))
    return mi.Frame3f(new_s, new_t, new_n)
