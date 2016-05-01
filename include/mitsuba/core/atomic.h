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
 * integer compare and exchange operations to perform changes atomically.
 */
template <typename Type = Float> class AtomicFloat {
private:
    typedef typename std::conditional<sizeof(Type) == 4,
                                      uint32_t, uint64_t>::type Storage;

public:
    /// Initialize the AtomicFloat with a given floating point value
    explicit AtomicFloat(Type v = 0) { m_bits = memcpy_cast<Storage>(v); }

    /// Convert the AtomicFloat into a normal floating point value
    operator Type() const { return memcpy_cast<Type>(m_bits); }

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

protected:
    /// Apply a FP operation atomically (verified that this will be nicely inlined in the above iterators)
    template <typename Func> AtomicFloat& do_atomic(Func func) {
        Storage oldBits = m_bits, newBits;
        do {
            newBits = memcpy_cast<Storage>(func(memcpy_cast<Type>(oldBits)));
        } while (!m_bits.compare_exchange_weak(oldBits, newBits));
        return *this;
    }

protected:
    std::atomic<Storage> m_bits;
};

NAMESPACE_END(mitsuba)
