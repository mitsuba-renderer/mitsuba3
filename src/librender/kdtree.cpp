#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT ShapeKDTree<Float, Spectrum>::ShapeKDTree(const Properties &props)
    : Base(SurfaceAreaHeuristic3f(
          /* kd-tree construction: Relative cost of a shape intersection
             operation in the surface area heuristic. */
          props.float_("kd_intersection_cost", 20.f),
          /* kd-tree construction: Relative cost of a kd-tree traversal
             operation in the surface area heuristic. */
          props.float_("kd_traversal_cost", 15.f),
          /* kd-tree construction: Bonus factor for cutting away regions of
             empty space */
          props.float_("kd_empty_space_bonus", .9f))) {

    /* kd-tree construction: A kd-tree node containing this many or fewer
       primitives will not be split */
    if (props.has_property("kd_stop_prims"))
        set_stop_primitives(props.int_("kd_stop_prims"));

    /* kd-tree construction: Maximum tree depth */
    if (props.has_property("kd_max_depth"))
        set_max_depth(props.int_("kd_max_depth"));

    /* kd-tree construction: Number of bins used by the min-max binning method */
    if (props.has_property("kd_min_max_bins"))
        set_min_max_bins(props.int_("kd_min_max_bins"));

    /* kd-tree construction: Enable primitive clipping? Generally leads to a
      significant improvement of the resulting tree. */
    if (props.has_property("kd_clip"))
        set_clip_primitives(props.bool_("kd_clip"));

    /* kd-tree construction: specify whether or not bad splits can be "retracted". */
    if (props.has_property("kd_retract_bad_splits"))
        set_retract_bad_splits(props.bool_("kd_retract_bad_splits"));

    /* kd-tree construction: Specify the number of primitives, at which the
       builder will switch from (approximate) Min-Max binning to the accurate
       O(n log n) SAH-based optimization method. */
    if (props.has_property("kd_exact_primitive_threshold"))
        set_exact_primitive_threshold(props.int_("kd_exact_primitive_threshold"));

    m_primitive_map.push_back(0);
}

MTS_VARIANT void ShapeKDTree<Float, Spectrum>::build() {
    Timer timer;
    Log(Info, "Building a SAH kd-tree (%i primitives) ..",
        primitive_count());

    Base::build();

    Log(Info, "Finished. (%s of storage, took %s)",
        util::mem_string(m_index_count * sizeof(Index) +
                        m_node_count * sizeof(KDNode)),
        util::time_string(timer.value())
    );
}

MTS_VARIANT void ShapeKDTree<Float, Spectrum>::add_shape(Shape *shape) {
    Assert(!ready());
    m_primitive_map.push_back(m_primitive_map.back() +
                              shape->primitive_count());
    m_shapes.push_back(shape);
    m_bbox.expand(shape->bbox());
}

MTS_VARIANT std::string ShapeKDTree<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "ShapeKDTreeKDTree[" << std::endl
        << "  shapes = [" << std::endl;
    for (auto shape : m_shapes)
        oss << "    " << string::indent(shape, 4)
            << "," << std::endl;
    oss << "  ]" << std::endl << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS_VARIANT(ShapeKDTree, TShapeKDTree)
MTS_INSTANTIATE_CLASS(ShapeKDTree)
NAMESPACE_END(mitsuba)
