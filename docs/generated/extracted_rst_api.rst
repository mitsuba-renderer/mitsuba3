.. py:class:: mitsuba.core.AnimatedTransform

    Base class: :py:obj:`mitsuba.core.Object`

    Encapsulates an animated 4x4 homogeneous coordinate transformation

    The animation is stored as keyframe animation with linear segments.
    The implementation performs a polar decomposition of each keyframe
    into a 3x3 scale/shear matrix, a rotation quaternion, and a
    translation vector. These will all be interpolated independently at
    eval time.


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Transform4f`):
            *no description available*

    .. py:method:: mitsuba.core.AnimatedTransform.append(overloaded)


        .. py:method:: append(self, arg0, arg1)

            Append a keyframe to the current animated transform

            Parameter ``arg0`` (float):
                *no description available*

            Parameter ``arg1`` (:py:obj:`mitsuba.core.Transform4f`):
                *no description available*

        .. py:method:: append(self, arg0)

            Parameter ``arg0`` (:py:obj:`mitsuba.core.AnimatedTransform.Keyframe`):
                *no description available*

    .. py:method:: mitsuba.core.AnimatedTransform.eval(self, time, unused=True)

        Compatibility wrapper, which strips the mask argument and invokes
        eval()

        Parameter ``time`` (float):
            *no description available*

        Parameter ``unused`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.AnimatedTransform.has_scale(self)

        Determine whether the transformation involves any kind of scaling

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.AnimatedTransform.size(self)

        Return the number of keyframes

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.AnimatedTransform.translation_bounds(self)

        Return an axis-aligned box bounding the amount of translation
        throughout the animation sequence

        Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
            *no description available*

.. py:class:: mitsuba.core.Appender

    Base class: :py:obj:`mitsuba.core.Object`

    This class defines an abstract destination for logging-relevant
    information

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.core.Appender.append(self, level, text)

        Append a line of text with the given log level

        Parameter ``level`` (:py:obj:`mitsuba.core.LogLevel`):
            *no description available*

        Parameter ``text`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Appender.log_progress(self, progress, name, formatted, eta, ptr=None)

        Process a progress message

        Parameter ``progress`` (float):
            Percentage value in [0, 100]

        Parameter ``name`` (str):
            Title of the progress message

        Parameter ``formatted`` (str):
            Formatted string representation of the message

        Parameter ``eta`` (str):
            Estimated time until 100% is reached.

        Parameter ``ptr`` (capsule):
            Custom pointer payload. This is used to express the context of a
            progress message. When rendering a scene, it will usually contain
            a pointer to the associated ``RenderJob``.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.ArgParser

    Minimal command line argument parser

    This class provides a minimal cross-platform command line argument
    parser in the spirit of to GNU getopt. Both short and long arguments
    that accept an optional extra value are supported.

    The typical usage is

    .. code-block:: c

        ArgParser p;
        auto arg0 = p.register("--myParameter");
        auto arg1 = p.register("-f", true);
        p.parse(argc, argv);
        if (*arg0)
            std::cout << "Got --myParameter" << std::endl;
        if (*arg1)
            std::cout << "Got -f " << arg1->value() << std::endl;


    .. py:method:: __init__(self)


    .. py:method:: mitsuba.core.ArgParser.add(overloaded)


        .. py:method:: add(self, prefix, extra=False)

            Register a new argument with the given list of prefixes

            Parameter ``prefixes`` (List[str]):
                A list of command prefixes (i.e. {"-f", "--fast"})

            Parameter ``extra`` (bool):
                Indicates whether the argument accepts an extra argument value

            Parameter ``prefix`` (str):
                *no description available*

            Returns → :py:obj:`mitsuba.core.ArgParser.Arg`:
                *no description available*

        .. py:method:: add(self, prefixes, extra=False)

            Register a new argument with the given prefix

            Parameter ``prefix``:
                A single command prefix (i.e. "-f")

            Parameter ``extra`` (bool):
                Indicates whether the argument accepts an extra argument value

            Returns → :py:obj:`mitsuba.core.ArgParser.Arg`:
                *no description available*

    .. py:method:: mitsuba.core.ArgParser.executable_name(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.ArgParser.parse(self, arg0)

        Parse the given set of command line arguments

        Parameter ``arg0`` (List[str]):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.AtomicFloat

    Atomic floating point data type

    The class implements an an atomic floating point data type (which is
    not possible with the existing overloads provided by ``std::atomic``).
    It internally casts floating point values to an integer storage format
    and uses atomic integer compare and exchange operations to perform
    changes.

    .. py:method:: __init__(self, arg0)

        Initialize the AtomicFloat with a given floating point value

        Parameter ``arg0`` (float):
            *no description available*

        
.. py:class:: mitsuba.core.Bitmap

    Base class: :py:obj:`mitsuba.core.Object`

    General-purpose bitmap class with read and write support for several
    common file formats.

    This class handles loading of PNG, JPEG, BMP, TGA, as well as OpenEXR
    files, and it supports writing of PNG, JPEG and OpenEXR files.

    PNG and OpenEXR files are optionally annotated with string-valued
    metadata, and the gamma setting can be stored as well. Please see the
    class methods and enumerations for further detail.


    .. py:method:: __init__(self, pixel_format, component_format, size, channel_count=0)

        Create a bitmap of the specified type and allocate the necessary
        amount of memory

        Parameter ``pixel_format`` (:py:obj:`mitsuba.core.Bitmap.PixelFormat`):
            Specifies the pixel format (e.g. RGBA or Luminance-only)

        Parameter ``component_format`` (:py:obj:`mitsuba.core.Struct.Type`):
            Specifies how the per-pixel components are encoded (e.g. unsigned
            8 bit integers or 32-bit floating point values). The component
            format struct_type_v<Float> will be translated to the
            corresponding compile-time precision type (Float32 or Float64).

        Parameter ``size`` (enoki.scalar.Vector2u):
            Specifies the horizontal and vertical bitmap size in pixels

        Parameter ``channel_count`` (int):
            Channel count of the image. This parameter is only required when
            ``pixel_format`` = PixelFormat::MultiChannel

        Parameter ``data``:
            External pointer to the image data. If set to ``nullptr``, the
            implementation will allocate memory itself.

    .. py:method:: __init__(self, array, pixel_format=None)

        Initialize a Bitmap from a NumPy array

        Parameter ``array`` (array):
            *no description available*

        Parameter ``pixel_format`` (object):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Bitmap`):
            *no description available*

    .. py:method:: __init__(self, path, format=FileFormat.Auto)

        Parameter ``path`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.core.Bitmap.FileFormat`):
            *no description available*

    .. py:method:: __init__(self, stream, format=FileFormat.Auto)

        Parameter ``stream`` (:py:obj:`mitsuba.core.Stream`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.core.Bitmap.FileFormat`):
            *no description available*

    .. py:class:: mitsuba.core.Bitmap.AlphaTransform

        Members:

            None : No transformation (default)

            Premultiply : No transformation (default)

            Unpremultiply : No transformation (default)

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:class:: mitsuba.core.Bitmap.FileFormat

        Supported image file formats

        Members:

        .. py:data:: PNG

            Portable network graphics

            The following is supported:

            * Loading and saving of 8/16-bit per component bitmaps for all pixel
              formats (Y, YA, RGB, RGBA)

            * Loading and saving of 1-bit per component mask bitmaps

            * Loading and saving of string-valued metadata fields

        .. py:data:: OpenEXR

            OpenEXR high dynamic range file format developed by Industrial Light &
            Magic (ILM)

            The following is supported:

            * Loading and saving of Float16 / Float32/ UInt32 bitmaps with all
              supported RGB/Luminance/Alpha combinations

            * Loading and saving of spectral bitmaps

            * Loading and saving of XYZ tristimulus bitmaps

            * Loading and saving of string-valued metadata fields

            The following is *not* supported:

            * Saving of tiled images, tile-based read access

            * Display windows that are different than the data window

            * Loading of spectrum-valued bitmaps

        .. py:data:: RGBE

            RGBE image format by Greg Ward

            The following is supported

            * Loading and saving of Float32 - based RGB bitmaps

        .. py:data:: PFM

            PFM (Portable Float Map) image format

            The following is supported

            * Loading and saving of Float32 - based Luminance or RGB bitmaps

        .. py:data:: PPM

            PPM (Portable Pixel Map) image format

            The following is supported

            * Loading and saving of UInt8 and UInt16 - based RGB bitmaps

        .. py:data:: JPEG

            Joint Photographic Experts Group file format

            The following is supported:

            * Loading and saving of 8 bit per component RGB and luminance bitmaps

        .. py:data:: TGA

            Truevision Advanced Raster Graphics Array file format

            The following is supported:

            * Loading of uncompressed 8-bit RGB/RGBA files

        .. py:data:: BMP

            Windows Bitmap file format

            The following is supported:

            * Loading of uncompressed 8-bit luminance and RGBA bitmaps

        .. py:data:: Unknown

            Unknown file format

        .. py:data:: Auto

            Automatically detect the file format

            Note: this flag only applies when loading a file. In this case, the
            source stream must support the ``seek()`` operation.

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:class:: mitsuba.core.Bitmap.PixelFormat

        This enumeration lists all pixel format types supported by the Bitmap
        class. This both determines the number of channels, and how they
        should be interpreted

        Members:

        .. py:data:: Y

            Single-channel luminance bitmap

        .. py:data:: YA

            Two-channel luminance + alpha bitmap

        .. py:data:: RGB

            RGB bitmap

        .. py:data:: RGBA

            RGB bitmap + alpha channel

        .. py:data:: XYZ

            XYZ tristimulus bitmap

        .. py:data:: XYZA

            XYZ tristimulus + alpha channel

        .. py:data:: XYZAW

            XYZ tristimulus + alpha channel + weight

        .. py:data:: MultiChannel

            Arbitrary multi-channel bitmap without a fixed interpretation

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:method:: mitsuba.core.Bitmap.accumulate(overloaded)


        .. py:method:: accumulate(self, bitmap, source_offset, target_offset, size)

            Accumulate the contents of another bitmap into the region with the
            specified offset

            Out-of-bounds regions are safely ignored. It is assumed that ``bitmap
            != this``.

            Remark:
                This function throws an exception when the bitmaps use different
                component formats or channels.

            Parameter ``bitmap`` (:py:obj:`mitsuba.core.Bitmap`):
                *no description available*

            Parameter ``source_offset`` (enoki.scalar.Vector2i):
                *no description available*

            Parameter ``target_offset`` (enoki.scalar.Vector2i):
                *no description available*

            Parameter ``size`` (enoki.scalar.Vector2i):
                *no description available*

        .. py:method:: accumulate(self, bitmap, target_offset)

            Accumulate the contents of another bitmap into the region with the
            specified offset

            This convenience function calls the main ``accumulate()``
            implementation with ``size`` set to ``bitmap->size()`` and
            ``source_offset`` set to zero. Out-of-bounds regions are ignored. It
            is assumed that ``bitmap != this``.

            Remark:
                This function throws an exception when the bitmaps use different
                component formats or channels.

            Parameter ``bitmap`` (:py:obj:`mitsuba.core.Bitmap`):
                *no description available*

            Parameter ``target_offset`` (enoki.scalar.Vector2i):
                *no description available*

        .. py:method:: accumulate(self, bitmap)

            Accumulate the contents of another bitmap into the region with the
            specified offset

            This convenience function calls the main ``accumulate()``
            implementation with ``size`` set to ``bitmap->size()`` and
            ``source_offset`` and ``target_offset`` set to zero. Out-of-bounds
            regions are ignored. It is assumed that ``bitmap != this``.

            Remark:
                This function throws an exception when the bitmaps use different
                component formats or channels.

            Parameter ``bitmap`` (:py:obj:`mitsuba.core.Bitmap`):
                *no description available*

    .. py:method:: mitsuba.core.Bitmap.buffer_size(self)

        Return the bitmap size in bytes (excluding metadata)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.bytes_per_pixel(self)

        Return the number bytes of storage used per pixel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.channel_count(self)

        Return the number of channels used by this bitmap

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.clear(self)

        Clear the bitmap to zero

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.component_format(self)

        Return the component format of this bitmap

        Returns → :py:obj:`mitsuba.core.Struct.Type`:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.convert(overloaded)


        .. py:method:: convert(self, pixel_format, component_format, srgb_gamma, alpha_transform=AlphaTransform.None)

            Convert the bitmap into another pixel and/or component format

            This helper function can be used to efficiently convert a bitmap
            between different underlying representations. For instance, it can
            translate a uint8 sRGB bitmap to a linear float32 XYZ bitmap based on
            half-, single- or double-precision floating point-backed storage.

            This function roughly does the following:

            * For each pixel and channel, it converts the associated value into a
              normalized linear-space form (any gamma of the source bitmap is
              removed)

            * gamma correction (sRGB ramp) is applied if ``srgb_gamma`` is
              ``True``

            * The corrected value is clamped against the representable range of
              the desired component format.

            * The clamped gamma-corrected value is then written to the new bitmap

            If the pixel formats differ, this function will also perform basic
            conversions (e.g. spectrum to rgb, luminance to uniform spectrum
            values, etc.)

            Note that the alpha channel is assumed to be linear in both the source
            and target bitmap, hence it won't be affected by any gamma-related
            transformations.

            Remark:
                This ``convert()`` variant usually returns a new bitmap instance.
                When the conversion would just involve copying the original
                bitmap, the function becomes a no-op and returns the current
                instance.

            pixel_format Specifies the desired pixel format

            component_format Specifies the desired component format

            srgb_gamma Specifies whether a sRGB gamma ramp should be applied to
            the ouutput values.

            Parameter ``pixel_format`` (:py:obj:`mitsuba.core.Bitmap.PixelFormat`):
                *no description available*

            Parameter ``component_format`` (:py:obj:`mitsuba.core.Struct.Type`):
                *no description available*

            Parameter ``srgb_gamma`` (bool):
                *no description available*

            Parameter ``alpha_transform`` (:py:obj:`mitsuba.core.Bitmap.AlphaTransform`):
                *no description available*

            Returns → :py:obj:`mitsuba.core.Bitmap`:
                *no description available*

        .. py:method:: convert(self, target)

            Parameter ``target`` (:py:obj:`mitsuba.core.Bitmap`):
                *no description available*

    .. py:method:: mitsuba.core.Bitmap.detect_file_format(arg0)

        Attempt to detect the bitmap file format in a given stream

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Stream`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Bitmap.FileFormat`:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.has_alpha(self)

        Return whether this image has an alpha channel

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.height(self)

        Return the bitmap's height in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.metadata(self)

        Return a Properties object containing the image metadata

        Returns → mitsuba::Properties:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.pixel_count(self)

        Return the total number of pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.pixel_format(self)

        Return the pixel format of this bitmap

        Returns → :py:obj:`mitsuba.core.Bitmap.PixelFormat`:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.premultiplied_alpha(self)

        Return whether the bitmap uses premultiplied alpha

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.resample(overloaded)


        .. py:method:: resample(self, target, rfilter=None, bc=(FilterBoundaryCondition.Clamp, FilterBoundaryCondition.Clamp), clamp=(-inf, inf), temp=None)

            Up- or down-sample this image to a different resolution

            Uses the provided reconstruction filter and accounts for the requested
            horizontal and vertical boundary conditions when looking up data
            outside of the input domain.

            A minimum and maximum image value can be specified to prevent to
            prevent out-of-range values that are created by the resampling
            process.

            The optional ``temp`` parameter can be used to pass an image of
            resolution ``Vector2u(target->width(), this->height())`` to avoid
            intermediate memory allocations.

            Parameter ``target`` (:py:obj:`mitsuba.core.Bitmap`):
                Pre-allocated bitmap of the desired target resolution

            Parameter ``rfilter`` (:py:obj:`mitsuba.render.ReconstructionFilter`):
                A separable image reconstruction filter (default: 2-lobe Lanczos
                filter)

            Parameter ``bch``:
                Horizontal and vertical boundary conditions (default: clamp)

            Parameter ``clamp`` (Tuple[float, float]):
                Filtered image pixels will be clamped to the following range.
                Default: -infinity..infinity (i.e. no clamping is used)

            Parameter ``temp`` (:py:obj:`mitsuba.core.Bitmap`):
                Optional: image for intermediate computations

            Parameter ``bc`` (Tuple[:py:obj:`mitsuba.core.FilterBoundaryCondition`, :py:obj:`mitsuba.core.FilterBoundaryCondition`]):
                *no description available*

        .. py:method:: resample(self, res, rfilter=None, bc=(FilterBoundaryCondition.Clamp, FilterBoundaryCondition.Clamp), clamp=(-inf, inf))

            Up- or down-sample this image to a different resolution

            This version is similar to the above resample() function -- the main
            difference is that it does not work with preallocated bitmaps and
            takes the desired output resolution as first argument.

            Uses the provided reconstruction filter and accounts for the requested
            horizontal and vertical boundary conditions when looking up data
            outside of the input domain.

            A minimum and maximum image value can be specified to prevent to
            prevent out-of-range values that are created by the resampling
            process.

            Parameter ``res`` (enoki.scalar.Vector2u):
                Desired output resolution

            Parameter ``rfilter`` (:py:obj:`mitsuba.render.ReconstructionFilter`):
                A separable image reconstruction filter (default: 2-lobe Lanczos
                filter)

            Parameter ``bch``:
                Horizontal and vertical boundary conditions (default: clamp)

            Parameter ``clamp`` (Tuple[float, float]):
                Filtered image pixels will be clamped to the following range.
                Default: -infinity..infinity (i.e. no clamping is used)

            Parameter ``bc`` (Tuple[:py:obj:`mitsuba.core.FilterBoundaryCondition`, :py:obj:`mitsuba.core.FilterBoundaryCondition`]):
                *no description available*

            Returns → :py:obj:`mitsuba.core.Bitmap`:
                *no description available*

    .. py:method:: mitsuba.core.Bitmap.set_premultiplied_alpha(self, arg0)

        Specify whether the bitmap uses premultiplied alpha

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.set_srgb_gamma(self, arg0)

        Specify whether the bitmap uses an sRGB gamma encoding

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.size(self)

        Return the bitmap dimensions in pixels

        Returns → enoki.scalar.Vector2u:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.split(self)

        Split an multi-channel image buffer (e.g. from an OpenEXR image with
        lots of AOVs) into its constituent layers

        Returns → List[Tuple[str, :py:obj:`mitsuba.core.Bitmap`]]:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.srgb_gamma(self)

        Return whether the bitmap uses an sRGB gamma encoding

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.struct_(self)

        Return a ``Struct`` instance describing the contents of the bitmap
        (const version)

        Returns → :py:obj:`mitsuba.core.Struct`:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.vflip(self)

        Vertically flip the bitmap

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.width(self)

        Return the bitmap's width in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Bitmap.write(overloaded)


        .. py:method:: write(self, stream, format=FileFormat.Auto, quality=-1)

            Write an encoded form of the bitmap to a stream using the specified
            file format

            Parameter ``stream`` (:py:obj:`mitsuba.core.Stream`):
                Target stream that will receive the encoded output

            Parameter ``format`` (:py:obj:`mitsuba.core.Bitmap.FileFormat`):
                Target file format (OpenEXR, PNG, etc.) Detected from the filename
                by default.

            Parameter ``quality`` (int):
                Depending on the file format, this parameter takes on a slightly
                different meaning:

            * PNG images: Controls how much libpng will attempt to compress the
              output (with 1 being the lowest and 9 denoting the highest
              compression). The default argument uses the compression level 5.

            * JPEG images: denotes the desired quality (between 0 and 100). The
              default argument (-1) uses the highest quality (100).

            * OpenEXR images: denotes the quality level of the DWAB compressor,
              with higher values corresponding to a lower quality. A value of 45 is
              recommended as the default for lossy compression. The default argument
              (-1) causes the implementation to switch to the lossless PIZ
              compressor.

        .. py:method:: write(self, path, format=FileFormat.Auto, quality=-1)

            Write an encoded form of the bitmap to a file using the specified file
            format

            Parameter ``stream``:
                Target stream that will receive the encoded output

            Parameter ``format`` (:py:obj:`mitsuba.core.Bitmap.FileFormat`):
                Target file format (FileFormat::OpenEXR, FileFormat::PNG, etc.)
                Detected from the filename by default.

            Parameter ``quality`` (int):
                Depending on the file format, this parameter takes on a slightly
                different meaning:

            * PNG images: Controls how much libpng will attempt to compress the
              output (with 1 being the lowest and 9 denoting the highest
              compression). The default argument uses the compression level 5.

            * JPEG images: denotes the desired quality (between 0 and 100). The
              default argument (-1) uses the highest quality (100).

            * OpenEXR images: denotes the quality level of the DWAB compressor,
              with higher values corresponding to a lower quality. A value of 45 is
              recommended as the default for lossy compression. The default argument
              (-1) causes the implementation to switch to the lossless PIZ
              compressor.

            Parameter ``path`` (:py:obj:`mitsuba.core.filesystem.path`):
                *no description available*

    .. py:method:: mitsuba.core.Bitmap.write_async(self, path, format=FileFormat.Auto, quality=-1)

        Equivalent to write(), but executes asynchronously on a different
        thread

        Parameter ``path`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.core.Bitmap.FileFormat`):
            *no description available*

        Parameter ``quality`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.BoundingBox2f

    Generic n-dimensional bounding box data structure

    Maintains a minimum and maximum position along each dimension and
    provides various convenience functions for querying and modifying
    them.

    This class is parameterized by the underlying point data structure,
    which permits the use of different scalar types and dimensionalities,
    e.g.

    .. code-block:: c

        BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
        BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));


    Template parameter ``T``:
        The underlying point data type (e.g. ``Point2d``)


    .. py:method:: __init__(self)

        Create a new invalid bounding box

        Initializes the components of the minimum and maximum position to
        $\infty$ and $-\infty$, respectively.

    .. py:method:: __init__(self, p)

        Create a collapsed bounding box from a single point

        Parameter ``p`` (enoki.scalar.Vector2f):
            *no description available*

    .. py:method:: __init__(self, min, max)

        Create a bounding box from two positions

        Parameter ``min`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``max`` (enoki.scalar.Vector2f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox2f`):
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.center(self)

        Return the center point

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.clip(self, arg0)

        Clip this bounding box to another bounding box

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox2f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.collapsed(self)

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.contains(overloaded)


        .. py:method:: contains(self, p, strict=False)

            Check whether a point lies *on* or *inside* the bounding box

            Parameter ``p`` (enoki.scalar.Vector2f):
                The point to be tested

            Template parameter ``Strict``:
                Set this parameter to ``True`` if the bounding box boundary should
                be excluded in the test

            Remark:
                In the Python bindings, the 'Strict' argument is a normal function
                parameter with default value ``False``.

            Parameter ``strict`` (bool):
                *no description available*

            Returns → bool:
                *no description available*

        .. py:method:: contains(self, bbox, strict=False)

            Check whether a specified bounding box lies *on* or *within* the
            current bounding box

            Note that by definition, an 'invalid' bounding box (where min=$\infty$
            and max=$-\infty$) does not cover any space. Hence, this method will
            always return *true* when given such an argument.

            Template parameter ``Strict``:
                Set this parameter to ``True`` if the bounding box boundary should
                be excluded in the test

            Remark:
                In the Python bindings, the 'Strict' argument is a normal function
                parameter with default value ``False``.

            Parameter ``bbox`` (:py:obj:`mitsuba.core.BoundingBox2f`):
                *no description available*

            Parameter ``strict`` (bool):
                *no description available*

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.corner(self, arg0)

        Return the position of a bounding box corner

        Parameter ``arg0`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.distance(overloaded)


        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and the point ``p``.

            Parameter ``arg0`` (enoki.scalar.Vector2f):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox2f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.expand(overloaded)


        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another point

            Parameter ``arg0`` (enoki.scalar.Vector2f):
                *no description available*

        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another bounding box

            Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox2f`):
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.extents(self)

        Calculate the bounding box extents

        Returns → enoki.scalar.Vector2f:
            ``max - min``

    .. py:method:: mitsuba.core.BoundingBox2f.major_axis(self)

        Return the dimension index with the index associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.merge(arg0, arg1)

        Merge two bounding boxes

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox2f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.core.BoundingBox2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.BoundingBox2f`:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.minor_axis(self)

        Return the dimension index with the shortest associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.overlaps(self, bbox, strict=False)

        Check two axis-aligned bounding boxes for possible overlap.

        Parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``bbox`` (:py:obj:`mitsuba.core.BoundingBox2f`):
            *no description available*

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            ``True`` If overlap was detected.

    .. py:method:: mitsuba.core.BoundingBox2f.reset(self)

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to $\infty$ and $-\infty$, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.squared_distance(overloaded)


        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and the point ``p``.

            Parameter ``arg0`` (enoki.scalar.Vector2f):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox2f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.surface_area(self)

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.valid(self)

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox2f.volume(self)

        Calculate the n-dimensional volume of the bounding box

        Returns → float:
            *no description available*

.. py:class:: mitsuba.core.BoundingBox3f

    Generic n-dimensional bounding box data structure

    Maintains a minimum and maximum position along each dimension and
    provides various convenience functions for querying and modifying
    them.

    This class is parameterized by the underlying point data structure,
    which permits the use of different scalar types and dimensionalities,
    e.g.

    .. code-block:: c

        BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
        BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));


    Template parameter ``T``:
        The underlying point data type (e.g. ``Point2d``)


    .. py:method:: __init__(self)

        Create a new invalid bounding box

        Initializes the components of the minimum and maximum position to
        $\infty$ and $-\infty$, respectively.

    .. py:method:: __init__(self, p)

        Create a collapsed bounding box from a single point

        Parameter ``p`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: __init__(self, min, max)

        Create a bounding box from two positions

        Parameter ``min`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``max`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox3f`):
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.center(self)

        Return the center point

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.clip(self, arg0)

        Clip this bounding box to another bounding box

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.collapsed(self)

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.contains(overloaded)


        .. py:method:: contains(self, p, strict=False)

            Check whether a point lies *on* or *inside* the bounding box

            Parameter ``p`` (enoki.scalar.Vector3f):
                The point to be tested

            Template parameter ``Strict``:
                Set this parameter to ``True`` if the bounding box boundary should
                be excluded in the test

            Remark:
                In the Python bindings, the 'Strict' argument is a normal function
                parameter with default value ``False``.

            Parameter ``strict`` (bool):
                *no description available*

            Returns → bool:
                *no description available*

        .. py:method:: contains(self, bbox, strict=False)

            Check whether a specified bounding box lies *on* or *within* the
            current bounding box

            Note that by definition, an 'invalid' bounding box (where min=$\infty$
            and max=$-\infty$) does not cover any space. Hence, this method will
            always return *true* when given such an argument.

            Template parameter ``Strict``:
                Set this parameter to ``True`` if the bounding box boundary should
                be excluded in the test

            Remark:
                In the Python bindings, the 'Strict' argument is a normal function
                parameter with default value ``False``.

            Parameter ``bbox`` (:py:obj:`mitsuba.core.BoundingBox3f`):
                *no description available*

            Parameter ``strict`` (bool):
                *no description available*

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.corner(self, arg0)

        Return the position of a bounding box corner

        Parameter ``arg0`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.distance(overloaded)


        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and the point ``p``.

            Parameter ``arg0`` (enoki.scalar.Vector3f):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox3f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.expand(overloaded)


        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another point

            Parameter ``arg0`` (enoki.scalar.Vector3f):
                *no description available*

        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another bounding box

            Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox3f`):
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.extents(self)

        Calculate the bounding box extents

        Returns → enoki.scalar.Vector3f:
            ``max - min``

    .. py:method:: mitsuba.core.BoundingBox3f.major_axis(self)

        Return the dimension index with the index associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.merge(arg0, arg1)

        Merge two bounding boxes

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox3f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.core.BoundingBox3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.minor_axis(self)

        Return the dimension index with the shortest associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.overlaps(self, bbox, strict=False)

        Check two axis-aligned bounding boxes for possible overlap.

        Parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``bbox`` (:py:obj:`mitsuba.core.BoundingBox3f`):
            *no description available*

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            ``True`` If overlap was detected.

    .. py:method:: mitsuba.core.BoundingBox3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Note that this function ignores the ``(mint, maxt)`` interval
        associated with the ray.

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Returns → Tuple[bool, float, float]:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.reset(self)

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to $\infty$ and $-\infty$, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.squared_distance(overloaded)


        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and the point ``p``.

            Parameter ``arg0`` (enoki.scalar.Vector3f):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingBox3f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.surface_area(self)

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.valid(self)

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.BoundingBox3f.volume(self)

        Calculate the n-dimensional volume of the bounding box

        Returns → float:
            *no description available*

.. py:class:: mitsuba.core.BoundingSphere3f

    Generic n-dimensional bounding sphere data structure


    .. py:method:: __init__(self)

        Construct bounding sphere(s) at the origin having radius zero

    .. py:method:: __init__(self, arg0, arg1)

        Create bounding sphere(s) from given center point(s) with given
        size(s)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.BoundingSphere3f`):
            *no description available*

    .. py:method:: mitsuba.core.BoundingSphere3f.contains(self, p, strict=False)

        Check whether a point lies *on* or *inside* the bounding sphere

        Parameter ``p`` (enoki.scalar.Vector3f):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding sphere boundary
            should be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.BoundingSphere3f.empty(self)

        Return whether this bounding sphere has a radius of zero or less.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.BoundingSphere3f.expand(self, arg0)

        Expand the bounding sphere radius to contain another point.

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.BoundingSphere3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Returns → Tuple[bool, float, float]:
            *no description available*

.. py:class:: mitsuba.core.Class

    Stores meta-information about Object instances.

    This class provides a thin layer of RTTI (run-time type information),
    which is useful for doing things like:

    * Checking if an object derives from a certain class

    * Determining the parent of a class at runtime

    * Instantiating a class by name

    * Unserializing a class from a binary data stream

    See also:
        ref, Object

    .. py:method:: mitsuba.core.Class.alias(self)

        Return the scene description-specific alias, if applicable

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Class.name(self)

        Return the name of the class

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Class.parent(self)

        Return the Class object associated with the parent class of nullptr if
        it does not have one.

        Returns → :py:obj:`mitsuba.core.Class`:
            *no description available*

    .. py:method:: mitsuba.core.Class.variant(self)

        Return the variant of the class

        Returns → str:
            *no description available*

.. py:class:: mitsuba.core.ContinuousDistribution

    Continuous 1D probability distribution defined in terms of a regularly
    sampled linear interpolant

    This data structure represents a continuous 1D probability
    distribution that is defined as a linear interpolant of a regularly
    discretized signal. The class provides various routines for
    transforming uniformly distributed samples so that they follow the
    stored distribution. Note that unnormalized probability density
    functions (PDFs) will automatically be normalized during
    initialization. The associated scale factor can be retrieved using the
    function normalization().


    .. py:method:: __init__(self)

        Continuous 1D probability distribution defined in terms of a regularly
        sampled linear interpolant

        This data structure represents a continuous 1D probability
        distribution that is defined as a linear interpolant of a regularly
        discretized signal. The class provides various routines for
        transforming uniformly distributed samples so that they follow the
        stored distribution. Note that unnormalized probability density
        functions (PDFs) will automatically be normalized during
        initialization. The associated scale factor can be retrieved using the
        function normalization().

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.ContinuousDistribution`):
            *no description available*

    .. py:method:: __init__(self, range, pdf)

        Initialize from a given density function on the interval ``range``

        Parameter ``range`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``pdf`` (enoki.dynamic.Float32):
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.cdf(self)

        Return the unnormalized discrete cumulative distribution function over
        intervals

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.empty(self)

        Is the distribution object empty/uninitialized?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.eval_cdf(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.eval_cdf_normalized(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.eval_pdf(self, x, active=True)

        Evaluate the unnormalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.eval_pdf_normalized(self, x, active=True)

        Evaluate the normalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.integral(self)

        Return the original integral of PDF entries before normalization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.normalization(self)

        Return the normalization factor (i.e. the inverse of sum())

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.pdf(self)

        Return the unnormalized discretized probability density function

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.range(self)

        Return the range of the distribution

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            The sampled position.

    .. py:method:: mitsuba.core.ContinuousDistribution.sample_pdf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[float, float]:
            A tuple consisting of

        1. the sampled position. 2. the normalized probability density of the
        sample.

    .. py:method:: mitsuba.core.ContinuousDistribution.size(self)

        Return the number of discretizations

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.ContinuousDistribution.update(self)

        Update the internal state. Must be invoked when changing the pdf or
        range.

        Returns → None:
            *no description available*

.. py:data:: mitsuba.core.DEBUG
    :type: bool
    :value: False

.. py:class:: mitsuba.core.DefaultFormatter

    Base class: :py:obj:`mitsuba.core.Formatter`

    The default formatter used to turn log messages into a human-readable
    form

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.core.DefaultFormatter.has_class(self)

        See also:
            set_has_class

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.has_date(self)

        See also:
            set_has_date

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.has_log_level(self)

        See also:
            set_has_log_level

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.has_thread(self)

        See also:
            set_has_thread

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.set_has_class(self, arg0)

        Should class information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.set_has_date(self, arg0)

        Should date information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.set_has_log_level(self, arg0)

        Should log level information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.DefaultFormatter.set_has_thread(self, arg0)

        Should thread information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.DiscreteDistribution

    Discrete 1D probability distribution

    This data structure represents a discrete 1D probability distribution
    and provides various routines for transforming uniformly distributed
    samples so that they follow the stored distribution. Note that
    unnormalized probability mass functions (PMFs) will automatically be
    normalized during initialization. The associated scale factor can be
    retrieved using the function normalization().


    .. py:method:: __init__(self)

        Discrete 1D probability distribution

        This data structure represents a discrete 1D probability distribution
        and provides various routines for transforming uniformly distributed
        samples so that they follow the stored distribution. Note that
        unnormalized probability mass functions (PMFs) will automatically be
        normalized during initialization. The associated scale factor can be
        retrieved using the function normalization().

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.DiscreteDistribution`):
            *no description available*

    .. py:method:: __init__(self, pmf)

        Initialize from a given probability mass function

        Parameter ``pmf`` (enoki.dynamic.Float32):
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.cdf(self)

        Return the unnormalized cumulative distribution function

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.empty(self)

        Is the distribution object empty/uninitialized?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.eval_cdf(self, index, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        index ``index``

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.eval_cdf_normalized(self, index, active=True)

        Evaluate the normalized cumulative distribution function (CDF) at
        index ``index``

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.eval_pmf(self, index, active=True)

        Evaluate the unnormalized probability mass function (PMF) at index
        ``index``

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.eval_pmf_normalized(self, index, active=True)

        Evaluate the normalized probability mass function (PMF) at index
        ``index``

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.normalization(self)

        Return the normalization factor (i.e. the inverse of sum())

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.pmf(self)

        Return the unnormalized probability mass function

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → int:
            The discrete index associated with the sample

    .. py:method:: mitsuba.core.DiscreteDistribution.sample_pmf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[int, float]:
            A tuple consisting of

        1. the discrete index associated with the sample, and 2. the
        normalized probability value of the sample.

    .. py:method:: mitsuba.core.DiscreteDistribution.sample_reuse(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        The original sample is value adjusted so that it can be reused as a
        uniform variate.

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[int, float]:
            A tuple consisting of

        1. the discrete index associated with the sample, and 2. the re-scaled
        sample value.

    .. py:method:: mitsuba.core.DiscreteDistribution.sample_reuse_pmf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution.

        The original sample is value adjusted so that it can be reused as a
        uniform variate.

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[int, float, float]:
            A tuple consisting of

        1. the discrete index associated with the sample 2. the re-scaled
        sample value 3. the normalized probability value of the sample

    .. py:method:: mitsuba.core.DiscreteDistribution.size(self)

        Return the number of entries

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.sum(self)

        Return the original sum of PMF entries before normalization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution.update(self)

        Update the internal state. Must be invoked when changing the pmf.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.DiscreteDistribution2D

    .. py:method:: mitsuba.core.DiscreteDistribution2D.eval(self, pos, active=True)

        Parameter ``pos`` (enoki.scalar.Vector2u):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution2D.pdf(self, pos, active=True)

        Parameter ``pos`` (enoki.scalar.Vector2u):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.DiscreteDistribution2D.sample(self, sample, active=True)

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2u, float, enoki.scalar.Vector2f]:
            *no description available*

.. py:class:: mitsuba.core.DummyStream

    Base class: :py:obj:`mitsuba.core.Stream`

    Stream implementation that never writes to disk, but keeps track of
    the size of the content being written. It can be used, for example, to
    measure the precise amount of memory needed to store serialized
    content.

    .. py:method:: __init__(self)


.. py:class:: mitsuba.core.FileResolver

    Base class: :py:obj:`mitsuba.core.Object`

    Simple class for resolving paths on Linux/Windows/Mac OS

    This convenience class looks for a file or directory given its name
    and a set of search paths. The implementation walks through the search
    paths in order and stops once the file is found.


    .. py:method:: __init__(self)

        Initialize a new file resolver with the current working directory

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.FileResolver`):
            *no description available*

    .. py:method:: mitsuba.core.FileResolver.append(self, arg0)

        Append an entry to the end of the list of search paths

        Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.FileResolver.clear(self)

        Clear the list of search paths

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.FileResolver.prepend(self, arg0)

        Prepend an entry at the beginning of the list of search paths

        Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.FileResolver.resolve(self, arg0)

        Walk through the list of search paths and try to resolve the input
        path

        Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

.. py:class:: mitsuba.core.FileStream

    Base class: :py:obj:`mitsuba.core.Stream`

    Simple Stream implementation backed-up by a file.

    The underlying file abstraction is std::fstream, and so most
    operations can be expected to behave similarly.

    .. py:method:: __init__(self, p, mode=EMode.ERead)

        Constructs a new FileStream by opening the file pointed by ``p``.
        
        The file is opened in read-only or read/write mode as specified by
        ``mode``.
        
        Throws if trying to open a non-existing file in with write disabled.
        Throws an exception if the file cannot be opened / created.

        Parameter ``p`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Parameter ``mode`` (:py:obj:`mitsuba.core.FileStream.EMode`):
            *no description available*

        
    .. py:class:: mitsuba.core.FileStream.EMode

        Members:

        .. py:data:: ERead

            Opens a file in (binary) read-only mode

        .. py:data:: EReadWrite

            Opens (but never creates) a file in (binary) read-write mode

        .. py:data:: ETruncReadWrite

            Opens (and truncates) a file in (binary) read-write mode

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:method:: mitsuba.core.FileStream.path(self)

        Return the path descriptor associated with this FileStream

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

.. py:class:: mitsuba.core.FilterBoundaryCondition

    When resampling data to a different resolution using
    Resampler::resample(), this enumeration specifies how lookups
    *outside* of the input domain are handled.

    See also:
        Resampler

    Members:

    .. py:data:: Clamp

        Clamp to the outermost sample position (default)

    .. py:data:: Repeat

        Assume that the input repeats in a periodic fashion

    .. py:data:: Mirror

        Assume that the input is mirrored along the boundary

    .. py:data:: Zero

        Assume that the input function is zero outside of the defined domain

    .. py:data:: One

        Assume that the input function is equal to one outside of the defined
        domain

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.core.Formatter

    Base class: :py:obj:`mitsuba.core.Object`

    Abstract interface for converting log information into a human-
    readable format

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.core.Formatter.format(self, level, theClass, thread, file, line, msg)

        Turn a log message into a human-readable format

        Parameter ``level`` (:py:obj:`mitsuba.core.LogLevel`):
            The importance of the debug message

        Parameter ``class_``:
            Originating class or ``nullptr``

        Parameter ``thread`` (mitsuba::Thread):
            Thread, which is reponsible for creating the message

        Parameter ``file`` (str):
            File, which is responsible for creating the message

        Parameter ``line`` (int):
            Associated line within the source file

        Parameter ``msg`` (str):
            Text content associated with the log message

        Parameter ``theClass`` (:py:obj:`mitsuba.core.Class`):
            *no description available*

        Returns → str:
            *no description available*

.. py:class:: mitsuba.core.Frame3f

    Stores a three-dimensional orthonormal coordinate frame

    This class is used to convert between different cartesian coordinate
    systems and to efficiently evaluate trigonometric functions in a
    spherical coordinate system whose pole is aligned with the ``n`` axis
    (e.g. cos_theta(), sin_phi(), etc.).


    .. py:method:: __init__(self)

        Construct a new coordinate frame from a single vector

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Frame3f`):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``arg2`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.cos_phi(v)

        Give a unit direction, this function returns the cosine of the azimuth
        in a reference spherical coordinate system (see the Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.cos_phi_2(v)

        Give a unit direction, this function returns the squared cosine of the
        azimuth in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.cos_theta(v)

        Give a unit direction, this function returns the cosine of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.cos_theta_2(v)

        Give a unit direction, this function returns the square cosine of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.sin_phi(v)

        Give a unit direction, this function returns the sine of the azimuth
        in a reference spherical coordinate system (see the Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.sin_phi_2(v)

        Give a unit direction, this function returns the squared sine of the
        azimuth in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.sin_theta(v)

        Give a unit direction, this function returns the sine of the elevation
        angle in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.sin_theta_2(v)

        Give a unit direction, this function returns the square sine of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.sincos_phi(v)

        Give a unit direction, this function returns the sine and cosine of
        the azimuth in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.sincos_phi_2(v)

        Give a unit direction, this function returns the squared sine and
        cosine of the azimuth in a reference spherical coordinate system (see
        the Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.tan_theta(v)

        Give a unit direction, this function returns the tangent of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.tan_theta_2(v)

        Give a unit direction, this function returns the square tangent of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.to_local(self, v)

        Convert from world coordinates to local coordinates

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.to_world(self, v)

        Convert from local coordinates to world coordinates

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Frame3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Frame3f`:
            *no description available*

.. py:class:: mitsuba.core.Hierarchical2D0

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(hmax(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values=[], normalize=True, enable_sampling=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][0]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``enable_sampling`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.Hierarchical2D0.eval(self, pos, param=[], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D0.invert(self, sample, param=[], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D0.sample(self, sample, param=[], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.Hierarchical2D1

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(hmax(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][1]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.Hierarchical2D1.eval(self, pos, param=[0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D1.invert(self, sample, param=[0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D1.sample(self, sample, param=[0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.Hierarchical2D2

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(hmax(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][2]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.Hierarchical2D2.eval(self, pos, param=[0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D2.invert(self, sample, param=[0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D2.sample(self, sample, param=[0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.Hierarchical2D3

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(hmax(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][3]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.Hierarchical2D3.eval(self, pos, param=[0, 0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D3.invert(self, sample, param=[0, 0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.Hierarchical2D3.sample(self, sample, param=[0, 0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.IrregularContinuousDistribution

    Continuous 1D probability distribution defined in terms of an
    *irregularly* sampled linear interpolant

    This data structure represents a continuous 1D probability
    distribution that is defined as a linear interpolant of an irregularly
    discretized signal. The class provides various routines for
    transforming uniformly distributed samples so that they follow the
    stored distribution. Note that unnormalized probability density
    functions (PDFs) will automatically be normalized during
    initialization. The associated scale factor can be retrieved using the
    function normalization().


    .. py:method:: __init__(self)

        Continuous 1D probability distribution defined in terms of an
        *irregularly* sampled linear interpolant

        This data structure represents a continuous 1D probability
        distribution that is defined as a linear interpolant of an irregularly
        discretized signal. The class provides various routines for
        transforming uniformly distributed samples so that they follow the
        stored distribution. Note that unnormalized probability density
        functions (PDFs) will automatically be normalized during
        initialization. The associated scale factor can be retrieved using the
        function normalization().

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.IrregularContinuousDistribution`):
            *no description available*

    .. py:method:: __init__(self, nodes, pdf)

        Initialize from a given density function discretized on nodes
        ``nodes``

        Parameter ``nodes`` (enoki.dynamic.Float32):
            *no description available*

        Parameter ``pdf`` (enoki.dynamic.Float32):
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.cdf(self)

        Return the unnormalized discrete cumulative distribution function over
        intervals

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.empty(self)

        Is the distribution object empty/uninitialized?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.eval_cdf(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.eval_cdf_normalized(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.eval_pdf(self, x, active=True)

        Evaluate the unnormalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.eval_pdf_normalized(self, x, active=True)

        Evaluate the normalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.integral(self)

        Return the original integral of PDF entries before normalization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.nodes(self)

        Return the nodes of the underlying discretization

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.normalization(self)

        Return the normalization factor (i.e. the inverse of sum())

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.pdf(self)

        Return the unnormalized discretized probability density function

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            The sampled position.

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.sample_pdf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[float, float]:
            A tuple consisting of

        1. the sampled position. 2. the normalized probability density of the
        sample.

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.size(self)

        Return the number of discretizations

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.IrregularContinuousDistribution.update(self)

        Update the internal state. Must be invoked when changing the pdf or
        range.

        Returns → None:
            *no description available*

.. py:function:: mitsuba.core.Log(level, msg)

    Parameter ``level`` (:py:obj:`mitsuba.core.LogLevel`):
        *no description available*

    Parameter ``msg`` (str):
        *no description available*

    Returns → None:
        *no description available*

.. py:class:: mitsuba.core.LogLevel

    Available Log message types

    Members:

    .. py:data:: Trace

        < Trace message, for extremely verbose debugging

    .. py:data:: Debug

        < Debug message, usually turned off

    .. py:data:: Info

        < More relevant debug / information message

    .. py:data:: Warn

        < Warning message

    .. py:data:: Error

        < Error message, causes an exception to be thrown

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.core.Logger

    Base class: :py:obj:`mitsuba.core.Object`

    Responsible for processing log messages

    Upon receiving a log message, the Logger class invokes a Formatter to
    convert it into a human-readable form. Following that, it sends this
    information to every registered Appender.

    .. py:method:: __init__(self, arg0)

        Construct a new logger with the given minimum log level

        Parameter ``arg0`` (:py:obj:`mitsuba.core.LogLevel`):
            *no description available*

        
    .. py:method:: mitsuba.core.Logger.add_appender(self, arg0)

        Add an appender to this logger

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Appender`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Logger.appender(self, arg0)

        Return one of the appenders

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Appender`:
            *no description available*

    .. py:method:: mitsuba.core.Logger.appender_count(self)

        Return the number of registered appenders

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Logger.clear_appenders(self)

        Remove all appenders from this logger

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Logger.error_level(self)

        Return the current error level

        Returns → :py:obj:`mitsuba.core.LogLevel`:
            *no description available*

    .. py:method:: mitsuba.core.Logger.formatter(self)

        Return the logger's formatter implementation

        Returns → :py:obj:`mitsuba.core.Formatter`:
            *no description available*

    .. py:method:: mitsuba.core.Logger.log_level(self)

        Return the current log level

        Returns → :py:obj:`mitsuba.core.LogLevel`:
            *no description available*

    .. py:method:: mitsuba.core.Logger.log_progress(self, progress, name, formatted, eta, ptr=None)

        Process a progress message

        Parameter ``progress`` (float):
            Percentage value in [0, 100]

        Parameter ``name`` (str):
            Title of the progress message

        Parameter ``formatted`` (str):
            Formatted string representation of the message

        Parameter ``eta`` (str):
            Estimated time until 100% is reached.

        Parameter ``ptr`` (capsule):
            Custom pointer payload. This is used to express the context of a
            progress message. When rendering a scene, it will usually contain
            a pointer to the associated ``RenderJob``.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Logger.read_log(self)

        Return the contents of the log file as a string

        Throws a runtime exception upon failure

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Logger.remove_appender(self, arg0)

        Remove an appender from this logger

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Appender`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Logger.set_error_level(self, arg0)

        Set the error log level (this level and anything above will throw
        exceptions).

        The value provided here can be used for instance to turn warnings into
        errors. But *level* must always be less than Error, i.e. it isn't
        possible to cause errors not to throw an exception.

        Parameter ``arg0`` (:py:obj:`mitsuba.core.LogLevel`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Logger.set_formatter(self, arg0)

        Set the logger's formatter implementation

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Formatter`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Logger.set_log_level(self, arg0)

        Set the log level (everything below will be ignored)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.LogLevel`):
            *no description available*

        Returns → None:
            *no description available*

.. py:data:: mitsuba.core.MTS_AUTHORS
    :type: str
    :value: Realistic Graphics Lab, EPFL

.. py:data:: mitsuba.core.MTS_ENABLE_EMBREE
    :type: bool
    :value: False

.. py:data:: mitsuba.core.MTS_ENABLE_OPTIX
    :type: bool
    :value: True

.. py:data:: mitsuba.core.MTS_FILTER_RESOLUTION
    :type: int
    :value: 31

.. py:data:: mitsuba.core.MTS_VERSION
    :type: str
    :value: 2.1.0

.. py:data:: mitsuba.core.MTS_VERSION_MAJOR
    :type: int
    :value: 2

.. py:data:: mitsuba.core.MTS_VERSION_MINOR
    :type: int
    :value: 1

.. py:data:: mitsuba.core.MTS_VERSION_PATCH
    :type: int
    :value: 0

.. py:data:: mitsuba.core.MTS_YEAR
    :type: str
    :value: 2020

.. py:class:: mitsuba.core.MarginalContinuous2D0

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values=[], normalize=True, enable_sampling=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][0]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``enable_sampling`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalContinuous2D0.eval(self, pos, param=[], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D0.invert(self, sample, param=[], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D0.sample(self, sample, param=[], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalContinuous2D1

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][1]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalContinuous2D1.eval(self, pos, param=[0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D1.invert(self, sample, param=[0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D1.sample(self, sample, param=[0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalContinuous2D2

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][2]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalContinuous2D2.eval(self, pos, param=[0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D2.invert(self, sample, param=[0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D2.sample(self, sample, param=[0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalContinuous2D3

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][3]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalContinuous2D3.eval(self, pos, param=[0, 0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D3.invert(self, sample, param=[0, 0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalContinuous2D3.sample(self, sample, param=[0, 0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalDiscrete2D0

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values=[], normalize=True, enable_sampling=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][0]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``enable_sampling`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalDiscrete2D0.eval(self, pos, param=[], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D0.invert(self, sample, param=[], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D0.sample(self, sample, param=[], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector0f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalDiscrete2D1

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][1]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalDiscrete2D1.eval(self, pos, param=[0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D1.invert(self, sample, param=[0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D1.sample(self, sample, param=[0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector1f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalDiscrete2D2

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][2]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalDiscrete2D2.eval(self, pos, param=[0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D2.invert(self, sample, param=[0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D2.sample(self, sample, param=[0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.MarginalDiscrete2D3

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (numpy.ndarray[float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][3]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.core.MarginalDiscrete2D3.eval(self, pos, param=[0, 0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D3.invert(self, sample, param=[0, 0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

    .. py:method:: mitsuba.core.MarginalDiscrete2D3.sample(self, sample, param=[0, 0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``param`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            *no description available*

.. py:class:: mitsuba.core.Matrix2f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2, arg3)

        Parameter ``arg0`` (float):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

        Parameter ``arg2`` (float):
            *no description available*

        Parameter ``arg3`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Parameter ``arg0`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Vector2f):
            *no description available*

    .. py:method:: mitsuba.core.Matrix2f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix2f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix2f.identity(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix2f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix2f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Matrix2f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Matrix2f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix2f:
            *no description available*

.. py:class:: mitsuba.core.Matrix3f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

        Parameter ``arg0`` (float):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

        Parameter ``arg2`` (float):
            *no description available*

        Parameter ``arg3`` (float):
            *no description available*

        Parameter ``arg4`` (float):
            *no description available*

        Parameter ``arg5`` (float):
            *no description available*

        Parameter ``arg6`` (float):
            *no description available*

        Parameter ``arg7`` (float):
            *no description available*

        Parameter ``arg8`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``arg2`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: mitsuba.core.Matrix3f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix3f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix3f.identity(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix3f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix3f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Matrix3f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Matrix3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix3f:
            *no description available*

.. py:class:: mitsuba.core.Matrix4f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15)

        Parameter ``arg0`` (float):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

        Parameter ``arg2`` (float):
            *no description available*

        Parameter ``arg3`` (float):
            *no description available*

        Parameter ``arg4`` (float):
            *no description available*

        Parameter ``arg5`` (float):
            *no description available*

        Parameter ``arg6`` (float):
            *no description available*

        Parameter ``arg7`` (float):
            *no description available*

        Parameter ``arg8`` (float):
            *no description available*

        Parameter ``arg9`` (float):
            *no description available*

        Parameter ``arg10`` (float):
            *no description available*

        Parameter ``arg11`` (float):
            *no description available*

        Parameter ``arg12`` (float):
            *no description available*

        Parameter ``arg13`` (float):
            *no description available*

        Parameter ``arg14`` (float):
            *no description available*

        Parameter ``arg15`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2, arg3)

        Parameter ``arg0`` (enoki.scalar.Vector4f):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Vector4f):
            *no description available*

        Parameter ``arg2`` (enoki.scalar.Vector4f):
            *no description available*

        Parameter ``arg3`` (enoki.scalar.Vector4f):
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.identity(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.look_at(origin, target, up)

        Parameter ``origin`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``target`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``up`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.rotate(axis, angle)

        Parameter ``axis`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.scale(arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.translate(arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:method:: mitsuba.core.Matrix4f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

.. py:class:: mitsuba.core.MemoryMappedFile

    Base class: :py:obj:`mitsuba.core.Object`

    Basic cross-platform abstraction for memory mapped files

    Remark:
        The Python API has one additional constructor
        <tt>MemoryMappedFile(filename, array)<tt>, which creates a new
        file, maps it into memory, and copies the array contents.


    .. py:method:: __init__(self, filename, size)

        Create a new memory-mapped file of the specified size

        Parameter ``filename`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

    .. py:method:: __init__(self, filename, write=False)

        Map the specified file into memory

        Parameter ``filename`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Parameter ``write`` (bool):
            *no description available*

    .. py:method:: __init__(self, filename, array)

        Parameter ``filename`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Parameter ``array`` (array):
            *no description available*

    .. py:method:: mitsuba.core.MemoryMappedFile.can_write(self)

        Return whether the mapped memory region can be modified

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.MemoryMappedFile.create_temporary(arg0)

        Create a temporary memory-mapped file

        Remark:
            When closing the mapping, the file is automatically deleted.
            Mitsuba additionally informs the OS that any outstanding changes
            that haven't yet been written to disk can be discarded (Linux/OSX
            only).

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.core.MemoryMappedFile`:
            *no description available*

    .. py:method:: mitsuba.core.MemoryMappedFile.data(self)

        Return a pointer to the file contents in memory

        Returns → capsule:
            *no description available*

    .. py:method:: mitsuba.core.MemoryMappedFile.filename(self)

        Return the associated filename

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.core.MemoryMappedFile.resize(self, arg0)

        Resize the memory-mapped file

        This involves remapping the file, which will generally change the
        pointer obtained via data()

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.MemoryMappedFile.size(self)

        Return the size of the mapped region

        Returns → int:
            *no description available*

.. py:class:: mitsuba.core.MemoryStream

    Base class: :py:obj:`mitsuba.core.Stream`

    Simple memory buffer-based stream with automatic memory management. It
    always has read & write capabilities.

    The underlying memory storage of this implementation dynamically
    expands as data is written to the stream, à la ``std::vector``.

    .. py:method:: __init__(self, capacity=512)

        Creates a new memory stream, initializing the memory buffer with a
        capacity of ``capacity`` bytes. For best performance, set this
        argument to the estimated size of the content that will be written to
        the stream.

        Parameter ``capacity`` (int):
            *no description available*

        
    .. py:method:: mitsuba.core.MemoryStream.capacity(self)

        Return the current capacity of the underlying memory buffer

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.MemoryStream.owns_buffer(self)

        Return whether or not the memory stream owns the underlying buffer

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.core.Object

    Object base class with builtin reference counting

    This class (in conjunction with the 'ref' reference counter)
    constitutes the foundation of an efficient reference-counted object
    hierarchy. The implementation here is an alternative to standard
    mechanisms for reference counting such as ``std::shared_ptr`` from the
    STL.

    Why not simply use ``std::shared_ptr``? To be spec-compliant, such
    shared pointers must associate a special record with every instance,
    which stores at least two counters plus a deletion function.
    Allocating this record naturally incurs further overheads to maintain
    data structures within the memory allocator. In addition to this, the
    size of an individual ``shared_ptr`` references is at least two data
    words. All of this quickly adds up and leads to significant overheads
    for large collections of instances, hence the need for an alternative
    in Mitsuba.

    In contrast, the ``Object`` class allows for a highly efficient
    implementation that only adds 32 bits to the base object (for the
    counter) and has no overhead for references.


    .. py:method:: __init__(self)

        Default constructor

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Object`):
            *no description available*

    .. py:method:: mitsuba.core.Object.class_(self)

        Return a Class instance containing run-time type information about
        this Object

        See also:
            Class

        Returns → :py:obj:`mitsuba.core.Class`:
            *no description available*

    .. py:method:: mitsuba.core.Object.dec_ref(self, dealloc=True)

        Decrease the reference count of the object and possibly deallocate it.

        The object will automatically be deallocated once the reference count
        reaches zero.

        Parameter ``dealloc`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Object.expand(self)

        Expand the object into a list of sub-objects and return them

        In some cases, an Object instance is merely a container for a number
        of sub-objects. In the context of Mitsuba, an example would be a
        combined sun & sky emitter instantiated via XML, which recursively
        expands into a separate sun & sky instance. This functionality is
        supported by any Mitsuba object, hence it is located this level.

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.core.Object.id(self)

        Return an identifier of the current instance (if available)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Object.inc_ref(self)

        Increase the object's reference count by one

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Object.parameters_changed(self, keys=[])

        Update internal state after applying changes to parameters

        This function should be invoked when attributes (obtained via
        traverse) are modified in some way. The obect can then update its
        internal state so that derived quantities are consistent with the
        change.

        Parameter ``keys`` (List[str]):
            Optional list of names (obtained via traverse) corresponding to
            the attributes that have been modified. Can also be used to notify
            when this function is called from a parent object by adding a
            "parent" key to the list. When empty, the object should assume
            that any attribute might have changed.

        Remark:
            The default implementation does nothing.

        See also:
            TraversalCallback

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Object.ref_count(self)

        Return the current reference count

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Object.traverse(self, cb)

        Traverse the attributes and object graph of this instance

        Implementing this function enables recursive traversal of C++ scene
        graphs. It is e.g. used to determine the set of differentiable
        parameters when using Mitsuba for optimization.

        Remark:
            The default implementation does nothing.

        See also:
            TraversalCallback

        Parameter ``cb`` (:py:obj:`mitsuba.core.TraversalCallback`):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.PCG32

    PCG32 pseudorandom number generator proposed by Melissa O'Neill

    .. py:method:: __init__(self, initstate=9600629759793949339, initseq=15726070495360670683)

        Initialize the pseudorandom number generator with the seed() function

        Parameter ``initstate`` (int):
            *no description available*

        Parameter ``initseq`` (int):
            *no description available*

        
    .. py:method:: mitsuba.core.PCG32.advance(self, arg0)

        Multi-step advance function (jump-ahead, jump-back)

        The method used here is based on Brown, "Random Number Generation with
        Arbitrary Stride", Transactions of the American Nuclear Society (Nov.
        1994). The algorithm is very similar to fast exponentiation.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.PCG32.next_float32(overloaded)


        .. py:method:: next_float32(self)

            Generate a single precision floating point value on the interval [0,
            1)

            Returns → float:
                *no description available*

        .. py:method:: next_float32(self, mask)

            Masked version of next_float32

            Parameter ``mask`` (bool):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.core.PCG32.next_uint32(overloaded)


        .. py:method:: next_uint32(self)

            Generate a uniformly distributed unsigned 32-bit random number

            Returns → int:
                *no description available*

        .. py:method:: next_uint32(self, mask)

            Masked version of next_uint32

            Parameter ``mask`` (bool):
                *no description available*

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.core.PCG32.next_uint32_bounded(overloaded)


        .. py:method:: next_uint32_bounded(self, bound)

            Generate a uniformly distributed integer r, where 0 <= r < bound

            Parameter ``bound`` (int):
                *no description available*

            Returns → int:
                *no description available*

        .. py:method:: next_uint32_bounded(self, bound, mask)

            Parameter ``bound`` (int):
                *no description available*

            Parameter ``mask`` (bool):
                *no description available*

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.core.PCG32.next_uint64(overloaded)


        .. py:method:: next_uint64(self)

            Generate a uniformly distributed unsigned 64-bit random number

            Returns → int:
                *no description available*

        .. py:method:: next_uint64(self, mask)

            Masked version of next_uint64

            Parameter ``mask`` (bool):
                *no description available*

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.core.PCG32.next_uint64_bounded(overloaded)


        .. py:method:: next_uint64_bounded(self, bound)

            Generate a uniformly distributed integer r, where 0 <= r < bound

            Parameter ``bound`` (int):
                *no description available*

            Returns → int:
                *no description available*

        .. py:method:: next_uint64_bounded(self, bound, mask)

            Parameter ``bound`` (int):
                *no description available*

            Parameter ``mask`` (bool):
                *no description available*

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.core.PCG32.seed(self, initstate=9600629759793949339, initseq=15726070495360670683)

        Seed the pseudorandom number generator

        Specified in two parts: a state initializer and a sequence selection
        constant (a.k.a. stream id)

        Parameter ``initstate`` (int):
            *no description available*

        Parameter ``initseq`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.PluginManager

    The object factory is responsible for loading plugin modules and
    instantiating object instances.

    Ordinarily, this class will be used by making repeated calls to the
    create_object() methods. The generated instances are then assembled
    into a final object graph, such as a scene. One such examples is the
    SceneHandler class, which parses an XML scene file by essentially
    translating the XML elements into calls to create_object().

    .. py:method:: mitsuba.core.PluginManager.get_plugin_class(self, name, variant)

        Return the class corresponding to a plugin for a specific variant

        Parameter ``name`` (str):
            *no description available*

        Parameter ``variant`` (str):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Class`:
            *no description available*

    .. py:method:: mitsuba.core.PluginManager.instance()

        Return the global plugin manager

        Returns → :py:obj:`mitsuba.core.PluginManager`:
            *no description available*

.. py:class:: mitsuba.core.Properties

    Associative parameter map for constructing subclasses of Object.

    Note that the Python bindings for this class do not implement the
    various type-dependent getters and setters. Instead, they are accessed
    just like a normal Python map, e.g:

    TODO update

    .. code-block:: c

        myProps = mitsuba.core.Properties("plugin_name")
        myProps["stringProperty"] = "hello"
        myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)



    .. py:method:: __init__(self)

        Construct an empty property container

    .. py:method:: __init__(self, arg0)

        Construct an empty property container with a specific plugin name

        Parameter ``arg0`` (str):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Properties`):
            *no description available*

    .. py:method:: mitsuba.core.Properties.copy_attribute(self, arg0, arg1, arg2)

        Copy a single attribute from another Properties object and potentially
        rename it

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Properties`):
            *no description available*

        Parameter ``arg1`` (str):
            *no description available*

        Parameter ``arg2`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Properties.has_property(self, arg0)

        Verify if a value with the specified name exists

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Properties.id(self)

        Returns a unique identifier associated with this instance (or an empty
        string)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Properties.mark_queried(self, arg0)

        Manually mark a certain property as queried

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            ``True`` upon success

    .. py:method:: mitsuba.core.Properties.merge(self, arg0)

        Merge another properties record into the current one.

        Existing properties will be overwritten with the values from ``props``
        if they have the same name.

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Properties`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Properties.plugin_name(self)

        Get the associated plugin name

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Properties.property_names(self)

        Return an array containing the names of all stored properties

        Returns → List[str]:
            *no description available*

    .. py:method:: mitsuba.core.Properties.remove_property(self, arg0)

        Remove a property with the specified name

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            ``True`` upon success

    .. py:method:: mitsuba.core.Properties.set_id(self, arg0)

        Set the unique identifier associated with this instance

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Properties.set_plugin_name(self, arg0)

        Set the associated plugin name

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Properties.unqueried(self)

        Return the list of un-queried attributed

        Returns → List[str]:
            *no description available*

    .. py:method:: mitsuba.core.Properties.was_queried(self, arg0)

        Check if a certain property was queried

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.core.RadicalInverse

    Base class: :py:obj:`mitsuba.core.Object`

    Efficient implementation of a radical inverse function with prime
    bases including scrambled versions.

    This class is used to implement Halton and Hammersley sequences for
    QMC integration in Mitsuba.

    .. py:method:: __init__(self, max_base=8161, scramble=-1)

        Parameter ``max_base`` (int):
            *no description available*

        Parameter ``scramble`` (int):
            *no description available*


    .. py:method:: mitsuba.core.RadicalInverse.base(self, arg0)

        Returns the n-th prime base used by the sequence

        These prime numbers are used as bases in the radical inverse function
        implementation.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.RadicalInverse.bases(self)

        Return the number of prime bases for which precomputed tables are
        available

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.RadicalInverse.eval(self, base_index, index)

        Calculate the radical inverse function

        This function is used as a building block to construct Halton and
        Hammersley sequences. Roughly, it computes a b-ary representation of
        the input value ``index``, mirrors it along the decimal point, and
        returns the resulting fractional value. The implementation here uses
        prime numbers for ``b``.

        Parameter ``base_index`` (int):
            Selects the n-th prime that is used as a base when computing the
            radical inverse function (0 corresponds to 2, 1->3, 2->5, etc.).
            The value specified here must be between 0 and 1023.

        Parameter ``index`` (int):
            Denotes the index that should be mapped through the radical
            inverse function

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.RadicalInverse.eval_scrambled(self, base_index, index)

        Calculate a scrambled radical inverse function

        This function is used as a building block to construct permuted Halton
        and Hammersley sequence variants. It works like the normal radical
        inverse function eval(), except that every digit is run through an
        extra scrambling permutation.

        Parameter ``base_index`` (int):
            *no description available*

        Parameter ``index`` (int):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.RadicalInverse.inverse_permutation(self, arg0)

        Return the inverse permutation corresponding to the given prime number
        basis

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.RadicalInverse.permutation(self, arg0)

        Return the permutation corresponding to the given prime number basis

        Parameter ``arg0`` (int):
            *no description available*

        Returns → numpy.ndarray[uint16]:
            *no description available*

    .. py:method:: mitsuba.core.RadicalInverse.scramble(self)

        Return the original scramble value

        Returns → int:
            *no description available*

.. py:class:: mitsuba.core.Ray3f

    Simple n-dimensional ray segment data structure

    Along with the ray origin and direction, this data structure
    additionally stores a ray segment [mint, maxt] (whose entries may
    include positive/negative infinity), as well as the componentwise
    reciprocals of the ray direction. That is just done for convenience,
    as these values are frequently required.

    Remark:
        Important: be careful when changing the ray direction. You must
        call update() to compute the componentwise reciprocals as well, or
        Mitsuba's ray-object intersection code may produce undefined
        results.


    .. py:method:: __init__(self)

        Create an unitialized ray

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

    .. py:method:: __init__(self, o, d, time, wavelengths)

        Parameter ``o`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``d`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``time`` (float):
            *no description available*

        Parameter ``wavelengths`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: __init__(self, o, d, mint, maxt, time, wavelengths)

        Parameter ``o`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``d`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``mint`` (float):
            *no description available*

        Parameter ``maxt`` (float):
            *no description available*

        Parameter ``time`` (float):
            *no description available*

        Parameter ``wavelengths`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: __init__(self, other, mint, maxt)

        Parameter ``other`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``mint`` (float):
            *no description available*

        Parameter ``maxt`` (float):
            *no description available*

    .. py:method:: mitsuba.core.Ray3f.d
        :property:

        < Ray direction

    .. py:method:: mitsuba.core.Ray3f.d_rcp
        :property:

        < Componentwise reciprocals of the ray direction

    .. py:method:: mitsuba.core.Ray3f.maxt
        :property:

        < Maximum position on the ray segment

    .. py:method:: mitsuba.core.Ray3f.mint
        :property:

        < Minimum position on the ray segment

    .. py:method:: mitsuba.core.Ray3f.o
        :property:

        < Ray origin

    .. py:method:: mitsuba.core.Ray3f.time
        :property:

        < Time value associated with this ray

    .. py:method:: mitsuba.core.Ray3f.update(self)

        Update the reciprocal ray directions after changing 'd'

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Ray3f.wavelengths
        :property:

        < Wavelength packet associated with the ray

    .. py:method:: mitsuba.core.Ray3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Ray3f`:
            *no description available*

.. py:class:: mitsuba.core.RayDifferential3f

    Base class: :py:obj:`mitsuba.core.Ray3f`

    Ray differential -- enhances the basic ray class with offset rays for
    two adjacent pixels on the view plane


    .. py:method:: __init__(self, ray)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

    .. py:method:: __init__(self, o, d, time, wavelengths)

        Initialize without differentials.

        Parameter ``o`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``d`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``time`` (float):
            *no description available*

        Parameter ``wavelengths`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: mitsuba.core.RayDifferential3f.scale_differential(self, amount)

        Parameter ``amount`` (float):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.core.ReconstructionFilter

    Base class: :py:obj:`mitsuba.core.Object`

    Generic interface to separable image reconstruction filters

    When resampling bitmaps or adding samples to a rendering in progress,
    Mitsuba first convolves them with a image reconstruction filter.
    Various kinds are implemented as subclasses of this interface.

    Because image filters are generally too expensive to evaluate for each
    sample, the implementation of this class internally precomputes an
    discrete representation, whose resolution given by
    MTS_FILTER_RESOLUTION.

    .. py:method:: mitsuba.core.ReconstructionFilter.border_size(self)

        Return the block border size required when rendering with this filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.ReconstructionFilter.eval(self, x, active=True)

        Evaluate the filter function

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ReconstructionFilter.eval_discretized(self, x, active=True)

        Evaluate a discretized version of the filter (generally faster than
        'eval')

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.core.ReconstructionFilter.radius(self)

        Return the filter's width

        Returns → float:
            *no description available*

.. py:class:: mitsuba.core.Resampler

    Utility class for efficiently resampling discrete datasets to
    different resolutions

    Template parameter ``Scalar``:
        Denotes the underlying floating point data type (i.e. ``half``,
        ``float``, or ``double``)

    .. py:method:: __init__(self, rfilter, source_res, target_res)

        Create a new Resampler object that transforms between the specified
        resolutions
        
        This constructor precomputes all information needed to efficiently
        perform the desired resampling operation. For that reason, it is most
        efficient if it can be used over and over again (e.g. to resample the
        equal-sized rows of a bitmap)
        
        Parameter ``source_res`` (int):
            Source resolution
        
        Parameter ``target_res`` (int):
            Desired target resolution

        Parameter ``rfilter`` (:py:obj:`mitsuba.render.ReconstructionFilter`):
            *no description available*

        
    .. py:method:: mitsuba.core.Resampler.boundary_condition(self)

        Return the boundary condition that should be used when looking up
        samples outside of the defined input domain

        Returns → :py:obj:`mitsuba.core.FilterBoundaryCondition`:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.clamp(self)

        Returns the range to which resampled values will be clamped

        The default is -infinity to infinity (i.e. no clamping is used)

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.resample(self, self, source, source_stride, target_stride, channels)

        Resample a multi-channel array and clamp the results to a specified
        valid range

        Parameter ``source`` (int):
            Source array of samples

        Parameter ``target``:
            Target array of samples

        Parameter ``source_stride`` (array):
            Stride of samples in the source array. A value of '1' implies that
            they are densely packed.

        Parameter ``target_stride`` (int):
            Stride of samples in the source array. A value of '1' implies that
            they are densely packed.

        Parameter ``channels`` (int):
            Number of channels to be resampled

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.set_boundary_condition(self, arg0)

        Set the boundary condition that should be used when looking up samples
        outside of the defined input domain

        The default is FilterBoundaryCondition::Clamp

        Parameter ``arg0`` (:py:obj:`mitsuba.core.FilterBoundaryCondition`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.set_clamp(self, arg0)

        If specified, resampled values will be clamped to the given range

        Parameter ``arg0`` (Tuple[float, float]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.source_resolution(self)

        Return the reconstruction filter's source resolution

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.taps(self)

        Return the number of taps used by the reconstruction filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Resampler.target_resolution(self)

        Return the reconstruction filter's target resolution

        Returns → int:
            *no description available*

.. py:class:: mitsuba.core.ScopedSetThreadEnvironment

    RAII-style class to temporarily switch to another thread's logger/file
    resolver

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.ThreadEnvironment`):
            *no description available*


.. py:class:: mitsuba.core.Stream

    Base class: :py:obj:`mitsuba.core.Object`

    Abstract seekable stream class

    Specifies all functions to be implemented by stream subclasses and
    provides various convenience functions layered on top of on them.

    All read**X**() and write**X**() methods support transparent
    conversion based on the endianness of the underlying system and the
    value passed to set_byte_order(). Whenever host_byte_order() and
    byte_order() disagree, the endianness is swapped.

    See also:
        FileStream, MemoryStream, DummyStream

    .. py:class:: mitsuba.core.Stream.EByteOrder

        Defines the byte order (endianness) to use in this Stream

        Members:

        .. py:data:: EBigEndian

            < PowerPC, SPARC, Motorola 68K

        .. py:data:: ELittleEndian

            < x86, x86_64

        .. py:data:: ENetworkByteOrder

            < Network byte order (an alias for big endian)

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:method:: mitsuba.core.Stream.byte_order(self)

        Returns the byte order of this stream.

        Returns → :py:obj:`mitsuba.core.Stream.EByteOrder`:
            *no description available*

    .. py:method:: mitsuba.core.Stream.can_read(self)

        Can we read from the stream?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Stream.can_write(self)

        Can we write to the stream?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Stream.close(self)

        Closes the stream.

        No further read or write operations are permitted.

        This function is idempotent. It may be called automatically by the
        destructor.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.flush(self)

        Flushes the stream's buffers, if any

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.host_byte_order()

        Returns the byte order of the underlying machine.

        Returns → :py:obj:`mitsuba.core.Stream.EByteOrder`:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read(self, arg0)

        Writes a specified amount of data into the stream. \note This does
        **not** handle endianness swapping.

        Throws an exception when not all data could be written.
        Implementations need to handle endianness swap when appropriate.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → bytes:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_bool(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_double(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_float(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_int16(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_int32(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_int64(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_int8(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_line(self)

        Convenience function for reading a line of text from an ASCII file

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_single(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_string(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_uint16(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_uint32(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_uint64(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.read_uint8(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.seek(self, arg0)

        Seeks to a position inside the stream.

        Seeking beyond the size of the buffer will not modify the length of
        its contents. However, a subsequent write should start at the seeked
        position and update the size appropriately.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.set_byte_order(self, arg0)

        Sets the byte order to use in this stream.

        Automatic conversion will be performed on read and write operations to
        match the system's native endianness.

        No consistency is guaranteed if this method is called after performing
        some read and write operations on the system using a different
        endianness.

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Stream.EByteOrder`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.size(self)

        Returns the size of the stream

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Stream.skip(self, arg0)

        Skip ahead by a given number of bytes

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.tell(self)

        Gets the current position inside the stream

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Stream.truncate(self, arg0)

        Truncates the stream to a given size.

        The position is updated to ``min(old_position, size)``. Throws an
        exception if in read-only mode.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write(self, arg0)

        Writes a specified amount of data into the stream. \note This does
        **not** handle endianness swapping.

        Throws an exception when not all data could be written.
        Implementations need to handle endianness swap when appropriate.

        Parameter ``arg0`` (bytes):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_bool(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_double(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (float):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_float(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (float):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_int16(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_int32(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_int64(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_int8(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_line(self, arg0)

        Convenience function for writing a line of text to an ASCII file

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_single(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (float):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_string(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (str):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_uint16(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_uint32(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_uint64(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Stream.write_uint8(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

.. py:class:: mitsuba.core.StreamAppender

    Base class: :py:obj:`mitsuba.core.Appender`

    %Appender implementation, which writes to an arbitrary C++ output
    stream

    .. py:method:: __init__(self, arg0)

        Create a new stream appender
        
        Remark:
            This constructor is not exposed in the Python bindings

        Parameter ``arg0`` (str):
            *no description available*

        
    .. py:method:: mitsuba.core.StreamAppender.logs_to_file(self)

        Does this appender log to a file

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.StreamAppender.read_log(self)

        Return the contents of the log file as a string

        Returns → str:
            *no description available*

.. py:class:: mitsuba.core.Struct

    Base class: :py:obj:`mitsuba.core.Object`

    Descriptor for specifying the contents and in-memory layout of a POD-
    style data record

    Remark:
        The python API provides an additional ``dtype()`` method, which
        returns the NumPy ``dtype`` equivalent of a given ``Struct``
        instance.

    .. py:method:: __init__(self, pack=False, byte_order=ByteOrder.HostByteOrder)

        Create a new ``Struct`` and indicate whether the contents are packed
        or aligned

        Parameter ``pack`` (bool):
            *no description available*

        Parameter ``byte_order`` (:py:obj:`mitsuba.core.Struct.ByteOrder`):
            *no description available*

        
    .. py:class:: mitsuba.core.Struct.ByteOrder

        Members:

            LittleEndian : 

            BigEndian : 

            HostByteOrder : 

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:class:: mitsuba.core.Struct.Field

        Field specifier with size and offset

    .. py:method:: mitsuba.core.Struct.Field.blend
        :property:

        For use with StructConverter::convert()

        Specifies a pair of weights and source field names that will be
        linearly blended to obtain the output field value. Note that this only
        works for floating point fields or integer fields with the
        Flags::Normalized flag. Gamma-corrected fields will be blended in
        linear space.

    .. py:method:: mitsuba.core.Struct.Field.flags
        :property:

        Additional flags

    .. py:method:: mitsuba.core.Struct.Field.is_float(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.Field.is_integer(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.Field.is_signed(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.Field.is_unsigned(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.Field.name
        :property:

        Name of the field

    .. py:method:: mitsuba.core.Struct.Field.offset
        :property:

        Offset within the ``Struct`` (in bytes)

    .. py:method:: mitsuba.core.Struct.Field.range(self)

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.core.Struct.Field.size
        :property:

        Size in bytes

    .. py:method:: mitsuba.core.Struct.Field.type
        :property:

        Type identifier

    .. py:class:: mitsuba.core.Struct.Flags

        Members:

        .. py:data:: Normalized

            Specifies whether an integer field encodes a normalized value in the
            range [0, 1]. The flag is ignored if specified for floating point
            valued fields.

        .. py:data:: Gamma

            Specifies whether the field encodes a sRGB gamma-corrected value.
            Assumes ``Normalized`` is also specified.

        .. py:data:: Weight

            In FieldConverter::convert, when an input structure contains a weight
            field, the value of all entries are considered to be expressed
            relative to its value. Converting to an un-weighted structure entails
            a division by the weight.

        .. py:data:: Assert

            In FieldConverter::convert, check that the field value matches the
            specified default value. Otherwise, return a failure

        .. py:data:: Alpha

            Specifies whether the field encodes an alpha value

        .. py:data:: PremultipliedAlpha

            Specifies whether the field encodes an alpha premultiplied value

        .. py:data:: Default

            In FieldConverter::convert, when the field is missing in the source
            record, replace it by the specified default value

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:class:: mitsuba.core.Struct.Type

        Members:

            Int8 : 

            UInt8 : 

            Int16 : 

            UInt16 : 

            Int32 : 

            UInt32 : 

            Int64 : 

            UInt64 : 

            Float16 : 

            Float32 : 

            Float64 : 

            Invalid : 


        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*

        .. py:method:: __init__(self, dtype)

            Parameter ``dtype`` (dtype):
                *no description available*

    .. py:method:: mitsuba.core.Struct.alignment(self)

        Return the alignment (in bytes) of the data structure

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Struct.append(self, name, type, flags=Flags.???, default=0.0)

        Append a new field to the ``Struct``; determines size and offset
        automatically

        Parameter ``name`` (str):
            *no description available*

        Parameter ``type`` (:py:obj:`mitsuba.core.Struct.Type`):
            *no description available*

        Parameter ``flags`` (int):
            *no description available*

        Parameter ``default`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Struct`:
            *no description available*

    .. py:method:: mitsuba.core.Struct.byte_order(self)

        Return the byte order of the ``Struct``

        Returns → :py:obj:`mitsuba.core.Struct.ByteOrder`:
            *no description available*

    .. py:method:: mitsuba.core.Struct.dtype(self)

        Return a NumPy dtype corresponding to this data structure

        Returns → dtype:
            *no description available*

    .. py:method:: mitsuba.core.Struct.field(self, arg0)

        Look up a field by name (throws an exception if not found)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Struct.Field`:
            *no description available*

    .. py:method:: mitsuba.core.Struct.field_count(self)

        Return the number of fields

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Struct.has_field(self, arg0)

        Check if the ``Struct`` has a field of the specified name

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.is_float(arg0)

        Check whether the given type is a floating point type

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.is_integer(arg0)

        Check whether the given type is an integer type

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.is_signed(arg0)

        Check whether the given type is a signed type

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.is_unsigned(arg0)

        Check whether the given type is an unsigned type

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Struct.range(arg0)

        Return the representable range of the given type

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Struct.Type`):
            *no description available*

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.core.Struct.size(self)

        Return the size (in bytes) of the data structure, including padding

        Returns → int:
            *no description available*

.. py:class:: mitsuba.core.StructConverter

    Base class: :py:obj:`mitsuba.core.Object`

    This class solves the any-to-any problem: effiently converting from
    one kind of structured data representation to another

    Graphics applications often need to convert from one kind of
    structured representation to another, for instance when loading/saving
    image or mesh data. Consider the following data records which both
    describe positions tagged with color data.

    .. code-block:: c

        struct Source { // <-- Big endian! :(
           uint8_t r, g, b; // in sRGB
           half x, y, z;
        };

        struct Target { // <-- Little endian!
           float x, y, z;
           float r, g, b, a; // in linear space
        };


    The record ``Source`` may represent what is stored in a file on disk,
    while ``Target`` represents the expected input of the implementation.
    Not only are the formats (e.g. float vs half or uint8_t, incompatible
    endianness) and encodings different (e.g. gamma correction vs linear
    space), but the second record even has a different order and extra
    fields that don't exist in the first one.

    This class provides a routine convert() which <ol>

    * reorders entries

    * converts between many different formats (u[int]8-64, float16-64)

    * performs endianness conversion

    * applies or removes gamma correction

    * optionally checks that certain entries have expected default values

    * substitutes missing values with specified defaults

    * performs linear transformations of groups of fields (e.g. between
    different RGB color spaces)

    * applies dithering to avoid banding artifacts when converting 2D
    images

    </ol>

    The above operations can be arranged in countless ways, which makes it
    hard to provide an efficient generic implementation of this
    functionality. For this reason, the implementation of this class
    relies on a JIT compiler that generates fast conversion code on demand
    for each specific conversion. The function is cached and reused in
    case the same conversion is needed later on. Note that JIT compilation
    only works on x86_64 processors; other platforms use a slow generic
    fallback implementation.

    .. py:method:: __init__(self, source, target, dither=False)

        Parameter ``source`` (:py:obj:`mitsuba.core.Struct`):
            *no description available*

        Parameter ``target`` (:py:obj:`mitsuba.core.Struct`):
            *no description available*

        Parameter ``dither`` (bool):
            *no description available*


    .. py:method:: mitsuba.core.StructConverter.convert(self, arg0)

        Parameter ``arg0`` (bytes):
            *no description available*

        Returns → bytes:
            *no description available*

    .. py:method:: mitsuba.core.StructConverter.source(self)

        Return the source ``Struct`` descriptor

        Returns → :py:obj:`mitsuba.core.Struct`:
            *no description available*

    .. py:method:: mitsuba.core.StructConverter.target(self)

        Return the target ``Struct`` descriptor

        Returns → :py:obj:`mitsuba.core.Struct`:
            *no description available*

.. py:class:: mitsuba.core.Thread

    Base class: :py:obj:`mitsuba.core.Object`

    Cross-platform thread implementation

    Mitsuba threads are internally implemented via the ``std::thread``
    class defined in C++11. This wrapper class is needed to attach
    additional state (Loggers, Path resolvers, etc.) that is inherited
    when a thread launches another thread.

    .. py:method:: __init__(self, name)

        Parameter ``name`` (str):
            *no description available*


    .. py:class:: mitsuba.core.Thread.EPriority

        Possible priority values for Thread::set_priority()

        Members:

        .. py:data:: EIdlePriority

            

        .. py:data:: ELowestPriority

            

        .. py:data:: ELowPriority

            

        .. py:data:: ENormalPriority

            

        .. py:data:: EHighPriority

            

        .. py:data:: EHighestPriority

            

        .. py:data:: ERealtimePriority

            

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:method:: mitsuba.core.Thread.core_affinity(self)

        Return the core affinity

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.core.Thread.detach(self)

        Detach the thread and release resources

        After a call to this function, join() cannot be used anymore. This
        releases resources, which would otherwise be held until a call to
        join().

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.file_resolver(self)

        Return the file resolver associated with the current thread

        Returns → :py:obj:`mitsuba.core.FileResolver`:
            *no description available*

    .. py:method:: mitsuba.core.Thread.is_critical(self)

        Return the value of the critical flag

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Thread.is_running(self)

        Is this thread still running?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Thread.join(self)

        Wait until the thread finishes

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.logger(self)

        Return the thread's logger instance

        Returns → :py:obj:`mitsuba.core.Logger`:
            *no description available*

    .. py:method:: mitsuba.core.Thread.name(self)

        Return the name of this thread

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.Thread.parent(self)

        Return the parent thread

        Returns → :py:obj:`mitsuba.core.Thread`:
            *no description available*

    .. py:method:: mitsuba.core.Thread.priority(self)

        Return the thread priority

        Returns → :py:obj:`mitsuba.core.Thread.EPriority`:
            *no description available*

    .. py:method:: mitsuba.core.Thread.register_external_thread(arg0)

        Register a new thread (e.g. TBB, Python) with Mituba thread system.
        Returns true upon success.

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.Thread.set_core_affinity(self, arg0)

        Set the core affinity

        This function provides a hint to the operating system scheduler that
        the thread should preferably run on the specified processor core. By
        default, the parameter is set to -1, which means that there is no
        affinity.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.set_critical(self, arg0)

        Specify whether or not this thread is critical

        When an thread marked critical crashes from an uncaught exception, the
        whole process is brought down. The default is ``False``.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.set_file_resolver(self, arg0)

        Set the file resolver associated with the current thread

        Parameter ``arg0`` (:py:obj:`mitsuba.core.FileResolver`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.set_logger(self, arg0)

        Set the logger instance used to process log messages from this thread

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Logger`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.set_name(self, arg0)

        Set the name of this thread

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.set_priority(self, arg0)

        Set the thread priority

        This does not always work -- for instance, Linux requires root
        privileges for this operation.

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Thread.EPriority`):
            *no description available*

        Returns → bool:
            ``True`` upon success.

    .. py:method:: mitsuba.core.Thread.sleep(arg0)

        Sleep for a certain amount of time (in milliseconds)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.start(self)

        Start the thread

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.Thread.thread()

        Return the current thread

        Returns → :py:obj:`mitsuba.core.Thread`:
            *no description available*

    .. py:method:: mitsuba.core.Thread.thread_id()

        Return a unique ID that is associated with this thread

        Returns → int:
            *no description available*

.. py:class:: mitsuba.core.ThreadEnvironment

    Captures a thread environment (logger, file resolver, profiler flags).
    Used with ScopedSetThreadEnvironment

    .. py:method:: __init__(self)


.. py:class:: mitsuba.core.Transform3f

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Transform3f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (array):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (enoki.scalar.Matrix3f):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (enoki.scalar.Matrix3f):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Matrix3f):
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.core.Transform3f.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.core.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.rotate(angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.scale(v)

        Create a scale transformation

        Parameter ``v`` (enoki.scalar.Vector2f):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.transform_point(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector2f):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.transform_vector(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector2f):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.translate(v)

        Create a translation transformation

        Parameter ``v`` (enoki.scalar.Vector2f):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform3f`:
            *no description available*

.. py:class:: mitsuba.core.Transform4f

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Transform4f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (array):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (enoki.scalar.Matrix4f):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (enoki.scalar.Matrix4f):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Matrix4f):
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.extract(self)

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.core.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.from_frame(frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.core.Frame3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.core.Transform4f.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.look_at(origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (enoki.scalar.Vector3f):
            Camera position

        Parameter ``target`` (enoki.scalar.Vector3f):
            Target vector

        Parameter ``up`` (enoki.scalar.Vector3f):
            Up vector

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.orthographic(near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.perspective(fov, near, far)

        Create a perspective transformation. (Maps [near, far] to [0, 1])

        Projects vectors in camera space onto a plane at z=1:

        x_proj = x / z y_proj = y / z z_proj = (far * (z - near)) / (z * (far-
        near))

        Camera-space depths are not mapped linearly!

        Parameter ``fov`` (float):
            Field of view in degrees

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.rotate(axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.scale(v)

        Create a scale transformation

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.to_frame(frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.core.Frame3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.transform_normal(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.transform_point(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.transform_vector(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.translate(v)

        Create a translation transformation

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.core.Transform4f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Transform4f`:
            *no description available*

.. py:class:: mitsuba.core.TraversalCallback

.. py:data:: mitsuba.core.USE_EMBREE
    :type: bool
    :value: False

.. py:data:: mitsuba.core.USE_OPTIX
    :type: bool
    :value: False

.. py:class:: mitsuba.core.Vector0f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector0d):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector0f, arg0: enoki.scalar.Vector0i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector0u):
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0f:
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.eval(self)

        Returns → enoki.scalar.Vector0f:
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0f:
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.managed(self)

        Returns → enoki.scalar.Vector0f:
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector0f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0f:
            *no description available*

.. py:class:: mitsuba.core.Vector0i


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector0i):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector0i, arg0: enoki.scalar.Vector0u) -> None

        Parameter ``arg0`` (enoki.scalar.Vector0d):
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0i:
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.eval(self)

        Returns → enoki.scalar.Vector0i:
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0i:
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.managed(self)

        Returns → enoki.scalar.Vector0i:
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector0i.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0i:
            *no description available*

.. py:class:: mitsuba.core.Vector0u


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector0u):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector0u, arg0: enoki.scalar.Vector0i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector0d):
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0u:
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.eval(self)

        Returns → enoki.scalar.Vector0u:
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0u:
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.managed(self)

        Returns → enoki.scalar.Vector0u:
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector0u.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector0u:
            *no description available*

.. py:class:: mitsuba.core.Vector1f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector1f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector1d):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector1f, arg0: enoki.scalar.Vector1i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector1u):
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1f:
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.eval(self)

        Returns → enoki.scalar.Vector1f:
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1f:
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.managed(self)

        Returns → enoki.scalar.Vector1f:
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector1f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1f:
            *no description available*

.. py:class:: mitsuba.core.Vector1i


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector1i):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector1f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector1i, arg0: enoki.scalar.Vector1u) -> None

        Parameter ``arg0`` (enoki.scalar.Vector1d):
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1i:
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.eval(self)

        Returns → enoki.scalar.Vector1i:
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1i:
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.managed(self)

        Returns → enoki.scalar.Vector1i:
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector1i.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1i:
            *no description available*

.. py:class:: mitsuba.core.Vector1u


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector1u):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector1f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector1u, arg0: enoki.scalar.Vector1i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector1d):
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1u:
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.eval(self)

        Returns → enoki.scalar.Vector1u:
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1u:
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.managed(self)

        Returns → enoki.scalar.Vector1u:
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector1u.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector1u:
            *no description available*

.. py:class:: mitsuba.core.Vector2f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector2f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, x, y)

        Parameter ``x`` (float):
            *no description available*

        Parameter ``y`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector2f, arg0: enoki.scalar.Vector2u) -> None

        11. __init__(self: enoki.scalar.Vector2f, arg0: enoki.scalar.Vector2i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector2d):
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.eval(self)

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.managed(self)

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector2f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

.. py:class:: mitsuba.core.Vector2i


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector2i):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, x, y)

        Parameter ``x`` (int):
            *no description available*

        Parameter ``y`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector2i, arg0: enoki.scalar.Vector2d) -> None

        11. __init__(self: enoki.scalar.Vector2i, arg0: enoki.scalar.Vector2u) -> None

        Parameter ``arg0`` (enoki.scalar.Vector2f):
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.eval(self)

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.managed(self)

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector2i.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2i:
            *no description available*

.. py:class:: mitsuba.core.Vector2u


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector2u):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, x, y)

        Parameter ``x`` (int):
            *no description available*

        Parameter ``y`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector2u, arg0: enoki.scalar.Vector2d) -> None

        11. __init__(self: enoki.scalar.Vector2u, arg0: enoki.scalar.Vector2i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector2f):
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2u:
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.eval(self)

        Returns → enoki.scalar.Vector2u:
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2u:
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.managed(self)

        Returns → enoki.scalar.Vector2u:
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector2u.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector2u:
            *no description available*

.. py:class:: mitsuba.core.Vector3f


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, x, y, z)

        Parameter ``x`` (float):
            *no description available*

        Parameter ``y`` (float):
            *no description available*

        Parameter ``z`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector3f, arg0: enoki.scalar.Vector3u) -> None

        11. __init__(self: enoki.scalar.Vector3f, arg0: enoki.scalar.Vector3i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector3d):
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.eval(self)

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.full(value, size=1)

        Parameter ``value`` (float):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.managed(self)

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

.. py:class:: mitsuba.core.Vector3i


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3i):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, x, y, z)

        Parameter ``x`` (int):
            *no description available*

        Parameter ``y`` (int):
            *no description available*

        Parameter ``z`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector3i, arg0: enoki.scalar.Vector3d) -> None

        11. __init__(self: enoki.scalar.Vector3i, arg0: enoki.scalar.Vector3u) -> None

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3i:
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.eval(self)

        Returns → enoki.scalar.Vector3i:
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3i:
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.managed(self)

        Returns → enoki.scalar.Vector3i:
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector3i.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3i:
            *no description available*

.. py:class:: mitsuba.core.Vector3u


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (enoki.scalar.Vector3u):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (torch_tensor):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (tuple):
            *no description available*

    .. py:method:: __init__(self, x, y, z)

        Parameter ``x`` (int):
            *no description available*

        Parameter ``y`` (int):
            *no description available*

        Parameter ``z`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: enoki.scalar.Vector3u, arg0: enoki.scalar.Vector3d) -> None

        11. __init__(self: enoki.scalar.Vector3u, arg0: enoki.scalar.Vector3i) -> None

        Parameter ``arg0`` (enoki.scalar.Vector3f):
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.empty(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3u:
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.eval(self)

        Returns → enoki.scalar.Vector3u:
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.full(value, size=1)

        Parameter ``value`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3u:
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.managed(self)

        Returns → enoki.scalar.Vector3u:
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.numpy(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.torch(self, eval=True)

        Parameter ``eval`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.core.Vector3u.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → enoki.scalar.Vector3u:
            *no description available*

.. py:class:: mitsuba.core.ZStream

    Base class: :py:obj:`mitsuba.core.Stream`

    Transparent compression/decompression stream based on ``zlib``.

    This class transparently decompresses and compresses reads and writes
    to a nested stream, respectively.

    .. py:method:: __init__(self, child_stream, stream_type=EStreamType.EDeflateStream, level=-1)

        Creates a new compression stream with the given underlying stream.
        This new instance takes ownership of the child stream. The child
        stream must outlive the ZStream.

        Parameter ``child_stream`` (:py:obj:`mitsuba.core.Stream`):
            *no description available*

        Parameter ``stream_type`` (:py:obj:`mitsuba.core.ZStream.EStreamType`):
            *no description available*

        Parameter ``level`` (int):
            *no description available*

        
    .. py:class:: mitsuba.core.ZStream.EStreamType

        Members:

        .. py:data:: EDeflateStream

            < A raw deflate stream

        .. py:data:: EGZipStream

            < A gzip-compatible stream

        .. py:method:: __init__(self, arg0)

            Parameter ``arg0`` (int):
                *no description available*


    .. py:method:: mitsuba.core.ZStream.child_stream(self)

        Returns the child stream of this compression stream

        Returns → object:
            *no description available*

.. py:function:: mitsuba.core.cast_object

    Capsule objects let you wrap a C "void *" pointer in a Python
    object.  They're a way of passing data through the Python interpreter
    without creating your own custom type.

    Capsules are used for communication between extension modules.
    They provide a way for an extension module to export a C interface
    to other extension modules, so that extension modules can use the
    Python import mechanism to link to one another.

.. py:function:: mitsuba.core.casters

    Capsule objects let you wrap a C "void *" pointer in a Python
    object.  They're a way of passing data through the Python interpreter
    without creating your own custom type.

    Capsules are used for communication between extension modules.
    They provide a way for an extension module to export a C interface
    to other extension modules, so that extension modules can use the
    Python import mechanism to link to one another.

.. py:function:: mitsuba.core.cie1931_xyz(wavelength)

    Evaluate the CIE 1931 XYZ color matching functions given a wavelength
    in nanometers

    Parameter ``wavelength`` (float):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.cie1931_y(wavelength)

    Evaluate the CIE 1931 Y color matching function given a wavelength in
    nanometers

    Parameter ``wavelength`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.coordinate_system(n)

    Complete the set {a} to an orthonormal basis {a, b, c}

    Parameter ``n`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → Tuple[enoki.scalar.Vector3f, enoki.scalar.Vector3f]:
        *no description available*

.. py:function:: mitsuba.core.depolarize(arg0)

    Parameter ``arg0`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.filesystem.absolute(arg0)

    Returns an absolute path to the same location pointed by ``p``,
    relative to ``base``.

    See also:
        http ://en.cppreference.com/w/cpp/experimental/fs/absolute)

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → :py:obj:`mitsuba.core.filesystem.path`:
        *no description available*

.. py:function:: mitsuba.core.filesystem.create_directory(arg0)

    Creates a directory at ``p`` as if ``mkdir`` was used. Returns true if
    directory creation was successful, false otherwise. If ``p`` already
    exists and is already a directory, the function does nothing (this
    condition is not treated as an error).

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.core.filesystem.current_path()

    Returns the current working directory (equivalent to getcwd)

    Returns → :py:obj:`mitsuba.core.filesystem.path`:
        *no description available*

.. py:function:: mitsuba.core.filesystem.equivalent(arg0, arg1)

    Checks whether two paths refer to the same file system object. Both
    must refer to an existing file or directory. Symlinks are followed to
    determine equivalence.

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Parameter ``arg1`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.core.filesystem.exists(arg0)

    Checks if ``p`` points to an existing filesystem object.

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.core.filesystem.file_size(arg0)

    Returns the size (in bytes) of a regular file at ``p``. Attempting to
    determine the size of a directory (as well as any other file that is
    not a regular file or a symlink) is treated as an error.

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.core.filesystem.is_directory(arg0)

    Checks if ``p`` points to a directory.

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.core.filesystem.is_regular_file(arg0)

    Checks if ``p`` points to a regular file, as opposed to a directory or
    symlink.

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:class:: mitsuba.core.filesystem.path

    Represents a path to a filesystem resource. On construction, the path
    is parsed and stored in a system-agnostic representation. The path can
    be converted back to the system-specific string using ``native()`` or
    ``string()``.


    .. py:method:: __init__(self)

        Default constructor. Constructs an empty path. An empty path is
        considered relative.

    .. py:method:: __init__(self, arg0)

        Copy constructor.

        Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Construct a path from a string with native type. On Windows, the path
        can use both '/' or '\\' as a delimiter.

        Parameter ``arg0`` (str):
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.clear(self)

        Makes the path an empty path. An empty path is considered relative.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.empty(self)

        Checks if the path is empty

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.extension(self)

        Returns the extension of the filename component of the path (the
        substring starting at the rightmost period, including the period).
        Special paths '.' and '..' have an empty extension.

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.filename(self)

        Returns the filename component of the path, including the extension.

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.is_absolute(self)

        Checks if the path is absolute.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.is_relative(self)

        Checks if the path is relative.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.native(self)

        Returns the path in the form of a native string, so that it can be
        passed directly to system APIs. The path is constructed using the
        system's preferred separator and the native string type.

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.parent_path(self)

        Returns the path to the parent directory. Returns an empty path if it
        is already empty or if it has only one element.

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.core.filesystem.path.replace_extension(self, arg0)

        Replaces the substring starting at the rightmost '.' symbol by the
        provided string.

        A '.' symbol is automatically inserted if the replacement does not
        start with a dot. Removes the extension altogether if the empty path
        is passed. If there is no extension, appends a '.' followed by the
        replacement. If the path is empty, '.' or '..', the method does
        nothing.

        Returns *this.

        Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Returns → :py:obj:`mitsuba.core.filesystem.path`:
            *no description available*

.. py:data:: mitsuba.core.filesystem.preferred_separator
    :type: str
    :value: /

.. py:function:: mitsuba.core.filesystem.remove(arg0)

    Removes a file or empty directory. Returns true if removal was
    successful, false if there was an error (e.g. the file did not exist).

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.core.filesystem.resize_file(arg0, arg1)

    Changes the size of the regular file named by ``p`` as if ``truncate``
    was called. If the file was larger than ``target_length``, the
    remainder is discarded. The file must exist.

    Parameter ``arg0`` (:py:obj:`mitsuba.core.filesystem.path`):
        *no description available*

    Parameter ``arg1`` (int):
        *no description available*

    Returns → bool:
        *no description available*

.. py:data:: mitsuba.core.float_dtype
    :type: str
    :value: f

.. py:function:: mitsuba.core.get_property(cpp_type, ptr, parent)

    Parameter ``cpp_type`` (capsule):
        *no description available*

    Parameter ``ptr`` (capsule):
        *no description available*

    Parameter ``parent`` (handle):
        *no description available*

    Returns → object:
        *no description available*

.. py:data:: mitsuba.core.is_monochromatic
    :type: bool
    :value: False

.. py:data:: mitsuba.core.is_polarized
    :type: bool
    :value: False

.. py:data:: mitsuba.core.is_rgb
    :type: bool
    :value: True

.. py:data:: mitsuba.core.is_spectral
    :type: bool
    :value: False

.. py:function:: mitsuba.core.luminance(overloaded)


    .. py:function:: luminance(value, wavelengths, active=True)

        Parameter ``value`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``wavelengths`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:function:: luminance(c)

        Parameter ``c`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

.. py:data:: mitsuba.core.math.E
    :type: float
    :value: 2.7182817459106445

.. py:data:: mitsuba.core.math.Epsilon
    :type: float
    :value: 5.960464477539063e-08

.. py:data:: mitsuba.core.math.Infinity
    :type: float
    :value: inf

.. py:data:: mitsuba.core.math.InvFourPi
    :type: float
    :value: 0.07957746833562851

.. py:data:: mitsuba.core.math.InvPi
    :type: float
    :value: 0.31830987334251404

.. py:data:: mitsuba.core.math.InvSqrtPi
    :type: float
    :value: 0.564189612865448

.. py:data:: mitsuba.core.math.InvSqrtTwo
    :type: float
    :value: 0.7071067690849304

.. py:data:: mitsuba.core.math.InvSqrtTwoPi
    :type: float
    :value: 0.3989422917366028

.. py:data:: mitsuba.core.math.InvTwoPi
    :type: float
    :value: 0.15915493667125702

.. py:data:: mitsuba.core.math.Max
    :type: float
    :value: 3.4028234663852886e+38

.. py:data:: mitsuba.core.math.Min
    :type: float
    :value: 1.1754943508222875e-38

.. py:data:: mitsuba.core.math.OneMinusEpsilon
    :type: float
    :value: 0.9999999403953552

.. py:data:: mitsuba.core.math.Pi
    :type: float
    :value: 3.1415927410125732

.. py:data:: mitsuba.core.math.RayEpsilon
    :type: float
    :value: 8.940696716308594e-05

.. py:data:: mitsuba.core.math.RecipOverflow
    :type: float
    :value: 2.938735877055719e-39

.. py:data:: mitsuba.core.math.ShadowEpsilon
    :type: float
    :value: 0.0008940696716308594

.. py:data:: mitsuba.core.math.SqrtPi
    :type: float
    :value: 1.7724539041519165

.. py:data:: mitsuba.core.math.SqrtTwo
    :type: float
    :value: 1.4142135381698608

.. py:data:: mitsuba.core.math.SqrtTwoPi
    :type: float
    :value: 2.5066282749176025

.. py:function:: mitsuba.core.math.chi2(arg0, arg1, arg2)

    Compute the Chi^2 statistic and degrees of freedom of the given arrays
    while pooling low-valued entries together

    Given a list of observations counts (``obs[i]``) and expected
    observation counts (``exp[i]``), this function accumulates the Chi^2
    statistic, that is, ``(obs-exp)^2 / exp`` for each element ``0, ...,
    n-1``.

    Minimum expected cell frequency. The Chi^2 test statistic is not
    useful when when the expected frequency in a cell is low (e.g. less
    than 5), because normality assumptions break down in this case.
    Therefore, the implementation will merge such low-frequency cells when
    they fall below the threshold specified here. Specifically, low-valued
    cells with ``exp[i] < pool_threshold`` are pooled into larger groups
    that are above the threshold before their contents are added to the
    Chi^2 statistic.

    The function returns the statistic value, degrees of freedom, below-
    treshold entries and resulting number of pooled regions.

    Parameter ``arg0`` (enoki.dynamic.Float64):
        *no description available*

    Parameter ``arg1`` (enoki.dynamic.Float64):
        *no description available*

    Parameter ``arg2`` (float):
        *no description available*

    Returns → Tuple[float, int, int, int]:
        *no description available*

.. py:function:: mitsuba.core.math.find_interval(size, pred)

    Find an interval in an ordered set

    This function performs a binary search to find an index ``i`` such
    that ``pred(i)`` is ``True`` and ``pred(i+1)`` is ``False``, where
    ``pred`` is a user-specified predicate that monotonically decreases
    over this range (i.e. max one ``True`` -> ``False`` transition).

    The predicate will be evaluated exactly <tt>floor(log2(size)) + 1<tt>
    times. Note that the template parameter ``Index`` is automatically
    inferred from the supplied predicate, which takes an index or an index
    vector of type ``Index`` as input argument and can (optionally) take a
    mask argument as well. In the vectorized case, each vector lane can
    use different predicate. When ``pred`` is ``False`` for all entries,
    the function returns ``0``, and when it is ``True`` for all cases, it
    returns <tt>size-2<tt>.

    The main use case of this function is to locate an interval (i, i+1)
    in an ordered list.

    .. code-block:: c

        float my_list[] = { 1, 1.5f, 4.f, ... };

        UInt32 index = find_interval(
            sizeof(my_list) / sizeof(float),
            [](UInt32 index, mask_t<UInt32> active) {
                return gather<Float>(my_list, index, active) < x;
            }
        );


    Parameter ``size`` (int):
        *no description available*

    Parameter ``pred`` (Callable[[int], bool]):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.core.math.is_power_of_two(arg0)

    Check whether the provided integer is a power of two

    Parameter ``arg0`` (int):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.core.math.legendre_p(overloaded)


    .. py:function:: legendre_p(l, x)

        Evaluate the l-th Legendre polynomial using recurrence

        Parameter ``l`` (int):
            *no description available*

        Parameter ``x`` (float):
            *no description available*

        Returns → float:
            *no description available*

    .. py:function:: legendre_p(l, m, x)

        Evaluate the l-th Legendre polynomial using recurrence

        Parameter ``l`` (int):
            *no description available*

        Parameter ``m`` (int):
            *no description available*

        Parameter ``x`` (float):
            *no description available*

        Returns → float:
            *no description available*

.. py:function:: mitsuba.core.math.legendre_pd(l, x)

    Evaluate the l-th Legendre polynomial and its derivative using
    recurrence

    Parameter ``l`` (int):
        *no description available*

    Parameter ``x`` (float):
        *no description available*

    Returns → Tuple[float, float]:
        *no description available*

.. py:function:: mitsuba.core.math.legendre_pd_diff(l, x)

    Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x)

    Parameter ``l`` (int):
        *no description available*

    Parameter ``x`` (float):
        *no description available*

    Returns → Tuple[float, float]:
        *no description available*

.. py:function:: mitsuba.core.math.linear_to_srgb(arg0)

    Applies the sRGB gamma curve to the given argument.

    Parameter ``arg0`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.math.morton_decode2(m)

    Parameter ``m`` (int):
        *no description available*

    Returns → enoki.scalar.Vector2u:
        *no description available*

.. py:function:: mitsuba.core.math.morton_decode3(m)

    Parameter ``m`` (int):
        *no description available*

    Returns → enoki.scalar.Vector3u:
        *no description available*

.. py:function:: mitsuba.core.math.morton_encode2(v)

    Parameter ``v`` (enoki.scalar.Vector2u):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.core.math.morton_encode3(v)

    Parameter ``v`` (enoki.scalar.Vector3u):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.core.math.round_to_power_of_two(arg0)

    Round an unsigned integer to the next integer power of two

    Parameter ``arg0`` (int):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.core.math.solve_quadratic(a, b, c)

    Solve a quadratic equation of the form a*x^2 + b*x + c = 0.

    Parameter ``a`` (float):
        *no description available*

    Parameter ``b`` (float):
        *no description available*

    Parameter ``c`` (float):
        *no description available*

    Returns → Tuple[bool, float, float]:
        ``True`` if a solution could be found

.. py:function:: mitsuba.core.math.srgb_to_linear(arg0)

    Applies the inverse sRGB gamma curve to the given argument.

    Parameter ``arg0`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.math.ulpdiff(arg0, arg1)

    Compare the difference in ULPs between a reference value and another
    given floating point number

    Parameter ``arg0`` (float):
        *no description available*

    Parameter ``arg1`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.pdf_rgb_spectrum(overloaded)


    .. py:function:: pdf_rgb_spectrum(wavelengths)

        PDF for the sample_rgb_spectrum strategy. It is valid to call this
        function for a single wavelength (Float), a set of wavelengths
        (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all cases,
        the PDF is returned per wavelength.

        Parameter ``wavelengths`` (float):
            *no description available*

        Returns → float:
            *no description available*

    .. py:function:: pdf_rgb_spectrum(wavelengths)

        PDF for the sample_rgb_spectrum strategy. It is valid to call this
        function for a single wavelength (Float), a set of wavelengths
        (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all cases,
        the PDF is returned per wavelength.

        Parameter ``wavelengths`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

.. py:function:: mitsuba.core.pdf_uniform_spectrum(overloaded)


    .. py:function:: pdf_uniform_spectrum(wavelengths)

        Parameter ``wavelengths`` (float):
            *no description available*

        Returns → float:
            *no description available*

    .. py:function:: pdf_uniform_spectrum(wavelengths)

        Parameter ``wavelengths`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

.. py:function:: mitsuba.core.permute(value, sample_count, seed, rounds=4)

    Generate pseudorandom permutation vector using a shuffling network and
    the sample_tea function. This algorithm has a O(log2(sample_count))
    complexity but only supports permutation vectors whose lengths are a
    power of 2.

    Parameter ``index``:
        Input index to be mapped

    Parameter ``sample_count`` (int):
        Length of the permutation vector

    Parameter ``seed`` (int):
        Seed value used as second input to the Tiny Encryption Algorithm.
        Can be used to generate different permutation vectors.

    Parameter ``rounds`` (int):
        How many rounds should be executed by the Tiny Encryption
        Algorithm? The default is 2.

    Parameter ``value`` (int):
        *no description available*

    Returns → int:
        The index corresponding to the input index in the pseudorandom
        permutation vector.

.. py:function:: mitsuba.core.permute_kensler(i, l, p, active=True)

    Generate pseudorandom permutation vector using the algorithm described
    in Pixar's technical memo "Correlated Multi-Jittered Sampling":

    https://graphics.pixar.com/library/MultiJitteredSampling/

    Unlike permute, this function supports permutation vectors of any
    length.

    Parameter ``index``:
        Input index to be mapped

    Parameter ``sample_count``:
        Length of the permutation vector

    Parameter ``seed``:
        Seed value used as second input to the Tiny Encryption Algorithm.
        Can be used to generate different permutation vectors.

    Parameter ``i`` (int):
        *no description available*

    Parameter ``l`` (int):
        *no description available*

    Parameter ``p`` (int):
        *no description available*

    Parameter ``active`` (bool):
        Mask to specify active lanes.

    Returns → int:
        The index corresponding to the input index in the pseudorandom
        permutation vector.

.. py:function:: mitsuba.core.quad.composite_simpson(n)

    Computes the nodes and weights of a composite Simpson quadrature rule
    with the given number of evaluations.

    Integration is over the interval $[-1, 1]$, which will be split into
    $(n-1) / 2$ sub-intervals with overlapping endpoints. A 3-point
    Simpson rule is applied per interval, which is exact for polynomials
    of degree three or less.

    Parameter ``n`` (int):
        Desired number of evalution points. Must be an odd number bigger
        than 3.

    Returns → Tuple[enoki::DynamicArray<enoki::Packet<float, 16ul> >, enoki::DynamicArray<enoki::Packet<float, 16ul> >]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.core.quad.composite_simpson_38(n)

    Computes the nodes and weights of a composite Simpson 3/8 quadrature
    rule with the given number of evaluations.

    Integration is over the interval $[-1, 1]$, which will be split into
    $(n-1) / 3$ sub-intervals with overlapping endpoints. A 4-point
    Simpson rule is applied per interval, which is exact for polynomials
    of degree four or less.

    Parameter ``n`` (int):
        Desired number of evalution points. Must be an odd number bigger
        than 3.

    Returns → Tuple[enoki::DynamicArray<enoki::Packet<float, 16ul> >, enoki::DynamicArray<enoki::Packet<float, 16ul> >]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.core.quad.gauss_legendre(n)

    Computes the nodes and weights of a Gauss-Legendre quadrature (aka
    "Gaussian quadrature") rule with the given number of evaluations.

    Integration is over the interval $[-1, 1]$. Gauss-Legendre quadrature
    maximizes the order of exactly integrable polynomials achieves this up
    to degree $2n-1$ (where $n$ is the number of function evaluations).

    This method is numerically well-behaved until about $n=200$ and then
    becomes progressively less accurate. It is generally not a good idea
    to go much higher---in any case, a composite or adaptive integration
    scheme will be superior for large $n$.

    Parameter ``n`` (int):
        Desired number of evalution points

    Returns → Tuple[enoki::DynamicArray<enoki::Packet<float, 16ul> >, enoki::DynamicArray<enoki::Packet<float, 16ul> >]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.core.quad.gauss_lobatto(n)

    Computes the nodes and weights of a Gauss-Lobatto quadrature rule with
    the given number of evaluations.

    Integration is over the interval $[-1, 1]$. Gauss-Lobatto quadrature
    is preferable to Gauss-Legendre quadrature whenever the endpoints of
    the integration domain should explicitly be included. It maximizes the
    order of exactly integrable polynomials subject to this constraint and
    achieves this up to degree $2n-3$ (where $n$ is the number of function
    evaluations).

    This method is numerically well-behaved until about $n=200$ and then
    becomes progressively less accurate. It is generally not a good idea
    to go much higher---in any case, a composite or adaptive integration
    scheme will be superior for large $n$.

    Parameter ``n`` (int):
        Desired number of evalution points

    Returns → Tuple[enoki::DynamicArray<enoki::Packet<float, 16ul> >, enoki::DynamicArray<enoki::Packet<float, 16ul> >]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.core.radical_inverse_2(index, scramble)

    Van der Corput radical inverse in base 2

    Parameter ``index`` (int):
        *no description available*

    Parameter ``scramble`` (int):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.sample_rgb_spectrum(overloaded)


    .. py:function:: sample_rgb_spectrum(sample)

        Importance sample a "importance spectrum" that concentrates the
        computation on wavelengths that are relevant for rendering of RGB data

        Based on "An Improved Technique for Full Spectral Rendering" by
        Radziszewski, Boryczko, and Alda

        Returns a tuple with the sampled wavelength and inverse PDF

        Parameter ``sample`` (float):
            *no description available*

        Returns → Tuple[float, float]:
            *no description available*

    .. py:function:: sample_rgb_spectrum(sample)

        Importance sample a "importance spectrum" that concentrates the
        computation on wavelengths that are relevant for rendering of RGB data

        Based on "An Improved Technique for Full Spectral Rendering" by
        Radziszewski, Boryczko, and Alda

        Returns a tuple with the sampled wavelength and inverse PDF

        Parameter ``sample`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → Tuple[enoki.scalar.Vector3f, enoki.scalar.Vector3f]:
            *no description available*

.. py:function:: mitsuba.core.sample_tea_32(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    For details, refer to "GPU Random Numbers via the Tiny Encryption
    Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → int:
        A uniformly distributed 32-bit integer

.. py:function:: mitsuba.core.sample_tea_float

    sample_tea_float32(v0: int, v1: int, rounds: int = 4) -> float

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    This function uses sample_tea to return single precision floating
    point numbers on the interval ``[0, 1)``

    Parameter ``v0``:
        First input value to be encrypted (could be the sample index)

    Parameter ``v1``:
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds``:
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns:
        A uniformly distributed floating point number on the interval
        ``[0, 1)``

.. py:function:: mitsuba.core.sample_tea_float32(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    This function uses sample_tea to return single precision floating
    point numbers on the interval ``[0, 1)``

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → float:
        A uniformly distributed floating point number on the interval
        ``[0, 1)``

.. py:function:: mitsuba.core.sample_tea_float64(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    This function uses sample_tea to return double precision floating
    point numbers on the interval ``[0, 1)``

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → float:
        A uniformly distributed floating point number on the interval
        ``[0, 1)``

.. py:function:: mitsuba.core.sample_uniform_spectrum(overloaded)


    .. py:function:: sample_uniform_spectrum(sample)

        Parameter ``sample`` (float):
            *no description available*

        Returns → Tuple[float, float]:
            *no description available*

    .. py:function:: sample_uniform_spectrum(sample)

        Parameter ``sample`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → Tuple[enoki.scalar.Vector3f, enoki.scalar.Vector3f]:
            *no description available*

.. py:function:: mitsuba.core.set_property(arg0, arg1, arg2)

    Parameter ``arg0`` (capsule):
        *no description available*

    Parameter ``arg1`` (capsule):
        *no description available*

    Parameter ``arg2`` (handle):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.core.set_thread_count(count=-1)

    Sets the maximum number of threads to be used for parallelized execution of Mitsuba code. Defaults to -1 (automatic).

    Parameter ``count`` (int):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.core.sobol_2(index, scramble)

    Sobol' radical inverse in base 2

    Parameter ``index`` (int):
        *no description available*

    Parameter ``scramble`` (int):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.spline.eval_1d(overloaded)


    .. py:function:: eval_1d(min, max, values, x)

        Evaluate a cubic spline interpolant of a *uniformly* sampled 1D
        function

        The implementation relies on Catmull-Rom splines, i.e. it uses finite
        differences to approximate the derivatives at the endpoints of each
        spline segment.

        Template parameter ``Extrapolate``:
            Extrapolate values when ``x`` is out of range? (default:
            ``False``)

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max`` (float):
            Position of the last node

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing ``size`` regularly spaced evaluations in the
            range [``min``, ``max``] of the approximated function.

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``x`` (float):
            Evaluation point

        Remark:
            The Python API lacks the ``size`` parameter, which is inferred
            automatically from the size of the input array.

        Remark:
            The Python API provides a vectorized version which evaluates the
            function for many arguments ``x``.

        Returns → float:
            The interpolated value or zero when ``Extrapolate=false`` and
            ``x`` lies outside of [``min``, ``max``]

    .. py:function:: eval_1d(nodes, values, x)

        Evaluate a cubic spline interpolant of a *non*-uniformly sampled 1D
        function

        The implementation relies on Catmull-Rom splines, i.e. it uses finite
        differences to approximate the derivatives at the endpoints of each
        spline segment.

        Template parameter ``Extrapolate``:
            Extrapolate values when ``x`` is out of range? (default:
            ``False``)

        Parameter ``nodes`` (numpy.ndarray[float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing function evaluations matched to the entries of
            ``nodes``.

        Parameter ``size``:
            Denotes the size of the ``nodes`` and ``values`` array

        Parameter ``x`` (float):
            Evaluation point

        Remark:
            The Python API lacks the ``size`` parameter, which is inferred
            automatically from the size of the input array

        Remark:
            The Python API provides a vectorized version which evaluates the
            function for many arguments ``x``.

        Returns → float:
            The interpolated value or zero when ``Extrapolate=false`` and
            ``x`` lies outside of \a [``min``, ``max``]

.. py:function:: mitsuba.core.spline.eval_2d(nodes1, nodes2, values, x, y)

    Evaluate a cubic spline interpolant of a uniformly sampled 2D function

    This implementation relies on a tensor product of Catmull-Rom splines,
    i.e. it uses finite differences to approximate the derivatives for
    each dimension at the endpoints of spline patches.

    Template parameter ``Extrapolate``:
        Extrapolate values when ``p`` is out of range? (default:
        ``False``)

    Parameter ``nodes1`` (numpy.ndarray[float32]):
        Arrays containing ``size1`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated
        on the ``X`` axis (in increasing order)

    Parameter ``size1``:
        Denotes the size of the ``nodes1`` array

    Parameter ``nodes``:
        Arrays containing ``size2`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated
        on the ``Y`` axis (in increasing order)

    Parameter ``size2``:
        Denotes the size of the ``nodes2`` array

    Parameter ``values`` (numpy.ndarray[float32]):
        A 2D floating point array of ``size1*size2`` cells containing
        irregularly spaced evaluations of the function to be interpolated.
        Consecutive entries of this array correspond to increments in the
        ``X`` coordinate.

    Parameter ``x`` (float):
        ``X`` coordinate of the evaluation point

    Parameter ``y`` (float):
        ``Y`` coordinate of the evaluation point

    Remark:
        The Python API lacks the ``size1`` and ``size2`` parameters, which
        are inferred automatically from the size of the input arrays.

    Parameter ``nodes2`` (numpy.ndarray[float32]):
        *no description available*

    Returns → float:
        The interpolated value or zero when ``Extrapolate=false``tt> and
        ``(x,y)`` lies outside of the node range

.. py:function:: mitsuba.core.spline.eval_spline(f0, f1, d0, d1, t)

    Compute the definite integral and derivative of a cubic spline that is
    parameterized by the function values and derivatives at the endpoints
    of the interval ``[0, 1]``.

    Parameter ``f0`` (float):
        The function value at the left position

    Parameter ``f1`` (float):
        The function value at the right position

    Parameter ``d0`` (float):
        The function derivative at the left position

    Parameter ``d1`` (float):
        The function derivative at the right position

    Parameter ``t`` (float):
        The parameter variable

    Returns → float:
        The interpolated function value at ``t``

.. py:function:: mitsuba.core.spline.eval_spline_d(f0, f1, d0, d1, t)

    Compute the value and derivative of a cubic spline that is
    parameterized by the function values and derivatives of the interval
    ``[0, 1]``.

    Parameter ``f0`` (float):
        The function value at the left position

    Parameter ``f1`` (float):
        The function value at the right position

    Parameter ``d0`` (float):
        The function derivative at the left position

    Parameter ``d1`` (float):
        The function derivative at the right position

    Parameter ``t`` (float):
        The parameter variable

    Returns → Tuple[float, float]:
        The interpolated function value and its derivative at ``t``

.. py:function:: mitsuba.core.spline.eval_spline_i(f0, f1, d0, d1, t)

    Compute the definite integral and value of a cubic spline that is
    parameterized by the function values and derivatives of the interval
    ``[0, 1]``.

    Parameter ``f0`` (float):
        The function value at the left position

    Parameter ``f1`` (float):
        The function value at the right position

    Parameter ``d0`` (float):
        The function derivative at the left position

    Parameter ``d1`` (float):
        The function derivative at the right position

    Parameter ``t`` (float):
        *no description available*

    Returns → Tuple[float, float]:
        The definite integral and the interpolated function value at ``t``

.. py:function:: mitsuba.core.spline.eval_spline_weights(overloaded)


    .. py:function:: eval_spline_weights(min, max, size, x)

        Compute weights to perform a spline-interpolated lookup on a
        *uniformly* sampled 1D function.

        The implementation relies on Catmull-Rom splines, i.e. it uses finite
        differences to approximate the derivatives at the endpoints of each
        spline segment. The resulting weights are identical those internally
        used by sample_1d().

        Template parameter ``Extrapolate``:
            Extrapolate values when ``x`` is out of range? (default:
            ``False``)

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max`` (float):
            Position of the last node

        Parameter ``size`` (int):
            Denotes the number of function samples

        Parameter ``x`` (float):
            Evaluation point

        Parameter ``weights``:
            Pointer to a weight array of size 4 that will be populated

        Remark:
            In the Python API, the ``offset`` and ``weights`` parameters are
            returned as the second and third elements of a triple.

        Returns → Tuple[bool, int, numpy.ndarray[float32]]:
            A boolean set to ``True`` on success and ``False`` when
            ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
            and an offset into the function samples associated with weights[0]

    .. py:function:: eval_spline_weights(nodes, x)

        Compute weights to perform a spline-interpolated lookup on a
        *non*-uniformly sampled 1D function.

        The implementation relies on Catmull-Rom splines, i.e. it uses finite
        differences to approximate the derivatives at the endpoints of each
        spline segment. The resulting weights are identical those internally
        used by sample_1d().

        Template parameter ``Extrapolate``:
            Extrapolate values when ``x`` is out of range? (default:
            ``False``)

        Parameter ``nodes`` (numpy.ndarray[float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``size``:
            Denotes the size of the ``nodes`` array

        Parameter ``x`` (float):
            Evaluation point

        Parameter ``weights``:
            Pointer to a weight array of size 4 that will be populated

        Remark:
            The Python API lacks the ``size`` parameter, which is inferred
            automatically from the size of the input array. The ``offset`` and
            ``weights`` parameters are returned as the second and third
            elements of a triple.

        Returns → Tuple[bool, int, numpy.ndarray[float32]]:
            A boolean set to ``True`` on success and ``False`` when
            ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
            and an offset into the function samples associated with weights[0]

.. py:function:: mitsuba.core.spline.integrate_1d(overloaded)


    .. py:function:: integrate_1d(min, max, values)

        Computes a prefix sum of integrals over segments of a *uniformly*
        sampled 1D Catmull-Rom spline interpolant

        This is useful for sampling spline segments as part of an importance
        sampling scheme (in conjunction with sample_1d)

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max`` (float):
            Position of the last node

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing ``size`` regularly spaced evaluations in the
            range [``min``, ``max``] of the approximated function.

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``out``:
            An array with ``size`` entries, which will be used to store the
            prefix sum

        Remark:
            The Python API lacks the ``size`` and ``out`` parameters. The
            former is inferred automatically from the size of the input array,
            and ``out`` is returned as a list.

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:function:: integrate_1d(nodes, values)

        Computes a prefix sum of integrals over segments of a *non*-uniformly
        sampled 1D Catmull-Rom spline interpolant

        This is useful for sampling spline segments as part of an importance
        sampling scheme (in conjunction with sample_1d)

        Parameter ``nodes`` (numpy.ndarray[float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing function evaluations matched to the entries of
            ``nodes``.

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``out``:
            An array with ``size`` entries, which will be used to store the
            prefix sum

        Remark:
            The Python API lacks the ``size`` and ``out`` parameters. The
            former is inferred automatically from the size of the input array,
            and ``out`` is returned as a list.

        Returns → enoki.dynamic.Float32:
            *no description available*

.. py:function:: mitsuba.core.spline.invert_1d(overloaded)


    .. py:function:: invert_1d(min, max_, values, y, eps=9.999999974752427e-07)

        Invert a cubic spline interpolant of a *uniformly* sampled 1D
        function. The spline interpolant must be *monotonically increasing*.

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max``:
            Position of the last node

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing ``size`` regularly spaced evaluations in the
            range [``min``, ``max``] of the approximated function.

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``y`` (float):
            Input parameter for the inversion

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → float:
            The spline parameter ``t`` such that ``eval_1d(..., t)=y``

        Parameter ``max_`` (float):
            *no description available*

    .. py:function:: invert_1d(nodes, values, y, eps=9.999999974752427e-07)

        Invert a cubic spline interpolant of a *non*-uniformly sampled 1D
        function. The spline interpolant must be *monotonically increasing*.

        Parameter ``nodes`` (numpy.ndarray[float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing function evaluations matched to the entries of
            ``nodes``.

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``y`` (float):
            Input parameter for the inversion

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → float:
            The spline parameter ``t`` such that ``eval_1d(..., t)=y``

.. py:function:: mitsuba.core.spline.sample_1d(overloaded)


    .. py:function:: sample_1d(min, max, values, cdf, sample, eps=9.999999974752427e-07)

        Importance sample a segment of a *uniformly* sampled 1D Catmull-Rom
        spline interpolant

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max`` (float):
            Position of the last node

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing ``size`` regularly spaced evaluations in the
            range [``min``, ``max``] of the approximated function.

        Parameter ``cdf`` (numpy.ndarray[float32]):
            Array containing a cumulative distribution function computed by
            integrate_1d().

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``sample`` (float):
            A uniformly distributed random sample in the interval ``[0,1]``

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → Tuple[float, float, float]:
            1. The sampled position 2. The value of the spline evaluated at
            the sampled position 3. The probability density at the sampled
            position (which only differs from item 2. when the function does
            not integrate to one)

    .. py:function:: sample_1d(nodes, values, cdf, sample, eps=9.999999974752427e-07)

        Importance sample a segment of a *non*-uniformly sampled 1D Catmull-
        Rom spline interpolant

        Parameter ``nodes`` (numpy.ndarray[float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[float32]):
            Array containing function evaluations matched to the entries of
            ``nodes``.

        Parameter ``cdf`` (numpy.ndarray[float32]):
            Array containing a cumulative distribution function computed by
            integrate_1d().

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``sample`` (float):
            A uniformly distributed random sample in the interval ``[0,1]``

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → Tuple[float, float, float]:
            1. The sampled position 2. The value of the spline evaluated at
            the sampled position 3. The probability density at the sampled
            position (which only differs from item 2. when the function does
            not integrate to one)

.. py:function:: mitsuba.core.srgb_to_xyz(rgb, active=True)

    Convert ITU-R Rec. BT.709 linear RGB to XYZ tristimulus values

    Parameter ``rgb`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``active`` (bool):
        Mask to specify active lanes.

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.unpolarized(arg0)

    Parameter ``arg0`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.util.core_count()

    Determine the number of available CPU cores (including virtual cores)

    Returns → int:
        *no description available*

.. py:function:: mitsuba.core.util.mem_string(size, precise=False)

    Turn a memory size into a human-readable string

    Parameter ``size`` (int):
        *no description available*

    Parameter ``precise`` (bool):
        *no description available*

    Returns → str:
        *no description available*

.. py:function:: mitsuba.core.util.time_string(time, precise=False)

    Convert a time difference (in seconds) to a string representation

    Parameter ``time`` (float):
        Time difference in (fractional) sections

    Parameter ``precise`` (bool):
        When set to true, a higher-precision string representation is
        generated.

    Returns → str:
        *no description available*

.. py:function:: mitsuba.core.util.trap_debugger()

    Generate a trap instruction if running in a debugger; otherwise,
    return.

    Returns → None:
        *no description available*

.. py:function:: mitsuba.core.warp.beckmann_to_square(v, alpha)

    Inverse of the mapping square_to_uniform_cone

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``alpha`` (float):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.bilinear_to_square(v00, v10, v01, v11, sample)

    Inverse of square_to_bilinear

    Parameter ``v00`` (float):
        *no description available*

    Parameter ``v10`` (float):
        *no description available*

    Parameter ``v01`` (float):
        *no description available*

    Parameter ``v11`` (float):
        *no description available*

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → Tuple[enoki.scalar.Vector2f, float]:
        *no description available*

.. py:function:: mitsuba.core.warp.cosine_hemisphere_to_square(v)

    Inverse of the mapping square_to_cosine_hemisphere

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.interval_to_linear(v0, v1, sample)

    Importance sample a linear interpolant

    Given a linear interpolant on the unit interval with boundary values
    ``v0``, ``v1`` (where ``v1`` is the value at ``x=1``), warp a
    uniformly distributed input sample ``sample`` so that the resulting
    probability distribution matches the linear interpolant.

    Parameter ``v0`` (float):
        *no description available*

    Parameter ``v1`` (float):
        *no description available*

    Parameter ``sample`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.interval_to_nonuniform_tent(a, b, c, d)

    Warp a uniformly distributed sample on [0, 1] to a nonuniform tent
    distribution with nodes ``{a, b, c}``

    Parameter ``a`` (float):
        *no description available*

    Parameter ``b`` (float):
        *no description available*

    Parameter ``c`` (float):
        *no description available*

    Parameter ``d`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.interval_to_tent(sample)

    Warp a uniformly distributed sample on [0, 1] to a tent distribution

    Parameter ``sample`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.linear_to_interval(v0, v1, sample)

    Inverse of interval_to_linear

    Parameter ``v0`` (float):
        *no description available*

    Parameter ``v1`` (float):
        *no description available*

    Parameter ``sample`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_beckmann(sample, alpha)

    Warp a uniformly distributed square sample to a Beckmann distribution

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Parameter ``alpha`` (float):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_beckmann_pdf(v, alpha)

    Probability density of square_to_beckmann()

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``alpha`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_bilinear(v00, v10, v01, v11, sample)

    Importance sample a bilinear interpolant

    Given a bilinear interpolant on the unit square with corner values
    ``v00``, ``v10``, ``v01``, ``v11`` (where ``v10`` is the value at
    (x,y) == (0, 0)), warp a uniformly distributed input sample ``sample``
    so that the resulting probability distribution matches the linear
    interpolant.

    The implementation first samples the marginal distribution to obtain
    ``y``, followed by sampling the conditional distribution to obtain
    ``x``.

    Returns the sampled point and PDF for convenience.

    Parameter ``v00`` (float):
        *no description available*

    Parameter ``v10`` (float):
        *no description available*

    Parameter ``v01`` (float):
        *no description available*

    Parameter ``v11`` (float):
        *no description available*

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → Tuple[enoki.scalar.Vector2f, float]:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_bilinear_pdf(v00, v10, v01, v11, sample)

    Parameter ``v00`` (float):
        *no description available*

    Parameter ``v10`` (float):
        *no description available*

    Parameter ``v01`` (float):
        *no description available*

    Parameter ``v11`` (float):
        *no description available*

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_cosine_hemisphere(sample)

    Sample a cosine-weighted vector on the unit hemisphere with respect to
    solid angles

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_cosine_hemisphere_pdf(v)

    Density of square_to_cosine_hemisphere() with respect to solid angles

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_rough_fiber(sample, wi, tangent, kappa)

    Warp a uniformly distributed square sample to a rough fiber
    distribution

    Parameter ``sample`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``wi`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``tangent`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``kappa`` (float):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_rough_fiber_pdf(v, wi, tangent, kappa)

    Probability density of square_to_rough_fiber()

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``wi`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``tangent`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``kappa`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_std_normal(v)

    Sample a point on a 2D standard normal distribution. Internally uses
    the Box-Muller transformation

    Parameter ``v`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_std_normal_pdf(v)

    Parameter ``v`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_tent(sample)

    Warp a uniformly distributed square sample to a 2D tent distribution

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_tent_pdf(v)

    Density of square_to_tent per unit area.

    Parameter ``v`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_cone(v, cos_cutoff)

    Uniformly sample a vector that lies within a given cone of angles
    around the Z axis

    Parameter ``cos_cutoff`` (float):
        Cosine of the cutoff angle

    Parameter ``sample``:
        A uniformly distributed sample on $[0,1]^2$

    Parameter ``v`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_cone_pdf(v, cos_cutoff)

    Density of square_to_uniform_cone per unit area.

    Parameter ``cos_cutoff`` (float):
        Cosine of the cutoff angle

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_disk(sample)

    Uniformly sample a vector on a 2D disk

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_disk_concentric(sample)

    Low-distortion concentric square to disk mapping by Peter Shirley

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_disk_concentric_pdf(p)

    Density of square_to_uniform_disk per unit area

    Parameter ``p`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_disk_pdf(p)

    Density of square_to_uniform_disk per unit area

    Parameter ``p`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_hemisphere(sample)

    Uniformly sample a vector on the unit hemisphere with respect to solid
    angles

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_hemisphere_pdf(v)

    Density of square_to_uniform_hemisphere() with respect to solid angles

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_sphere(sample)

    Uniformly sample a vector on the unit sphere with respect to solid
    angles

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_sphere_pdf(v)

    Density of square_to_uniform_sphere() with respect to solid angles

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_square_concentric(sample)

    Low-distortion concentric square to square mapping (meant to be used
    in conjunction with another warping method that maps to the sphere)

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_triangle(sample)

    Convert an uniformly distributed square sample into barycentric
    coordinates

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_uniform_triangle_pdf(p)

    Density of square_to_uniform_triangle per unit area.

    Parameter ``p`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_von_mises_fisher(sample, kappa)

    Warp a uniformly distributed square sample to a von Mises Fisher
    distribution

    Parameter ``sample`` (enoki.scalar.Vector2f):
        *no description available*

    Parameter ``kappa`` (float):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.core.warp.square_to_von_mises_fisher_pdf(v, kappa)

    Probability density of square_to_von_mises_fisher()

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``kappa`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.tent_to_interval(value)

    Warp a uniformly distributed sample on [0, 1] to a tent distribution

    Parameter ``value`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.core.warp.tent_to_square(value)

    Warp a uniformly distributed square sample to a 2D tent distribution

    Parameter ``value`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.uniform_cone_to_square(v, cos_cutoff)

    Inverse of the mapping square_to_uniform_cone

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``cos_cutoff`` (float):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.uniform_disk_to_square(p)

    Inverse of the mapping square_to_uniform_disk

    Parameter ``p`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.uniform_disk_to_square_concentric(p)

    Inverse of the mapping square_to_uniform_disk_concentric

    Parameter ``p`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.uniform_hemisphere_to_square(v)

    Inverse of the mapping square_to_uniform_hemisphere

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.uniform_sphere_to_square(sample)

    Inverse of the mapping square_to_uniform_sphere

    Parameter ``sample`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.uniform_triangle_to_square(p)

    Inverse of the mapping square_to_uniform_triangle

    Parameter ``p`` (enoki.scalar.Vector2f):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.warp.von_mises_fisher_to_square(v, kappa)

    Inverse of the mapping von_mises_fisher_to_square

    Parameter ``v`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``kappa`` (float):
        *no description available*

    Returns → enoki.scalar.Vector2f:
        *no description available*

.. py:function:: mitsuba.core.xml.load_dict(dict)

    Load a Mitsuba scene or object from an Python dictionary

    Parameter ``dict`` (dict):
        Python dictionary containing the object description

    Returns → object:
        *no description available*

.. py:function:: mitsuba.core.xml.load_file(path, update_scene=False, **kwargs)

    Load a Mitsuba scene from an XML file

    Parameter ``path`` (str):
        Filename of the scene XML file

    Parameter ``parameters``:
        Optional list of parameters that can be referenced as ``$varname``
        in the scene.

    Parameter ``variant``:
        Specifies the variant of plugins to instantiate (e.g.
        "scalar_rgb")

    Parameter ``update_scene`` (bool):
        When Mitsuba updates scene to a newer version, should the updated
        XML file be written back to disk?

    Returns → object:
        *no description available*

.. py:function:: mitsuba.core.xml.load_string(string)

    Load a Mitsuba scene from an XML string

    Parameter ``string`` (str, **kwargs):
        *no description available*

    Returns → object:
        *no description available*

.. py:function:: mitsuba.core.xyz_to_srgb(rgb, active=True)

    Convert XYZ tristimulus values to ITU-R Rec. BT.709 linear RGB

    Parameter ``rgb`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``active`` (bool):
        Mask to specify active lanes.

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:class:: mitsuba.render.BSDF

    Base class: :py:obj:`mitsuba.core.Object`

    Bidirectional Scattering Distribution Function (BSDF) interface

    This class provides an abstract interface to all %BSDF plugins in
    Mitsuba. It exposes functions for evaluating and sampling the model,
    and for querying associated probability densities.

    By default, functions in class sample and evaluate the complete BSDF,
    but it also allows to pick and choose individual components of multi-
    lobed BSDFs based on their properties and component indices. This
    selection is specified using a context data structure that is provided
    along with every operation.

    When polarization is enabled, BSDF sampling and evaluation returns 4x4
    Mueller matrices that describe how scattering changes the polarization
    state of incident light. Mueller matrices (e.g. for mirrors) are
    expressed with respect to a reference coordinate system for the
    incident and outgoing direction. The convention used here is that
    these coordinate systems are given by ``coordinate_system(wi)`` and
    ``coordinate_system(wo)``, where 'wi' and 'wo' are the incident and
    outgoing direction in local coordinates.

    See also:
        :py:obj:`mitsuba.render.BSDFContext`

    See also:
        :py:obj:`mitsuba.render.BSDFSample3f`

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.core.Properties`):
            *no description available*


    .. py:method:: mitsuba.render.BSDF.component_count(self, active=True)

        Number of components this BSDF is comprised of.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.BSDF.eval(self, ctx, si, wo, active=True)

        Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo) and
        multiply by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.render.BSDFContext`):
            A context data structure describing which lobes to evalute, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (enoki.scalar.Vector3f):
            The outgoing direction

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.BSDF.eval_null_transmission(self, si, active=True)

        Evaluate un-scattered transmission component of the BSDF

        This method will evaluate the un-scattered transmission
        (BSDFFlags::Null) of the BSDF for light arriving from direction ``w``.
        The default implementation returns zero.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.BSDF.flags(overloaded)


        .. py:method:: flags(self, active=True)

            Flags for all components combined.

            Parameter ``active`` (bool):
                Mask to specify active lanes.

            Returns → int:
                *no description available*

        .. py:method:: flags(self, index, active=True)

            Flags for a specific component of this BSDF.

            Parameter ``index`` (int):
                *no description available*

            Parameter ``active`` (bool):
                Mask to specify active lanes.

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.render.BSDF.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.render.BSDF.needs_differentials(self, active=True)

        Does the implementation require access to texture-space differentials?

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.BSDF.pdf(self, ctx, si, wo, active=True)

        Compute the probability per unit solid angle of sampling a given
        direction

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.render.BSDFContext`):
            A context data structure describing which lobes to evalute, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (enoki.scalar.Vector3f):
            The outgoing direction

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.BSDF.sample(self, ctx, si, sample1, sample2, active=True)

        Importance sample the BSDF model

        The function returns a sample data structure along with the importance
        weight, which is the value of the BSDF divided by the probability
        density, and multiplied by the cosine foreshortening factor (if needed
        --- it is omitted for degenerate BSDFs like smooth
        mirrors/dielectrics).

        If the supplied context data strutcures selects subset of components
        in a multi-lobe BRDF model, the sampling is restricted to this subset.
        Depending on the provided transport type, either the BSDF or its
        adjoint version is sampled.

        When sampling a continuous/non-delta component, this method also
        multiplies by the cosine foreshorening factor with respect to the
        sampled direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.render.BSDFContext`):
            A context data structure describing which lobes to sample, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``sample1`` (float):
            A uniformly distributed sample on $[0,1]$. It is used to select
            the BSDF lobe in multi-lobe models.

        Parameter ``sample2`` (enoki.scalar.Vector2f):
            A uniformly distributed sample on $[0,1]^2$. It is used to
            generate the sampled direction.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.render.BSDFSample3f`, enoki.scalar.Vector3f]:
            A pair (bs, value) consisting of

        bs: Sampling record, indicating the sampled direction, PDF values and
        other information. The contents are undefined if sampling failed.

        value: The BSDF value (multiplied by the cosine foreshortening factor
        when a non-delta component is sampled). A zero spectrum indicates that
        sampling failed.

.. py:class:: mitsuba.render.BSDFContext

    Context data structure for BSDF evaluation and sampling

    BSDF models in Mitsuba can be queried and sampled using a variety of
    different modes -- for instance, a rendering algorithm can indicate
    whether radiance or importance is being transported, and it can also
    restrict evaluation and sampling to a subset of lobes in a a multi-
    lobe BSDF model.

    The BSDFContext data structure encodes these preferences and is
    supplied to most BSDF methods.


    .. py:method:: __init__(self, mode=TransportMode.Radiance)

        //! @}

        Parameter ``mode`` (:py:obj:`mitsuba.render.TransportMode`):
            *no description available*

    .. py:method:: __init__(self, mode, type_mak, component)

        Parameter ``mode`` (:py:obj:`mitsuba.render.TransportMode`):
            *no description available*

        Parameter ``type_mak`` (int):
            *no description available*

        Parameter ``component`` (int):
            *no description available*

    .. py:method:: mitsuba.render.BSDFContext.component
        :property:

        Integer value of requested BSDF component index to be
        sampled/evaluated.

    .. py:method:: mitsuba.render.BSDFContext.is_enabled(self, type, component=0)

        Checks whether a given BSDF component type and BSDF component index
        are enabled in this context.

        Parameter ``type`` (:py:obj:`mitsuba.render.BSDFFlags`):
            *no description available*

        Parameter ``component`` (int):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.BSDFContext.mode
        :property:

        Transported mode (radiance or importance)

    .. py:method:: mitsuba.render.BSDFContext.reverse(self)

        Reverse the direction of light transport in the record

        This updates the transport mode (radiance to importance and vice
        versa).

        Returns → None:
            *no description available*

.. py:class:: mitsuba.render.BSDFFlags

    This list of flags is used to classify the different types of lobes
    that are implemented in a BSDF instance.

    They are also useful for picking out individual components, e.g., by
    setting combinations in BSDFContext::type_mask.

    Members:

    .. py:data:: None

        No flags set (default value)

    .. py:data:: Null

        'null' scattering event, i.e. particles do not undergo deflection

    .. py:data:: DiffuseReflection

        Ideally diffuse reflection

    .. py:data:: DiffuseTransmission

        Ideally diffuse transmission

    .. py:data:: GlossyReflection

        Glossy reflection

    .. py:data:: GlossyTransmission

        Glossy transmission

    .. py:data:: DeltaReflection

        Reflection into a discrete set of directions

    .. py:data:: DeltaTransmission

        Transmission into a discrete set of directions

    .. py:data:: Anisotropic

        The lobe is not invariant to rotation around the normal

    .. py:data:: SpatiallyVarying

        The BSDF depends on the UV coordinates

    .. py:data:: NonSymmetric

        Flags non-symmetry (e.g. transmission in dielectric materials)

    .. py:data:: FrontSide

        Supports interactions on the front-facing side

    .. py:data:: BackSide

        Supports interactions on the back-facing side

    .. py:data:: Reflection

        Any reflection component (scattering into discrete, 1D, or 2D set of
        directions)

    .. py:data:: Transmission

        Any transmission component (scattering into discrete, 1D, or 2D set of
        directions)

    .. py:data:: Diffuse

        Diffuse scattering into a 2D set of directions

    .. py:data:: Glossy

        Non-diffuse scattering into a 2D set of directions

    .. py:data:: Smooth

        Scattering into a 2D set of directions

    .. py:data:: Delta

        Scattering into a discrete set of directions

    .. py:data:: Delta1D

        Scattering into a 1D space of directions

    .. py:data:: All

        Any kind of scattering

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.render.BSDFSample3f

    Data structure holding the result of BSDF sampling operations.


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, wo)

        Given a surface interaction and an incident/exitant direction pair
        (wi, wo), create a query record to evaluate the BSDF or its sampling
        density.

        By default, all components will be sampled regardless of what measure
        they live on.

        Parameter ``wo`` (enoki.scalar.Vector3f):
            An outgoing direction in local coordinates. This should be a
            normalized direction vector that points *away* from the scattering
            event.

    .. py:method:: __init__(self, bs)

        Copy constructor

        Parameter ``bs`` (:py:obj:`mitsuba.render.BSDFSample3f`):
            *no description available*

    .. py:method:: mitsuba.render.BSDFSample3f.eta
        :property:

        Relative index of refraction in the sampled direction

    .. py:method:: mitsuba.render.BSDFSample3f.pdf
        :property:

        Probability density at the sample

    .. py:method:: mitsuba.render.BSDFSample3f.sampled_component
        :property:

        Stores the component index that was sampled by BSDF::sample()

    .. py:method:: mitsuba.render.BSDFSample3f.sampled_type
        :property:

        Stores the component type that was sampled by BSDF::sample()

    .. py:method:: mitsuba.render.BSDFSample3f.wo
        :property:

        Normalized outgoing direction in local coordinates

.. py:class:: mitsuba.render.DirectionSample3f

    Base class: :py:obj:`mitsuba.render.PositionSample3f`

    Record for solid-angle based area sampling techniques

    This data structure is used in techniques that sample positions
    relative to a fixed reference position in the scene. For instance,
    *direct illumination strategies* importance sample the incident
    radiance received by a given surface location. Mitsuba uses this
    approach in a wider bidirectional sense: sampling the incident
    importance due to a sensor also uses the same data structures and
    strategies, which are referred to as *direct sampling*.

    This record inherits all fields from PositionSample and extends it
    with two useful quantities that are cached so that they don't need to
    be recomputed: the unit direction and distance from the reference
    position to the sampled point.


    .. py:method:: __init__(self)

        Construct an unitialized direct sample

    .. py:method:: __init__(self, other)

        Construct from a position sample

        Parameter ``other`` (:py:obj:`mitsuba.render.PositionSample3f`):
            *no description available*

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.render.DirectionSample3f`):
            *no description available*

    .. py:method:: __init__(self, p, n, uv, time, pdf, delta, object, d, dist)

        Element-by-element constructor

        Parameter ``p`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``n`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``uv`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``time`` (float):
            *no description available*

        Parameter ``pdf`` (float):
            *no description available*

        Parameter ``delta`` (bool):
            *no description available*

        Parameter ``object`` (:py:obj:`mitsuba.core.Object`):
            *no description available*

        Parameter ``d`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``dist`` (float):
            *no description available*

    .. py:method:: __init__(self, si, ref)

        Create a position sampling record from a surface intersection

        This is useful to determine the hypothetical sampling density on a
        surface after hitting it using standard ray tracing. This happens for
        instance in path tracing with multiple importance sampling.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            *no description available*

        Parameter ``ref`` (:py:obj:`mitsuba.render.Interaction3f`):
            *no description available*

    .. py:method:: mitsuba.render.DirectionSample3f.d
        :property:

        Unit direction from the reference point to the target shape

    .. py:method:: mitsuba.render.DirectionSample3f.dist
        :property:

        Distance from the reference point to the target shape

    .. py:method:: mitsuba.render.DirectionSample3f.set_query(self, ray, si)

        Setup this record so that it can be used to *query* the density of a
        surface position (where the reference point lies on a *surface*).

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            Reference to the ray that generated the intersection ``si``. The
            ray origin must be located at the reference surface and point
            towards ``si``.p.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            A surface intersection record (usually on an emitter).

        \note Defined in scene.h

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.DirectionSample3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.render.DirectionSample3f`:
            *no description available*

.. py:class:: mitsuba.render.Emitter

    Base class: :py:obj:`mitsuba.render.Endpoint`

    .. py:method:: mitsuba.render.Emitter.flags(self, arg0)

        Flags for all components combined.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Emitter.is_environment(self)

        Is this an environment map light emitter?

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.render.EmitterFlags

    This list of flags is used to classify the different types of
    emitters.

    Members:

    .. py:data:: None

        No flags set (default value)

    .. py:data:: DeltaPosition

        The emitter lies at a single point in space

    .. py:data:: DeltaDirection

        The emitter emits light in a single direction

    .. py:data:: Infinite

        The emitter is placed at infinity (e.g. environment maps)

    .. py:data:: Surface

        The emitter is attached to a surface (e.g. area emitters)

    .. py:data:: SpatiallyVarying

        The emission depends on the UV coordinates

    .. py:data:: Delta

        Delta function in either position or direction

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.render.Endpoint

    Base class: :py:obj:`mitsuba.core.Object`

    Endpoint: an abstract interface to light sources and sensors

    This class implements an abstract interface to all sensors and light
    sources emitting radiance and importance, respectively. Subclasses
    implement functions to evaluate and sample the profile, and to compute
    probability densities associated with the provided sampling
    techniques.

    The name *endpoint* refers to the property that while a light path may
    involve any number of scattering events, it always starts and ends
    with emission and a measurement, respectively.

    In addition to Endpoint::sample_ray, which generates a sample from the
    profile, subclasses also provide a specialized direction sampling
    method. This is a generalization of direct illumination techniques to
    both emitters *and* sensors. A direction sampling method is given an
    arbitrary reference position in the scene and samples a direction from
    the reference point towards the endpoint (ideally proportional to the
    emission/sensitivity profile). This reduces the sampling domain from
    4D to 2D, which often enables the construction of smarter specialized
    sampling techniques.

    When rendering scenes involving participating media, it is important
    to know what medium surrounds the sensors and light sources. For this
    reason, every endpoint instance keeps a reference to a medium (which
    may be set to ``nullptr`` when it is surrounded by vacuum).

    .. py:method:: mitsuba.render.Endpoint.bbox(self)

        Return an axis-aligned box bounding the spatial extents of the emitter

        Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.eval(self, si, active=True)

        Given a ray-surface intersection, return the emitted radiance or
        importance traveling along the reverse direction

        This function is e.g. used when an area light source has been hit by a
        ray in a path tracing-style integrator, and it subsequently needs to
        be queried for the emitted radiance along the negative ray direction.
        The default implementation throws an exception, which states that the
        method is not implemented.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            An intersect record that specfies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector3f:
            The emitted radiance or importance

    .. py:method:: mitsuba.render.Endpoint.medium(self)

        Return a pointer to the medium that surrounds the emitter

        Returns → :py:obj:`mitsuba.render.Medium`:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.needs_sample_2(self)

        Does the method sample_ray() require a uniformly distributed 2D sample
        for the ``sample2`` parameter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.needs_sample_3(self)

        Does the method sample_ray() require a uniformly distributed 2D sample
        for the ``sample3`` parameter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.pdf_direction(self, it, ds, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        Parameter ``ds`` (:py:obj:`mitsuba.render.DirectionSample3f`):
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.render.Interaction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.sample_direction(self, it, sample, active=True)

        Given a reference point in the scene, sample a direction from the
        reference point towards the endpoint (ideally proportional to the
        emission/sensitivity profile)

        This operation is a generalization of direct illumination techniques
        to both emitters *and* sensors. A direction sampling method is given
        an arbitrary reference position in the scene and samples a direction
        from the reference point towards the endpoint (ideally proportional to
        the emission/sensitivity profile). This reduces the sampling domain
        from 4D to 2D, which often enables the construction of smarter
        specialized sampling techniques.

        Ideally, the implementation should importance sample the product of
        the emission profile and the geometry term between the reference point
        and the position on the endpoint.

        The default implementation throws an exception.

        Parameter ``ref``:
            A reference position somewhere within the scene.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``it`` (:py:obj:`mitsuba.render.Interaction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.render.DirectionSample3f`, enoki.scalar.Vector3f]:
            A DirectionSample instance describing the generated sample along
            with a spectral importance weight.

    .. py:method:: mitsuba.render.Endpoint.sample_ray(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray proportional to the endpoint's
        sensitivity/emission profile.

        The endpoint profile is a six-dimensional quantity that depends on
        time, wavelength, surface position, and direction. This function takes
        a given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray follows the
        profile. Any discrepancies between ideal and actual sampled profile
        are absorbed into a spectral importance weight that is returned along
        with the ray.

        Parameter ``time`` (float):
            The scene time associated with the ray to be sampled

        Parameter ``sample1`` (float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the emission profile.

        Parameter ``sample2`` (enoki.scalar.Vector2f):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument corresponds to the sample position
            in fractional pixel coordinates relative to the crop window of the
            underlying film. This argument is ignored if ``needs_sample_2() ==
            false``.

        Parameter ``sample3`` (enoki.scalar.Vector2f):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument determines the position on the
            aperture of the sensor. This argument is ignored if
            ``needs_sample_3() == false``.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.core.Ray3f`, enoki.scalar.Vector3f]:
            The sampled ray and (potentially spectrally varying) importance
            weights. The latter account for the difference between the profile
            and the actual used sampling density function.

    .. py:method:: mitsuba.render.Endpoint.set_medium(self, medium)

        Set the medium that surrounds the emitter.

        Parameter ``medium`` (:py:obj:`mitsuba.render.Medium`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.set_shape(self, shape)

        Set the shape associated with this endpoint.

        Parameter ``shape`` (:py:obj:`mitsuba.render.Shape`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.shape(self)

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.render.Shape`:
            *no description available*

    .. py:method:: mitsuba.render.Endpoint.world_transform(self)

        Return the local space to world space transformation

        Returns → :py:obj:`mitsuba.core.AnimatedTransform`:
            *no description available*

.. py:class:: mitsuba.render.Film

    Base class: :py:obj:`mitsuba.core.Object`

    Abstract film base class - used to store samples generated by
    Integrator implementations.

    To avoid lock-related bottlenecks when rendering with many cores,
    rendering threads first store results in an "image block", which is
    then committed to the film using the put() method.

    .. py:method:: mitsuba.render.Film.bitmap(self, raw=False)

        Return a bitmap object storing the developed contents of the film

        Parameter ``raw`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Bitmap`:
            *no description available*

    .. py:method:: mitsuba.render.Film.crop_offset(self)

        Return the offset of the crop window

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.render.Film.crop_size(self)

        Return the size of the crop window

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.render.Film.destination_exists(self, basename)

        Does the destination file already exist?

        Parameter ``basename`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Film.develop(overloaded)


        .. py:method:: develop(self)

        .. py:method:: develop(self, offset, size, target_offset, target)

            Parameter ``offset`` (enoki.scalar.Vector2i):
                *no description available*

            Parameter ``size`` (enoki.scalar.Vector2i):
                *no description available*

            Parameter ``target_offset`` (enoki.scalar.Vector2i):
                *no description available*

            Parameter ``target`` (:py:obj:`mitsuba.core.Bitmap`):
                *no description available*

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.render.Film.has_high_quality_edges(self)

        Should regions slightly outside the image plane be sampled to improve
        the quality of the reconstruction at the edges? This only makes sense
        when reconstruction filters other than the box filter are used.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Film.prepare(self, channels)

        Configure the film for rendering a specified set of channels

        Parameter ``channels`` (List[str]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Film.put(self, block)

        Merge an image block into the film. This methods should be thread-
        safe.

        Parameter ``block`` (:py:obj:`mitsuba.render.ImageBlock`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Film.reconstruction_filter(self)

        Return the image reconstruction filter (const version)

        Returns → :py:obj:`mitsuba.core.ReconstructionFilter`:
            *no description available*

    .. py:method:: mitsuba.render.Film.set_crop_window(self, arg0, arg1)

        Set the size and offset of the crop window.

        Parameter ``arg0`` (enoki.scalar.Vector2i):
            *no description available*

        Parameter ``arg1`` (enoki.scalar.Vector2i):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Film.set_destination_file(self, filename)

        Set the target filename (with or without extension)

        Parameter ``filename`` (:py:obj:`mitsuba.core.filesystem.path`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Film.size(self)

        Ignoring the crop window, return the resolution of the underlying
        sensor

        Returns → enoki.scalar.Vector2i:
            *no description available*

.. py:class:: mitsuba.render.HitComputeFlags

    Members:

    .. py:data:: None

        No flags set

    .. py:data:: Minimal

        Compute position and geometric normal

    .. py:data:: UV

        Compute UV coordinates

    .. py:data:: dPdUV

        Compute position partials wrt. UV coordinates

    .. py:data:: dNGdUV

        Compute the geometric normal partials wrt. the UV coordinates

    .. py:data:: dNSdUV

        Compute the shading normal partials wrt. the UV coordinates

    .. py:data:: ShadingFrame

        Compute shading normal and shading frame

    .. py:data:: NonDifferentiable

        Force computed fields to not be be differentiable

    .. py:data:: All

        Compute all fields of the surface interaction data structure (default)

    .. py:data:: AllNonDifferentiable

        Compute all fields of the surface interaction data structure in a non
        differentiable way

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.render.ImageBlock

    Base class: :py:obj:`mitsuba.core.Object`

    Storage for an image sub-block (a.k.a render bucket)

    This class is used by image-based parallel processes and encapsulates
    computed rectangular regions of an image. This allows for easy and
    efficient distributed rendering of large images. Image blocks usually
    also include a border region storing contributions that are slightly
    outside of the block, which is required to support image
    reconstruction filters.

    .. py:method:: __init__(self, size, channel_count, filter=None, warn_negative=True, warn_invalid=True, border=True, normalize=False)

        Parameter ``size`` (enoki.scalar.Vector2i):
            *no description available*

        Parameter ``channel_count`` (int):
            *no description available*

        Parameter ``filter`` (:py:obj:`mitsuba.core.ReconstructionFilter`):
            *no description available*

        Parameter ``warn_negative`` (bool):
            *no description available*

        Parameter ``warn_invalid`` (bool):
            *no description available*

        Parameter ``border`` (bool):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*


    .. py:method:: mitsuba.render.ImageBlock.border_size(self)

        Return the border region used by the reconstruction filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.channel_count(self)

        Return the number of channels stored by the image block

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.clear(self)

        Clear everything to zero.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.data(self)

        Return the underlying pixel buffer

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.height(self)

        Return the bitmap's height in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.offset(self)

        Return the current block offset

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.put(overloaded)


        .. py:method:: put(self, block)

            Accumulate another image block into this one

            Parameter ``block`` (:py:obj:`mitsuba.render.ImageBlock`):
                *no description available*

        .. py:method:: put(self, pos, wavelengths, value, alpha=1.0, active=True)

            Store a single sample / packets of samples inside the image block.

            \note This method is only valid if a reconstruction filter was given
            at the construction of the block.

            Parameter ``pos`` (enoki.scalar.Vector2f):
                Denotes the sample position in fractional pixel coordinates. It is
                not checked, and so must be valid. The block's offset is
                subtracted from the given position to obtain the

            Parameter ``wavelengths`` (enoki.scalar.Vector0f):
                Sample wavelengths in nanometers

            Parameter ``value`` (enoki.scalar.Vector3f):
                Sample value assocated with the specified wavelengths

            Parameter ``alpha`` (float):
                Alpha value assocated with the sample

            Returns → bool:
                ``False`` if one of the sample values was *invalid*, e.g. NaN or
                negative. A warning is also printed if ``m_warn_negative`` or
                ``m_warn_invalid`` is enabled.

            Parameter ``active`` (bool):
                Mask to specify active lanes.

        .. py:method:: put(self, pos, data, active=True)

            Parameter ``pos`` (enoki.scalar.Vector2f):
                *no description available*

            Parameter ``data`` (List[float]):
                *no description available*

            Parameter ``active`` (bool):
                Mask to specify active lanes.

    .. py:method:: mitsuba.render.ImageBlock.set_offset(self, offset)

        Set the current block offset.

        This corresponds to the offset from the top-left corner of a larger
        image (e.g. a Film) to the top-left corner of this ImageBlock
        instance.

        Parameter ``offset`` (enoki.scalar.Vector2i):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.set_warn_invalid(self, value)

        Warn when writing invalid (NaN, +/- infinity) sample values?

        Parameter ``value`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.set_warn_negative(self, value)

        Warn when writing negative sample values?

        Parameter ``value`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.size(self)

        Return the current block size

        Returns → enoki.scalar.Vector2i:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.warn_invalid(self)

        Warn when writing invalid (NaN, +/- infinity) sample values?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.warn_negative(self)

        Warn when writing negative sample values?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.ImageBlock.width(self)

        Return the bitmap's width in pixels

        Returns → int:
            *no description available*

.. py:class:: mitsuba.render.Integrator

    Base class: :py:obj:`mitsuba.core.Object`

    Abstract integrator base class, which does not make any assumptions
    with regards to how radiance is computed.

    In Mitsuba, the different rendering techniques are collectively
    referred to as *integrators*, since they perform integration over a
    high-dimensional space. Each integrator represents a specific approach
    for solving the light transport equation---usually favored in certain
    scenarios, but at the same time affected by its own set of intrinsic
    limitations. Therefore, it is important to carefully select an
    integrator based on user-specified accuracy requirements and
    properties of the scene to be rendered.

    This is the base class of all integrators; it does not make any
    assumptions on how radiance is computed, which allows for many
    different kinds of implementations.

    .. py:method:: mitsuba.render.Integrator.cancel(self)

        Cancel a running render job

        This function can be called asynchronously to cancel a running render
        job. In this case, render() will quit with a return value of
        ``False``.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Integrator.render(self, scene, sensor)

        Perform the main rendering job. Returns ``True`` upon success

        Parameter ``scene`` (:py:obj:`mitsuba.render.Scene`):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.render.Sensor`):
            *no description available*

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.render.Interaction3f

    Generic surface interaction data structure

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.render.Interaction3f.is_valid(self)

        Is the current interaction valid?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Interaction3f.p
        :property:

        Position of the interaction in world coordinates

    .. py:method:: mitsuba.render.Interaction3f.spawn_ray(self, d)

        Spawn a semi-infinite ray towards the given direction

        Parameter ``d`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Ray3f`:
            *no description available*

    .. py:method:: mitsuba.render.Interaction3f.spawn_ray_to(self, t)

        Spawn a finite ray towards the given position

        Parameter ``t`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → :py:obj:`mitsuba.core.Ray3f`:
            *no description available*

    .. py:method:: mitsuba.render.Interaction3f.t
        :property:

        Distance traveled along the ray

    .. py:method:: mitsuba.render.Interaction3f.time
        :property:

        Time value associated with the interaction

    .. py:method:: mitsuba.render.Interaction3f.wavelengths
        :property:

        Wavelengths associated with the ray that produced this interaction

    .. py:method:: mitsuba.render.Interaction3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.render.Interaction3f`:
            *no description available*

.. py:class:: mitsuba.render.Medium

    Base class: :py:obj:`mitsuba.core.Object`

    .. py:method:: mitsuba.render.Medium.eval_tr_and_pdf(self, mi, si, active=True)

        Parameter ``mi`` (:py:obj:`mitsuba.render.MediumInteraction3f`):
            *no description available*

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector3f, enoki.scalar.Vector3f]:
            *no description available*

    .. py:method:: mitsuba.render.Medium.get_combined_extinction(self, mi, active=True)

        Parameter ``mi`` (:py:obj:`mitsuba.render.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.Medium.get_scattering_coefficients(self, mi, active=True)

        Parameter ``mi`` (:py:obj:`mitsuba.render.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector3f, enoki.scalar.Vector3f, enoki.scalar.Vector3f]:
            *no description available*

    .. py:method:: mitsuba.render.Medium.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.render.Medium.intersect_aabb(self, ray)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Returns → Tuple[bool, float, float]:
            *no description available*

    .. py:method:: mitsuba.render.Medium.phase_function(self)

        Return the phase function of this medium

        Returns → :py:obj:`mitsuba.render.PhaseFunction`:
            *no description available*

    .. py:method:: mitsuba.render.Medium.sample_interaction(self, ray, sample, channel, active=True)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``sample`` (float):
            *no description available*

        Parameter ``channel`` (int):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.MediumInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.render.Medium.use_emitter_sampling(self)

        Returns whether this specific medium instance uses emitter sampling

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.render.MediumInteraction3f

    Base class: :py:obj:`mitsuba.render.Interaction3f`

    Stores information related to a medium scattering interaction

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.render.MediumInteraction3f.medium
        :property:

        Pointer to the associated medium

    .. py:method:: mitsuba.render.MediumInteraction3f.sh_frame
        :property:

        Shading frame

    .. py:method:: mitsuba.render.MediumInteraction3f.to_local(self, v)

        Convert a world-space vector into local shading coordinates

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.MediumInteraction3f.to_world(self, v)

        Convert a local shading-space vector into world space

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.MediumInteraction3f.wi
        :property:

        Incident direction in the local shading frame

    .. py:method:: mitsuba.render.MediumInteraction3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.render.MediumInteraction3f`:
            *no description available*

.. py:class:: mitsuba.render.Mesh

    Base class: :py:obj:`mitsuba.render.Shape`

    __init__(self: :py:obj:`mitsuba.render.Mesh`, name: str, vertex_count: int, face_count: int, props: :py:obj:`mitsuba.core.Properties` = Properties[
      plugin_name = "",
      id = "",
      elements = {
      }
    ]
    , has_vertex_normals: bool = False, has_vertex_texcoords: bool = False) -> None

    Create a new mesh with the given vertex and face data structures

    .. py:method:: mitsuba.render.Mesh.add_attribute(self, name, size, buffer)

        Add an attribute buffer with the given ``name`` and ``dim``

        Parameter ``name`` (str):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``buffer`` (enoki.dynamic.Float32):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.attribute_buffer(self, name)

        Return the mesh attribute associated with ``name``

        Parameter ``name`` (str):
            *no description available*

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.eval_parameterization(self, uv, active=True)

        Parameter ``uv`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.face_count(self)

        Return the total number of faces

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.faces_buffer(self)

        Return face indices buffer

        Returns → enoki.dynamic.UInt32:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.has_vertex_normals(self)

        Does this mesh have per-vertex normals?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.has_vertex_texcoords(self)

        Does this mesh have per-vertex texture coordinates?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.ray_intersect_triangle(self, index, ray, active=True)

        Ray-triangle intersection test

        Uses the algorithm by Moeller and Trumbore discussed at
        ``http://www.acm.org/jgt/papers/MollerTrumbore97/code.html``.

        Parameter ``index`` (int):
            Index of the triangle to be intersected.

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            The ray segment to be used for the intersection query.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.PreliminaryIntersection3f`:
            Returns an ordered tuple ``(mask, u, v, t)``, where ``mask``
            indicates whether an intersection was found, ``t`` contains the
            distance from the ray origin to the intersection point, and ``u``
            and ``v`` contains the first two components of the intersection in
            barycentric coordinates

    .. py:method:: mitsuba.render.Mesh.recompute_bbox(self)

        Recompute the bounding box (e.g. after modifying the vertex positions)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.recompute_vertex_normals(self)

        Compute smooth vertex normals and replace the current normal values

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.vertex_count(self)

        Return the total number of vertices

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.vertex_normals_buffer(self)

        Return vertex normals buffer

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.vertex_positions_buffer(self)

        Return vertex positions buffer

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.vertex_texcoords_buffer(self)

        Return vertex texcoords buffer

        Returns → enoki.dynamic.Float32:
            *no description available*

    .. py:method:: mitsuba.render.Mesh.write_ply(self, filename)

        Export mesh as a binary PLY file

        Parameter ``filename`` (str):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.render.MicrofacetDistribution

    Implementation of the Beckman and GGX / Trowbridge-Reitz microfacet
    distributions and various useful sampling routines

    Based on the papers

    "Microfacet Models for Refraction through Rough Surfaces" by Bruce
    Walter, Stephen R. Marschner, Hongsong Li, and Kenneth E. Torrance

    and

    "Importance Sampling Microfacet-Based BSDFs using the Distribution of
    Visible Normals" by Eric Heitz and Eugene D'Eon

    The visible normal sampling code was provided by Eric Heitz and Eugene
    D'Eon. An improvement of the Beckmann model sampling routine is
    discussed in

    "An Improved Visible Normal Sampling Routine for the Beckmann
    Distribution" by Wenzel Jakob

    An improvement of the GGX model sampling routine is discussed in "A
    Simpler and Exact Sampling Routine for the GGX Distribution of Visible
    Normals" by Eric Heitz


    .. py:method:: __init__(self, type, alpha, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.render.MicrofacetType`):
            *no description available*

        Parameter ``alpha`` (float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, type, alpha_u, alpha_v, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.render.MicrofacetType`):
            *no description available*

        Parameter ``alpha_u`` (float):
            *no description available*

        Parameter ``alpha_v`` (float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, type, alpha, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.render.MicrofacetType`):
            *no description available*

        Parameter ``alpha`` (float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, type, alpha_u, alpha_v, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.render.MicrofacetType`):
            *no description available*

        Parameter ``alpha_u`` (float):
            *no description available*

        Parameter ``alpha_v`` (float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Properties`):
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.G(self, wi, wo, m)

        Smith's separable shadowing-masking approximation

        Parameter ``wi`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``wo`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``m`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.alpha(self)

        Return the roughness (isotropic case)

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.alpha_u(self)

        Return the roughness along the tangent direction

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.alpha_v(self)

        Return the roughness along the bitangent direction

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.eval(self, m)

        Evaluate the microfacet distribution function

        Parameter ``m`` (enoki.scalar.Vector3f):
            The microfacet normal

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.is_anisotropic(self)

        Is this an anisotropic microfacet distribution?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.is_isotropic(self)

        Is this an isotropic microfacet distribution?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.pdf(self, wi, m)

        Returns the density function associated with the sample() function.

        Parameter ``wi`` (enoki.scalar.Vector3f):
            The incident direction (only relevant if visible normal sampling
            is used)

        Parameter ``m`` (enoki.scalar.Vector3f):
            The microfacet normal

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.sample(self, wi, sample)

        Draw a sample from the microfacet normal distribution and return the
        associated probability density

        Parameter ``sample`` (enoki.scalar.Vector2f):
            A uniformly distributed 2D sample

        Parameter ``pdf``:
            The probability density wrt. solid angles

        Parameter ``wi`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → Tuple[enoki.scalar.Vector3f, float]:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.sample_visible(self)

        Return whether or not only visible normals are sampled?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.sample_visible_11(self, cos_theta_i, sample)

        Visible normal sampling code for the alpha=1 case

        Parameter ``cos_theta_i`` (float):
            *no description available*

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.scale_alpha(self, value)

        Scale the roughness values by some constant

        Parameter ``value`` (float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.smith_g1(self, v, m)

        Smith's shadowing-masking function for a single direction

        Parameter ``v`` (enoki.scalar.Vector3f):
            An arbitrary direction

        Parameter ``m`` (enoki.scalar.Vector3f):
            The microfacet normal

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.MicrofacetDistribution.type(self)

        Return the distribution type

        Returns → :py:obj:`mitsuba.render.MicrofacetType`:
            *no description available*

.. py:class:: mitsuba.render.MicrofacetType

    Supported normal distribution functions

    Members:

    .. py:data:: Beckmann

        Beckmann distribution derived from Gaussian random surfaces

    .. py:data:: GGX

        GGX: Long-tailed distribution for very rough surfaces (aka.
        Trowbridge-Reitz distr.)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.render.MonteCarloIntegrator

    Base class: :py:obj:`mitsuba.render.SamplingIntegrator`

.. py:class:: mitsuba.render.PhaseFunction

    Base class: :py:obj:`mitsuba.core.Object`

    .. py:method:: mitsuba.render.PhaseFunction.eval(self, ctx, mi, wo, active=True)

        Evaluates the phase function model

        The function returns the value (which equals the PDF) of the phase
        function in the query direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.render.PhaseFunctionContext`):
            A phase function sampling context, contains information about the
            transport mode

        Parameter ``mi`` (:py:obj:`mitsuba.render.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``wo`` (enoki.scalar.Vector3f):
            An outgoing direction to evaluate.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            The value of the phase function in direction wo

    .. py:method:: mitsuba.render.PhaseFunction.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.render.PhaseFunction.sample(self, ctx, mi, sample1, active=True)

        Importance sample the phase function model

        The function returns a sampled direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.render.PhaseFunctionContext`):
            A phase function sampling context, contains information about the
            transport mode

        Parameter ``mi`` (:py:obj:`mitsuba.render.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``sample``:
            A uniformly distributed sample on $[0,1]^2$. It is used to
            generate the sampled direction.

        Parameter ``sample1`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector3f, float]:
            A sampled direction wo

.. py:class:: mitsuba.render.PhaseFunctionContext

    //! @}

    .. py:method:: mitsuba.render.PhaseFunctionContext.reverse(self)

        Reverse the direction of light transport in the record

        This updates the transport mode (radiance to importance and vice
        versa).

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.PhaseFunctionContext.sampler
        :property:

        Sampler object

.. py:class:: mitsuba.render.PhaseFunctionFlags

    This enumeration is used to classify phase functions into different
    types, i.e. into isotropic, anisotropic and microflake phase
    functions.

    This can be used to optimize implementatons to for example have less
    overhead if the phase function is not a microflake phase function.

    Members:

    .. py:data:: None

        

    .. py:data:: Isotropic

        

    .. py:data:: Anisotropic

        

    .. py:data:: Microflake

        

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:class:: mitsuba.render.PositionSample3f

    Generic sampling record for positions

    This sampling record is used to implement techniques that draw a
    position from a point, line, surface, or volume domain in 3D and
    furthermore provide auxilary information about the sample.

    Apart from returning the position and (optionally) the surface normal,
    the responsible sampling method must annotate the record with the
    associated probability density and delta.


    .. py:method:: __init__(self)

        Construct an unitialized position sample

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.render.PositionSample3f`):
            *no description available*

    .. py:method:: __init__(self, si)

        Create a position sampling record from a surface intersection

        This is useful to determine the hypothetical sampling density on a
        surface after hitting it using standard ray tracing. This happens for
        instance in path tracing with multiple importance sampling.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            *no description available*

    .. py:method:: mitsuba.render.PositionSample3f.delta
        :property:

        Set if the sample was drawn from a degenerate (Dirac delta)
        distribution

        Note: we use an array of booleans instead of a mask, so that slicing a
        dynamic array of PositionSample remains possible even on architectures
        where scalar_t<Mask> != bool (e.g. Knights Landing).

    .. py:method:: mitsuba.render.PositionSample3f.n
        :property:

        Sampled surface normal (if applicable)

    .. py:method:: mitsuba.render.PositionSample3f.object
        :property:

        Optional: pointer to an associated object

        In some uses of this record, sampling a position also involves
        choosing one of several objects (shapes, emitters, ..) on which the
        position lies. In that case, the ``object`` attribute stores a pointer
        to this object.

    .. py:method:: mitsuba.render.PositionSample3f.p
        :property:

        Sampled position

    .. py:method:: mitsuba.render.PositionSample3f.pdf
        :property:

        Probability density at the sample

    .. py:method:: mitsuba.render.PositionSample3f.time
        :property:

        Associated time value

    .. py:method:: mitsuba.render.PositionSample3f.uv
        :property:

        Optional: 2D sample position associated with the record

        In some uses of this record, a sampled position may be associated with
        an important 2D quantity, such as the texture coordinates on a
        triangle mesh or a position on the aperture of a sensor. When
        applicable, such positions are stored in the ``uv`` attribute.

    .. py:method:: mitsuba.render.PositionSample3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.render.PositionSample3f`:
            *no description available*

.. py:class:: mitsuba.render.PreliminaryIntersection3f

    Stores preliminary information related to a ray intersection

    This data structure is used as return type for the
    Shape::ray_intersect_preliminary efficient ray intersection routine.
    It stores whether the shape is intersected by a given ray, and cache
    preliminary information about the intersection if that is the case.

    If the intersection is deemed relevant, detailed intersection
    information can later be obtained via the create_surface_interaction()
    method.

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.render.PreliminaryIntersection3f.compute_surface_interaction(self, ray, flags=HitComputeFlags.All, active=True)

        Compute and return detailed information related to a surface
        interaction

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            Ray associated with the ray intersection

        Parameter ``flags`` (:py:obj:`mitsuba.render.HitComputeFlags`):
            Flags specifying which information should be computed

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.SurfaceInteraction3f`:
            A data structure containing the detailed information

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.instance
        :property:

        Stores a pointer to the parent instance (if applicable)

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.is_valid(self)

        Is the current interaction valid?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.prim_index
        :property:

        Primitive index, e.g. the triangle ID (if applicable)

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.prim_uv
        :property:

        2D coordinates on the primitive surface parameterization

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.shape
        :property:

        Pointer to the associated shape

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.shape_index
        :property:

        Shape index, e.g. the shape ID in shapegroup (if applicable)

    .. py:method:: mitsuba.render.PreliminaryIntersection3f.t
        :property:

        Distance traveled along the ray

.. py:class:: mitsuba.render.ProjectiveCamera

    Base class: :py:obj:`mitsuba.render.Sensor`

    Projective camera interface

    This class provides an abstract interface to several types of sensors
    that are commonly used in computer graphics, such as perspective and
    orthographic camera models.

    The interface is meant to be implemented by any kind of sensor, whose
    world to clip space transformation can be explained using only linear
    operations on homogeneous coordinates.

    A useful feature of ProjectiveCamera sensors is that their view can be
    rendered using the traditional OpenGL pipeline.

    .. py:method:: mitsuba.render.ProjectiveCamera.far_clip(self)

        Return the far clip plane distance

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.ProjectiveCamera.focus_distance(self)

        Return the distance to the focal plane

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.ProjectiveCamera.near_clip(self)

        Return the near clip plane distance

        Returns → float:
            *no description available*

.. py:class:: mitsuba.render.Sampler

    Base class: :py:obj:`mitsuba.core.Object`

    Base class of all sample generators.

    For each sample in a pixel, a sample generator produces a
    (hypothetical) point in the infinite dimensional random number cube. A
    rendering algorithm can then request subsequent 1D or 2D components of
    this point using the ``next_1d`` and ``next_2d`` functions.

    Scalar and wavefront rendering algorithms will need interact with the
    sampler interface in a slightly different way:

    Scalar rendering algorithm:

    1. Before beginning to render a pixel block, the rendering algorithm
    calls ``seed`` to initialize a new sequence with the specific seed
    offset. 2. The first pixel sample can now be computed, after which
    ``advance`` needs to be invoked. This repeats until all pixel samples
    have been generated. Note that some implementations need to be
    configured for a certain number of pixel samples, and exceeding these
    will lead to an exception being thrown. 3. While computing a pixel
    sample, the rendering algorithm usually requests batches of (pseudo-)
    random numbers using the ``next_1d`` and ``next_2d`` functions before
    moving on to the next sample.

    Wavefront rendering algorithm:

    1. Before beginning to render the wavefront, the rendering algorithm
    needs to inform the sampler of the amount of samples rendered in
    parallel for every pixel in the wavefront. This can be achieved by
    calling ``set_samples_per_wavefront`` . 2. Then the rendering
    algorithm should seed the sampler and set the appropriate wavefront
    size by calling ``seed``. A different seed value, based on the
    ``base_seed`` and the seed offset, will be used for every sample (of
    every pixel) in the wavefront. 3. ``advance`` can be used to advance
    to the next sample in the sequence. 4. As in the scalar approach, the
    rendering algorithm can request batches of (pseudo-) random numbers
    using the ``next_1d`` and ``next_2d`` functions.

    .. py:method:: mitsuba.render.Sampler.advance(self)

        Advance to the next sample.

        A subsequent call to ``next_1d`` or ``next_2d`` will access the first
        1D or 2D components of this sample.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.clone(self)

        Create a clone of this sampler

        The clone is allowed to be different to some extent, e.g. a
        pseudorandom generator should be based on a different random seed
        compared to the original. All other parameters are copied exactly.

        May throw an exception if not supported. Cloning may also change the
        state of the original sampler (e.g. by using the next 1D sample as a
        seed for the clone).

        Returns → :py:obj:`mitsuba.render.Sampler`:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.next_1d(self, active=True)

        Retrieve the next component value from the current sample

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.next_2d(self, active=True)

        Retrieve the next two component values from the current sample

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector2f:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.sample_count(self)

        Return the number of samples per pixel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.seed(self, seed_offset, wavefront_size=1)

        Deterministically seed the underlying RNG, if applicable.

        In the context of wavefront ray tracing & dynamic arrays, this
        function must be called with ``wavefront_size`` matching the size of
        the wavefront.

        Parameter ``seed_offset`` (int):
            *no description available*

        Parameter ``wavefront_size`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.set_samples_per_wavefront(self, samples_per_wavefront)

        Set the number of samples per pass in wavefront modes (default is 1)

        Parameter ``samples_per_wavefront`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Sampler.wavefront_size(self)

        Return the size of the wavefront (or 0, if not seeded)

        Returns → int:
            *no description available*

.. py:class:: mitsuba.render.SamplingIntegrator

    Base class: :py:obj:`mitsuba.render.Integrator`

    Integrator based on Monte Carlo sampling

    This integrator performs Monte Carlo integration to return an unbiased
    statistical estimate of the radiance value along a given ray. The
    default implementation of the render() method then repeatedly invokes
    this estimator to compute all pixels of the image.

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.core.Properties`):
            *no description available*


    .. py:method:: mitsuba.render.SamplingIntegrator.aov_names(self)

        For integrators that return one or more arbitrary output variables
        (AOVs), this function specifies a list of associated channel names.
        The default implementation simply returns an empty vector.

        Returns → List[str]:
            *no description available*

    .. py:method:: mitsuba.render.SamplingIntegrator.sample(self, scene, sampler, ray, medium=None, active=True)

        Sample the incident radiance along a ray.

        Parameter ``scene`` (:py:obj:`mitsuba.render.Scene`):
            The underlying scene in which the radiance function should be
            sampled

        Parameter ``sampler`` (:py:obj:`mitsuba.render.Sampler`):
            A source of (pseudo-/quasi-) random numbers

        Parameter ``ray`` (:py:obj:`mitsuba.core.RayDifferential3f`):
            A ray, optionally with differentials

        Parameter ``medium`` (:py:obj:`mitsuba.render.Medium`):
            If the ray is inside a medium, this parameter holds a pointer to
            that medium

        Parameter ``active`` (bool):
            A mask that indicates which SIMD lanes are active

        Parameter ``aov``:
            Integrators may return one or more arbitrary output variables
            (AOVs) via this parameter. If ``nullptr`` is provided to this
            argument, no AOVs should be returned. Otherwise, the caller
            guarantees that space for at least ``aov_names().size()`` entries
            has been allocated.

        Returns → Tuple[enoki.scalar.Vector3f, bool, List[float]]:
            A pair containing a spectrum and a mask specifying whether a
            surface or medium interaction was sampled. False mask entries
            indicate that the ray "escaped" the scene, in which case the the
            returned spectrum contains the contribution of environment maps,
            if present. The mask can be used to estimate a suitable alpha
            channel of a rendered image.

        Remark:
            In the Python bindings, this function returns the ``aov`` output
            argument as an additional return value. In other words: `` (spec,
            mask, aov) = integrator.sample(scene, sampler, ray, medium,
            active) ``

    .. py:method:: mitsuba.render.SamplingIntegrator.should_stop(self)

        Indicates whether cancel() or a timeout have occured. Should be
        checked regularly in the integrator's main loop so that timeouts are
        enforced accurately.

        Note that accurate timeouts rely on m_render_timer, which needs to be
        reset at the beginning of the rendering phase.

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.render.Scene

    Base class: :py:obj:`mitsuba.core.Object`

    .. py:method:: mitsuba.render.Scene.bbox(self)

        Return a bounding box surrounding the scene

        Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.render.Scene.emitters(self)

        Return the list of emitters

        Returns → List[:py:obj:`mitsuba.render.Emitter`]:
            *no description available*

    .. py:method:: mitsuba.render.Scene.environment(self)

        Return the environment emitter (if any)

        Returns → :py:obj:`mitsuba.render.Emitter`:
            *no description available*

    .. py:method:: mitsuba.render.Scene.integrator(self)

        Return the scene's integrator

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.render.Scene.pdf_emitter_direction(self, ref, active=True)

        Parameter ``ref`` (:py:obj:`mitsuba.render.Interaction`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.Scene.ray_intersect(overloaded)


        .. py:method:: ray_intersect(self, ray, active=True)

            Intersect a ray against all primitives stored in the scene and return
            information about the resulting surface interaction

            Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
                A 3-dimensional ray data structure with minimum/maximum extent
                information, as well as a time value (which matters when the
                shapes are in motion)

            Returns → :py:obj:`mitsuba.render.SurfaceInteraction`:
                A detailed surface interaction record. Query its ``is_valid()``
                method to determine whether an intersection was actually found.

            Parameter ``active`` (bool):
                Mask to specify active lanes.

        .. py:method:: ray_intersect(self, ray, flags, active=True)

            Intersect a ray against all primitives stored in the scene and return
            information about the resulting surface interaction

            Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
                A 3-dimensional ray data structure with minimum/maximum extent
                information, as well as a time value (which matters when the
                shapes are in motion)

            Returns → :py:obj:`mitsuba.render.SurfaceInteraction`:
                A detailed surface interaction record. Query its ``is_valid()``
                method to determine whether an intersection was actually found.

            Parameter ``flags`` (:py:obj:`mitsuba.render.HitComputeFlags`):
                *no description available*

            Parameter ``active`` (bool):
                Mask to specify active lanes.

    .. py:method:: mitsuba.render.Scene.ray_intersect_naive(self, ray, active=True)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.SurfaceInteraction`:
            *no description available*

    .. py:method:: mitsuba.render.Scene.ray_intersect_preliminary(self, ray, active=True)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.PreliminaryIntersection`:
            *no description available*

    .. py:method:: mitsuba.render.Scene.ray_test(self, ray, active=True)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Scene.sample_emitter_direction(self, ref, sample, test_visibility=True, mask=True)

        Parameter ``ref`` (:py:obj:`mitsuba.render.Interaction`):
            *no description available*

        Parameter ``sample`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``test_visibility`` (bool):
            *no description available*

        Parameter ``mask`` (bool):
            *no description available*

        Returns → Tuple[:py:obj:`mitsuba.render.DirectionSample`, enoki.scalar.Vector3f]:
            *no description available*

    .. py:method:: mitsuba.render.Scene.sensors(self)

        Return the list of sensors

        Returns → List[:py:obj:`mitsuba.render.Sensor`]:
            *no description available*

    .. py:method:: mitsuba.render.Scene.shapes(self)

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.render.Scene.shapes_grad_enabled(self)

        Return whether any of the shape's parameters require gradient

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.render.Sensor

    Base class: :py:obj:`mitsuba.render.Endpoint`

    .. py:method:: mitsuba.render.Sensor.film(self)

        Return the Film instance associated with this sensor

        Returns → :py:obj:`mitsuba.render.Film`:
            *no description available*

    .. py:method:: mitsuba.render.Sensor.needs_aperture_sample(self)

        Does the sampling technique require a sample for the aperture
        position?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Sensor.sample_ray_differential(self, time, sample1, sample2, sample3, active=True)

        Parameter ``time`` (float):
            *no description available*

        Parameter ``sample1`` (float):
            *no description available*

        Parameter ``sample2`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``sample3`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.core.RayDifferential3f`, enoki.scalar.Vector3f]:
            *no description available*

    .. py:method:: mitsuba.render.Sensor.sampler(self)

        Return the sensor's sample generator

        This is the *root* sampler, which will later be cloned a number of
        times to provide each participating worker thread with its own
        instance (see Scene::sampler()). Therefore, this sampler should never
        be used for anything except creating clones.

        Returns → :py:obj:`mitsuba.render.Sampler`:
            *no description available*

    .. py:method:: mitsuba.render.Sensor.shutter_open(self)

        Return the time value of the shutter opening event

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.Sensor.shutter_open_time(self)

        Return the length, for which the shutter remains open

        Returns → float:
            *no description available*

.. py:class:: mitsuba.render.Shape

    Base class: :py:obj:`mitsuba.core.Object`

    Base class of all geometric shapes in Mitsuba

    This class provides core functionality for sampling positions on
    surfaces, computing ray intersections, and bounding shapes within ray
    intersection acceleration data structures.

    .. py:method:: mitsuba.render.Shape.bbox(overloaded)


        .. py:method:: bbox(self)

            Return an axis aligned box that bounds all shape primitives (including
            any transformations that may have been applied to them)

            Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
                *no description available*

        .. py:method:: bbox(self, index)

            Return an axis aligned box that bounds a single shape primitive
            (including any transformations that may have been applied to it)

            Remark:
                The default implementation simply calls bbox()

            Parameter ``index`` (int):
                *no description available*

            Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
                *no description available*

        .. py:method:: bbox(self, index, clip)

            Return an axis aligned box that bounds a single shape primitive after
            it has been clipped to another bounding box.

            This is extremely important to construct high-quality kd-trees. The
            default implementation just takes the bounding box returned by
            bbox(ScalarIndex index) and clips it to *clip*.

            Parameter ``index`` (int):
                *no description available*

            Parameter ``clip`` (:py:obj:`mitsuba.core.BoundingBox3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
                *no description available*

    .. py:method:: mitsuba.render.Shape.bsdf(self)

        Returns → :py:obj:`mitsuba.render.BSDF`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.compute_surface_interaction(self, ray, pi, flags=HitComputeFlags.All, active=True)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``pi`` (:py:obj:`mitsuba.render.PreliminaryIntersection3f`):
            *no description available*

        Parameter ``flags`` (:py:obj:`mitsuba.render.HitComputeFlags`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.effective_primitive_count(self)

        Return the number of primitives (triangles, hairs, ..) contributed to
        the scene by this shape

        Includes instanced geometry. The default implementation simply returns
        the same value as primitive_count().

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Shape.emitter(self, active=True)

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.Emitter`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.exterior_medium(self)

        Return the medium that lies on the exterior of this shape

        Returns → :py:obj:`mitsuba.render.Medium`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.render.Shape.interior_medium(self)

        Return the medium that lies on the interior of this shape

        Returns → :py:obj:`mitsuba.render.Medium`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.is_emitter(self)

        Is this shape also an area emitter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Shape.is_medium_transition(self)

        Does the surface of this shape mark a medium transition?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Shape.is_mesh(self)

        Is this shape a triangle mesh?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Shape.is_sensor(self)

        Is this shape also an area sensor?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Shape.parameters_grad_enabled(self)

        Return whether shape's parameters require gradients (default
        implementation return false)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Shape.pdf_direction(self, it, ps, active=True)

        Query the probability density of sample_direction()

        Parameter ``it`` (:py:obj:`mitsuba.render.Interaction3f`):
            A reference position somewhere within the scene.

        Parameter ``ps`` (:py:obj:`mitsuba.render.DirectionSample3f`):
            A position record describing the sample in question

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            The probability density per unit solid angle

    .. py:method:: mitsuba.render.Shape.pdf_position(self, ps, active=True)

        Query the probability density of sample_position() for a particular
        point on the surface.

        Parameter ``ps`` (:py:obj:`mitsuba.render.PositionSample3f`):
            A position record describing the sample in question

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            The probability density per unit area

    .. py:method:: mitsuba.render.Shape.primitive_count(self)

        Returns the number of sub-primitives that make up this shape

        Remark:
            The default implementation simply returns ``1``

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Shape.ray_intersect(self, ray, flags=HitComputeFlags.All, active=True)

        Test for an intersection and return detailed information

        This operation combines the prior ray_intersect_preliminary() and
        compute_surface_interaction() operations.

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``flags`` (:py:obj:`mitsuba.render.HitComputeFlags`):
            Describe how the detailed information should be computed

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.ray_intersect_preliminary(self, ray, active=True)

        Fast ray intersection test

        Efficiently test whether the shape is intersected by the given ray,
        and cache preliminary information about the intersection if that is
        the case.

        If the intersection is deemed relevant (e.g. the closest to the ray
        origin), detailed intersection information can later be obtained via
        the create_surface_interaction() method.

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``cache``:
            Temporary space (``(MTS_KD_INTERSECTION_CACHE_SIZE-2) *
            sizeof(Float[P])`` bytes) that must be supplied to cache
            information about the intersection.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.PreliminaryIntersection3f`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.ray_test(self, ray, active=True)

        Parameter ``ray`` (:py:obj:`mitsuba.core.Ray3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Shape.sample_direction(self, it, sample, active=True)

        Sample a direction towards this shape with respect to solid angles
        measured at a reference position within the scene

        An ideal implementation of this interface would achieve a uniform
        solid angle density within the surface region that is visible from the
        reference position ``it.p`` (though such an ideal implementation is
        usually neither feasible nor advisable due to poor efficiency).

        The function returns the sampled position and the inverse probability
        per unit solid angle associated with the sample.

        When the Shape subclass does not supply a custom implementation of
        this function, the Shape class reverts to a fallback approach that
        piggybacks on sample_position(). This will generally lead to a
        suboptimal sample placement and higher variance in Monte Carlo
        estimators using the samples.

        Parameter ``it`` (:py:obj:`mitsuba.render.Interaction3f`):
            A reference position somewhere within the scene.

        Parameter ``sample`` (enoki.scalar.Vector2f):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.DirectionSample3f`:
            A DirectionSample instance describing the generated sample

    .. py:method:: mitsuba.render.Shape.sample_position(self, time, sample, active=True)

        Sample a point on the surface of this shape

        The sampling strategy is ideally uniform over the surface, though
        implementations are allowed to deviate from a perfectly uniform
        distribution as long as this is reflected in the returned probability
        density.

        Parameter ``time`` (float):
            The scene time associated with the position sample

        Parameter ``sample`` (enoki.scalar.Vector2f):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.PositionSample3f`:
            A PositionSample instance describing the generated sample

    .. py:method:: mitsuba.render.Shape.sensor(self)

        Returns → :py:obj:`mitsuba.render.Sensor`:
            *no description available*

    .. py:method:: mitsuba.render.Shape.surface_area(self)

        Return the shape's surface area.

        The function assumes that the object is not undergoing some kind of
        time-dependent scaling.

        The default implementation throws an exception.

        Returns → float:
            *no description available*

.. py:class:: mitsuba.render.ShapeKDTree

    Base class: :py:obj:`mitsuba.core.Object`

    Create an empty kd-tree and take build-related parameters from
    ``props``.

    .. py:method:: mitsuba.render.ShapeKDTree.add_shape(self, arg0)

        Register a new shape with the kd-tree (to be called before build())

        Parameter ``arg0`` (:py:obj:`mitsuba.render.Shape`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.ShapeKDTree.bbox(self)

        Returns → :py:obj:`mitsuba.core.BoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.render.ShapeKDTree.build(overloaded)


        .. py:method:: build(self)

            Build the kd-tree

        .. py:method:: build(self)

            Build the kd-tree

    .. py:method:: mitsuba.render.ShapeKDTree.primitive_count(self)

        Return the number of registered primitives

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.ShapeKDTree.shape(self, arg0)

        Return the i-th shape (const version)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.render.Shape`:
            *no description available*

    .. py:method:: mitsuba.render.ShapeKDTree.shape_count(self)

        Return the number of registered shapes

        Returns → int:
            *no description available*

.. py:class:: mitsuba.render.Spiral

    Base class: :py:obj:`mitsuba.core.Object`

    Generates a spiral of blocks to be rendered.

    Author:
        Adam Arbree Aug 25, 2005 RayTracer.java Used with permission.
        Copyright 2005 Program of Computer Graphics, Cornell University

    .. py:method:: __init__(self, size, offset, block_size=32, passes=1)

        Create a new spiral generator for the given size, offset into a larger
        frame, and block size

        Parameter ``size`` (enoki.scalar.Vector2i):
            *no description available*

        Parameter ``offset`` (enoki.scalar.Vector2i):
            *no description available*

        Parameter ``block_size`` (int):
            *no description available*

        Parameter ``passes`` (int):
            *no description available*

        
    .. py:method:: mitsuba.render.Spiral.block_count(self)

        Return the total number of blocks

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Spiral.max_block_size(self)

        Return the maximum block size

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.render.Spiral.next_block(self)

        Return the offset, size and unique identifer of the next block.

        A size of zero indicates that the spiral traversal is done.

        Returns → Tuple[enoki.scalar.Vector2i, enoki.scalar.Vector2i, int]:
            *no description available*

    .. py:method:: mitsuba.render.Spiral.reset(self)

        Reset the spiral to its initial state. Does not affect the number of
        passes.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.Spiral.set_passes(self, arg0)

        Sets the number of time the spiral should automatically reset. Not
        affected by a call to reset.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.render.SurfaceInteraction3f

    Base class: :py:obj:`mitsuba.render.Interaction3f`

    Stores information related to a surface scattering interaction


    .. py:method:: __init__(self)

        Construct from a position sample. Unavailable fields such as `wi` and
        the partial derivatives are left uninitialized. The `shape` pointer is
        left uninitialized because we can't guarantee that the given
        PositionSample::object points to a Shape instance.

    .. py:method:: __init__(self, ps, wavelengths)

        Construct from a position sample. Unavailable fields such as `wi` and
        the partial derivatives are left uninitialized. The `shape` pointer is
        left uninitialized because we can't guarantee that the given
        PositionSample::object points to a Shape instance.

        Parameter ``ps`` (:py:obj:`mitsuba.render.PositionSample`):
            *no description available*

        Parameter ``wavelengths`` (enoki.scalar.Vector0f):
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.bsdf(overloaded)


        .. py:method:: bsdf(self, ray)

            Returns the BSDF of the intersected shape.

            The parameter ``ray`` must match the one used to create the
            interaction record. This function computes texture coordinate partials
            if this is required by the BSDF (e.g. for texture filtering).

            Implementation in 'bsdf.h'

            Parameter ``ray`` (:py:obj:`mitsuba.core.RayDifferential3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.render.BSDF`:
                *no description available*

        .. py:method:: bsdf(self)

            Returns → :py:obj:`mitsuba.render.BSDF`:
                *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.compute_uv_partials(self, ray)

        Computes texture coordinate partials

        Parameter ``ray`` (:py:obj:`mitsuba.core.RayDifferential3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.dn_du
        :property:

        Normal partials wrt. the UV parameterization

    .. py:method:: mitsuba.render.SurfaceInteraction3f.dn_dv
        :property:

        Normal partials wrt. the UV parameterization

    .. py:method:: mitsuba.render.SurfaceInteraction3f.dp_du
        :property:

        Position partials wrt. the UV parameterization

    .. py:method:: mitsuba.render.SurfaceInteraction3f.dp_dv
        :property:

        Position partials wrt. the UV parameterization

    .. py:method:: mitsuba.render.SurfaceInteraction3f.duv_dx
        :property:

        UV partials wrt. changes in screen-space

    .. py:method:: mitsuba.render.SurfaceInteraction3f.duv_dy
        :property:

        UV partials wrt. changes in screen-space

    .. py:method:: mitsuba.render.SurfaceInteraction3f.emitter(self, scene, active=True)

        Return the emitter associated with the intersection (if any) \note
        Defined in scene.h

        Parameter ``scene`` (:py:obj:`mitsuba.render.Scene`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.render.Emitter`:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.has_n_partials(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.has_uv_partials(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.instance
        :property:

        Stores a pointer to the parent instance (if applicable)

    .. py:method:: mitsuba.render.SurfaceInteraction3f.is_medium_transition(self)

        Does the surface mark a transition between two media?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.is_sensor(self)

        Is the intersected shape also a sensor?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.n
        :property:

        Geometric normal

    .. py:method:: mitsuba.render.SurfaceInteraction3f.prim_index
        :property:

        Primitive index, e.g. the triangle ID (if applicable)

    .. py:method:: mitsuba.render.SurfaceInteraction3f.sh_frame
        :property:

        Shading frame

    .. py:method:: mitsuba.render.SurfaceInteraction3f.shape
        :property:

        Pointer to the associated shape

    .. py:method:: mitsuba.render.SurfaceInteraction3f.target_medium(overloaded)


        .. py:method:: target_medium(self, d)

            Determine the target medium

            When ``is_medium_transition()`` = ``True``, determine the medium that
            contains the ``ray(this->p, d)``

            Parameter ``d`` (enoki.scalar.Vector3f):
                *no description available*

            Returns → :py:obj:`mitsuba.render.Medium`:
                *no description available*

        .. py:method:: target_medium(self, cos_theta)

            Determine the target medium based on the cosine of the angle between
            the geometric normal and a direction

            Returns the exterior medium when ``cos_theta > 0`` and the interior
            medium when ``cos_theta <= 0``.

            Parameter ``cos_theta`` (float):
                *no description available*

            Returns → :py:obj:`mitsuba.render.Medium`:
                *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.to_local(self, v)

        Convert a world-space vector into local shading coordinates

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.to_local_mueller(self, M_world, wi_world, wo_world)

        Converts a Mueller matrix defined in world space to a local frame

        A Mueller matrix operates from the (implicitly) defined frame
        stokes_basis(in_forward) to the frame stokes_basis(out_forward). This
        method converts a Mueller matrix defined on directions in world-space
        to a Mueller matrix defined in the local frame.

        This expands to a no-op in non-polarized modes.

        Parameter ``in_forward_local``:
            Incident direction (along propagation direction of light), given
            in world-space coordinates.

        Parameter ``wo_local``:
            Outgoing direction (along propagation direction of light), given
            in world-space coordinates.

        Parameter ``M_world`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``wi_world`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``wo_world`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            Equivalent Mueller matrix that operates in local frame
            coordinates.

    .. py:method:: mitsuba.render.SurfaceInteraction3f.to_world(self, v)

        Convert a local shading-space vector into world space

        Parameter ``v`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:method:: mitsuba.render.SurfaceInteraction3f.to_world_mueller(self, M_local, wi_local, wo_local)

        Converts a Mueller matrix defined in a local frame to world space

        A Mueller matrix operates from the (implicitly) defined frame
        stokes_basis(in_forward) to the frame stokes_basis(out_forward). This
        method converts a Mueller matrix defined on directions in the local
        frame to a Mueller matrix defined on world-space directions.

        This expands to a no-op in non-polarized modes.

        Parameter ``M_local`` (enoki.scalar.Vector3f):
            The Mueller matrix in local space, e.g. returned by a BSDF.

        Parameter ``in_forward_local``:
            Incident direction (along propagation direction of light), given
            in local frame coordinates.

        Parameter ``wo_local`` (enoki.scalar.Vector3f):
            Outgoing direction (along propagation direction of light), given
            in local frame coordinates.

        Parameter ``wi_local`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            Equivalent Mueller matrix that operates in world-space
            coordinates.

    .. py:method:: mitsuba.render.SurfaceInteraction3f.uv
        :property:

        UV surface coordinates

    .. py:method:: mitsuba.render.SurfaceInteraction3f.wi
        :property:

        Incident direction in the local shading frame

    .. py:method:: mitsuba.render.SurfaceInteraction3f.zero(size=1)

        Parameter ``size`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.render.SurfaceInteraction3f`:
            *no description available*

.. py:class:: mitsuba.render.Texture

    Base class: :py:obj:`mitsuba.core.Object`

    Base class of all surface texture implementations

    This class implements a generic texture map that supports evaluation
    at arbitrary surface positions and wavelengths (if compiled in
    spectral mode). It can be used to provide both intensities (e.g. for
    light sources) and unitless reflectance parameters (e.g. an albedo of
    a reflectance model).

    The spectrum can be evaluated at arbitrary (continuous) wavelengths,
    though the underlying function it is not required to be smooth or even
    continuous.

    .. py:method:: mitsuba.render.Texture.D65(scale=1.0)

        Parameter ``scale`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.render.Texture`:
            *no description available*

    .. py:method:: mitsuba.render.Texture.eval(self, si, active=True)

        Evaluate the texture at the given surface interaction

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector3f:
            An unpolarized spectral power distribution or reflectance value

    .. py:method:: mitsuba.render.Texture.eval_1(self, si, active=True)

        Monochromatic evaluation of the texture at the given surface
        interaction

        This function differs from eval() in that it provided raw access to
        scalar intensity/reflectance values without any color processing (e.g.
        spectral upsampling). This is useful in parts of the renderer that
        encode scalar quantities using textures, e.g. a height field.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            An scalar intensity or reflectance value

    .. py:method:: mitsuba.render.Texture.eval_3(self, si, active=True)

        Trichromatic evaluation of the texture at the given surface
        interaction

        This function differs from eval() in that it provided raw access to
        RGB intensity/reflectance values without any additional color
        processing (e.g. RGB-to-spectral upsampling). This is useful in parts
        of the renderer that encode 3D quantities using textures, e.g. a
        normal map.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector3f:
            An trichromatic intensity or reflectance value

    .. py:method:: mitsuba.render.Texture.is_spatially_varying(self)

        Does this texture evaluation depend on the UV coordinates

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.render.Texture.mean(self)

        Return the mean value of the spectrum over the support
        (MTS_WAVELENGTH_MIN..MTS_WAVELENGTH_MAX)

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Even if the operation is provided, it may only return an
        approximation.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.Texture.pdf_position(self, p, active=True)

        Returns the probability per unit area of sample_position()

        Parameter ``p`` (enoki.scalar.Vector2f):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.render.Texture.pdf_spectrum(self, si, active=True)

        Evaluate the density function of the sample_spectrum() method as a
        probability per unit wavelength (in units of 1/nm).

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → enoki.scalar.Vector0f:
            A density value for each wavelength in ``si.wavelengths`` (hence
            the Wavelength type).

    .. py:method:: mitsuba.render.Texture.sample_position(self, sample, active=True)

        Importance sample a surface position proportional to the overall
        spectral reflectance or intensity of the texture

        This function assumes that the texture is implemented as a mapping
        from 2D UV positions to texture values, which is not necessarily true
        for all textures (e.g. 3D noise functions, mesh attributes, etc.). For
        this reason, not every will plugin provide a specialized
        implementation, and the default implementation simply return the input
        sample (i.e. uniform sampling is used).

        Parameter ``sample`` (enoki.scalar.Vector2f):
            A 2D vector of uniform variates

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector2f, float]:
            1. A texture-space position in the range :math:`[0, 1]^2`

        2. The associated probability per unit area in UV space

    .. py:method:: mitsuba.render.Texture.sample_spectrum(self, si, sample, active=True)

        Importance sample a set of wavelengths proportional to the spectrum
        defined at the given surface position

        Not every implementation necessarily provides this function, and it is
        a no-op when compiling non-spectral variants of Mitsuba. The default
        implementation throws an exception.

        Parameter ``si`` (:py:obj:`mitsuba.render.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``sample`` (enoki.scalar.Vector0f):
            A uniform variate for each desired wavelength.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → Tuple[enoki.scalar.Vector0f, enoki.scalar.Vector3f]:
            1. Set of sampled wavelengths specified in nanometers

        2. The Monte Carlo importance weight (Spectral power distribution
        value divided by the sampling density)

.. py:class:: mitsuba.render.TransportMode

    Specifies the transport mode when sampling or evaluating a scattering
    function

    Members:

    .. py:data:: Radiance

        Radiance transport

    .. py:data:: Importance

        Importance transport

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*


.. py:function:: mitsuba.render.eval_reflectance(type, alpha_u, alpha_v, wi, eta)

    Parameter ``type`` (:py:obj:`mitsuba.render.MicrofacetType`):
        *no description available*

    Parameter ``alpha_u`` (float):
        *no description available*

    Parameter ``alpha_v`` (float):
        *no description available*

    Parameter ``wi`` (:py:obj:`mitsuba.render.Vector`):
        *no description available*

    Parameter ``eta`` (float):
        *no description available*

    Returns → enoki.dynamic.Float32:
        *no description available*

.. py:function:: mitsuba.render.fresnel(cos_theta_i, eta)

    Calculates the unpolarized Fresnel reflection coefficient at a planar
    interface between two dielectrics

    Parameter ``cos_theta_i`` (float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (float):
        Relative refractive index of the interface. A value greater than
        1.0 means that the surface normal is pointing into the region of
        lower density.

    Returns → Tuple[float, float, float, float]:
        A tuple (F, cos_theta_t, eta_it, eta_ti) consisting of

    F Fresnel reflection coefficient.

    cos_theta_t Cosine of the angle between the surface normal and the
    transmitted ray

    eta_it Relative index of refraction in the direction of travel.

    eta_ti Reciprocal of the relative index of refraction in the direction
    of travel. This also happens to be equal to the scale factor that must
    be applied to the X and Y component of the refracted direction.

.. py:function:: mitsuba.render.fresnel_conductor(cos_theta_i, eta)

    Calculates the unpolarized Fresnel reflection coefficient at a planar
    interface of a conductor, i.e. a surface with a complex-valued
    relative index of refraction

    Remark:
        The implementation assumes that cos_theta_i > 0, i.e. light enters
        from *outside* of the conducting layer (generally a reasonable
        assumption unless very thin layers are being simulated)

    Parameter ``cos_theta_i`` (float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (enoki.scalar.Complex2f):
        Relative refractive index (complex-valued)

    Returns → float:
        The unpolarized Fresnel reflection coefficient.

.. py:function:: mitsuba.render.fresnel_polarized(cos_theta_i, eta)

    Calculates the polarized Fresnel reflection coefficient at a planar
    interface between two dielectrics or conductors. Returns complex
    values encoding the amplitude and phase shift of the s- and
    p-polarized waves.

    This is the most general version, which subsumes all others (at the
    cost of transcendental function evaluations in the complex-valued
    arithmetic)

    Parameter ``cos_theta_i`` (float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (enoki.scalar.Complex2f):
        Complex-valued relative refractive index of the interface. In the
        real case, a value greater than 1.0 case means that the surface
        normal points into the region of lower density.

    Returns → Tuple[enoki.scalar.Complex2f, enoki.scalar.Complex2f, float, enoki.scalar.Complex2f, enoki.scalar.Complex2f]:
        A tuple (a_s, a_p, cos_theta_t, eta_it, eta_ti) consisting of

    a_s Perpendicularly polarized wave amplitude and phase shift.

    a_p Parallel polarized wave amplitude and phase shift.

    cos_theta_t Cosine of the angle between the surface normal and the
    transmitted ray. Zero in the case of total internal reflection.

    eta_it Relative index of refraction in the direction of travel

    eta_ti Reciprocal of the relative index of refraction in the direction
    of travel. In the real-valued case, this also happens to be equal to
    the scale factor that must be applied to the X and Y component of the
    refracted direction.

.. py:function:: mitsuba.render.has_flag(overloaded)


    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (:py:obj:`mitsuba.render.HitComputeFlags`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.render.HitComputeFlags`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.render.BSDFFlags`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.render.PhaseFunctionFlags`):
            *no description available*

        Returns → bool:
            *no description available*

.. py:function:: mitsuba.render.mueller.absorber(overloaded)


    .. py:function:: absorber(value)

        Constructs the Mueller matrix of an ideal absorber

        Parameter ``value`` (float):
            The amount of absorption.

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: absorber(value)

        Constructs the Mueller matrix of an ideal absorber

        Parameter ``value`` (enoki.scalar.Vector3f):
            The amount of absorption.

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.depolarizer(overloaded)


    .. py:function:: depolarizer(value=1.0)

        Constructs the Mueller matrix of an ideal depolarizer

        Parameter ``value`` (float):
            The value of the (0, 0) element

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: depolarizer(value=1.0)

        Constructs the Mueller matrix of an ideal depolarizer

        Parameter ``value`` (enoki.scalar.Vector3f):
            The value of the (0, 0) element

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.diattenuator(overloaded)


    .. py:function:: diattenuator(x, y)

        Constructs the Mueller matrix of a linear diattenuator, which
        attenuates the electric field components at 0 and 90 degrees by 'x'
        and 'y', * respectively.

        Parameter ``x`` (float):
            *no description available*

        Parameter ``y`` (float):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: diattenuator(x, y)

        Constructs the Mueller matrix of a linear diattenuator, which
        attenuates the electric field components at 0 and 90 degrees by 'x'
        and 'y', * respectively.

        Parameter ``x`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``y`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.linear_polarizer(overloaded)


    .. py:function:: linear_polarizer(value=1.0)

        Constructs the Mueller matrix of a linear polarizer which transmits
        linear polarization at 0 degrees.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (13)

        Parameter ``value`` (float):
            The amount of attenuation of the transmitted component (1
            corresponds to an ideal polarizer).

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: linear_polarizer(value=1.0)

        Constructs the Mueller matrix of a linear polarizer which transmits
        linear polarization at 0 degrees.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (13)

        Parameter ``value`` (enoki.scalar.Vector3f):
            The amount of attenuation of the transmitted component (1
            corresponds to an ideal polarizer).

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.linear_retarder(overloaded)


    .. py:function:: linear_retarder(phase)

        Constructs the Mueller matrix of a linear retarder which has its fast
        aligned vertically.

        This implements the general case with arbitrary phase shift and can be
        used to construct the common special cases of quarter-wave and half-
        wave plates.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (27)

        Parameter ``phase`` (float):
            The phase difference between the fast and slow axis

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: linear_retarder(phase)

        Constructs the Mueller matrix of a linear retarder which has its fast
        aligned vertically.

        This implements the general case with arbitrary phase shift and can be
        used to construct the common special cases of quarter-wave and half-
        wave plates.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (27)

        Parameter ``phase`` (enoki.scalar.Vector3f):
            The phase difference between the fast and slow axis

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.reverse(overloaded)


    .. py:function:: reverse(M)

        Reverse direction of propagation of the electric field. Also used for
        reflecting reference frames.

        Parameter ``M`` (enoki.scalar.Matrix4f):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: reverse(M)

        Reverse direction of propagation of the electric field. Also used for
        reflecting reference frames.

        Parameter ``M`` (enoki::Matrix<:py:obj:`mitsuba.render.Color`):
            *no description available*

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.rotate_mueller_basis(overloaded)


    .. py:function:: rotate_mueller_basis(M, in_forward, in_basis_current, in_basis_target, out_forward, out_basis_current, out_basis_target)

        Return the Mueller matrix for some new reference frames. This version
        rotates the input/output frames independently.

        This operation is often used in polarized light transport when we have
        a known Mueller matrix 'M' that operates from 'in_basis_current' to
        'out_basis_current' but instead want to re-express it as a Mueller
        matrix that operates from 'in_basis_target' to 'out_basis_target'.

        Parameter ``M`` (enoki.scalar.Matrix4f):
            The current Mueller matrix that operates from ``in_basis_current``
            to ``out_basis_current``.

        Parameter ``in_forward`` (enoki.scalar.Vector3f):
            Direction of travel for input Stokes vector (normalized)

        Parameter ``in_basis_current`` (enoki.scalar.Vector3f):
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``in_forward``.

        Parameter ``in_basis_target`` (enoki.scalar.Vector3f):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``in_forward``.

        Parameter ``out_forward`` (enoki.scalar.Vector3f):
            Direction of travel for input Stokes vector (normalized)

        Parameter ``out_basis_current`` (enoki.scalar.Vector3f):
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Parameter ``out_basis_target`` (enoki.scalar.Vector3f):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Returns → enoki.scalar.Matrix4f:
            New Mueller matrix that operates from ``in_basis_target`` to
            ``out_basis_target``.

    .. py:function:: rotate_mueller_basis(M, in_forward, in_basis_current, in_basis_target, out_forward, out_basis_current, out_basis_target)

        Return the Mueller matrix for some new reference frames. This version
        rotates the input/output frames independently.

        This operation is often used in polarized light transport when we have
        a known Mueller matrix 'M' that operates from 'in_basis_current' to
        'out_basis_current' but instead want to re-express it as a Mueller
        matrix that operates from 'in_basis_target' to 'out_basis_target'.

        Parameter ``M`` (enoki::Matrix<:py:obj:`mitsuba.render.Color`):
            The current Mueller matrix that operates from ``in_basis_current``
            to ``out_basis_current``.

        Parameter ``in_forward`` (enoki.scalar.Vector3f):
            Direction of travel for input Stokes vector (normalized)

        Parameter ``in_basis_current`` (enoki.scalar.Vector3f):
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``in_forward``.

        Parameter ``in_basis_target`` (enoki.scalar.Vector3f):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``in_forward``.

        Parameter ``out_forward`` (enoki.scalar.Vector3f):
            Direction of travel for input Stokes vector (normalized)

        Parameter ``out_basis_current`` (enoki.scalar.Vector3f):
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Parameter ``out_basis_target`` (enoki.scalar.Vector3f):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            New Mueller matrix that operates from ``in_basis_target`` to
            ``out_basis_target``.

.. py:function:: mitsuba.render.mueller.rotate_mueller_basis_collinear(overloaded)


    .. py:function:: rotate_mueller_basis_collinear(M, forward, basis_current, basis_target)

        Return the Mueller matrix for some new reference frames. This version
        applies the same rotation to the input/output frames.

        This operation is often used in polarized light transport when we have
        a known Mueller matrix 'M' that operates from 'basis_current' to
        'basis_current' but instead want to re-express it as a Mueller matrix
        that operates from 'basis_target' to 'basis_target'.

        Parameter ``M`` (enoki.scalar.Matrix4f):
            The current Mueller matrix that operates from ``basis_current`` to
            ``basis_current``.

        Parameter ``forward`` (enoki.scalar.Vector3f):
            Direction of travel for input Stokes vector (normalized)

        Parameter ``basis_current`` (enoki.scalar.Vector3f):
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``forward``.

        Parameter ``basis_target`` (enoki.scalar.Vector3f):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``forward``.

        Returns → enoki.scalar.Matrix4f:
            New Mueller matrix that operates from ``basis_target`` to
            ``basis_target``.

    .. py:function:: rotate_mueller_basis_collinear(M, forward, basis_current, basis_target)

        Return the Mueller matrix for some new reference frames. This version
        applies the same rotation to the input/output frames.

        This operation is often used in polarized light transport when we have
        a known Mueller matrix 'M' that operates from 'basis_current' to
        'basis_current' but instead want to re-express it as a Mueller matrix
        that operates from 'basis_target' to 'basis_target'.

        Parameter ``M`` (enoki::Matrix<:py:obj:`mitsuba.render.Color`):
            The current Mueller matrix that operates from ``basis_current`` to
            ``basis_current``.

        Parameter ``forward`` (enoki.scalar.Vector3f):
            Direction of travel for input Stokes vector (normalized)

        Parameter ``basis_current`` (enoki.scalar.Vector3f):
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``forward``.

        Parameter ``basis_target`` (enoki.scalar.Vector3f):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``forward``.

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            New Mueller matrix that operates from ``basis_target`` to
            ``basis_target``.

.. py:function:: mitsuba.render.mueller.rotate_stokes_basis(wi, basis_current, basis_target)

    Gives the Mueller matrix that alignes the reference frames (defined by
    their respective basis vectors) of two collinear stokes vectors.

    If we have a stokes vector s_current expressed in 'basis_current', we
    can re-interpret it as a stokes vector rotate_stokes_basis(..) * s1
    that is expressed in 'basis_target' instead. For example: Horizontally
    polarized light [1,1,0,0] in a basis [1,0,0] can be interpreted as
    +45˚ linear polarized light [1,0,1,0] by switching to a target basis
    [0.707, -0.707, 0].

    Parameter ``forward``:
        Direction of travel for Stokes vector (normalized)

    Parameter ``basis_current`` (enoki.scalar.Vector3f):
        Current (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``basis_target`` (enoki.scalar.Vector3f):
        Target (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``wi`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Matrix4f:
        Mueller matrix that performs the desired change of reference
        frames.

.. py:function:: mitsuba.render.mueller.rotate_stokes_basis_m(wi, basis_current, basis_target)

    Gives the Mueller matrix that alignes the reference frames (defined by
    their respective basis vectors) of two collinear stokes vectors.

    If we have a stokes vector s_current expressed in 'basis_current', we
    can re-interpret it as a stokes vector rotate_stokes_basis(..) * s1
    that is expressed in 'basis_target' instead. For example: Horizontally
    polarized light [1,1,0,0] in a basis [1,0,0] can be interpreted as
    +45˚ linear polarized light [1,0,1,0] by switching to a target basis
    [0.707, -0.707, 0].

    Parameter ``forward``:
        Direction of travel for Stokes vector (normalized)

    Parameter ``basis_current`` (enoki.scalar.Vector3f):
        Current (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``basis_target`` (enoki.scalar.Vector3f):
        Target (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``wi`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
        Mueller matrix that performs the desired change of reference
        frames.

.. py:function:: mitsuba.render.mueller.rotated_element(overloaded)


    .. py:function:: rotated_element(theta, M)

        Applies a counter-clockwise rotation to the mueller matrix of a given
        element.

        Parameter ``theta`` (float):
            *no description available*

        Parameter ``M`` (enoki.scalar.Matrix4f):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: rotated_element(theta, M)

        Applies a counter-clockwise rotation to the mueller matrix of a given
        element.

        Parameter ``theta`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``M`` (enoki::Matrix<:py:obj:`mitsuba.render.Color`):
            *no description available*

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.rotator(overloaded)


    .. py:function:: rotator(theta)

        Constructs the Mueller matrix of an ideal rotator, which performs a
        counter-clockwise rotation of the electric field by 'theta' radians
        (when facing the light beam from the sensor side).

        To be more precise, it rotates the reference frame of the current
        Stokes vector. For example: horizontally linear polarized light s1 =
        [1,1,0,0] will look like -45˚ linear polarized light s2 = R(45˚) * s1
        = [1,0,-1,0] after applying a rotator of +45˚ to it.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (43)

        Parameter ``theta`` (float):
            *no description available*

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: rotator(theta)

        Constructs the Mueller matrix of an ideal rotator, which performs a
        counter-clockwise rotation of the electric field by 'theta' radians
        (when facing the light beam from the sensor side).

        To be more precise, it rotates the reference frame of the current
        Stokes vector. For example: horizontally linear polarized light s1 =
        [1,1,0,0] will look like -45˚ linear polarized light s2 = R(45˚) * s1
        = [1,0,-1,0] after applying a rotator of +45˚ to it.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (43)

        Parameter ``theta`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.specular_reflection(overloaded)


    .. py:function:: specular_reflection(cos_theta_i, eta)

        Calculates the Mueller matrix of a specular reflection at an interface
        between two dielectrics or conductors.

        Parameter ``cos_theta_i`` (float):
            Cosine of the angle between the surface normal and the incident
            ray

        Parameter ``eta`` (enoki.scalar.Complex2f):
            Complex-valued relative refractive index of the interface. In the
            real case, a value greater than 1.0 case means that the surface
            normal points into the region of lower density.

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: specular_reflection(cos_theta_i, eta)

        Calculates the Mueller matrix of a specular reflection at an interface
        between two dielectrics or conductors.

        Parameter ``cos_theta_i`` (enoki.scalar.Vector3f):
            Cosine of the angle between the surface normal and the incident
            ray

        Parameter ``eta`` (enoki::Complex<:py:obj:`mitsuba.render.Color`):
            Complex-valued relative refractive index of the interface. In the
            real case, a value greater than 1.0 case means that the surface
            normal points into the region of lower density.

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.specular_transmission(overloaded)


    .. py:function:: specular_transmission(cos_theta_i, eta)

        Calculates the Mueller matrix of a specular transmission at an
        interface between two dielectrics or conductors.

        Parameter ``cos_theta_i`` (float):
            Cosine of the angle between the surface normal and the incident
            ray

        Parameter ``eta`` (float):
            Complex-valued relative refractive index of the interface. A value
            greater than 1.0 in the real case means that the surface normal is
            pointing into the region of lower density.

        Returns → enoki.scalar.Matrix4f:
            *no description available*

    .. py:function:: specular_transmission(cos_theta_i, eta)

        Calculates the Mueller matrix of a specular transmission at an
        interface between two dielectrics or conductors.

        Parameter ``cos_theta_i`` (enoki.scalar.Vector3f):
            Cosine of the angle between the surface normal and the incident
            ray

        Parameter ``eta`` (enoki.scalar.Vector3f):
            Complex-valued relative refractive index of the interface. A value
            greater than 1.0 in the real case means that the surface normal is
            pointing into the region of lower density.

        Returns → enoki::Matrix<:py:obj:`mitsuba.render.Color`:
            *no description available*

.. py:function:: mitsuba.render.mueller.stokes_basis(w)

    Gives the reference frame basis for a Stokes vector.

    For light transport involving polarized quantities it is essential to
    keep track of reference frames. A Stokes vector is only meaningful if
    we also know w.r.t. which basis this state of light is observed. In
    Mitsuba, these reference frames are never explicitly stored but
    instead can be computed on the fly using this function.

    Parameter ``w`` (enoki.scalar.Vector3f):
        Direction of travel for Stokes vector (normalized)

    Returns → enoki.scalar.Vector3f:
        The (implicitly defined) reference coordinate system basis for the
        Stokes vector travelling along w.

.. py:function:: mitsuba.render.mueller.unit_angle(a, b)

    Parameter ``a`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``b`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.render.reflect(overloaded)


    .. py:function:: reflect(wi)

        Reflection in local coordinates

        Parameter ``wi`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:function:: reflect(wi, m)

        Reflect ``wi`` with respect to a given surface normal

        Parameter ``wi`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``m`` (enoki.scalar.Vector3f):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

.. py:function:: mitsuba.render.refract(overloaded)


    .. py:function:: refract(wi, cos_theta_t, eta_ti)

        Refraction in local coordinates

        The 'cos_theta_t' and 'eta_ti' parameters are given by the last two
        tuple entries returned by the fresnel and fresnel_polarized functions.

        Parameter ``wi`` (enoki.scalar.Vector3f):
            *no description available*

        Parameter ``cos_theta_t`` (float):
            *no description available*

        Parameter ``eta_ti`` (float):
            *no description available*

        Returns → enoki.scalar.Vector3f:
            *no description available*

    .. py:function:: refract(wi, m, cos_theta_t, eta_ti)

        Refract ``wi`` with respect to a given surface normal

        Parameter ``wi`` (enoki.scalar.Vector3f):
            Direction to refract

        Parameter ``m`` (enoki.scalar.Vector3f):
            Surface normal

        Parameter ``cos_theta_t`` (float):
            Cosine of the angle between the normal the transmitted ray, as
            computed e.g. by fresnel.

        Parameter ``eta_ti`` (float):
            Relative index of refraction (transmitted / incident)

        Returns → enoki.scalar.Vector3f:
            *no description available*

.. py:function:: mitsuba.render.register_bsdf(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.core.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render.register_emitter(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.core.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render.register_integrator(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.core.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render.register_medium(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.core.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render.register_phasefunction(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.core.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render.register_sensor(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.core.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render.srgb_model_eval(arg0, arg1)

    Parameter ``arg0`` (enoki.scalar.Vector3f):
        *no description available*

    Parameter ``arg1`` (enoki.scalar.Vector0f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.render.srgb_model_fetch(arg0)

    Look up the model coefficients for a sRGB color value @param c An sRGB
    color value where all components are in [0, 1]. @return Coefficients
    for use with srgb_model_eval

    Parameter ``arg0`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → enoki.scalar.Vector3f:
        *no description available*

.. py:function:: mitsuba.render.srgb_model_mean(arg0)

    Parameter ``arg0`` (enoki.scalar.Vector3f):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.python.chi2.BSDFAdapter(bsdf_type, extra, wi=[0, 0, 1], ctx=None)

    Adapter to test BSDF sampling using the Chi^2 test.

    Parameter ``bsdf_type`` (string):
        Name of the BSDF plugin to instantiate.

    Parameter ``extra`` (string):
        Additional XML used to specify the BSDF's parameters.

    Parameter ``wi`` (array(3,)):
        Incoming direction, in local coordinates.

.. py:class:: mitsuba.python.chi2.ChiSquareTest(domain, sample_func, pdf_func, sample_dim=2, sample_count=1000000, res=101, ires=4)

    Implements Pearson's chi-square test for goodness of fit of a distribution
    to a known reference distribution.

    The implementation here specifically compares a Monte Carlo sampling
    strategy on a 2D (or lower dimensional) space against a reference
    distribution obtained by numerically integrating a probability density
    function over grid in the distribution's parameter domain.

    Parameter ``domain`` (object):
       An implementation of the domain interface (``SphericalDomain``, etc.),
       which transforms between the parameter and target domain of the
       distribution

    Parameter ``sample_func`` (function):
       An importance sampling function which maps an array of uniform variates
       of size ``[sample_dim, sample_count]`` to an array of ``sample_count``
       samples on the target domain.

    Parameter ``pdf_func`` (function):
       Function that is expected to specify the probability density of the
       samples produced by ``sample_func``. The test will try to collect
       sufficient statistical evidence to reject this hypothesis.

    Parameter ``sample_dim`` (int):
       Number of random dimensions consumed by ``sample_func`` per sample. The
       default value is ``2``.

    Parameter ``sample_count`` (int):
       Total number of samples to be generated. The test will have more
       evidence as this number tends to infinity. The default value is
       ``1000000``.

    Parameter ``res`` (int):
       Vertical resolution of the generated histograms. The horizontal
       resolution will be calculated as ``res * domain.aspect()``. The
       default value of ``101`` is intentionally an odd number to prevent
       issues with floating point precision at sharp boundaries that may
       separate the domain into two parts (e.g. top hemisphere of a sphere
       parameterization).

    Parameter ``ires`` (int):
       Number of horizontal/vertical subintervals used to numerically integrate
       the probability density over each histogram cell (using the trapezoid
       rule). The default value is ``4``.

    Notes:

    The following attributes are part of the public API:

    messages: string
        The implementation may generate a number of messages while running the
        test, which can be retrieved via this attribute.

    histogram: array
        The histogram array is populated by the ``tabulate_histogram()`` method
        and stored in this attribute.

    pdf: array
        The probability density function array is populated by the
        ``tabulate_pdf()`` method and stored in this attribute.

    p_value: float
        The p-value of the test is computed in the ``run()`` method and stored
        in this attribute.

    .. py:method:: mitsuba.python.chi2.ChiSquareTest.tabulate_histogram()

        Invoke the provided sampling strategy many times and generate a
        histogram in the parameter domain. If ``sample_func`` returns a tuple
        ``(positions, weights)`` instead of just positions, the samples are
        considered to be weighted.

    .. py:method:: mitsuba.python.chi2.ChiSquareTest.tabulate_pdf()

        Numerically integrate the provided probability density function over
        each cell to generate an array resembling the histogram computed by
        ``tabulate_histogram()``. The function uses the trapezoid rule over
        intervals discretized into ``self.ires`` separate function evaluations.

    .. py:method:: mitsuba.python.chi2.ChiSquareTest.run(significance_level=0.01, test_count=1, quiet=False)

        Run the Chi^2 test

        Parameter ``significance_level`` (float):
            Denotes the desired significance level (e.g. 0.01 for a test at the
            1% significance level)

        Parameter ``test_count`` (int):
            Specifies the total number of statistical tests run by the user.
            This value will be used to adjust the provided significance level
            so that the combination of the entire set of tests has the provided
            significance level.

        Returns → bool:
            ``True`` upon success, ``False`` if the null hypothesis was
            rejected.


.. py:class:: mitsuba.python.chi2.LineDomain(bounds=[-1.0, 1.0])

    The identity map on the line.

.. py:function:: mitsuba.python.chi2.MicrofacetAdapter(md_type, alpha, sample_visible=False)

    Adapter for testing microfacet distribution sampling techniques
    (separately from BSDF models, which are also tested)

.. py:function:: mitsuba.python.chi2.PhaseFunctionAdapter(phase_type, extra, wi=[0, 0, 1])

    Adapter to test phase function sampling using the Chi^2 test.

    Parameter ``phase_type`` (string):
        Name of the phase function plugin to instantiate.

    Parameter ``extra`` (string):
        Additional XML used to specify the phase function's parameters.

    Parameter ``wi`` (array(3,)):
        Incoming direction, in local coordinates.

.. py:class:: mitsuba.python.chi2.PlanarDomain(bounds=None)

    The identity map on the plane

.. py:function:: mitsuba.python.chi2.SpectrumAdapter(value)

    Adapter which permits testing 1D spectral power distributions using the
    Chi^2 test.

.. py:class:: mitsuba.python.chi2.SphericalDomain

    Maps between the unit sphere and a [cos(theta), phi] parameterization.

.. py:class:: mitsuba.python.util.ParameterMap(properties, hierarchy)

    Dictionary-like object that references various parameters used in a Mitsuba
    scene graph. Parameters can be read and written using standard syntax
    (``parameter_map[key]``). The class exposes several non-standard functions,
    specifically :py:meth:`~:py:obj:`mitsuba.python.util.ParameterMap.torch`()`,
    :py:meth:`~:py:obj:`mitsuba.python.util.ParameterMap.update`()`, and
    :py:meth:`~:py:obj:`mitsuba.python.util.ParameterMap.keep`()`.

    .. py:method:: __init__(properties, hierarchy)

        Private constructor (use
        :py:func:`mitsuba.python.util.traverse()` instead)

        
    .. py:method:: mitsuba.python.util.ParameterMap.torch() -> dict

        Converts all Enoki arrays into PyTorch arrays and return them as a
        dictionary. This is mainly useful when using PyTorch to optimize a
        Mitsuba scene.

    .. py:method:: mitsuba.python.util.ParameterMap.set_dirty(key: str)

        Marks a specific parameter and its parent objects as dirty. A subsequent call
        to :py:meth:`~:py:obj:`mitsuba.python.util.ParameterMap.update`()` will refresh their internal
        state. This function is automatically called when overwriting a parameter using
        :py:meth:`~:py:obj:`mitsuba.python.util.ParameterMap.__setitem__`()`.

    .. py:method:: mitsuba.python.util.ParameterMap.update() -> None

        This function should be called at the end of a sequence of writes
        to the dictionary. It automatically notifies all modified Mitsuba
        objects and their parent objects that they should refresh their
        internal state. For instance, the scene may rebuild the kd-tree
        when a shape was modified, etc.

    .. py:method:: mitsuba.python.util.ParameterMap.keep(keys: list) -> None

        Reduce the size of the dictionary by only keeping elements,
        whose keys are part of the provided list 'keys'.

.. py:function:: mitsuba.python.util.is_differentiable(p)

.. py:function:: mitsuba.python.util.traverse(node: mitsuba.core.Object) -> mitsuba.python.util.ParameterMap

    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.python.util.ParameterMap`.

.. py:function:: mitsuba.python.math.rlgamma(a, x)

    Regularized lower incomplete gamma function based on CEPHES

.. py:class:: mitsuba.python.autodiff.Adam(params, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-08)

    Base class: :py:obj:`mitsuba.python.autodiff.Optimizer`

    Implements the Adam optimizer presented in the paper *Adam: A Method for
    Stochastic Optimization* by Kingman and Ba, ICLR 2015.

    .. py:method:: __init__(params, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-08)

        Parameter ``lr``:
            learning rate
        
        Parameter ``beta_1``:
            controls the exponential averaging of first
            order gradient moments
        
        Parameter ``beta_2``:
            controls the exponential averaging of second
            order gradient moments

        
    .. py:method:: mitsuba.python.autodiff.Adam.step()

        Take a gradient step 

.. py:class:: mitsuba.python.autodiff.Optimizer(params, lr)

    Base class of all gradient-based optimizers (currently SGD and Adam)

    .. py:method:: __init__(params, lr)

        Parameter ``params``:
            dictionary ``(name: variable)`` of differentiable parameters to be
            optimized.
        
        Parameter ``lr``:
            learning rate

        
    .. py:method:: mitsuba.python.autodiff.Optimizer.set_learning_rate(lr)

        Set the learning rate.

    .. py:method:: mitsuba.python.autodiff.Optimizer.disable_gradients()

        Temporarily disable the generation of gradients.

.. py:class:: mitsuba.python.autodiff.SGD(params, lr, momentum=0)

    Base class: :py:obj:`mitsuba.python.autodiff.Optimizer`

    Implements basic stochastic gradient descent with a fixed learning rate
    and, optionally, momentum :cite:`Sutskever2013Importance` (0.9 is a typical
    parameter value for the ``momentum`` parameter).

    The momentum-based SGD uses the update equation

    .. math::

        v_{i+1} = \mu \cdot v_i +  g_{i+1}

    .. math::
        p_{i+1} = p_i + \varepsilon \cdot v_{i+1},

    where :math:`v` is the velocity, :math:`p` are the positions,
    :math:`\varepsilon` is the learning rate, and :math:`\mu` is
    the momentum parameter.

    .. py:method:: __init__(params, lr, momentum=0)

        Parameter ``lr``:
            learning rate
        
        Parameter ``momentum``:
            momentum factor

        
    .. py:method:: mitsuba.python.autodiff.SGD.step()

        Take a gradient step 

.. py:function:: mitsuba.python.autodiff._render_helper(scene, spp=None, sensor_index=0)

    Internally used function: render the specified Mitsuba scene and return a
    floating point array containing RGB values and AOVs, if applicable

.. py:function:: mitsuba.python.autodiff.render(scene, spp: Union[None, int, Tuple[int, int]] = None, unbiased=False, optimizer: mitsuba.python.autodiff.Optimizer = None, sensor_index=0)

    Perform a differentiable of the scene `scene`, returning a floating point
    array containing RGB values and AOVs, if applicable.

    Parameter ``spp`` (``None``, ``int``, or a 2-tuple ``(int, int)``):
       Specifies the number of samples per pixel to be used for rendering,
       overriding the value that is specified in the scene. If ``spp=None``,
       the original value takes precedence. If ``spp`` is a 2-tuple
       ``(spp_primal: int, spp_deriv: int)``, the first element specifies the
       number of samples for the *primal* pass, and the second specifies the
       number of samples for the *derivative* pass. See the explanation of the
       ``unbiased`` parameter for further detail on what these mean.

       Memory usage is roughly proportional to the ``spp``, value, hence this
       parameter should be reduced if you encounter out-of-memory errors.

    Parameter ``unbiased`` (``bool``):
        One potential issue when naively differentiating a rendering algorithm
        is that the same set of Monte Carlo sample is used to generate both the
        primal output (i.e. the image) along with derivative output. When the
        rendering algorithm and objective are jointly differentiated, we end up
        with expectations of products that do *not* satisfy the equality
        :math:`\mathbb{E}[X Y]=\mathbb{E}[X]\, \mathbb{E}[Y]` due to
        correlations between :math:`X` and :math:`Y` that result from this
        sample re-use.

        When ``unbiased=True``, the ``render()`` function will generate an
        *unbiased* estimate that de-correlates primal and derivative
        components, which boils down to rendering the image twice and naturally
        comes at some cost in performance :math:`(\sim 1.6      imes\!)`. Often,
        biased gradients are good enough, in which case ``unbiased=False``
        should be specified instead.

        The number of samples per pixel per pass can be specified separately
        for both passes by passing a tuple to the ``spp`` parameter.

        Note that unbiased mode is only relevant for reverse-mode
        differentiation. It is not needed when visualizing parameter gradients
        in image space using forward-mode differentiation.

    Parameter ``optimizer`` (:py:class:`mitsuba.python.autodiff.Optimizer`):
        The optimizer referencing relevant scene parameters must be specified
        when ``unbiased=True``. Otherwise, there is no need to provide this
        parameter.

    Parameter ``sensor_index`` (``int``):
        When the scene contains more than one sensor/camera, this parameter
        can be specified to select the desired sensor.

.. py:function:: mitsuba.python.autodiff.render_torch(scene, params=None, **kwargs)

.. py:function:: mitsuba.python.autodiff.write_bitmap(filename, data, resolution, write_async=True)

    Write the linearized RGB image in `data` to a PNG/EXR/.. file with
    resolution `resolution`.

.. py:class:: mitsuba.python.xml.Files

    Enum for different files or dicts containing specific info

.. py:class:: mitsuba.python.xml.WriteXML(path, split_files=False)

    File Writing API
    Populates a dictionary with scene data, then writes it to XML.

    .. py:method:: mitsuba.python.xml.WriteXML.data_add(key, value, file=0)

        Add an entry to a given subdict.

        Params
        ------

        key: dict key
        value: entry
        file: the subdict to which to add the data

    .. py:method:: mitsuba.python.xml.WriteXML.add_comment(comment, file=0)

        Add a comment to the scene dict

        Params
        ------

        comment: text of the comment
        file: the subdict to which to add the comment

    .. py:method:: mitsuba.python.xml.WriteXML.add_include(file)

        Add an include tag to the main file.
        This is used when splitting the XML scene file in multiple fragments.

        Params
        ------

        file: the file to include

    .. py:method:: mitsuba.python.xml.WriteXML.wf(ind, st, tabs=0)

        Write a string to file index ind.
        Optionally indent the string by a number of tabs

        Params
        ------

        ind: index of the file to write to
        st: text to write
        tabs: optional number of tabs to add

    .. py:method:: mitsuba.python.xml.WriteXML.set_filename(name)

        Open the files for output,
        using filenames based on the given base name.
        Create the necessary folders to create the file at the specified path.

        Params
        ------

        name: path to the scene.xml file to write.

    .. py:method:: mitsuba.python.xml.WriteXML.set_output_file(file)

        Switch next output to the given file index

        Params
        ------

        file: index of the file to start writing to

    .. py:method:: mitsuba.python.xml.WriteXML.write_comment(comment, file=None)

        Write an XML comment to file.

        Params
        ------

        comment: The text of the comment to write
        file: Index of the file to write to

    .. py:method:: mitsuba.python.xml.WriteXML.write_header(file, comment=None)

        Write an XML header to a specified file.
        Optionally add a comment to describe the file.

        Params
        ------

        file: The file to write to
        comment: Optional comment to add (e.g. "# Geometry file")

    .. py:method:: mitsuba.python.xml.WriteXML.open_element(name, attributes={}, file=None)

        Open an XML tag (e.g. emitter, bsdf...)

        Params
        ------

        name: Name of the tag (emitter, bsdf, shape...)
        attributes: Additional fileds to add to the opening tag (e.g. name, type...)
        file: File to write to

    .. py:method:: mitsuba.python.xml.WriteXML.close_element(file=None)

        Close the last tag we opened in a given file.

        Params
        ------

        file: The file to write to

    .. py:method:: mitsuba.python.xml.WriteXML.element(name, attributes={}, file=None)

        Write a single-line XML element.

        Params
        ------

        name: Name of the element (e.g. integer, string, rotate...)
        attributes: Additional fields to add to the element (e.g. name, value...)
        file: The file to write to

    .. py:method:: mitsuba.python.xml.WriteXML.get_plugin_tag(plugin_type)

        Get the corresponding tag of a given plugin (e.g. 'bsdf' for 'diffuse')
        If the given type (e.g. 'transform') is not a plugin, returns None.

        Params
        ------

        plugin_type: Name of the type (e.g. 'diffuse', 'ply'...)

    .. py:method:: mitsuba.python.xml.WriteXML.current_tag()

        Get the tag in which we are currently writing

    .. py:method:: mitsuba.python.xml.WriteXML.configure_defaults(scene_dict)

        Traverse the scene graph and look for properties in the defaults dict.
        For such properties, store their value in a default tag and replace the value by $name in the prop.

        Params
        ------

        scene_dict: The dictionary containing the scene info

    .. py:method:: mitsuba.python.xml.WriteXML.preprocess_scene(scene_dict)

        Preprocess the scene dictionary before writing it to file:
            - Add default properties.
            - Reorder the scene dict before writing it to file.
            - Separate the dict into different category-specific subdicts.
            - If not splitting files, merge them in the end.

        Params
        ------

        scene_dict: The dictionary containing the scene data

    .. py:method:: mitsuba.python.xml.WriteXML.format_spectrum(entry, entry_type)

        Format rgb or spectrum tags to the proper XML output.
        The entry should contain the name and value of the spectrum entry.
        The type is passed separately, since it is popped from the dict in write_dict

        Params
        ------

        entry: the dict containing the spectrum
        entry_type: either 'spectrum' or 'rgb'

    .. py:method:: mitsuba.python.xml.WriteXML.format_path(filepath, tag)

        Given a filepath, either copy it in the scene folder (in the corresponding directory)
        or convert it to a relative path.

        Params
        ------

        filepath: the path to the given file
        tag: the tag this path property belongs to in (shape, texture, spectrum)

    .. py:method:: mitsuba.python.xml.WriteXML.write_dict(data)

        Main XML writing routine.
        Given a dictionary, iterate over its entries and write them to file.
        Calls itself for nested dictionaries.

        Params
        ------

        data: The dictionary to write to file.

    .. py:method:: mitsuba.python.xml.WriteXML.process(scene_dict)

        Preprocess then write the input dict to XML file format

        Params
        ------

        scene_dict: The dictionary containing all the scene info.

    .. py:method:: mitsuba.python.xml.WriteXML.transform_matrix(transform)

        Converts a mitsuba Transform4f into a dict entry.
        This dict entry won't have a 'type' because it's handled in a specific case.

        Params
        ------

        transform: the given transform matrix

    .. py:method:: mitsuba.python.xml.WriteXML.decompose_transform(transform, export_scale=False)

        Export a transform as a combination of rotation, scale and translation.
        This helps manually modifying the transform after export (for cameras for instance)

        Params
        ------

        transform: The Transform4f transform matrix to decompose
        export_scale: Whether to add a scale property or not. (e.g. don't do it for cameras to avoid clutter)

.. py:function:: mitsuba.python.xml.copy2(src, dst, *, follow_symlinks=True)

    Copy data and metadata. Return the file's destination.

    Metadata is copied with copystat(). Please see the copystat function
    for more information.

    The destination may be a directory.

    If follow_symlinks is false, symlinks won't be followed. This
    resembles GNU's "cp -P src dst".


.. py:function:: mitsuba.python.xml.dict_to_xml(scene_dict, filename, split_files=False)

.. py:function:: mitsuba.python.test.util.fresolver_append_path(func)

    Function decorator that adds the mitsuba project root
    to the FileResolver's search path. This is useful in particular
    for tests that e.g. load scenes, and need to specify paths to resources.

    The file resolver is restored to its previous state once the test's
    execution has finished.

.. py:function:: mitsuba.python.test.util.getframeinfo(frame, context=1)

    Get information about a frame or traceback object.

    A tuple of five things is returned: the filename, the line number of
    the current line, the function name, a list of lines of context from
    the source code, and the index of the current line within that list.
    The optional second argument specifies the number of lines of context
    to return, which are centered around the current line.

.. py:function:: mitsuba.python.test.util.make_tmpfile(request, tmpdir_factory)

.. py:function:: mitsuba.python.test.util.stack(context=1)

    Return a list of records for the stack above the caller's frame.

.. py:function:: mitsuba.python.test.util.tmpfile(request, tmpdir_factory)

    Fixture to create a temporary file

.. py:function:: mitsuba.python.test.util.wraps(wrapped, assigned=('__module__', '__name__', '__qualname__', '__doc__', '__annotations__'), updated=('__dict__',))

    Decorator factory to apply update_wrapper() to a wrapper function

    Returns a decorator that invokes update_wrapper() with the decorated
    function as the wrapper argument and the arguments to wraps() as the
    remaining arguments. Default arguments are as for update_wrapper().
    This is a convenience function to simplify applying partial() to
    update_wrapper().

