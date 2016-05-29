#include <mitsuba/core/warp.h>
#include "python.h"

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

    m2.mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)
      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)
      .mdef(warp, squareToCosineHemisphere)
      .mdef(warp, squareToUniformCone)
      .mdef(warp, squareToUniformConePdf)
      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)
      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, uniformDiskToSquareConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)
      .mdef(warp, squareToUniformTriangle)
      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)
      .mdef(warp, squareToTent)
      .mdef(warp, intervalToNonuniformTent);
}
