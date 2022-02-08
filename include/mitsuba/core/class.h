#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <functional>

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
class MI_EXPORT_LIB Class {
public:
    using ConstructFunctor   = std::function<Object *(const Properties &props)>;
    using UnserializeFunctor = std::function<Object *(Stream *stream)>;

    /**
     * \brief Construct a new class descriptor
     *
     * This method should never be called manually. Instead, use
     * the \ref MI_DECLARE_CLASS macro to automatically do this for you.
     *
     * \param name
     *     Name of the class
     *
     * \param parent
     *     Name of the parent class
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
          const std::string &variant = "",
          ConstructFunctor construct = {},
          UnserializeFunctor unserialize = {},
          const std::string &alias = "");

    /// Return the name of the class
    const std::string &name() const { return m_name; }

    /// Return the variant of the class
    const std::string &variant() const { return m_variant; }

    /// Return the scene description-specific alias, if applicable
    const std::string &alias() const { return m_alias; }

    /// Does the class support instantiation over RTTI?
    bool is_constructible() const { return (bool) m_construct; }

    /// Does the class support serialization?
    bool is_serializable() const { return (bool) m_unserialize; }

    /** \brief Return the Class object associated with the parent
     * class of nullptr if it does not have one.
     */
    const Class *parent() const { return m_parent; }

    /// Check whether this class derives from \a class_
    bool derives_from(const Class *class_) const;

    /// Look up a class by its name
    static const Class *for_name(const std::string &name,
                                 const std::string &variant = "");

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
    std::string m_name, m_parent_name, m_variant, m_alias;
    Class *m_parent;
    ConstructFunctor m_construct;
    UnserializeFunctor m_unserialize;
    static bool m_is_initialized;
};

/**
 * \brief Return the \ref Class object corresponding to a named class.
 *
 * Call the Macro without quotes, e.g. \c MI_CLASS(SerializableObject)
 */
#define MI_CLASS(x) x::m_class

extern MI_EXPORT_LIB const Class* m_class;

/**
 * \brief This macro should be invoked in the class declaration of classes
 * that directly or indirectly derive from \ref Object.
 */
#define MI_DECLARE_CLASS()                         \
    virtual const Class *class_() const override;   \
public:                                             \
    static Class *m_class;

/**
 * \brief This macro should be invoked in the main implementation \c .cpp file of
 * any (non-templated) class that derives from \ref Object.
 *
 * This is needed for the basic RTTI support provided by Mitsuba objects (see
 * \ref Class for details). A basic class definition might look as follows:
 *
 * \code
 * class MyObject : public Object {
 * public:
 *
 *     MyObject();
 *
 *     /// Declare RTTI data structures
 *     MI_DECLARE_CLASS()
 * protected:
 *     /// Important: declare a protected virtual destructor
 *     virtual ~MyObject();
 *
 * };
 *
 * /// in .cpp file
 * /// Implement RTTI data structures
 * MI_IMPLEMENT_CLASS(MyObject, Object)
 * \endcode
 *
 * The virtual protected destructure ensures that instances can only be
 * deallocated using the reference counting wrapper \ref ref.
 *
 * \sa MI_IMPLEMENT_CLASS
 */

#define MI_IMPLEMENT_CLASS(Name, Parent, ...)                                                     \
    Class *Name::m_class = new Class(                                                              \
        #Name, #Parent, "",                                                                        \
        ::mitsuba::detail::get_construct_functor<Name>(),                                          \
        ::mitsuba::detail::get_unserialize_functor<Name>(),                                        \
         ## __VA_ARGS__);                                                                          \
    const Class *Name::class_() const { return m_class; }

/*
 * \brief This macro should be invoked in the main implementation \c .cpp file of
 * any (templated) class that derives from \ref Object.
 *
 * This is needed for the basic RTTI support provided by Mitsuba objects (see
 * \ref Class for details).
 *
 * The virtual protected destructure ensures that instances can only be
 * deallocated using the reference counting wrapper \ref ref.
 *
 * \sa MI_IMPLEMENT_CLASS_VARIANT
 */
#define MI_IMPLEMENT_CLASS_VARIANT(Name, Parent, ...)                                             \
    MI_VARIANT Class *Name<Float, Spectrum>::m_class = new Class(                                 \
        #Name, #Parent,                                                                            \
        ::mitsuba::detail::get_variant<Float, Spectrum>(),                                         \
        ::mitsuba::detail::get_construct_functor<Name<Float, Spectrum>>(),                         \
        ::mitsuba::detail::get_unserialize_functor<Name<Float, Spectrum>>(),                       \
         ## __VA_ARGS__);                                                                          \
    MI_VARIANT const Class *Name<Float, Spectrum>::class_() const { return m_class; }


/// Instantiate and export a Mitsuba plugin
#define MI_EXPORT_PLUGIN(Name, Descr)                                                             \
    extern "C" {                                                                                   \
        MI_EXPORT const char *plugin_name() { return #Name; }                                     \
        MI_EXPORT const char *plugin_descr() { return Descr; }                                    \
    }                                                                                              \
    MI_INSTANTIATE_CLASS(Name)

NAMESPACE_BEGIN(detail)
template <typename, typename Arg, typename = void>
struct is_constructible : std::false_type { };

template <typename T, typename Arg>
struct is_constructible<T, Arg, std::void_t<decltype(new T(std::declval<Arg>()))> > : std::true_type { };

template <typename T, typename Arg>
constexpr bool is_constructible_v = is_constructible<T, Arg>::value;

template <typename T, std::enable_if_t<is_constructible_v<T, const Properties&>, int> = 0>
Class::ConstructFunctor get_construct_functor() { return [](const Properties& p) -> Object* { return new T(p); }; }
template <typename T, std::enable_if_t<!is_constructible_v<T, const Properties&>, int> = 0>
Class::ConstructFunctor get_construct_functor() { return {}; }
template <typename T, std::enable_if_t<is_constructible_v<T, Stream*>, int> = 0>
Class::UnserializeFunctor get_unserialize_functor() { return [](Stream* s) -> Object* { return new T(s); }; }
template <typename T, std::enable_if_t<!is_constructible_v<T, Stream*>, int> = 0>
Class::UnserializeFunctor get_unserialize_functor() { return {}; }
NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
