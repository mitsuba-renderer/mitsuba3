#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/python/python.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/bitmap.h>

#include <drjit/python.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

/*
 *  This file contains bindings for routines using in the Mitsuba-Blender add-on.
 */

// Blender struct to be used when object is passed using the `.as_pointer()` method.
namespace blender {
    struct ImBuf {
        int x, y;
        unsigned char planes;
        int channels;
        int flags;
        char padding[24]; // Padding for other structs that are ignored here
        float *data;
        /// ...
    };
    struct RenderPass {
        struct RenderPass *next, *prev;
        int channels;
        char name[64];
        char chan_id[8];
        ImBuf *ibuf;  // The only thing we are interested in
        int rectx, recty;
        // ...
    };
    struct PackedFile {
        int size;
        int seek;
        const void *data;
        void* padding;
    };
}

using ContigCpuNdArray = nb::ndarray<nb::device::cpu, nb::c_contig>;

MI_PY_EXPORT(blender) {
    /// Routine to accelerates the writing of a numpy array image into a
    /// RenderPass iBuf data buffer.
    m.def("write_blender_framebuffer", [](ContigCpuNdArray data, const size_t ptr) {
        blender::RenderPass *render_pass = reinterpret_cast<blender::RenderPass *>(ptr);

          if (data.ndim() != 3)
              throw nb::type_error("Invalid num of dimensions. Expected three!");

          if (data.dtype() != nb::dtype<float>())
              throw nb::type_error("Invalid array type. Expected float32!");

          float* src = (float*) data.data();
          float* dst = render_pass->ibuf->data;

          const size_t width = data.shape(0);
          const size_t height = data.shape(1);
          const size_t src_channels = data.shape(2);
          const size_t dst_channels = render_pass->channels;

          for (size_t y = 0; y < height; ++y) {
              size_t src_index = y * width * src_channels;
              size_t dst_index = y * width * dst_channels;

              for (size_t x = 0; x < width; ++x) {
                  for (size_t c = 0; c < dst_channels; c++)
                      dst[dst_index + c] = (c < src_channels ? src[src_index + c] : 1.f);

                  src_index += src_channels;
                  dst_index += dst_channels;
              }
        }
    });

    /// Routine to directly load a Bitmap image from a Blender packed file.
    m.def("packed_file_to_bitmap", [](const size_t packed_file_ptr) {
        blender::PackedFile *packed_file = reinterpret_cast<blender::PackedFile *>(packed_file_ptr);
        ref<MemoryStream> stream = new MemoryStream((void *) packed_file->data, packed_file->size);
        ref<Bitmap> bmp = new Bitmap(stream.get());
        return bmp;
    });
}
