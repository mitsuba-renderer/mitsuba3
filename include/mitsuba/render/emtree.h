#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/emitter.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Emitter selection with a spatial hierarchy over the finite emitters
 *
 * The tree is a hierarchy over the emitters of finite extent with a nonzero
 * sampling weight, built from \ref Emitter::bounds() with the surface area
 * orientation heuristic of Conty Estevez and Kulla. Each node occupies 8
 * words in ``m_nodes`` (see \ref Node). The root occupies the first slot, and
 * every inner node points to a group of 8 consecutive slots that collapse 3
 * levels of the binary hierarchy. Slots of a group without a child have zero
 * energy.
 *
 * Every emitter in the tree receives a path code (see
 * \ref Emitter::emitter_tree_code()). Infinite emitters stay outside the
 * tree and are weighed against its root (see \ref weights()).
 *
 * The tree requires at least two finite emitters. The constructor raises
 * an exception when an emitter cannot provide its bounds.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB EmitterTree final : public Object {
public:
    MI_IMPORT_TYPES(Emitter, EmitterPtr)

    /// Build the tree and assign the emitters' path codes
    EmitterTree(std::vector<ref<Emitter>> &emitters,
                const std::vector<ref<Emitter>> &portals);

    /// See \ref Scene::sample_emitter_direction()
    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref, Point2f sample,
                     Mask two_sided, Mask active) const;

    /// See \ref Scene::pdf_emitter_direction()
    Float pdf_direction(const Interaction3f &ref, const DirectionSample3f &ds,
                        Mask two_sided, Mask active) const;

    MI_DECLARE_CLASS(EmitterTree)

private:
    /**
     * Packed 32-byte node, shared by tree traversal and portal importance.
     * ``EmitterTreeCluster::pack()`` in emtree.cpp documents the layout, and
     * the constants above it the child word.
     */
    using Node = dr::Array<UInt32, 8>;
    using UInt32Buffer = DynamicBuffer<UInt32>;

    /// Selection weights of the tree root and of the infinite emitters
    struct Weights {
        Float tree = 0.f, total = 0.f;
        std::vector<Float> infinite;
    };

    /// Result of \ref select()
    struct Selection {
        EmitterPtr emitter = nullptr;  ///< Chosen emitter, null when none
        Float weight = 0.f;            ///< Inverse selection probability, zero when none
        Float sample = 0.f;            ///< Sample rescaled to the chosen emitter
    };

    /// Importance of an emitter tree node for a receiver at ``p`` with normal ``n``
    Float importance(const Node &node, const Point3f &p,
                     const Normal3f &n, const Mask &two_sided) const;

    /**
     * \brief Importance of the infinite emitter ``index`` for the same receiver
     *
     * Environment emitters of a scene with light portals shine through the
     * portals, which act as area emitters of the environment's mean radiance.
     * Other infinite emitters use their irradiance estimate times the bound
     * on the receiver cosine.
     */
    Float infinite_importance(size_t index, const Point3f &p, const Normal3f &n,
                              const Mask &two_sided) const;

    /// Does infinite emitter ``index`` shine through the scene's portals?
    bool uses_portals(size_t index) const {
        return m_portal_nodes.size() != 0 &&
               m_infinite_emitters[index]->is_environment();
    }

    Weights weights(const Point3f &p, const Normal3f &n,
                    const Mask &two_sided) const;

    /// Select an emitter for the receiver at ``ref``
    Selection select(const Interaction3f &ref, Float sample,
                     const Mask &two_sided, Mask active) const;

    /// Probability that select() picks ``emitter`` for the receiver at ``ref``
    Float pdf(const Interaction3f &ref, const EmitterPtr &emitter,
              const Mask &two_sided, Mask active) const;

    /**
     * \brief Descend from the root to a leaf and return the leaf's child word
     *
     * Sampling picks each child with the sample ``aux``, rescales ``aux`` to
     * the chosen child and multiplies ``value`` by the inverse probability of
     * the choice. Replay follows the path code ``aux`` and multiplies
     * ``value`` by the probability of each choice.
     */
    template <bool Replay>
    UInt32 descend(const Point3f &p, const Normal3f &n, const Mask &two_sided,
                   Float &value, std::conditional_t<Replay, UInt32, Float> &aux,
                   const Mask &active) const;

    /// Packed emitter tree nodes
    UInt32Buffer m_nodes;

    /// Emitter stored in each leaf
    DynamicBuffer<EmitterPtr> m_emitters;

    /// Infinite emitters with nonzero sampling weight
    std::vector<ref<Emitter>> m_infinite_emitters;

    /// Four floats per infinite emitter: direction xyz and weighted irradiance
    DynamicBuffer<Float> m_infinite_records;

    /// One emitter tree node record per portal, with the portal area as energy
    UInt32Buffer m_portal_nodes;

    // Keep all derived buffers reachable through Scene for dr.freeze().
    MI_DECLARE_TRAVERSE_CB(m_nodes, m_emitters, m_infinite_emitters,
                           m_infinite_records, m_portal_nodes)
};

MI_EXTERN_CLASS(EmitterTree)
NAMESPACE_END(mitsuba)
