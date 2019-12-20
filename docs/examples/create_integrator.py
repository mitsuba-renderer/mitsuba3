import mitsuba
from mitsuba.packet_rgb.core import *
from mitsuba.packet_rgb.render import *


class CustomIntegrator(SamplingIntegrator):
    def __init__(self, props=Properties()):
        print(f"type(props): {type(props)}")
        SamplingIntegrator.__init__(props)
        self.value_function = value_function

    def eval(self, it, active = True):
        inside = active & np.all( (it.p >= 0) & (it.p <= 1), axis=0 )
        val = self.value_function(it.p)
        return np.where(inside, val, 0.)

    def mean(self):
        return 43

props = Properties()
print(f"type(props): {type(props)}")
integrator = CustomIntegrator()