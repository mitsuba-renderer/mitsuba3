#include <mitsuba/core/mmap.h>
#include <mitsuba/core/util.h>

#if defined(__LINUX__) || defined(__OSX__)
# include <sys/mman.h>
# include <fcntl.h>
# include <unistd.h>
#elif defined(__WINDOWS__)
# include <windows.h>
#endif

NAMESPACE_BEGIN(mitsuba)

struct MemoryMappedFile::MemoryMappedFilePrivate {
    fs::path filename;
#if defined(__WINDOWS__)
    HANDLE file;
    HANDLE file_mapping;
#endif
    size_t size;
    void *data;
    bool can_write;
    bool temp;

    MemoryMappedFilePrivate(const fs::path &f = "", size_t s = 0)
        : filename(f), size(s), data(nullptr), can_write(false), temp(false) { }

    void create() {
        #if defined(__LINUX__) || defined(__OSX__)
            int fd = open(filename.string().c_str(), O_RDWR | O_CREAT | O_TRUNC, 0664);
            if (fd == -1)
                Throw("Could not open \"%s\"!", filename.string());

            int result = lseek(fd, size-1, SEEK_SET);
            if (result == -1)
                Throw("Could not set file size of \"%s\"!", filename.string());
            result = write(fd, "", 1);
            if (result != 1)
                Throw("Could not write to \"%s\"!", filename.string());

            data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data == MAP_FAILED) {
                data = nullptr;
                Throw("Could not map \"%s\" to memory!", filename.string());
            }
            if (close(fd) != 0)
                Throw("close(): unable to close file!");
        #elif defined(__WINDOWS__)
            file = CreateFileW(filename.native().c_str(), GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                Throw("Could not open \"%s\": %s", filename.string(),
                    util::last_error());

            file_mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0,
                static_cast<DWORD>(size), nullptr);
            if (file_mapping == nullptr)
                Throw("CreateFileMapping: Could not map \"%s\" to memory: %s",
                    filename.string(), util::last_error());

            data = (void *) MapViewOfFile(file_mapping, FILE_MAP_WRITE, 0, 0, 0);
            if (data == nullptr)
                Throw("MapViewOfFile: Could not map \"%s\" to memory: %s",
                    filename.string(), util::last_error());
        #endif
        can_write = true;
    }

    void create_temp() {
        can_write = true;
        temp = true;

        #if defined(__LINUX__) || defined(__OSX__)
            const char *tmpdir = getenv("TMPDIR");
            std::string path_template = tmpdir != nullptr ? tmpdir : "/tmp";
            path_template += "/mitsuba_XXXXXXXX";
            char *path = strdup(path_template.c_str());
            int fd = mkstemp(path);
            if (fd == -1)
                Throw("Unable to create temporary file (1): %s", strerror(errno));
            filename = fs::path(path);
            free(path);

            int result = lseek(fd, size-1, SEEK_SET);
            if (result == -1)
                Throw("Could not set file size of \"%s\"!", filename.string());

            result = write(fd, "", 1);
            if (result != 1)
                Throw("Could not write to \"%s\"!", filename.string());

            data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data == MAP_FAILED) {
                data = nullptr;
                Throw("Could not map \"%s\" to memory!", filename.string());
            }

            if (close(fd) != 0)
                Throw("close(): unable to close file!");
        #elif defined(__WINDOWS__)
            WCHAR temp_path[MAX_PATH];
            WCHAR temp_filename[MAX_PATH];

            unsigned int ret = GetTempPathW(MAX_PATH, temp_path);
            if (ret == 0 || ret > MAX_PATH)
                Throw("GetTempPath failed(): %s", util::last_error());

            ret = GetTempFileNameW(temp_path, L"mitsuba", 0, temp_filename);
            if (ret == 0)
                Throw("GetTempFileName failed(): %s", util::last_error());

            file = CreateFileW(temp_filename, GENERIC_READ | GENERIC_WRITE,
                0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (file == INVALID_HANDLE_VALUE)
                Throw("Error while trying to create temporary file: %s",
                    util::last_error());

            filename = fs::path(temp_filename);

            file_mapping = CreateFileMapping(file, nullptr, PAGE_READWRITE, 0,
                static_cast<DWORD>(size), nullptr);
            if (file_mapping == nullptr)
                Throw("CreateFileMapping: Could not map \"%s\" to memory: %s",
                    filename.string(), util::last_error());
            data = (void *) MapViewOfFile(file_mapping, FILE_MAP_WRITE, 0, 0, 0);
            if (data == nullptr)
                Throw("MapViewOfFile: Could not map \"%s\" to memory: %s",
                    filename.string(), util::last_error());
        #endif
    }

    void map() {
        if (!fs::exists(filename))
            Throw("The file \"%s\" does not exist!", filename.string());
        if (!fs::is_regular_file(filename))
            Throw("\"%s\" is not a regular file!", filename.string());
        size = (size_t) fs::file_size(filename);

        #if defined(__LINUX__) || defined(__OSX__)
            int fd = open(filename.string().c_str(), can_write ? O_RDWR : O_RDONLY);
            if (fd == -1)
                Throw("Could not open \"%s\"!", filename.string());

            data = mmap(nullptr, size, PROT_READ | (can_write ? PROT_WRITE : 0), MAP_SHARED, fd, 0);
            if (data == MAP_FAILED) {
                data = nullptr;
                Throw("Could not map \"%s\" to memory!", filename.string());
            }

            if (close(fd) != 0)
                Throw("close(): unable to close file!");
        #elif defined(__WINDOWS__)
            file = CreateFileW(filename.native().c_str(), GENERIC_READ | (can_write ? GENERIC_WRITE : 0),
                FILE_SHARE_WRITE|FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr);

            if (file == INVALID_HANDLE_VALUE)
                Throw("Could not open \"%s\": %s", filename.string(),
                    util::last_error());

            file_mapping = CreateFileMappingW(file, nullptr, can_write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, nullptr);
            if (file_mapping == nullptr)
                Throw("CreateFileMapping: Could not map \"%s\" to memory: %s",
                    filename.string(), util::last_error());

            data = (void *) MapViewOfFile(file_mapping, can_write ? FILE_MAP_WRITE : FILE_MAP_READ, 0, 0, 0);
            if (data == nullptr)
                Throw("MapViewOfFile: Could not map \"%s\" to memory: %s",
                    filename.string(), util::last_error());
        #endif
    }

    void unmap() {
        Log(Trace, "Unmapping \"%s\" from memory", filename.string());

        #if defined(__LINUX__) || defined(__OSX__)
            if (temp) {
                /* Temporary file that will be deleted in any case:
                   invalidate dirty pages to avoid a costly flush to disk */
                int retval = msync(data, size, MS_INVALIDATE);
                if (retval != 0)
                    Throw("munmap(): unable to unmap memory: %s", strerror(errno));
            }

            int retval = munmap(data, size);
            if (retval != 0)
                Throw("munmap(): unable to unmap memory: %s", strerror(errno));
        #elif defined(__WINDOWS__)
            if (!UnmapViewOfFile(data))
                Throw("UnmapViewOfFile(): unable to unmap memory: %s", util::last_error());
            if (!CloseHandle(file_mapping))
                Throw("CloseHandle(): unable to close file mapping: %s", util::last_error());
            if (!CloseHandle(file))
                Throw("CloseHandle(): unable to close file: %s", util::last_error());
        #endif

        if (temp) {
            try {
                fs::remove(filename);
            } catch (...) {
                Log(Warn, "unmap(): Unable to delete file \"%s\"", filename.string());
            }
        }

        data = nullptr;
        size = 0;
    }
};

MemoryMappedFile::MemoryMappedFile()
    : d(new MemoryMappedFilePrivate()) { }

MemoryMappedFile::MemoryMappedFile(const fs::path &filename, size_t size)
    : d(new MemoryMappedFilePrivate(filename, size)) {
    Log(Trace, "Creating memory-mapped file \"%s\" (%s)..",
        filename.filename().string(), util::mem_string(d->size));
    d->create();
}

MemoryMappedFile::MemoryMappedFile(const fs::path &filename, bool write)
    : d(new MemoryMappedFilePrivate(filename)) {
    d->can_write = write;
    d->map();
    Log(Trace, "Mapped \"%s\" into memory (%s)..",
        filename.filename().string(), util::mem_string(d->size));
}

MemoryMappedFile::~MemoryMappedFile() {
    if (d->data) {
        try {
            d->unmap();
        } catch (std::exception &e) {
            /* Don't throw exceptions from a constructor */
            Log(Warn, "%s", e.what());
        }
    }
}

void MemoryMappedFile::resize(size_t size) {
    if (!d->data)
        Throw("Internal error in MemoryMappedFile::resize()!");
    bool temp = d->temp;
    d->temp = false;
    d->unmap();
    fs::resize_file(d->filename, size);
    d->size = size;
    d->map();
    d->temp = temp;
}

void *MemoryMappedFile::data() {
    return d->data;
}

const void *MemoryMappedFile::data() const {
    return d->data;
}

size_t MemoryMappedFile::size() const {
    return d->size;
}

bool MemoryMappedFile::can_write() const {
    return d->can_write;
}

const fs::path &MemoryMappedFile::filename() const {
    return d->filename;
}

ref<MemoryMappedFile> MemoryMappedFile::create_temporary(size_t size) {
    ref<MemoryMappedFile> result = new MemoryMappedFile();
    result->d->size = size;
    result->d->create_temp();
    return result;
}

std::string MemoryMappedFile::to_string() const {
    std::ostringstream oss;
    oss << "MemoryMappedFile[" << std::endl
        << "  filename = \"" << d->filename.string() << "\"," << std::endl
        << "  size = " << util::mem_string(d->size) << "," << std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(MemoryMappedFile, Object)

NAMESPACE_END(mitsuba)
