#pragma once

#include <set>
#include <vector>
#include <Eigen/Core>

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


template <typename T1, typename T2> struct serialization_helper<std::pair<T1, T2>> {
    static std::string type_id() {
        return "P" +
               serialization_helper<T1>::type_id() +
               serialization_helper<T2>::type_id();
    }

    static void write(Stream &s, const std::pair<T1, T1> *value, size_t count) {
        std::unique_ptr<T1> first (new T1[count]);
        std::unique_ptr<T2> second(new T2[count]);

        for (size_t i = 0; i<count; ++i) {
            first.get()[i]  = value[i].first;
            second.get()[i] = value[i].second;
        }

        serialization_helper<T1>::write(s, first.get(), count);
        serialization_helper<T2>::write(s, second.get(), count);
    }

    static void read(Stream &s, std::pair<T1, T1> *value, size_t count) {
        std::unique_ptr<T1> first (new T1[count]);
        std::unique_ptr<T2> second(new T2[count]);

        serialization_helper<T1>::read(s, first.get(), count);
        serialization_helper<T2>::read(s, second.get(), count);

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

    static void write(Stream &s, const std::vector<T> *value, size_t count) {
        for (size_t i = 0; i<count; ++i) {
            uint32_t size = (uint32_t) value->size();
            s.write(&size, sizeof(uint32_t));
            serialization_helper<T>::write(s, value->data(), size);
            value++;
        }
    }

    static void read(Stream &s, std::vector<T> *value, size_t count) {
        for (size_t i = 0; i<count; ++i) {
            uint32_t size = 0;
            s.read(&size, sizeof(uint32_t));
            value->resize(size);
            serialization_helper<T>::read(s, value->data(), size);
            value++;
        }
    }
};

template <typename T> struct serialization_helper<std::set<T>> {
    static std::string type_id() {
        return "S" + serialization_helper<T>::type_id();
    }

    static void write(Stream &s, const std::set<T> *value, size_t count) {
        for (size_t i = 0; i<count; ++i) {
            std::vector<T> temp(value->size());
            uint32_t idx = 0;
            for (auto it = value->begin(); it != value->end(); ++it)
                temp[idx++] = *it;
            serialization_helper<std::vector<T>>::write(s, &temp, 1);
            value++;
        }
    }

    static void read(Stream &s, std::set<T> *value, size_t count) {
        for (size_t i = 0; i<count; ++i) {
            std::vector<T> temp;
            serialization_helper<std::vector<T>>::read(s, &temp, 1);
            value->clear();
            for (auto k: temp)
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

static void write(Stream &s, const Matrix *value, size_t count) {
    for (size_t i = 0; i<count; ++i) {
        uint32_t rows = value->rows(), cols = value->cols();
        s.write(&rows, sizeof(uint32_t));
        s.write(&cols, sizeof(uint32_t));
        serialization_helper<Scalar>::write(s, value->data(), rows*cols);
        value++;
    }
}

static void read(Stream &s, Matrix *value, size_t count) {
    for (size_t i = 0; i<count; ++i) {
        uint32_t rows = 0, cols = 0;
        s.read(&rows, sizeof(uint32_t));
        s.read(&cols, sizeof(uint32_t));
        value->resize(rows, cols);
        serialization_helper<Scalar>::read(s, value->data(), rows*cols);
        value++;
    }
}
};

NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
