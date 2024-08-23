#include <mitsuba/render/mesh.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MergeShape final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape)
    MI_IMPORT_TYPES(BSDF, Medium, Emitter, Sensor, Mesh)

    MergeShape(const Properties &props) {
        // Note: we are *not* calling the `Shape` constructor as we do not
        // want to accept various properties such as `to_world`.
        std::unordered_map<Key, ref<Mesh>, key_hasher> tbl;
        size_t visited = 0, ignored = 0;
        Timer timer;

        for (auto [unused, shape] : props.objects()) {
            ref<Mesh> mesh(dynamic_cast<Mesh *>(shape.get()));

            if (!mesh || mesh->has_mesh_attributes()) {
                m_objects.push_back(shape);
                ignored++;
                continue;
            }

            Key key;
            key.bsdf = mesh->bsdf();
            key.interior_medium = mesh->interior_medium();
            key.exterior_medium = mesh->exterior_medium();
            key.emitter = mesh->emitter();
            key.sensor = mesh->sensor();
            key.has_normals = mesh->has_vertex_normals();
            key.has_texcoords = mesh->has_vertex_texcoords();
            key.has_face_normals = mesh->has_face_normals();

            auto [it, success] = tbl.try_emplace(key, mesh);
            if (!success)
                it->second = it->second->merge(mesh);

            visited++;
        }

        for (auto &kv : tbl) {
            if (tbl.size() == 1)
                kv.second->set_id(props.id());
            m_objects.push_back((ref<Object>) kv.second);
        }

        Log(Info, "Collapsed %zu into %zu meshes. (took %s, %zu objects ignored)",
            visited, tbl.size(), util::time_string((float) timer.value()), ignored);

        if constexpr (dr::is_jit_v<Float>)
            jit_registry_put(dr::backend_v<Float>, "mitsuba::Shape", this);
    }

    std::vector<ref<Object>> expand() const override {
        return m_objects;
    }

    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }

    MI_DECLARE_CLASS()

private:
    struct Key {
        const BSDF *bsdf;
        const Medium *interior_medium;
        const Medium *exterior_medium;
        const Emitter *emitter;
        const Sensor *sensor;
        bool has_normals;
        bool has_texcoords;
        bool has_face_normals;

        bool operator==(const Key &o) const {
            return bsdf == o.bsdf &&
                   interior_medium == o.interior_medium &&
                   exterior_medium == o.exterior_medium &&
                   emitter == o.emitter &&
                   sensor == o.sensor &&
                   has_normals == o.has_normals &&
                   has_texcoords == o.has_texcoords &&
                   has_face_normals == o.has_face_normals;
        }
    };

    template <typename T, typename... Ts>
    static inline void hash_combine(std::size_t &seed, const T &v, const Ts &...rest) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        (hash_combine(seed, rest), ...);
    }

    struct key_hasher {
        size_t operator()(const Key &k) const {
            size_t seed = 0;
            int flags = (k.has_normals ? 1 : 0) + (k.has_texcoords ? 2 : 0) +
                        (k.has_face_normals ? 4 : 0);
            hash_combine(seed, k.bsdf, k.interior_medium, k.exterior_medium,
                         k.emitter, k.sensor, flags);
            return seed;
        }
    };
private:
    std::vector<ref<Object>> m_objects;
};

MI_IMPLEMENT_CLASS_VARIANT(MergeShape, Shape)
MI_EXPORT_PLUGIN(MergeShape, "MergeShape intersection primitive");
NAMESPACE_END(mitsuba)
