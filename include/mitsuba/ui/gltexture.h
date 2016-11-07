#pragma once

#include <mitsuba/core/bitmap.h>
#include <nanogui/glutil.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_UI GLTexture : public Object {
public:
    GLTexture(const Bitmap *bitmap);

    MTS_DECLARE_CLASS()

protected:
    virtual ~GLTexture() { }

private:
    ref<const Bitmap> m_bitmap;
};

NAMESPACE_END(mitsuba)
