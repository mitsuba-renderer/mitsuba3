#pragma once

#include <mitsuba/core/object.h>
#include <functional>
#include <tuple>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

inline size_t hash_combine(size_t hash1, size_t hash2) {
    return hash2 ^ (hash1 + 0x9e3779b9 + (hash2 << 6) + (hash2 >> 2));
}

template <typename T, std::enable_if_t<!std::is_enum_v<T>, int> = 0> size_t hash(const T &t) {
    return std::hash<T>()(t);
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0> size_t hash(const T &t) {
    /* Enum hash workaround for GCC / C++11 */
    return hash(typename std::underlying_type<T>::type(t));
}


template <typename Tuple, size_t Index = std::tuple_size_v<Tuple> - 1>
struct tuple_hasher {
    size_t operator()(const Tuple &t) const {
        return hash_combine(hash(std::get<Index>(t)),
                            tuple_hasher<Tuple, Index - 1>()(t));
    }
};

template <typename... Args>
size_t hash(const std::tuple<Args...> &t) {
    return tuple_hasher<std::tuple<Args...>>()(t);
}

template <typename T1, typename T2> size_t hash(const std::pair<T1, T2> &t) {
    return hash_combine(hash(t.first), hash(t.second));
}

template <typename T> size_t hash(const ref<T> &t) {
    return hash(*t.get());
}

template <typename T, typename Allocator>
size_t hash(const std::vector<T, Allocator> &v) {
    size_t value = 0;
    for (auto const &item : v)
        value = hash_combine(value, hash(item));
    return value;
}

template <typename T> struct hasher {
    size_t operator()(const T &t) const {
        return hash(t);
    }
};

template <typename T> struct comparator {
    size_t operator()(const T &t1, const T &t2) const {
        return t1 == t2;
    }
};

template <typename T1, typename T2> struct comparator<std::pair<T1, T2>> {
    size_t operator()(const std::pair<T1, T2> &t1, const std::pair<T1, T2> &t2) const {
        return comparator<T1>()(t1.first, t2.first) &&
               comparator<T2>()(t1.second, t2.second);
    }
};

template <typename T> struct comparator<ref<T>> {
    size_t operator()(const ref<T> &t1, const ref<T> &t2) const {
        return *t1 == *t2;
    }
};

NAMESPACE_END(end)
