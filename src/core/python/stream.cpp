#include <mitsuba/core/stream.h>
#include <mitsuba/core/dstream.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/zstream.h>

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

#define DECLARE_RW(Type, ReadableName)                                         \
    def("read_" ReadableName,                                                  \
        [](Stream &s) {                                                        \
            Type v;                                                            \
            s.read(v);                                                         \
            return py::cast(v);                                                \
        }, D(Stream, read, 2))                                                 \
    .def("write_" ReadableName,                                                \
         [](Stream &s, const Type &v) {                                        \
             s.write(v);                                                       \
             return py::cast(v);                                               \
         }, D(Stream, write, 2))

MTS_PY_EXPORT(Stream) {
    auto cls = MTS_PY_CLASS(Stream, Object);

    py::enum_<Stream::EByteOrder>(cls, "EByteOrder", D(Stream, EByteOrder))
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
        .def("write", [](Stream &s, py::bytes b) {
            std::string data(b);
            s.write(data.c_str(), data.size());
        }, D(Stream, write))
        .def("read", [](Stream &s, size_t size) {
            std::unique_ptr<char> tmp(new char[size]);
            s.read((void *) tmp.get(), size);
            return py::bytes(tmp.get(), size);
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

MTS_PY_EXPORT(DummyStream) {
    MTS_PY_CLASS(DummyStream, Stream)
        .def(py::init<>(), D(DummyStream, DummyStream));
}

MTS_PY_EXPORT(FileStream) {
    auto fs = MTS_PY_CLASS(FileStream, Stream)
        .def_method(FileStream, path);

    py::enum_<FileStream::EMode>(fs, "EMode", D(FileStream, EMode))
        .value("ERead", FileStream::ERead, D(FileStream, EMode, ERead))
        .value("EReadWrite", FileStream::EReadWrite, D(FileStream, EMode, EReadWrite))
        .value("ETruncReadWrite", FileStream::ETruncReadWrite, D(FileStream, EMode, ETruncReadWrite))
        .export_values();

    fs.def(py::init<const mitsuba::filesystem::path &, FileStream::EMode>(),
        "p"_a, "mode"_a = FileStream::ERead, D(FileStream, FileStream));
}

MTS_PY_EXPORT(MemoryStream) {
    MTS_PY_CLASS(MemoryStream, Stream)
        .def(py::init<size_t>(), D(MemoryStream, MemoryStream),
            "capacity"_a = 512)
        .def_method(MemoryStream, capacity)
        .def_method(MemoryStream, owns_buffer);
}

MTS_PY_EXPORT(ZStream) {
    auto c = MTS_PY_CLASS(ZStream, Stream);

    py::enum_<ZStream::EStreamType>(c, "EStreamType", D(ZStream, EStreamType))
        .value("EDeflateStream", ZStream::EDeflateStream, D(ZStream, EStreamType, EDeflateStream))
        .value("EGZipStream", ZStream::EGZipStream, D(ZStream, EStreamType, EGZipStream))
        .export_values();


    c.def(py::init<Stream*, ZStream::EStreamType, int>(), D(ZStream, ZStream),
        "child_stream"_a,
        "stream_type"_a = ZStream::EDeflateStream,
        "level"_a = -1)
        .def("child_stream", [](ZStream &stream) {
            return py::cast(stream.child_stream());
        }, D(ZStream, child_stream));
}
