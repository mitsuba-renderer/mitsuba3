#pragma once

#include <mitsuba/mitsuba.h>
#include <string>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Minimal command line argument parser
 *
 * This class provides a minimal cross-platform command line argument parser in
 * the spirit of to GNU getopt. Both short and long arguments that accept an
 * optional extra value are supported.
 *
 * The typical usage is
 *
 * \code
 * ArgParser p;
 * auto arg0 = p.register("--myParameter");
 * auto arg1 = p.register("-f", true);
 * p.parse(argc, argv);
 * if (*arg0)
 *     std::cout << "Got --myParameter" << std::endl;
 * if (*arg1)
 *     std::cout << "Got -f " << arg1->value() << std::endl;
 * \endcode
 *
 */
class MI_EXPORT_LIB ArgParser {
public:
    struct MI_EXPORT_LIB Arg {
        friend class ArgParser;
    public:
        /// Returns whether the argument has been specified
        operator bool() const { return m_present; }

        /// Specifies whether the argument takes an extra value
        bool extra() const { return m_extra; }

        /// Specifies how many times the argument has been specified
        size_t count() const;

        /**
         * \brief For arguments that are specified multiple times, advance to
         * the next one.
         */
        const Arg *next() const { return m_next; }

        /// Return the extra argument associated with this argument
        const std::string &as_string() const { return m_value; }

        /// Return the extra argument associated with this argument
        int as_int() const;

        /// Return the extra argument associated with this argument
        double as_float() const;

        /// Release all memory
        ~Arg() { delete m_next; }

    protected:
        /**
         * \brief Construct a new argument with the given prefixes
         *
         * \param prefixes
         *     A list of command prefixes (i.e. {"-f", "--fast"})
         *
         * \param extra
         *     Indicates whether the argument accepts an extra argument value
         */
        Arg(const std::vector<std::string> &prefixes, bool extra)
            : m_prefixes(prefixes), m_extra(extra), m_present(false),
              m_next(nullptr) { }

        /// Copy constructor (does not copy argument values)
        Arg(const Arg &a)
            : m_prefixes(a.m_prefixes), m_extra(a.m_extra), m_present(false),
              m_next(nullptr) { }

        /// Append a argument value at the end
        void append(const std::string &value = "");

    private:
        std::vector<std::string> m_prefixes;
        bool m_extra;
        bool m_present;
        Arg *m_next;
        std::string m_value;
    };

public:
    /**
     * \brief Register a new argument with the given prefix
     *
     * \param prefix
     *     A single command prefix (i.e. "-f")
     *
     * \param extra
     *     Indicates whether the argument accepts an extra argument value
     */
    const Arg *add(const std::string &prefix, bool extra = false) {
        return add(std::vector<std::string>({ prefix }), extra);
    }

    /**
     * \brief Register a new argument with the given list of prefixes
     *
     * \param prefixes
     *     A list of command prefixes (i.e. {"-f", "--fast"})
     *
     * \param extra
     *     Indicates whether the argument accepts an extra argument value
     */
    const Arg *add(const std::vector<std::string> &prefixes, bool extra = false) {
        Arg *arg = new Arg(prefixes, extra);
        m_args.push_back(arg);
        return arg;
    }

    /// Parse the given set of command line arguments
    void parse(int argc, const char **argv);

    /// Parse the given set of command line arguments
    void parse(int argc, char **argv) {
        parse(argc, const_cast<const char **>(argv));
    }

    /// Return the name of the invoked application executable
    const std::string &executable_name() { return m_executable_name; }

private:
    std::vector<Arg *> m_args;
    std::string m_executable_name;
};

NAMESPACE_END(mitsuba)
