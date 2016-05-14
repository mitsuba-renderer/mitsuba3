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
R"doc(An AnnotatedStream adds Table of Contents capabilities to an
underlying stream. A Stream instance must first be created and passed
to to the constructor.

Table of Contents: objects and variables written to the stream are
prepended by a field name. Contents can then be queried by field name,
as if using a map. A hierarchy can be created by ``push``ing and
``pop``ing prefixes.)doc";

static const char *__doc_mitsuba_AnnotatedStream_AnnotatedStream =
R"doc(Creates an AnnotatedStream based on the given Stream (decorator
pattern). Anything written to the AnnotatedStream is ultimately passed
down to the given Stream instance. The given Stream instance should
not be destructed before this.

TODO: in order not to use unique_ptr, use move constructor or such?)doc";

static const char *__doc_mitsuba_AnnotatedStream_get = R"doc(Retrieve a field from the serialized file (only valid in read mode))doc";

static const char *__doc_mitsuba_AnnotatedStream_getClass = R"doc()doc";

static const char *__doc_mitsuba_AnnotatedStream_keys = R"doc(Return all field names under the current name prefix)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_prefixStack = R"doc(Stack of prefixes currently applied, constituting a path)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_stream = R"doc(Underlying stream where the names and contents are written)doc";

static const char *__doc_mitsuba_AnnotatedStream_m_table =
R"doc(Maintains the mapping: full field name -> (type, position in the
stream))doc";

static const char *__doc_mitsuba_AnnotatedStream_pop = R"doc(Pop a name prefix from the stack)doc";

static const char *__doc_mitsuba_AnnotatedStream_push =
R"doc(Push a name prefix onto the stack (use this to isolate identically-
named data fields).)doc";

static const char *__doc_mitsuba_AnnotatedStream_set = R"doc(Store a field in the serialized file (only valid in write mode))doc";

static const char *__doc_mitsuba_Appender =
R"doc(This class defines an abstract destination for logging-relevant
information)doc";

static const char *__doc_mitsuba_Appender_append = R"doc(Append a line of text with the given log level)doc";

static const char *__doc_mitsuba_Appender_getClass = R"doc()doc";

static const char *__doc_mitsuba_Appender_logProgress =
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

static const char *__doc_mitsuba_ArgParser_Arg_asFloat = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_asInt = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_asString = R"doc(Return the extra argument associated with this argument)doc";

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

static const char *__doc_mitsuba_ArgParser_executableName = R"doc(Return the name of the invoked application executable)doc";

static const char *__doc_mitsuba_ArgParser_m_args = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_m_executableName = R"doc()doc";

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

static const char *__doc_mitsuba_Class_construct =
R"doc(Generate an instance of this class

Parameter ``props``:
    A list of extra parameters that are supplied to the constructor

Remark:
    Throws an exception if the class is not constructible)doc";

static const char *__doc_mitsuba_Class_derivesFrom = R"doc(Check whether this class derives from *theClass*)doc";

static const char *__doc_mitsuba_Class_forName = R"doc(Look up a class by its name)doc";

static const char *__doc_mitsuba_Class_getName = R"doc(Return the name of the represented class)doc";

static const char *__doc_mitsuba_Class_getParent =
R"doc(Return the Class object associated with the parent class of nullptr if
it does not have one.)doc";

static const char *__doc_mitsuba_Class_initializeOnce = R"doc(Initialize a class - called by staticInitialization())doc";

static const char *__doc_mitsuba_Class_isAbstract =
R"doc(Return whether or not the class represented by this Class object
contains pure virtual methods)doc";

static const char *__doc_mitsuba_Class_isConstructible = R"doc(Does the class support instantiation over RTTI?)doc";

static const char *__doc_mitsuba_Class_isSerializable = R"doc(Does the class support serialization?)doc";

static const char *__doc_mitsuba_Class_m_abstract = R"doc()doc";

static const char *__doc_mitsuba_Class_m_constr = R"doc()doc";

static const char *__doc_mitsuba_Class_m_name = R"doc()doc";

static const char *__doc_mitsuba_Class_m_parent = R"doc()doc";

static const char *__doc_mitsuba_Class_m_parentName = R"doc()doc";

static const char *__doc_mitsuba_Class_m_unser = R"doc()doc";

static const char *__doc_mitsuba_Class_rttiIsInitialized = R"doc(Check if the RTTI layer has been initialized)doc";

static const char *__doc_mitsuba_Class_staticInitialization =
R"doc(Initializes the built-in RTTI and creates a list of all compiled
classes)doc";

static const char *__doc_mitsuba_Class_staticShutdown = R"doc(Free the memory taken by staticInitialization())doc";

static const char *__doc_mitsuba_Class_unserialize =
R"doc(Unserialize an instance of the class

Remark:
    Throws an exception if the class is not unserializable)doc";

static const char *__doc_mitsuba_DefaultFormatter =
R"doc(The default formatter used to turn log messages into a human-readable
form)doc";

static const char *__doc_mitsuba_DefaultFormatter_DefaultFormatter = R"doc(Create a new default formatter)doc";

static const char *__doc_mitsuba_DefaultFormatter_format = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_getClass = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_getHaveClass =
R"doc(See also:
    setHaveClass)doc";

static const char *__doc_mitsuba_DefaultFormatter_getHaveDate =
R"doc(See also:
    setHaveDate)doc";

static const char *__doc_mitsuba_DefaultFormatter_getHaveLogLevel =
R"doc(See also:
    setHaveLogLevel)doc";

static const char *__doc_mitsuba_DefaultFormatter_getHaveThread =
R"doc(See also:
    setHaveThread)doc";

static const char *__doc_mitsuba_DefaultFormatter_m_haveClass = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_haveDate = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_haveLogLevel = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_haveThread = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_setHaveClass = R"doc(Should class information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_setHaveDate = R"doc(Should date information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_setHaveLogLevel = R"doc(Should log level information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_setHaveThread = R"doc(Should thread information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DummyStream =
R"doc(Stream implementation that never writes to disk, but keeps track of
the size of the content being written. It can be used, for example, to
measure the precise amount of memory needed to store serialized
content.)doc";

static const char *__doc_mitsuba_DummyStream_DummyStream = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_canRead =
R"doc(Always returns false, as nothing written to a ``DummyStream`` is
actually written.)doc";

static const char *__doc_mitsuba_DummyStream_canWrite = R"doc(Always returns true.)doc";

static const char *__doc_mitsuba_DummyStream_flush = R"doc(No-op for ``DummyStream``.)doc";

static const char *__doc_mitsuba_DummyStream_getClass = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_getPos = R"doc(Returns the current position in the stream.)doc";

static const char *__doc_mitsuba_DummyStream_getSize = R"doc(Returns the size of the stream.)doc";

static const char *__doc_mitsuba_DummyStream_m_pos =
R"doc(Current position in the "virtual" stream (even though nothing is ever
written, we need to maintain consistent positioning).)doc";

static const char *__doc_mitsuba_DummyStream_m_size = R"doc(Size of all data written to the stream)doc";

static const char *__doc_mitsuba_DummyStream_read = R"doc(Always throws, since DummyStream is write-only.)doc";

static const char *__doc_mitsuba_DummyStream_seek =
R"doc(Updates the current position in the stream. Even though the
``DummyStream`` doesn't write anywhere, position is taken into account
to correctly maintain the size of the stream.

Throws if attempting to seek beyond the size of the stream.)doc";

static const char *__doc_mitsuba_DummyStream_toString = R"doc(Returns a string representation)doc";

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

static const char *__doc_mitsuba_FileResolver_clear = R"doc(Clear the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_contains = R"doc(Check if a given path is included in the search path list)doc";

static const char *__doc_mitsuba_FileResolver_end = R"doc(Return an iterator at the end of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_end_2 = R"doc(Return an iterator at the end of the list of search paths (const))doc";

static const char *__doc_mitsuba_FileResolver_erase = R"doc(Erase the entry at the given iterator position)doc";

static const char *__doc_mitsuba_FileResolver_erase_2 = R"doc(Erase the search path from the list)doc";

static const char *__doc_mitsuba_FileResolver_getClass = R"doc()doc";

static const char *__doc_mitsuba_FileResolver_m_paths = R"doc()doc";

static const char *__doc_mitsuba_FileResolver_operator_array = R"doc(Return an entry from the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_operator_array_2 = R"doc(Return an entry from the list of search paths (const))doc";

static const char *__doc_mitsuba_FileResolver_prepend = R"doc(Prepend an entry at the beginning of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_resolve =
R"doc(Walk through the list of search paths and try to resolve the input
path)doc";

static const char *__doc_mitsuba_FileResolver_size = R"doc(Return the number of search paths)doc";

static const char *__doc_mitsuba_FileResolver_toString = R"doc(Return a human-readable representation of this instance)doc";

static const char *__doc_mitsuba_FileStream =
R"doc(Simple Stream implementation backed-up by a file.

TODO: explain which low-level tools are used for implementation.)doc";

static const char *__doc_mitsuba_FileStream_FileStream =
R"doc(Constructs a new FileStream by opening the file pointed by ``p``. The
file is opened in append mode, and read / write mode as specified by
``writeEnabled``.

Throws an exception if the file cannot be open.)doc";

static const char *__doc_mitsuba_FileStream_flush = R"doc(Flushes any buffered operation to the underlying file.)doc";

static const char *__doc_mitsuba_FileStream_getClass = R"doc()doc";

static const char *__doc_mitsuba_FileStream_getPos = R"doc(Gets the current position inside the file)doc";

static const char *__doc_mitsuba_FileStream_getSize = R"doc(Returns the size of the file)doc";

static const char *__doc_mitsuba_FileStream_m_file = R"doc()doc";

static const char *__doc_mitsuba_FileStream_m_path = R"doc()doc";

static const char *__doc_mitsuba_FileStream_read =
R"doc(Reads a specified amount of data from the stream. Throws an exception
when the stream ended prematurely.)doc";

static const char *__doc_mitsuba_FileStream_seek =
R"doc(Seeks to a position inside the stream. Throws an exception when trying
to seek beyond the limits of the file.)doc";

static const char *__doc_mitsuba_FileStream_toString = R"doc(Returns a string representation)doc";

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

static const char *__doc_mitsuba_Formatter_format =
R"doc(Turn a log message into a human-readable format

Parameter ``level``:
    The importance of the debug message

Parameter ``theClass``:
    Originating class or nullptr

Parameter ``thread``:
    Thread, which is reponsible for creating the message

Parameter ``file``:
    File, which is responsible for creating the message

Parameter ``line``:
    Associated line within the source file

Parameter ``msg``:
    Text content associated with the log message)doc";

static const char *__doc_mitsuba_Formatter_getClass = R"doc()doc";

static const char *__doc_mitsuba_Logger =
R"doc(Responsible for processing log messages

Upon receiving a log message, the Logger class invokes a Formatter to
convert it into a human-readable form. Following that, it sends this
information to every registered Appender.)doc";

static const char *__doc_mitsuba_Logger_Logger = R"doc(Construct a new logger with the given minimum log level)doc";

static const char *__doc_mitsuba_Logger_addAppender = R"doc(Add an appender to this logger)doc";

static const char *__doc_mitsuba_Logger_clearAppenders = R"doc(Remove all appenders from this logger)doc";

static const char *__doc_mitsuba_Logger_d = R"doc()doc";

static const char *__doc_mitsuba_Logger_getAppender = R"doc(Return one of the appenders)doc";

static const char *__doc_mitsuba_Logger_getAppenderCount = R"doc(Return the number of registered appenders)doc";

static const char *__doc_mitsuba_Logger_getAppender_2 = R"doc(Return one of the appenders)doc";

static const char *__doc_mitsuba_Logger_getClass = R"doc()doc";

static const char *__doc_mitsuba_Logger_getErrorLevel = R"doc(Return the current error level)doc";

static const char *__doc_mitsuba_Logger_getFormatter = R"doc(Return the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_getFormatter_2 = R"doc(Return the logger's formatter implementation (const))doc";

static const char *__doc_mitsuba_Logger_getLogLevel = R"doc(Return the current log level)doc";

static const char *__doc_mitsuba_Logger_log =
R"doc(Process a log message

Parameter ``level``:
    Log level of the message

Parameter ``theClass``:
    Class descriptor of the message creator

Parameter ``filename``:
    Source file of the message creator

Parameter ``line``:
    Source line number of the message creator

Parameter ``fmt``:
    printf-style string formatter \note This function is not exposed
    in the Python bindings. Instead, please use \cc mitsuba.core.Log)doc";

static const char *__doc_mitsuba_Logger_logProgress =
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

static const char *__doc_mitsuba_Logger_m_logLevel = R"doc()doc";

static const char *__doc_mitsuba_Logger_readLog =
R"doc(Return the contents of the log file as a string

Throws a runtime exception upon failure)doc";

static const char *__doc_mitsuba_Logger_removeAppender = R"doc(Remove an appender from this logger)doc";

static const char *__doc_mitsuba_Logger_setErrorLevel =
R"doc(Set the error log level (this level and anything above will throw
exceptions).

The value provided here can be used for instance to turn warnings into
errors. But *level* must always be less than EError, i.e. it isn't
possible to cause errors not to throw an exception.)doc";

static const char *__doc_mitsuba_Logger_setFormatter = R"doc(Set the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_setLogLevel = R"doc(Set the log level (everything below will be ignored))doc";

static const char *__doc_mitsuba_Logger_staticInitialization = R"doc(Initialize logging)doc";

static const char *__doc_mitsuba_Logger_staticShutdown = R"doc(Shutdown logging)doc";

static const char *__doc_mitsuba_MemoryStream =
R"doc(Simple memory buffer-based stream with automatic memory management. It
always has read & write capabilities.

The underlying memory storage of this implementation dynamically
expands as data is written to the stream.)doc";

static const char *__doc_mitsuba_MemoryStream_MemoryStream =
R"doc(Creates a new memory stream, initializing the memory buffer with a
capacity of ``initialSize`` bytes. For best performance, set this
argument to the estimated size of the content that will be written to
the stream.)doc";

static const char *__doc_mitsuba_MemoryStream_MemoryStream_2 =
R"doc(Creates a memory stream, which operates on a pre-allocated buffer.

A memory stream created in this way will never resize the underlying
buffer. An exception is thrown e.g. when attempting to extend its
size.

Remark:
    This constructor is not available in the python bindings.)doc";

static const char *__doc_mitsuba_MemoryStream_canRead = R"doc(Always returns true.)doc";

static const char *__doc_mitsuba_MemoryStream_canWrite = R"doc(Always returns true.)doc";

static const char *__doc_mitsuba_MemoryStream_flush = R"doc(No-op since all writes are made directly to memory)doc";

static const char *__doc_mitsuba_MemoryStream_getClass = R"doc()doc";

static const char *__doc_mitsuba_MemoryStream_getPos = R"doc(Gets the current position inside the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_getSize =
R"doc(Returns the size of the contents written to the memory buffer. \note
This is not equal to the size of the memory buffer in general.)doc";

static const char *__doc_mitsuba_MemoryStream_m_capacity = R"doc(Current size of the allocated memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_data = R"doc(Pointer to the memory buffer (might not be owned))doc";

static const char *__doc_mitsuba_MemoryStream_m_ownsBuffer = R"doc(False if the MemoryStream was created from a pre-allocated buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_pos = R"doc(Current position inside of the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_size = R"doc(Current size of the contents written to the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_read =
R"doc(Reads a specified amount of data from the stream. Throws an exception
if trying to read further than the current size of the contents.)doc";

static const char *__doc_mitsuba_MemoryStream_resize = R"doc()doc";

static const char *__doc_mitsuba_MemoryStream_seek =
R"doc(Seeks to a position inside the stream. If trying to seek beyond the
size of the content, or even beyond the capacity buffer, it is simply
adjusted accordingly. The contents of the memory that was skipped is
undefined.)doc";

static const char *__doc_mitsuba_MemoryStream_toString = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_MemoryStream_truncate =
R"doc(Truncates the contents **and** the memory buffer's capacity to a given
size. The position is updated to ``min(old_position, size)``.

\note This will throw is the MemoryStream was initialized with a pre-
allocated buffer.)doc";

static const char *__doc_mitsuba_MemoryStream_write =
R"doc(Writes a specified amount of data into the memory buffer. The capacity
of the memory buffer is extended if necessary.)doc";

static const char *__doc_mitsuba_Object = R"doc(Reference counted object base class)doc";

static const char *__doc_mitsuba_Object_Object = R"doc(Default constructor)doc";

static const char *__doc_mitsuba_Object_Object_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Object_decRef =
R"doc(Decrease the reference count of the object and possibly deallocate it.

The object will automatically be deallocated once the reference count
reaches zero.)doc";

static const char *__doc_mitsuba_Object_getClass =
R"doc(Return a Class instance containing run-time type information about
this Object

See also:
    Class)doc";

static const char *__doc_mitsuba_Object_getRefCount = R"doc(Return the current reference count)doc";

static const char *__doc_mitsuba_Object_incRef = R"doc(Increase the object's reference count by one)doc";

static const char *__doc_mitsuba_Object_m_refCount = R"doc()doc";

static const char *__doc_mitsuba_Object_toString =
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
createObject() methods. The generated instances are then assembled
into a final object graph, such as a scene. One such examples is the
SceneHandler class, which parses an XML scene file by esentially
translating the XML elements into calls to createObject().

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

static const char *__doc_mitsuba_PluginManager_createObject =
R"doc(Instantiate a plugin, verify its type, and return the newly created
object instance.

Parameter ``classType``:
    Expected type of the instance. An exception will be thrown if it
    turns out not to derive from this class.

Parameter ``props``:
    A Properties instance containing all information required to find
    and construct the plugin.)doc";

static const char *__doc_mitsuba_PluginManager_createObject_2 =
R"doc(Instantiate a plugin and return the new instance (without verifying
its type).

Parameter ``props``:
    A Properties instance containing all information required to find
    and construct the plugin.)doc";

static const char *__doc_mitsuba_PluginManager_d = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_ensurePluginLoaded = R"doc(Ensure that a plugin is loaded and ready)doc";

static const char *__doc_mitsuba_PluginManager_getClass = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_getInstance = R"doc(Return the global plugin manager)doc";

static const char *__doc_mitsuba_PluginManager_getLoadedPlugins = R"doc(Return the list of loaded plugins)doc";

static const char *__doc_mitsuba_Properties =
R"doc(Associative parameter map for constructing subclasses of Object.

Note that the Python bindings for this class do not implement the
various type-dependent getters and setters. Instead, they are accessed
just like a normal Python map, e.g:

```
myProps = mitsuba.core.Properties("pluginName")
myProps["stringProperty"] = "hello"
myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)
```)doc";

static const char *__doc_mitsuba_Properties_EPropertyType = R"doc(Supported types of properties)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EAnimatedTransform = R"doc(An animated 4x4 transformation)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EBoolean = R"doc(Boolean value (true/false))doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EFloat = R"doc(Floating point value)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EInteger = R"doc(64-bit signed integer)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EObject = R"doc(Arbitrary object)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EPoint = R"doc(3D point)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_ESpectrum = R"doc(Discretized color spectrum)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EString = R"doc(Arbitrary-length string)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_ETransform = R"doc(4x4 transform for homogeneous coordinates)doc";

static const char *__doc_mitsuba_Properties_EPropertyType_EVector3f = R"doc(3D vector)doc";

static const char *__doc_mitsuba_Properties_Properties = R"doc(Construct an empty property container)doc";

static const char *__doc_mitsuba_Properties_Properties_2 = R"doc(Construct an empty property container with a specific plugin name)doc";

static const char *__doc_mitsuba_Properties_Properties_3 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Properties_copyAttribute =
R"doc(Copy an attribute from another Properties object and potentially
rename it)doc";

static const char *__doc_mitsuba_Properties_d = R"doc()doc";

static const char *__doc_mitsuba_Properties_getBoolean = R"doc(Retrieve a boolean value)doc";

static const char *__doc_mitsuba_Properties_getBoolean_2 = R"doc(Retrieve a boolean value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_getFloat = R"doc(Retrieve a floating point value)doc";

static const char *__doc_mitsuba_Properties_getFloat_2 = R"doc(Retrieve a floating point value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_getID =
R"doc(Returns a unique identifier associated with this instance (or an empty
string))doc";

static const char *__doc_mitsuba_Properties_getInteger = R"doc(Retrieve an integer value)doc";

static const char *__doc_mitsuba_Properties_getInteger_2 = R"doc(Retrieve a boolean value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_getLong = R"doc(Retrieve an integer value)doc";

static const char *__doc_mitsuba_Properties_getLong_2 = R"doc(Retrieve an integer value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_getObject = R"doc(Retrieve an arbitrary object)doc";

static const char *__doc_mitsuba_Properties_getObject_2 = R"doc(Retrieve an arbitrary object (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_getPluginName = R"doc(Get the associated plugin name)doc";

static const char *__doc_mitsuba_Properties_getPropertyNames = R"doc(Store an array containing the names of all stored properties)doc";

static const char *__doc_mitsuba_Properties_getPropertyType =
R"doc(Returns the type of an existing property. If no property exists under
that name, an error is logged and type ``void`` is returned.)doc";

static const char *__doc_mitsuba_Properties_getString = R"doc(Retrieve a string value)doc";

static const char *__doc_mitsuba_Properties_getString_2 = R"doc(Retrieve a string value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_getUnqueried = R"doc(Return the list of un-queried attributed)doc";

static const char *__doc_mitsuba_Properties_getVector3f = R"doc(Retrieve a vector)doc";

static const char *__doc_mitsuba_Properties_getVector3f_2 = R"doc(Retrieve a vector (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_hasProperty = R"doc(Verify if a value with the specified name exists)doc";

static const char *__doc_mitsuba_Properties_markQueried =
R"doc(Manually mark a certain property as queried

Returns:
    ``True`` upon success)doc";

static const char *__doc_mitsuba_Properties_merge =
R"doc(Merge another properties record into the current one. Existing
properties will be overwritten with the values from ``props`` if they
have the same name.)doc";

static const char *__doc_mitsuba_Properties_operator_assign = R"doc(Assignment operator)doc";

static const char *__doc_mitsuba_Properties_operator_eq = R"doc(Equality comparison operator)doc";

static const char *__doc_mitsuba_Properties_operator_ne = R"doc(Inequality comparison operator)doc";

static const char *__doc_mitsuba_Properties_removeProperty =
R"doc(Remove a property with the specified name

Returns:
    ``True`` upon success)doc";

static const char *__doc_mitsuba_Properties_setBoolean = R"doc(Store a boolean value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_setFloat = R"doc(Store a floating point value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_setID = R"doc(Set the unique identifier associated with this instance)doc";

static const char *__doc_mitsuba_Properties_setInteger = R"doc(Set an integer value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_setLong = R"doc(Store an integer value in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_setObject = R"doc(Store an arbitrary object in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_setPluginName = R"doc(Set the associated plugin name)doc";

static const char *__doc_mitsuba_Properties_setString = R"doc(Store a string in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_setVector3f = R"doc(Store a vector in the Properties instance)doc";

static const char *__doc_mitsuba_Properties_wasQueried = R"doc(Check if a certain property was queried)doc";

static const char *__doc_mitsuba_Stream =
R"doc(Abstract seekable stream class

Specifies all functions to be implemented by stream subclasses and
provides various convenience functions layered on top of on them.

All read**X**() and write**X**() methods support transparent
conversion based on the endianness of the underlying system and the
value passed to setByteOrder(). Whenever getHostByteOrder() and
getByteOrder() disagree, the endianness is swapped.

TODO: explain Table of Contents feature. TODO: explain write-only /
read-only modes.

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

static const char *__doc_mitsuba_StreamAppender_getClass = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_logProgress = R"doc(Process a progress message)doc";

static const char *__doc_mitsuba_StreamAppender_logsToFile = R"doc(Does this appender log to a file)doc";

static const char *__doc_mitsuba_StreamAppender_m_fileName = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_isFile = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_lastMessageWasProgress = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_stream = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_readLog = R"doc(Return the contents of the log file as a string)doc";

static const char *__doc_mitsuba_StreamAppender_toString = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Stream_EByteOrder = R"doc(Defines the byte order to use in this Stream)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_EBigEndian = R"doc(< PowerPC, SPARC, Motorola 68K)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_ELittleEndian = R"doc(< x86, x86_64)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_ENetworkByteOrder = R"doc(< Network byte order (an alias for big endian))doc";

static const char *__doc_mitsuba_Stream_Stream =
R"doc(Creates a new stream. By default, it assumes the byte order of the
underlying system, i.e. no endianness conversion is performed.

Parameter ``writeEnabled``:
    If true, the stream will be write-only, otherwise it will be read-
    only.)doc";

static const char *__doc_mitsuba_Stream_Stream_2 = R"doc(Copy is disallowed.)doc";

static const char *__doc_mitsuba_Stream_canRead = R"doc(Can we read from the stream?)doc";

static const char *__doc_mitsuba_Stream_canWrite = R"doc(Can we write to the stream?)doc";

static const char *__doc_mitsuba_Stream_flush = R"doc(Flushes the stream's buffers, if any)doc";

static const char *__doc_mitsuba_Stream_getClass = R"doc()doc";

static const char *__doc_mitsuba_Stream_getPos = R"doc(Gets the current position inside the stream)doc";

static const char *__doc_mitsuba_Stream_getSize = R"doc(Returns the size of the stream)doc";

static const char *__doc_mitsuba_Stream_m_byteOrder = R"doc()doc";

static const char *__doc_mitsuba_Stream_m_writeMode = R"doc()doc";

static const char *__doc_mitsuba_Stream_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Stream_read =
R"doc(Reads a specified amount of data from the stream.

Throws an exception when the stream ended prematurely. Implementations
need to handle endianness swap when appropriate.)doc";

static const char *__doc_mitsuba_Stream_readValue =
R"doc(Reads one object of type T from the stream at the current position by
delegating to the appropriate ``serialization_helper``.)doc";

static const char *__doc_mitsuba_Stream_seek =
R"doc(Seeks to a position inside the stream. Throws an exception when trying
to seek beyond the limits of the stream.)doc";

static const char *__doc_mitsuba_Stream_toString = R"doc(Returns a string representation of the stream)doc";

static const char *__doc_mitsuba_Stream_truncate =
R"doc(Truncates the stream to a given size. The position is updated to
``min(old_position, size)``.

Throws an exception if in read-only mode.)doc";

static const char *__doc_mitsuba_Stream_write =
R"doc(Writes a specified amount of data into the stream.

Throws an exception when not all data could be written.
Implementations need to handle endianness swap when appropriate.)doc";

static const char *__doc_mitsuba_Stream_writeValue =
R"doc(Reads one object of type T from the stream at the current position by
delegating to the appropriate ``serialization_helper``.)doc";

static const char *__doc_mitsuba_TNormal = R"doc(3-dimensional surface normal representation)doc";

static const char *__doc_mitsuba_TNormal_TNormal = R"doc()doc";

static const char *__doc_mitsuba_TNormal_TNormal_2 = R"doc()doc";

static const char *__doc_mitsuba_TNormal_operator_Matrix = R"doc(Convert to an Eigen vector (definition in transform.h))doc";

static const char *__doc_mitsuba_TPoint = R"doc()doc";

static const char *__doc_mitsuba_TPoint_TPoint = R"doc()doc";

static const char *__doc_mitsuba_TPoint_TPoint_2 = R"doc()doc";

static const char *__doc_mitsuba_TPoint_operator_Matrix = R"doc(Convert to an Eigen vector (definition in transform.h))doc";

static const char *__doc_mitsuba_TVector = R"doc()doc";

static const char *__doc_mitsuba_TVector_TVector = R"doc()doc";

static const char *__doc_mitsuba_TVector_TVector_2 = R"doc()doc";

static const char *__doc_mitsuba_TVector_operator_Matrix = R"doc(Convert to an Eigen vector (definition in transform.h))doc";

static const char *__doc_mitsuba_Thread =
R"doc(Cross-platform thread implementation

Mitsuba threads are internally implemented via the ``std::thread``
class defined in C++11. This wrapper class is needed to attach
additional state (Loggers, Path resolvers, etc.) that is inherited
when a thread launches another thread.)doc";

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

static const char *__doc_mitsuba_ThreadLocalBase_get = R"doc(Return the data value associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocalBase_get_2 =
R"doc(Return the data value associated with the current thread (const
version))doc";

static const char *__doc_mitsuba_ThreadLocalBase_m_constructFunctor = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocalBase_m_destructFunctor = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocalBase_registerThread = R"doc(A new thread was started -- set up local TLS data structures)doc";

static const char *__doc_mitsuba_ThreadLocalBase_staticInitialization = R"doc(Set up core data structures for TLS management)doc";

static const char *__doc_mitsuba_ThreadLocalBase_staticShutdown = R"doc(Destruct core data structures for TLS management)doc";

static const char *__doc_mitsuba_ThreadLocalBase_unregisterThread =
R"doc(A thread has died -- destroy any remaining TLS entries associated with
it)doc";

static const char *__doc_mitsuba_ThreadLocal_ThreadLocal = R"doc(Construct a new thread local storage object)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_T0 = R"doc(Return a reference to the data associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_assign = R"doc(Update the data associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_const_T0 =
R"doc(Return a reference to the data associated with the current thread
(const version))doc";

static const char *__doc_mitsuba_Thread_EPriority = R"doc(Possible priority values for Thread::setPriority())doc";

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

static const char *__doc_mitsuba_Thread_d = R"doc()doc";

static const char *__doc_mitsuba_Thread_detach =
R"doc(Detach the thread and release resources

After a call to this function, join() cannot be used anymore. This
releases resources, which would otherwise be held until a call to
join().)doc";

static const char *__doc_mitsuba_Thread_dispatch = R"doc(Initialize thread execution environment and then call run())doc";

static const char *__doc_mitsuba_Thread_exit = R"doc(Exit the thread, should be called from inside the thread)doc";

static const char *__doc_mitsuba_Thread_getClass = R"doc()doc";

static const char *__doc_mitsuba_Thread_getCoreAffinity = R"doc(Return the core affinity)doc";

static const char *__doc_mitsuba_Thread_getCritical = R"doc(Return the value of the critical flag)doc";

static const char *__doc_mitsuba_Thread_getFileResolver = R"doc(Return the file resolver associated with the current thread)doc";

static const char *__doc_mitsuba_Thread_getFileResolver_2 = R"doc(Return the parent thread (const version))doc";

static const char *__doc_mitsuba_Thread_getID = R"doc(Return a unique ID that is associated with this thread)doc";

static const char *__doc_mitsuba_Thread_getLogger = R"doc(Return the thread's logger instance)doc";

static const char *__doc_mitsuba_Thread_getName = R"doc(Return the name of this thread)doc";

static const char *__doc_mitsuba_Thread_getParent = R"doc(Return the parent thread)doc";

static const char *__doc_mitsuba_Thread_getParent_2 = R"doc(Return the parent thread (const version))doc";

static const char *__doc_mitsuba_Thread_getPriority = R"doc(Return the thread priority)doc";

static const char *__doc_mitsuba_Thread_getThread = R"doc(Return the current thread)doc";

static const char *__doc_mitsuba_Thread_isRunning = R"doc(Is this thread still running?)doc";

static const char *__doc_mitsuba_Thread_join = R"doc(Wait until the thread finishes)doc";

static const char *__doc_mitsuba_Thread_run = R"doc(The thread's run method)doc";

static const char *__doc_mitsuba_Thread_setCoreAffinity =
R"doc(Set the core affinity

This function provides a hint to the operating system scheduler that
the thread should preferably run on the specified processor core. By
default, the parameter is set to -1, which means that there is no
affinity.)doc";

static const char *__doc_mitsuba_Thread_setCritical =
R"doc(Specify whether or not this thread is critical

When an thread marked critical crashes from an uncaught exception, the
whole process is brought down. The default is ``False``.)doc";

static const char *__doc_mitsuba_Thread_setFileResolver = R"doc(Set the file resolver associated with the current thread)doc";

static const char *__doc_mitsuba_Thread_setLogger = R"doc(Set the logger instance used to process log messages from this thread)doc";

static const char *__doc_mitsuba_Thread_setName = R"doc(Set the name of this thread)doc";

static const char *__doc_mitsuba_Thread_setPriority =
R"doc(Set the thread priority

This does not always work -- for instance, Linux requires root
privileges for this operation.

Returns:
    ``True`` upon success.)doc";

static const char *__doc_mitsuba_Thread_sleep = R"doc(Sleep for a certain amount of time (in milliseconds))doc";

static const char *__doc_mitsuba_Thread_start = R"doc(Start the thread)doc";

static const char *__doc_mitsuba_Thread_staticInitialization = R"doc(Initialize the threading system)doc";

static const char *__doc_mitsuba_Thread_staticShutdown = R"doc(Shut down the threading system)doc";

static const char *__doc_mitsuba_Thread_toString = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Thread_yield = R"doc(Yield to another processor)doc";

static const char *__doc_mitsuba_coordinateSystem = R"doc(Complete the set {a} to an orthonormal base {a, b, c})doc";

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
remainder is discarded.)doc";

static const char *__doc_mitsuba_math_clamp = R"doc(Generic range clamping function)doc";

static const char *__doc_mitsuba_math_comp_ellint_1 = R"doc(Complete elliptic integral of the first kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_1_2 = R"doc(Complete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_2 = R"doc(Complete elliptic integral of the second kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_2_2 = R"doc(Complete elliptic integral of the second kind (single precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_3 = R"doc(Complete elliptic integral of the third kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_3_2 = R"doc(Complete elliptic integral of the third kind (single precision))doc";

static const char *__doc_mitsuba_math_degToRad = R"doc(Convert degrees to radians)doc";

static const char *__doc_mitsuba_math_ellint_1 = R"doc(Incomplete elliptic integral of the first kind (double precision))doc";

static const char *__doc_mitsuba_math_ellint_1_2 = R"doc(Incomplete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_ellint_2 = R"doc(Incomplete elliptic integral of the second kind (double precision))doc";

static const char *__doc_mitsuba_math_ellint_2_2 = R"doc(Incomplete elliptic integral of the second kind (single precision))doc";

static const char *__doc_mitsuba_math_ellint_3 = R"doc(Incomplete elliptic integral of the third kind (double precision))doc";

static const char *__doc_mitsuba_math_ellint_3_2 = R"doc(Incomplete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_erf = R"doc(Error function (double precision))doc";

static const char *__doc_mitsuba_math_erf_2 = R"doc(Error function (single precision))doc";

static const char *__doc_mitsuba_math_erfinv = R"doc(Inverse error function (double precision))doc";

static const char *__doc_mitsuba_math_erfinv_2 = R"doc(Inverse error function (single precision))doc";

static const char *__doc_mitsuba_math_findInterval =
R"doc(Find an interval in an ordered set

This function is very similar to ``std::upper_bound``, but it uses a
functor rather than an actual array to permit working with
procedurally defined data. It returns the index ``i`` such that
pred(i) is ``True`` and pred(i+1) is ``False``.

This function is primarily used to locate an interval (i, i+1) for
linear interpolation, hence its name. To avoid issues out of bounds
accesses, and to deal with predicates that evaluate to ``True`` or
``False`` on the entire domain, the returned left interval index is
clamped to the range ``[0, size-2]``.)doc";

static const char *__doc_mitsuba_math_i0e =
R"doc(Exponentially scaled modified Bessel function of the first kind (order
0), double precision)doc";

static const char *__doc_mitsuba_math_i0e_2 =
R"doc(Exponentially scaled modified Bessel function of the first kind (order
0), single precision)doc";

static const char *__doc_mitsuba_math_legendre_p =
R"doc(Evaluate the l-th Legendre polynomial using recurrence, single
precision)doc";

static const char *__doc_mitsuba_math_legendre_p_2 =
R"doc(Evaluate the l-th Legendre polynomial using recurrence, double
precision)doc";

static const char *__doc_mitsuba_math_legendre_p_3 =
R"doc(Evaluate the an associated Legendre polynomial using recurrence,
single precision)doc";

static const char *__doc_mitsuba_math_legendre_p_4 =
R"doc(Evaluate the an associated Legendre polynomial using recurrence,
double precision)doc";

static const char *__doc_mitsuba_math_legendre_pd =
R"doc(Evaluate the l-th Legendre polynomial and its derivative using
recurrence, single precision)doc";

static const char *__doc_mitsuba_math_legendre_pd_2 =
R"doc(Evaluate the l-th Legendre polynomial and its derivative using
recurrence, double precision)doc";

static const char *__doc_mitsuba_math_legendre_pd_diff =
R"doc(Evaluate the function ``legendre_pd(l+1, x) - legendre_pd(l-1, x)``,
single precision)doc";

static const char *__doc_mitsuba_math_legendre_pd_diff_2 =
R"doc(Evaluate the function ``legendre_pd(l+1, x) - legendre_pd(l-1, x)``,
double precision)doc";

static const char *__doc_mitsuba_math_normal_cdf =
R"doc(Cumulative distribution function of the standard normal distribution
(double precision))doc";

static const char *__doc_mitsuba_math_normal_cdf_2 =
R"doc(Cumulative distribution function of the standard normal distribution
(single precision))doc";

static const char *__doc_mitsuba_math_normal_quantile =
R"doc(Quantile function of the standard normal distribution (double
precision))doc";

static const char *__doc_mitsuba_math_normal_quantile_2 =
R"doc(Quantile function of the standard normal distribution (single
precision))doc";

static const char *__doc_mitsuba_math_radToDeg = R"doc(/ Convert radians to degrees)doc";

static const char *__doc_mitsuba_math_safe_acos =
R"doc(Arccosine variant that gracefully handles arguments > 1 due to
roundoff errors)doc";

static const char *__doc_mitsuba_math_safe_asin =
R"doc(Arcsine variant that gracefully handles arguments > 1 due to roundoff
errors)doc";

static const char *__doc_mitsuba_math_safe_sqrt =
R"doc(Square root variant that gracefully handles arguments < 0 due to
roundoff errors)doc";

static const char *__doc_mitsuba_math_signum =
R"doc(Simple signum function -- note that it returns the FP sign of the
input (and never zero))doc";

static const char *__doc_mitsuba_math_ulpdiff =
R"doc(Compare the difference in ULPs between a reference value and another
given floating point number)doc";

static const char *__doc_mitsuba_memcpy_cast = R"doc(Cast between types that have an identical binary representation.)doc";

static const char *__doc_mitsuba_operator_Matrix = R"doc(Convert to an Eigen vector (definition in transform.h))doc";

static const char *__doc_mitsuba_operator_Matrix_2 = R"doc(Convert to an Eigen vector (definition in transform.h))doc";

static const char *__doc_mitsuba_operator_Matrix_3 = R"doc(Convert to an Eigen vector (definition in transform.h))doc";

static const char *__doc_mitsuba_operator_lshift = R"doc(Prints the canonical string representation of an object instance)doc";

static const char *__doc_mitsuba_operator_lshift_2 = R"doc(Prints the canonical string representation of an object instance)doc";

static const char *__doc_mitsuba_operator_lshift_3 = R"doc()doc";

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

static const char *__doc_mitsuba_string_ends_with = R"doc(Check if the given string ends with a specified suffix)doc";

static const char *__doc_mitsuba_string_operator_lshift = R"doc(Turns a vector of elements into a human-readable representation)doc";

static const char *__doc_mitsuba_string_starts_with = R"doc(Check if the given string starts with a specified prefix)doc";

static const char *__doc_mitsuba_string_toLower =
R"doc(Return a lower-case version of the given string (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_toUpper =
R"doc(Return a upper-case version of the given string (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_tokenize =
R"doc(Chop up the string given a set of delimiters (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_util_getCoreCount = R"doc(Determine the number of available CPU cores (including virtual cores))doc";

static const char *__doc_mitsuba_util_getLibraryPath = R"doc(Return the absolute path to <tt>libmitsuba-core.dylib/so/dll<tt>)doc";

static const char *__doc_mitsuba_util_memString = R"doc(Turn a memory size into a human-readable string)doc";

static const char *__doc_mitsuba_util_mkString =
R"doc(Joins elements of ``v`` into a string, separated by an optional
delimiter.)doc";

static const char *__doc_mitsuba_util_timeString =
R"doc(Convert a time difference (in seconds) to a string representation

Parameter ``time``:
    Time difference in (fractional) sections

Parameter ``precise``:
    When set to true, a higher-precision string representation is
    generated.)doc";

static const char *__doc_mitsuba_util_trapDebugger =
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

static const char *__doc_mitsuba_xml_loadFile = R"doc(Load a Mitsuba scene from an XML file)doc";

static const char *__doc_mitsuba_xml_loadString = R"doc(Load a Mitsuba scene from an XML string)doc";

static const char *__doc_operator_lshift = R"doc()doc";

static const char *__doc_operator_lshift_2 = R"doc()doc";

static const char *__doc_operator_mul = R"doc()doc";

static const char *__doc_operator_mul_2 = R"doc()doc";

static const char *__doc_pcg32 = R"doc(PCG32 Pseudorandom number generator)doc";

static const char *__doc_pcg32_8 = R"doc(8 parallel PCG32 pseudorandom number generators)doc";

static const char *__doc_pcg32_8_nextDouble =
R"doc(Generate eight double precision floating point value on the interval
[0, 1)

Remark:
    Since the underlying random number generator produces 32 bit
    output, only the first 32 mantissa bits will be filled (however,
    the resolution is still finer than in nextFloat(), which only uses
    23 mantissa bits))doc";

static const char *__doc_pcg32_8_nextFloat =
R"doc(Generate eight single precision floating point value on the interval
[0, 1))doc";

static const char *__doc_pcg32_8_nextUInt = R"doc(Generate 8 uniformly distributed unsigned 32-bit random numbers)doc";

static const char *__doc_pcg32_8_pcg32_8 = R"doc(Initialize the pseudorandom number generator with default seed)doc";

static const char *__doc_pcg32_8_pcg32_8_2 = R"doc(Initialize the pseudorandom number generator with the seed() function)doc";

static const char *__doc_pcg32_8_rng = R"doc()doc";

static const char *__doc_pcg32_8_seed =
R"doc(Seed the pseudorandom number generator

Specified in two parts: a state initializer and a sequence selection
constant (a.k.a. stream id))doc";

static const char *__doc_pcg32_advance =
R"doc(Multi-step advance function (jump-ahead, jump-back)

The method used here is based on Brown, "Random Number Generation with
Arbitrary Stride", Transactions of the American Nuclear Society (Nov.
1994). The algorithm is very similar to fast exponentiation.)doc";

static const char *__doc_pcg32_inc = R"doc()doc";

static const char *__doc_pcg32_nextDouble =
R"doc(Generate a double precision floating point value on the interval [0,
1)

Remark:
    Since the underlying random number generator produces 32 bit
    output, only the first 32 mantissa bits will be filled (however,
    the resolution is still finer than in nextFloat(), which only uses
    23 mantissa bits))doc";

static const char *__doc_pcg32_nextFloat =
R"doc(Generate a single precision floating point value on the interval [0,
1))doc";

static const char *__doc_pcg32_nextUInt = R"doc(Generate a uniformly distributed unsigned 32-bit random number)doc";

static const char *__doc_pcg32_nextUInt_2 = R"doc(Generate a uniformly distributed number, r, where 0 <= r < bound)doc";

static const char *__doc_pcg32_operator_eq = R"doc(Equality operator)doc";

static const char *__doc_pcg32_operator_ne = R"doc(Inequality operator)doc";

static const char *__doc_pcg32_operator_sub = R"doc(Compute the distance between two PCG32 pseudorandom number generators)doc";

static const char *__doc_pcg32_pcg32 = R"doc(Initialize the pseudorandom number generator with default seed)doc";

static const char *__doc_pcg32_pcg32_2 = R"doc(Initialize the pseudorandom number generator with the seed() function)doc";

static const char *__doc_pcg32_seed =
R"doc(Seed the pseudorandom number generator

Specified in two parts: a state initializer and a sequence selection
constant (a.k.a. stream id))doc";

static const char *__doc_pcg32_shuffle =
R"doc(Draw uniformly distributed permutation and permute the given STL
container

From: Knuth, TAoCP Vol. 2 (3rd 3d), Section 3.4.2)doc";

static const char *__doc_pcg32_state = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

