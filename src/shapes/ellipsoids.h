#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/profiler.h>
#include <drjit/tensor.h>
#include <drjit/quaternion.h>
#include <tsl/robin_map.h>

#include "ply.h"

NAMESPACE_BEGIN(mitsuba)

template <typename T>
struct Ellipsoid {
    Point<T, 3> center;     // Center values
    Vector<T, 3> scale;     // Scale values
    dr::Quaternion<T> quat; // To world rotation quaternion values
};

constexpr uint32_t EllipsoidStructSize = 10u;

/**
 * \brief Generic container class for ellipsoids.
 *
 * This is a convenience data structure meant to hold ellipsoids shape
 * data (centers, scales, rotation) and its extra attributes.
 */
template <typename Float, typename Spectrum>
class EllipsoidsData {
public:
    MI_IMPORT_TYPES()
    using FloatStorage  = DynamicBuffer<dr::replace_scalar_t<Float, float>>;
    using BoolStorage   = DynamicBuffer<Mask>;
    using UInt32Storage = DynamicBuffer<UInt32>;
    using ArrayXf       = dr::DynamicArray<Float>;
    using AttributesMap = tsl::robin_map<std::string, FloatStorage, std::hash<std::string_view>, std::equal_to<>>;

    EllipsoidsData() { }


    /// Constructor from a single PLY file or multiple separate tensors
    EllipsoidsData(const Properties &props) {
        if (props.has_property("filename")) {
            if (props.has_property("data"))
                Throw("Cannot specify both \"data\" and \"filename\".");
            if (props.has_property("centers"))
                Throw("Cannot specify both \"centers\" and \"filename\".");

            FileResolver* fs = mitsuba::file_resolver();
            fs::path file_path = fs->resolve(props.get<std::string_view>("filename"));
            std::string name = file_path.filename().string();

            auto fail = [&](const char *descr) {
                Throw("Error while loading PLY file \"%s\": %s!", name, descr);
            };

            Log(Debug, "Loading ellipsoids from \"%s\" ..", name);
            if (!fs::exists(file_path))
                fail("file not found");

            ref<Stream> stream = new FileStream(file_path);
            ScopedPhase phase(ProfilerPhase::LoadGeometry);

            PLYHeader header;
            try {
                header = parse_ply_header(stream, name);
                if (header.ascii) {
                    if (stream->size() > 100 * 1024)
                        Log(Warn,
                            "\"%s\": performance warning -- this file uses the ASCII PLY format, which "
                            "is slow to parse. Consider converting it to the binary PLY format.",
                            name);
                    stream = parse_ascii((FileStream *) stream.get(), header.elements, name);
                }
            } catch (const std::exception &e) {
                fail(e.what());
            }

            auto &el = header.elements[0];

            // Check that the PLY files are structured as follow:
            // x, y, z
            // nx, ny, nz
            //  ... extras ...
            // scale_0, scale_1, scale_2
            // rot_0, rot_1, rot_2, rot_3

            bool failure = false;
            size_t i1 = 0;
            for (auto &f : { "x", "y", "z", "nx", "ny", "nz" })
                failure |= ((*el.struct_)[i1++].name != f);

            size_t i2 = el.struct_->field_count() - 7;
            for (auto &f : { "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3" })
                failure |= ((*el.struct_)[i2++].name != f);

            if (failure) {
                std::cout << "el.struct: " << el.struct_ << std::endl;
                Throw("Invalid structure in PLY file!");
            }

            size_t extras_count = el.struct_->field_count() - 13;
            std::vector<std::pair<std::string, uint32_t>> extras;

            bool is_3dg = ((*el.struct_)[6].name == "f_dc_0" && (*el.struct_)[el.struct_->field_count() - 8].name == "opacity");

            if (!is_3dg) {
                size_t i = 0;
                std::string prefix = "";
                size_t count = 0;
                while (i < extras_count) {
                    auto name2 = (*el.struct_)[6 + i].name;
                    auto current_prefix = name2.substr(0, name2.find_last_of("_"));
                    if (prefix == current_prefix) {
                        count++;
                    } else {
                        if (count > 0)
                            extras.push_back({ prefix, (uint32_t) count });
                        prefix = current_prefix;
                        count = 1;
                    }
                    i++;
                }
                extras.push_back({ prefix, (uint32_t) count });
            } else {
                extras.push_back({ "sh_coeffs", (uint32_t) extras_count - 1 });
                extras.push_back({ "opacity", 1 });
            }

            size_t scale_offset = el.struct_->field_count() - 7;
            size_t quat_offset  = el.struct_->field_count() - 4;

            std::unique_ptr<float[]> buf(new float[el.struct_->field_count()]);
            std::unique_ptr<float[]> ellipsoid_data(new float[el.count * EllipsoidStructSize]);

            std::vector<std::unique_ptr<float[]>> extras_data;
            for (auto [name2, dim] : extras)
                extras_data.push_back(std::unique_ptr<float[]>(new float[el.count * dim]));

            ScalarFloat scale_factor = props.get<ScalarFloat>("scale_factor", 1.f);

            auto to_world = props.get<ScalarAffineTransform4f>("to_world", ScalarAffineTransform4f());
            auto [to_world_S, to_world_Q, to_world_T] = transform_decompose(to_world.matrix, 25);
            float to_world_scale = dr::mean(dr::diag(to_world_S));

            size_t count = 0;
            for (size_t i = 0; i < el.count; i++) {
                stream->read(buf.get(), el.struct_->size());

                ScalarPoint3f center = dr::load<ScalarPoint3f>(buf.get());
                center = to_world * center;

                ScalarPoint3f scale  = dr::load<ScalarPoint3f>(buf.get() + scale_offset);
                scale = dr::exp(scale); // Scaling activation (exponential)
                scale = dr::maximum(scale, 1e-6f);
                scale *= scale_factor;
                scale *= to_world_scale;

                ScalarQuaternion4f quat = ScalarQuaternion4f(
                    buf.get()[quat_offset + 1], // i
                    buf.get()[quat_offset + 2], // j
                    buf.get()[quat_offset + 3], // k
                    buf.get()[quat_offset + 0]  // r
                );
                quat = to_world_Q * quat;
                quat = dr::normalize(quat);

                dr::store(ellipsoid_data.get() + EllipsoidStructSize * count + 0, center);
                dr::store(ellipsoid_data.get() + EllipsoidStructSize * count + 3, scale);
                dr::store(ellipsoid_data.get() + EllipsoidStructSize * count + 6, quat);

                if (is_3dg) {
                    size_t sh_coeffs_count = extras_count - 1;
                    size_t sh_n = sh_coeffs_count / 3;
                    extras_data[0].get()[count * sh_coeffs_count + 0] = buf.get()[6 + 0];
                    extras_data[0].get()[count * sh_coeffs_count + 1] = buf.get()[6 + 1];
                    extras_data[0].get()[count * sh_coeffs_count + 2] = buf.get()[6 + 2];
                    for (size_t j = 1; j < sh_n; j++) { // SH coefficients are stored in a strange order!?
                        extras_data[0].get()[count * sh_coeffs_count + j * 3 + 0] = buf.get()[6 + (j - 1) + 3];
                        extras_data[0].get()[count * sh_coeffs_count + j * 3 + 1] = buf.get()[6 + (j - 1) + sh_n + 2];
                        extras_data[0].get()[count * sh_coeffs_count + j * 3 + 2] = buf.get()[6 + (j - 1) + 2 * sh_n + 1];
                    }

                    float opacity = buf.get()[el.struct_->field_count() - 8];
                    opacity = 1.f / (1.f + dr::exp(-opacity)); // Opacity activation (sigmoid)
                    opacity = dr::clip(opacity, 1e-8f, 1.f - 1e-8f);
                    extras_data[1].get()[count] = opacity;
                } else {
                    size_t offset = 0;
                    for (size_t j = 0; j < extras.size(); j++) {
                        size_t dim = extras[j].second;
                        for (size_t k = 0; k < dim; k++) {
                            extras_data[j].get()[count * dim + k] = buf.get()[6 + offset++];
                        }
                    }
                }

                count++;
            }

            m_data = dr::load<FloatStorage>(ellipsoid_data.get(), count * EllipsoidStructSize);

            if (is_3dg) {
                m_attributes.insert({ "sh_coeffs", dr::load<FloatStorage>(extras_data[0].get(), count * (extras_count - 1)) });
                m_attributes.insert({ "opacities", dr::load<FloatStorage>(extras_data[1].get(), count) });
            } else {
                for (size_t i = 0; i < extras.size(); i++)
                    m_attributes.insert({ extras[i].first, dr::load<FloatStorage>(extras_data[i].get(), count * extras[i].second) });
            }
        } else if (props.has_property("data")) {
            if (props.has_property("filename"))
                Throw("Cannot specify both \"data\" and \"filename\".");
            if (props.has_property("centers"))
                Throw("Cannot specify both \"centers\" and \"data\".");
            if (props.has_property("scale_factor"))
                Throw("\"scale_factor\" parameter is only supported with PLY files!");
            if (props.has_property("to_world"))
                Throw("\"to_world\" is only supported when loading PLY file!");

            const TensorXf &data = props.get_any<TensorXf>("data");
            if (data.ndim() > 1 && data.shape(1) != EllipsoidStructSize)
                Throw("TensorXf data must have shape (N, EllipsoidStructSize) or (N * EllipsoidStructSize)!");

            if (data.ndim() == 1 && data.shape(0) % EllipsoidStructSize != 0)
                Throw("Flat TensorXf data width must be a multiple of EllipsoidStructSize!");

            m_data = data.array();
        } else if (props.has_property("centers")) {
            const TensorXf32 &centers = props.get_any<TensorXf32>("centers");
            const TensorXf32 &scales  = props.get_any<TensorXf32>("scales");
            const TensorXf32 &quats   = props.get_any<TensorXf32>("quaternions");

            if (props.has_property("to_world"))
                Throw("\"to_world\" is only supported when loading PLY file!");
            if (centers.shape(1) != 3)
                Throw("TensorXf centers must have shape (N, 3)!");
            if (quats.shape(1) != 4)
                Throw("TensorXf quats must have shape (N, 4)!");
            if (scales.shape(1) != 3)
                Throw("TensorXf scales must have shape (N, 3)!");
            if (props.has_property("scale_factor"))
                Throw("\"scale_factor\" parameter is only supported with PLY files!");

            if (centers.shape(0) != quats.shape(0) || centers.shape(0) != scales.shape(0))
                Throw("TensorXf centers, quaternions and scales must have the same number of rows!");

            m_data = dr::zeros<FloatStorage>(centers.shape(0) * EllipsoidStructSize);
            UInt32Storage idx = dr::arange<UInt32Storage>(centers.shape(0));
            for (int i = 0; i < 3; i++)
                dr::scatter(m_data, dr::gather<FloatStorage>(centers.array(), idx * 3 + i), idx * EllipsoidStructSize + i);
            for (int i = 0; i < 3; i++)
                dr::scatter(m_data, dr::gather<FloatStorage>(scales.array(), idx * 3 + i), idx * EllipsoidStructSize + 3 + i);
            for (int i = 0; i < 4; i++)
                dr::scatter(m_data, dr::gather<FloatStorage>(quats.array(), idx * 4 + i), idx * EllipsoidStructSize + 6 + i);
            dr::eval(m_data);
        } else {
            Throw("Must specify either \"data\" or \"filename\" or \"centers\".");
        }

        m_data_pointer = m_data.data();

        m_extent_multiplier = props.get<ScalarFloat>("extent", 3.f);
        m_extent_adaptive_clamping = props.get<bool>("extent_adaptive_clamping", false);

        // Load any other ellipsoids attributes
        auto unqueried = props.unqueried();
        if (!unqueried.empty()) {
            for (auto &key : unqueried) {
                if (key == "shell")
                    continue;
                const TensorXf &tensor = props.get_any<TensorXf>(key);
                if (tensor.ndim() != 2)
                    Throw("Ellipsoids attribute \"%s\" must be a 2 dimensional tensor!", key);
                if (tensor.shape(0) != count())
                    Throw("Ellipsoids attribute \"%s\" must have the same number of entries as ellipsoids (%u vs %u)", key, tensor.shape(0), count());

                m_attributes.insert({ key , tensor.array() });
            }
        }

        if (m_extent_adaptive_clamping && (!has_attribute("opacities"))) {
            Log(Warn, "Ellipsoids must have attribute \"opacities\" to use adaptive clamping! Disabling adaptive clamping.");
            m_extent_adaptive_clamping = false;
        }

        compute_extents();
    }

    void traverse(TraversalCallback *cb) {
        cb->put("data", m_data, ParamFlags::Differentiable);
        for (auto it = m_attributes.begin(); it != m_attributes.end(); ++it)
            cb->put(it.key(), it.value(), ParamFlags::Differentiable);

        cb->put("extent",                   m_extent_multiplier,        ParamFlags::ReadOnly);
        cb->put("extent_adaptive_clamping", m_extent_adaptive_clamping, ParamFlags::ReadOnly);
    }

    void parameters_changed() {
        if constexpr (!dr::is_cuda_v<Float>)
            m_data_pointer = m_data.data();

        for (auto& [name, attr]: m_attributes) {
            if (dr::width(attr) % count() != 0)
                Throw("Attribute \"%s\" must have the same number of entries as ellipsoids (%u vs %u)", name, dr::width(attr), count());
        }

        compute_extents();
    }

    void compute_extents() {
        if (m_extent_adaptive_clamping) {
            auto indices = dr::arange<UInt32Storage>(count());
            FloatStorage opacities = dr::gather<FloatStorage>(m_attributes["opacities"], indices);
            float alpha = 0.01f; // minimum response of the Gaussian
            m_extents = dr::sqrt(2.f * dr::log(opacities / alpha)) * m_extent_multiplier / 3.f;
        } else {
            m_extents = dr::full<FloatStorage>(m_extent_multiplier, count());
        }

        m_extents_pointer = m_extents.data();
    }

    size_t count() const { return dr::width(m_data) / EllipsoidStructSize; }

    bool has_attribute(std::string_view name) const {
        if (m_attributes.find(name) != m_attributes.end())
            return true;
        if (name == "center" || name == "quaternion" || name == "scale")
            return true;
        return false;
    }

    Float eval_attribute_1(std::string_view name,
                           const SurfaceInteraction3f &si,
                           Mask active) const {
        const auto& it = m_attributes.find(name);
        if (it != m_attributes.end()) {
            const auto& attr = it->second;
            if (dr::width(attr) == count())
                return dr::gather<Float>(attr, si.prim_index, active);
        }

        if (name == "extent")
            return extents<Float>(si.prim_index, active);

        Throw("Unknown attribute %s!", name);
    }

    Color3f eval_attribute_3(std::string_view name,
                             const SurfaceInteraction3f &si,
                             Mask active) const {
        const auto& it = m_attributes.find(name);
        if (it != m_attributes.end()) {
            const auto& attr = it->second;
            if (dr::width(attr) / count() == 3)
                return dr::gather<Color3f>(attr, si.prim_index, active);
        }

        if (name == "center" || name == "scale") {
            auto ellipsoid = get_ellipsoid<Float>(si.prim_index, active);
            if (name == "center")
                return ellipsoid.center;
            else if(name == "scale")
                return ellipsoid.scale;
        }

        Throw("Unknown attribute %s!", name);
    }

    /// Helper meta-template function to call the templated Float::gather_packet_ with a runtime size
    template <int N = 256, typename Index, typename Mask>
    ArrayXf gather_packet_dynamic(FloatStorage attr, Index index, Mask active) const {
        if constexpr (N <= 1) {
            return ArrayXf();
        } else {
            uint32_t dim = (uint32_t) (dr::width(attr) / count());
            if (dim > 256)
                Throw("Over the maximum number of dimensions for attributes! %u vs 256", dim);
            if (N == dim) {
                auto tmp = Float::template gather_packet_<N>(attr, index, active, ReduceMode::Auto);
                ArrayXf res = dr::zeros<ArrayXf>((size_t) N);
                for (int i = 0; i < N; i++)
                    res.entry(i) = tmp[i];
                return res;
            } else {
                return gather_packet_dynamic<N - 1>(attr, index, active);
            }
        }
    }

    ArrayXf eval_attribute_x(std::string_view name,
                             const SurfaceInteraction3f &si,
                             Mask active) const {
        const auto& it = m_attributes.find(name);
        if (it != m_attributes.end()) {
            const auto& attr = it->second;
            if constexpr (!dr::is_jit_v<Float>) {
                uint32_t dim = (uint32_t) (dr::width(attr) / count());
                ArrayXf res = dr::zeros<ArrayXf>(dim);
                for (uint32_t i = 0; i < dim; i++)
                    res.entry(i) = dr::gather<Float>(attr, si.prim_index * dim + i, active);
                return res;
            } else {
                return gather_packet_dynamic(attr, si.prim_index, active);
            }
        }

        if (name == "ellipsoid" || name == "center" || name == "quaternion" || name == "scale") {
            auto ellipsoid = get_ellipsoid<Float>(si.prim_index, active);
            if (name == "ellipsoid") {
                ArrayXf res = dr::zeros<ArrayXf>(EllipsoidStructSize);
                for (int i = 0; i < 3; ++i)
                    res.entry(i) = ellipsoid.center[i];
                for (int i = 0; i < 3; ++i)
                    res.entry(i + 3) = ellipsoid.scale[i];
                for (int i = 0; i < 4; ++i)
                    res.entry(i + 6) = ellipsoid.quat[i];
                return res;
            } else if (name == "center") {
                return ArrayXf({ ellipsoid.center.x(), ellipsoid.center.y(), ellipsoid.center.z() });
            } else if(name == "quaterion") {
                return ArrayXf({ ellipsoid.quat.x(), ellipsoid.quat.y(), ellipsoid.quat.z(), ellipsoid.quat.w() });
            } else if(name == "scale") {
                return ArrayXf({ ellipsoid.scale.x(), ellipsoid.scale.y(), ellipsoid.scale.z() });
            }
        }

        Throw("Unknown attribute %s!", name);
    }

    /// Helper routine to extract the data for a given ellipsoid.
    template <typename Float_, typename Index_, typename Mask_ = dr::mask_t<Float>>
    Ellipsoid<Float_> get_ellipsoid(Index_ index, Mask_ active = true) const {
        Index_ idx = index * EllipsoidStructSize;
        Ellipsoid<Float_> ellipsoid;
        if constexpr (!dr::is_jit_v<Index_>) {
            DRJIT_MARK_USED(active);
            auto tmp = dr::load<dr::Array<float, EllipsoidStructSize>>(&m_data_pointer[idx]);
            ellipsoid.center = Point<Float_, 3>((Float_) tmp[0], (Float_) tmp[1], (Float_) tmp[2]);
            ellipsoid.scale  = Vector<Float_, 3>((Float_) tmp[3], (Float_) tmp[4], (Float_) tmp[5]);
            ellipsoid.quat   = dr::Quaternion<Float_>((Float_) tmp[6], (Float_) tmp[7], (Float_) tmp[8], (Float_) tmp[9]);
        } else {
            auto tmp = Float_::template gather_packet_<EllipsoidStructSize>(m_data, index, active, ReduceMode::Auto);
            ellipsoid.center = Point<Float_, 3>(tmp[0], tmp[1], tmp[2]);
            ellipsoid.scale  = Vector<Float_, 3>(tmp[3], tmp[4], tmp[5]);
            ellipsoid.quat   = dr::Quaternion<Float_>(tmp[6], tmp[7], tmp[8], tmp[9]);
        }
        return ellipsoid;
    }

    template <typename Float_, typename Index_, typename Mask_ = dr::mask_t<Index_>>
    Float_ extents(Index_ index, Mask_ active = true) const {
        DRJIT_MARK_USED(active);
        if constexpr (!dr::is_jit_v<Index_>)
            return (Float_) m_extents_pointer[index];
        else
            return dr::gather<Float_>(m_extents, index, active);
    }

    FloatStorage& extents_data() { return m_extents; }

    const FloatStorage& extents_data() const { return m_extents; }

    FloatStorage& data() { return m_data; }

    const FloatStorage& data() const { return m_data; }

    AttributesMap& attributes() { return m_attributes; }

    const AttributesMap& attributes() const { return m_attributes; }

private:
    /// The buffer for the ellipsoid data: centers, scales, and quaternions.
    FloatStorage m_data;

    /// The pointer to the ellipsoid data above (used in Embree's kernel)
    float *m_data_pointer;

    /// The extent of the ellipsoid support defined by its shell
    ScalarFloat m_extent_multiplier;
    bool m_extent_adaptive_clamping;
    FloatStorage m_extents;

    /// The pointer to the ellipsoid extents data above (used in Embree's kernel)
    float *m_extents_pointer;

    /// Arbitrary attributes for ellipsoids
    AttributesMap m_attributes;
};

NAMESPACE_END(mitsuba)
