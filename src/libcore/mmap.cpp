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
    bool read_only;
    bool temp;

    MemoryMappedFilePrivate(const fs::path &f = "", size_t s = 0)
        : filename(f), size(s), data(NULL), read_only(false), temp(false) { }

    void create() {
        #if defined(__LINUX__) || defined(__OSX__)
            int fd = open(filename.string().c_str(), O_RDWR | O_CREAT | O_TRUNC, 0664);
            if (fd == -1)
                Log(EError, "Could not open \"%s\"!", filename.string().c_str());

            int result = lseek(fd, size-1, SEEK_SET);
            if (result == -1)
                Log(EError, "Could not set file size of \"%s\"!", filename.string().c_str());
            result = write(fd, "", 1);
            if (result != 1)
                Log(EError, "Could not write to \"%s\"!", filename.string().c_str());

            data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data == NULL)
                Log(EError, "Could not map \"%s\" to memory!", filename.string().c_str());
            if (close(fd) != 0)
                Log(EError, "close(): unable to close file!");
        #elif defined(__WINDOWS__)
            file = CreateFile(filename.string().c_str(), GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file == INVALID_HANDLE_VALUE)
                Log(EError, "Could not open \"%s\": %s", filename.string().c_str(),
                    util::last_error().c_str());

            file_mapping = CreateFileMapping(file, NULL, PAGE_READWRITE, 0,
                static_cast<DWORD>(size), NULL);
            if (file_mapping == NULL)
                Log(EError, "CreateFileMapping: Could not map \"%s\" to memory: %s",
                    filename.string().c_str(), util::last_error().c_str());

            data = (void *) MapViewOfFile(file_mapping, FILE_MAP_WRITE, 0, 0, 0);
            if (data == NULL)
                Log(EError, "MapViewOfFile: Could not map \"%s\" to memory: %s",
                    filename.string().c_str(), util::last_error().c_str());
        #endif
        read_only = false;
    }

    void createTemp() {
        read_only = false;
        temp = true;

        #if defined(__LINUX__) || defined(__OSX__)
            char *path = strdup("/tmp/mitsuba_XXXXXX");
            int fd = mkstemp(path);
            if (fd == -1)
                Log(EError, "Unable to create temporary file (1): %s", strerror(errno));
            filename = fs::path(path);
            free(path);

            int result = lseek(fd, size-1, SEEK_SET);
            if (result == -1)
                Log(EError, "Could not set file size of \"%s\"!", filename.string().c_str());

            result = write(fd, "", 1);
            if (result != 1)
                Log(EError, "Could not write to \"%s\"!", filename.string().c_str());

            data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data == NULL)
                Log(EError, "Could not map \"%s\" to memory!", filename.string().c_str());

            if (close(fd) != 0)
                Log(EError, "close(): unable to close file!");
        #elif defined(__WINDOWS__)
            WCHAR temp_path[MAX_PATH];
            WCHAR temp_filename[MAX_PATH];

            unsigned int ret = GetTempPathW(MAX_PATH, temp_path);
            if (ret == 0 || ret > MAX_PATH)
                Log(EError, "GetTempPath failed(): %s", util::last_error().c_str());

            ret = GetTempFileNameW(temp_path, L"mitsuba", 0, temp_filename);
            if (ret == 0)
                Log(EError, "GetTempFileName failed(): %s", util::last_error().c_str());

            file = CreateFileW(temp_filename, GENERIC_READ | GENERIC_WRITE,
                0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            if (file == INVALID_HANDLE_VALUE)
                Log(EError, "Error while trying to create temporary file: %s",
                    util::last_error().c_str());

            filename = fs::path(temp_filename);

            file_mapping = CreateFileMapping(file, NULL, PAGE_READWRITE, 0,
                static_cast<DWORD>(size), NULL);
            if (file_mapping == NULL)
                Log(EError, "CreateFileMapping: Could not map \"%s\" to memory: %s",
                    filename.string().c_str(), util::last_error().c_str());
            data = (void *) MapViewOfFile(file_mapping, FILE_MAP_WRITE, 0, 0, 0);
            if (data == NULL)
                Log(EError, "MapViewOfFile: Could not map \"%s\" to memory: %s",
                    filename.string().c_str(), util::last_error().c_str());
        #endif
    }

    void map() {
        if (!fs::exists(filename))
            Log(EError, "The file \"%s\" does not exist!", filename.string().c_str());
        size = (size_t) fs::file_size(filename);

        #if defined(__LINUX__) || defined(__OSX__)
            int fd = open(filename.string().c_str(), read_only ? O_RDONLY : O_RDWR);
            if (fd == -1)
                Log(EError, "Could not open \"%s\"!", filename.string().c_str());
            data = mmap(NULL, size, PROT_READ | (read_only ? 0 : PROT_WRITE), MAP_SHARED, fd, 0);
            if (data == NULL)
                Log(EError, "Could not map \"%s\" to memory!", filename.string().c_str());
            if (close(fd) != 0)
                Log(EError, "close(): unable to close file!");
        #elif defined(__WINDOWS__)
            file = CreateFile(filename.string().c_str(), GENERIC_READ | (read_only ? 0 : GENERIC_WRITE),
                FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, NULL);

            if (file == INVALID_HANDLE_VALUE)
                Log(EError, "Could not open \"%s\": %s", filename.string().c_str(),
                    util::last_error().c_str());

            file_mapping = CreateFileMapping(file, NULL, read_only ? PAGE_READONLY : PAGE_READWRITE, 0, 0, NULL);
            if (file_mapping == NULL)
                Log(EError, "CreateFileMapping: Could not map \"%s\" to memory: %s",
                    filename.string().c_str(), util::last_error().c_str());

            data = (void *) MapViewOfFile(file_mapping, read_only ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
            if (data == NULL)
                Log(EError, "MapViewOfFile: Could not map \"%s\" to memory: %s",
                    filename.string().c_str(), util::last_error().c_str());
        #endif
    }

    void unmap() {
        Log(ETrace, "Unmapping \"%s\" from memory",
            filename.string().c_str());

        #if defined(__LINUX__) || defined(__OSX__)
            if (temp) {
                /* Temporary file that will be deleted in any case:
                   invalidate dirty pages to avoid a costly flush to disk */
                int retval = msync(data, size, MS_INVALIDATE);
                if (retval != 0)
                    Log(EError, "munmap(): unable to unmap memory: %s", strerror(errno));
            }

            int retval = munmap(data, size);
            if (retval != 0)
                Log(EError, "munmap(): unable to unmap memory: %s", strerror(errno));
        #elif defined(__WINDOWS__)
            if (!UnmapViewOfFile(data))
                Log(EError, "UnmapViewOfFile(): unable to unmap memory: %s", util::last_error().c_str());
            if (!CloseHandle(file_mapping))
                Log(EError, "CloseHandle(): unable to close file mapping: %s", util::last_error().c_str());
            if (!CloseHandle(file))
                Log(EError, "CloseHandle(): unable to close file: %s", util::last_error().c_str());
        #endif

        if (temp) {
            try {
                fs::remove(filename);
            } catch (...) {
                Log(EWarn, "unmap(): Unable to delete file \"%s\"", filename.string().c_str());
            }
        }

        data = NULL;
        size = 0;
    }
};

MemoryMappedFile::MemoryMappedFile()
    : d(new MemoryMappedFilePrivate()) { }

MemoryMappedFile::MemoryMappedFile(const fs::path &filename, size_t size)
    : d(new MemoryMappedFilePrivate(filename, size)) {
    Log(ETrace, "Creating memory-mapped file \"%s\" (%s)..",
        filename.filename().string().c_str(), util::mem_string(d->size).c_str());
    d->create();
}

MemoryMappedFile::MemoryMappedFile(const fs::path &filename, bool read_only)
    : d(new MemoryMappedFilePrivate(filename)) {
    d->read_only = read_only;
    d->map();
    Log(ETrace, "Mapped \"%s\" into memory (%s)..",
        filename.filename().string().c_str(), util::mem_string(d->size).c_str());
}

MemoryMappedFile::~MemoryMappedFile() {
    if (d->data) {
        try {
            d->unmap();
        } catch (std::exception &e) {
            /* Don't throw exceptions from a constructor */
            Log(EWarn, "%s", e.what());
        }
    }
}

void MemoryMappedFile::resize(size_t size) {
    if (!d->data)
        Log(EError, "Internal error in MemoryMappedFile::resize()!");
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

bool MemoryMappedFile::is_read_only() const {
    return d->read_only;
}

const fs::path &MemoryMappedFile::filename() const {
    return d->filename;
}

ref<MemoryMappedFile> MemoryMappedFile::create_temporary(size_t size) {
    ref<MemoryMappedFile> result = new MemoryMappedFile();
    result->d->size = size;
    result->d->createTemp();
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
