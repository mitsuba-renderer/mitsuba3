#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

Transform Transform::perspective(Float fov, Float near, Float far) {
    /* Project vectors in camera space onto a plane at z=1:
     *
     *  x_proj = x / z
     *  y_proj = y / z
     *  z_proj = (far * (z - near)) / (z * (far-near))
     *
     *  Camera-space depths are not mapped linearly!
     */
    Float recip = 1.f / (far - near);

    /* Perform a scale so that the field of view is mapped
       to the interval [-1, 1] */
    Float tan = enoki::tan(deg_to_rad(fov * .5f)),
          cot = 1.f / tan;

    Matrix4f trafo = diag<Matrix4f>(Vector4f(cot, cot, far * recip, -near * far * recip));
    trafo(3, 2) = 1.f;

    Matrix4f inv_trafo = diag<Matrix4f>(Vector4f(tan, tan, 0.f, rcp(near)));
    trafo(2, 3) = 1.f;
    trafo(3, 2) = (near - far) / (far * near);

    return Transform(trafo, inv_trafo);
}

Transform Transform::orthographic(Float near, Float far) {
    return scale(Vector3f(1.f, 1.f, 1.f / (far - near))) *
           translate(Vector3f(0.f, 0.f, -near));
}

Transform Transform::look_at(const Point3f &origin, const Point3f &target, const Vector3f &up) {
    Vector3f dir = target - origin;
    if (unlikely(target - origin == zero<Vector3f>()))
        Throw("look_at(): 'origin' and 'target' coincide!");
    dir = normalize(dir);

    Vector3f left = cross(up, dir);
    if (unlikely(left == zero<Vector3f>()))
        Throw("look_at(): the forward and upward direction must be linearly independent!");
    left = normalize(left);

    Vector3f new_up = cross(dir, left);

    Matrix4f result = Matrix4f::from_cols(
        concat(left, 0.f),
        concat(new_up, 0.f),
        concat(dir, 0.f),
        concat(origin, 1.f)
    );

    Matrix4f inverse = Matrix4f::from_rows(
        concat(left, 0.f),
        concat(new_up, 0.f),
        concat(dir, 0.f),
        Vector4f(0.f, 0.f, 0.f, 1.f)
    );

    inverse[3] = inverse * concat(-origin, 1.f);

    return Transform(result, inverse);
}

std::ostream &operator<<(std::ostream &os, const Transform &t) {
    os << t.matrix();
    return os;
}

void AnimatedTransform::append_keyframe(Float time, const Transform &trafo) {
    if (!m_keyframes.empty() && time <= m_keyframes.back().time)
        Throw("AnimatedTransform::append_keyframe(): time values must be "
              "strictly monotonically increasing!");

    Matrix3f M; Quaternion4f Q; Vector3f T;

    /* Perform a polar decomposition into a 3x3 scale/shear matrix,
       a rotation quaternion, and a translation vector. These will
       all be interpolated independently. */
    std::tie(M, Q, T) = enoki::transform_decompose(trafo.matrix());

    m_keyframes.push_back(Keyframe { time, M, Q, T });
    m_transform = trafo;
}


template Point3f   MTS_EXPORT_CORE Transform::operator*(const Point3f&) const;
template Point3fP  MTS_EXPORT_CORE Transform::operator*(const Point3fP&) const;
template Point3f   MTS_EXPORT_CORE Transform::transform_affine(const Point3f&) const;
template Point3fP  MTS_EXPORT_CORE Transform::transform_affine(const Point3fP&) const;

template Vector3f  MTS_EXPORT_CORE Transform::operator*(const Vector3f&) const;
template Vector3fP MTS_EXPORT_CORE Transform::operator*(const Vector3fP&) const;
template Vector3f  MTS_EXPORT_CORE Transform::transform_affine(const Vector3f&) const;
template Vector3fP MTS_EXPORT_CORE Transform::transform_affine(const Vector3fP&) const;

template Normal3f  MTS_EXPORT_CORE Transform::operator*(const Normal3f&) const;
template Normal3fP MTS_EXPORT_CORE Transform::operator*(const Normal3fP&) const;
template Normal3f  MTS_EXPORT_CORE Transform::transform_affine(const Normal3f&) const;
template Normal3fP MTS_EXPORT_CORE Transform::transform_affine(const Normal3fP&) const;

template Ray3f     MTS_EXPORT_CORE Transform::operator*(const Ray3f&) const;
template Ray3fP    MTS_EXPORT_CORE Transform::operator*(const Ray3fP&) const;
template Ray3f     MTS_EXPORT_CORE Transform::transform_affine(const Ray3f&) const;
template Ray3fP    MTS_EXPORT_CORE Transform::transform_affine(const Ray3fP&) const;

template auto MTS_EXPORT_CORE AnimatedTransform::lookup(const Float &,  const bool &) const;
template auto MTS_EXPORT_CORE AnimatedTransform::lookup(const FloatP &, const mask_t<FloatP> &) const;

NAMESPACE_END(mitsuba)
