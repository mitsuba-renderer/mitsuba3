#pragma once

#include <mitsuba/mitsuba.h>
#include <type_traits>
#include <atomic>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Atomic floating point data type
 *
 * The class implements an an atomic floating point data type (which is not
 * possible with the existing overloads provided by <tt>std::atomic</tt>). It
 * internally casts floating point values to an integer storage format and uses
 * atomic integer compare and exchange operations to perform changes.
 */
template <typename Type = float> class AtomicFloat {
private:
    using Storage = std::conditional_t<sizeof(Type) == 4, uint32_t, uint64_t>;

public:
    /// Initialize the AtomicFloat with a given floating point value
    explicit AtomicFloat(Type v = 0.f) { m_bits = memcpy_cast<Storage>(v); }

    /// Convert the AtomicFloat into a normal floating point value
    operator Type() const { return memcpy_cast<Type>(m_bits.load(std::memory_order_relaxed)); }

    /// Overwrite the AtomicFloat with a floating point value
    AtomicFloat &operator=(Type v) { m_bits = memcpy_cast<Storage>(v); return *this; }

    /// Atomically add a floating point value
    AtomicFloat &operator+=(Type arg) { return do_atomic([arg](Type value) { return value + arg; }); }

    /// Atomically subtract a floating point value
    AtomicFloat &operator-=(Type arg) { return do_atomic([arg](Type value) { return value - arg; }); }

    /// Atomically multiply by a floating point value
    AtomicFloat &operator*=(Type arg) { return do_atomic([arg](Type value) { return value * arg; }); }

    /// Atomically divide by a floating point value
    AtomicFloat &operator/=(Type arg) { return do_atomic([arg](Type value) { return value / arg; }); }

    /// Atomically compute the minimum
    AtomicFloat &min(Type arg) { return do_atomic([arg](Type value) { return std::min(value, arg); }); }

    /// Atomically compute the maximum
    AtomicFloat &max(Type arg) { return do_atomic([arg](Type value) { return std::max(value, arg); }); }


protected:
    /// Apply a FP operation atomically (verified that this will be nicely inlined in the above operators)
    template <typename Func> AtomicFloat& do_atomic(Func func) {
        Storage old_bits = m_bits.load(std::memory_order::memory_order_relaxed), new_bits;
        do {
            new_bits = memcpy_cast<Storage>(func(memcpy_cast<Type>(old_bits)));
            if (new_bits == old_bits)
                break;
        } while (!m_bits.compare_exchange_weak(old_bits, new_bits));
        return *this;
    }

protected:
    std::atomic<Storage> m_bits;
};

NAMESPACE_END(mitsuba)
