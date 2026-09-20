#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/packed.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-bitmap:

Bitmap texture (:monosp:`bitmap`)
---------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the bitmap to be loaded.

 * - index
   - |int|
   - When :monosp:`filename` refers to a ``.packed`` texture container,
     this parameter can optionally be used to select a file by index. (Default: 0)

 * - name
   - |string|
   - When :monosp:`filename` refers to a ``.packed`` texture container,
     this parameter can optionally be used to select a file by name.

 * - bitmap
   - :monosp:`Bitmap object`
   - When creating a Bitmap texture at runtime, e.g. from Python or C++,
     an existing Bitmap image instance can be passed directly rather than
     loading it from the filesystem with :paramtype:`filename`.

 * - data
   - |tensor|
   - Tensor array containing the texture data. Similarly to the
     :paramtype:`bitmap` parameter, this field can only be used at runtime.
     The data is assumed to be linear. Unless :paramtype:`raw` is set,
     spectral variants upsample three-channel tensors like any other
     color texture.
   - |exposed|, |differentiable|

 * - filter_type
   - |string|
   - Specifies how pixel values are interpolated and filtered when queried over
     larger UV regions. The following options are currently available:

     - ``bilinear`` (default): perform bilinear interpolation, but no filtering.

     - ``nearest``: disable filtering and interpolation. In this mode, the
       plugin performs nearest neighbor lookups of texture values.

     - ``trilinear``: build a MIP pyramid and blend the two levels that
       match the size of the lookup's footprint. See the discussion of
       texture filtering below.

     - ``anisotropic``: like ``trilinear``, but with additional taps along
       the major axis of the footprint, up to :paramtype:`max_anisotropy`.

     The two filtered modes are unsupported for color textures in spectral
     variants and fall back to ``bilinear`` there.

 * - max_anisotropy
   - |int|
   - Upper bound on the number of taps used by ``anisotropic`` filtering, in
     the range :math:`[1, 16]`. (Default: 8)

 * - wrap_mode
   - |string|
   - Controls the behavior of texture evaluations that fall outside of the
     :math:`[0, 1]` range. The following options are currently available:

     - ``repeat`` (default): tile the texture infinitely.

     - ``mirror``: mirror the texture along its boundaries.

     - ``clamp``: clamp coordinates to the edge of the texture.

 * - format
   - |string|
   - Specifies the underlying texture storage format. The following options are
     currently available:

     - ``auto`` (default): Match the storage precision to the source: use 8 bits
         per channel for 8-bit images, half precision for 16-bit images, and
         otherwise the native floating point representation of the Mitsuba
         variant. Note that 8-bit storage is not differentiable. Request
         ``float16`` or ``variant`` to optimize such textures.

     - ``variant``: Use the corresponding native floating point representation
         of the Mitsuba variant

     - ``float16``: Store the texture in half precision

     - ``uint8``: Store the texture using 8 bits per channel. When the source
         image is sRGB-encoded and :paramtype:`raw` is :monosp:`false`, the
         values are linearized on each lookup. This mode is the most memory
         efficient, but note that the texture is *not* differentiable.

 * - raw
   - |bool|
   - Should the transformation to the stored color data (e.g. sRGB to linear,
     spectral upsampling) be disabled? You will want to enable this when working
     with bitmaps storing normal maps that use a linear encoding.
     (Default: false)

 * - to_uv
   - |transform|
   - Specifies an optional 3x3 transformation matrix that will be applied to UV
     values. A 4x4 matrix can also be provided, in which case the extra row and
     column are ignored.
   - |exposed|

 * - accel
   - |bool|
   - Hardware acceleration features can be used in CUDA mode. These features can
     cause small differences as hardware interpolation methods typically have a
     loss of precision (not exactly 32-bit arithmetic). (Default: true)

This plugin provides a bitmap texture that performs interpolated lookups given
a JPEG, PNG, OpenEXR, RGBE, TGA, or BMP input file.

When loading the plugin, the data is first converted into a usable color
representation for the renderer:

* In :monosp:`rgb` modes, sRGB textures are converted into a linear color space.
* In :monosp:`spectral` modes, sRGB textures are *spectrally upsampled* to
  plausible smooth spectra :cite:`Jakob2019Spectral`.
* In :monosp:`monochrome` modes, sRGB textures are converted to grayscale.

These conversions can alternatively be disabled with the :paramtype:`raw` flag,
e.g. when textured data is already in linear space or does not represent colors
at all.

**Texture filtering.** A distant or obliquely viewed texture covers many
texels per pixel, which can introduce significant variance and aliasing,
especially at low sampling rates. Mitsuba can optionally use *ray cones*
:cite:`AkenineMoller2019RayCones` to track the elliptical texture region
visible within a pixel and perform filtered texture lookups that average
over this footprint. Set :paramtype:`filter_type` to ``trilinear`` or
``anisotropic`` to enable this behavior. The trilinear filter reduces the
footprint to a single level of detail, while the anisotropic filter takes
several taps along its major axis, which is slower but produces better
quality. Filtering currently only affects directly visible surfaces, and the
:monosp:`cone_scale` sensor parameter adjusts the footprint size.

.. figure:: ../../resources/data/docs/images/textures/bitmap_filtering.svg
   :width: 100%

   Single-sample renderings of a checkerboard plane with the ``bilinear``,
   ``trilinear``, and ``anisotropic`` filter modes, each next to a converged
   reference. Unfiltered lookups alias near the horizon, trilinear filtering
   removes the aliasing but blurs the grazing view, and anisotropic filtering
   preserves most detail.

**Block-compressed textures.** The plugin supports block-compressed texture
formats (BC4, BC5, and BC7), which greatly reduce memory usage when rendering
large textured assets on a GPU. The Metal and CUDA backends decode such textures
in hardware, while the LLVM backend unpacks them into regular textures. Spectral
variants unpack color textures, since the spectral upsampling step is
incompatible with block compression. Block-compressed textures use 8 bits per
channel, are not differentiable, and ignore the :paramtype:`format` parameter.
The ``trilinear`` and ``anisotropic`` filters require a container entry with a
complete MIP chain and otherwise fall back to ``bilinear``.

To use this feature, run ``python -m mitsuba.pack_tex <scene.xml>``, which
packs the textures of a scene into a ``.packed`` container (see the :ref:`file
format description <sec-pack-format>`, which also explains the role of the
different BC variants) and writes an updated scene referencing it. The bitmap
textures of that scene specify the container as :paramtype:`filename` and select
an entry via :paramtype:`index`.

.. tabs::
    .. code-tab:: xml
        :name: bitmap-texture

        <texture type="bitmap">
            <string name="filename" value="texture.png"/>
            <string name="wrap_mode" value="mirror"/>
        </texture>

    .. code-tab:: python

        'type': 'bitmap',
        'filename': 'texture.png',
        'wrap_mode': 'mirror'

*/

// Forward declaration of specialized bitmap texture
template <typename Float, typename Spectrum, typename StoredType, bool Upsample>
class BitmapTextureImpl;

NAMESPACE_BEGIN(detail)
/// Class name tagged with the storage precision (for diagnostics / RTTI), since
/// it depends on a template parameter that MI_DECLARE_CLASS() cannot capture.
template <typename StoredType>
constexpr const char *bitmap_class_name() {
    using StoredScalar = dr::scalar_t<StoredType>;
    if constexpr (std::is_same_v<StoredScalar, double>)
        return "BitmapTextureImpl[float64]";
    else if constexpr (std::is_same_v<StoredScalar, float>)
        return "BitmapTextureImpl[float32]";
    else if constexpr (std::is_same_v<StoredScalar, dr::half>)
        return "BitmapTextureImpl[float16]";
    else if constexpr (std::is_same_v<StoredScalar, uint8_t>)
        return "BitmapTextureImpl[uint8]";
    else
        return "BitmapTextureImpl[?]";
}

/// Header of a block-compressed texture entry in a ``.packed`` container
struct BCHeader {
    uint8_t format = 0, srgb = 0, n_levels = 0, channels = 0;
    uint32_t width = 0, height = 0;

    /// Bytes of the compressed representation of MIP level ``level``
    size_t level_bytes(uint32_t level) const {
        size_t w = std::max(width >> level, 1u),
               h = std::max(height >> level, 1u);
        return ((w + 3) / 4) * ((h + 3) / 4) * (format == 4 ? 8 : 16);
    }

    /// Number of levels of a complete MIP chain
    uint32_t full_mip_levels() const {
        return 1 + dr::log2i(std::max(width, height));
    }
};
NAMESPACE_END(detail)

template <typename Float, typename Spectrum>
class BitmapTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    /// Storage precision of the texture (resolved from the `format` property)
    enum class Format {
        Auto,     ///< 8-bit for 8-bit sources, half for 16-bit, native otherwise
        Variant,  ///< The variant's native floating point precision
        Float16,  ///< Half precision
        UInt8     ///< 8 bits per channel (optionally sRGB-decoded on lookup)
    };

    BitmapTexture(const Properties &props) : Texture(props) {
        m_transform = props.get<ScalarAffineTransform3f>("to_uv", ScalarAffineTransform3f());

        // Should Mitsuba disable transformations to the stored color data?
        // (e.g. sRGB to linear, spectral upsampling, etc.)
        m_raw = props.get<bool>("raw", false);
        m_accel = props.get<bool>("accel", true);

        // Filter mode
        {
            std::string_view filter_mode_str = props.get<std::string_view>("filter_type", "bilinear");
            int max_aniso = props.get<int>("max_anisotropy", 8);
            if (max_aniso < 1 || max_aniso > 16)
                Throw("Invalid \"max_anisotropy\" value %i, must be in "
                      "the range [1, 16]!", max_aniso);

            m_filter_mode = dr::FilterMode::Linear;
            m_mip_filter = dr::MipFilter::Disabled;
            m_max_aniso = 1;
            if (filter_mode_str == "nearest") {
                m_filter_mode = dr::FilterMode::Nearest;
            } else if (filter_mode_str == "trilinear") {
                m_mip_filter = dr::MipFilter::Linear;
            } else if (filter_mode_str == "anisotropic") {
                m_mip_filter = dr::MipFilter::Linear;
                m_max_aniso = (uint32_t) max_aniso;
            } else if (filter_mode_str != "bilinear") {
                Throw("Invalid filter type \"%s\", must be one of: \"nearest\", "
                      "\"bilinear\", \"trilinear\", or \"anisotropic\"!",
                      filter_mode_str);
            }
        }

        // Wrap mode
        {
            std::string_view wrap_mode_str = props.get<std::string_view>("wrap_mode", "repeat");
            if (wrap_mode_str == "repeat")
                m_wrap_mode = dr::WrapMode::Repeat;
            else if (wrap_mode_str == "mirror")
                m_wrap_mode = dr::WrapMode::Mirror;
            else if (wrap_mode_str == "clamp")
                m_wrap_mode = dr::WrapMode::Clamp;
            else
                Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
                      "\"mirror\", or \"clamp\"!", wrap_mode_str);
        }

        // Format
        {
            std::string_view format_str = props.get<std::string_view>("format", "auto");
            if (format_str == "auto")
                m_format = Format::Auto;
            else if (format_str == "variant")
                m_format = Format::Variant;
            else if (format_str == "float16" || format_str == "fp16")
                m_format = Format::Float16; // "fp16" kept for backwards compat
            else if (format_str == "uint8")
                m_format = Format::UInt8;
            else
                Throw("Invalid format \"%s\", must be one of: \"auto\", "
                      "\"variant\", \"float16\", or \"uint8\"!", format_str);
        }

        // Store
        {
            if (props.has_property("bitmap")) {
                // Creates a Bitmap texture directly from an existing Bitmap
                if (props.has_property("filename"))
                    Throw("Cannot specify both \"bitmap\" and \"filename\".");
                Log(Debug, "Loading bitmap texture from memory...");
                // Note: ref-counted, so we don't have to worry about lifetime
                ref<Object> other = props.get<ref<Object>>("bitmap");
                Bitmap *b = dynamic_cast<Bitmap *>(other.get());
                if (!b)
                    Throw("Property \"bitmap\" must be a Bitmap instance.");
                m_bitmap = b;
            } else if (props.has_property("filename")) {
                // Creates a Bitmap texture by loading an image from the filesystem
                FileResolver* fs = file_resolver();
                fs::path file_path = fs->resolve(props.get<std::string_view>("filename"));
                m_name = file_path.filename().string();
                Log(Debug, "Loading bitmap texture from \"%s\" ..", m_name);
                if (file_path.extension() == ".packed")
                    load_container(props, file_path);
                else
                    m_bitmap = new Bitmap(file_path);
            } else if (props.has_property("data")) {
                m_tensor = std::move(const_cast<TensorXf&>(props.get_any<TensorXf>("data")));
                if (m_tensor.ndim() != 3)
                    Throw("Bitmap raw tensor has dimension %lu, expected 3",
                        m_tensor.ndim());

                size_t channel_count = m_tensor.shape(2);
                if (channel_count != 1 && channel_count != 3)
                    Throw("Unsupported tensor channel count: %d"
                          "(expected 1 or 3)", channel_count);
            }
        }
    }

    std::vector<ref<Object>> expand() const override {
        return { ref<Object>(expand_impl()) };
    }

    MI_DECLARE_CLASS(BitmapTexture)

protected:
    Object *expand_impl() const {
        if (m_bc_levels)
            return load_blocks();

        // A tensor was provided by the user
        if (!m_bitmap)
            return instantiate<Float>(std::move(m_tensor), /* srgb = */ false);

        // Pick a storage precision and instantiate the matching implementation
        switch (resolve_format()) {
            case Format::UInt8:
                return load_bitmap<dr::replace_scalar_t<Float, uint8_t>>();
            case Format::Float16:
                return load_bitmap<dr::replace_scalar_t<Float, dr::half>>();
            default: // Format::Variant
                return load_bitmap<Float>();
        }
    }

    /// Resolve `Format::Auto` to a concrete storage precision
    Format resolve_format() const {
        if (m_format != Format::Auto)
            return m_format;
        size_t bytes_per_channel =
            m_bitmap->bytes_per_pixel() / m_bitmap->channel_count();
        if (bytes_per_channel == 1)
            return Format::UInt8;
        else if (bytes_per_channel == 2)
            return Format::Float16;
        else
            return Format::Variant;
    }

    /// Load the bitmap into a tensor of the given storage type and build
    /// the implementation object
    template <typename StoredType> Object *load_bitmap() const {
        using StoredScalar = dr::scalar_t<StoredType>;
        constexpr bool IsUInt8 = std::is_same_v<StoredScalar, uint8_t>;

        // `raw` data carries no color transform: treat it as already linear
        if (m_raw)
            m_bitmap->set_srgb_gamma(false);

        Bitmap::PixelFormat pf = target_pixel_format(m_bitmap->pixel_format());
        bool srgb = IsUInt8 && m_bitmap->srgb_gamma();

        // Bring the bitmap into the storage format (skipped when already matching)
        m_bitmap = prepare_bitmap(pf, struct_type_v<StoredScalar>,
                                  /* keep_srgb_gamma = */ srgb);

        ScalarVector2i res(m_bitmap->size());
        using StoredTensorXf = dr::replace_scalar_t<TensorXf, StoredScalar>;
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(),
                            m_bitmap->channel_count() };

        return instantiate<StoredType>(
            StoredTensorXf(m_bitmap->data(), 3, shape), srgb);
    }

    /// Bring the source bitmap into the exact format the texture needs
    ref<Bitmap> prepare_bitmap(Bitmap::PixelFormat pf, sj::Type ct,
                               bool keep_srgb_gamma) const {
        ref<Bitmap> bitmap = m_bitmap;

        if (bitmap->pixel_format()     != pf ||
            bitmap->component_format() != ct ||
            bitmap->srgb_gamma()       != keep_srgb_gamma)
            bitmap = bitmap->convert(pf, ct, keep_srgb_gamma);

        // Image must be at least 2x2 pixels in size
        if (dr::any(bitmap->size() < 2))
            bitmap = bitmap->pad_to(ScalarVector2u(2));

        return bitmap;
    }

    /// Map the source pixel format to the 1- or 3-channel layout the texture
    /// stores (alpha is dropped and XYZ converted to RGB by `convert()`).
    Bitmap::PixelFormat target_pixel_format(Bitmap::PixelFormat pf) const {
        switch (pf) {
            case Bitmap::PixelFormat::Y:
            case Bitmap::PixelFormat::YA:
                return Bitmap::PixelFormat::Y;
            case Bitmap::PixelFormat::RGB:
            case Bitmap::PixelFormat::RGBA:
            case Bitmap::PixelFormat::XYZ:
            case Bitmap::PixelFormat::XYZA:
                return Bitmap::PixelFormat::RGB;
            default:
                Throw("The texture needs a known pixel format "
                      "(Y[A], RGB[A], XYZ[A] are supported).");
        }
    }

    /// Read the selected entry of a ``.packed`` texture container
    void load_container(const Properties &props, const fs::path &file_path) {
        ref<PackedFile> file = PackedFile::open(file_path);

        size_t index;
        if (props.has_property("name")) {
            std::string_view name = props.get<std::string_view>("name");
            index = file->find(name);
            if (index == file->entry_count())
                Throw("Error while loading texture \"%s\": the container has "
                      "no entry named \"%s\"!", m_name, name);
        } else {
            int index_prop = props.get<int>("index", 0);
            if (index_prop < 0 || (size_t) index_prop >= file->entry_count())
                Throw("Error while loading texture \"%s\": entry index %i is "
                      "out of range (the container has %zu entries)!",
                      m_name, index_prop, file->entry_count());
            index = (size_t) index_prop;
        }

        PackedFile::Entry entry = file->entry(index);
        m_name = entry.name().empty()
            ? tfm::format("%s[%zu]", m_name, index)
            : tfm::format("%s[%s]", m_name, entry.name());

        auto fail = [&](const char *descr) {
            Throw("Error while loading texture \"%s\": %s!", m_name, descr);
        };

        char tag[4];
        memcpy(tag, entry.data(), 4);
        entry.skip(4);
        if (memcmp(tag, "BTEX", 4) != 0)
            fail("the container entry does not hold a texture");
        if (entry.read<uint32_t>() != 1)
            fail("unsupported texture entry version");

        detail::BCHeader &e = m_bc_header;
        e.format   = entry.read<uint8_t>();
        e.srgb     = entry.read<uint8_t>();
        e.n_levels = entry.read<uint8_t>();
        e.channels = entry.read<uint8_t>();
        e.width    = entry.read<uint32_t>();
        e.height   = entry.read<uint32_t>();

        bool valid_format = (e.format == 4 && e.channels == 1) ||
                            (e.format == 5 && e.channels == 2) ||
                            (e.format == 7 && (e.channels == 3 || e.channels == 4));
        if (!valid_format || e.width == 0 || e.height == 0 ||
            e.n_levels == 0 || e.n_levels > e.full_mip_levels())
            fail("invalid texture entry header");

        // A filtered lookup needs the complete MIP chain from the container.
        // Only the levels that will be used are decompressed.
        m_bc_levels = 1;
        if (m_mip_filter != dr::MipFilter::Disabled && !decompress_bc()) {
            if (e.n_levels == e.full_mip_levels())
                m_bc_levels = e.n_levels;
            else
                Log(Warn, "Bitmap texture \"%s\": the texture container stores "
                          "no MIP chain, falling back to \"bilinear\" filtering.",
                          m_name);
        }
        size_t size = 0;
        for (uint32_t l = 0; l < m_bc_levels; ++l)
            size += e.level_bytes(l);

        // Decompress straight into the array handed to the texture. On the
        // GPU backends, this is a pinned host buffer that the texture upload
        // reads directly.
        uint8_t *ptr = nullptr;
        if constexpr (dr::is_jit_v<Float>)
            ptr = (uint8_t *) jit_malloc(dr::backend_v<Float>, size,
                                         /* shared = */ true);
        else
            m_bc_blocks = dr::empty<BlockStorage>(size);

        try {
            uint8_t *dst = dr::is_jit_v<Float> ? ptr : m_bc_blocks.data();
            for (uint32_t l = 0; l < m_bc_levels; ++l) {
                size_t level_size = e.level_bytes(l);
                entry.read_array(dst, level_size);
                dst += level_size;
            }
        } catch (...) {
            if constexpr (dr::is_jit_v<Float>)
                jit_free(ptr);
            throw;
        }

        if constexpr (dr::is_jit_v<Float>)
            m_bc_blocks = BlockStorage::map_(ptr, size, /* free = */ true);
    }

    /// Decompress block-compressed RGB inputs in spectral variants
    bool decompress_bc() const {
        return is_spectral_v<Spectrum> && !m_raw && m_bc_header.channels >= 3;
    }

    /// Build the implementation object from a container entry
    Object *load_blocks() const {
        using StoredType = dr::replace_scalar_t<Float, uint8_t>;
        using StoredTensorXf = dr::replace_scalar_t<TensorXf, uint8_t>;
        using StoredTexture2f = dr::Texture<StoredType, 2>;
        const detail::BCHeader &e = m_bc_header;

        bool filtered = m_bc_levels > 1, decode = decompress_bc();
        // Color data keeps its sRGB encoding, which the texture decodes on lookup
        bool srgb = e.srgb && !m_raw;
        size_t shape[2] = { e.height, e.width },
               channels = e.channels == 4 ? 3 : e.channels;

        // Without hardware acceleration, the texture decodes on the host
        StoredTexture2f texture(
            shape, channels, (dr::BlockFormat) e.format, m_bc_blocks,
            m_bc_levels, m_accel && !decode, m_filter_mode, m_wrap_mode, srgb,
            filtered ? m_mip_filter : dr::MipFilter::Disabled,
            filtered ? m_max_aniso : 1);

        if (decode) {
            StoredTensorXf tensor = texture.tensor();
            dr::eval(tensor);
            return instantiate<StoredType>(std::move(tensor), srgb);
        }

        return new BitmapTextureImpl<Float, Spectrum, StoredType, false>(
            Properties(), m_name, m_transform, m_raw, srgb, std::move(texture));
    }

    /// Construct the concrete `BitmapTextureImpl` for the chosen storage type
    template <typename StoredType, typename Tensor>
    Object *instantiate(Tensor &&tensor, bool srgb) const {
        if constexpr (is_spectral_v<Spectrum>) {
            if (!m_raw && tensor.shape()[2] == 3) {
                // Spectral upsampling is nonlinear, which rules out MIP mapping
                if (m_mip_filter != dr::MipFilter::Disabled)
                    Log(Warn, "Bitmap texture \"%s\": filtered lookups are "
                              "unsupported for color textures in spectral "
                              "variants, falling back to \"bilinear\".", m_name);

                return new BitmapTextureImpl<Float, Spectrum, StoredType, true>(
                    Properties(), m_name, m_transform, m_filter_mode,
                    m_wrap_mode, dr::MipFilter::Disabled, 1, m_raw, m_accel,
                    srgb, std::forward<Tensor>(tensor));
            }
        }

        return new BitmapTextureImpl<Float, Spectrum, StoredType, false>(
            Properties(), m_name, m_transform, m_filter_mode, m_wrap_mode,
            m_mip_filter, m_max_aniso, m_raw, m_accel, srgb,
            std::forward<Tensor>(tensor));
    }

private:
    // Requested storage precision
    Format m_format;

    // Use hardware-accelerated texture lookups when available?
    bool m_accel;

    // Disable color transformations (sRGB decoding, spectral upsampling)?
    bool m_raw;

    // Transformation applied to UV coordinates
    ScalarAffineTransform3f m_transform;

    // Name used in diagnostics
    std::string m_name;

    // Interpolation mode within a MIP level
    dr::FilterMode m_filter_mode;

    // Handling of lookups outside of [0, 1]
    dr::WrapMode m_wrap_mode;

    // MIP level selection of filtered lookups
    dr::MipFilter m_mip_filter;

    // Upper bound on the taps of anisotropic filtering
    uint32_t m_max_aniso;

    // Source image, when loaded from a file or passed via 'bitmap'
    mutable ref<Bitmap> m_bitmap;

    // Source tensor, when passed via 'data'
    TensorXf m_tensor;

    using BlockStorage = DynamicBuffer<dr::replace_scalar_t<Float, uint8_t>>;

    // Header of the selected entry of a packed texture container
    detail::BCHeader m_bc_header;

    // Block-compressed data of that entry
    BlockStorage m_bc_blocks;

    // Number of MIP levels in 'm_bc_blocks' (zero if not block-compressed)
    uint32_t m_bc_levels = 0;

    MI_TRAVERSE_CB(Texture, m_bitmap, m_tensor)
};

/**
 * Bitmap texture with a given storage type
 *
 * With ``Upsample`` set, the texture holds sRGB data that is spectrally upsampled
 * for rendering. The sRGB data then resides in ``m_rgb``, and ``m_texture``
 * stores the derived upsampling coefficients. Otherwise, ``m_texture`` stores
 * the data itself.
 */
template <typename Float, typename Spectrum, typename StoredType, bool Upsample>
class BitmapTextureImpl final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    using StoredScalar           = dr::scalar_t<StoredType>;
    using StoredColor3f          = Color<StoredType, 3>;
    using StoredTensorXf         = dr::replace_scalar_t<TensorXf, StoredScalar>;
    static constexpr bool IsUInt8 = std::is_same_v<StoredScalar, uint8_t>;

    // Upsampling coefficients always use variant precision storage
    using TexelType              = std::conditional_t<Upsample, Float, StoredType>;
    using TexelTexture2f         = dr::Texture<TexelType, 2>;

    template <typename Tensor>
    BitmapTextureImpl(const Properties &props,
                      const std::string& name,
                      const ScalarAffineTransform3f& transform,
                      dr::FilterMode filter_mode,
                      dr::WrapMode wrap_mode,
                      dr::MipFilter mip_filter,
                      uint32_t max_aniso,
                      bool raw,
                      bool accel,
                      bool srgb,
                      Tensor&& tensor) :
        Texture(props),
        m_name(name),
        m_transform(transform),
        m_raw(raw),
        m_srgb(srgb) {

        // The sampling distribution is built on first use
        rebuild_internals(tensor, false, true);

        if constexpr (Upsample) {
            m_rgb = std::forward<Tensor>(tensor);
            m_texture = TexelTexture2f(upsample(), accel, filter_mode,
                                       wrap_mode);
        } else {
            m_texture = TexelTexture2f(std::forward<Tensor>(tensor), accel,
                                       filter_mode, wrap_mode, srgb,
                                       mip_filter, max_aniso);
        }
    }

    /// Wrap an existing (block-compressed, hence 8-bit) texture
    BitmapTextureImpl(const Properties &props,
                      const std::string& name,
                      const ScalarAffineTransform3f& transform,
                      bool raw,
                      bool srgb,
                      TexelTexture2f &&texture) :
        Texture(props),
        m_name(name),
        m_transform(transform),
        m_raw(raw),
        m_srgb(srgb),
        m_texture(std::move(texture)) {
        if (m_transform != ScalarAffineTransform3f())
            dr::make_opaque(m_transform);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("data", data(),
                IsUInt8 ? ParamFlags::NonDifferentiable
                        : ParamFlags::Differentiable);
        cb->put("to_uv", m_transform, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "data")) {
            const dr::vector<size_t> &shape = data().shape();
            const size_t channels = shape[2];
            if (Upsample && channels != 3)
                Throw("parameters_changed(): The bitmap texture \"%s\" changed "
                      "to %d channels, but spectral upsampling requires "
                      "3 channels!",
                      m_name, channels);
            else if (channels != 1 && channels != 2 && channels != 3)
                Throw("parameters_changed(): The bitmap texture \"%s\" changed "
                      "to %d channels, only textures with 1, 2, or 3 "
                      "channels are supported!",
                      m_name, channels);
            else if (shape[0] < 2 || shape[1] < 2)
                Throw("parameters_changed(): The bitmap texture \"%s\" changed "
                      "to %zux%zu, but it must be at least 2x2 pixels in size!",
                      m_name, shape[0], shape[1]);

            if constexpr (Upsample)
                m_texture.set_tensor(upsample());
            else
                m_texture.update_inplace();

            rebuild_internals(data(), m_distr2d != nullptr, true);
        }

        if ((keys.empty() || string::contains(keys, "to_uv")) && m_distr2d)
            check_sampling_transform();
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (is_spectral_v<Spectrum> && !Upsample && channels != 1)
            Throw("eval(): The bitmap texture \"%s\" was queried for a spectrum, "
                  "but its %zu-channel data does not undergo spectral "
                  "upsampling (raw=%s)!",
                  m_name, channels, m_raw ? "true" : "false");

        if (dr::none_or<false>(active))
            return dr::zeros<UnpolarizedSpectrum>();

        if constexpr (is_monochromatic_v<Spectrum>) {
            // Identical to eval_1() in this variant
            return eval_1(si, active);
        } else if constexpr (Upsample) {
            return interpolate_spectral(si, active);
        } else if constexpr (is_spectral_v<Spectrum>) {
            // Only monochromatic textures reach this point (see above)
            return interpolate_1(si, active);
        } else {
            if (channels == 1)
                return interpolate_1(si, active);
            else
                return interpolate_3(si, active);
        }
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        if (channels == 1)
            return interpolate_1(si, active);
        else // 2 or 3 channels
            return luminance(interpolate_3(si, active));
    }

    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (dr::none_or<false>(active))
            return dr::zeros<Vector2f>();

        // The gradient of a nearest-neighbor lookup is zero almost everywhere
        if (m_texture.filter_mode() != dr::FilterMode::Linear)
            return Vector2f(0.f);

        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;
        BilinearWeights bw = bilinear_weights(uv);
        const Point2f &w0 = bw.w0, &w1 = bw.w1;

        Float f00, f10, f01, f11;
        if constexpr (Upsample) {
            dr::Array<Color3f, 4> c = fetch_rgb(bw.i, active);
            f00 = luminance(c[0]);
            f10 = luminance(c[1]);
            f01 = luminance(c[2]);
            f11 = luminance(c[3]);
        } else if (channels == 1) {
            using Data1 = dr::Array<Float, 1>;
            dr::Array<Data1, 4> c =
                m_texture.template eval_fetch<Data1>(uv, active);
            f00 = c[0].x(); f10 = c[1].x();
            f01 = c[2].x(); f11 = c[3].x();
        } else if (channels == 2) {
            dr::Array<Vector2f, 4> c =
                m_texture.template eval_fetch<Vector2f>(uv, active);
            f00 = luminance(reconstruct_normal(c[0]));
            f10 = luminance(reconstruct_normal(c[1]));
            f01 = luminance(reconstruct_normal(c[2]));
            f11 = luminance(reconstruct_normal(c[3]));
        } else { // 3 channels
            dr::Array<Color3f, 4> c =
                m_texture.template eval_fetch<Color3f>(uv, active);
            f00 = luminance(c[0]);
            f10 = luminance(c[1]);
            f01 = luminance(c[2]);
            f11 = luminance(c[3]);
        }

        // Partials w.r.t. pixel coordinate x and y
        Vector2f df_xy{
            dr::fmadd(w0.y(), f10 - f00, w1.y() * (f11 - f01)),
            dr::fmadd(w0.x(), f01 - f00, w1.x() * (f11 - f10))
        };

        // Partials w.r.t. the transformed coordinates
        Vector2f df_st = resolution() * df_xy;

        // Partials w.r.t. u and v (include uv transform by transpose multiply)
        const auto &tm = m_transform.matrix;
        return Vector2f{ dr::fmadd(tm.entry(0, 0),  df_st.x(),
                                   tm.entry(1, 0) * df_st.y()),
                         dr::fmadd(tm.entry(0, 1),  df_st.x(),
                                   tm.entry(1, 1) * df_st.y()) };
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        const size_t channels = m_texture.channel_count();
        if (channels == 1)
            Throw("eval_3(): The bitmap texture \"%s\" was queried for a RGB "
                  "value, but it is monochromatic!",
                  m_name);
        if (dr::none_or<false>(active))
            return dr::zeros<Color3f>();

        return interpolate_3(si, active);
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return { dr::zeros<Point2f>(), dr::zeros<Float>() };

        if (!m_distr2d)
            init_distr();

        auto [pos, pdf, sample2] = m_distr2d->sample(sample, active);

        ScalarVector2i res = resolution();
        ScalarVector2f inv_resolution = dr::rcp(ScalarVector2f(res));

        if (m_texture.filter_mode() == dr::FilterMode::Nearest) {
            sample2 = (Point2f(pos) + sample2) * inv_resolution;
        } else {
            sample2 = (Point2f(pos) + 0.5f + warp::square_to_tent(sample2)) *
                      inv_resolution;

            switch (m_texture.wrap_mode()) {
                case dr::WrapMode::Repeat:
                    sample2[sample2 < 0.f] += 1.f;
                    sample2[sample2 > 1.f] -= 1.f;
                    break;

                // Texel sampling is restricted to [0, 1] and only interpolation
                // with one row/column of pixels beyond that is considered, so
                // both clamp/mirror effectively use the same strategy. No such
                // distinction is needed for the pdf() method.
                case dr::WrapMode::Clamp:
                case dr::WrapMode::Mirror:
                    sample2[sample2 < 0.f] = -sample2;
                    sample2[sample2 > 1.f] = 2.f - sample2;
                    break;
            }
        }

        return { m_transform.inverse() * sample2,
                 pdf_texture(sample2, active) };
    }

    Float pdf_position(const Point2f &pos, Mask active = true) const override {
        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        if (!m_distr2d)
            init_distr();

        return pdf_texture(m_transform * pos, active);
    }

    /// Position sampling density, in the texture's own parameterization
    Float pdf_texture(const Point2f &pos_, Mask active) const {
        ScalarVector2i res = resolution();
        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            BilinearWeights bw = bilinear_weights(pos_);
            const Vector2i &uv_i = bw.i;
            const Point2f &w0 = bw.w0, &w1 = bw.w1;

            Float v00 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(0, 0)),
                                       active),
                  v10 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(1, 0)),
                                       active),
                  v01 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(0, 1)),
                                       active),
                  v11 = m_distr2d->pdf(m_texture.wrap(uv_i + Point2i(1, 1)),
                                       active);

            Float v0 = dr::fmadd(w0.x(), v00, w1.x() * v10),
                  v1 = dr::fmadd(w0.x(), v01, w1.x() * v11);

            return dr::fmadd(w0.y(), v0, w1.y() * v1) * dr::prod(res);
        } else {
            // Scale to bitmap resolution, no shift
            Point2f uv = pos_ * res;

            // Integer pixel positions for nearest-neighbor interpolation
            Vector2i uv_i = m_texture.wrap(dr::floor2int<Vector2i>(uv));

            return m_distr2d->pdf(uv_i, active) * dr::prod(res);
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si, const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if (dr::none_or<false>(active))
            return { dr::zeros<Wavelength>(), dr::zeros<UnpolarizedSpectrum>() };

        if constexpr (is_spectral_v<Spectrum>) {
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths,
                     eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    ScalarVector2i resolution() const override {
        const size_t *shape = m_texture.shape();
        return { (int) shape[1], (int) shape[0] };
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BitmapTexture[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  resolution = \"" << resolution() << "\"," << std::endl
            << "  raw = " << (int) m_raw << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    static constexpr const char *ClassName = detail::bitmap_class_name<StoredType>();
    std::string_view class_name() const override { return ClassName; }

protected:
    /// The texture data exposed by ``traverse()``
    StoredTensorXf &data() {
        if constexpr (Upsample)
            return m_rgb;
        else
            return m_texture.tensor();
    }

    /// Derive the spectral upsampling coefficients from the sRGB data
    TensorXf upsample() const {
        const dr::vector<size_t> &s = m_rgb.shape();
        size_t shape[3] = { s[0], s[1], 3 };
        DynamicBuffer<Float> coeff;

        if constexpr (dr::is_jit_v<Float>) {
            Color3f rgb(decode(dr::unravel<StoredColor3f>(m_rgb.array())));
            coeff = dr::ravel(SRGBModel<Float, Spectrum>::fetch(rgb));
        } else {
            size_t size = s[0] * s[1] * 3;
            coeff = dr::empty<DynamicBuffer<Float>>(size);
            const StoredScalar *in = m_rgb.data();
            ScalarFloat *out = coeff.data();
            for (size_t i = 0; i < size; i += 3) {
                Color3f rgb(decode(in[i]), decode(in[i + 1]), decode(in[i + 2]));
                dr::Array<Float, 3> c = SRGBModel<Float, Spectrum>::fetch(rgb);
                out[i] = c[0];
                out[i + 1] = c[1];
                out[i + 2] = c[2];
            }
        }

        return TensorXf(std::move(coeff), 3, shape);
    }

    /// Look up a texel of the sRGB data, applying the wrap mode
    Color3f rgb_texel(const Vector2i &p, Mask active) const {
        Vector2i q = m_texture.wrap(p);
        UInt32 index = UInt32(q.y() * resolution().x() + q.x());
        return Color3f(
            decode(dr::gather<StoredColor3f>(m_rgb.array(), index, active)));
    }

    /// Fetch the four sRGB texels of a bilinear lookup with base texel ``i``,
    /// in the order of ``Texture::eval_fetch()``
    dr::Array<Color3f, 4> fetch_rgb(const Vector2i &i, Mask active) const {
        return { rgb_texel(i, active),
                 rgb_texel(i + Vector2i(1, 0), active),
                 rgb_texel(i + Vector2i(0, 1), active),
                 rgb_texel(i + Vector2i(1, 1), active) };
    }

    /// Interpolate the sRGB data
    Color3f interpolate_rgb(const Point2f &uv, Mask active) const {
        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            BilinearWeights bw = bilinear_weights(uv);
            return bilerp(bw, fetch_rgb(bw.i, active));
        } else {
            return rgb_texel(dr::floor2int<Vector2i>(uv * resolution()), active);
        }
    }

    /// Interpolation weights (and base texel) of a bilinear lookup
    struct BilinearWeights {
        Vector2i i;        ///< Lower-left integer texel coordinate
        Point2f w0, w1;    ///< Weights toward the lower / upper texel
    };

    /// Blend four texels given in the order of ``Texture::eval_fetch()``
    template <typename Value>
    static Value bilerp(const BilinearWeights &bw, const dr::Array<Value, 4> &v) {
        const Point2f &w0 = bw.w0, &w1 = bw.w1;
        Value v0 = dr::fmadd(w0.x(), v[0], w1.x() * v[1]),
              v1 = dr::fmadd(w0.x(), v[2], w1.x() * v[3]);
        return dr::fmadd(w0.y(), v0, w1.y() * v1);
    }

    /**
     * Compute the bilinear interpolation weights for a texture-space
     * coordinate
     *
     * Applies the half-texel shift between the UV and texel-center conventions,
     * then returns the lower-left integer texel and the interpolation weights.
     */
    BilinearWeights bilinear_weights(const Point2f &uv) const {
        Point2f p    = dr::fmadd(uv, resolution(), -0.5f);
        Vector2i i   = dr::floor2int<Vector2i>(p);
        Point2f w1   = p - Point2f(i),
                w0   = 1.f - w1;
        return { i, w0, w1 };
    }

    /// Does the texture have a MIP pyramid for filtered lookups?
    bool filtered() const override { return m_texture.mip_levels() > 1; }

    /// Columns of the ray cone's UV footprint in the texture's own
    /// parameterization, in the form that ``eval_filtered()`` expects
    MI_INLINE std::pair<Vector2f, Vector2f>
    footprint(const SurfaceInteraction3f &si) const {
        const Matrix2f &m = si.footprint;
        return { m_transform * Vector2f(m(0, 0), m(1, 0)),
                 m_transform * Vector2f(m(0, 1), m(1, 1)) };
    }

    /**
     * Evaluates the texture at the given surface interaction using
     * spectral upsampling
     */
    MI_INLINE UnpolarizedSpectrum
    interpolate_spectral(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        if (m_texture.filter_mode() == dr::FilterMode::Linear) {
            // Fetch the four enclosing texels and spectrally upsample each
            // *before* interpolating. Upsampling is nonlinear, so blending the
            // RGB values first (e.g. via m_texture.eval()) would be incorrect.
            dr::Array<Color3f, 4> v =
                m_texture.template eval_fetch<Color3f>(uv, active);

            dr::Array<UnpolarizedSpectrum, 4> s;
            for (size_t i = 0; i < 4; ++i)
                s[i] = srgb_model_eval<UnpolarizedSpectrum>(v[i], si.wavelengths);

            return bilerp(bilinear_weights(uv), s);
        } else {
            Color3f out = m_texture.template eval<Color3f>(uv, active);

            return srgb_model_eval<UnpolarizedSpectrum>(out, si.wavelengths);
        }
    }

    /**
     * Evaluates the texture at the given surface interaction
     *
     * Should only be used when the texture has exactly 1 channel.
     */
    MI_INLINE Float interpolate_1(const SurfaceInteraction3f &si,
                                   Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        using Data1 = dr::Array<Float, 1>;
        if (filtered()) {
            auto [ddx, ddy] = footprint(si);
            return m_texture.template eval_filtered<Data1>(uv, ddx, ddy, active).x();
        }
        return m_texture.template eval<Data1>(uv, active).x();
    }

    /**
     * Evaluates the texture at the given surface interaction
     *
     * Should only be used when the texture has 2 or 3 channels. A two-channel
     * texture stores the *x* and *y* components of a tangent-space normal map
     * (see \ref reconstruct_normal()).
     */
    MI_INLINE Color3f interpolate_3(const SurfaceInteraction3f &si,
                                     Mask active) const {
        if constexpr (!dr::is_array_v<Mask>)
            active = true;

        Point2f uv = m_transform * si.uv;

        if constexpr (Upsample)
            return interpolate_rgb(uv, active);

        if (m_texture.channel_count() == 2) {
            Vector2f xy;
            if (filtered()) {
                auto [ddx, ddy] = footprint(si);
                xy = m_texture.template eval_filtered<Vector2f>(uv, ddx, ddy, active);
            } else {
                xy = m_texture.template eval<Vector2f>(uv, active);
            }
            return reconstruct_normal(xy);
        }

        if (filtered()) {
            auto [ddx, ddy] = footprint(si);
            return m_texture.template eval_filtered<Color3f>(uv, ddx, ddy, active);
        }
        return m_texture.template eval<Color3f>(uv, active);
    }

    /**
     * Complete a two-channel normal map lookup
     *
     * The stored values encode the *x* and *y* components of a unit normal as
     * ``(n + 1) / 2``. The function derives the (positive) *z* component and
     * returns all three in the same encoding, so that the result matches the
     * lookup of a regular three-channel normal map.
     */
    MI_INLINE Color3f reconstruct_normal(const Vector2f &xy) const {
        Vector2f n = dr::fmadd(xy, 2.f, -1.f);
        Float z = dr::safe_sqrt(1.f - dr::squared_norm(n));
        return Color3f(xy.x(), xy.y(), dr::fmadd(z, .5f, .5f));
    }

    /**
     * Decode a raw stored value to linear, mirroring the texture's own
     * sampling-time conversion
     *
     * For 8-bit storage this normalizes to [0, 1] and undoes the sRGB curve (if
     * enabled); for float/half storage it is a plain cast resolved at compile
     * time. Used to derive the 2D distribution from the stored tensor.
     * Works for a scalar value or a Dr.Jit array of stored values alike.
     */
    template <typename T>
    MI_INLINE dr::replace_scalar_t<T, ScalarFloat> decode(const T &v) const {
        using Result = dr::replace_scalar_t<T, ScalarFloat>;
        if constexpr (IsUInt8) {
            Result f = Result(v) * ScalarFloat(1.0 / 255.0);
            return m_srgb ? dr::srgb_to_linear(f) : f;
        } else {
            return Result(v);
        }
    }

    /**
     * Recompute the 2D sampling distribution (if requested) following an
     * update, and warn about float data outside of [0, 1]. Neither is needed
     * for a plain 8-bit texture, which then costs nothing.
     */
    void rebuild_internals(const StoredTensorXf& tensor, bool init_distr,
                           bool check_range) {
        if (m_transform != ScalarAffineTransform3f())
            dr::make_opaque(m_transform);

        // 8-bit storage cannot leave the [0, 1] range
        check_range &= !m_raw && !IsUInt8;
        if (!init_distr && !check_range)
            return;

        const dr::vector<size_t> &shape = tensor.shape();
        size_t pixel_count = shape[0] * shape[1],
               channels    = shape[2];

        if (channels == 2)
            Throw("Bitmap texture \"%s\": position sampling is not supported "
                  "for two-channel normal map textures.", m_name);

        bool range_issue = false;
        using FloatStorage = DynamicBuffer<Float>;
        FloatStorage values;

        if constexpr (dr::is_jit_v<Float>) {
            if (channels == 3) {
                StoredColor3f stored = dr::gather<StoredColor3f>(
                    tensor.array(), dr::arange<UInt32>(pixel_count));
                Color<FloatStorage, 3> c3(decode(stored));
                values = luminance(c3);
            } else {
                values = decode(tensor.array());
            }

            if (check_range)
                range_issue = dr::any(values < 0 || values > 1);
        } else {
            // Reduce each texel to a single value (luminance, or spectral mean)
            // in one pass, accumulating a [0, 1] range check and, when
            // requested, the per-texel values backing the 2D distribution.
            StoredScalar *ptr = (StoredScalar *) tensor.data();
            ScalarFloat *out = nullptr;
            if (init_distr) {
                values = dr::empty<FloatStorage>(pixel_count);
                out = values.data();
            }

            auto reduce = [&](auto texel_value) {
                for (size_t i = 0; i < pixel_count; ++i) {
                    ScalarFloat v = texel_value(ptr);
                    if (out)
                        *out++ = v;
                    range_issue |= check_range && (v < 0 || v > 1);
                }
            };

            if (channels == 3) {
                reduce([&](StoredScalar *&p) {
                    Color3f c3(decode(p[0]), decode(p[1]), decode(p[2]));
                    p += 3;
                    return ScalarFloat(luminance(c3));
                });
            } else {
                reduce([&](StoredScalar *&p) {
                    return ScalarFloat(decode(*p++));
                });
            }
        }

        if (init_distr)
            m_distr2d = std::make_unique<DiscreteDistribution2D<Float>>(
                values, resolution());

        if (range_issue)
            Log(Warn,
                "BitmapTexture: texture named \"%s\" contains pixels that "
                "exceed the [0, 1] range!",
                m_name);
    }

    /// Construct 2D distribution upon first access, avoid races
    MI_INLINE void init_distr() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_distr2d) {
            check_sampling_transform();
            dr::scoped_eval_scope<Float> guard;
            auto self = const_cast<BitmapTextureImpl *>(this);
            self->rebuild_internals(self->data(), true, false);
        }
    }

protected:
    /// Transferring the texel distribution to the surface parameterization
    /// needs ``to_uv`` to permute the corners of the unit square
    void check_sampling_transform() const {
        const ScalarPoint2f corners[4] = { { 0.f, 0.f }, { 1.f, 0.f },
                                           { 1.f, 1.f }, { 0.f, 1.f } };
        uint32_t hits = 0;
        for (const ScalarPoint2f &c : corners)
            for (uint32_t j = 0; j < 4; ++j)
                if (dr::squared_norm(m_transform * c - corners[j]) < 1e-8f)
                    hits |= 1u << j;

        if (hits != 0xF)
            Throw("Bitmap texture \"%s\": position sampling (e.g. of an area "
                  "emitter's radiance) requires a 'to_uv' transformation that "
                  "maps the unit square onto itself, such as a flip, a "
                  "transpose or a multiple of a 90 degree rotation. Under %s "
                  "a texel has no well-defined surface position.",
                  m_name, m_transform);
    }

    // Name used in diagnostics
    std::string m_name;

    // Transformation applied to UV coordinates
    ScalarAffineTransform3f m_transform;

    // Were color transformations (sRGB decoding, spectral upsampling) disabled?
    bool m_raw;

    // Does 8-bit data need sRGB decoding?
    bool m_srgb;

    // Texture used for lookups, which stores upsampling coefficients if
    // 'Upsample' is set and the texture data otherwise
    TexelTexture2f m_texture;

    // sRGB data from which 'm_texture' is derived if 'Upsample' is set
    StoredTensorXf m_rgb;

    // Guards the construction of 'm_distr2d', since textures may be shared
    // between threads
    mutable std::mutex m_mutex;

    // Sampling distribution, built on first use
    std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;

    MI_TRAVERSE_CB(Texture, m_texture, m_rgb, m_distr2d)
};

MI_EXPORT_PLUGIN(BitmapTexture)

NAMESPACE_END(mitsuba)
