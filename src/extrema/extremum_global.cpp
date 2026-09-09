#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _extremum-extremum_global:

Extremum global structure (:monosp:`extremum_global`)
-----------------------------------------------------

This plugin holds the global minorant and majorant values of a volume.
At runtime, traversal is performed via a single segment determined by the
passed ``mint`` and ``maxt`` values.
*/

template <typename Float, typename Spectrum>
class ExtremumGlobal final : public Extremum<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Extremum, m_bbox, m_scale)
    MI_IMPORT_TYPES(Volume)

    using TrackingStateType    = TrackingState<Float, Spectrum>;
    using TrackingFunctionType = TrackingFunction<Float, Spectrum>;

    ExtremumGlobal(const Properties &props) : Base(props) {}

    void build(const Volume *volume) override {
        // placeholder minorant value
        m_minorant = 0.f;
        m_majorant = volume->max();
    }

    TrackingStateType traverse_extremum(
        const Ray3f &/*ray*/,
        Float mint,
        Float maxt,
        UInt32 channel,
        TrackingStateType state,
        const TrackingFunctionType &func,
        Mask active
    ) const override {
        ExtremumSegment segment(mint, maxt, m_scale*m_minorant, m_scale*m_majorant);

        struct LoopState {
            ExtremumSegment segment;
            TrackingStateType state;
            Mask advance;
            Mask active;

            DRJIT_STRUCT(LoopState, segment, state, advance, active)
        } ls {
            segment,
            state,
            /*advance =*/true,
            active
        };

        dr::tie(ls) = dr::while_loop(
        dr::make_tuple(ls),
        [](const LoopState &ls){ return ls.active; },
        [func, channel](LoopState &ls){
            std::tie(ls.advance, ls.active) =
                func(ls.segment, &ls.state, channel, ls.active);
            ls.active &= !ls.advance;
        });

        return ls.state;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ExtremumGlobal[" << std::endl
            << "  minorant = " << m_minorant << "," << std::endl
            << "  majorant = " << m_majorant << "," << std::endl
            << "  scale = " << m_scale << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(ExtremumGlobal)

private:
    ScalarFloat m_minorant;
    ScalarFloat m_majorant;
};

MI_EXPORT_PLUGIN(ExtremumGlobal)
NAMESPACE_END(mitsuba)
