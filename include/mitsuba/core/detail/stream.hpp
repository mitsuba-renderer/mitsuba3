#pragma once

#include <set>
#include <vector>

#if defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <Eigen/Core>

#if defined(__GNUG__)
#  pragma GCC diagnostic pop
#endif

#if defined(__WINDOWS__)
    #include <intrin.h>
#endif

/* \brief Basic Stream serialization capabilities can go here.
 * This header file contains template specializations to add support for
 * serialization of several basic files. Support for new types can be added
 * in other headers by providing the appropriate template specializations.
 */

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

template <typename T> T swap16(const T &v) {
#if !defined(__WINDOWS__)
    return memcpy_cast<T>(__builtin_bswap16(memcpy_cast<uint16_t>(v)));
#else
    return memcpy_cast<T>(_byteswap_ushort(memcpy_cast<uint16_t>(v)));
#endif
}

template <typename T> T swap32(const T &v) {
#if !defined(__WINDOWS__)
    return memcpy_cast<T>(__builtin_bswap32(memcpy_cast<uint32_t>(v)));
#else
    return memcpy_cast<T>(_byteswap_ulong(memcpy_cast<uint32_t>(v)));
#endif
}

template <typename T> T swap64(const T &v) {
#if !defined(__WINDOWS__)
    return memcpy_cast<T>(__builtin_bswap64(memcpy_cast<uint64_t>(v)));
#else
    return memcpy_cast<T>(_byteswap_uint64(memcpy_cast<uint64_t>(v)));
#endif
}

/**
 * \brief The <tt>serialization_trait</tt> templated structure provides
 * a unique type identifier for each supported type.
 * <tt>type_id</tt> is a unique, prefix-free code identifying the type.
 */
template <typename T, typename SFINAE = void> struct serialization_traits { };
template <> struct serialization_traits<int8_t> {
    const char *type_id = "u8";
    static int8_t swap(const int8_t &v) { return v; };
};
template <> struct serialization_traits<uint8_t>  {
    const char *type_id = "s8";
    static uint8_t swap(const uint8_t &v) { return v; };
};
template <> struct serialization_traits<int16_t>  {
    const char *type_id = "u16";
    static int16_t swap(const int16_t &v) { return swap16(v); };
};
template <> struct serialization_traits<uint16_t> {
    const char *type_id = "s16";
    static uint16_t swap(const uint16_t &v) { return swap16(v); };
};
template <> struct serialization_traits<int32_t>  {
    const char *type_id = "u32";
    static int32_t swap(const int32_t &v) { return swap32(v); };
};
template <> struct serialization_traits<uint32_t> {
    const char *type_id = "s32";
    static uint32_t swap(const uint32_t &v) { return swap32(v); };
};
template <> struct serialization_traits<int64_t>  {
    const char *type_id = "u64";
    static int64_t swap(const int64_t &v) { return swap64(v); };
};
template <> struct serialization_traits<uint64_t> {
    const char *type_id = "s64";
    static uint64_t swap(const uint64_t &v) { return swap64(v); };
};
template <> struct serialization_traits<float>    {
    const char *type_id = "f32";
    static float swap(const float &v) { return swap32(v); };
};
template <> struct serialization_traits<double>   {
    const char *type_id = "f64";
    static double swap(const double &v) { return swap64(v); };
};
template <> struct serialization_traits<bool>     {
    const char *type_id = "b8";
    static bool swap(const bool &v) { return v; };
};
template <> struct serialization_traits<char>     {
    const char *type_id = "c8";
    static char swap(const char &v) { return v; };
};

template <typename T> struct serialization_traits<T> :
    serialization_traits<typename std::underlying_type<T>::type,
        typename std::enable_if<std::is_enum<T>::value>::type> { };

/** \brief The serialization_helper<T> implementations for new types should
 * in general be implemented as a series of calls to the lower-level
 * serialization_helper::{read,write} functions.
 * This way, endianness swapping needs only be handled at the lowest level.
 */
template <typename T> struct serialization_helper {
    static std::string type_id() { return serialization_traits<T>().type_id; }

    /** \brief Writes <tt>count</tt> values of type T into stream <tt>s</tt>
     * starting at its current position.
     * Note: <tt>count</tt> is the number of values, <b>not</b> a size in bytes.
     *
     * Support for additional types can be added in any header file by
     * declaring a template specialization for your type.
     */
    static void write(Stream &s, const T *value, size_t count, bool swap) {
        if (!swap) {
            s.write(value, sizeof(T) * count);
        } else {
            std::unique_ptr<T[]> v(new T[count]);
            for (size_t i = 0; i < count; ++i) {
                v[i] = serialization_traits<T>::swap(*value);
                value++;
            }
            s.write(v.get(), sizeof(T) * count);
        }
    }

    /** \brief Reads <tt>count</tt> values of type T from stream <tt>s</tt>,
     * starting at its current position.
     * Note: <tt>count</tt> is the number of values, <b>not</b> a size in bytes.
     *
     * Support for additional types can be added in any header file by
     * declaring a template specialization for your type.
     */
    static void read(Stream &s, T *value, size_t count, bool swap) {
        s.read(value, sizeof(T) * count);
        if (swap) {
            for (size_t i = 0; i < count; ++i) {
                value[i] = serialization_traits<T>::swap(value[i]);
            }
        }
    }
};

template <> struct serialization_helper<std::string> {
    static std::string type_id() { return "Vc8"; }

    static void write(Stream &s, const std::string *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t length = static_cast<uint32_t>(value->length());
            serialization_helper<uint32_t>::write(s, &length, 1, swap);
            serialization_helper<char>::write(s, value->data(),
                                              value->length(), swap);
            value++;
        }
    }

    static void read(Stream &s, std::string *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t length;
            serialization_helper<uint32_t>::read(s, &length, 1, swap);
            value->resize(length);
            serialization_helper<char>::read(s, const_cast<char *>(value->data()),
                                             length, swap);
            value++;
        }
    }
};


template <typename T1, typename T2> struct serialization_helper<std::pair<T1, T2>> {
    static std::string type_id() {
        return "P" +
               serialization_helper<T1>::type_id() +
               serialization_helper<T2>::type_id();
    }

    static void write(Stream &s, const std::pair<T1, T1> *value, size_t count, bool swap) {
        std::unique_ptr<T1> first (new T1[count]);
        std::unique_ptr<T2> second(new T2[count]);

        for (size_t i = 0; i<count; ++i) {
            first.get()[i]  = value[i].first;
            second.get()[i] = value[i].second;
        }

        serialization_helper<T1>::write(s, first.get(), count, swap);
        serialization_helper<T2>::write(s, second.get(), count, swap);
    }

    static void read(Stream &s, std::pair<T1, T1> *value, size_t count, bool swap) {
        std::unique_ptr<T1> first (new T1[count]);
        std::unique_ptr<T2> second(new T2[count]);

        serialization_helper<T1>::read(s, first.get(), count, swap);
        serialization_helper<T2>::read(s, second.get(), count, swap);

        for (size_t i = 0; i<count; ++i) {
            value[i].first = first.get()[i];
            value[i].second = second.get()[i];
        }
    }
};

template <typename T> struct serialization_helper<std::vector<T>> {
    static std::string type_id() {
        return "V" + serialization_helper<T>::type_id();
    }

    static void write(Stream &s, const std::vector<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i<count; ++i) {
            uint32_t size = static_cast<uint32_t>(value->size());
            serialization_helper<uint32_t>::write(s, &size, 1, swap);
            serialization_helper<T>::write(s, value->data(), size, swap);
            value++;
        }
    }

    static void read(Stream &s, std::vector<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i<count; ++i) {
            uint32_t size = 0;
            serialization_helper<uint32_t>::read(s, &size, 1, swap);
            value->resize(size);
            serialization_helper<T>::read(s, value->data(), size, swap);
            value++;
        }
    }
};

template <typename T> struct serialization_helper<std::set<T>> {
    static std::string type_id() {
        return "S" + serialization_helper<T>::type_id();
    }

    static void write(Stream &s, const std::set<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i<count; ++i) {
            std::vector<T> temp(value->size());
            uint32_t idx = 0;
            for (auto it = value->begin(); it != value->end(); ++it)
                temp[idx++] = *it;
            serialization_helper<std::vector<T>>::write(s, &temp, 1, swap);
            value++;
        }
    }

    static void read(Stream &s, std::set<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i<count; ++i) {
            std::vector<T> temp;
            serialization_helper<std::vector<T>>::read(s, &temp, 1, swap);
            value->clear();
            for (auto k : temp)
                value->insert(k);
            value++;
        }
    }
};

template <typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
struct serialization_helper<Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>> {
    typedef Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols> Matrix;

    static std::string type_id() {
        return "M" + serialization_helper<Scalar>::type_id();
    }

    static void write(Stream &s, const Matrix *value, size_t count, bool swap) {
        for (size_t i = 0; i<count; ++i) {
            uint32_t rows = static_cast<uint32_t>(value->rows()),
                     cols = static_cast<uint32_t>(value->cols());
            serialization_helper<uint32_t >::write(s, &rows, 1, swap);
            serialization_helper<uint32_t >::write(s, &cols, 1, swap);
            serialization_helper<Scalar>::write(s, value->data(), rows*cols, swap);
            value++;
        }
    }

    static void read(Stream &s, Matrix *value, size_t count, bool swap) {
        for (size_t i = 0; i<count; ++i) {
            uint32_t rows = 0, cols = 0;
            serialization_helper<uint32_t>::read(s, &rows, 1, swap);
            serialization_helper<uint32_t>::read(s, &cols, 1, swap);
            value->resize(rows, cols);
            serialization_helper<Scalar>::read(s, value->data(), rows*cols, swap);
            value++;
        }
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
