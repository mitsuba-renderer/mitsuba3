#include <nanobind/nanobind.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/logger.h>
#include "any.h"

namespace mitsuba {

/// Storage helper for arbitrary Python objects in Properties
class PythonObjectStorage : public Any::Base {
    nb::handle m_obj;
    const std::type_info *m_cpp_type_info;
    void *m_cpp_ptr;

public:
    PythonObjectStorage(nb::handle obj) {
        nb::gil_scoped_acquire guard;
        m_obj = obj;
        m_cpp_type_info = &nb::type_info(obj.type());
        m_cpp_ptr = nb::inst_ptr<void>(obj);
        m_obj.inc_ref();
    }

    ~PythonObjectStorage() {
        nb::gil_scoped_acquire gil;
        m_obj.dec_ref();
    }

    const std::type_info &type() const override {
        return *m_cpp_type_info;
    }

    void *ptr() override {
        return m_cpp_ptr;
    }
};

/// Create an Any object that stores a Python object
Any any_wrap(nb::handle obj) {
    if (!nb::inst_check(obj))
        throw std::bad_cast();

    Any::Base *storage = new PythonObjectStorage(obj);
    return Any(storage);
}

} // namespace mitsuba
