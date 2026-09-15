#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mmap.h>
#include <tsl/robin_map.h>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>

NAMESPACE_BEGIN(mitsuba)

/**
 * Container that stores several files (*entries*) in one ``.packed`` file
 *
 * The container is written in a streaming fashion: `begin()` starts a new
 * entry, whose bytes then go through `stream()` and `write_array()`, and
 * `close()` appends the dictionary that lists the position, size, and name
 * of every entry. The "File formats" section of the documentation
 * describes the resulting layout.
 *
 * Readers memory-map the container via `open()` and access entries through
 * `entry()`, which returns a cursor over the mapped bytes. Opened containers
 * are cached by filename so that a scene that references the same container
 * from many plugins shares one memory map. A cached container is remapped
 * when the file changes on disk, and `clear_cache()` releases all mappings.
 *
 * Bulk data within an entry consists of *compressed arrays*: sequences of
 * LZ4-HC blocks that `Entry::read_array()` decompresses straight into the
 * destination buffer of the caller (e.g. staging memory that a GPU upload
 * reads directly).
 */
class MI_EXPORT_LIB PackedFile : public Object {
public:
    /// Uncompressed bytes per block of a compressed array
    static constexpr size_t BlockSize = 16 * 1024 * 1024;

    /**
     * Cursor over the bytes of one container entry
     *
     * The cursor reads little-endian scalars, length-prefixed strings, and
     * compressed arrays in sequence, checking that every read stays within
     * the entry. It refers to the memory map of the container and must not
     * outlive it.
     */
    class MI_EXPORT_LIB Entry {
    public:
        /// Name of the entry (empty when none was stored)
        std::string_view name() const { return m_name; }

        /// Index of the entry within the container
        size_t index() const { return m_index; }

        /// Size of the entry in bytes
        size_t size() const { return m_end - m_begin; }

        /// Number of bytes that remain to be read
        size_t remaining() const { return m_end - m_ptr; }

        /// Pointer to the current position
        const uint8_t *data() const { return m_ptr; }

        /// Read a little-endian scalar
        template <typename T> T read() {
            T value;
            std::memcpy(&value, advance(sizeof(T)), sizeof(T));
            return value;
        }

        /// Read a string prefixed by its ``uint32`` length
        std::string read_string();

        /// Skip ``size`` bytes
        void skip(size_t size) { advance(size); }

        /**
         * Decompress an array into ``dst``
         *
         * The stored uncompressed size must equal ``size``, which is the
         * number of bytes that the caller expects (and allocated).
         */
        void read_array(void *dst, size_t size);

        /// Skip a compressed array
        void skip_array();

    private:
        friend class PackedFile;
        Entry(const PackedFile *file, size_t index);

        /// Advance the cursor and return the prior position
        const uint8_t *advance(size_t size);

        /// Raise an error that mentions the entry
        [[noreturn]] void fail(const char *descr) const;

        std::string_view m_name;
        std::string m_filename;
        size_t m_index;
        const uint8_t *m_begin, *m_ptr, *m_end;
    };

    /// Create a new container at ``filename`` for writing
    PackedFile(const fs::path &filename);

    /**
     * Open the container ``filename`` for reading
     *
     * The file is memory-mapped, and the result is shared with other
     * callers that open the same file.
     */
    static ref<PackedFile> open(const fs::path &filename);

    /// Release the memory maps of all cached containers
    static void clear_cache();

    /// Return the filename of the container
    const fs::path &filename() const { return m_filename; }

    /// Is this container being written?
    bool can_write() const { return m_stream != nullptr; }

    // =========================================================================
    //! @{ \name Reading
    // =========================================================================

    /// Number of entries
    size_t entry_count() const { return m_records.size(); }

    /// Name of the entry with the given index
    std::string_view entry_name(size_t index) const;

    /// Index of the entry with the given name, or ``entry_count()`` if absent
    size_t find(std::string_view name) const;

    /// Return a cursor over the entry with the given index
    Entry entry(size_t index) const;

    //! @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Writing
    // =========================================================================

    /// Start a new entry with the given name, finishing the previous one
    void begin(std::string_view name);

    /// Stream that receives the bytes of the current entry
    Stream *stream();

    /// Append ``size`` bytes at ``data`` to the current entry as a compressed array
    void write_array(const void *data, size_t size);

    /// Encode ``size`` bytes at ``data`` as a compressed array and write it to ``stream``
    static void write_array(Stream *stream, const void *data, size_t size);

    /// Finish the container by writing its dictionary, then close the file
    void close();

    //! @}
    // =========================================================================

    std::string to_string() const override;

    /// Finish an unfinished container and release the memory map
    ~PackedFile();

    MI_DECLARE_CLASS(PackedFile)
protected:
    /// Map an existing container
    PackedFile(const fs::path &filename, ref<MemoryMappedFile> mmap);

    /// Record the size of the entry being written
    void finish_entry();

private:
    struct Record {
        uint64_t offset = 0, size = 0;
        uint32_t name_offset = 0, name_length = 0;
    };

    fs::path m_filename;
    std::vector<Record> m_records;

    // Reading
    ref<MemoryMappedFile> m_mmap;
    const char *m_names = nullptr;
    tsl::robin_map<std::string_view, uint32_t> m_index;

    // Writing
    ref<FileStream> m_stream;
    std::string m_name_table;
};

NAMESPACE_END(mitsuba)
