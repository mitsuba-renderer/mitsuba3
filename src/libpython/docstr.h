/*
  This file contains docstrings for the Python bindings.
  Do not edit! These were automatically extracted by mkdoc.py
 */

#define __EXPAND(x)                              x
#define __COUNT(_1, _2, _3, _4, _5, COUNT, ...)  COUNT
#define __VA_SIZE(...)                           __EXPAND(__COUNT(__VA_ARGS__, 5, 4, 3, 2, 1))
#define __CAT1(a, b)                             a ## b
#define __CAT2(a, b)                             __CAT1(a, b)
#define __DOC1(n1)                               __doc_##n1
#define __DOC2(n1, n2)                           __doc_##n1##_##n2
#define __DOC3(n1, n2, n3)                       __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4)                   __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5)               __doc_##n1##_##n2##_##n3##_##n4_##n5
#define DOC(...)                                 __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *__doc_mitsuba_Object = R"doc(Reference counted object base class)doc";

static const char *__doc_mitsuba_Object_Object = R"doc(Default constructor)doc";

static const char *__doc_mitsuba_Object_Object_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Object_decRef =
R"doc(Decrease the reference count of the object and possibly deallocate it.

The object will automatically be deallocated once the reference count
reaches zero.)doc";

static const char *__doc_mitsuba_Object_getRefCount = R"doc(Return the current reference count)doc";

static const char *__doc_mitsuba_Object_incRef = R"doc(Increase the object's reference count by one)doc";

static const char *__doc_mitsuba_Object_m_refCount = R"doc()doc";

static const char *__doc_mitsuba_Object_toString =
R"doc(Return a human-readable string representation of the object's contents.

This function is mainly useful for debugging purposes and should ideally be
implemented by all subclasses. The default implementation simply returns
``MyObject[unknown]``, where ``MyObject`` is the name of the subclass.)doc";

static const char *__doc_mitsuba_Properties = R"doc(Reference counted object base class)doc";

static const char *__doc_mitsuba_Properties_Properties = R"doc(Default constructor)doc";

static const char *__doc_mitsuba_Properties_Properties_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_detail_variant_helper = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_copy = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_destruct = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_helper_move = R"doc()doc";

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

static const char *__doc_mitsuba_ref_operator_assign = R"doc(Move another reference into the current one)doc";

static const char *__doc_mitsuba_ref_operator_assign_2 = R"doc(Overwrite this reference with another reference)doc";

static const char *__doc_mitsuba_ref_operator_assign_3 = R"doc(Overwrite this reference with a pointer to another object)doc";

static const char *__doc_mitsuba_ref_operator_eq = R"doc(Compare this reference with another reference)doc";

static const char *__doc_mitsuba_ref_operator_eq_2 = R"doc(Compare this reference with a pointer)doc";

static const char *__doc_mitsuba_ref_operator_mul = R"doc(Return a C++ reference to the referenced object)doc";

static const char *__doc_mitsuba_ref_operator_mul_2 = R"doc(Return a const C++ reference to the referenced object)doc";

static const char *__doc_mitsuba_ref_operator_ne = R"doc(Compare this reference with another reference)doc";

static const char *__doc_mitsuba_ref_operator_ne_2 = R"doc(Compare this reference with a pointer)doc";

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

