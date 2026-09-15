#include <mitsuba/core/packed.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>
#include <drjit-core/jit.h>
#include <algorithm>
#include <memory>
#include <mutex>

#include <sys/stat.h>

NAMESPACE_BEGIN(mitsuba)

/// Size of the file header and of one dictionary record
static constexpr size_t HeaderSize = 24, RecordSize = 24;

/// Identifies a version of a file on disk
struct FileStamp {
    uint64_t size = 0, mtime = 0;
    bool operator==(const FileStamp &s) const {
        return size == s.size && mtime == s.mtime;
    }
};

static FileStamp file_stamp(const fs::path &path) {
#if defined(_WIN32)
    struct _stat64 s;
    if (_wstat64(path.native().c_str(), &s) != 0)
        Throw("Could not stat \"%s\"!", path.string());
    return { (uint64_t) s.st_size, (uint64_t) s.st_mtime };
#else
    struct stat s;
    if (stat(path.native().c_str(), &s) != 0)
        Throw("Could not stat \"%s\"!", path.string());
#  if defined(__APPLE__)
    uint64_t mtime = (uint64_t) s.st_mtimespec.tv_sec * 1000000000ull +
                     (uint64_t) s.st_mtimespec.tv_nsec;
#  else
    uint64_t mtime = (uint64_t) s.st_mtim.tv_sec * 1000000000ull +
                     (uint64_t) s.st_mtim.tv_nsec;
#  endif
    return { (uint64_t) s.st_size, mtime };
#endif
}

struct CachedFile {
    ref<PackedFile> file;
    FileStamp stamp;
};

/// Containers opened for reading, keyed by their absolute path. Allocated on
/// demand and never freed so that its destruction can't race with unloading.
static std::mutex *cache_mutex = new std::mutex();
static tsl::robin_map<std::string, CachedFile> *cache =
    new tsl::robin_map<std::string, CachedFile>();

// -----------------------------------------------------------------------------
// Writing
// -----------------------------------------------------------------------------

PackedFile::PackedFile(const fs::path &filename) : m_filename(filename) {
    m_stream = new FileStream(filename, FileStream::ETruncReadWrite);
    m_stream->set_byte_order(Stream::ELittleEndian);
    m_stream->write("MIPACKED", 8);
    m_stream->write((uint16_t) 1);  // version
    m_stream->write((uint16_t) 0);  // entry count, patched by close()
    m_stream->write((uint32_t) 0);  // unused
    m_stream->write((uint64_t) 0);  // dictionary offset, patched by close()
}

void PackedFile::begin(std::string_view name) {
    if (!m_stream)
        Throw("PackedFile::begin(): \"%s\" is not open for writing!",
              m_filename.string());
    if (m_records.size() == 0xFFFF)
        Throw("PackedFile::begin(): \"%s\" cannot hold more than 65535 "
              "entries!", m_filename.string());
    if (m_name_table.size() + name.size() > 0xFFFFFFFFull)
        Throw("PackedFile::begin(): \"%s\": the name table exceeds 4 GiB!",
              m_filename.string());

    finish_entry();

    Record r;
    r.offset = m_stream->tell();
    r.name_offset = (uint32_t) m_name_table.size();
    r.name_length = (uint32_t) name.size();
    m_name_table += name;
    m_records.push_back(r);
}

void PackedFile::finish_entry() {
    if (!m_records.empty())
        m_records.back().size = m_stream->tell() - m_records.back().offset;
}

Stream *PackedFile::stream() {
    if (!m_stream)
        Throw("PackedFile::stream(): \"%s\" is not open for writing!",
              m_filename.string());
    if (m_records.empty())
        Throw("PackedFile::stream(): call begin() to start an entry first!");
    return m_stream.get();
}

void PackedFile::write_array(const void *data, size_t size) {
    write_array(stream(), data, size);
}

void PackedFile::write_array(Stream *stream, const void *data, size_t size) {
    stream->write((uint64_t) size);
    stream->write((uint32_t) BlockSize);

    size_t bound = BlockSize + BlockSize / 255 + 16;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bound]);

    for (size_t offset = 0; offset < size; offset += BlockSize) {
        size_t n = std::min(BlockSize, size - offset),
               c = jit_lz4_compress((const uint8_t *) data + offset, n,
                                    buf.get(), bound);
        stream->write((uint32_t) c);
        stream->write(buf.get(), c);
    }
}

void PackedFile::close() {
    if (!m_stream)
        return;

    finish_entry();
    uint64_t dict_offset = m_stream->tell();

    for (const Record &r : m_records) {
        m_stream->write(r.offset);
        m_stream->write(r.size);
        m_stream->write(r.name_offset);
        m_stream->write(r.name_length);
    }
    m_stream->write(m_name_table.data(), m_name_table.size());

    m_stream->seek(10);
    m_stream->write((uint16_t) m_records.size());
    m_stream->seek(16);
    m_stream->write(dict_offset);
    m_stream->close();
    m_stream = nullptr;
    m_names = m_name_table.data();
}

PackedFile::~PackedFile() {
    if (m_stream) {
        try {
            close();
        } catch (std::exception &e) {
            Log(Warn, "%s", e.what());
        }
    }
}

// -----------------------------------------------------------------------------
// Reading
// -----------------------------------------------------------------------------

ref<PackedFile> PackedFile::open(const fs::path &filename) {
    if (!fs::is_regular_file(filename))
        Throw("PackedFile::open(): \"%s\" does not exist or is not a regular "
              "file!", filename.string());

    fs::path path = fs::absolute(filename);
    std::string key = path.string();
    FileStamp stamp = file_stamp(path);

    std::lock_guard<std::mutex> guard(*cache_mutex);
    auto it = cache->find(key);
    if (it != cache->end() && it->second.stamp == stamp)
        return it->second.file;

    ref<PackedFile> file = new PackedFile(path, new MemoryMappedFile(path));
    (*cache)[key] = CachedFile { file, stamp };
    return file;
}

void PackedFile::clear_cache() {
    std::lock_guard<std::mutex> guard(*cache_mutex);
    cache->clear();
}

PackedFile::PackedFile(const fs::path &filename, ref<MemoryMappedFile> mmap)
    : m_filename(filename), m_mmap(mmap) {
    auto fail = [&](const char *descr) {
        Throw("PackedFile::open(): \"%s\": %s!", m_filename.string(), descr);
    };

    const uint8_t *data = (const uint8_t *) m_mmap->data();
    size_t size = m_mmap->size();
    auto read = [&](size_t offset, auto &value) {
        std::memcpy(&value, data + offset, sizeof(value));
    };

    if (size < HeaderSize || std::memcmp(data, "MIPACKED", 8) != 0)
        fail("not a packed file");

    uint16_t version, count;
    uint64_t dict_offset;
    read(8, version);
    read(10, count);
    read(16, dict_offset);
    if (version != 1)
        fail("unsupported file version");

    size_t table_offset = dict_offset + RecordSize * (size_t) count;
    if (dict_offset < HeaderSize || dict_offset > size ||
        table_offset > size)
        fail("invalid dictionary offset");

    size_t table_size = size - table_offset;
    m_names = (const char *) data + table_offset;
    m_records.resize(count);
    for (size_t i = 0; i < count; ++i) {
        Record &r = m_records[i];
        size_t pos = dict_offset + RecordSize * i;
        read(pos, r.offset);
        read(pos + 8, r.size);
        read(pos + 16, r.name_offset);
        read(pos + 20, r.name_length);
        if (r.offset < HeaderSize || r.offset > dict_offset ||
            r.size > dict_offset - r.offset ||
            r.name_offset > table_size ||
            r.name_length > table_size - r.name_offset)
            fail("invalid dictionary record");
    }

    m_index.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        m_index.emplace(entry_name(i), i);
}

std::string_view PackedFile::entry_name(size_t index) const {
    if (index >= m_records.size())
        Throw("PackedFile::entry_name(): \"%s\": index %zu is out of range "
              "(the container has %zu entries)!",
              m_filename.string(), index, m_records.size());
    const Record &r = m_records[index];
    return std::string_view(m_names + r.name_offset, r.name_length);
}

size_t PackedFile::find(std::string_view name) const {
    auto it = m_index.find(name);
    return it != m_index.end() ? it->second : m_records.size();
}

PackedFile::Entry PackedFile::entry(size_t index) const {
    if (!m_mmap)
        Throw("PackedFile::entry(): \"%s\" is not open for reading!",
              m_filename.string());
    if (index >= m_records.size())
        Throw("PackedFile::entry(): \"%s\": index %zu is out of range "
              "(the container has %zu entries)!",
              m_filename.string(), index, m_records.size());
    return Entry(this, index);
}

std::string PackedFile::to_string() const {
    std::ostringstream oss;
    oss << "PackedFile[" << std::endl
        << "  filename = \"" << m_filename.string() << "\"," << std::endl
        << "  entries = " << m_records.size() << "," << std::endl
        << "]";
    return oss.str();
}

// -----------------------------------------------------------------------------
// Entry cursor
// -----------------------------------------------------------------------------

PackedFile::Entry::Entry(const PackedFile *file, size_t index)
    : m_name(file->entry_name(index)),
      m_filename(file->m_filename.filename().string()), m_index(index) {
    const Record &r = file->m_records[index];
    m_begin = (const uint8_t *) file->m_mmap->data() + r.offset;
    m_ptr = m_begin;
    m_end = m_begin + r.size;
}

void PackedFile::Entry::fail(const char *descr) const {
    Throw("Error while reading entry %zu (\"%s\") of \"%s\": %s!", m_index,
          m_name, m_filename, descr);
}

const uint8_t *PackedFile::Entry::advance(size_t size) {
    if (size > (size_t) (m_end - m_ptr))
        fail("unexpected end of entry");
    const uint8_t *result = m_ptr;
    m_ptr += size;
    return result;
}

std::string PackedFile::Entry::read_string() {
    uint32_t length = read<uint32_t>();
    return std::string((const char *) advance(length), length);
}

void PackedFile::Entry::read_array(void *dst, size_t size) {
    uint64_t stored = read<uint64_t>();
    uint32_t block = read<uint32_t>();
    if (stored != size)
        fail("the size of a compressed array does not match the expected "
             "size");
    if (block == 0 && size != 0)
        fail("invalid block size");

    for (size_t offset = 0; offset < size; offset += block) {
        size_t n = std::min((size_t) block, size - offset);
        uint32_t c = read<uint32_t>();
        const uint8_t *src = advance(c);
        try {
            jit_lz4_decompress(src, c, (uint8_t *) dst + offset, n);
        } catch (std::exception &e) {
            fail(e.what());
        }
    }
}

void PackedFile::Entry::skip_array() {
    uint64_t size = read<uint64_t>();
    uint32_t block = read<uint32_t>();
    if (block == 0 && size != 0)
        fail("invalid block size");

    for (uint64_t offset = 0; offset < size; offset += block)
        advance(read<uint32_t>());
}

NAMESPACE_END(mitsuba)
