/*
  This file contains docstrings for the Python bindings.
  Do not edit! These were automatically extracted by mkdoc.py
 */

#define __EXPAND(x)                                      x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define __VA_SIZE(...)                                   __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1))
#define __CAT1(a, b)                                     a ## b
#define __CAT2(a, b)                                     __CAT1(a, b)
#define __DOC1(n1)                                       __doc_##n1
#define __DOC2(n1, n2)                                   __doc_##n1##_##n2
#define __DOC3(n1, n2, n3)                               __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4)                           __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5)                       __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6)                   __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7)               __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                         __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *__doc_mitsuba_AnnotatedStream =
R"doc(An AnnotatedStream adds a table of contents to an underlying stream
that can later be used to selectively read specific items.

A Stream instance must first be created and passed to to the
constructor. The underlying stream should either be empty or a stream
that was previously written with an AnnotatedStream, so that it
contains a proper Table of Contents.

Table of Contents: objects and variables written to the stream are
prepended by a field name. Contents can then be queried by field name,
as if using a map. A hierarchy can be created by ``push``ing and
``pop``ing prefixes. The root of this hierarchy is the empty prefix
"".

The table of contents is automatically read from the underlying stream
on creation and written back on destruction.)doc";

static const char *__doc_mitsuba_AnnotatedStream_AnnotatedStream =
R"doc(Creates an AnnotatedStream based on the given Stream (decorator
pattern). Anything written to the AnnotatedStream is ultimately passed
down to the given Stream instance. The given Stream instance should
not be destructed before this.

Throws if ``write_mode`` is enabled (resp. disabled) but the
underlying stream does not have write (resp. read) capabilities.

Throws if the underlying stream has read capabilities and is not empty
but does not correspond to a valid AnnotatedStream (i.e. it does not
start with the kSerializedHeaderId sentry).

Parameter ``write_mode``:
    Whether to use write mode. The stream is either read-only or
    write-only.

Parameter ``throw_on_missing``:
    Whether an error should be thrown when get is called for a missing
    field.)doc";

static const char *__doc_mitsuba_AnnotatedStream_can_read = R"doc(Whether the underlying stream has read capabilities and is not closed.)doc";

static const char *__doc_mitsuba_AnnotatedStream_can_write =
R"doc(Whether the underlying stream has write capabilities and is not
closed.)doc";

static const char *__doc_mitsuba_AnnotatedStream_class = R"doc()doc";

static const char *__doc_mitsuba_AnnotatedStream_close =
R"doc(Closes the annotated stream. No further read or write operations are
permitted.

\note The underlying stream is not automatically closed by this
function. It may, however, call its own ``close`` function in its
destructor.

This function is idempotent and causes the ToC to be written out to
the stream. It is called automatically by the destructor.)doc";

static const char *__doc_mitsuba_AnnotatedStream_compatibilityMode = R"doc(Whether the stream won't throw when trying to get missing fields.)doc";

static const char *__doc_mitsuba_AnnotatedStream_get =
R"doc(Retrieve a field from the serialized file (only valid in read mode)

Throws if the field exists but has the wrong type. Throws if the field
is not found and ``throw_on_missing`` is true.)doc";

static const char *__doc_mitsuba_AnnotatedStream_get_base =
R"doc(Attempts to seek to the position of the given field. The active prefix
(from previous push operations) is prepended to the given ``name``.

Throws if the field exists but has the wrong type. Throws if the field
is not found and ``m_throw_on_missing`` is true.)doc";

static const char *__doc_mitsuba_AnnotatedStream_is_closed =
R"doc(Whether the annotated stream has been closed (no further read or
writes permitted))doc";

static const char *__doc_mitsuba_AnnotatedStream_keys =
R"doc(Return all field names under the current name prefix. Nested names are
returned with the full path prepended, e.g.: level_1.level_2.my_name)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_is_closed =
R"doc(Whether the annotated stream is closed (independent of the underlying
stream).)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_prefix_stack =
R"doc(Stack of accumulated prefixes, i.e. ``m_prefix_stack.back`` is the
full prefix path currently applied.)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_stream = R"doc(Underlying stream where the names and contents are written)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_table =
R"doc(Maintains the mapping: full field name -> (type, position in the
stream))doc";

static const char *__doc_mitsuba_AnnotatedStream_m_throw_on_missing = R"doc()doc";

static const char *__doc_mitsuba_AnnotatedStream_m_write_mode = R"doc()doc";

static const char *__doc_mitsuba_AnnotatedStream_pop = R"doc(Pop a name prefix from the stack)doc";

static const char *__doc_mitsuba_AnnotatedStream_push =
R"doc(Push a name prefix onto the stack (use this to isolate identically-
named data fields).)doc";

static const char *__doc_mitsuba_AnnotatedStream_read_toc =
R"doc(Read back the table of contents from the underlying stream and update
the in-memory ``m_table`` accordingly. Should be called on
construction.

Throws if the underlying stream does not have read capabilities.
Throws if the underlying stream does not have start with the
AnnotatedStream sentry (kSerializedHeaderId).)doc";

static const char *__doc_mitsuba_AnnotatedStream_set = R"doc(Store a field in the serialized file (only valid in write mode))doc";

static const char *__doc_mitsuba_AnnotatedStream_set_base =
R"doc(Attempts to associate the current position of the stream to the given
field. The active prefix (from previous push operations) is prepended
to the ``name`` of the field.

Throws if a value was already set with that name (including prefix).)doc";

static const char *__doc_mitsuba_AnnotatedStream_size = R"doc(Returns the current size of the underlying stream)doc";

static const char *__doc_mitsuba_AnnotatedStream_write_toc =
R"doc(Write back the table of contents to the underlying stream. Should be
called on destruction.)doc";

static const char *__doc_mitsuba_Appender =
R"doc(This class defines an abstract destination for logging-relevant
information)doc";

static const char *__doc_mitsuba_Appender_append = R"doc(Append a line of text with the given log level)doc";

static const char *__doc_mitsuba_Appender_class = R"doc()doc";

static const char *__doc_mitsuba_Appender_log_progress =
R"doc(Process a progress message

Parameter ``progress``:
    Percentage value in [0, 100]

Parameter ``name``:
    Title of the progress message

Parameter ``formatted``:
    Formatted string representation of the message

Parameter ``eta``:
    Estimated time until 100% is reached.

Parameter ``ptr``:
    Custom pointer payload. This is used to express the context of a
    progress message. When rendering a scene, it will usually contain
    a pointer to the associated ``RenderJob``.)doc";

static const char *__doc_mitsuba_ArgParser =
R"doc(Minimal command line argument parser

This class provides a minimal cross-platform command line argument
parser in the spirit of to GNU getopt. Both short and long arguments
that accept an optional extra value are supported.

The typical usage is

```
ArgParser p;
auto arg0 = p.register("--myParameter");
auto arg1 = p.register("-f", true);
p.parse(argc, argv);
if (*arg0)
    std::cout << "Got --myParameter" << std::endl;
if (*arg1)
    std::cout << "Got -f " << arg1->value() << std::endl;
```)doc";

static const char *__doc_mitsuba_ArgParser_Arg = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_Arg =
R"doc(Construct a new argument with the given prefixes

Parameter ``prefixes``:
    A list of command prefixes (i.e. {"-f", "--fast"})

Parameter ``extra``:
    Indicates whether the argument accepts an extra argument value)doc";

static const char *__doc_mitsuba_ArgParser_Arg_Arg_2 = R"doc(Copy constructor (does not copy argument values))doc";

static const char *__doc_mitsuba_ArgParser_Arg_append = R"doc(Append a argument value at the end)doc";

static const char *__doc_mitsuba_ArgParser_Arg_as_float = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_as_int = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_as_string = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_count = R"doc(Specifies how many times the argument has been specified)doc";

static const char *__doc_mitsuba_ArgParser_Arg_extra = R"doc(Specifies whether the argument takes an extra value)doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_extra = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_next = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_prefixes = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_present = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_value = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_next =
R"doc(For arguments that are specified multiple times, advance to the next
one.)doc";

static const char *__doc_mitsuba_ArgParser_Arg_operator_bool = R"doc(Returns whether the argument has been specified)doc";

static const char *__doc_mitsuba_ArgParser_add =
R"doc(Register a new argument with the given prefix

Parameter ``prefix``:
    A single command prefix (i.e. "-f")

Parameter ``extra``:
    Indicates whether the argument accepts an extra argument value)doc";

static const char *__doc_mitsuba_ArgParser_add_2 =
R"doc(Register a new argument with the given list of prefixes

Parameter ``prefixes``:
    A list of command prefixes (i.e. {"-f", "--fast"})

Parameter ``extra``:
    Indicates whether the argument accepts an extra argument value)doc";

static const char *__doc_mitsuba_ArgParser_executable_name = R"doc(Return the name of the invoked application executable)doc";

static const char *__doc_mitsuba_ArgParser_m_args = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_m_executable_name = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_parse = R"doc(Parse the given set of command line arguments)doc";

static const char *__doc_mitsuba_ArgParser_parse_2 = R"doc(Parse the given set of command line arguments)doc";

static const char *__doc_mitsuba_AtomicFloat =
R"doc(Atomic floating point data type

The class implements an an atomic floating point data type (which is
not possible with the existing overloads provided by ``std::atomic``).
It internally casts floating point values to an integer storage format
and uses atomic integer compare and exchange operations to perform
changes.)doc";

static const char *__doc_mitsuba_AtomicFloat_AtomicFloat = R"doc(Initialize the AtomicFloat with a given floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_do_atomic =
R"doc(Apply a FP operation atomically (verified that this will be nicely
inlined in the above operators))doc";

static const char *__doc_mitsuba_AtomicFloat_m_bits = R"doc()doc";

static const char *__doc_mitsuba_AtomicFloat_operator_T0 = R"doc(Convert the AtomicFloat into a normal floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_assign = R"doc(Overwrite the AtomicFloat with a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_iadd = R"doc(Atomically add a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_idiv = R"doc(Atomically divide by a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_imul = R"doc(Atomically multiply by a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_isub = R"doc(Atomically subtract a floating point value)doc";

static const char *__doc_mitsuba_BSDF = R"doc()doc";

static const char *__doc_mitsuba_BSDF_class = R"doc()doc";

static const char *__doc_mitsuba_BSDF_dummy = R"doc()doc";

static const char *__doc_mitsuba_Bitmap =
R"doc(General-purpose bitmap class with read and write support for several
common file formats.

This class handles loading of PNG, JPEG, BMP, TGA, as well as OpenEXR
files, and it supports writing of PNG, JPEG and OpenEXR files.

PNG and OpenEXR files are optionally annotated with string-valued
metadata, and the gamma setting can be stored as well. Please see the
class methods and enumerations for further detail.)doc";

static const char *__doc_mitsuba_Bitmap_Bitmap =
R"doc(Create a bitmap of the specified type and allocate the necessary
amount of memory

Parameter ``pfmt``:
    Specifies the pixel format (e.g. RGBA or Luminance-only)

Parameter ``cfmt``:
    Specifies how the per-pixel components are encoded (e.g. unsigned
    8 bit integers or 32-bit floating point values)

Parameter ``size``:
    Specifies the horizontal and vertical bitmap size in pixels

Parameter ``channel_count``:
    Channel count of the image. This parameter is only required when
    ``pfmt`` = EMultiChannel

Parameter ``data``:
    External pointer to the image data. If set to ``nullptr``, the
    implementation will allocate memory itself.)doc";

static const char *__doc_mitsuba_Bitmap_Bitmap_2 =
R"doc(Load a bitmap from an arbitrary stream data source

Parameter ``stream``:
    Pointer to an arbitrary stream data source

Parameter ``format``:
    File format to be read (PNG/EXR/Auto-detect ...))doc";

static const char *__doc_mitsuba_Bitmap_Bitmap_3 = R"doc(Copy constructor (copies the image contents))doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat = R"doc(Supported file formats)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EAuto =
R"doc(Automatically detect the file format

Note: this flag only applies when loading a file. In this case, the
source stream must support the ``seek``() operation.)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EBMP =
R"doc(Windows Bitmap file format

The following is supported:

* Loading of uncompressed 8-bit luminance and RGBA bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EJPEG =
R"doc(Joint Photographic Experts Group file format

The following is supported:

* Loading and saving of 8 bit per component RGB and luminance bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EOpenEXR =
R"doc(OpenEXR high dynamic range file format developed by Industrial Light &
Magic (ILM)

The following is supported:

* Loading and saving of Eloat16 / EFloat32/ EUInt32 bitmaps with all
supported RGB/Luminance/Alpha combinations

* Loading and saving of spectral bitmaps</tt>

* Loading and saving of XYZ tristimulus bitmaps</tt>

* Loading and saving of string-valued metadata fields

The following is *not* supported:

* Saving of tiled images, tile-based read access

* Display windows that are different than the data window

* Loading of spectrum-valued bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EPFM =
R"doc(PFM (Portable Float Map) image format

The following is supported

* Loading and saving of EFloat32 - based Luminance or RGB bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EPNG =
R"doc(Portable network graphics

The following is supported:

* Loading and saving of 8/16-bit per component bitmaps for all pixel
formats (ELuminance, ELuminanceAlpha, ERGB, ERGBA)

* Loading and saving of 1-bit per component mask bitmaps

* Loading and saving of string-valued metadata fields)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_EPPM =
R"doc(PPM (Portable Pixel Map) image format

The following is supported

* Loading and saving of EUInt8 and EUInt16 - based RGB bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_ERGBE =
R"doc(RGBE image format by Greg Ward

The following is supported

* Loading and saving of EFloat32 - based RGB bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_EFileFormat_ETGA =
R"doc(Truevision Advanced Raster Graphics Array file format

The following is supported:

* Loading of uncompressed 8-bit RGB/RGBA files)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat =
R"doc(This enumeration lists all pixel format types supported by the Bitmap
class. This both determines the number of channels, and how they
should be interpreted)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_ELuminance = R"doc(Single-channel luminance bitmap)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_ELuminanceAlpha = R"doc(Two-channel luminance + alpha bitmap)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_EMultiChannel = R"doc(Arbitrary multi-channel bitmap without a fixed interpretation)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_ERGB = R"doc(RGB bitmap)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_ERGBA = R"doc(RGB bitmap + alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_EXYZ = R"doc(XYZ tristimulus bitmap)doc";

static const char *__doc_mitsuba_Bitmap_EPixelFormat_EXYZA = R"doc(XYZ tristimulus + alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_Layer =
R"doc(Describes a sub-layer of a multilayer bitmap (e.g. OpenEXR)

A layer is defined as a named collection of bitmap channels along with
a pixel format. This data structure is used by Bitmap::getLayers().)doc";

static const char *__doc_mitsuba_Bitmap_Layer_name = R"doc(Descriptive name of the bitmap layer)doc";

static const char *__doc_mitsuba_Bitmap_Layer_pixel_format = R"doc(Pixel format of the layer)doc";

static const char *__doc_mitsuba_Bitmap_Layer_struct = R"doc(Data structure listing channels and component formats)doc";

static const char *__doc_mitsuba_Bitmap_buffer_size = R"doc(Return the bitmap size in bytes (excluding metadata))doc";

static const char *__doc_mitsuba_Bitmap_bytes_per_pixel = R"doc(Return the number bytes of storage used per pixel)doc";

static const char *__doc_mitsuba_Bitmap_channel_count = R"doc(Return the number of channels used by this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_class = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_clear = R"doc(Clear the bitmap to zero)doc";

static const char *__doc_mitsuba_Bitmap_component_format = R"doc(Return the component format of this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_data = R"doc(Return a pointer to the underlying bitmap storage)doc";

static const char *__doc_mitsuba_Bitmap_data_2 = R"doc(Return a pointer to the underlying bitmap storage)doc";

static const char *__doc_mitsuba_Bitmap_gamma = R"doc(Return the bitmap's gamma identifier (-1: sRGB))doc";

static const char *__doc_mitsuba_Bitmap_has_alpha = R"doc(Return whether this image has an alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_height = R"doc(Return the bitmap's height in pixels)doc";

static const char *__doc_mitsuba_Bitmap_m_component_format = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_data = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_gamma = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_metadata = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_owns_data = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_pixel_format = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_size = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_struct = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_metadata = R"doc(Return a Properties object containing the image metadata)doc";

static const char *__doc_mitsuba_Bitmap_metadata_2 =
R"doc(Return a Properties object containing the image metadata (const
version))doc";

static const char *__doc_mitsuba_Bitmap_pixel_count = R"doc(Return the total number of pixels)doc";

static const char *__doc_mitsuba_Bitmap_pixel_format = R"doc(Return the pixel format of this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_read_open_exr = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_rebuild_struct = R"doc(Rebuild the 'm_struct' field based on the pixel format etc.)doc";

static const char *__doc_mitsuba_Bitmap_set_gamma = R"doc(Set the bitmap's gamma identifier (-1: sRGB))doc";

static const char *__doc_mitsuba_Bitmap_set_metadata = R"doc(Set the a Properties object containing the image metadata)doc";

static const char *__doc_mitsuba_Bitmap_size = R"doc(Return the bitmap dimensions in pixels)doc";

static const char *__doc_mitsuba_Bitmap_struct = R"doc(Return a ``Struct`` instance describing the contents of the bitmap)doc";

static const char *__doc_mitsuba_Bitmap_to_string = R"doc(Return a human-readable summary of this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_width = R"doc(Return the bitmap's width in pixels)doc";

static const char *__doc_mitsuba_Bitmap_write =
R"doc(Write an encoded form of the bitmap to a file using the specified file
format

Parameter ``format``:
    Target file format (EOpenEXR, EPNG, or EOpenEXR)

Parameter ``stream``:
    Target stream that will receive the encoded output

Parameter ``compression``:
    For PNG images, this parameter can be used to control how strongly
    libpng will try to compress the output (with 1 being the lowest
    and 9 denoting the highest compression). Note that saving files
    with the highest compression will be very slow. For JPEG files,
    this denotes the desired quality (between 0 and 100, the latter
    being best). The default argument (-1) uses compression 5 for PNG
    and 100 for JPEG files.)doc";

static const char *__doc_mitsuba_Bitmap_write_2 =
R"doc(Write an encoded form of the bitmap to a stream using the specified
file format

Parameter ``format``:
    Target file format (EOpenEXR, EPNG, or EOpenEXR)

Parameter ``stream``:
    Target stream that will receive the encoded output

Parameter ``compression``:
    For PNG images, this parameter can be used to control how strongly
    libpng will try to compress the output (with 1 being the lowest
    and 9 denoting the highest compression). Note that saving files
    with the highest compression will be very slow. For JPEG files,
    this denotes the desired quality (between 0 and 100, the latter
    being best). The default argument (-1) uses compression 5 for PNG
    and 100 for JPEG files.)doc";

static const char *__doc_mitsuba_Bitmap_write_jpeg = R"doc(Save a file using the JPEG file format)doc";

static const char *__doc_mitsuba_Bitmap_write_openexr = R"doc(Write a file using the OpenEXR file format)doc";

static const char *__doc_mitsuba_BoundingBox =
R"doc(Generic n-dimensional bounding box data structure

Maintains a minimum and maximum position along each dimension and
provides various convenience functions for querying and modifying
them.

This class is parameterized by the underlying point data structure,
which permits the use of different scalar types and dimensionalities,
e.g.

```
BoundingBox<Vector3i> integerBBox(Point3i(0, 1, 3), Point3i(4, 5, 6));
BoundingBox<Vector2d> doubleBBox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));
```

Template parameter ``T``:
    The underlying point data type (e.g. ``Point2d``))doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox =
R"doc(Create a new invalid bounding box

Initializes the components of the minimum and maximum position to
$\infty$ and $-\infty$, respectively.)doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox_2 = R"doc(Create a collapsed bounding box from a single point)doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox_3 = R"doc(Create a bounding box from two positions)doc";

static const char *__doc_mitsuba_BoundingBox_center = R"doc(Return the center point)doc";

static const char *__doc_mitsuba_BoundingBox_clip = R"doc(Clip this bounding box to another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_collapsed =
R"doc(Check whether this bounding box has collapsed to a point, line, or
plane)doc";

static const char *__doc_mitsuba_BoundingBox_contains =
R"doc(Check whether a point lies *on* or *inside* the bounding box

Parameter ``p``:
    The point to be tested

Template parameter ``Strict``:
    Set this parameter to ``True`` if the bounding box boundary should
    be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.)doc";

static const char *__doc_mitsuba_BoundingBox_contains_2 =
R"doc(Check whether a specified bounding box lies *on* or *within* the
current bounding box

Note that by definition, an 'invalid' bounding box (where min=$\infty$
and max=$-\infty$) does not cover any space. Hence, this method will
always return *true* when given such an argument.

Template parameter ``Strict``:
    Set this parameter to ``True`` if the bounding box boundary should
    be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.)doc";

static const char *__doc_mitsuba_BoundingBox_corner = R"doc(Return the position of a bounding box corner)doc";

static const char *__doc_mitsuba_BoundingBox_distance =
R"doc(Calculate the shortest distance between the axis-aligned bounding box
and the point ``p``.)doc";

static const char *__doc_mitsuba_BoundingBox_distance_2 =
R"doc(Calculate the shortest distance between the axis-aligned bounding box
and ``bbox``.)doc";

static const char *__doc_mitsuba_BoundingBox_expand = R"doc(Expand the bounding box to contain another point)doc";

static const char *__doc_mitsuba_BoundingBox_expand_2 = R"doc(Expand the bounding box to contain another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_extents =
R"doc(Calculate the bounding box extents

Returns:
    max-min)doc";

static const char *__doc_mitsuba_BoundingBox_major_axis = R"doc(Return the dimension index with the largest associated side length)doc";

static const char *__doc_mitsuba_BoundingBox_max = R"doc(< Component-wise maximum)doc";

static const char *__doc_mitsuba_BoundingBox_merge = R"doc(Merge two bounding boxes)doc";

static const char *__doc_mitsuba_BoundingBox_min = R"doc(< Component-wise minimum)doc";

static const char *__doc_mitsuba_BoundingBox_minor_axis = R"doc(Return the dimension index with the shortest associated side length)doc";

static const char *__doc_mitsuba_BoundingBox_operator_eq = R"doc(Test for equality against another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_operator_ne = R"doc(Test for inequality against another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_overlaps =
R"doc(Check two axis-aligned bounding boxes for possible overlap.

Parameter ``Strict``:
    Set this parameter to ``True`` if the bounding box boundary should
    be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.

Returns:
    ``True`` If overlap was detected.)doc";

static const char *__doc_mitsuba_BoundingBox_ray_intersect = R"doc(Check if a ray intersects a bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_reset =
R"doc(Mark the bounding box as invalid.

This operation sets the components of the minimum and maximum position
to $\infty$ and $-\infty$, respectively.)doc";

static const char *__doc_mitsuba_BoundingBox_squared_distance =
R"doc(Calculate the shortest squared distance between the axis-aligned
bounding box and the point ``p``.)doc";

static const char *__doc_mitsuba_BoundingBox_squared_distance_2 =
R"doc(Calculate the shortest squared distance between the axis-aligned
bounding box and ``bbox``.)doc";

static const char *__doc_mitsuba_BoundingBox_surface_area = R"doc(Calculate the n-1 dimensional volume of the boundary)doc";

static const char *__doc_mitsuba_BoundingBox_surface_area_2 = R"doc(Calculate the n-1 dimensional volume of the boundary)doc";

static const char *__doc_mitsuba_BoundingBox_valid =
R"doc(Check whether this is a valid bounding box

A bounding box ``bbox`` is considered to be valid when

```
bbox.min[i] <= bbox.max[i]
```

holds for each component ``i``.)doc";

static const char *__doc_mitsuba_BoundingBox_volume = R"doc(Calculate the n-dimensional volume of the bounding box)doc";

static const char *__doc_mitsuba_Class =
R"doc(Stores meta-information about Object instances.

This class provides a thin layer of RTTI (run-time type information),
which is useful for doing things like:

* Checking if an object derives from a certain class

* Determining the parent of a class at runtime

* Instantiating a class by name

* Unserializing a class from a binary data stream

See also:
    ref, Object)doc";

static const char *__doc_mitsuba_Class_Class =
R"doc(Construct a new class descriptor

This method should never be called manually. Instead, use the
MTS_IMPLEMENT_CLASS macro to automatically do this for you.

Parameter ``name``:
    Name of the class

Parameter ``parent``:
    Name of the parent class

Parameter ``abstract``:
    ``True`` if the class contains pure virtual methods

Parameter ``constr``:
    Pointer to a default construction function

Parameter ``unser``:
    Pointer to a unserialization construction function)doc";

static const char *__doc_mitsuba_Class_Class_2 =
R"doc(Construct a new class descriptor

This constructor additionally takes an alias name that is used to
refer to instances of this type in Mitsuba's scene specification
language.

This method should never be called manually. Instead, use the
MTS_IMPLEMENT_CLASS_ALIAS macro to automatically do this for you.

Parameter ``name``:
    Name of the class

Parameter ``alias``:
    Name used to refer to instances of this type in Mitsuba's scene
    description language (usually the same as ``name``)

Parameter ``parent``:
    Name of the parent class

Parameter ``abstract``:
    ``True`` if the class contains pure virtual methods

Parameter ``constr``:
    Pointer to a default construction function

Parameter ``unser``:
    Pointer to a unserialization construction function)doc";

static const char *__doc_mitsuba_Class_alias = R"doc(Return the scene description-specific alias)doc";

static const char *__doc_mitsuba_Class_construct =
R"doc(Generate an instance of this class

Parameter ``props``:
    A list of extra parameters that are supplied to the constructor

Remark:
    Throws an exception if the class is not constructible)doc";

static const char *__doc_mitsuba_Class_derives_from = R"doc(Check whether this class derives from *class_*)doc";

static const char *__doc_mitsuba_Class_for_name = R"doc(Look up a class by its name)doc";

static const char *__doc_mitsuba_Class_initialize_once = R"doc(Initialize a class - called by static_initialization())doc";

static const char *__doc_mitsuba_Class_is_abstract =
R"doc(Return whether or not the class represented by this Class object
contains pure virtual methods)doc";

static const char *__doc_mitsuba_Class_is_constructible = R"doc(Does the class support instantiation over RTTI?)doc";

static const char *__doc_mitsuba_Class_is_serializable = R"doc(Does the class support serialization?)doc";

static const char *__doc_mitsuba_Class_m_abstract = R"doc()doc";

static const char *__doc_mitsuba_Class_m_alias = R"doc()doc";

static const char *__doc_mitsuba_Class_m_constr = R"doc()doc";

static const char *__doc_mitsuba_Class_m_name = R"doc()doc";

static const char *__doc_mitsuba_Class_m_parent = R"doc()doc";

static const char *__doc_mitsuba_Class_m_parent_name = R"doc()doc";

static const char *__doc_mitsuba_Class_m_unser = R"doc()doc";

static const char *__doc_mitsuba_Class_name = R"doc(Return the name of the represented class)doc";

static const char *__doc_mitsuba_Class_parent =
R"doc(Return the Class object associated with the parent class of nullptr if
it does not have one.)doc";

static const char *__doc_mitsuba_Class_rtti_is_initialized = R"doc(Check if the RTTI layer has been initialized)doc";

static const char *__doc_mitsuba_Class_static_initialization =
R"doc(Initializes the built-in RTTI and creates a list of all compiled
classes)doc";

static const char *__doc_mitsuba_Class_static_shutdown = R"doc(Free the memory taken by static_initialization())doc";

static const char *__doc_mitsuba_Class_unserialize =
R"doc(Unserialize an instance of the class

Remark:
    Throws an exception if the class is not unserializable)doc";

static const char *__doc_mitsuba_Color = R"doc(//! @{ \name Data types for RGB data)doc";

static const char *__doc_mitsuba_Color_Color = R"doc()doc";

static const char *__doc_mitsuba_Color_Color_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_Color_3 = R"doc()doc";

static const char *__doc_mitsuba_Color_b = R"doc()doc";

static const char *__doc_mitsuba_Color_b_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_g = R"doc()doc";

static const char *__doc_mitsuba_Color_g_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Color_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_r = R"doc()doc";

static const char *__doc_mitsuba_Color_r_2 = R"doc()doc";

static const char *__doc_mitsuba_ContinuousSpectrum =
R"doc(Abstract continuous spectral power distribution data type, which
supports evaluation at arbitrary wavelengths.

Remark:
    The term 'continuous' doesn't necessarily mean that the underlying
    spectrum is continuous, but rather emphasizes the fact that it is
    a function over the reals (as opposed to the discretely sampled
    spectrum, which only stores samples at a finite set of
    wavelengths).)doc";

static const char *__doc_mitsuba_ContinuousSpectrum_class = R"doc()doc";

static const char *__doc_mitsuba_ContinuousSpectrum_eval =
R"doc(Evaluate the value of the spectral power distribution at a set of
wavelengths

Parameter ``lambda``:
    List of wavelengths specified in nanometers)doc";

static const char *__doc_mitsuba_ContinuousSpectrum_eval_2 = R"doc(Vectorized version of eval())doc";

static const char *__doc_mitsuba_ContinuousSpectrum_integral =
R"doc(Return the integral over the spectrum over its support

Not every implementation may provide this function; the default
implementation throws an exception.

Even if the operation is provided, it may only return an
approximation.)doc";

static const char *__doc_mitsuba_ContinuousSpectrum_pdf =
R"doc(Return the probability distribution of the sample() method as a
probability per unit wavelength (in nm).

Not every implementation may provide this function; the default
implementation throws an exception.)doc";

static const char *__doc_mitsuba_ContinuousSpectrum_pdf_2 = R"doc(Vectorized version of pdf())doc";

static const char *__doc_mitsuba_ContinuousSpectrum_sample =
R"doc(Importance sample the spectral power distribution

Not every implementation may provide this function; the default
implementation throws an exception.

Parameter ``sample``:
    A uniform variate

Returns:
    1. Set of sampled wavelengths specified in nanometers 2. The Monte
    Carlo sampling weight (SPD value divided by the sampling density)
    3. Sample probability per unit wavelength (in units of 1/nm))doc";

static const char *__doc_mitsuba_ContinuousSpectrum_sample_2 = R"doc(Vectorized version of sample())doc";

static const char *__doc_mitsuba_DefaultFormatter =
R"doc(The default formatter used to turn log messages into a human-readable
form)doc";

static const char *__doc_mitsuba_DefaultFormatter_DefaultFormatter = R"doc(Create a new default formatter)doc";

static const char *__doc_mitsuba_DefaultFormatter_class = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_format = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_has_class =
R"doc(See also:
    set_has_class)doc";

static const char *__doc_mitsuba_DefaultFormatter_has_date =
R"doc(See also:
    set_has_date)doc";

static const char *__doc_mitsuba_DefaultFormatter_has_log_level =
R"doc(See also:
    set_has_log_level)doc";

static const char *__doc_mitsuba_DefaultFormatter_has_thread =
R"doc(See also:
    set_has_thread)doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_class = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_date = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_log_level = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_thread = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_class = R"doc(Should class information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_date = R"doc(Should date information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_log_level = R"doc(Should log level information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_thread = R"doc(Should thread information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DiscreteDistribution =
R"doc(Discrete probability distribution

This data structure can be used to transform uniformly distributed
samples to a stored discrete probability distribution.)doc";

static const char *__doc_mitsuba_DiscreteDistribution_DiscreteDistribution = R"doc(Allocate memory for a distribution with the given number of entries)doc";

static const char *__doc_mitsuba_DiscreteDistribution_append =
R"doc(Append an entry with the specified discrete probability.

Remark:
    ``pdf_value`` must be non-negative.)doc";

static const char *__doc_mitsuba_DiscreteDistribution_cdf =
R"doc(Return the cdf entries.

Note that if n values have been appended, there will be (n+1) entries
in this CDF (the first one being 0).)doc";

static const char *__doc_mitsuba_DiscreteDistribution_clear = R"doc(Clear all entries)doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_normalization = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_normalized = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_range_end = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_range_start = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_sum = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_normalization =
R"doc(Return the normalization factor (i.e. the inverse of get_sum())

This assumes that normalize() has previously been called)doc";

static const char *__doc_mitsuba_DiscreteDistribution_normalize =
R"doc(Normalize the distribution

Throws an exception when the distribution contains no elements. The
distribution is not considered to be normalized if the sum of
probabilities equals zero.

Returns:
    Sum of the (previously unnormalized) entries)doc";

static const char *__doc_mitsuba_DiscreteDistribution_normalized = R"doc(Have the probability densities been normalized?)doc";

static const char *__doc_mitsuba_DiscreteDistribution_operator_array = R"doc(Access an entry by its index)doc";

static const char *__doc_mitsuba_DiscreteDistribution_reserve = R"doc(Reserve memory for a certain number of entries)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample =
R"doc(%Transform a uniformly distributed sample to the stored distribution

Parameter ``sample_value``:
    A uniformly distributed sample on [0,1]

Returns:
    The discrete index associated with the sample)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample_pdf =
R"doc(%Transform a uniformly distributed sample to the stored distribution

Parameter ``sample_value``:
    A uniformly distributed sample on [0,1]

Returns:
    A pair with (the discrete index associated with the sample,
    probability value of the sample).)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample_reuse =
R"doc(%Transform a uniformly distributed sample to the stored distribution

The original sample is value adjusted so that it can be "reused".

\param[in,out] sample_value A uniformly distributed sample on [0,1]

Returns:
    The discrete index associated with the sample

\note In the Python API, the rescaled sample value is returned in
second position.)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample_reuse_pdf =
R"doc(%Transform a uniformly distributed sample to the stored distribution.

The original sample is value adjusted so that it can be "reused".

\param[in,out] sample_value A uniformly distributed sample on [0,1]

Returns:
    A pair with (the discrete index associated with the sample,
    probability value of the sample). \note In the Python API, the
    rescaled sample value is returned in third position.)doc";

static const char *__doc_mitsuba_DiscreteDistribution_size = R"doc(Return the number of entries so far)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sum =
R"doc(Return the original (unnormalized) sum of all PDF entries

This assumes that normalize() has previously been called.)doc";

static const char *__doc_mitsuba_DummyStream =
R"doc(Stream implementation that never writes to disk, but keeps track of
the size of the content being written. It can be used, for example, to
measure the precise amount of memory needed to store serialized
content.)doc";

static const char *__doc_mitsuba_DummyStream_DummyStream = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_can_read =
R"doc(Always returns false, as nothing written to a ``DummyStream`` is
actually written.)doc";

static const char *__doc_mitsuba_DummyStream_can_write = R"doc(Always returns true, except if the steam is closed.)doc";

static const char *__doc_mitsuba_DummyStream_class = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_close =
R"doc(Closes the stream. No further read or write operations are permitted.

This function is idempotent. It may be called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_DummyStream_flush = R"doc(No-op for ``DummyStream``.)doc";

static const char *__doc_mitsuba_DummyStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_DummyStream_m_is_closed = R"doc(Whether the stream has been closed.)doc";

static const char *__doc_mitsuba_DummyStream_m_pos =
R"doc(Current position in the "virtual" stream (even though nothing is ever
written, we need to maintain consistent positioning).)doc";

static const char *__doc_mitsuba_DummyStream_m_size = R"doc(Size of all data written to the stream)doc";

static const char *__doc_mitsuba_DummyStream_read = R"doc(Always throws, since DummyStream is write-only.)doc";

static const char *__doc_mitsuba_DummyStream_seek =
R"doc(Updates the current position in the stream. Even though the
``DummyStream`` doesn't write anywhere, position is taken into account
to accurately compute the size of the stream.)doc";

static const char *__doc_mitsuba_DummyStream_size = R"doc(Returns the size of the stream.)doc";

static const char *__doc_mitsuba_DummyStream_tell = R"doc(Returns the current position in the stream.)doc";

static const char *__doc_mitsuba_DummyStream_truncate =
R"doc(Simply sets the current size of the stream. The position is updated to
``min(old_position, size)``.)doc";

static const char *__doc_mitsuba_DummyStream_write =
R"doc(Does not actually write anything, only updates the stream's position
and size.)doc";

static const char *__doc_mitsuba_ELogLevel = R"doc(Available Log message types)doc";

static const char *__doc_mitsuba_ELogLevel_EDebug = R"doc(< Debug message, usually turned off)doc";

static const char *__doc_mitsuba_ELogLevel_EError = R"doc(< Error message, causes an exception to be thrown)doc";

static const char *__doc_mitsuba_ELogLevel_EInfo = R"doc(< More relevant debug / information message)doc";

static const char *__doc_mitsuba_ELogLevel_ETrace = R"doc(< Trace message, for extremely verbose debugging)doc";

static const char *__doc_mitsuba_ELogLevel_EWarn = R"doc(< Warning message)doc";

static const char *__doc_mitsuba_FileResolver =
R"doc(Simple class for resolving paths on Linux/Windows/Mac OS

This convenience class looks for a file or directory given its name
and a set of search paths. The implementation walks through the search
paths in order and stops once the file is found.)doc";

static const char *__doc_mitsuba_FileResolver_FileResolver = R"doc(Initialize a new file resolver with the current working directory)doc";

static const char *__doc_mitsuba_FileResolver_append = R"doc(Append an entry to the end of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_begin = R"doc(Return an iterator at the beginning of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_begin_2 =
R"doc(Return an iterator at the beginning of the list of search paths
(const))doc";

static const char *__doc_mitsuba_FileResolver_class = R"doc()doc";

static const char *__doc_mitsuba_FileResolver_clear = R"doc(Clear the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_contains = R"doc(Check if a given path is included in the search path list)doc";

static const char *__doc_mitsuba_FileResolver_end = R"doc(Return an iterator at the end of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_end_2 = R"doc(Return an iterator at the end of the list of search paths (const))doc";

static const char *__doc_mitsuba_FileResolver_erase = R"doc(Erase the entry at the given iterator position)doc";

static const char *__doc_mitsuba_FileResolver_erase_2 = R"doc(Erase the search path from the list)doc";

static const char *__doc_mitsuba_FileResolver_m_paths = R"doc()doc";

static const char *__doc_mitsuba_FileResolver_operator_array = R"doc(Return an entry from the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_operator_array_2 = R"doc(Return an entry from the list of search paths (const))doc";

static const char *__doc_mitsuba_FileResolver_prepend = R"doc(Prepend an entry at the beginning of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_resolve =
R"doc(Walk through the list of search paths and try to resolve the input
path)doc";

static const char *__doc_mitsuba_FileResolver_size = R"doc(Return the number of search paths)doc";

static const char *__doc_mitsuba_FileResolver_to_string = R"doc(Return a human-readable representation of this instance)doc";

static const char *__doc_mitsuba_FileStream =
R"doc(Simple Stream implementation backed-up by a file. The underlying file
abstraction is std::fstream, and so most operations can be expected to
behave similarly.)doc";

static const char *__doc_mitsuba_FileStream_FileStream =
R"doc(Constructs a new FileStream by opening the file pointed by ``p``. The
file is opened in read-only or read/write mode as specified by
``write_enabled``.

If ``write_enabled`` and the file did not exist before, it is created.
Throws if trying to open a non-existing file in with write disabled.
Throws an exception if the file cannot be opened / created.)doc";

static const char *__doc_mitsuba_FileStream_can_read = R"doc(True except if the stream was closed.)doc";

static const char *__doc_mitsuba_FileStream_can_write = R"doc(Whether the field was open in write-mode (and was not closed))doc";

static const char *__doc_mitsuba_FileStream_class = R"doc()doc";

static const char *__doc_mitsuba_FileStream_close =
R"doc(Closes the stream and the underlying file. No further read or write
operations are permitted.

This function is idempotent. It is called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_FileStream_flush = R"doc(Flushes any buffered operation to the underlying file.)doc";

static const char *__doc_mitsuba_FileStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_FileStream_m_file = R"doc()doc";

static const char *__doc_mitsuba_FileStream_m_path = R"doc()doc";

static const char *__doc_mitsuba_FileStream_m_write_enabled = R"doc()doc";

static const char *__doc_mitsuba_FileStream_native = R"doc(Return the "native" std::fstream associated with this FileStream)doc";

static const char *__doc_mitsuba_FileStream_path = R"doc(Return the path descriptor associated with this FileStream)doc";

static const char *__doc_mitsuba_FileStream_read =
R"doc(Reads a specified amount of data from the stream. Throws an exception
when the stream ended prematurely.)doc";

static const char *__doc_mitsuba_FileStream_read_line = R"doc(Convenience function for reading a line of text from an ASCII file)doc";

static const char *__doc_mitsuba_FileStream_seek =
R"doc(Seeks to a position inside the stream. May throw if the resulting
state is invalid.)doc";

static const char *__doc_mitsuba_FileStream_size =
R"doc(Returns the size of the file. \note After a write, the size may not be
updated until a flush is performed.)doc";

static const char *__doc_mitsuba_FileStream_tell = R"doc(Gets the current position inside the file)doc";

static const char *__doc_mitsuba_FileStream_to_string = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_FileStream_truncate =
R"doc(Truncates the file to a given size. Automatically flushes the stream
before truncating the file. The position is updated to
``min(old_position, size)``.

Throws an exception if in read-only mode.)doc";

static const char *__doc_mitsuba_FileStream_write =
R"doc(Writes a specified amount of data into the stream. Throws an exception
when not all data could be written.)doc";

static const char *__doc_mitsuba_Formatter =
R"doc(Abstract interface for converting log information into a human-
readable format)doc";

static const char *__doc_mitsuba_Formatter_class = R"doc()doc";

static const char *__doc_mitsuba_Formatter_format =
R"doc(Turn a log message into a human-readable format

Parameter ``level``:
    The importance of the debug message

Parameter ``class_``:
    Originating class or ``nullptr``

Parameter ``thread``:
    Thread, which is reponsible for creating the message

Parameter ``file``:
    File, which is responsible for creating the message

Parameter ``line``:
    Associated line within the source file

Parameter ``msg``:
    Text content associated with the log message)doc";

static const char *__doc_mitsuba_Frame =
R"doc(Stores a three-dimensional orthonormal coordinate frame

This class is used to convert between different cartesian coordinate
systems and to efficiently evaluate trigonometric functions in a
spherical coordinate system whose pole is aligned with the ``n`` axis
(e.g. cos_theta(), sin_phi(), etc.).)doc";

static const char *__doc_mitsuba_Frame_Frame = R"doc(Default constructor -- performs no initialization!)doc";

static const char *__doc_mitsuba_Frame_Frame_2 = R"doc(Given a normal and tangent vectors, construct a new coordinate frame)doc";

static const char *__doc_mitsuba_Frame_Frame_3 = R"doc(Construct a frame from the given orthonormal vectors)doc";

static const char *__doc_mitsuba_Frame_Frame_4 = R"doc(Construct a new coordinate frame from a single vector)doc";

static const char *__doc_mitsuba_Frame_cos_phi =
R"doc(Assuming that the given direction is in the local coordinate system,
return the cosine of the phi parameter in spherical coordinates)doc";

static const char *__doc_mitsuba_Frame_cos_phi_2 =
R"doc(Assuming that the given direction is in the local coordinate system,
return the squared cosine of the phi parameter in spherical
coordinates)doc";

static const char *__doc_mitsuba_Frame_cos_theta =
R"doc(Assuming that the given direction is in the local coordinate system,
return the cosine of the angle between the normal and v)doc";

static const char *__doc_mitsuba_Frame_cos_theta_2 =
R"doc(Assuming that the given direction is in the local coordinate system,
return the squared cosine of the angle between the normal and v)doc";

static const char *__doc_mitsuba_Frame_n = R"doc()doc";

static const char *__doc_mitsuba_Frame_operator_eq = R"doc(Equality test)doc";

static const char *__doc_mitsuba_Frame_operator_ne = R"doc(Inequality test)doc";

static const char *__doc_mitsuba_Frame_s = R"doc()doc";

static const char *__doc_mitsuba_Frame_sin_phi =
R"doc(Assuming that the given direction is in the local coordinate system,
return the sine of the phi parameter in spherical coordinates)doc";

static const char *__doc_mitsuba_Frame_sin_phi_2 =
R"doc(Assuming that the given direction is in the local coordinate system,
return the squared sine of the phi parameter in spherical coordinates)doc";

static const char *__doc_mitsuba_Frame_sin_theta =
R"doc(Assuming that the given direction is in the local coordinate system,
return the sine of the angle between the normal and v)doc";

static const char *__doc_mitsuba_Frame_sin_theta_2 =
R"doc(Assuming that the given direction is in the local coordinate system,
return the squared sine of the angle between the normal and v)doc";

static const char *__doc_mitsuba_Frame_t = R"doc()doc";

static const char *__doc_mitsuba_Frame_tan_theta =
R"doc(Assuming that the given direction is in the local coordinate system,
return the tangent of the angle between the normal and v)doc";

static const char *__doc_mitsuba_Frame_tan_theta_2 =
R"doc(Assuming that the given direction is in the local coordinate system,
return the squared tangent of the angle between the normal and v)doc";

static const char *__doc_mitsuba_Frame_to_local = R"doc(Convert from world coordinates to local coordinates)doc";

static const char *__doc_mitsuba_Frame_to_world = R"doc(Convert from local coordinates to world coordinates)doc";

static const char *__doc_mitsuba_Frame_uv =
R"doc(Assuming that the given direction is in the local coordinate system,
return the u and v coordinates of the vector 'v')doc";

static const char *__doc_mitsuba_GLTexture = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_EInterpolation = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_EInterpolation_ELinear = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_EInterpolation_EMipMapLinear = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_EInterpolation_ENearest = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_GLTexture = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_bind = R"doc(Bind the texture to a specific texture unit)doc";

static const char *__doc_mitsuba_GLTexture_class = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_free = R"doc(Release underlying OpenGL objects)doc";

static const char *__doc_mitsuba_GLTexture_id = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_init = R"doc(Create a new uniform texture)doc";

static const char *__doc_mitsuba_GLTexture_m_id = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_m_index = R"doc()doc";

static const char *__doc_mitsuba_GLTexture_refresh = R"doc(Refresh the texture from the provided bitmap)doc";

static const char *__doc_mitsuba_GLTexture_release = R"doc(Release/unbind the textur)doc";

static const char *__doc_mitsuba_GLTexture_set_interpolation = R"doc(Set the interpolation mode)doc";

static const char *__doc_mitsuba_InterpolatedSpectrum = R"doc(Linear interpolant of a regularly sampled spectrum)doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_InterpolatedSpectrum = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_class = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_eval = R"doc(//! @{ \name Implementation of the ContinuousSpectrum interface)doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_eval_2 = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_eval_3 = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_integral = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_data = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_integral = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_interval_size = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_inv_interval_size = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_lambda_max = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_lambda_min = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_normalization = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_m_size_minus_2 = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_pdf = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_pdf_2 = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_sample = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_sample_2 = R"doc()doc";

static const char *__doc_mitsuba_InterpolatedSpectrum_sample_3 = R"doc()doc";

static const char *__doc_mitsuba_Jit = R"doc()doc";

static const char *__doc_mitsuba_Jit_Jit = R"doc()doc";

static const char *__doc_mitsuba_Jit_Jit_2 = R"doc()doc";

static const char *__doc_mitsuba_Jit_getInstance = R"doc()doc";

static const char *__doc_mitsuba_Jit_mutex = R"doc()doc";

static const char *__doc_mitsuba_Jit_runtime = R"doc()doc";

static const char *__doc_mitsuba_Jit_static_initialization =
R"doc(Statically initialize the JIT runtime

This function also does a runtime-check to ensure that the host
processor supports all instruction sets which were selected at compile
time. If not, the application is terminated via ``abort``().)doc";

static const char *__doc_mitsuba_Jit_static_shutdown = R"doc(Release all memory used by JIT-compiled routines)doc";

static const char *__doc_mitsuba_Logger =
R"doc(Responsible for processing log messages

Upon receiving a log message, the Logger class invokes a Formatter to
convert it into a human-readable form. Following that, it sends this
information to every registered Appender.)doc";

static const char *__doc_mitsuba_Logger_Logger = R"doc(Construct a new logger with the given minimum log level)doc";

static const char *__doc_mitsuba_Logger_add_appender = R"doc(Add an appender to this logger)doc";

static const char *__doc_mitsuba_Logger_appender = R"doc(Return one of the appenders)doc";

static const char *__doc_mitsuba_Logger_appender_2 = R"doc(Return one of the appenders)doc";

static const char *__doc_mitsuba_Logger_appender_count = R"doc(Return the number of registered appenders)doc";

static const char *__doc_mitsuba_Logger_class = R"doc()doc";

static const char *__doc_mitsuba_Logger_clear_appenders = R"doc(Remove all appenders from this logger)doc";

static const char *__doc_mitsuba_Logger_d = R"doc()doc";

static const char *__doc_mitsuba_Logger_error_level = R"doc(Return the current error level)doc";

static const char *__doc_mitsuba_Logger_formatter = R"doc(Return the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_formatter_2 = R"doc(Return the logger's formatter implementation (const))doc";

static const char *__doc_mitsuba_Logger_log =
R"doc(Process a log message

Parameter ``level``:
    Log level of the message

Parameter ``class_``:
    Class descriptor of the message creator

Parameter ``filename``:
    Source file of the message creator

Parameter ``line``:
    Source line number of the message creator

Parameter ``fmt``:
    printf-style string formatter

\note This function is not exposed in the Python bindings. Instead,
please use \cc mitsuba.core.Log)doc";

static const char *__doc_mitsuba_Logger_log_level = R"doc(Return the current log level)doc";

static const char *__doc_mitsuba_Logger_log_progress =
R"doc(Process a progress message

Parameter ``progress``:
    Percentage value in [0, 100]

Parameter ``name``:
    Title of the progress message

Parameter ``formatted``:
    Formatted string representation of the message

Parameter ``eta``:
    Estimated time until 100% is reached.

Parameter ``ptr``:
    Custom pointer payload. This is used to express the context of a
    progress message. When rendering a scene, it will usually contain
    a pointer to the associated ``RenderJob``.)doc";

static const char *__doc_mitsuba_Logger_m_log_level = R"doc()doc";

static const char *__doc_mitsuba_Logger_read_log =
R"doc(Return the contents of the log file as a string

Throws a runtime exception upon failure)doc";

static const char *__doc_mitsuba_Logger_remove_appender = R"doc(Remove an appender from this logger)doc";

static const char *__doc_mitsuba_Logger_set_error_level =
R"doc(Set the error log level (this level and anything above will throw
exceptions).

The value provided here can be used for instance to turn warnings into
errors. But *level* must always be less than EError, i.e. it isn't
possible to cause errors not to throw an exception.)doc";

static const char *__doc_mitsuba_Logger_set_formatter = R"doc(Set the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_set_log_level = R"doc(Set the log level (everything below will be ignored))doc";

static const char *__doc_mitsuba_Logger_static_initialization = R"doc(Initialize logging)doc";

static const char *__doc_mitsuba_Logger_static_shutdown = R"doc(Shutdown logging)doc";

static const char *__doc_mitsuba_MemoryStream =
R"doc(Simple memory buffer-based stream with automatic memory management. It
always has read & write capabilities.

The underlying memory storage of this implementation dynamically
expands as data is written to the stream, à la ``std::vector``.)doc";

static const char *__doc_mitsuba_MemoryStream_MemoryStream =
R"doc(Creates a new memory stream, initializing the memory buffer with a
capacity of ``capacity`` bytes. For best performance, set this
argument to the estimated size of the content that will be written to
the stream.)doc";

static const char *__doc_mitsuba_MemoryStream_MemoryStream_2 =
R"doc(Creates a memory stream, which operates on a pre-allocated buffer.

A memory stream created in this way will never resize the underlying
buffer. An exception is thrown e.g. when attempting to extend its
size.

Remark:
    This constructor is not available in the python bindings.)doc";

static const char *__doc_mitsuba_MemoryStream_can_read = R"doc(Always returns true, except if the stream is closed.)doc";

static const char *__doc_mitsuba_MemoryStream_can_write = R"doc(Always returns true, except if the stream is closed.)doc";

static const char *__doc_mitsuba_MemoryStream_capacity = R"doc(Return the current capacity of the underlying memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_class = R"doc()doc";

static const char *__doc_mitsuba_MemoryStream_close =
R"doc(Closes the stream. No further read or write operations are permitted.

This function is idempotent. It may be called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_MemoryStream_flush = R"doc(No-op since all writes are made directly to memory)doc";

static const char *__doc_mitsuba_MemoryStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_MemoryStream_m_capacity = R"doc(Current size of the allocated memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_data = R"doc(Pointer to the memory buffer (might not be owned))doc";

static const char *__doc_mitsuba_MemoryStream_m_is_closed = R"doc(Whether the stream has been closed.)doc";

static const char *__doc_mitsuba_MemoryStream_m_owns_buffer = R"doc(False if the MemoryStream was created from a pre-allocated buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_pos = R"doc(Current position inside of the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_size = R"doc(Current size of the contents written to the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_owns_buffer = R"doc(Return whether or not the memory stream owns the underlying buffer)doc";

static const char *__doc_mitsuba_MemoryStream_read =
R"doc(Reads a specified amount of data from the stream. Throws an exception
if trying to read further than the current size of the contents.)doc";

static const char *__doc_mitsuba_MemoryStream_resize = R"doc()doc";

static const char *__doc_mitsuba_MemoryStream_seek =
R"doc(Seeks to a position inside the stream. You may seek beyond the size of
the stream's contents, or even beyond the buffer's capacity. The size
and capacity are **not** affected. A subsequent write would then
expand the size and capacity accordingly. The contents of the memory
that was skipped is undefined.)doc";

static const char *__doc_mitsuba_MemoryStream_size =
R"doc(Returns the size of the contents written to the memory buffer. \note
This is not equal to the size of the memory buffer in general, since
we allocate more capacity at once.)doc";

static const char *__doc_mitsuba_MemoryStream_tell =
R"doc(Gets the current position inside the memory buffer. Note that this
might be further than the stream's size or even capacity.)doc";

static const char *__doc_mitsuba_MemoryStream_to_string = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_MemoryStream_truncate =
R"doc(Truncates the contents **and** the memory buffer's capacity to a given
size. The position is updated to ``min(old_position, size)``.

\note This will throw is the MemoryStream was initialized with a pre-
allocated buffer.)doc";

static const char *__doc_mitsuba_MemoryStream_write =
R"doc(Writes a specified amount of data into the memory buffer. The capacity
of the memory buffer is extended if necessary.)doc";

static const char *__doc_mitsuba_Mesh = R"doc()doc";

static const char *__doc_mitsuba_Mesh_Vertex = R"doc()doc";

static const char *__doc_mitsuba_Mesh_Vertex_x = R"doc()doc";

static const char *__doc_mitsuba_Mesh_Vertex_y = R"doc()doc";

static const char *__doc_mitsuba_Mesh_Vertex_z = R"doc()doc";

static const char *__doc_mitsuba_Mesh_bbox =
R"doc(Return an axis aligned box that bounds the set of triangles (including
any transformations that may have been applied to them))doc";

static const char *__doc_mitsuba_Mesh_bbox_2 =
R"doc(Return an axis aligned box that bounds a single triangle (including
any transformations that may have been applied to it))doc";

static const char *__doc_mitsuba_Mesh_bbox_3 =
R"doc(Return an axis aligned box that bounds a single triangle after it has
been clipped to another bounding box.

This is extremely important to construct decent kd-trees.)doc";

static const char *__doc_mitsuba_Mesh_class = R"doc()doc";

static const char *__doc_mitsuba_Mesh_face = R"doc(Return a pointer to the raw face buffer (at a specified face index))doc";

static const char *__doc_mitsuba_Mesh_face_2 = R"doc(Return a pointer to the raw face buffer (at a specified face index))doc";

static const char *__doc_mitsuba_Mesh_face_count = R"doc(Return the total number of faces)doc";

static const char *__doc_mitsuba_Mesh_face_struct =
R"doc(Return a ``Struct`` instance describing the contents of the face
buffer)doc";

static const char *__doc_mitsuba_Mesh_faces = R"doc(Return a pointer to the raw face buffer)doc";

static const char *__doc_mitsuba_Mesh_faces_2 = R"doc(Return a pointer to the raw face buffer)doc";

static const char *__doc_mitsuba_Mesh_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_face_count = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_face_size = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_face_struct = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_faces = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_name = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_count = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_size = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_struct = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertices = R"doc()doc";

static const char *__doc_mitsuba_Mesh_primitive_count =
R"doc(Returns the number of sub-primitives (i.e. triangles) that make up
this shape)doc";

static const char *__doc_mitsuba_Mesh_ray_intersect =
R"doc(Ray-triangle intersection test

Uses the algorithm by Moeller and Trumbore discussed at
``http://www.acm.org/jgt/papers/MollerTrumbore97/code.html``.

Parameter ``index``:
    Index of the triangle to be intersected

Parameter ``ray``:
    The ray segment to be used for the intersection query. The ray's
    minimum and maximum extent values are not considered.

Returns:
    Returns an ordered tuple ``(mask, u, v, t)``, where ``mask``
    indicates whether an intersection was found, ``t`` contains the
    distance from the ray origin to the intersection point, and ``u``
    and ``v`` contains the first two components of the intersection in
    barycentric coordinates)doc";

static const char *__doc_mitsuba_Mesh_to_string = R"doc(Return a human-readable string representation of the shape contents.)doc";

static const char *__doc_mitsuba_Mesh_vertex =
R"doc(Return a pointer to the raw vertex buffer (at a specified vertex
index))doc";

static const char *__doc_mitsuba_Mesh_vertex_2 =
R"doc(Return a pointer to the raw vertex buffer (at a specified vertex
index))doc";

static const char *__doc_mitsuba_Mesh_vertex_count = R"doc(Return the total number of vertices)doc";

static const char *__doc_mitsuba_Mesh_vertex_struct =
R"doc(Return a ``Struct`` instance describing the contents of the vertex
buffer)doc";

static const char *__doc_mitsuba_Mesh_vertices = R"doc(Return a pointer to the raw vertex buffer)doc";

static const char *__doc_mitsuba_Mesh_vertices_2 = R"doc(Return a pointer to the raw vertex buffer)doc";

static const char *__doc_mitsuba_Mesh_write = R"doc(Export mesh using the file format implemented by the subclass)doc";

static const char *__doc_mitsuba_NamedReference = R"doc(Wrapper object used to represent named references to Object instances)doc";

static const char *__doc_mitsuba_NamedReference_NamedReference = R"doc()doc";

static const char *__doc_mitsuba_NamedReference_m_value = R"doc()doc";

static const char *__doc_mitsuba_NamedReference_operator_const_std_1_basic_string = R"doc()doc";

static const char *__doc_mitsuba_NamedReference_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_NamedReference_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_Normal = R"doc()doc";

static const char *__doc_mitsuba_Normal_Normal = R"doc()doc";

static const char *__doc_mitsuba_Normal_Normal_2 = R"doc()doc";

static const char *__doc_mitsuba_Normal_Normal_3 = R"doc()doc";

static const char *__doc_mitsuba_Normal_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Normal_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Object =
R"doc(Object base class with builtin reference counting

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
counter) and has no overhead for references.)doc";

static const char *__doc_mitsuba_Object_Object = R"doc(Default constructor)doc";

static const char *__doc_mitsuba_Object_Object_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Object_class =
R"doc(Return a Class instance containing run-time type information about
this Object

See also:
    Class)doc";

static const char *__doc_mitsuba_Object_dec_ref =
R"doc(Decrease the reference count of the object and possibly deallocate it.

The object will automatically be deallocated once the reference count
reaches zero.)doc";

static const char *__doc_mitsuba_Object_inc_ref = R"doc(Increase the object's reference count by one)doc";

static const char *__doc_mitsuba_Object_m_ref_count = R"doc()doc";

static const char *__doc_mitsuba_Object_ref_count = R"doc(Return the current reference count)doc";

static const char *__doc_mitsuba_Object_to_string =
R"doc(Return a human-readable string representation of the object's
contents.

This function is mainly useful for debugging purposes and should
ideally be implemented by all subclasses. The default implementation
simply returns ``MyObject[<address of 'this' pointer>]``, where
``MyObject`` is the name of the class.)doc";

static const char *__doc_mitsuba_PluginManager =
R"doc(/// XXX update The object factory is responsible for loading plugin
modules and instantiating object instances.

Ordinarily, this class will be used by making repeated calls to the
create_object() methods. The generated instances are then assembled
into a final object graph, such as a scene. One such examples is the
SceneHandler class, which parses an XML scene file by esentially
translating the XML elements into calls to create_object().

Since this kind of construction method can be tiresome when
dynamically building scenes from Python, this class has an additional
Python-only method ``create``(), which works as follows:

```
from mitsuba.core import *

pmgr = PluginManager.getInstance()
camera = pmgr.create({
    "type" : "perspective",
    "toWorld" : Transform.lookAt(
        Point(0, 0, -10),
        Point(0, 0, 0),
        Vector(0, 1, 0)
    ),
    "film" : {
        "type" : "ldrfilm",
        "width" : 1920,
        "height" : 1080
    }
})
```

The above snippet constructs a Camera instance from a plugin named
``perspective``.so/dll/dylib and adds a child object named ``film``,
which is a Film instance loaded from the plugin
``ldrfilm``.so/dll/dylib. By the time the function returns, the object
hierarchy has already been assembled, and the
ConfigurableObject::configure() methods of every object has been
called.)doc";

static const char *__doc_mitsuba_PluginManager_PluginManager = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_class = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_create_object =
R"doc(Instantiate a plugin, verify its type, and return the newly created
object instance.

Parameter ``classType``:
    Expected type of the instance. An exception will be thrown if it
    turns out not to derive from this class.

Parameter ``props``:
    A Properties instance containing all information required to find
    and construct the plugin.)doc";

static const char *__doc_mitsuba_PluginManager_create_object_2 =
R"doc(Instantiate a plugin and return the new instance (without verifying
its type).

Parameter ``props``:
    A Properties instance containing all information required to find
    and construct the plugin.)doc";

static const char *__doc_mitsuba_PluginManager_d = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_ensure_plugin_loaded = R"doc(Ensure that a plugin is loaded and ready)doc";

static const char *__doc_mitsuba_PluginManager_instance = R"doc(Return the global plugin manager)doc";

static const char *__doc_mitsuba_PluginManager_loaded_plugins = R"doc(Return the list of loaded plugins)doc";

static const char *__doc_mitsuba_Point = R"doc()doc";

static const char *__doc_mitsuba_Point_Point = R"doc()doc";

static const char *__doc_mitsuba_Point_Point_2 = R"doc()doc";

static const char *__doc_mitsuba_Point_Point_3 = R"doc()doc";

static const char *__doc_mitsuba_Point_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Point_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Properties =
R"doc(Associative parameter map for constructing subclasses of Object.

Note that the Python bindings for this class do not implement the
various type-dependent getters and setters. Instead, they are accessed
just like a normal Python map, e.g:

```
myProps = mitsuba.core.Properties("plugin_name")
myProps["stringProperty"] = "hello"
myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)
```)doc";

static const char *__doc_mitsuba_Properties_EPropertyType = R"doc(Supported types of properties)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EAnimatedTransform = R"doc(An animated 4x4 transformation)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EBool = R"doc(Boolean value (true/false))doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EFloat = R"doc(Floating point value)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_ELong = R"doc(64-bit signed integer)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_ENamedReference = R"doc(Named reference to another named object)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EObject = R"doc(Arbitrary object)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EPoint3f = R"doc(3D point)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_ESpectrum = R"doc(Discretized color spectrum)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EString = R"doc(String)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_ETransform = R"doc(4x4 transform for homogeneous coordinates)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EVector3f = R"doc(3D vector)doc";

static const char *__doc_mitsuba_Properties_Properties = R"doc(Construct an empty property container)doc";

static const char *__doc_mitsuba_Properties_Properties_2 = R"doc(Construct an empty property container with a specific plugin name)doc";

static const char *__doc_mitsuba_Properties_Properties_3 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Properties_as_string = R"doc(Return one of the parameters (converting it to a string if necessary))doc";

static const char *__doc_mitsuba_Properties_as_string_2 =
R"doc(Return one of the parameters (converting it to a string if necessary,
with default value))doc";

static const char *__doc_mitsuba_Properties_bool = R"doc(Retrieve a boolean value)doc";

static const char *__doc_mitsuba_Properties_bool_2 = R"doc(Retrieve a boolean value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_copy_attribute =
R"doc(Copy a single attribute from another Properties object and potentially
rename it)doc";

static const char *__doc_mitsuba_Properties_d = R"doc()doc";

static const char *__doc_mitsuba_Properties_float = R"doc(Retrieve a floating point value)doc";

static const char *__doc_mitsuba_Properties_float_2 = R"doc(Retrieve a floating point value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_has_property = R"doc(Verify if a value with the specified name exists)doc";

static const char *__doc_mitsuba_Properties_id =
R"doc(Returns a unique identifier associated with this instance (or an empty
string))doc";

static const char *__doc_mitsuba_Properties_int = R"doc(Retrieve an integer value)doc";

static const char *__doc_mitsuba_Properties_int_2 = R"doc(Retrieve a boolean value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_long = R"doc(Retrieve an integer value)doc";

static const char *__doc_mitsuba_Properties_long_2 = R"doc(Retrieve an integer value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_mark_queried =
R"doc(Manually mark a certain property as queried

Returns:
    ``True`` upon success)doc";

static const char *__doc_mitsuba_Properties_merge =
R"doc(Merge another properties record into the current one.

Existing properties will be overwritten with the values from ``props``
if they have the same name.)doc";

static const char *__doc_mitsuba_Properties_named_reference = R"doc(Retrieve a named reference value)doc";

static const char *__doc_mitsuba_Properties_named_reference_2 =
R"doc(Retrieve a named reference value (use default value if no entry
exists))doc";

static const char *__doc_mitsuba_Properties_named_references = R"doc(Return an array containing all named references and their destinations)doc";

static const char *__doc_mitsuba_Properties_object = R"doc(Retrieve an arbitrary object)doc";

static const char *__doc_mitsuba_Properties_object_2 = R"doc(Retrieve an arbitrary object (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_objects =
R"doc(Return an array containing names and references for all stored objects
and)doc";

static const char *__doc_mitsuba_Properties_operator_assign = R"doc(Assignment operator)doc";

static const char *__doc_mitsuba_Properties_operator_eq = R"doc(Equality comparison operator)doc";

static const char *__doc_mitsuba_Properties_operator_ne = R"doc(Inequality comparison operator)doc";

static const char *__doc_mitsuba_Properties_plugin_name = R"doc(Get the associated plugin name)doc";

static const char *__doc_mitsuba_Properties_point3f = R"doc(Retrieve a 3D point)doc";

static const char *__doc_mitsuba_Properties_point3f_2 = R"doc(Retrieve a 3D point (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_property_names = R"doc(Return an array containing the names of all stored properties)doc";

static const char *__doc_mitsuba_Properties_property_type =
R"doc(Returns the type of an existing property. If no property exists under
that name, an error is logged and type ``void`` is returned.)doc";

static const char *__doc_mitsuba_Properties_remove_property =
R"doc(Remove a property with the specified name

Returns:
    ``True`` upon success)doc";

static const char *__doc_mitsuba_Properties_set_bool = R"doc(Store a boolean value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_float = R"doc(Store a floating point value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_id = R"doc(Set the unique identifier associated with this instance)doc";

static const char *__doc_mitsuba_Properties_set_int = R"doc(Set an integer value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_long = R"doc(Store an integer value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_named_reference = R"doc(Store a named reference in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_object = R"doc(Store an arbitrary object in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_plugin_name = R"doc(Set the associated plugin name)doc";

static const char *__doc_mitsuba_Properties_set_point3f = R"doc(Store a 3D point in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_string = R"doc(Store a string in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_set_transform =
R"doc(Store a 4x4 homogeneous coordinate transformation in the Properties
instance)doc";

static const char *__doc_mitsuba_Properties_set_vector3f = R"doc(Store a 3D vector in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_string = R"doc(Retrieve a string value)doc";

static const char *__doc_mitsuba_Properties_string_2 = R"doc(Retrieve a string value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_transform = R"doc(Retrieve a 4x4 homogeneous coordinate transformation)doc";

static const char *__doc_mitsuba_Properties_transform_2 =
R"doc(Retrieve a 4x4 homogeneous coordinate transformation (use default
value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_unqueried = R"doc(Return the list of un-queried attributed)doc";

static const char *__doc_mitsuba_Properties_vector3f = R"doc(Retrieve a 3D vector)doc";

static const char *__doc_mitsuba_Properties_vector3f_2 = R"doc(Retrieve a 3D vector (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_was_queried = R"doc(Check if a certain property was queried)doc";

static const char *__doc_mitsuba_RadicalInverse =
R"doc(Efficient implementation of a radical inverse function with prime
bases including scrambled versions.

This class is used to implement Halton and Hammersley sequences for
QMC integration in Mitsuba.)doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_divisor = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_recip = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_unused = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_value = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_RadicalInverse =
R"doc(Precompute data structures that are used to evaluate the radical
inverse and scrambled radical inverse function

Parameter ``max_base``:
    Sets the value of the largest prime number base. The default
    interval [2, 8161] contains exactly 1024 prime bases.

Parameter ``scramble``:
    Selects the desired permutation type, where ``-1`` denotes the
    Faure permutations; any other number causes a pseudorandom
    permutation to be built seeded by the value of ``scramble``.)doc";

static const char *__doc_mitsuba_RadicalInverse_base =
R"doc(Returns the n-th prime base used by the sequence

These prime numbers are used as bases in the radical inverse function
implementation.)doc";

static const char *__doc_mitsuba_RadicalInverse_bases =
R"doc(Return the number of prime bases for which precomputed tables are
available)doc";

static const char *__doc_mitsuba_RadicalInverse_class = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_compute_faure_permutations =
R"doc(Compute the Faure permutations using dynamic programming

For reference, see "Good permutations for extreme discrepancy" by
Henri Faure, Journal of Number Theory, Vol. 42, 1, 1992.)doc";

static const char *__doc_mitsuba_RadicalInverse_eval =
R"doc(Calculate the radical inverse function

This function is used as a building block to construct Halton and
Hammersley sequences. Roughly, it computes a b-ary representation of
the input value ``index``, mirrors it along the decimal point, and
returns the resulting fractional value. The implementation here uses
prime numbers for ``b``.

Parameter ``base_index``:
    Selects the n-th prime that is used as a base when computing the
    radical inverse function (0 corresponds to 2, 1->3, 2->5, etc.).
    The value specified here must be between 0 and 1023.

Parameter ``index``:
    Denotes the index that should be mapped through the radical
    inverse function)doc";

static const char *__doc_mitsuba_RadicalInverse_eval_2 = R"doc(Vectorized implementation of eval())doc";

static const char *__doc_mitsuba_RadicalInverse_eval_scrambled =
R"doc(Calculate a scrambled radical inverse function

This function is used as a building block to construct permuted Halton
and Hammersley sequence variants. It works like the normal radical
inverse function eval(), except that every digit is run through an
extra scrambling permutation.)doc";

static const char *__doc_mitsuba_RadicalInverse_eval_scrambled_2 = R"doc(Vectorized implementation of eval_scrambled())doc";

static const char *__doc_mitsuba_RadicalInverse_inverse_permutation =
R"doc(Return the inverse permutation corresponding to the given prime number
basis)doc";

static const char *__doc_mitsuba_RadicalInverse_invert_permutation = R"doc(Invert one of the permutations)doc";

static const char *__doc_mitsuba_RadicalInverse_m_base = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_base_count = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_inv_permutation_storage = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_inv_permutations = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_permutation_storage = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_permutations = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_scramble = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_permutation = R"doc(Return the permutation corresponding to the given prime number basis)doc";

static const char *__doc_mitsuba_RadicalInverse_scramble = R"doc(Return the original scramble value)doc";

static const char *__doc_mitsuba_RadicalInverse_to_string = R"doc(Return a human-readable string representation)doc";

static const char *__doc_mitsuba_Ray =
R"doc(Simple n-dimensional ray segment data structure

Along with the ray origin and direction, this data structure
additionally stores a ray segment [mint, maxt] (whose entries may
include positive/negative infinity), as well as the componentwise
reciprocals of the ray direction. That is just done for convenience,
as these values are frequently required.

Remark:
    Important: be careful when changing the ray direction. You must
    call update() to compute the componentwise reciprocals as well, or
    Mitsuba's ray-triangle intersection code will produce undefined
    results.)doc";

static const char *__doc_mitsuba_RayDifferential =
R"doc(%Ray differential -- enhances the basic ray class with information
about the rays of adjacent pixels on the view plane)doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential_2 = R"doc(Conversion from other Ray types)doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential_3 = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_d_x = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_d_y = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_has_differentials = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_o_x = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_o_y = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_scale = R"doc()doc";

static const char *__doc_mitsuba_Ray_Ray = R"doc(Construct a new ray)doc";

static const char *__doc_mitsuba_Ray_Ray_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Ray_Ray_3 = R"doc(Conversion from other Ray types)doc";

static const char *__doc_mitsuba_Ray_Ray_4 = R"doc(Construct a new ray)doc";

static const char *__doc_mitsuba_Ray_Ray_5 = R"doc(Construct a new ray)doc";

static const char *__doc_mitsuba_Ray_Ray_6 = R"doc(Construct a new ray)doc";

static const char *__doc_mitsuba_Ray_Ray_7 = R"doc(Copy a ray, but change the covered segment of the copy)doc";

static const char *__doc_mitsuba_Ray_d = R"doc(< Ray direction)doc";

static const char *__doc_mitsuba_Ray_d_rcp = R"doc(< Componentwise reciprocals of the ray direction)doc";

static const char *__doc_mitsuba_Ray_maxt = R"doc(< Maximum position on the ray segment)doc";

static const char *__doc_mitsuba_Ray_mint = R"doc(< Minimum position on the ray segment)doc";

static const char *__doc_mitsuba_Ray_o = R"doc(< Ray origin)doc";

static const char *__doc_mitsuba_Ray_operator_call = R"doc(Return the position of a point along the ray)doc";

static const char *__doc_mitsuba_Ray_reverse = R"doc(Return a ray that points into the opposite direction)doc";

static const char *__doc_mitsuba_Ray_time = R"doc(< Time value associated with this ray)doc";

static const char *__doc_mitsuba_Ray_update = R"doc(Update the reciprocal ray directions after changing 'd')doc";

static const char *__doc_mitsuba_Scene = R"doc()doc";

static const char *__doc_mitsuba_Scene_Scene = R"doc()doc";

static const char *__doc_mitsuba_Scene_class = R"doc()doc";

static const char *__doc_mitsuba_Scene_kdtree = R"doc()doc";

static const char *__doc_mitsuba_Scene_kdtree_2 = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_kdtree = R"doc()doc";

static const char *__doc_mitsuba_Scene_to_string = R"doc(Return a human-readable string representation of the scene contents.)doc";

static const char *__doc_mitsuba_Shape = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_ShapeKDTree = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_add_shape = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_bbox = R"doc(Return the bounding box of the i-th primitive)doc";

static const char *__doc_mitsuba_ShapeKDTree_bbox_2 = R"doc(Return the (clipped) bounding box of the i-th primitive)doc";

static const char *__doc_mitsuba_ShapeKDTree_build = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_class = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_find_shape =
R"doc(Map an abstract TShapeKDTree primitive index to a specific shape
managed by the ShapeKDTree.

The function returns the shape index and updates the *idx* parameter
to point to the primitive index (e.g. triangle ID) within the shape.)doc";

static const char *__doc_mitsuba_ShapeKDTree_m_primitive_map = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_m_shapes = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_primitive_count = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_shape = R"doc(Return the i-th shape)doc";

static const char *__doc_mitsuba_ShapeKDTree_shape_2 = R"doc(Return the i-th shape)doc";

static const char *__doc_mitsuba_ShapeKDTree_shape_count = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_to_string = R"doc(Return a human-readable string representation of the scene contents.)doc";

static const char *__doc_mitsuba_Shape_bbox =
R"doc(Return an axis aligned box that bounds all shape primitives (including
any transformations that may have been applied to them))doc";

static const char *__doc_mitsuba_Shape_bbox_2 =
R"doc(Return an axis aligned box that bounds a single shape primitive
(including any transformations that may have been applied to it)

Remark:
    The default implementation simply calls bbox())doc";

static const char *__doc_mitsuba_Shape_bbox_3 =
R"doc(Return an axis aligned box that bounds a single shape primitive after
it has been clipped to another bounding box.

This is extremely important to construct decent kd-trees. The default
implementation just takes the bounding box returned by bbox(Index
index) and clips it to *clip*.)doc";

static const char *__doc_mitsuba_Shape_class = R"doc()doc";

static const char *__doc_mitsuba_Shape_primitive_count =
R"doc(Returns the number of sub-primitives that make up this shape

Remark:
    The default implementation simply returns ``1``)doc";

static const char *__doc_mitsuba_Stream =
R"doc(Abstract seekable stream class

Specifies all functions to be implemented by stream subclasses and
provides various convenience functions layered on top of on them.

All read**X**() and write**X**() methods support transparent
conversion based on the endianness of the underlying system and the
value passed to set_byte_order(). Whenever host_byte_order() and
byte_order() disagree, the endianness is swapped.

See also:
    FileStream, MemoryStream, DummyStream)doc";

static const char *__doc_mitsuba_StreamAppender =
R"doc(%Appender implementation, which writes to an arbitrary C++ output
stream)doc";

static const char *__doc_mitsuba_StreamAppender_StreamAppender =
R"doc(Create a new stream appender

Remark:
    This constructor is not exposed in the Python bindings)doc";

static const char *__doc_mitsuba_StreamAppender_StreamAppender_2 = R"doc(Create a new stream appender logging to a file)doc";

static const char *__doc_mitsuba_StreamAppender_append = R"doc(Append a line of text)doc";

static const char *__doc_mitsuba_StreamAppender_class = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_log_progress = R"doc(Process a progress message)doc";

static const char *__doc_mitsuba_StreamAppender_logs_to_file = R"doc(Does this appender log to a file)doc";

static const char *__doc_mitsuba_StreamAppender_m_fileName = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_is_file = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_last_message_was_progress = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_stream = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_read_log = R"doc(Return the contents of the log file as a string)doc";

static const char *__doc_mitsuba_StreamAppender_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Stream_EByteOrder = R"doc(Defines the byte order (endianness) to use in this Stream)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_EBigEndian = R"doc(< PowerPC, SPARC, Motorola 68K)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_ELittleEndian = R"doc(< x86, x86_64)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_ENetworkByteOrder = R"doc(< Network byte order (an alias for big endian))doc";

static const char *__doc_mitsuba_Stream_Stream =
R"doc(Creates a new stream.

By default, this function sets the stream byte order to that of the
system (i.e. no conversion is performed))doc";

static const char *__doc_mitsuba_Stream_Stream_2 = R"doc(Copying is disallowed.)doc";

static const char *__doc_mitsuba_Stream_byte_order = R"doc(Returns the byte order of this stream.)doc";

static const char *__doc_mitsuba_Stream_can_read = R"doc(Can we read from the stream?)doc";

static const char *__doc_mitsuba_Stream_can_write = R"doc(Can we write to the stream?)doc";

static const char *__doc_mitsuba_Stream_class = R"doc()doc";

static const char *__doc_mitsuba_Stream_close =
R"doc(Closes the stream.

No further read or write operations are permitted.

This function is idempotent. It may be called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_Stream_flush = R"doc(Flushes the stream's buffers, if any)doc";

static const char *__doc_mitsuba_Stream_host_byte_order = R"doc(Returns the byte order of the underlying machine.)doc";

static const char *__doc_mitsuba_Stream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_Stream_m_byte_order = R"doc()doc";

static const char *__doc_mitsuba_Stream_needs_endianness_swap =
R"doc(Returns true if we need to perform endianness swapping before writing
or reading.)doc";

static const char *__doc_mitsuba_Stream_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Stream_read =
R"doc(Reads a specified amount of data from the stream. \note This does
**not** handle endianness swapping.

Throws an exception when the stream ended prematurely. Implementations
need to handle endianness swap when appropriate.)doc";

static const char *__doc_mitsuba_Stream_read_2 =
R"doc(Reads one object of type T from the stream at the current position by
delegating to the appropriate ``serialization_helper``.

Endianness swapping is handled automatically if needed.)doc";

static const char *__doc_mitsuba_Stream_read_line = R"doc(Convenience function for reading a line of text from an ASCII file)doc";

static const char *__doc_mitsuba_Stream_seek =
R"doc(Seeks to a position inside the stream.

Seeking beyond the size of the buffer will not modify the length of
its contents. However, a subsequent write should start at the seeked
position and update the size appropriately.)doc";

static const char *__doc_mitsuba_Stream_set_byte_order =
R"doc(Sets the byte order to use in this stream.

Automatic conversion will be performed on read and write operations to
match the system's native endianness.

No consistency is guaranteed if this method is called after performing
some read and write operations on the system using a different
endianness.)doc";

static const char *__doc_mitsuba_Stream_size = R"doc(Returns the size of the stream)doc";

static const char *__doc_mitsuba_Stream_tell = R"doc(Gets the current position inside the stream)doc";

static const char *__doc_mitsuba_Stream_to_string = R"doc(Returns a human-readable desriptor of the stream)doc";

static const char *__doc_mitsuba_Stream_truncate =
R"doc(Truncates the stream to a given size.

The position is updated to ``min(old_position, size)``. Throws an
exception if in read-only mode.)doc";

static const char *__doc_mitsuba_Stream_write =
R"doc(Writes a specified amount of data into the stream. \note This does
**not** handle endianness swapping.

Throws an exception when not all data could be written.
Implementations need to handle endianness swap when appropriate.)doc";

static const char *__doc_mitsuba_Stream_write_2 =
R"doc(Reads one object of type T from the stream at the current position by
delegating to the appropriate ``serialization_helper``.

Endianness swapping is handled automatically if needed.)doc";

static const char *__doc_mitsuba_Stream_write_line = R"doc(Convenience function for writing a line of text to an ASCII file)doc";

static const char *__doc_mitsuba_Struct =
R"doc(Descriptor for specifying the contents and in-memory layout of a POD-
style data record

Remark:
    The python API provides an additional ``dtype``() method, which
    returns the NumPy ``dtype`` equivalent of a given ``Struct``
    instance.)doc";

static const char *__doc_mitsuba_StructConverter =
R"doc(This class solves the any-to-any problem: effiently converting from
one kind of structured data representation to another

Graphics applications often need to convert from one kind of
structured representation to another. Consider the following data
records which both describe positions tagged with color data.

```
struct Source { // <-- Big endian! :(
   uint8_t r, g, b; // in sRGB
   half x, y, z;
};

struct Target { // <-- Little endian!
   float x, y, z;
   float r, g, b, a; // in linear space
};
```

The record ``Source`` may represent what is stored in a file on disk,
while ``Target`` represents the assumed input of an existing
algorithm. Not only are the formats (e.g. float vs half or uint8_t,
incompatible endianness) and encodings different (e.g. gamma
correction vs linear space), but the second record even has a
different order and extra fields that don't exist in the first one.

This class provides a routine convert() which <ol>

* reorders entries

* converts between many different formats (u[int]8-64, float16-64)

* performs endianness conversion

* applies or removes gamma correction

* optionally checks that certain entries have expected default values

* substitutes missing values with specified defaults

</ol>

On x86_64 platforms, the implementation of this class relies on a JIT
compiler to instantiate a function that efficiently performs the
conversion for any number of elements. The function is cached and
reused if this particular conversion is needed any any later point.

On non-x86_64 platforms, a slow fallback implementation is used.)doc";

static const char *__doc_mitsuba_StructConverter_StructConverter =
R"doc(Construct an optimized conversion routine going from ``source`` to
``target``)doc";

static const char *__doc_mitsuba_StructConverter_class = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_convert = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_m_func = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_m_source = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_m_target = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_source = R"doc(Return the source ``Struct`` descriptor)doc";

static const char *__doc_mitsuba_StructConverter_target = R"doc(Return the target ``Struct`` descriptor)doc";

static const char *__doc_mitsuba_StructConverter_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Struct_EByteOrder = R"doc(Byte order of the fields in the ``Struct``)doc";

static const char *__doc_mitsuba_Struct_EByteOrder_EBigEndian = R"doc()doc";

static const char *__doc_mitsuba_Struct_EByteOrder_EHostByteOrder = R"doc()doc";

static const char *__doc_mitsuba_Struct_EByteOrder_ELittleEndian = R"doc()doc";

static const char *__doc_mitsuba_Struct_EFlags = R"doc(Field-specific flags)doc";

static const char *__doc_mitsuba_Struct_EFlags_EAssert =
R"doc(In FieldConverter::convert, check that the field value matches the
specified default value. Otherwise, return a failure)doc";

static const char *__doc_mitsuba_Struct_EFlags_EDefault =
R"doc(In FieldConverter::convert, when the field is missing in the source
record, replace it by the specified default value)doc";

static const char *__doc_mitsuba_Struct_EFlags_EGamma =
R"doc(Specifies whether the field encodes a sRGB gamma-corrected value.
Assumes ``ENormalized`` is also specified.)doc";

static const char *__doc_mitsuba_Struct_EFlags_ENormalized =
R"doc(Only applies to integer fields: specifies whether the field encodes a
normalized value in the range [0, 1])doc";

static const char *__doc_mitsuba_Struct_EType = R"doc(Type of a field in the ``Struct``)doc";

static const char *__doc_mitsuba_Struct_EType_EFloat = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EFloat16 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EFloat32 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EFloat64 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EInt16 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EInt32 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EInt64 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EInt8 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EUInt16 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EUInt32 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EUInt64 = R"doc()doc";

static const char *__doc_mitsuba_Struct_EType_EUInt8 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field = R"doc(Field specifier with size and offset)doc";

static const char *__doc_mitsuba_Struct_Field_default = R"doc(Default value)doc";

static const char *__doc_mitsuba_Struct_Field_flags = R"doc(Additional flags)doc";

static const char *__doc_mitsuba_Struct_Field_is_float = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_is_integer = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_is_signed = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_is_unsigned = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_name = R"doc(Name of the field)doc";

static const char *__doc_mitsuba_Struct_Field_offset = R"doc(Offset within the ``Struct`` (in bytes))doc";

static const char *__doc_mitsuba_Struct_Field_operator_eq = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_Struct_Field_operator_ne = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_Struct_Field_range = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_size = R"doc(Size in bytes)doc";

static const char *__doc_mitsuba_Struct_Field_type = R"doc(Type identifier)doc";

static const char *__doc_mitsuba_Struct_Struct =
R"doc(Create a new ``Struct`` and indicate whether the contents are packed
or aligned)doc";

static const char *__doc_mitsuba_Struct_Struct_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Struct_alignment = R"doc(Return the alignment (in bytes) of the data structure)doc";

static const char *__doc_mitsuba_Struct_append =
R"doc(Append a new field to the ``Struct``; determines size and offset
automatically)doc";

static const char *__doc_mitsuba_Struct_append_2 = R"doc(Append a new field to the ``Struct`` (manual version))doc";

static const char *__doc_mitsuba_Struct_begin = R"doc(Return an iterator associated with the first field)doc";

static const char *__doc_mitsuba_Struct_begin_2 = R"doc(Return an iterator associated with the first field)doc";

static const char *__doc_mitsuba_Struct_byte_order = R"doc(Return the byte order of the ``Struct``)doc";

static const char *__doc_mitsuba_Struct_class = R"doc()doc";

static const char *__doc_mitsuba_Struct_end = R"doc(Return an iterator associated with the end of the data structure)doc";

static const char *__doc_mitsuba_Struct_end_2 = R"doc(Return an iterator associated with the end of the data structure)doc";

static const char *__doc_mitsuba_Struct_field = R"doc(Look up a field by name (throws an exception if not found))doc";

static const char *__doc_mitsuba_Struct_field_2 = R"doc(Look up a field by name. Throws an exception if not found)doc";

static const char *__doc_mitsuba_Struct_field_count = R"doc(Return the number of fields)doc";

static const char *__doc_mitsuba_Struct_has_field = R"doc(Check if the ``Struct`` has a field of the specified name)doc";

static const char *__doc_mitsuba_Struct_host_byte_order = R"doc(Return the byte order of the host machine)doc";

static const char *__doc_mitsuba_Struct_m_byte_order = R"doc()doc";

static const char *__doc_mitsuba_Struct_m_fields = R"doc()doc";

static const char *__doc_mitsuba_Struct_m_pack = R"doc()doc";

static const char *__doc_mitsuba_Struct_operator_array = R"doc(Access an individual field entry)doc";

static const char *__doc_mitsuba_Struct_operator_array_2 = R"doc(Access an individual field entry)doc";

static const char *__doc_mitsuba_Struct_operator_eq = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_Struct_operator_ne = R"doc(Inequality operator)doc";

static const char *__doc_mitsuba_Struct_size = R"doc(Return the size (in bytes) of the data structure, including padding)doc";

static const char *__doc_mitsuba_Struct_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_SurfaceAreaHeuristic3f = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_empty_space_bonus =
R"doc(Return the bonus factor for empty space used by the tree construction
heuristic)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_eval = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_inner_cost =
R"doc(Evaluate the surface area heuristic

Given a split on axis *axis* at position *split*, compute the
probability of traversing the left and right child during a typical
query operation. In the case of the surface area heuristic, this is
simply the ratio of surface areas.)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_leaf_cost = R"doc(Evaluate the cost of a leaf node)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_m_empty_space_bonus = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_m_query_cost = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_m_temp0 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_m_temp1 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_m_temp2 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_m_traversal_cost = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_query_cost =
R"doc(Return the query cost used by the tree construction heuristic

(This is the average cost for testing a shape against a kd-tree query))doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_set_bounding_box =
R"doc(Initialize the surface area heuristic with the bounds of a parent node

Precomputes some information so that traversal probabilities of
potential split planes can be evaluated efficiently)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3f_traversal_cost =
R"doc(Get the cost of a traversal operation used by the tree construction
heuristic)doc";

static const char *__doc_mitsuba_TPCG32 = R"doc()doc";

static const char *__doc_mitsuba_TPCG32_TPCG32 = R"doc(Initialize the pseudorandom number generator with the seed() function)doc";

static const char *__doc_mitsuba_TPCG32_advance =
R"doc(Multi-step advance function (jump-ahead, jump-back)

The method used here is based on Brown, "Random Number Generation with
Arbitrary Stride", Transactions of the American Nuclear Society (Nov.
1994). The algorithm is very similar to fast exponentiation.)doc";

static const char *__doc_mitsuba_TPCG32_inc = R"doc()doc";

static const char *__doc_mitsuba_TPCG32_next_float = R"doc()doc";

static const char *__doc_mitsuba_TPCG32_next_float32 =
R"doc(Generate a single precision floating point value on the interval [0,
1))doc";

static const char *__doc_mitsuba_TPCG32_next_float64 =
R"doc(Generate a double precision floating point value on the interval [0,
1)

Remark:
    Since the underlying random number generator produces 32 bit
    output, only the first 32 mantissa bits will be filled (however,
    the resolution is still finer than in next_float(), which only
    uses 23 mantissa bits))doc";

static const char *__doc_mitsuba_TPCG32_next_uint32 = R"doc(Generate a uniformly distributed unsigned 32-bit random number)doc";

static const char *__doc_mitsuba_TPCG32_next_uint32_2 = R"doc(Generate a uniformly distributed number, r, where 0 <= r < bound)doc";

static const char *__doc_mitsuba_TPCG32_next_uint32_masked =
R"doc(Generate a uniformly distributed unsigned 32-bit random number

Remark:
    Masked version: only activates some of the RNGs)doc";

static const char *__doc_mitsuba_TPCG32_operator_eq = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_TPCG32_operator_ne = R"doc(Inequality operator)doc";

static const char *__doc_mitsuba_TPCG32_operator_sub = R"doc(Compute the distance between two PCG32 pseudorandom number generators)doc";

static const char *__doc_mitsuba_TPCG32_seed =
R"doc(Seed the pseudorandom number generator

Specified in two parts: a state initializer and a sequence selection
constant (a.k.a. stream id))doc";

static const char *__doc_mitsuba_TPCG32_shuffle =
R"doc(Draw uniformly distributed permutation and permute the given STL
container

From: Knuth, TAoCP Vol. 2 (3rd 3d), Section 3.4.2)doc";

static const char *__doc_mitsuba_TPCG32_state = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree =
R"doc(Optimized KD-tree acceleration data structure for n-dimensional (n<=4)
shapes and various queries involving them.

Note that this class mainly concerns itself with primitives that cover
*a region* of space. For point data, other implementations will be
more suitable. The most important application in Mitsuba is the fast
construction of high-quality trees for ray tracing. See the class
ShapeKDTree for this specialization.

The code in this class is a fully generic kd-tree implementation,
which can theoretically support any kind of shape. However, subclasses
still need to provide the following signatures for a functional
implementation:

```
/// Return the total number of primitives
Size primitive_count() const;

/// Return the axis-aligned bounding box of a certain primitive
BoundingBox bbox(Index primIdx) const;

/// Return the bounding box of a primitive when clipped to another bounding box
BoundingBox bbox(Index primIdx, const BoundingBox &aabb) const;
```

This class follows the "Curiously recurring template" design pattern
so that the above functions can be inlined (in particular, no virtual
calls will be necessary!).

When the kd-tree is initially built, this class optimizes a cost
heuristic every time a split plane has to be chosen. For ray tracing,
the heuristic is usually the surface area heuristic (SAH), but other
choices are possible as well. The tree cost model must be passed as a
template argument, which can use a supplied bounding box and split
candidate to compute approximate probabilities of recursing into the
left and right subrees during a typical kd-tree query operation. See
SurfaceAreaHeuristic3f for an example of the interface that must be
implemented.

The kd-tree construction algorithm creates 'perfect split' trees as
outlined in the paper "On Building fast kd-Trees for Ray Tracing, and
on doing that in O(N log N)" by Ingo Wald and Vlastimil Havran. This
works even when the tree is not meant to be used for ray tracing. For
polygonal meshes, the involved Sutherland-Hodgman iterations can be
quite expensive in terms of the overall construction time. The
set_clip_primitives() method can be used to deactivate perfect splits
at the cost of a lower-quality tree.

Because the O(N log N) construction algorithm tends to cause many
incoherent memory accesses and does not parallelize particularly well,
a different method known as *Min-Max Binning* is used for the top
levels of the tree. Min-Max-binning is an approximation to the O(N log
N) approach, which works extremely well at the top of the tree (i.e.
when there are many elements). This algorithm realized as a series of
efficient parallel sweeps that harness the available cores at all
levels (even at the root node). Each iteration splits the list of
primitives into independent subtrees which can also be processed in
parallel. Eventually, the input data is reduced into sufficiently
small chunks, at which point the implementation switches over to the
more accurate O(N log N) builder. The various thresholds and
parameters for these different methods can be accessed and configured
via getters and setters of this class.)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext =
R"doc(Helper data structure used during tree construction (shared by all
threads))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_BuildContext = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_bad_refines = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_derived = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_exp_leaves_visited = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_exp_primitives_queried = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_exp_traversal_steps = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_index_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_local = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_max_depth = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_max_prims_in_leaf = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_node_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_nonempty_leaf_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_prim_buckets = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_pruned = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_retracted_splits = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_temp_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_thread = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_work_units = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask =
R"doc(TBB task for building subtrees in parallel

This class is responsible for building a subtree of the final kd-tree.
It recursively spawns new tasks for its respective subtrees to enable
parallel construction.

At the top of the tree, it uses min-max-binning and parallel
reductions to create sufficient parallelism. When the number of
elements is sufficiently small, it switches to a more accurate O(N log
N) builder which uses normal recursion on the stack (i.e. it does not
spawn further parallel pieces of work).)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_BuildTask = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_build_nlogn = R"doc(Recursively run the O(N log N builder))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_execute = R"doc(Run one iteration of min-max binning and spawn recursive tasks)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_bad_refines = R"doc(Number of "bad refines" so far)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_bbox = R"doc(Bounding box of the node)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_cost = R"doc(This scalar should be set to the final cost when done)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_ctx = R"doc(Context with build-specific variables (shared by all threads/tasks))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_depth = R"doc(Depth of the node within the tree)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_indices = R"doc(Index list of primitives to be organized)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_local = R"doc(Local context with thread local variables)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_node = R"doc(Node to be initialized by this task)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_tight_bbox = R"doc(Tighter bounding box of the contained primitives)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_make_leaf =
R"doc(Create a leaf node using the given set of indices (called by min-max
binning))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_make_leaf_2 =
R"doc(Create a leaf node using the given edge event list (called by the O(N
log N) builder))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_transition_to_nlogn =
R"doc(Create an initial sorted edge event list and start the O(N log N)
builder)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_traverse =
R"doc(Traverse a subtree and collect all encountered primitive references in
a set)doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage =
R"doc(Compact storage for primitive classification

When classifying primitives with respect to a split plane, a data
structure is needed to hold the tertiary result of this operation.
This class implements a compact storage (2 bits per entry) in the
spirit of the std::vector<bool> specialization.)doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_get = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_m_buffer = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_m_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_resize = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_set = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_size = R"doc(Return the size (in bytes))doc";

static const char *__doc_mitsuba_TShapeKDTree_EPrimClassification =
R"doc(Enumeration representing the state of a classified primitive in the
O(N log N) builder)doc";

static const char *__doc_mitsuba_TShapeKDTree_EPrimClassification_EBoth = R"doc(< Primitive straddles the split plane)doc";

static const char *__doc_mitsuba_TShapeKDTree_EPrimClassification_EIgnore = R"doc(< Primitive was handled already, ignore from now on)doc";

static const char *__doc_mitsuba_TShapeKDTree_EPrimClassification_ELeft = R"doc(< Primitive is left of the split plane)doc";

static const char *__doc_mitsuba_TShapeKDTree_EPrimClassification_ERight = R"doc(< Primitive is right of the split plane)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent =
R"doc(Describes the beginning or end of a primitive under orthogonal
projection onto different axes)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EEventType = R"doc(Possible event types)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EEventType_EEdgeEnd = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EEventType_EEdgePlanar = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EEventType_EEdgeStart = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EdgeEvent = R"doc(Dummy constructor)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EdgeEvent_2 = R"doc(Create a new edge event)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_axis = R"doc(Event axis)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_index = R"doc(Primitive index)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_operator_lt = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_pos = R"doc(Plane position)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_set_invalid = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_type = R"doc(Event type: end/planar/start)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_valid = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode = R"doc(kd-tree node in 8 bytes.)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_axis = R"doc(Return the split axis (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_data = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_leaf = R"doc(Is this a leaf node?)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_left = R"doc(Return the left child (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_left_offset =
R"doc(Assuming that this is an inner node, return the relative offset to the
left child)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_primitive_count = R"doc(Assuming this is a leaf node, return the number of primitives)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_primitive_offset = R"doc(Assuming this is a leaf node, return the first primitive index)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_right = R"doc(Return the left child (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_set_inner_node =
R"doc(Initialize an interior kd-tree node.

Returns ``False`` if the offset or number of primitives is so large
that it can't be represented)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_set_leaf_node =
R"doc(Initialize a leaf kd-tree node.

Returns ``False`` if the offset or number of primitives is so large
that it can't be represented)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_split = R"doc(Return the split plane location (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext =
R"doc(Helper data structure used during tree construction (used by a single
thread))doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_classification_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_ctx = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_left_alloc = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_right_alloc = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins =
R"doc(Min-max binning data structure with parallel binning & partitioning
steps

See "Highly Parallel Fast KD-tree Construction for Interactive Ray
Tracing of Dynamic Scenes" by M. Shevtsov, A. Soupikov and A. Kapustin)doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_MinMaxBins = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_MinMaxBins_2 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_MinMaxBins_3 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_left_bounds = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_left_indices = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_right_bounds = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_right_indices = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_best_candidate = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_bin_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_bins = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_inv_bin_size = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_max_bin = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_operator_iadd = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_partition =
R"doc(Given a suitable split candidate, compute tight bounding boxes for the
left and right subtrees and return associated primitive lists.)doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_put = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator =
R"doc(During kd-tree construction, large amounts of memory are required to
temporarily hold index and edge event lists. When not implemented
properly, these allocations can become a critical bottleneck. The
class OrderedChunkAllocator provides a specialized memory allocator,
which reserves memory in chunks of at least 512KiB (this number is
configurable). An important assumption made by the allocator is that
memory will be released in the exact same order in which it was
previously allocated. This makes it possible to create an
implementation with a very low memory overhead. Note that no locking
is done, hence each thread will need its own allocator.)doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_Chunk = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_contains = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_cur = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_remainder = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_size = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_start = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_Chunk_used = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_OrderedChunkAllocator = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_allocate =
R"doc(Request a block of memory from the allocator

Walks through the list of chunks to find one with enough free memory.
If no chunk could be found, a new one is created.)doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_chunk_count = R"doc(Return the currently allocated number of chunks)doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_m_chunks = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_m_min_allocation = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_release = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_shrink_allocation = R"doc(Shrink the size of the last allocated chunk)doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_size = R"doc(Return the total amount of chunk memory in bytes)doc";

static const char *__doc_mitsuba_TShapeKDTree_OrderedChunkAllocator_used = R"doc(Return the total amount of used memory in bytes)doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate = R"doc(Data type for split candidates suggested by the tree cost model)doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_axis = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_cost = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_left_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_planar_left = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_right_bin = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_right_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_split = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_TShapeKDTree = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_build = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_class = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_clip_primitives = R"doc(Return whether primitive clipping is used during tree construction)doc";

static const char *__doc_mitsuba_TShapeKDTree_compute_statistics = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_cost_model = R"doc(Return the cost model used by the tree construction algorithm)doc";

static const char *__doc_mitsuba_TShapeKDTree_derived = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_derived_2 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_exact_primitive_threshold =
R"doc(Return the number of primitives, at which the builder will switch from
(approximate) Min-Max binning to the accurate O(n log n) optimization
method.)doc";

static const char *__doc_mitsuba_TShapeKDTree_log_level = R"doc(Return the log level of kd-tree status messages)doc";

static const char *__doc_mitsuba_TShapeKDTree_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_clip_primitives = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_cost_model = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_exact_prim_threshold = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_index_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_indices = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_log_level = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_max_bad_refines = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_max_depth = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_min_max_bins = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_node_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_nodes = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_retract_bad_splits = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_stop_primitives = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_max_bad_refines =
R"doc(Return the number of bad refines allowed to happen in succession
before a leaf node will be created.)doc";

static const char *__doc_mitsuba_TShapeKDTree_max_depth = R"doc(Return the maximum tree depth (0 == use heuristic))doc";

static const char *__doc_mitsuba_TShapeKDTree_min_max_bins = R"doc(Return the number of bins used for Min-Max binning)doc";

static const char *__doc_mitsuba_TShapeKDTree_ready = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_retract_bad_splits = R"doc(Return whether or not bad splits can be "retracted".)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_clip_primitives = R"doc(Set whether primitive clipping is used during tree construction)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_exact_primitive_threshold =
R"doc(Specify the number of primitives, at which the builder will switch
from (approximate) Min-Max binning to the accurate O(n log n)
optimization method.)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_log_level = R"doc(Return the log level of kd-tree status messages)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_max_bad_refines =
R"doc(Set the number of bad refines allowed to happen in succession before a
leaf node will be created.)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_max_depth = R"doc(Set the maximum tree depth (0 == use heuristic))doc";

static const char *__doc_mitsuba_TShapeKDTree_set_min_max_bins = R"doc(Set the number of bins used for Min-Max binning)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_retract_bad_splits = R"doc(Specify whether or not bad splits can be "retracted".)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_stop_primitives =
R"doc(Set the number of primitives, at which recursion will stop when
building the tree.)doc";

static const char *__doc_mitsuba_TShapeKDTree_stop_primitives =
R"doc(Return the number of primitives, at which recursion will stop when
building the tree.)doc";

static const char *__doc_mitsuba_Thread =
R"doc(Cross-platform thread implementation

Mitsuba threads are internally implemented via the ``std::thread``
class defined in C++11. This wrapper class is needed to attach
additional state (Loggers, Path resolvers, etc.) that is inherited
when a thread launches another thread.)doc";

static const char *__doc_mitsuba_ThreadEnvironment =
R"doc(RAII-style class to temporarily switch to another thread's logger/file
resolver)doc";

static const char *__doc_mitsuba_ThreadEnvironment_ThreadEnvironment = R"doc()doc";

static const char *__doc_mitsuba_ThreadEnvironment_m_file_resolver = R"doc()doc";

static const char *__doc_mitsuba_ThreadEnvironment_m_logger = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocal =
R"doc(Flexible platform-independent thread local storage class

This class implements a generic thread local storage object. For
details, refer to its base class, ThreadLocalBase.)doc";

static const char *__doc_mitsuba_ThreadLocalBase =
R"doc(Flexible platform-independent thread local storage class

This class implements a generic thread local storage object that can
be used in situations where the new ``thread_local`` keyword is not
available (e.g. on Mac OS, as of 2016), or when TLS object are created
dynamically (which ``thread_local`` does not allow).

The native TLS classes on Linux/MacOS/Windows only support a limited
number of dynamically allocated entries (usually 1024 or 1088).
Furthermore, they do not provide appropriate cleanup semantics when
the TLS object or one of the assocated threads dies. The custom TLS
code provided by this class has no such limits (caching in various
subsystems of Mitsuba may create a huge amount, so this is a big
deal), and it also has the desired cleanup semantics: TLS entries are
destroyed when the owning thread dies *or* when the ``ThreadLocal``
instance is freed (whichever occurs first).

The implementation is designed to make the ``get``() operation as fast
as as possible at the cost of more involved locking when creating or
destroying threads and TLS objects. To actually instantiate a TLS
object with a specific type, use the ThreadLocal class.

See also:
    ThreadLocal)doc";

static const char *__doc_mitsuba_ThreadLocalBase_ThreadLocalBase = R"doc(Construct a new thread local storage object)doc";

static const char *__doc_mitsuba_ThreadLocalBase_clear =
R"doc(Release all current instances associated with this TLS

Dangerous: don't use this method when the data is still concurrently
being used by other threads)doc";

static const char *__doc_mitsuba_ThreadLocalBase_get = R"doc(Return the data value associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocalBase_get_2 =
R"doc(Return the data value associated with the current thread (const
version))doc";

static const char *__doc_mitsuba_ThreadLocalBase_m_constructFunctor = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocalBase_m_destructFunctor = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocalBase_register_thread =
R"doc(A new thread was started -- set up local TLS data structures. Returns
``True`` upon success)doc";

static const char *__doc_mitsuba_ThreadLocalBase_static_initialization = R"doc(Set up core data structures for TLS management)doc";

static const char *__doc_mitsuba_ThreadLocalBase_static_shutdown = R"doc(Destruct core data structures for TLS management)doc";

static const char *__doc_mitsuba_ThreadLocalBase_unregister_thread =
R"doc(A thread has died -- destroy any remaining TLS entries associated with
it)doc";

static const char *__doc_mitsuba_ThreadLocal_ThreadLocal = R"doc(Construct a new thread local storage object)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_T0 = R"doc(Return a reference to the data associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_assign = R"doc(Update the data associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_const_T0 =
R"doc(Return a reference to the data associated with the current thread
(const version))doc";

static const char *__doc_mitsuba_Thread_EPriority = R"doc(Possible priority values for Thread::set_priority())doc";

static const char *__doc_mitsuba_Thread_EPriority_EHighPriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_EPriority_EHighestPriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_EPriority_EIdlePriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_EPriority_ELowPriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_EPriority_ELowestPriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_EPriority_ENormalPriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_EPriority_ERealtimePriority = R"doc()doc";

static const char *__doc_mitsuba_Thread_Thread =
R"doc(Create a new thread object

Parameter ``name``:
    An identifying name of this thread (will be shown in debug
    messages))doc";

static const char *__doc_mitsuba_Thread_class = R"doc()doc";

static const char *__doc_mitsuba_Thread_core_affinity = R"doc(Return the core affinity)doc";

static const char *__doc_mitsuba_Thread_d = R"doc()doc";

static const char *__doc_mitsuba_Thread_detach =
R"doc(Detach the thread and release resources

After a call to this function, join() cannot be used anymore. This
releases resources, which would otherwise be held until a call to
join().)doc";

static const char *__doc_mitsuba_Thread_dispatch = R"doc(Initialize thread execution environment and then call run())doc";

static const char *__doc_mitsuba_Thread_exit = R"doc(Exit the thread, should be called from inside the thread)doc";

static const char *__doc_mitsuba_Thread_file_resolver = R"doc(Return the file resolver associated with the current thread)doc";

static const char *__doc_mitsuba_Thread_file_resolver_2 = R"doc(Return the parent thread (const version))doc";

static const char *__doc_mitsuba_Thread_id = R"doc(Return a unique ID that is associated with this thread)doc";

static const char *__doc_mitsuba_Thread_is_critical = R"doc(Return the value of the critical flag)doc";

static const char *__doc_mitsuba_Thread_is_running = R"doc(Is this thread still running?)doc";

static const char *__doc_mitsuba_Thread_join = R"doc(Wait until the thread finishes)doc";

static const char *__doc_mitsuba_Thread_logger = R"doc(Return the thread's logger instance)doc";

static const char *__doc_mitsuba_Thread_name = R"doc(Return the name of this thread)doc";

static const char *__doc_mitsuba_Thread_parent = R"doc(Return the parent thread)doc";

static const char *__doc_mitsuba_Thread_parent_2 = R"doc(Return the parent thread (const version))doc";

static const char *__doc_mitsuba_Thread_priority = R"doc(Return the thread priority)doc";

static const char *__doc_mitsuba_Thread_run = R"doc(The thread's run method)doc";

static const char *__doc_mitsuba_Thread_set_core_affinity =
R"doc(Set the core affinity

This function provides a hint to the operating system scheduler that
the thread should preferably run on the specified processor core. By
default, the parameter is set to -1, which means that there is no
affinity.)doc";

static const char *__doc_mitsuba_Thread_set_critical =
R"doc(Specify whether or not this thread is critical

When an thread marked critical crashes from an uncaught exception, the
whole process is brought down. The default is ``False``.)doc";

static const char *__doc_mitsuba_Thread_set_file_resolver = R"doc(Set the file resolver associated with the current thread)doc";

static const char *__doc_mitsuba_Thread_set_logger = R"doc(Set the logger instance used to process log messages from this thread)doc";

static const char *__doc_mitsuba_Thread_set_name = R"doc(Set the name of this thread)doc";

static const char *__doc_mitsuba_Thread_set_priority =
R"doc(Set the thread priority

This does not always work -- for instance, Linux requires root
privileges for this operation.

Returns:
    ``True`` upon success.)doc";

static const char *__doc_mitsuba_Thread_sleep = R"doc(Sleep for a certain amount of time (in milliseconds))doc";

static const char *__doc_mitsuba_Thread_start = R"doc(Start the thread)doc";

static const char *__doc_mitsuba_Thread_static_initialization = R"doc(Initialize the threading system)doc";

static const char *__doc_mitsuba_Thread_static_shutdown = R"doc(Shut down the threading system)doc";

static const char *__doc_mitsuba_Thread_thread = R"doc(Return the current thread)doc";

static const char *__doc_mitsuba_Thread_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Thread_yield = R"doc(Yield to another processor)doc";

static const char *__doc_mitsuba_Timer = R"doc()doc";

static const char *__doc_mitsuba_Timer_Timer = R"doc()doc";

static const char *__doc_mitsuba_Timer_begin_stage = R"doc()doc";

static const char *__doc_mitsuba_Timer_end_stage = R"doc()doc";

static const char *__doc_mitsuba_Timer_reset = R"doc()doc";

static const char *__doc_mitsuba_Timer_start = R"doc()doc";

static const char *__doc_mitsuba_Timer_value = R"doc()doc";

static const char *__doc_mitsuba_Transform =
R"doc(Encapsulates a 4x4 homogeneous coordinate transformation and its
inverse

This class provides a set of overloaded matrix-vector multiplication
operators for vectors, points, and normals (all of them transform
differently under homogeneous coordinate transformations))doc";

static const char *__doc_mitsuba_Transform_Transform = R"doc(Create the identity transformation)doc";

static const char *__doc_mitsuba_Transform_Transform_2 =
R"doc(Initialize the transformation from the given matrix (and compute its
inverse))doc";

static const char *__doc_mitsuba_Transform_Transform_3 = R"doc(Initialize the transformation from the given matrix and inverse)doc";

static const char *__doc_mitsuba_Transform_inverse =
R"doc(Compute the inverse of this transformation (for free, since it is
already known))doc";

static const char *__doc_mitsuba_Transform_inverse_matrix = R"doc(Return the underlying 4x4 inverse matrix)doc";

static const char *__doc_mitsuba_Transform_look_at =
R"doc(Create a look-at camera transformation

Parameter ``p``:
    Camera position

Parameter ``t``:
    Target vector

Parameter ``u``:
    Up vector)doc";

static const char *__doc_mitsuba_Transform_m_inverse = R"doc()doc";

static const char *__doc_mitsuba_Transform_m_value = R"doc()doc";

static const char *__doc_mitsuba_Transform_matrix = R"doc(Return the underlying 4x4 matrix)doc";

static const char *__doc_mitsuba_Transform_operator_eq = R"doc(Equality comparison operator)doc";

static const char *__doc_mitsuba_Transform_operator_mul = R"doc(Concatenate transformations)doc";

static const char *__doc_mitsuba_Transform_operator_mul_2 = R"doc(Transform a 3D point)doc";

static const char *__doc_mitsuba_Transform_operator_mul_3 = R"doc(Transform a 3D vector)doc";

static const char *__doc_mitsuba_Transform_operator_mul_4 = R"doc(Transform a 3D normal)doc";

static const char *__doc_mitsuba_Transform_operator_mul_5 = R"doc(Transform a ray (for affine/non-perspective transformations))doc";

static const char *__doc_mitsuba_Transform_operator_ne = R"doc(Inequality comparison operator)doc";

static const char *__doc_mitsuba_Transform_orthographic =
R"doc(Create an orthographic transformation, which maps Z to [0,1] and
leaves the X and Y coordinates untouched.

Parameter ``clip_near``:
    Near clipping plane

Parameter ``clip_far``:
    Far clipping plane)doc";

static const char *__doc_mitsuba_Transform_perspective =
R"doc(Create a perspective transformation. (Maps [near, far] to [0, 1])

Parameter ``fov``:
    Field of view in degrees

Parameter ``clip_near``:
    Near clipping plane

Parameter ``clip_far``:
    Far clipping plane)doc";

static const char *__doc_mitsuba_Transform_rotate =
R"doc(Create a rotation transformation around an arbitrary axis. The angle
is specified in degrees)doc";

static const char *__doc_mitsuba_Transform_scale = R"doc(Create a scale transformation)doc";

static const char *__doc_mitsuba_Transform_transform_affine = R"doc(Transform a 3D point (for affine/non-perspective transformations))doc";

static const char *__doc_mitsuba_Transform_transform_affine_2 = R"doc(Transform a 3D vector (for affine/non-perspective transformations))doc";

static const char *__doc_mitsuba_Transform_transform_affine_3 = R"doc(Transform a 3D normal (for affine/non-perspective transformations))doc";

static const char *__doc_mitsuba_Transform_transform_affine_4 = R"doc(Transform a ray (for affine/non-perspective transformations))doc";

static const char *__doc_mitsuba_Transform_translate = R"doc(Create a translation transformation)doc";

static const char *__doc_mitsuba_Vector = R"doc(//! @{ \name Elementary vector, point, and normal data types)doc";

static const char *__doc_mitsuba_Vector_Vector = R"doc()doc";

static const char *__doc_mitsuba_Vector_Vector_2 = R"doc()doc";

static const char *__doc_mitsuba_Vector_Vector_3 = R"doc()doc";

static const char *__doc_mitsuba_Vector_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Vector_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_ZStream =
R"doc(Transparent compression/decompression stream based on ``zlib``.

This class transparently decompresses and compresses reads and writes
to a nested stream, respectively.)doc";

static const char *__doc_mitsuba_ZStream_EStreamType = R"doc()doc";

static const char *__doc_mitsuba_ZStream_EStreamType_EDeflateStream = R"doc(< A raw deflate stream)doc";

static const char *__doc_mitsuba_ZStream_EStreamType_EGZipStream = R"doc(< A gzip-compatible stream)doc";

static const char *__doc_mitsuba_ZStream_ZStream =
R"doc(Creates a new compression stream with the given underlying stream.
This new instance takes ownership of the child stream. The child
stream must outlive the ZStream.)doc";

static const char *__doc_mitsuba_ZStream_can_read = R"doc(Can we read from the stream?)doc";

static const char *__doc_mitsuba_ZStream_can_write = R"doc(Can we write to the stream?)doc";

static const char *__doc_mitsuba_ZStream_child_stream = R"doc(Returns the child stream of this compression stream)doc";

static const char *__doc_mitsuba_ZStream_child_stream_2 = R"doc(Returns the child stream of this compression stream)doc";

static const char *__doc_mitsuba_ZStream_class = R"doc()doc";

static const char *__doc_mitsuba_ZStream_close =
R"doc(Closes the stream, but not the underlying child stream. No further
read or write operations are permitted.

This function is idempotent. It is called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_ZStream_flush = R"doc(Flushes any buffered data)doc";

static const char *__doc_mitsuba_ZStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_ZStream_m_child_stream = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_deflate_buffer = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_deflate_stream = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_didWrite = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_inflate_buffer = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_inflate_stream = R"doc()doc";

static const char *__doc_mitsuba_ZStream_read =
R"doc(Reads a specified amount of data from the stream, decompressing it
first using ZLib. Throws an exception when the stream ended
prematurely.)doc";

static const char *__doc_mitsuba_ZStream_seek = R"doc(Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_size = R"doc(Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_tell = R"doc(Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_to_string = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_ZStream_truncate = R"doc(/ Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_write =
R"doc(Writes a specified amount of data into the stream, compressing it
first using ZLib. Throws an exception when not all data could be
written.)doc";

static const char *__doc_mitsuba_cie1931_xyz =
R"doc(Compute the CIE 1931 XYZ color matching functions given a wavelength
in nanometers

Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley Journal
of Computer Graphics Techniques Vol 2, No 2, 2013)doc";

static const char *__doc_mitsuba_cie1931_y =
R"doc(Compute the CIE 1931 Y color matching function given a wavelength in
nanometers

Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley Journal
of Computer Graphics Techniques Vol 2, No 2, 2013)doc";

static const char *__doc_mitsuba_class = R"doc()doc";

static const char *__doc_mitsuba_comparator = R"doc()doc";

static const char *__doc_mitsuba_comparator_operator_call = R"doc()doc";

static const char *__doc_mitsuba_coordinate_system = R"doc(Complete the set {a} to an orthonormal basis {a, b, c})doc";

static const char *__doc_mitsuba_detail_Log = R"doc()doc";

static const char *__doc_mitsuba_detail_Throw = R"doc()doc";

static const char *__doc_mitsuba_detail_get_construct_functor = R"doc()doc";

static const char *__doc_mitsuba_detail_get_construct_functor_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_get_unserialize_functor = R"doc()doc";

static const char *__doc_mitsuba_detail_get_unserialize_functor_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_copy = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_destruct = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_equals = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_move = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_visit = R"doc()doc";

static const char *__doc_mitsuba_filesystem_absolute =
R"doc(Returns an absolute path to the same location pointed by ``p``,
relative to ``base``.

See also:
    http ://en.cppreference.com/w/cpp/experimental/fs/absolute))doc";

static const char *__doc_mitsuba_filesystem_create_directory =
R"doc(Creates a directory at ``p`` as if ``mkdir`` was used. Returns true if
directory creation was successful, false otherwise. If ``p`` already
exists and is already a directory, the function does nothing (this
condition is not treated as an error).)doc";

static const char *__doc_mitsuba_filesystem_current_path = R"doc(Returns the current working directory (equivalent to getcwd))doc";

static const char *__doc_mitsuba_filesystem_equivalent =
R"doc(Checks whether two paths refer to the same file system object. Both
must refer to an existing file or directory. Symlinks are followed to
determine equivalence.)doc";

static const char *__doc_mitsuba_filesystem_exists = R"doc(Checks if ``p`` points to an existing filesystem object.)doc";

static const char *__doc_mitsuba_filesystem_file_size =
R"doc(Returns the size (in bytes) of a regular file at ``p``. Attempting to
determine the size of a directory (as well as any other file that is
not a regular file or a symlink) is treated as an error.)doc";

static const char *__doc_mitsuba_filesystem_is_directory = R"doc(Checks if ``p`` points to a directory.)doc";

static const char *__doc_mitsuba_filesystem_is_regular_file =
R"doc(Checks if ``p`` points to a regular file, as opposed to a directory or
symlink.)doc";

static const char *__doc_mitsuba_filesystem_path =
R"doc(Represents a path to a filesystem resource. On construction, the path
is parsed and stored in a system-agnostic representation. The path can
be converted back to the system-specific string using ``native()`` or
``string()``.)doc";

static const char *__doc_mitsuba_filesystem_path_clear = R"doc(Makes the path an empty path. An empty path is considered relative.)doc";

static const char *__doc_mitsuba_filesystem_path_empty = R"doc(Checks if the path is empty)doc";

static const char *__doc_mitsuba_filesystem_path_extension =
R"doc(Returns the extension of the filename component of the path (the
substring starting at the rightmost period, including the period).
Special paths '.' and '..' have an empty extension.)doc";

static const char *__doc_mitsuba_filesystem_path_filename = R"doc(Returns the filename component of the path, including the extension.)doc";

static const char *__doc_mitsuba_filesystem_path_is_absolute = R"doc(Checks if the path is absolute.)doc";

static const char *__doc_mitsuba_filesystem_path_is_relative = R"doc(Checks if the path is relative.)doc";

static const char *__doc_mitsuba_filesystem_path_m_absolute = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_m_path = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_native =
R"doc(Returns the path in the form of a native string, so that it can be
passed directly to system APIs. The path is constructed using the
system's preferred separator and the native string type.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign = R"doc(Assignment operator.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign_2 = R"doc(Move assignment operator.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign_3 =
R"doc(Assignment from the system's native string type. Acts similarly to the
string constructor.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_basic_string =
R"doc(Implicit conversion operator to the basic_string corresponding to the
system's character type. Equivalent to calling ``native()``.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_div = R"doc(Concatenates two paths with a directory separator.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_eq =
R"doc(Equality operator. Warning: this only checks for lexicographic
equivalence. To check whether two paths point to the same filesystem
resource, use ``equivalent``.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_ne = R"doc(Inequality operator.)doc";

static const char *__doc_mitsuba_filesystem_path_parent_path =
R"doc(Returns the path to the parent directory. Returns an empty path if it
is already empty or if it has only one element.)doc";

static const char *__doc_mitsuba_filesystem_path_path =
R"doc(Default constructor. Constructs an empty path. An empty path is
considered relative.)doc";

static const char *__doc_mitsuba_filesystem_path_path_2 = R"doc(Copy constructor.)doc";

static const char *__doc_mitsuba_filesystem_path_path_3 = R"doc(Move constructor.)doc";

static const char *__doc_mitsuba_filesystem_path_path_4 =
R"doc(Construct a path from a string with native type. On Windows, the path
can use both '/' or '\\' as a delimiter.)doc";

static const char *__doc_mitsuba_filesystem_path_path_5 =
R"doc(Construct a path from a string with native type. On Windows, the path
can use both '/' or '\\' as a delimiter.)doc";

static const char *__doc_mitsuba_filesystem_path_replace_extension =
R"doc(Replaces the substring starting at the rightmost '.' symbol by the
provided string. A '.' symbol is automatically inserted if the
replacement does not start with a dot.

Removes the extension altogether if the empty path is passed. If there
is no extension, appends a '.' followed by the replacement.

If the path is empty, '.' or '..': does nothing.

Returns *this.)doc";

static const char *__doc_mitsuba_filesystem_path_set = R"doc(Builds a path from the passed string.)doc";

static const char *__doc_mitsuba_filesystem_path_str = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_string = R"doc(Equivalent to native(), converted to the std::string type)doc";

static const char *__doc_mitsuba_filesystem_path_tokenize =
R"doc(Splits a string into tokens delimited by any of the characters passed
in ``delim``.)doc";

static const char *__doc_mitsuba_filesystem_remove =
R"doc(Removes a file or empty directory. Returns true if removal was
successful, false if there was an error (e.g. the file did not exist).)doc";

static const char *__doc_mitsuba_filesystem_resize_file =
R"doc(Changes the size of the regular file named by ``p`` as if ``truncate``
was called. If the file was larger than ``target_length``, the
remainder is discarded. The file must exist.)doc";

static const char *__doc_mitsuba_for_each_type = R"doc(Base case)doc";

static const char *__doc_mitsuba_for_each_type_recurse = R"doc()doc";

static const char *__doc_mitsuba_hash = R"doc()doc";

static const char *__doc_mitsuba_hash_2 = R"doc()doc";

static const char *__doc_mitsuba_hash_3 = R"doc()doc";

static const char *__doc_mitsuba_hash_4 = R"doc()doc";

static const char *__doc_mitsuba_hash_5 = R"doc()doc";

static const char *__doc_mitsuba_hash_6 = R"doc()doc";

static const char *__doc_mitsuba_hash_combine = R"doc()doc";

static const char *__doc_mitsuba_hasher = R"doc()doc";

static const char *__doc_mitsuba_hasher_operator_call = R"doc()doc";

static const char *__doc_mitsuba_inv = R"doc(Compute the inverse of a matrix)doc";

static const char *__doc_mitsuba_is_constructible =
R"doc(Replacement for std::is_constructible which also works when the
destructor is not accessible)doc";

static const char *__doc_mitsuba_is_constructible_test = R"doc()doc";

static const char *__doc_mitsuba_is_constructible_test_2 = R"doc()doc";

static const char *__doc_mitsuba_math_bisect =
R"doc(Bisect a floating point interval given a predicate function

This function takes an interval [``left``, ``right``] and a predicate
``pred`` as inputs. It assumes that ``pred(left)==true`` and
``pred(right)==false``. It also assumes that there is a single
floating point number ``t`` such that ``pred`` is ``True`` for all
values in the range [``left``, ``t``] and ``False`` for all values in
the range (``t``, ``right``].

The bisection search then finds and returns ``t`` by repeatedly
splitting the input interval. The number of iterations is roughly
bounded by the number of bits of the underlying floating point
representation.)doc";

static const char *__doc_mitsuba_math_chbevl = R"doc(//! @{ \name Bessel functions)doc";

static const char *__doc_mitsuba_math_chi2 =
R"doc(Compute the Chi^2 statistic and degrees of freedom of the given arrays
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
treshold entries and resulting number of pooled regions.)doc";

static const char *__doc_mitsuba_math_comp_ellint_1 = R"doc(Complete elliptic integral of the first kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_1_2 = R"doc(Complete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_2 = R"doc(Complete elliptic integral of the second kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_2_2 = R"doc(Complete elliptic integral of the second kind (single precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_3 = R"doc(Complete elliptic integral of the third kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_3_2 = R"doc(Complete elliptic integral of the third kind (single precision))doc";

static const char *__doc_mitsuba_math_deg_to_rad = R"doc(Convert degrees to radians)doc";

static const char *__doc_mitsuba_math_ellint_1 = R"doc(Incomplete elliptic integral of the first kind (double precision))doc";

static const char *__doc_mitsuba_math_ellint_1_2 = R"doc(Incomplete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_ellint_2 = R"doc(Incomplete elliptic integral of the second kind (double precision))doc";

static const char *__doc_mitsuba_math_ellint_2_2 = R"doc(Incomplete elliptic integral of the second kind (single precision))doc";

static const char *__doc_mitsuba_math_ellint_3 = R"doc(Incomplete elliptic integral of the third kind (double precision))doc";

static const char *__doc_mitsuba_math_ellint_3_2 = R"doc(Incomplete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_find_interval =
R"doc(Find an interval in an ordered set

This function is very similar to ``std::upper_bound``, but it uses a
functor rather than an actual array to permit working with
procedurally defined data. It returns the index ``i`` such that
pred(i) is ``True`` and pred(i+1) is ``False``.

This function is primarily used to locate an interval (i, i+1) for
linear interpolation, hence its name. To avoid issues out of bounds
accesses, and to deal with predicates that evaluate to ``True`` or
``False`` on the entire domain, the returned left interval index is
clamped to the range ``[left, right-2]``.)doc";

static const char *__doc_mitsuba_math_find_interval_2 = R"doc()doc";

static const char *__doc_mitsuba_math_find_interval_3 =
R"doc(Find an interval in an ordered set

This function is very similar to ``std::upper_bound``, but it uses a
functor rather than an actual array to permit working with
procedurally defined data. It returns the index ``i`` such that
pred(i) is ``True`` and pred(i+1) is ``False``. See below for special
cases.

This function is primarily used to locate an interval (i, i+1) for
linear interpolation, hence its name. To avoid issues out of bounds
accesses, and to deal with predicates that evaluate to ``True`` or
``False`` on the entire domain, the returned left interval index is
clamped to the range ``[left, right-2]``. In particular: If there is
no index such that pred(i) is true, we return (left). If there is no
index such that pred(i+1) is false, we return (right-2).

Remark:
    This function is intended for vectorized predicates and
    additionally accepts a mask as an input. This mask can be used to
    disable some of the array entries. The mask is passed to the
    predicate as a second parameter.)doc";

static const char *__doc_mitsuba_math_find_interval_4 = R"doc()doc";

static const char *__doc_mitsuba_math_gamma = R"doc(Apply the sRGB gamma curve to a floating point scalar)doc";

static const char *__doc_mitsuba_math_i0e = R"doc()doc";

static const char *__doc_mitsuba_math_inv_gamma = R"doc(Apply the inverse of the sRGB gamma curve to a floating point scalar)doc";

static const char *__doc_mitsuba_math_is_power_of_two = R"doc(Check whether the provided integer is a power of two)doc";

static const char *__doc_mitsuba_math_legendre_p = R"doc(Evaluate the l-th Legendre polynomial using recurrence)doc";

static const char *__doc_mitsuba_math_legendre_p_2 = R"doc(Evaluate an associated Legendre polynomial using recurrence)doc";

static const char *__doc_mitsuba_math_legendre_pd =
R"doc(Evaluate the l-th Legendre polynomial and its derivative using
recurrence)doc";

static const char *__doc_mitsuba_math_legendre_pd_diff = R"doc(Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x))doc";

static const char *__doc_mitsuba_math_modulo = R"doc(Always-positive modulo function)doc";

static const char *__doc_mitsuba_math_normal_cdf = R"doc()doc";

static const char *__doc_mitsuba_math_normal_quantile = R"doc()doc";

static const char *__doc_mitsuba_math_rad_to_deg = R"doc(/ Convert radians to degrees)doc";

static const char *__doc_mitsuba_math_round_to_power_of_two = R"doc(Round an unsigned integer to the next integer power of two)doc";

static const char *__doc_mitsuba_math_solve_quadratic =
R"doc(Solve a quadratic equation of the form a*x^2 + b*x + c = 0.

Returns:
    ``True`` if a solution could be found)doc";

static const char *__doc_mitsuba_math_ulpdiff =
R"doc(Compare the difference in ULPs between a reference value and another
given floating point number)doc";

static const char *__doc_mitsuba_memcpy_cast = R"doc(Cast between types that have an identical binary representation.)doc";

static const char *__doc_mitsuba_operator_lshift = R"doc(Prints the canonical string representation of an object instance)doc";

static const char *__doc_mitsuba_operator_lshift_2 = R"doc(Prints the canonical string representation of an object instance)doc";

static const char *__doc_mitsuba_operator_lshift_3 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_4 = R"doc(Print a string representation of the bounding box)doc";

static const char *__doc_mitsuba_operator_lshift_5 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_6 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_7 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_8 = R"doc(Return a string representation of the bounding box)doc";

static const char *__doc_mitsuba_operator_lshift_9 = R"doc()doc";

static const char *__doc_mitsuba_ref =
R"doc(Reference counting helper

The *ref* template is a simple wrapper to store a pointer to an
object. It takes care of increasing and decreasing the object's
reference count as needed. When the last reference goes out of scope,
the associated object will be deallocated.

The advantage over C++ solutions such as ``std::shared_ptr`` is that
the reference count is very compactly integrated into the base object
itself.)doc";

static const char *__doc_mitsuba_ref_get = R"doc(Return a const pointer to the referenced object)doc";

static const char *__doc_mitsuba_ref_get_2 = R"doc(Return a pointer to the referenced object)doc";

static const char *__doc_mitsuba_ref_m_ptr = R"doc()doc";

static const char *__doc_mitsuba_ref_operator_T0 = R"doc(Return a pointer to the referenced object)doc";

static const char *__doc_mitsuba_ref_operator_assign = R"doc(Move another reference into the current one)doc";

static const char *__doc_mitsuba_ref_operator_assign_2 = R"doc(Overwrite this reference with another reference)doc";

static const char *__doc_mitsuba_ref_operator_assign_3 = R"doc(Overwrite this reference with a pointer to another object)doc";

static const char *__doc_mitsuba_ref_operator_bool = R"doc(Check if the object is defined)doc";

static const char *__doc_mitsuba_ref_operator_const_T0 = R"doc(Return a pointer to the referenced object)doc";

static const char *__doc_mitsuba_ref_operator_eq = R"doc(Compare this reference to another reference)doc";

static const char *__doc_mitsuba_ref_operator_eq_2 = R"doc(Compare this reference to a pointer)doc";

static const char *__doc_mitsuba_ref_operator_mul = R"doc(Return a C++ reference to the referenced object)doc";

static const char *__doc_mitsuba_ref_operator_mul_2 = R"doc(Return a const C++ reference to the referenced object)doc";

static const char *__doc_mitsuba_ref_operator_ne = R"doc(Compare this reference to another reference)doc";

static const char *__doc_mitsuba_ref_operator_ne_2 = R"doc(Compare this reference to a pointer)doc";

static const char *__doc_mitsuba_ref_operator_sub = R"doc(Access the object referenced by this reference)doc";

static const char *__doc_mitsuba_ref_operator_sub_2 = R"doc(Access the object referenced by this reference)doc";

static const char *__doc_mitsuba_ref_ref = R"doc(Create a ``nullptr``-valued reference)doc";

static const char *__doc_mitsuba_ref_ref_2 = R"doc(Construct a reference from a pointer)doc";

static const char *__doc_mitsuba_ref_ref_3 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_ref_ref_4 = R"doc(Move constructor)doc";

static const char *__doc_mitsuba_rgb_spectrum = R"doc()doc";

static const char *__doc_mitsuba_sample_tea_32 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

For details, refer to "GPU Random Numbers via the Tiny Encryption
Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

Parameter ``v0``:
    First input value to be encrypted (could be the sample index)

Parameter ``v1``:
    Second input value to be encrypted (e.g. the requested random
    number dimension)

Parameter ``rounds``:
    How many rounds should be executed? The default for random number
    generation is 4.

Returns:
    A uniformly distributed 32-bit integer)doc";

static const char *__doc_mitsuba_sample_tea_64 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

For details, refer to "GPU Random Numbers via the Tiny Encryption
Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

Parameter ``v0``:
    First input value to be encrypted (could be the sample index)

Parameter ``v1``:
    Second input value to be encrypted (e.g. the requested random
    number dimension)

Parameter ``rounds``:
    How many rounds should be executed? The default for random number
    generation is 4.

Returns:
    A uniformly distributed 64-bit integer)doc";

static const char *__doc_mitsuba_sample_tea_float =
R"doc(Alias to sample_tea_float32 or sample_tea_float64 based on compilation
flags)doc";

static const char *__doc_mitsuba_sample_tea_float32 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

This function uses sample_tea_ to return single precision floating
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
    ``[0, 1)``)doc";

static const char *__doc_mitsuba_sample_tea_float64 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

This function uses sample_tea_ to return double precision floating
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
    ``[0, 1)``)doc";

static const char *__doc_mitsuba_spline_eval_1d =
R"doc(Evaluate a cubic spline interpolant of a *uniformly* sampled 1D
function

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment.

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``x``:
    Evaluation point

Remark:
    The Python API lacks the ``size`` parameter, which is inferred
    automatically from the size of the input array.

Remark:
    The Python API provides a vectorized version which evaluates the
    function for many arguments ``x``.

Returns:
    The interpolated value or zero when ``Extrapolate=false`` and
    ``x`` lies outside of [``min``, ``max``])doc";

static const char *__doc_mitsuba_spline_eval_1d_2 =
R"doc(Evaluate a cubic spline interpolant of a *non*-uniformly sampled 1D
function

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment.

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``size``:
    Denotes the size of the ``nodes`` and ``values`` array

Parameter ``x``:
    Evaluation point

Remark:
    The Python API lacks the ``size`` parameter, which is inferred
    automatically from the size of the input array

Remark:
    The Python API provides a vectorized version which evaluates the
    function for many arguments ``x``.

Returns:
    The interpolated value or zero when ``Extrapolate=false`` and
    ``x`` lies outside of \a [``min``, ``max``])doc";

static const char *__doc_mitsuba_spline_eval_2d =
R"doc(Evaluate a cubic spline interpolant of a uniformly sampled 2D function

This implementation relies on a tensor product of Catmull-Rom splines,
i.e. it uses finite differences to approximate the derivatives for
each dimension at the endpoints of spline patches.

Template parameter ``Extrapolate``:
    Extrapolate values when ``p`` is out of range? (default:
    ``False``)

Parameter ``nodes1``:
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

Parameter ``values``:
    A 2D floating point array of ``size1*size2`` cells containing
    irregularly spaced evaluations of the function to be interpolated.
    Consecutive entries of this array correspond to increments in the
    ``X`` coordinate.

Parameter ``x``:
    ``X`` coordinate of the evaluation point

Parameter ``y``:
    ``Y`` coordinate of the evaluation point

Remark:
    The Python API lacks the ``size1`` and ``size2`` parameters, which
    are inferred automatically from the size of the input arrays.

Returns:
    The interpolated value or zero when ``Extrapolate=false``tt> and
    ``(x,y)`` lies outside of the node range)doc";

static const char *__doc_mitsuba_spline_eval_spline =
R"doc(Compute the definite integral and derivative of a cubic spline that is
parameterized by the function values and derivatives at the endpoints
of the interval ``[0, 1]``.

Parameter ``f0``:
    The function value at the left position

Parameter ``f1``:
    The function value at the right position

Parameter ``d0``:
    The function derivative at the left position

Parameter ``d1``:
    The function derivative at the right position

Parameter ``t``:
    The parameter variable

Returns:
    The interpolated function value at ``t``)doc";

static const char *__doc_mitsuba_spline_eval_spline_d =
R"doc(Compute the value and derivative of a cubic spline that is
parameterized by the function values and derivatives of the interval
``[0, 1]``.

Parameter ``f0``:
    The function value at the left position

Parameter ``f1``:
    The function value at the right position

Parameter ``d0``:
    The function derivative at the left position

Parameter ``d1``:
    The function derivative at the right position

Parameter ``t``:
    The parameter variable

Returns:
    The interpolated function value and its derivative at ``t``)doc";

static const char *__doc_mitsuba_spline_eval_spline_i =
R"doc(Compute the definite integral and value of a cubic spline that is
parameterized by the function values and derivatives of the interval
``[0, 1]``.

Parameter ``f0``:
    The function value at the left position

Parameter ``f1``:
    The function value at the right position

Parameter ``d0``:
    The function derivative at the left position

Parameter ``d1``:
    The function derivative at the right position

Returns:
    The definite integral and the interpolated function value at ``t``)doc";

static const char *__doc_mitsuba_spline_eval_spline_weights =
R"doc(Compute weights to perform a spline-interpolated lookup on a
*uniformly* sampled 1D function.

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment. The resulting weights are identical those internally
used by sample_1d().

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``size``:
    Denotes the number of function samples

Parameter ``x``:
    Evaluation point

Parameter ``weights``:
    Pointer to a weight array of size 4 that will be populated

Remark:
    In the Python API, the ``offset`` and ``weights`` parameters are
    returned as the second and third elements of a triple.

Returns:
    A boolean set to ``True`` on success and ``False`` when
    ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
    and an offset into the function samples associated with weights[0])doc";

static const char *__doc_mitsuba_spline_eval_spline_weights_2 =
R"doc(Compute weights to perform a spline-interpolated lookup on a
*non*-uniformly sampled 1D function.

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment. The resulting weights are identical those internally
used by sample_1d().

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``size``:
    Denotes the size of the ``nodes`` array

Parameter ``x``:
    Evaluation point

Parameter ``weights``:
    Pointer to a weight array of size 4 that will be populated

Remark:
    The Python API lacks the ``size`` parameter, which is inferred
    automatically from the size of the input array. The ``offset`` and
    ``weights`` parameters are returned as the second and third
    elements of a triple.

Returns:
    A boolean set to ``True`` on success and ``False`` when
    ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
    and an offset into the function samples associated with weights[0])doc";

static const char *__doc_mitsuba_spline_integrate_1d =
R"doc(Computes a prefix sum of integrals over segments of a *uniformly*
sampled 1D Catmull-Rom spline interpolant

This is useful for sampling spline segments as part of an importance
sampling scheme (in conjunction with sample_1d)

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
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
    and ``out`` is returned as a list.)doc";

static const char *__doc_mitsuba_spline_integrate_1d_2 =
R"doc(Computes a prefix sum of integrals over segments of a *non*-uniformly
sampled 1D Catmull-Rom spline interpolant

This is useful for sampling spline segments as part of an importance
sampling scheme (in conjunction with sample_1d)

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
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
    and ``out`` is returned as a list.)doc";

static const char *__doc_mitsuba_spline_invert_1d =
R"doc(Invert a cubic spline interpolant of a *uniformly* sampled 1D
function. The spline interpolant must be *monotonically increasing*.

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``y``:
    Input parameter for the inversion

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    The spline parameter ``t`` such that ``eval_1d(..., t)=y``)doc";

static const char *__doc_mitsuba_spline_invert_1d_2 =
R"doc(Invert a cubic spline interpolant of a *non*-uniformly sampled 1D
function. The spline interpolant must be *monotonically increasing*.

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``y``:
    Input parameter for the inversion

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    The spline parameter ``t`` such that ``eval_1d(..., t)=y``)doc";

static const char *__doc_mitsuba_spline_sample_1d =
R"doc(Importance sample a segment of a *uniformly* sampled 1D Catmull-Rom
spline interpolant

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``cdf``:
    Array containing a cumulative distribution function computed by
    integrate_1d().

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``sample``:
    A uniformly distributed random sample in the interval ``[0,1]``

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    1. The sampled position 2. The value of the spline evaluated at
    the sampled position 3. The probability density at the sampled
    position (which only differs from item 2. when the function does
    not integrate to one))doc";

static const char *__doc_mitsuba_spline_sample_1d_2 =
R"doc(Importance sample a segment of a *non*-uniformly sampled 1D Catmull-
Rom spline interpolant

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``cdf``:
    Array containing a cumulative distribution function computed by
    integrate_1d().

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``sample``:
    A uniformly distributed random sample in the interval ``[0,1]``

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    1. The sampled position 2. The value of the spline evaluated at
    the sampled position 3. The probability density at the sampled
    position (which only differs from item 2. when the function does
    not integrate to one))doc";

static const char *__doc_mitsuba_string_ends_with = R"doc(Check if the given string ends with a specified suffix)doc";

static const char *__doc_mitsuba_string_indent = R"doc(Indent every line of a string by some number of spaces)doc";

static const char *__doc_mitsuba_string_starts_with = R"doc(Check if the given string starts with a specified prefix)doc";

static const char *__doc_mitsuba_string_to_lower =
R"doc(Return a lower-case version of the given string (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_to_upper =
R"doc(Return a upper-case version of the given string (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_tokenize =
R"doc(Chop up the string given a set of delimiters (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_trim = R"doc()doc";

static const char *__doc_mitsuba_tuple_hasher = R"doc()doc";

static const char *__doc_mitsuba_tuple_hasher_operator_call = R"doc()doc";

static const char *__doc_mitsuba_type_suffix =
R"doc(Convenience function which computes an array size/type suffix (like
'2u' or '3fP'))doc";

static const char *__doc_mitsuba_util_core_count = R"doc(Determine the number of available CPU cores (including virtual cores))doc";

static const char *__doc_mitsuba_util_library_path = R"doc(Return the absolute path to <tt>libmitsuba-core.dylib/so/dll<tt>)doc";

static const char *__doc_mitsuba_util_mem_string = R"doc(Turn a memory size into a human-readable string)doc";

static const char *__doc_mitsuba_util_time_string =
R"doc(Convert a time difference (in seconds) to a string representation

Parameter ``time``:
    Time difference in (fractional) sections

Parameter ``precise``:
    When set to true, a higher-precision string representation is
    generated.)doc";

static const char *__doc_mitsuba_util_trap_debugger =
R"doc(Generate a trap instruction if running in a debugger; otherwise,
return.)doc";

static const char *__doc_mitsuba_variant = R"doc()doc";

static const char *__doc_mitsuba_variant_data = R"doc()doc";

static const char *__doc_mitsuba_variant_empty = R"doc()doc";

static const char *__doc_mitsuba_variant_is = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign_3 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign_4 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_const_type_parameter_1_0 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_type_parameter_1_0 = R"doc()doc";

static const char *__doc_mitsuba_variant_type = R"doc()doc";

static const char *__doc_mitsuba_variant_type_info = R"doc()doc";

static const char *__doc_mitsuba_variant_variant = R"doc()doc";

static const char *__doc_mitsuba_variant_variant_2 = R"doc()doc";

static const char *__doc_mitsuba_variant_variant_3 = R"doc()doc";

static const char *__doc_mitsuba_variant_visit = R"doc()doc";

static const char *__doc_mitsuba_warp_beckmann_to_square = R"doc(Inverse of the mapping square_to_uniform_cone)doc";

static const char *__doc_mitsuba_warp_cosine_hemisphere_to_square = R"doc(Inverse of the mapping square_to_cosine_hemisphere)doc";

static const char *__doc_mitsuba_warp_detail_i0 = R"doc()doc";

static const char *__doc_mitsuba_warp_detail_log_i0 = R"doc()doc";

static const char *__doc_mitsuba_warp_interval_to_nonuniform_tent =
R"doc(Warp a uniformly distributed sample on [0, 1] to a nonuniform tent
distribution with nodes ``{a, b, c}``)doc";

static const char *__doc_mitsuba_warp_interval_to_tent = R"doc(Warp a uniformly distributed sample on [0, 1] to a tent distribution)doc";

static const char *__doc_mitsuba_warp_square_to_beckmann = R"doc(Warp a uniformly distributed square sample to a Beckmann distribution)doc";

static const char *__doc_mitsuba_warp_square_to_beckmann_pdf = R"doc(Probability density of square_to_beckmann())doc";

static const char *__doc_mitsuba_warp_square_to_cosine_hemisphere =
R"doc(Sample a cosine-weighted vector on the unit hemisphere with respect to
solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_cosine_hemisphere_pdf = R"doc(Density of square_to_cosine_hemisphere() with respect to solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_rough_fiber =
R"doc(Warp a uniformly distributed square sample to a rough fiber
distribution)doc";

static const char *__doc_mitsuba_warp_square_to_rough_fiber_pdf = R"doc(Probability density of square_to_rough_fiber())doc";

static const char *__doc_mitsuba_warp_square_to_std_normal =
R"doc(Sample a point on a 2D standard normal distribution. Internally uses
the Box-Muller transformation)doc";

static const char *__doc_mitsuba_warp_square_to_std_normal_pdf = R"doc()doc";

static const char *__doc_mitsuba_warp_square_to_tent = R"doc(Warp a uniformly distributed square sample to a 2D tent distribution)doc";

static const char *__doc_mitsuba_warp_square_to_tent_pdf = R"doc(Density of square_to_tent per unit area.)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_cone =
R"doc(Uniformly sample a vector that lies within a given cone of angles
around the Z axis

Parameter ``cos_cutoff``:
    Cosine of the cutoff angle

Parameter ``sample``:
    A uniformly distributed sample on $[0,1]^2$)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_cone_pdf =
R"doc(Density of square_to_uniform_cone per unit area.

Parameter ``cos_cutoff``:
    Cosine of the cutoff angle)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk = R"doc(Uniformly sample a vector on a 2D disk)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk_concentric = R"doc(Low-distortion concentric square to disk mapping by Peter Shirley)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk_concentric_pdf = R"doc(Density of square_to_uniform_disk per unit area)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk_pdf = R"doc(Density of square_to_uniform_disk per unit area)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_hemisphere =
R"doc(Uniformly sample a vector on the unit hemisphere with respect to solid
angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_hemisphere_pdf = R"doc(Density of square_to_uniform_hemisphere() with respect to solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_sphere =
R"doc(Uniformly sample a vector on the unit sphere with respect to solid
angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_sphere_pdf = R"doc(Density of square_to_uniform_sphere() with respect to solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_triangle =
R"doc(Convert an uniformly distributed square sample into barycentric
coordinates)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_triangle_pdf = R"doc(Density of square_to_uniform_triangle per unit area.)doc";

static const char *__doc_mitsuba_warp_square_to_von_mises_fisher =
R"doc(Warp a uniformly distributed square sample to a von Mises Fisher
distribution)doc";

static const char *__doc_mitsuba_warp_square_to_von_mises_fisher_pdf = R"doc(Probability density of square_to_von_mises_fisher())doc";

static const char *__doc_mitsuba_warp_tent_to_interval = R"doc(Warp a uniformly distributed sample on [0, 1] to a tent distribution)doc";

static const char *__doc_mitsuba_warp_tent_to_square = R"doc(Warp a uniformly distributed square sample to a 2D tent distribution)doc";

static const char *__doc_mitsuba_warp_uniform_cone_to_square = R"doc(Inverse of the mapping square_to_uniform_cone)doc";

static const char *__doc_mitsuba_warp_uniform_disk_to_square = R"doc(Inverse of the mapping square_to_uniform_disk)doc";

static const char *__doc_mitsuba_warp_uniform_disk_to_square_concentric = R"doc(Inverse of the mapping square_to_uniform_disk_concentric)doc";

static const char *__doc_mitsuba_warp_uniform_hemisphere_to_square = R"doc(Inverse of the mapping square_to_uniform_hemisphere)doc";

static const char *__doc_mitsuba_warp_uniform_sphere_to_square = R"doc(Inverse of the mapping square_to_uniform_sphere)doc";

static const char *__doc_mitsuba_warp_uniform_triangle_to_square = R"doc(Inverse of the mapping square_to_uniform_triangle)doc";

static const char *__doc_mitsuba_warp_von_mises_fisher_to_square = R"doc(Inverse of the mapping von_mises_fisher_to_square)doc";

static const char *__doc_mitsuba_xml_load_file = R"doc(Load a Mitsuba scene from an XML file)doc";

static const char *__doc_mitsuba_xml_load_string = R"doc(Load a Mitsuba scene from an XML string)doc";

static const char *__doc_operator_lshift = R"doc(Turns a vector of elements into a human-readable representation)doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

