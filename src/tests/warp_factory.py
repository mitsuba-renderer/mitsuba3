from collections import OrderedDict

from mitsuba.core import warp, BoundingBox3f
from mitsuba.core.warp import SamplingType, \
                              WarpAdapter, LineWarpAdapter, PlaneWarpAdapter, \
                              IdentityWarpAdapter, SphereWarpAdapter

class WarpFactory:
    def __init__(self, adapter, name, f, pdf, arguments = [], bbox = None,
                 returnsWeight = False):
        """returnsWeight: whether the passed f and pdf functions return
        the expected (warped point, weight) pair or only the warped
        point. In the second case, we wrap the function and return a
        weight of 1.0 for every point."""
        self.adapter = adapter
        self.name = name
        self.f = f
        self.pdf = pdf
        self.arguments = arguments
        self.bbox = bbox
        self.returnsWeight = returnsWeight

    def bind(self, args):
        kwargs = dict()
        kwargs['name'] = self.name
        if self.returnsWeight:
            kwargs['f'] = lambda s: self.f(s, **args)
        else:
            kwargs['f'] = lambda s: (self.f(s, **args), 1.0)
        kwargs['pdf'] = lambda v: self.pdf(v, **args)

        kwargs['arguments'] = self.arguments
        if (self.bbox):
            if (hasattr(self.bbox, '__call__')):
                kwargs['bbox'] = self.bbox(**args)
            else:
                kwargs['bbox'] = self.bbox

        return self.adapter(**kwargs)

class IdentityWarpFactory:
    def __init__(self):
        self.name = "Identity"
        self.arguments = []

    def bind(self, args):
        return IdentityWarpAdapter()

def make_factories():
    def uniform1D(s, length):
        return length * s
    def uniform1DPdf(v, length):
        if (v >= 0) and (v <= length):
            return 1
        return 0

    """Creates an appropriate WarpFactory for each available warping function"""
    factories = [
        IdentityWarpFactory(),
        WarpFactory(PlaneWarpAdapter,
            "Square to uniform disk",
            warp.squareToUniformDisk,
            warp.squareToUniformDiskPdf),
        WarpFactory(PlaneWarpAdapter,
            "Square to uniform disk concentric",
            warp.squareToUniformDiskConcentric,
            warp.squareToUniformDiskConcentricPdf),
        WarpFactory(PlaneWarpAdapter,
            "Square to uniform triangle",
            warp.squareToUniformTriangle,
            warp.squareToUniformTrianglePdf,
            bbox = WarpAdapter.kUnitSquareBoundingBox),
        # TODO: manage the case of infinite support (need inverse mapping?)
        WarpFactory(PlaneWarpAdapter,
            "Square to 2D gaussian",
            warp.squareToStdNormal,
            warp.squareToStdNormalPdf,
            bbox = BoundingBox3f([-5, -5, 0], [5, 5, 0])),
        WarpFactory(PlaneWarpAdapter,
            "Square to tent",
            warp.squareToTent,
            warp.squareToTentPdf),

        # 2D -> 3D warps
        WarpFactory(SphereWarpAdapter,
            "Square to uniform sphere",
            warp.squareToUniformSphere,
            warp.squareToUniformSpherePdf),
        WarpFactory(SphereWarpAdapter,
            "Square to uniform hemisphere",
            warp.squareToUniformHemisphere,
            warp.squareToUniformHemispherePdf),
        WarpFactory(SphereWarpAdapter,
            "Square to cosine hemisphere",
            warp.squareToCosineHemisphere,
            warp.squareToCosineHemispherePdf),
        WarpFactory(SphereWarpAdapter,
            "Square to uniform cone",
            warp.squareToUniformCone,
            warp.squareToUniformConePdf,
            arguments = [WarpAdapter.Argument("cosCutoff",
                                              minValue = 0.0,
                                              maxValue = 1.0 - 0.001,
                                              defaultValue = 0.5,
                                              description = "Cosine cutoff")]),

        # 1D -> 1D warps
        WarpFactory(LineWarpAdapter,
            "Uniform 1D over an interval",
            uniform1D,
            uniform1DPdf,
            arguments = [WarpAdapter.Argument("length",
                                              minValue = 0.001,
                                              maxValue = 10,
                                              defaultValue = 1)],
            bbox = lambda length: BoundingBox3f([0, 0, 0], [length, 0, 0]))
    ]

    warps = OrderedDict()
    for w in factories:
        warps[w.name] = w
    return warps

# Export
factories = make_factories()
