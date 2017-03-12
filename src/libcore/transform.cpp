#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

Transform Transform::translate(const Vector3f &v) {
    Matrix4f trafo(
        1, 0, 0, v.x(),
        0, 1, 0, v.y(),
        0, 0, 1, v.z(),
        0, 0, 0, 1
    );

    Matrix4f inv_trafo(
        1, 0, 0, -v.x(),
        0, 1, 0, -v.y(),
        0, 0, 1, -v.z(),
        0, 0, 0, 1
    );

    return Transform(trafo, inv_trafo);
}

Transform Transform::scale(const Vector3f &v) {
    Vector3f v_rcp = rcp(v);

    Matrix4f trafo(
        v.x(), 0,     0,     0,
        0,     v.y(), 0,     0,
        0,     0,     v.z(), 0,
        0,     0,     0,     1
    );

    Matrix4f inv_trafo(
        v_rcp.x(), 0,         0,         0,
        0,         v_rcp.y(), 0,         0,
        0,         0,         v_rcp.z(), 0,
        0,         0,         0,         1
    );

    return Transform(trafo, inv_trafo);
}

Transform Transform::rotate(const Vector3f &axis, Float angle) {
    Float sin_theta, cos_theta;

    /* Make sure that the axis is normalized */
    Vector3f naxis = normalize(axis);
    std::tie(sin_theta, cos_theta) = sincos(math::deg_to_rad(angle));

    Matrix4f result;
    result(0, 0) = naxis.x() * naxis.x() + (1.0f - naxis.x() * naxis.x()) * cos_theta;
    result(0, 1) = naxis.x() * naxis.y() * (1.0f - cos_theta) - naxis.z() * sin_theta;
    result(0, 2) = naxis.x() * naxis.z() * (1.0f - cos_theta) + naxis.y() * sin_theta;
    result(0, 3) = 0;

    result(1, 0) = naxis.x() * naxis.y() * (1.0f - cos_theta) + naxis.z() * sin_theta;
    result(1, 1) = naxis.y() * naxis.y() + (1.0f - naxis.y() * naxis.y()) * cos_theta;
    result(1, 2) = naxis.y() * naxis.z() * (1.0f - cos_theta) - naxis.x() * sin_theta;
    result(1, 3) = 0;

    result(2, 0) = naxis.x() * naxis.z() * (1.0f - cos_theta) - naxis.y() * sin_theta;
    result(2, 1) = naxis.y() * naxis.z() * (1.0f - cos_theta) + naxis.x() * sin_theta;
    result(2, 2) = naxis.z() * naxis.z() + (1.0f - naxis.z() * naxis.z()) * cos_theta;
    result(2, 3) = 0;

    result(3, 0) = 0;
    result(3, 1) = 0;
    result(3, 2) = 0;
    result(3, 3) = 1;

    /* The matrix is orthonormal */
    return Transform(result, enoki::transpose(result));
}

Transform Transform::perspective(Float fov, Float clip_near, Float clip_far) {
    /* Project vectors in camera space onto a plane at z=1:
     *
     *  x_proj = x / z
     *  y_proj = y / z
     *  z_proj = (far * (z - near)) / (z * (far-near))
     *
     *  Camera-space depths are not mapped linearly!
     */
    Float recip = 1.0f / (clip_far - clip_near);

    /* Perform a scale so that the field of view is mapped
     * to the interval [-1, 1] */
    Float cot = 1.0f / std::tan(math::deg_to_rad(fov / 2.0f));

    Matrix4f trafo(
        cot,  0,    0,   0,
        0,    cot,  0,   0,
        0,    0,    clip_far * recip, -clip_near * clip_far * recip,
        0,    0,    1,   0
    );

    return Transform(trafo);
}

Transform Transform::orthographic(Float clip_near, Float clip_far) {
    return scale(Vector3f(1.0f, 1.0f, 1.0f / (clip_far - clip_near))) *
           translate(Vector3f(0.0f, 0.0f, -clip_near));
}

Transform Transform::look_at(const Point3f &p, const Point3f &t, const Vector3f &up) {
    Vector3f dir = t-p;
    if (t - p == zero<Vector3f>())
        Throw("look_at(): 'origin' and 'target' coincide!");
    dir = normalize(dir);

    Vector3f left = cross(up, dir);
    if (left == zero<Vector3f>())
        Throw("look_at(): the forward and upward direction must be linearly independent!");
    left = normalize(left);

    Vector3f new_u = cross(dir, left);

    Matrix4f result, inverse;
    result(0, 0)  = left.x();  result(1, 0)  = left.y();  result(2, 0)  = left.z();  result(3, 0)  = 0;
    result(0, 1)  = new_u.x(); result(1, 1)  = new_u.y(); result(2, 1)  = new_u.z(); result(3, 1)  = 0;
    result(0, 2)  = dir.x();   result(1, 2)  = dir.y();   result(2, 2)  = dir.z();   result(3, 2)  = 0;
    result(0, 3)  = p.x();     result(1, 3)  = p.y();     result(2, 3)  = p.z();     result(3, 3)  = 1;

    /* The inverse is simple to compute for this matrix, so do it directly here */
    Vector3f q(
        result(0, 0) * p.x() + result(1, 0) * p.y() + result(2, 0) * p.z(),
        result(0, 1) * p.x() + result(1, 1) * p.y() + result(2, 1) * p.z(),
        result(0, 2) * p.x() + result(1, 2) * p.y() + result(2, 2) * p.z());

    inverse(0, 0) = left.x(); inverse(1, 0) = new_u.x(); inverse(2, 0) = dir.x(); inverse(3, 0) = 0;
    inverse(0, 1) = left.y(); inverse(1, 1) = new_u.y(); inverse(2, 1) = dir.y(); inverse(3, 1) = 0;
    inverse(0, 2) = left.z(); inverse(1, 2) = new_u.z(); inverse(2, 2) = dir.z(); inverse(3, 2) = 0;
    inverse(0, 3) = -q.x();   inverse(1, 3) = -q.y();    inverse(2, 3) = -q.z();  inverse(3, 3) = 1;

    return Transform(result, inverse);
}

Matrix4f inv(const Matrix4f &m) {
    Matrix4f I(m);

    size_t ipiv[4] = { 0, 1, 2, 3 };
    for (size_t k = 0; k < 4; ++k) {
        Float largest = 0.f;
        size_t piv = 0;

        /* Find the largest pivot in the current column */
        for (size_t j = k; j < 4; ++j) {
            Float value = std::abs(I(j, k));
            if (value > largest) {
                largest = value;
                piv = j;
            }
        }

        if (largest == 0.f)
            throw std::runtime_error("Singular matrix!");

        /* Row exchange */
        if (piv != k) {
            for (size_t j = 0; j < 4; ++j)
                std::swap(I(k, j), I(piv, j));
            std::swap(ipiv[k], ipiv[piv]);
        }

        Float scale = 1.f / I(k, k);
        I(k, k) = 1.f;
        for (size_t j = 0; j < 4; j++)
            I(k, j) *= scale;

        /* Jordan reduction */
        for (size_t i = 0; i < 4; i++) {
            if (i != k) {
                Float tmp = I(i, k);
                I(i, k) = 0;

                for (int j = 0; j < 4; j++)
                    I(i, j) -= I(k, j) * tmp;
            }
        }
    }

    /* Backward permutation */
    Matrix4f out;
    for (size_t j = 0; j < 4; ++j)
        out.coeff(ipiv[j]) = I.coeff(j);
    return out;
}

std::ostream &operator<<(std::ostream &os, const Transform &t) {
    os << t.matrix();
    return os;
}

template Point3f   MTS_EXPORT_CORE Transform::operator*(Point3f) const;
template Point3fP  MTS_EXPORT_CORE Transform::operator*(Point3fP) const;
template Point3f   MTS_EXPORT_CORE Transform::transform_affine(Point3f) const;
template Point3fP  MTS_EXPORT_CORE Transform::transform_affine(Point3fP) const;

template Vector3f  MTS_EXPORT_CORE Transform::operator*(Vector3f) const;
template Vector3fP MTS_EXPORT_CORE Transform::operator*(Vector3fP) const;
template Vector3f  MTS_EXPORT_CORE Transform::transform_affine(Vector3f) const;
template Vector3fP MTS_EXPORT_CORE Transform::transform_affine(Vector3fP) const;

template Normal3f  MTS_EXPORT_CORE Transform::operator*(Normal3f) const;
template Normal3fP MTS_EXPORT_CORE Transform::operator*(Normal3fP) const;
template Normal3f  MTS_EXPORT_CORE Transform::transform_affine(Normal3f) const;
template Normal3fP MTS_EXPORT_CORE Transform::transform_affine(Normal3fP) const;

template Ray3f     MTS_EXPORT_CORE Transform::operator*(Ray3f) const;
template Ray3fP    MTS_EXPORT_CORE Transform::operator*(Ray3fP) const;
template Ray3f     MTS_EXPORT_CORE Transform::transform_affine(Ray3f) const;
template Ray3fP    MTS_EXPORT_CORE Transform::transform_affine(Ray3fP) const;

NAMESPACE_END(mitsuba)
