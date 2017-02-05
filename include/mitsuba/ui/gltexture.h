#pragma once

#include <mitsuba/core/bitmap.h>
#include <nanogui/glutil.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_UI GLTexture : public Object {
public:
    GLTexture();

    GLuint id() const { return m_id; }

    enum EInterpolation {
        ENearest,
        ELinear,
        EMipMapLinear
    };

    /// Create a new uniform texture
    void init(const Bitmap *bitmap);

    /// Release underlying OpenGL objects
    void free();

    /// Bind the texture to a specific texture unit
    void bind(int index = 0);

    /// Set the interpolation mode
    void set_interpolation(EInterpolation intp);

    /// Release/unbind the textur
    void release();

    /// Refresh the texture from the provided bitmap
    void refresh(const Bitmap *bitmap);

    MTS_DECLARE_CLASS()

protected:
    virtual ~GLTexture();

private:
    GLuint m_id, m_index;
};

NAMESPACE_END(mitsuba)
