#include <mitsuba/core/fresolver.h>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)

FileResolver::FileResolver() {
    m_paths.push_back(fs::current_path());
}

fs::path FileResolver::resolve(const fs::path &path) const {
    for (auto const &base : m_paths) {
        fs::path combined = base / path;
        if (fs::exists(combined))
            return combined;
    }
    return path;
}

std::ostream &operator<<(std::ostream &os, const FileResolver &r) {
    os << "resolver[" << std::endl;
    for (size_t i = 0; i < r.m_paths.size(); ++i) {
        os << "  \"" << r.m_paths[i] << "\"";
        if (i + 1 < r.m_paths.size())
            os << ",";
        os << std::endl;
    }
    os << "]";
    return os;
}

MTS_IMPLEMENT_CLASS(FileResolver, Object)
NAMESPACE_END(mitsuba)
