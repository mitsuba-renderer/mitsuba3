#include <mitsuba/core/stream.h>
#include <mitsuba/core/dstream.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/zstream.h>

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>

#define DECLARE_RW(Type, ReadableName)                                         \
    def("read_" ReadableName,                                                  \
        [](Stream &s) {                                                        \
            Type v;                                                            \
            s.read(v);                                                         \
            return nb::cast(v);                                                \
        }, D(Stream, read, 2))                                                 \
    .def("write_" ReadableName,                                                \
         [](Stream &s, const Type &v) {                                        \
             s.write(v);                                                       \
             return nb::cast(v);                                               \
         }, D(Stream, write, 2))

MI_PY_EXPORT(Stream) {
    auto cls = MI_PY_CLASS(Stream, Object);

    nb::enum_<Stream::EByteOrder>(cls, "EByteOrder", D(Stream, EByteOrder))
        .value("EBigEndian", Stream::EBigEndian, D(Stream, EByteOrder, EBigEndian))
        .value("ELittleEndian", Stream::ELittleEndian, D(Stream, EByteOrder, ELittleEndian))
        .value("ENetworkByteOrder", Stream::ENetworkByteOrder, D(Stream, EByteOrder, ENetworkByteOrder))
        .export_values();

    cls. def_method(Stream, close)
        .def_method(Stream, set_byte_order)
        .def_method(Stream, byte_order)
        .def_method(Stream, seek)
        .def_method(Stream, truncate)
        .def_method(Stream, tell)
        .def_method(Stream, size)
        .def_method(Stream, flush)
        .def_method(Stream, can_read)
        .def_method(Stream, can_write)
        .def_static("host_byte_order", Stream::host_byte_order, D(Stream, host_byte_order))
        .def("write", [](Stream &s, nb::bytes b) {
            s.write(b.c_str(), b.size());
        }, D(Stream, write))
        .def("read", [](Stream &s, size_t size) {
            std::unique_ptr<char[]> tmp(new char[size]);
            s.read((void *) tmp.get(), size);
            return nb::bytes(tmp.get(), size);
        }, D(Stream, write))
        .def_method(Stream, skip)
        .def_method(Stream, read_line)
        .def_method(Stream, write_line)
        .DECLARE_RW(int8_t, "int8")
        .DECLARE_RW(uint8_t, "uint8")
        .DECLARE_RW(int16_t, "int16")
        .DECLARE_RW(uint16_t, "uint16")
        .DECLARE_RW(int32_t, "int32")
        .DECLARE_RW(uint32_t, "uint32")
        .DECLARE_RW(int64_t, "int64")
        .DECLARE_RW(uint64_t, "uint64")
        .DECLARE_RW(float, "single")
        .DECLARE_RW(double, "double")
        .DECLARE_RW(float, "float")
        .DECLARE_RW(bool, "bool")
        .DECLARE_RW(std::string, "string");
}

#undef DECLARE_RW

MI_PY_EXPORT(DummyStream) {
    MI_PY_CLASS(DummyStream, Stream)
        .def(nb::init<>(), D(DummyStream, DummyStream));
}

MI_PY_EXPORT(FileStream) {
    auto fs = MI_PY_CLASS(FileStream, Stream)
        .def_method(FileStream, path);

    nb::enum_<FileStream::EMode>(fs, "EMode", D(FileStream, EMode))
        .value("ERead", FileStream::ERead, D(FileStream, EMode, ERead))
        .value("EReadWrite", FileStream::EReadWrite, D(FileStream, EMode, EReadWrite))
        .value("ETruncReadWrite", FileStream::ETruncReadWrite, D(FileStream, EMode, ETruncReadWrite))
        .export_values();

    fs.def(nb::init<const mitsuba::filesystem::path &, FileStream::EMode>(),
        "p"_a, "mode"_a = FileStream::ERead, D(FileStream, FileStream));
}

MI_PY_EXPORT(MemoryStream) {
    MI_PY_CLASS(MemoryStream, Stream)
        .def(nb::init<size_t>(), D(MemoryStream, MemoryStream),
            "capacity"_a = 512)
        .def_method(MemoryStream, capacity)
        .def_method(MemoryStream, owns_buffer)
        .def("raw_buffer", [](MemoryStream &s) -> nb::bytes {
            return nb::bytes(reinterpret_cast<const char*>(s.raw_buffer()), s.size());
        });
}

MI_PY_EXPORT(ZStream) {
    auto c = MI_PY_CLASS(ZStream, Stream);

    nb::enum_<ZStream::EStreamType>(c, "EStreamType", D(ZStream, EStreamType))
        .value("EDeflateStream", ZStream::EDeflateStream, D(ZStream, EStreamType, EDeflateStream))
        .value("EGZipStream", ZStream::EGZipStream, D(ZStream, EStreamType, EGZipStream))
        .export_values();


    c.def(nb::init<Stream*, ZStream::EStreamType, int>(), D(ZStream, ZStream),
        "child_stream"_a,
        "stream_type"_a = ZStream::EDeflateStream,
        "level"_a = -1)
        .def("child_stream", [](ZStream &stream) {
            return nb::cast(stream.child_stream());
        }, D(ZStream, child_stream));
}
