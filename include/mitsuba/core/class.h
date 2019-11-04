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
     * \param register
     *     Whether this class should be registered with the XML parser.
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
          bool register_ = false,
          ConstructFunctor constr = nullptr,
          UnserializeFunctor unser = nullptr,
          const std::string &alias = "");

    /// Return the name of the represented class
    const std::string &name() const { return m_name; }

    /// Return the scene description-specific alias
    const std::string &alias() const { return m_alias; }

#if 0  // TODO: remove this?
    /**
     * \brief Return whether or not the class represented
     * by this Class object contains pure virtual methods
     */
    bool is_abstract() const { return m_abstract; }
#endif

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
    // bool m_abstract;
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
#define MTS_REGISTER_CLASS(Name, Parent, ...)                                                      \
    inline static const Class *m_class = new Class(                                                \
        #Name, #Parent, false,                                                                     \
        ::mitsuba::detail::get_construct_functor<Name>(),                                          \
        ::mitsuba::detail::get_unserialize_functor<Name>(),                                        \
         ## __VA_ARGS__);                                                                          \
    const Class *class_() const override { return m_class; }

/// Same as MTS_REGISTER_CLASS, but additionally registers the class with the XML parser
#define MTS_REGISTER_INTERFACE(Name, Parent, ...)                                                  \
    inline static const Class *m_class = new Class(                                                \
        #Name, #Parent, true,                                                                      \
        ::mitsuba::detail::get_construct_functor<Name>(),                                          \
        ::mitsuba::detail::get_unserialize_functor<Name>(),                                        \
         ## __VA_ARGS__);                                                                          \
    const Class *class_() const override { return m_class; }


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
 * \brief Declares an alias Base for the parent template class and
 * imports the desired methods and fields with `using` declarations.
 * This is useful when inheriting from template parents, since methods
 * and fields must be explicitly made visible.
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
    ENOKI_MAP_IMPORT(Base, ##__VA_ARGS__)

/**
 * Variant of MTS_USING_BASE for parent classes that are only templated over Float.
 */
#define MTS_USING_BASE_FLOAT(class_, ...) \
    using Base = class_<Float>; \
    ENOKI_MAP_IMPORT(Base, ##__VA_ARGS__)

extern MTS_EXPORT_CORE const Class *m_class;

NAMESPACE_END(mitsuba)
