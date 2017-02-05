#include <mitsuba/render/spectrum.h>
#include "python.h"

MTS_PY_EXPORT(Spectrum) {
    MTS_PY_CLASS(ContinuousSpectrum, Object)
        .mdef(ContinuousSpectrum, eval)
        .mdef(ContinuousSpectrum, pdf)
        .mdef(ContinuousSpectrum, sample);
}
