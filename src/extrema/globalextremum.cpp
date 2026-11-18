#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _extremum-globalextremum:

Global Extremum (:monosp:`globalextremum`)
-------------------------------------------

This plugin holds the global minorant and majorant values of a volume.
At runtime, traversal is performed via a single segment determined by the
passed ``mint`` and ``maxt`` values.
*/

template <typename Float, typename Spectrum>
class GlobalExtremum final : public Extremum<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Extremum, m_bbox, m_scale)
    MI_IMPORT_TYPES(Volume)

    GlobalExtremum(const Properties &props) : Base(props) {}

    void build(const Volume *volume) override {
        m_minorant = volume->min();
        m_majorant = volume->max();
    }

    ExtremumState begin(const Ray3f &ray, Float mint, Float maxt,
                        Mask active) const override {
        auto [hit, d0, d1] = m_bbox.ray_intersect(ray);

        ExtremumState state = dr::zeros<ExtremumState>();
        state.o    = ray.o;
        state.d    = ray.d;
        state.mint = dr::select(hit, dr::maximum(mint, d0), mint);
        state.maxt = dr::select(hit && active, dr::minimum(maxt, d1),
                                state.mint);
        return state;
    }

    ExtremumSegment next(ExtremumState &state, const Ray3f &,
                         Mask active) const override {
        Float maxt = dr::select(active, state.maxt, state.mint);

        ExtremumSegment segment(state.mint, maxt, m_scale * m_minorant,
                                m_scale * m_majorant);
        dr::masked(state.mint, active) = maxt;
        return segment;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "GlobalExtremum[" << std::endl
            << "  minorant = " << m_minorant << "," << std::endl
            << "  majorant = " << m_majorant << "," << std::endl
            << "  scale = " << m_scale << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(GlobalExtremum)

private:
    ScalarFloat m_minorant;
    ScalarFloat m_majorant;
};

MI_EXPORT_PLUGIN(GlobalExtremum)
NAMESPACE_END(mitsuba)
