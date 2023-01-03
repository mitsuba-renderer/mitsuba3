#pragma once

#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB SDF : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_TYPES()
    MI_IMPORT_BASE(Shape)

    using typename Base::ScalarSize;

    // =========================================================================
    //! @{ \name Accessors (normals, hessians, normals, etc)
    // =========================================================================
    //

    virtual Normal3f smooth_sh(const Point3f & /*p*/,
                               const Float * /*u_ptr*/ = nullptr,
                               const Float * /*u_ptr*/ = nullptr,
                               const Float * /*w_ptr*/ = nullptr) const {
        return Normal3f(0.f);
    };

    virtual Normal3f smooth(const Point3f & /*p*/) const {
        return Normal3f(0.f);
    }

    virtual Matrix3f smooth_hessian(const Point3f & /*p*/) const {
        return Matrix3f(0.f);
    }

    virtual ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f(0.f);
    }

    /// @}
    // =========================================================================

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

protected:
    SDF(const Properties &);
    inline SDF() {}
    virtual ~SDF();

    MI_DECLARE_CLASS()
};

MI_EXTERN_CLASS(SDF)
NAMESPACE_END(mitsuba)
