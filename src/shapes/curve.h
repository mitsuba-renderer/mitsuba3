#pragma once

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/packed.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <drjit/unique_buffer.h>
#include <cmath>
#include <string>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// Version of the curve entries of a ``.packed`` container
constexpr uint32_t PackedCurveVersion = 1;

/**
 * Control points of a set of curves, shared by the curve shape plugins
 *
 * Every control point occupies four floats (position followed by the
 * radius) of ``control_points``, in the layout that the plugins keep on the
 * device. The buffer is allocated in device-shared staging memory so that
 * `adopt()` can hand it to the device without a further host-side copy.
 *
 * ``offsets`` lists the index of the first control point of each curve,
 * followed by the total number of control points. It stays on the host,
 * where the plugins turn it into their segment index buffer.
 */
struct CurveData {
    std::string name;
    drjit::unique_buffer<float> control_points;
    std::vector<uint32_t> offsets;

    size_t curve_count() const { return offsets.size() - 1; }
    size_t control_point_count() const { return offsets.back(); }
};

/**
 * Move a staging buffer allocated by `drjit::unique_buffer` into a Dr.Jit
 * array
 *
 * The LLVM backend adopts the allocation in place, the GPU backends enqueue
 * a single asynchronous copy that reads from the shared memory directly.
 */
template <typename Buffer, typename T>
Buffer adopt(drjit::unique_buffer<T> &buf) {
    constexpr JitBackend Backend = dr::backend_v<Buffer>;
    size_t size = buf.size();

    if (size == 0)
        return Buffer();

    if constexpr (Backend == JitBackend::LLVM) {
        return Buffer::map_(buf.release(), size, /* free = */ true);
    } else if constexpr (Backend != JitBackend::None) {
        using Detached = dr::detached_t<Buffer>;
        Buffer result = Detached::steal(jit_var_mem_copy(
            Backend, Detached::Type, buf.data(), size, /* from_host = */ 0));
        buf.reset();
        return result;
    } else {
        Buffer result = dr::load<Buffer>(buf.data(), size);
        buf.reset();
        return result;
    }
}

NAMESPACE_BEGIN(detail)

template <bool Negate, size_t N>
inline void advance(const char **start_, const char *end,
                    const char (&delim)[N]) {
    const char *start = *start_;

    while (true) {
        bool is_delim = false;
        for (size_t i = 0; i < N; ++i)
            if (*start == delim[i])
                is_delim = true;
        if ((is_delim ^ Negate) || start == end)
            break;
        ++start;
    }

    *start_ = start;
}

/// Parse a text file with one ``x y z radius`` control point per line and
/// blank lines separating the curves
inline void load_text_curves(JitBackend backend, const fs::path &file_path,
                             CurveData &data) {
    auto fail = [&](const char *descr, auto... args) {
        Throw(("Error while loading curves from \"%s\": " +
               std::string(descr)).c_str(), data.name, args...);
    };

    ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path);

    // The number of control points is unknown until the file is parsed, so
    // this path accumulates them on the heap and stages them at the end
    std::vector<float> points;
    points.reserve((size_t) mmap->size() / 10);

    const char *ptr = (const char *) mmap->data();
    const char *eof = ptr + mmap->size();
    char buf[1025];
    bool new_curve = true;

    while (ptr < eof) {
        const char *next = ptr;
        advance<false>(&next, eof, "\n");

        size_t size = (size_t) (next - ptr);
        if (size >= sizeof(buf) - 1)
            fail("file contains an excessively long line! (%i characters)!", size);
        memcpy(buf, ptr, size);
        buf[size] = '\0';
        ptr = next + 1;

        const char *cur = buf, *eol = buf + size;
        advance<true>(&cur, eol, " \t\r");

        if (*cur == '\0') {
            new_curve = true;
            continue;
        }

        if (new_curve) {
            data.offsets.push_back((uint32_t) (points.size() / 4));
            new_curve = false;
        }

        float value[4];
        bool parse_error = false;
        for (size_t i = 0; i < 4; ++i) {
            const char *orig = cur;
            value[i] = string::strtof<float>(cur, (char **) &cur);
            parse_error |= cur == orig;
        }

        if (unlikely(parse_error))
            fail("Could not parse line \"%s\"!", buf);
        if (unlikely(!std::isfinite(value[0]) || !std::isfinite(value[1]) ||
                     !std::isfinite(value[2])))
            fail("Control point contains invalid position data (line: \"%s\")!", buf);
        if (unlikely(!std::isfinite(value[3])))
            fail("Control point contains invalid radius data (line: \"%s\")!", buf);

        points.insert(points.end(), value, value + 4);
    }

    if (data.offsets.empty())
        fail("Empty curve file: no control points were read!");
    data.offsets.push_back((uint32_t) (points.size() / 4));

    data.control_points =
        drjit::unique_buffer<float>(backend, points.size(), /* shared = */ true);
    memcpy(data.control_points.data(), points.data(),
           points.size() * sizeof(float));
}

/// Read a curve entry of a ``.packed`` container, selected by the ``index``
/// or ``name`` property
inline void load_packed_curves(JitBackend backend, const Properties &props,
                               const fs::path &file_path, CurveData &data) {
    ref<PackedFile> file = PackedFile::open(file_path);

    size_t index;
    if (props.has_property("name")) {
        std::string_view name = props.get<std::string_view>("name");
        index = file->find(name);
        if (index == file->entry_count())
            Throw("Error while loading curves from \"%s\": the container has "
                  "no entry named \"%s\"!", data.name, name);
    } else {
        int index_prop = props.get<int>("index", 0);
        if (index_prop < 0 || (size_t) index_prop >= file->entry_count())
            Throw("Error while loading curves from \"%s\": entry index %i is "
                  "out of range (the container has %zu entries)!",
                  data.name, index_prop, file->entry_count());
        index = (size_t) index_prop;
    }

    PackedFile::Entry e = file->entry(index);
    data.name = e.name().empty()
        ? tfm::format("%s@%zu", data.name, index)
        : std::string(e.name());

    auto fail = [&](const char *descr) {
        Throw("Error while loading curves from \"%s\": %s!", data.name, descr);
    };

    char tag[4];
    memcpy(tag, e.data(), 4);
    e.skip(4);
    if (memcmp(tag, "CURV", 4) != 0)
        fail("the container entry does not hold curves");
    if (e.read<uint32_t>() != PackedCurveVersion)
        fail("unsupported curve entry version");

    uint32_t curve_count = e.read<uint32_t>(),
             point_count = e.read<uint32_t>();
    if (curve_count == 0)
        fail("the entry holds no curves");

    data.offsets.resize((size_t) curve_count + 1);
    e.read_array(data.offsets.data(), data.offsets.size() * sizeof(uint32_t));

    // Decompress the control points straight into the staging buffer
    data.control_points = drjit::unique_buffer<float>(
        backend, (size_t) point_count * 4, /* shared = */ true);
    e.read_array(data.control_points.data(),
                 data.control_points.size() * sizeof(float));

    if (data.offsets.front() != 0 || data.offsets.back() != point_count)
        fail("invalid curve offsets");
    for (size_t i = 0; i < curve_count; ++i)
        if (data.offsets[i] > data.offsets[i + 1])
            fail("invalid curve offsets");
}

NAMESPACE_END(detail)

/**
 * Load the curves named by the ``filename`` property
 *
 * A ``.packed`` container is read through the ``index`` or ``name``
 * property, any other file is parsed as text. ``to_world`` transforms the
 * positions, the radii stay as stored. Every curve must consist of at least
 * ``min_points`` control points.
 *
 * The control points end up in staging memory allocated on ``backend``. The
 * caller can still read them on the host before handing them to `adopt()`.
 */
inline CurveData load_curves(JitBackend backend, const Properties &props,
                             const AffineTransform<Point<float, 4>> &to_world,
                             size_t min_points) {
    fs::path file_path =
        file_resolver()->resolve(props.get<std::string_view>("filename"));

    CurveData data;
    data.name = file_path.filename().string();
    if (!fs::exists(file_path))
        Throw("Error while loading curves from \"%s\": file not found!",
              data.name);

    if (file_path.extension() == ".packed")
        detail::load_packed_curves(backend, props, file_path, data);
    else
        detail::load_text_curves(backend, file_path, data);

    for (size_t i = 0; i < data.curve_count(); ++i) {
        if (data.offsets[i + 1] - data.offsets[i] < min_points)
            Throw("Error while loading curves from \"%s\": curves must have "
                  "at least %zu control points!", data.name, min_points);
    }

    if (to_world != AffineTransform<Point<float, 4>>()) {
        float *ptr = data.control_points.data();
        for (size_t i = 0; i < data.control_point_count(); ++i, ptr += 4)
            dr::store(ptr, to_world * dr::load<Point<float, 3>>(ptr));
    }

    return data;
}

NAMESPACE_END(mitsuba)
