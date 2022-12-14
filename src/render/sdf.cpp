#include <mitsuba/render/sdf.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT SDF<Float, Spectrum>::SDF(const Properties &props) : Base(props) {
}

MI_VARIANT SDF<Float, Spectrum>::~SDF() { }

MI_VARIANT void SDF<Float, Spectrum>::traverse(TraversalCallback *callback) {
    Base::traverse(callback);
}

MI_VARIANT void SDF<Float, Spectrum>::parameters_changed( const std::vector<std::string> & /*keys*/) {
    Base::parameters_changed();
}

MI_IMPLEMENT_CLASS_VARIANT(SDF, Shape)
MI_INSTANTIATE_CLASS(SDF)
NAMESPACE_END(mitsuba)
