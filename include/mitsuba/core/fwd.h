#pragma once

#include <mitsuba/core/platform.h>

NAMESPACE_BEGIN(mitsuba)

class Object;
class Class;
template <typename> class ref;

class Appender;
class DefaultFormatter;
class FileResolver;
class Formatter;
class Logger;
class Mutex;
class Properties;
class Stream;
class StreamAppender;
class Thread;
class ThreadLocalBase;
enum ELogLevel : int;
template <typename, typename> class ThreadLocal;
template <typename> class AtomicFloat;

NAMESPACE_END(mitsuba)
