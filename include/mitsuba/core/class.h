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
     */
    Class(const std::string &name,
          const std::string &parent,
          bool abstract = false,
          ConstructFunctor constr = nullptr,
          UnserializeFunctor unser = nullptr);

    /**
     * \brief Construct a new class descriptor
     *
     * This constructor additionally takes an alias name that is used to refer
     * to instances of this type in Mitsuba's scene specification language.
     *
     * This method should never be called manually. Instead, use the \ref
     * MTS_IMPLEMENT_CLASS_ALIAS macro to automatically do this for you.
     *
     * \param name
     *     Name of the class
     *
     * \param alias
     *     Name used to refer to instances of this type in Mitsuba's
     *     scene description language (usually the same as \c name)
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
     */
    Class(const std::string &name,
          const std::string &alias,
          const std::string &parent,
          bool abstract = false,
          ConstructFunctor constr = nullptr,
          UnserializeFunctor unser = nullptr);

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
#define MTS_DECLARE_CLASS() \
    virtual const Class *class_() const override; \
public: \
    static Class *m_class;


NAMESPACE_BEGIN(detail)
template <typename T, typename std::enable_if<is_constructible<T, const Properties &>::value, int>::type = 0>
Class::ConstructFunctor get_construct_functor() { return [](const Properties &p) -> Object * { return new T(p); }; }
template <typename T, typename std::enable_if<!is_constructible<T, const Properties &>::value, int>::type = 0>
Class::ConstructFunctor get_construct_functor() { return nullptr; }
template <typename T, typename std::enable_if<is_constructible<T, Stream *>::value, int>::type = 0>
Class::UnserializeFunctor get_unserialize_functor() { return [](Stream *s) -> Object * { return new T(s); }; }
template <typename T, typename std::enable_if<!is_constructible<T, Stream *>::value, int>::type = 0>
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
 * \param Name Name of the class
 * \param Parent Name of the parent class
 */
#define MTS_IMPLEMENT_CLASS(Name, Parent) \
    Class *Name::m_class = new Class(#Name, #Parent, \
            std::is_abstract<Name>::value, \
            ::mitsuba::detail::get_construct_functor<Name>(), \
            ::mitsuba::detail::get_unserialize_functor<Name>()); \
    const Class *Name::class_() const { return m_class; }

#define MTS_IMPLEMENT_CLASS_ALIAS(Name, Alias, Parent) \
    Class *Name::m_class = new Class(#Name, Alias, #Parent, \
            std::is_abstract<Name>::value, \
            ::mitsuba::detail::get_construct_functor<Name>(), \
            ::mitsuba::detail::get_unserialize_functor<Name>()); \
    const Class *Name::class_() const { return m_class; }

extern MTS_EXPORT_CORE const Class *m_class;

#define MTS_EXPORT_PLUGIN(name, descr) \
    extern "C" { \
        MTS_EXPORT Object *CreateObject(const Properties &props) { \
            return new name(props); \
        } \
        MTS_EXPORT const char *Description = descr; \
    }

NAMESPACE_END(mitsuba)
