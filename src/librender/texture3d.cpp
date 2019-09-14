#include <mitsuba/core/properties.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/texture3d.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name 3D Texture implementation (just throws exceptions)
// =======================================================================

Texture3D::Texture3D(const Properties &props) {
    m_world_to_local = props.transform("to_world", Transform4f()).inverse();
    update_bbox();
}

Texture3D::~Texture3D() {}

Spectrumf Texture3D::eval(const Interaction3f & /*it*/) const {
    NotImplementedError("eval");
}
SpectrumfP Texture3D::eval(const Interaction3fP & /*it*/,
                           MaskP /*active*/) const {
    NotImplementedError("eval_p");
}
#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD Texture3D::eval(const Interaction3fD &, const MaskD &) const {
    NotImplementedError("eval_d");
}
#endif

std::pair<Spectrumf, Vector3f> Texture3D::eval_gradient(const Interaction3f & /*it*/) const {
    NotImplementedError("eval_gradient");
}
std::pair<SpectrumfP, Vector3fP> Texture3D::eval_gradient(const Interaction3fP & /*it*/,
                                                          MaskP /*active*/) const {
    NotImplementedError("eval_gradient_p");
}
#if defined(MTS_ENABLE_AUTODIFF)
std::pair<SpectrumfD, Vector3fD> Texture3D::eval_gradient(const Interaction3fD &,
                                                          const MaskD &) const {
    NotImplementedError("eval_gradient_d");
}
#endif

Float Texture3D::mean() const { NotImplementedError("mean"); }

void Texture3D::update_bbox() {
    Point3f a(0.0f, 0.0f, 0.0f);
    Point3f b(1.0f, 1.0f, 1.0f);
    a      = m_world_to_local.inverse() * a;
    b      = m_world_to_local.inverse() * b;
    m_bbox = BoundingBox3f(a);
    m_bbox.expand(b);
}

std::string Texture3D::to_string() const {
    std::ostringstream oss;
    oss << "Texture3D[" << std::endl
        << "  world_to_local = " << m_world_to_local << std::endl
        << "]";
    return oss.str();
}

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_ALIAS(Texture3D, Object)

NAMESPACE_END(mitsuba)
