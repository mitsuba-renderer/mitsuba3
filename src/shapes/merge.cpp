#include <mitsuba/render/mesh.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>
#include <tsl/robin_map.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MergeShape final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape)
    MI_IMPORT_TYPES(Mesh)

    MergeShape(const Properties &props) {
        // Note: we are *not* calling the `Shape` constructor as we do not
        // want to accept various properties such as `to_world`.
        tsl::robin_map<MergeKey, size_t, MergeKeyHasher> group_index;
        std::vector<std::vector<Base *>> groups;
        size_t visited = 0, ignored = 0;
        Timer timer;

        for (auto &prop : props.objects()) {
            ref<Object> shape = prop.get<ref<Object>>();
            Mesh *mesh = prop.try_get<Mesh>();

            if (!mesh || mesh->has_mesh_attributes()) {
                m_objects.push_back(shape);
                ignored++;
                continue;
            }

            // A ``MergeKey`` merges compatible meshes reliably, but its value
            // includes pointer addresses and thus nondeterministic across runs.
            // The following tracks unified groups in order of occurrence.
            auto [it, inserted] =
                group_index.try_emplace(mesh->merge_key(), groups.size());
            if (inserted)
                groups.emplace_back();
            groups[it->second].push_back(mesh);
            visited++;
        }

        // Merge each group in one go, which touches the input data once
        for (auto &group : groups) {
            ref<Mesh> merged = Mesh::merge(group);

            if (groups.size() == 1 && !props.id().empty())
                merged->set_id(props.id());

            m_objects.push_back((ref<Object>) merged);
        }

        Log(Info, "Collapsed %zu into %zu meshes. (took %s, %zu objects ignored)",
            visited, groups.size(), util::time_string((float) timer.value()),
            ignored);
    }

    std::vector<ref<Object>> expand() const override {
        return m_objects;
    }

    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }

    MI_DECLARE_CLASS(MergeShape)

private:
    std::vector<ref<Object>> m_objects;
};

MI_EXPORT_PLUGIN(MergeShape)
NAMESPACE_END(mitsuba)
