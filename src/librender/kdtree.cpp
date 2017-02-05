#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

ShapeKDTree::ShapeKDTree(const Properties &props)
    : Base(SurfaceAreaHeuristic3f(
          /* kd-tree construction: Relative cost of a shape intersection
             operation in the surface area heuristic. */
          props.float_("kdIntersectionCost", 20.f),
          /* kd-tree construction: Relative cost of a kd-tree traversal
             operation in the surface area heuristic. */
          props.float_("kdTraversalCost", 15.f),
          /* kd-tree construction: Bonus factor for cutting away regions of
             empty space */
          props.float_("kdEmptySpaceBonus", .9f))) {

    /* kd-tree construction: A kd-tree node containing this many or fewer
       primitives will not be split */
    if (props.has_property("kdStopPrims"))
        set_stop_primitives(props.int_("kdStopPrims"));

    /* kd-tree construction: Maximum tree depth */
    if (props.has_property("kdMaxDepth"))
        set_max_depth(props.int_("kdMaxDepth"));

    /* kd-tree construction: Number of bins used by the min-max binning method */
    if (props.has_property("kdMinMaxBins"))
        set_min_max_bins(props.int_("kdMinMaxBins"));

    /* kd-tree construction: Enable primitive clipping? Generally leads to a
      significant improvement of the resulting tree. */
    if (props.has_property("kdClip"))
        set_clip_primitives(props.bool_("kdClip"));

    /* kd-tree construction: specify whether or not bad splits can be "retracted". */
    if (props.has_property("kdRetractBadSplits"))
        set_retract_bad_splits(props.bool_("kdRetractBadSplits"));

    /* kd-tree construction: Specify the number of primitives, at which the
       builder will switch from (approximate) Min-Max binning to the accurate
       O(n log n) SAH-based optimization method. */
    if (props.has_property("kdExactPrimitiveThreshold"))
        set_exact_primitive_threshold(props.int_("kdExactPrimitiveThreshold"));

    m_primitive_map.push_back(0);
}

void ShapeKDTree::add_shape(Shape *shape) {
    Assert(!ready());
    m_primitive_map.push_back(m_primitive_map.back() + shape->primitive_count());
    m_shapes.push_back(shape);
    m_bbox.expand(shape->bbox());
}

std::string ShapeKDTree::to_string() const {
    std::ostringstream oss;
    oss << "ShapeKDTree[" << std::endl;
    oss << "  shapes = [" << std::endl;
    for (auto shape : m_shapes)
        oss << "    " << string::indent(shape->to_string(), 4) << ", " << std::endl;
    oss << "  ]" << std::endl;
    oss << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(ShapeKDTree, TShapeKDTree)
NAMESPACE_END(mitsuba)
