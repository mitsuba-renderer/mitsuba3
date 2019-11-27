""" Mitsuba core support library (generic mathematical and I/O routines) """

from . import (xml, filesystem, util, math, warp, spline)

__import__("mitsuba.core.mitsuba_core_ext")

from mitsuba.scalar_rgb.core import TraversalCallback
class DefaultTraversalCallback(TraversalCallback):
    def __init__(self, prefix, target):
        TraversalCallback.__init__(self)
        self.prefix = prefix
        self.target = target

    def put_parameter(self, name, value):
        self.target[self.prefix + "." + name] = value

    def put_object(self, name, obj):
        obj.traverse(DefaultTraversalCallback(self.prefix + "." + name, self.target))
