#include <mitsuba/core/tensor.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/util.h>
#include <tsl/robin_map.h>

NAMESPACE_BEGIN(mitsuba)

struct TensorFile::TensorFileImpl {
    tsl::robin_map<std::string, Field, std::hash<std::string_view>, std::equal_to<>> fields;
};

TensorFile::TensorFile(const fs::path &filename)
    : MemoryMappedFile(filename, false), d(new TensorFileImpl()) {
    if (size() < 12 + 2 + 4)
        Throw("Invalid tensor file: too small, truncated?");
    ref<MemoryStream> stream = new MemoryStream(data(), size());
    uint8_t header[12], version[2];
    uint32_t n_fields;
    stream->read(header, 12);
    stream->read(version, 2);
    stream->read(n_fields);

    if (memcmp(header, "tensor_file", 12) != 0)
        Throw("Invalid tensor file: invalid header.");
    else if (version[1] != 0 && version[1] != 0)
        Throw("Invalid tensor file: unknown file version.");

    Log(Info, "Loading tensor data from \"%s\" .. (%s, %i field%s)",
        filename.filename(), util::mem_string(stream->size()), n_fields,
        n_fields > 1 ? "s" : "");

    for (uint32_t i = 0; i < n_fields; ++i) {
        uint8_t dtype;
        uint16_t name_length, ndim;
        uint64_t offset;

        stream->read(name_length);
        std::string name(name_length, '\0');
        stream->read((char *) name.data(), name_length);
        stream->read(ndim);
        stream->read(dtype);
        stream->read(offset);
        if (dtype == (uint32_t) Struct::Type::Invalid || dtype > (uint32_t) Struct::Type::Float64)
            Throw("Invalid tensor file: unknown type.");

        std::vector<size_t> shape(ndim);
        for (size_t j = 0; j < (size_t) ndim; ++j) {
            uint64_t size_value;
            stream->read(size_value);
            shape[j] = (size_t) size_value;
        }

        d->fields[name] =
            Field{ (Struct::Type) dtype, static_cast<size_t>(offset), shape,
                   (const uint8_t *) data() + offset };
    }
}

/// Does the file contain a field of the specified name?
bool TensorFile::has_field(std::string_view name) const {
    return d->fields.find(name) != d->fields.end();
}

/// Return a data structure with information about the specified field
const TensorFile::Field &TensorFile::field(std::string_view name) const {
    auto it = d->fields.find(name);
    if (it == d->fields.end())
        Throw("TensorFile: field \"%s\" not found!", name);
    return it->second;
}

TensorFile::~TensorFile() { }

std::string TensorFile::to_string() const {
    std::ostringstream oss;
    oss << "TensorFile[" << std::endl
        << "  filename = \"" << filename() << "\"," << std::endl
        << "  size = " << util::mem_string(size()) << "," << std::endl
        << "  fields = {" << std::endl;

    using Pair = std::pair<std::string, Field>;
    std::vector<Pair> fields;
    for (auto it : d->fields)
         fields.emplace_back(it.first, it.second);
    std::sort(fields.begin(), fields.end(), [](const Pair &a, const Pair &b) {
        return a.second.offset < b.second.offset;
    });

    size_t ctr = 0;
    for (auto it : fields) {
        oss << "    \"" << it.first << "\"" << " => [" << std::endl
            << "      dtype = " << it.second.dtype << "," << std::endl
            << "      offset = " << it.second.offset << "," << std::endl
            << "      shape = [";
        const auto& shape = it.second.shape;
        for (size_t j = 0; j < shape.size(); ++j) {
            oss << shape[j];
            if (j + 1 < shape.size())
                oss << ", ";
        }

        oss << "]" << std::endl;

        oss << "    ]";
        if (++ctr < fields.size())
            oss << ",";
        oss << std::endl;

    }

    oss << "  }" << std::endl
        << "]";

    return oss.str();
}

std::string TensorFile::Field::to_string() const {
    std::ostringstream oss;

    oss << "TensorFile.Field[" << std::endl
        << "  dtype = " << dtype << "," << std::endl
        << "  offset = " << offset << "," << std::endl
        << "  shape = [";

    for (size_t j = 0; j < shape.size(); ++j) {
        oss << shape[j];
        if (j + 1 < shape.size())
            oss << ", ";
    }

    oss << "]" << std::endl;
    oss << "]";

    return oss.str();
}

NAMESPACE_END(mitsuba)
