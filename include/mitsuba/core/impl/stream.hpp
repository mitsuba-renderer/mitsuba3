#pragma once

/** \brief Basic Stream serialization capabilities can go here.
 * This header file contains template specializations to add support for
 * serialization of several basic files. Support for new types can be added
 * in other headers by providing the appropriate template specializations.
 */

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

/**
 * \brief The <tt>serialization_trait</tt> templated structure provides
 * a unique type identifier for each supported type.
 * <tt>type_id</tt> is a unique, prefix-free code identifying the type.
 */
template <typename T, typename SFINAE = void> struct serialization_traits { };
template <> struct serialization_traits<int8_t>           { const char *type_id = "u8";  };
template <> struct serialization_traits<uint8_t>          { const char *type_id = "s8";  };
template <> struct serialization_traits<int16_t>          { const char *type_id = "u16"; };
template <> struct serialization_traits<uint16_t>         { const char *type_id = "s16"; };
template <> struct serialization_traits<int32_t>          { const char *type_id = "u32"; };
template <> struct serialization_traits<uint32_t>         { const char *type_id = "s32"; };
template <> struct serialization_traits<int64_t>          { const char *type_id = "u64"; };
template <> struct serialization_traits<uint64_t>         { const char *type_id = "s64"; };
template <> struct serialization_traits<float>            { const char *type_id = "f32"; };
template <> struct serialization_traits<double>           { const char *type_id = "f64"; };
template <> struct serialization_traits<bool>             { const char *type_id = "b8";  };
template <> struct serialization_traits<char>             { const char *type_id = "c8";  };

template <typename T> struct serialization_traits<T> :
    serialization_traits<typename std::underlying_type<T>::type,
        typename std::enable_if<std::is_enum<T>::value>::type> { };

template <typename T> struct serialization_helper {
    static std::string type_id() { return serialization_traits<T>().type_id; }

    /** \brief Writes <tt>count</tt> values of type T into stream <tt>s</tt>
     * starting at its current position.
     * Note: <tt>count</tt> is the number of values, <b>not</b> a size in bytes.
     *
     * Support for additional types can be added in any header file by
     * declaring a template specialization for your type.
     */
    static void write(Stream &s, const T *value, size_t count) {
        s.write(value, sizeof(T) * count);
    }

    /** \brief Reads <tt>count</tt> values of type T from stream <tt>s</tt>,
     * starting at its current position.
     * Note: <tt>count</tt> is the number of values, <b>not</b> a size in bytes.
     *
     * Support for additional types can be added in any header file by
     * declaring a template specialization for your type.
     */
    static void read(Stream &s, T *value, size_t count) {
        s.read(value, sizeof(T) * count);
    }
};

template <> struct serialization_helper<std::string> {
    static std::string type_id() { return "Vc8"; }

    static void write(Stream &s, const std::string *value, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t length = (uint32_t) value->length();
            s.write(&length, sizeof(uint32_t));
            s.write((char *) value->data(), sizeof(char) * value->length());
            value++;
        }
    }

    static void read(Stream &s, std::string *value, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t length;
            s.read(&length, sizeof(uint32_t));
            value->resize(length);
            s.read((char *) value->data(), sizeof(char) * length);
            value++;
        }
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
