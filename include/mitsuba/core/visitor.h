#pragma once

#include <mitsuba/core/platform.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Visitor interface as defined in the boost::variant library.
 * See http://www.boost.org/doc/libs/1_60_0/doc/html/boost/static_visitor.html
 */
template <typename ResultType>
class MTS_EXPORT_CORE static_visitor {
public:
    typedef ResultType result_type;

    // TODO: needs to support exactly the right operator() calls
    virtual result_type operator()(const bool &visited) const = 0;
    virtual result_type operator()(const int64_t &visited) const = 0;
    virtual result_type operator()(const Float &visited) const = 0;
    virtual result_type operator()(const std::string &visited) const = 0;
};

NAMESPACE_END(mitsuba)
