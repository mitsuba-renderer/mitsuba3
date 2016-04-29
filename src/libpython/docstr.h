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


static const char *__doc_mitsuba_Appender = R"doc(This class defines an abstract destination for logging-relevant information)doc";

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
    progress message. When rendering a scene, it will usually contain a
    pointer to the associated ``RenderJob``.)doc";

static const char *__doc_mitsuba_AtomicFloat =
R"doc(Atomic floating point data type

The class implements an an atomic floating point data type (which is not
possible with the existing overloads provided by ``std::atomic``). It
internally casts floating point values to an integer storage format and
uses integer compare and exchange operations to perform changes atomically.)doc";

static const char *__doc_mitsuba_AtomicFloat_AtomicFloat = R"doc(Initialize the AtomicFloat with a given floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_do_atomic =
R"doc(Apply a FP operation atomically (verified that this will be nicely inlined
in the above iterators))doc";

static const char *__doc_mitsuba_AtomicFloat_m_bits = R"doc()doc";

static const char *__doc_mitsuba_AtomicFloat_operator_T0 = R"doc(Convert the AtomicFloat into a normal floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_assign = R"doc(Overwrite the AtomicFloat with a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_iadd = R"doc(Atomically add a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_idiv = R"doc(Atomically divide by a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_imul = R"doc(Atomically multiply by a floating point value)doc";

static const char *__doc_mitsuba_AtomicFloat_operator_isub = R"doc(Atomically subtract a floating point value)doc";

static const char *__doc_mitsuba_Class =
R"doc(\headerfile mitsuba/core/class.h mitsuba/mitsuba.h Stores meta-information
about Object instances.

This class provides a thin layer of RTTI (run-time type information), which
is useful for doing things like:

<ul> <li> Checking if an object derives from a certain class </li> <li>
Determining the parent of a class at runtime </li> <li> Instantiating a
class by name </li> <li> Unserializing a class from a binary data stream
</li> </ul>

See also:
    ref, Object)doc";

static const char *__doc_mitsuba_Class_Class =
R"doc(Construct a new class descriptor

This method should never be called manually. Instead, use one of the
MTS_IMPLEMENT_CLASS, MTS_IMPLEMENT_CLASS_S, MTS_IMPLEMENT_CLASS_I or
MTS_IMPLEMENT_CLASS_IS macros to automatically do this for you.

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

static const char *__doc_mitsuba_Class_construct = R"doc(Generate an instance of this class (if this is supported))doc";

static const char *__doc_mitsuba_Class_derivesFrom = R"doc(Check whether this class derives from *theClass*)doc";

static const char *__doc_mitsuba_Class_forName = R"doc(Look up a class by its name)doc";

static const char *__doc_mitsuba_Class_getName = R"doc(Return the name of the represented class)doc";

static const char *__doc_mitsuba_Class_getParent =
R"doc(Return the Class object associated with the parent class of nullptr if it
does not have one.)doc";

static const char *__doc_mitsuba_Class_initializeOnce = R"doc(Initialize a class - called by staticInitialization())doc";

static const char *__doc_mitsuba_Class_isAbstract =
R"doc(Return whether or not the class represented by this Class object contains
pure virtual methods)doc";

static const char *__doc_mitsuba_Class_isConstructible = R"doc(Does the class support instantiation over RTTI?)doc";

static const char *__doc_mitsuba_Class_isSerializable = R"doc(Does the class support serialization?)doc";

static const char *__doc_mitsuba_Class_m_abstract = R"doc()doc";

static const char *__doc_mitsuba_Class_m_constr = R"doc()doc";

static const char *__doc_mitsuba_Class_m_name = R"doc()doc";

static const char *__doc_mitsuba_Class_m_parent = R"doc()doc";

static const char *__doc_mitsuba_Class_m_parentName = R"doc()doc";

static const char *__doc_mitsuba_Class_m_unser = R"doc()doc";

static const char *__doc_mitsuba_Class_rttiIsInitialized = R"doc(Check if the RTTI layer has been initialized)doc";

static const char *__doc_mitsuba_Class_staticInitialization = R"doc(Initializes the built-in RTTI and creates a list of all compiled classes)doc";

static const char *__doc_mitsuba_Class_staticShutdown = R"doc(Free the memory taken by staticInitialization())doc";

static const char *__doc_mitsuba_Class_unserialize = R"doc(Unserialize an instance of the class (if this is supported).)doc";

static const char *__doc_mitsuba_DefaultFormatter = R"doc(The default formatter used to turn log messages into a human-readable form)doc";

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

static const char *__doc_mitsuba_ELogLevel = R"doc(Available Log message types)doc";

static const char *__doc_mitsuba_Formatter =
R"doc(Abstract interface for converting log information into a human-readable
format)doc";

static const char *__doc_mitsuba_Formatter_format =
R"doc(Turn a log message into a human-readable format

Parameter ``level``:
    The importance of the debug message

Parameter ``theClass``:
    Originating class or nullptr

Parameter ``thread``:
    Thread, which is reponsible for creating the message

Parameter ``text``:
    Text content associated with the log message

Parameter ``file``:
    File, which is responsible for creating the message

Parameter ``line``:
    Associated line within the source file)doc";

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
    printf-style string formatter \note This function is not exposed in the
    Python bindings. Instead, please use \cc mitsuba.core.Log)doc";

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
    progress message. When rendering a scene, it will usually contain a
    pointer to the associated ``RenderJob``.)doc";

static const char *__doc_mitsuba_Logger_m_logLevel = R"doc()doc";

static const char *__doc_mitsuba_Logger_readLog =
R"doc(Return the contents of the log file as a string

Throws a runtime exception upon failure)doc";

static const char *__doc_mitsuba_Logger_removeAppender = R"doc(Remove an appender from this logger)doc";

static const char *__doc_mitsuba_Logger_setErrorLevel =
R"doc(Set the error log level (this level and anything above will throw
exceptions).

The value provided here can be used for instance to turn warnings into
errors. But *level* must always be less than EError, i.e. it isn't possible
to cause errors not to throw an exception.)doc";

static const char *__doc_mitsuba_Logger_setFormatter = R"doc(Set the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_setLogLevel = R"doc(Set the log level (everything below will be ignored))doc";

static const char *__doc_mitsuba_Logger_staticInitialization = R"doc(Initialize logging)doc";

static const char *__doc_mitsuba_Logger_staticShutdown = R"doc(Shutdown logging)doc";

static const char *__doc_mitsuba_Object = R"doc(Reference counted object base class)doc";

static const char *__doc_mitsuba_Object_Object = R"doc(Default constructor)doc";

static const char *__doc_mitsuba_Object_Object_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Object_decRef =
R"doc(Decrease the reference count of the object and possibly deallocate it.

The object will automatically be deallocated once the reference count
reaches zero.)doc";

static const char *__doc_mitsuba_Object_getClass =
R"doc(Return a Class instance containing run-time type information about this
Object

See also:
    Class)doc";

static const char *__doc_mitsuba_Object_getRefCount = R"doc(Return the current reference count)doc";

static const char *__doc_mitsuba_Object_incRef = R"doc(Increase the object's reference count by one)doc";

static const char *__doc_mitsuba_Object_m_refCount = R"doc()doc";

static const char *__doc_mitsuba_Object_toString =
R"doc(Return a human-readable string representation of the object's contents.

This function is mainly useful for debugging purposes and should ideally be
implemented by all subclasses. The default implementation simply returns
<tt>MyObject[<address of 'this' pointer>]</tt>, where ``MyObject`` is the
name of the class.)doc";

static const char *__doc_mitsuba_Properties =
R"doc(Associative parameter map for constructing subclasses of
ConfigurableObject.

Note that the Python bindings for this class do not implement the various
type-dependent getters and setters. Instead, they are accessed just like a
normal Python map, e.g:

\code myProps = mitsuba.core.Properties("pluginName")
myProps["stringProperty"] = "hello" myProps["spectrumProperty"] =
mitsuba.core.Spectrum(1.0) \endcode)doc";

static const char *__doc_mitsuba_Properties_Properties = R"doc()doc";

static const char *__doc_mitsuba_Properties_Properties_2 = R"doc()doc";

static const char *__doc_mitsuba_Properties_d = R"doc()doc";

static const char *__doc_mitsuba_Properties_getBoolean = R"doc(Retrieve a boolean value)doc";

static const char *__doc_mitsuba_Properties_getBoolean_2 = R"doc(Retrieve a boolean value (use default value if no entry exists))doc";

static const char *__doc_mitsuba_Properties_hasProperty = R"doc(Verify if a value with the specified name exists)doc";

static const char *__doc_mitsuba_Properties_setBoolean = R"doc(Set a boolean value)doc";

static const char *__doc_mitsuba_StreamAppender = R"doc(%Appender implementation, which writes to an arbitrary C++ output stream)doc";

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

static const char *__doc_mitsuba_Thread =
R"doc(Cross-platform thread implementation

Mitsuba threads are internally implemented via the ``std::thread`` class
defined in C++11. This wrapper class is needed to attach additional state
(Loggers, Path resolvers, etc.) that is inherited when a thread launches
another thread.)doc";

static const char *__doc_mitsuba_ThreadLocal =
R"doc(Flexible platform-independent thread local storage class

This class implements a generic thread local storage object. For details,
refer to its base class, ThreadLocalBase.)doc";

static const char *__doc_mitsuba_ThreadLocalBase =
R"doc(Flexible platform-independent thread local storage class

This class implements a generic thread local storage object that can be
used in situations where the new ``thread_local`` keyword is not available
(e.g. on Mac OS, as of 2016), or when TLS object are created dynamically
(which ``thread_local`` does not allow).

The native TLS classes on Linux/MacOS/Windows only support a limited number
of dynamically allocated entries (usually 1024 or 1088). Furthermore, they
do not provide appropriate cleanup semantics when the TLS object or one of
the assocated threads dies. The custom TLS code provided by this class has
no such limits (caching in various subsystems of Mitsuba may create a huge
amount, so this is a big deal), and it also has the desired cleanup
semantics: TLS entries are destroyed when the owning thread dies *or* when
the ``ThreadLocal`` instance is freed (whichever occurs first).

The implementation is designed to make the ``get``() operation as fast as
as possible at the cost of more involved locking when creating or
destroying threads and TLS objects. To actually instantiate a TLS object
with a specific type, use to the ThreadLocal class.

See also:
    ThreadLocal)doc";

static const char *__doc_mitsuba_ThreadLocalBase_ThreadLocalBase = R"doc(Construct a new thread local storage object)doc";

static const char *__doc_mitsuba_ThreadLocalBase_get = R"doc(Return the data value associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocalBase_get_2 = R"doc(Return the data value associated with the current thread (const version))doc";

static const char *__doc_mitsuba_ThreadLocalBase_m_constructFunctor = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocalBase_m_destructFunctor = R"doc()doc";

static const char *__doc_mitsuba_ThreadLocalBase_registerThread = R"doc(A new thread was started -- set up local TLS data structures)doc";

static const char *__doc_mitsuba_ThreadLocalBase_staticInitialization = R"doc(Set up core data structures for TLS management)doc";

static const char *__doc_mitsuba_ThreadLocalBase_staticShutdown = R"doc(Destruct core data structures for TLS management)doc";

static const char *__doc_mitsuba_ThreadLocalBase_unregisterThread = R"doc(A thread has died -- destroy any remaining TLS entries associated with it)doc";

static const char *__doc_mitsuba_ThreadLocal_ThreadLocal = R"doc(Construct a new thread local storage object)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_T0 = R"doc(Return a reference to the data associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_assign = R"doc(Update the data associated with the current thread)doc";

static const char *__doc_mitsuba_ThreadLocal_operator_const_T0 =
R"doc(Return a reference to the data associated with the current thread (const
version))doc";

static const char *__doc_mitsuba_Thread_EPriority = R"doc(Possible priority values for Thread::setPriority())doc";

static const char *__doc_mitsuba_Thread_Thread =
R"doc(Create a new thread object

Parameter ``name``:
    An identifying name of this thread (will be shown in debug messages))doc";

static const char *__doc_mitsuba_Thread_d = R"doc()doc";

static const char *__doc_mitsuba_Thread_detach =
R"doc(Detach the thread and release resources

After a call to this function, join() cannot be used anymore. This releases
resources, which would otherwise be held until a call to join().)doc";

static const char *__doc_mitsuba_Thread_dispatch = R"doc(Initialize thread execution environment and then call run())doc";

static const char *__doc_mitsuba_Thread_exit = R"doc(Exit the thread, should be called from inside the thread)doc";

static const char *__doc_mitsuba_Thread_getClass = R"doc()doc";

static const char *__doc_mitsuba_Thread_getCoreAffinity = R"doc(Return the core affinity)doc";

static const char *__doc_mitsuba_Thread_getCritical = R"doc(Return the value of the critical flag)doc";

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

This function provides a hint to the operating system scheduler that the
thread should preferably run on the specified processor core. By default,
the parameter is set to -1, which means that there is no affinity.)doc";

static const char *__doc_mitsuba_Thread_setCritical =
R"doc(Specify whether or not this thread is critical

When an thread marked critical crashes from an uncaught exception, the
whole process is brought down. The default is ``False``.)doc";

static const char *__doc_mitsuba_Thread_setLogger = R"doc(Set the logger instance used to process log messages from this thread)doc";

static const char *__doc_mitsuba_Thread_setName = R"doc(Set the name of this thread)doc";

static const char *__doc_mitsuba_Thread_setPriority =
R"doc(Set the thread priority

This does not always work -- for instance, Linux requires root privileges
for this operation.

Returns:
    ``True`` upon success.)doc";

static const char *__doc_mitsuba_Thread_sleep = R"doc(Sleep for a certain amount of time (in milliseconds))doc";

static const char *__doc_mitsuba_Thread_start = R"doc(Start the thread)doc";

static const char *__doc_mitsuba_Thread_staticInitialization = R"doc(Initialize the threading system)doc";

static const char *__doc_mitsuba_Thread_staticShutdown = R"doc(Shut down the threading system)doc";

static const char *__doc_mitsuba_Thread_toString = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Thread_yield = R"doc(Yield to another processor)doc";

static const char *__doc_mitsuba_detail_get_construct_functor = R"doc()doc";

static const char *__doc_mitsuba_detail_get_construct_functor_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_get_unserialize_functor = R"doc()doc";

static const char *__doc_mitsuba_detail_get_unserialize_functor_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_copy = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_destruct = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_move = R"doc()doc";

static const char *__doc_mitsuba_filesystem_create_directory = R"doc()doc";

static const char *__doc_mitsuba_filesystem_current_path = R"doc(Returns the current working directory (equivalent to getcwd))doc";

static const char *__doc_mitsuba_filesystem_exists = R"doc()doc";

static const char *__doc_mitsuba_filesystem_file_size = R"doc()doc";

static const char *__doc_mitsuba_filesystem_is_directory = R"doc()doc";

static const char *__doc_mitsuba_filesystem_is_regular_file = R"doc()doc";

static const char *__doc_mitsuba_filesystem_make_absolute = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_clear = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_empty = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_extension =
R"doc(Returns the extension of the filename component of the path (the substring
starting at the rightmost period, including the period). Special paths '.'
and '..' have an empty extension.)doc";

static const char *__doc_mitsuba_filesystem_path_filename = R"doc(Returns the filename component of the path, including the extension.)doc";

static const char *__doc_mitsuba_filesystem_path_is_absolute = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_is_relative = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_m_absolute = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_m_path = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_native = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign_3 = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_basic_string = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_div = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_parent_path =
R"doc(Returns the path to the parent directory. Returns the empty path if it
already empty or if it has only one element.)doc";

static const char *__doc_mitsuba_filesystem_path_path = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_path_2 = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_path_3 = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_path_4 = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_path_5 = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_set = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_str = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_tokenize = R"doc()doc";

static const char *__doc_mitsuba_filesystem_remove = R"doc(Removes the file at the passed path)doc";

static const char *__doc_mitsuba_filesystem_resize_file = R"doc()doc";

static const char *__doc_mitsuba_getCoreCount = R"doc(Determine the number of available CPU cores (including virtual cores))doc";

static const char *__doc_mitsuba_math_clamp = R"doc(Generic range clamping function)doc";

static const char *__doc_mitsuba_math_comp_ellint_1 = R"doc(Complete elliptic integral of the first kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_1_2 = R"doc(Complete elliptic integral of the first kind (single precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_2 = R"doc(Complete elliptic integral of the second kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_2_2 = R"doc(Complete elliptic integral of the second kind (single precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_3 = R"doc(Complete elliptic integral of the third kind (double precision))doc";

static const char *__doc_mitsuba_math_comp_ellint_3_2 = R"doc(Complete elliptic integral of the third kind (single precision))doc";

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
functor rather than an actual array to permit working with procedurally
defined data. It returns the index ``i`` such that pred(i) is ``True`` and
pred(i+1) is ``False``.

This function is primarily used to locate an interval (i, i+1) for linear
interpolation, hence its name. To avoid issues out of bounds accesses, and
to deal with predicates that evaluate to ``True`` or ``False`` on the
entire domain, the returned left interval index is clamped to the range
``[0, size-2]``.)doc";

static const char *__doc_mitsuba_math_i0e =
R"doc(Exponentially scaled modified Bessel function of the first kind (order 0),
double precision)doc";

static const char *__doc_mitsuba_math_i0e_2 =
R"doc(Exponentially scaled modified Bessel function of the first kind (order 0),
single precision)doc";

static const char *__doc_mitsuba_math_legendre_p = R"doc(Evaluate the l-th Legendre polynomial using recurrence, single precision)doc";

static const char *__doc_mitsuba_math_legendre_p_2 = R"doc(Evaluate the l-th Legendre polynomial using recurrence, double precision)doc";

static const char *__doc_mitsuba_math_legendre_p_3 =
R"doc(Evaluate the an associated Legendre polynomial using recurrence, single
precision)doc";

static const char *__doc_mitsuba_math_legendre_p_4 =
R"doc(Evaluate the an associated Legendre polynomial using recurrence, double
precision)doc";

static const char *__doc_mitsuba_math_legendre_pd =
R"doc(Evaluate the l-th Legendre polynomial and its derivative using recurrence,
single precision)doc";

static const char *__doc_mitsuba_math_legendre_pd_2 =
R"doc(Evaluate the l-th Legendre polynomial and its derivative using recurrence,
double precision)doc";

static const char *__doc_mitsuba_math_legendre_pd_diff =
R"doc(Evaluate the function ``legendre_pd(l+1, x) - legendre_pd(l-1, x)``, single
precision)doc";

static const char *__doc_mitsuba_math_legendre_pd_diff_2 =
R"doc(Evaluate the function ``legendre_pd(l+1, x) - legendre_pd(l-1, x)``, double
precision)doc";

static const char *__doc_mitsuba_math_normal_cdf =
R"doc(Cumulative distribution function of the standard normal distribution
(double precision))doc";

static const char *__doc_mitsuba_math_normal_cdf_2 =
R"doc(Cumulative distribution function of the standard normal distribution
(single precision))doc";

static const char *__doc_mitsuba_math_normal_quantile = R"doc(Quantile function of the standard normal distribution (double precision))doc";

static const char *__doc_mitsuba_math_normal_quantile_2 = R"doc(Quantile function of the standard normal distribution (single precision))doc";

static const char *__doc_mitsuba_math_safe_acos =
R"doc(Arccosine variant that gracefully handles arguments > 1 due to roundoff
errors)doc";

static const char *__doc_mitsuba_math_safe_asin =
R"doc(Arcsine variant that gracefully handles arguments > 1 due to roundoff
errors)doc";

static const char *__doc_mitsuba_math_safe_sqrt =
R"doc(Square root variant that gracefully handles arguments < 0 due to roundoff
errors)doc";

static const char *__doc_mitsuba_math_signum =
R"doc(Simple signum function -- note that it returns the FP sign of the input
(and never zero))doc";

static const char *__doc_mitsuba_memcpy_cast = R"doc(Cast between types that have an identical binary representation.)doc";

static const char *__doc_mitsuba_ref =
R"doc(Reference counting helper

The *ref* template is a simple wrapper to store a pointer to an object. It
takes care of increasing and decreasing the object's reference count as
needed. When the last reference goes out of scope, the associated object
will be deallocated.

The advantage over C++ solutions such as ``std::shared_ptr`` is that the
reference count is very compactly integrated into the base object itself.)doc";

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

static const char *__doc_mitsuba_variant = R"doc()doc";

static const char *__doc_mitsuba_variant_data = R"doc()doc";

static const char *__doc_mitsuba_variant_empty = R"doc()doc";

static const char *__doc_mitsuba_variant_is = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign_3 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_assign_4 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_const_type_parameter_1_0 = R"doc()doc";

static const char *__doc_mitsuba_variant_operator_type_parameter_1_0 = R"doc()doc";

static const char *__doc_mitsuba_variant_type = R"doc()doc";

static const char *__doc_mitsuba_variant_type_info = R"doc()doc";

static const char *__doc_mitsuba_variant_variant = R"doc()doc";

static const char *__doc_mitsuba_variant_variant_2 = R"doc()doc";

static const char *__doc_mitsuba_variant_variant_3 = R"doc()doc";

static const char *__doc_pcg32 = R"doc(PCG32 Pseudorandom number generator)doc";

static const char *__doc_pcg32_advance =
R"doc(Multi-step advance function (jump-ahead, jump-back)

The method used here is based on Brown, "Random Number Generation with
Arbitrary Stride", Transactions of the American Nuclear Society (Nov.
1994). The algorithm is very similar to fast exponentiation.)doc";

static const char *__doc_pcg32_inc = R"doc()doc";

static const char *__doc_pcg32_nextDouble =
R"doc(Generate a double precision floating point value on the interval [0, 1)

Remark:
    Since the underlying random number generator produces 32 bit output,
    only the first 32 mantissa bits will be filled (however, the resolution
    is still finer than in nextFloat(), which only uses 23 mantissa bits))doc";

static const char *__doc_pcg32_nextFloat = R"doc(Generate a single precision floating point value on the interval [0, 1))doc";

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
R"doc(Draw uniformly distributed permutation and permute the given STL container

From: Knuth, TAoCP Vol. 2 (3rd 3d), Section 3.4.2)doc";

static const char *__doc_pcg32_state = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

