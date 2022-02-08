#include <mitsuba/core/profiler.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

#if defined(MI_ENABLE_ITTNOTIFY)
__itt_domain *mitsuba_itt_domain = nullptr;
__itt_string_handle *
    mitsuba_itt_phase[int(ProfilerPhase::ProfilerPhaseCount)] { };
#endif

void Profiler::static_initialization() {
#if defined(MI_ENABLE_ITTNOTIFY)
    mitsuba_itt_domain = __itt_domain_create("mitsuba");
    for (int i = 0; i < (int) ProfilerPhase::ProfilerPhaseCount; ++i)
        mitsuba_itt_phase[i] = __itt_string_handle_create(profiler_phase_id[i]);
#endif
}

void Profiler::static_shutdown() { }

NAMESPACE_END(mitsuba)
