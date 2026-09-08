import os
import sys

import drjit as dr
import mitsuba as mi

class custom_sensor(mi.Sensor):
    def __init__(self, props):
        mi.Sensor.__init__(self, props)

        self.rays = dr.zeros(mi.Ray3f)
        self.weight = mi.Float(0)
        self.dummy_film = None
    
    def sample_ray_differential(self, 
                                time,   # mi.Float
                                sample1, #wavelength_sample,  # mi.Float
                                sample2, #position_sample, 
                                sample3, #aperture_sample, 
                                mask: mi.Bool = True):

        return self.rays, self.weight

    def sample_ray(self, 
                    time,   # mi.Float
                    wavelength_sample,  # mi.Float
                    position_sample, 
                    aperture_sample, 
                    mask: mi.Bool = True):

        return self.rays, self.weight

    def film(self):
        return self.dummy_film

    def to_string(self):
        return ('Custom_sensor[\n'
                '    ray=%s,\n'
                '    weight=%s,\n'
                ']' % (self.rays, self.weight))

mi.register_sensor("custom_sensor", lambda props: custom_sensor(props)) 
