#include <mitsuba/core/bitmap.h>
#if defined(MTS_HAS_LIBPNG)
#include <png.h>
#endif
#include <iostream>

Bitmap::Bitmap() {
#if defined(MTS_HAS_LIBPNG)
    png_structp png_ptr;
    png_infop info_ptr;

    /* Create buffers */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, nullptr, nullptr);
    std::cout << png_ptr << std::endl;
#endif
}
