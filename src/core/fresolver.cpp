#include <mitsuba/core/fresolver.h>
#include <sstream>
#include <algorithm>

NAMESPACE_BEGIN(mitsuba)

FileResolver::FileResolver() : Object() {
    m_paths.push_back(fs::current_path());
}

FileResolver::FileResolver(const FileResolver &fr)
  : Object(), m_paths(fr.m_paths) { }

void FileResolver::erase(const fs::path &p) {
    m_paths.erase(std::remove(m_paths.begin(), m_paths.end(), p), m_paths.end());
}

bool FileResolver::contains(const fs::path &p) const {
    return std::find(m_paths.begin(), m_paths.end(), p) != m_paths.end();
}

fs::path FileResolver::resolve(const fs::path &path) const {
    if (!path.is_absolute()) {
        for (auto const &base : m_paths) {
            fs::path combined = base / path;
            if (fs::exists(combined))
                return combined;
        }
    }
    return path;
}

std::string FileResolver::to_string() const {
    std::ostringstream oss;
    oss << "FileResolver[" << std::endl;
    for (size_t i = 0; i < m_paths.size(); ++i) {
        oss << "  \"" << m_paths[i] << "\"";
        if (i + 1 < m_paths.size())
            oss << ",";
        oss << std::endl;
    }
    oss << "]";
    return oss.str();
}

static ref<FileResolver> __static_file_resolver;
static thread_local FileResolver *__thread_file_resolver = nullptr;

void set_file_resolver(FileResolver *file_resolver) { __static_file_resolver = file_resolver; }

FileResolver *file_resolver() {
    FileResolver *fr = __thread_file_resolver;
    return fr ? fr : __static_file_resolver.get();
}

ScopedFileResolver::ScopedFileResolver(FileResolver *fs)
    : m_backup(__thread_file_resolver) {
    __thread_file_resolver = fs;
}

ScopedFileResolver::~ScopedFileResolver() { __thread_file_resolver = m_backup; }

NAMESPACE_END(mitsuba)
