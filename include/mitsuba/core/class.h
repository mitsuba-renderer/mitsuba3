#pragma once

#include <mitsuba/mitsuba.h>
#include <string>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Stores meta-information about \ref Object instances.
 *
 * This class provides a thin layer of RTTI (run-time type information),
 * which is useful for doing things like:
 *
 * <ul>
 *    <li>Checking if an object derives from a certain class</li>
 *    <li>Determining the parent of a class at runtime</li>
 *    <li>Instantiating a class by name</li>
 *    <li>Unserializing a class from a binary data stream</li>
 * </ul>
 *
 * \sa ref, Object
 */
class MTS_EXPORT_CORE Class {
public:
    using ConstructFunctor   = Object *(*)(const Properties &props);
    using UnserializeFunctor = Object *(*)(Stream *stream);

    /**
     * \brief Construct a new class descriptor
     *
     * This method should never be called manually. Instead, use
     * the \ref MTS_IMPLEMENT_CLASS macro to automatically do this for you.
     *
     * \param name
     *     Name of the class
     *
     * \param parent
     *     Name of the parent class
     *
     * \param abstract
     *     \c true if the class contains pure virtual methods
     *
     * \param constr
     *     Pointer to a default construction function
     *
     * \param unser
     *     Pointer to a unserialization construction function
     *
     * \param alias
     *     Optional: name used to refer to instances of this type in Mitsuba's
     *     scene description language
     */
    Class(const std::string &name,
          const std::string &parent,
          bool abstract = false,
          ConstructFunctor constr = nullptr,
          UnserializeFunctor unser = nullptr,
          const std::string &alias = "");

    /// Return the name of the represented class
    const std::string &name() const { return m_name; }

    /// Return the scene description-specific alias
    const std::string &alias() const { return m_alias; }

    /**
     * \brief Return whether or not the class represented
     * by this Class object contains pure virtual methods
     */
    bool is_abstract() const { return m_abstract; }

    /// Does the class support instantiation over RTTI?
    bool is_constructible() const { return m_constr != nullptr; }

    /// Does the class support serialization?
    bool is_serializable() const { return m_unser != nullptr; }

    /** \brief Return the Class object associated with the parent
     * class of nullptr if it does not have one.
     */
    const Class *parent() const { return m_parent; }

    /// Check whether this class derives from \a class_
    bool derives_from(const Class *class_) const;

    /// Look up a class by its name
    static const Class *for_name(const std::string &name);

    /**
     * \brief Generate an instance of this class
     *
     * \param props
     *     A list of extra parameters that are supplied to the constructor
     *
     * \remark Throws an exception if the class is not constructible
     */
    ref<Object> construct(const Properties &props) const;

    /**
     * \brief Unserialize an instance of the class
     * \remark Throws an exception if the class is not unserializable
     */
    ref<Object> unserialize(Stream *stream) const;

    /// Check if the RTTI layer has been initialized
    static bool rtti_is_initialized() { return m_is_initialized; }

    /** \brief Initializes the built-in RTTI and creates
     * a list of all compiled classes
     */
    static void static_initialization();

    /// Free the memory taken by static_initialization()
    static void static_shutdown();
private:
    /** \brief Initialize a class - called by
     * static_initialization()
     */
    static void initialize_once(Class *class_);
private:
    std::string m_name, m_alias, m_parent_name;
    Class *m_parent;
    bool m_abstract;
    ConstructFunctor m_constr;
    UnserializeFunctor m_unser;
    static bool m_is_initialized;
};

/**
 * \brief Return the \ref Class object corresponding to a named class.
 *
 * Call the Macro without quotes, e.g. \c MTS_CLASS(SerializableObject)
 */
#define MTS_CLASS(x) x::m_class

/**
 * \brief This macro must be used in the initial definition in
 * classes that derive from \ref Object.
 *
 * This is needed for the basic RTTI support provided by Mitsuba objects.
 * For instance, a class definition might look like the following:
 *
 * \code
 * class MyObject : public Object {
 * public:
 *     MyObject();
 *
 *     /// Important: declare RTTI data structures
 *     MTS_DECLARE_CLASS()
 * protected:
 *     /// Important: needs to declare a protected virtual destructor
 *     virtual ~MyObject();
 *
 * };
 * \endcode
 */
#define MTS_DECLARE_CLASS()                                                                        \
    virtual const Class *class_() const override;                                                  \
public:                                                                                            \
    static Class *m_class;

NAMESPACE_BEGIN(detail)
/// Replacement for std::is_constructible which also works when the destructor is not accessible
template <typename T, typename... Args> struct is_constructible {
private:
    static std::false_type test(...);
    template <typename T2 = T>
    static std::true_type test(decltype(new T2(std::declval<Args>()...)) value);
public:
    static constexpr bool value = decltype(test(std::declval<T *>()))::value;
};

template <typename T, typename... Args>
constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template <typename T, typename std::enable_if_t<is_constructible_v<T, const Properties &>, int> = 0>
Class::ConstructFunctor get_construct_functor() { return [](const Properties &p) -> Object * { return new T(p); }; }
template <typename T, typename std::enable_if_t<!is_constructible_v<T, const Properties &>, int> = 0>
Class::ConstructFunctor get_construct_functor() { return nullptr; }
template <typename T, typename std::enable_if_t<is_constructible_v<T, Stream *>, int> = 0>
Class::UnserializeFunctor get_unserialize_functor() { return [](Stream *s) -> Object * { return new T(s); }; }
template <typename T, typename std::enable_if_t<!is_constructible_v<T, Stream *>, int> = 0>
Class::UnserializeFunctor get_unserialize_functor() { return nullptr; }
NAMESPACE_END(detail)

/**
 * \brief Creates basic RTTI support for a class
 *
 * This macro or one of its variants should be invoked in the main
 * implementation \c .cpp file of any class that derives from \ref Object.
 * This is needed for the basic RTTI support provided by Mitsuba objects.
 * For instance, the corresponding piece for the example shown in the
 * documentation of \ref MTS_DECLARE_CLASS might look like this:
 *
 * \code
 * MTS_IMPLEMENT_CLASS(MyObject, Object)
 * \endcode
 *
 * \param Name
 *     Name of the class
 *
 * \param Parent
 *     Name of the parent class
 */
#define MTS_IMPLEMENT_CLASS(Name, Parent, ...)                                                     \
    Class *Name::m_class = new Class(#Name, #Parent, std::is_abstract_v<Name>,                     \
                                     ::mitsuba::detail::get_construct_functor<Name>(),             \
                                     ::mitsuba::detail::get_unserialize_functor<Name>(),           \
                                      ## __VA_ARGS__);                                             \
    const Class *Name::class_() const { return m_class; }

/**
 * \brief Creates basic RTTI support for a template class parameterized
 * by a floating point and spectrum
 *
 * Many Mitsuba classes are actually templates that are parameterized by a
 * floating point type and a spectral type that is used to represent color
 * information. This macro should be used instead of \ref MTS_IMPLEMENT_CLASS
 * when \c Name refers to such a template class.
 *
 * \param Name
 *     Name of the class
 *
 * \param Parent
 *     Name of the parent class
 */
#define MTS_IMPLEMENT_CLASS_TEMPLATE(Name, Parent, ...)                                            \
    template <typename Value, typename Spectrum>                                                   \
    Class *Name<Value, Spectrum>::m_class =                                                        \
        new Class(#Name, #Parent, std::is_abstract_v<Name>,                                        \
                  ::mitsuba::detail::get_construct_functor<Name>(),                                \
                  ::mitsuba::detail::get_unserialize_functor<Name>(), ##__VA_ARGS__);              \
    template <typename Value, typename Spectrum>                                                   \
    const Class *Name<Value, Spectrum>::class_() const {                                           \
        return m_class;                                                                            \
    }



#define MTS_MAP_USING_FWD_0(x, peek, ...) \
    using Base::x; ENOKI_MAP_STMT_NEXT(peek, MTS_MAP_USING_FWD_1)(peek, __VA_ARGS__)
#define MTS_MAP_USING_FWD_1(x, peek, ...) \
    using Base::x; ENOKI_MAP_STMT_NEXT(peek, MTS_MAP_USING_FWD_0)(peek, __VA_ARGS__)

/**
 * \brief Declares an alias Base for the parent template class and
 * imports the desired methods and fields with `using` declarations.
 * This is useful when inheriting from template parents, since methods
 * and fields must be explicitly be made visible.
 * Note that the parent class must be templated over <Float, Spectrum>.
 *
 * For example,
 *     MTS_USING_BASE(BSDF, m_flags, m_components)
 * expands to:
 *     using Base = BSDF<Float, Spectrum>;
 *     using Base::m_flags;
 *     using Base::m_components;
 */
#define MTS_USING_BASE(class_, ...) \
    using Base = class_<Float, Spectrum>; \
    ENOKI_EVAL(MTS_MAP_USING_FWD_0(__VA_ARGS__, (), 0))

/**
 * Variant of MTS_USING_BASE for parents that are only templated over Float.
 */
#define MTS_USING_BASE_FLOAT(class_, ...) \
    using Base = class_<Float>; \
    ENOKI_EVAL(MTS_MAP_USING_FWD_0(__VA_ARGS__, (), 0))

extern MTS_EXPORT_CORE const Class *m_class;

NAMESPACE_END(mitsuba)
