#pragma once

#include <mitsuba/mitsuba.h>
#include <string>

#include <enoki-jit/jit.h>
#include <enoki/array_router.h>

#if defined(MTS_ENABLE_CUDA)
#  include <enoki/cuda.h>
#endif

#if defined(MTS_ENABLE_LLVM)
#  include <enoki/llvm.h>
#endif

#if defined(MTS_ENABLE_AUTODIFF)
#  include <enoki/autodiff.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/// Convenience function which computes an array size/type suffix (like '2u' or '3fP')
template <typename T> std::string type_suffix() {
    using S = ek::scalar_t<T>;
    using V = ek::value_t<T>;

    std::string id = std::to_string(ek::array_size_v<T>);

    if constexpr (ek::is_floating_point_v<S>) {
        if (std::is_same_v<S, ek::half>)
            id += 'h';
        else if (std::is_same_v<S, float>)
            id += 'f';
        else
            id += 'd';
    } else {
        if (ek::is_signed_v<S>)
            id += 'i';
        else
            id += 'u';
    }

    if (ek::is_diff_array_v<V>)
        id += 'D';

    if (ek::is_packed_array_v<V>)
        id += 'P';
    else if (ek::is_llvm_array_v<V>)
        id += 'L';
    else if (ek::is_cuda_array_v<V>)
        id += 'C';
    else if (ek::is_dynamic_array_v<V>)
        id += 'X';

    return id;
}

/// Round an integer to a multiple of the current packet size
template <typename Float>
inline size_t round_to_packet_size(size_t size) {
    constexpr size_t PacketSize = Float::Size;
    return (size + PacketSize - 1) / PacketSize * PacketSize;
}

/// RAII-style class to temporarily migrate variables on the host
template <bool Condition, typename ...Args>
struct scoped_migrate_to_host_cond {
    static constexpr bool IsCUDA = (ek::is_cuda_array_v<Args> || ...);
    static constexpr bool IsLLVM = (ek::is_llvm_array_v<Args> || ...);
    static constexpr bool IsJIT = IsCUDA || IsLLVM;
    static_assert (!(IsCUDA && IsLLVM));

    scoped_migrate_to_host_cond(Args&... args) : m_args(args...) {
        if constexpr (IsJIT && Condition) {
            ek::eval(args...);

            if constexpr (IsCUDA)
                migrate_tuple(AllocType::Host);

            ek::sync_thread();
        }
    }

    ~scoped_migrate_to_host_cond() {
        if constexpr (IsCUDA && Condition)
            migrate_tuple(AllocType::Device);
    }

private:
    void migrate_tuple(AllocType dest) {
        std::apply([dest](auto&&... args) {
            (ek::migrate(args, dest), ...);
            }, m_args);
    }

private:
    std::tuple<Args&...> m_args;
};

template <typename... Args>
struct scoped_migrate_to_host : public scoped_migrate_to_host_cond<true, Args...> {
    scoped_migrate_to_host(Args &... args)
        : scoped_migrate_to_host_cond<true, Args...>(args...){};
};

NAMESPACE_END(mitsuba)
