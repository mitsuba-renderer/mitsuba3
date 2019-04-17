#include <mitsuba/render/autodiff.h>
#include <mitsuba/core/logger.h>
#include <map>

NAMESPACE_BEGIN(mitsuba)

void DifferentiableObject::parameters_changed() { }

#if defined(MTS_ENABLE_AUTODIFF)
struct DifferentiableParameters::Details {
    std::string prefix;
    std::map<std::string, std::tuple<DifferentiableObject *, void *, size_t>> entries;

    Details() {}
    Details(const Details &other)
        : prefix(other.prefix), entries(other.entries) {}
    Details &operator=(const Details &other) = delete;
};


void DifferentiableObject::put_parameters(DifferentiableParameters &) { }

DifferentiableParameters::DifferentiableParameters() : d(new Details()) { }

DifferentiableParameters::DifferentiableParameters(const DifferentiableParameters &other)
    : d(new Details(*other.d)) {}

DifferentiableParameters::~DifferentiableParameters() { }

void DifferentiableParameters::set_prefix(const std::string &prefix) {
    d->prefix = prefix;
}

void DifferentiableParameters::put(DifferentiableObject *obj, const std::string &name, void *ptr, size_t dim) {
    if (d->entries.find(name) != d->entries.end())
        Throw("DifferentiableParameters::put(): parameters \"%s\" is already "
              "registered!", name);
    d->entries.emplace(d->prefix + name, std::make_tuple(obj, ptr, dim));
}

std::string DifferentiableParameters::to_string() const {
    std::ostringstream oss;
    oss << "DifferentiableParameters[" << std::endl;
    size_t i = 0;
    for (auto &kv: d->entries) {
        oss << "  " << kv.first;
        ++i;
        if (i != d->entries.size())
            oss << ",";
        oss << std::endl;
    }
    oss << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(DifferentiableParameters, Object)
#endif

MTS_IMPLEMENT_CLASS(DifferentiableObject, Object)
NAMESPACE_END(mitsuba)
