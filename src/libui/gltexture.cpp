#include <mitsuba/ui/gltexture.h>

NAMESPACE_BEGIN(mitsuba)

GLTexture::GLTexture(const Bitmap *bitmap) : m_bitmap(bitmap) {
}

MTS_IMPLEMENT_CLASS(GLTexture, Object)
NAMESPACE_END(mitsuba)

