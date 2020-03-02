import os
import numpy as np
import enoki as ek
import mitsuba

# Set the desired mitsuba variant
mitsuba.set_variant('gpu_rgb')

from mitsuba.core import Thread, math, Properties, Frame3f, Float, Vector3f, warp
from mitsuba.core.xml import load_file, load_string
from mitsuba.render import BSDF, BSDFContext, BSDFFlags, BSDFSample3f, SurfaceInteraction3f, \
                           register_bsdf, Texture

class MyDiffuseBSDF(BSDF):
    def __init__(self, props):
        BSDF.__init__(self, props)
        self.m_reflectance \
            = load_string('''<spectrum version='2.0.0' type='srgb' name="reflectance">
                                <rgb name="color" value="0.45, 0.90, 0.90"/>
                             </spectrum>''')
        self.m_flags = BSDFFlags.DiffuseReflection | BSDFFlags.FrontSide
        self.m_components = [self.m_flags]

    def sample(self, ctx, si, sample1, sample2, active):
        cos_theta_i = Frame3f.cos_theta(si.wi)

        active &= cos_theta_i > 0

        bs = BSDFSample3f()
        bs.wo  = warp.square_to_cosine_hemisphere(sample2)
        bs.pdf = warp.square_to_cosine_hemisphere_pdf(bs.wo)
        bs.eta = 1.0
        bs.sampled_type = +BSDFFlags.DiffuseReflection
        bs.sampled_component = 0

        value = self.m_reflectance.eval(si, active)

        return ( bs, ek.select(active & (bs.pdf > 0.0), value, Vector3f(0)) )

    def eval(self, ctx, si, wo, active):
        if not ctx.is_enabled(BSDFFlags.DiffuseReflection):
            return Vector3f(0)

        cos_theta_i = Frame3f.cos_theta(si.wi)
        cos_theta_o = Frame3f.cos_theta(wo)

        value = self.m_reflectance.eval(si, active) * math.InvPi * cos_theta_o

        return ek.select((cos_theta_i > 0.0) & (cos_theta_o > 0.0), value, Vector3f(0))

    def pdf(self, ctx, si, wo, active):
        if not ctx.is_enabled(BSDFFlags.DiffuseReflection):
            return Vector3f(0)

        cos_theta_i = Frame3f.cos_theta(si.wi)
        cos_theta_o = Frame3f.cos_theta(wo)

        pdf = warp.square_to_cosine_hemisphere_pdf(wo)

        return ek.select((cos_theta_i > 0.0) & (cos_theta_o > 0.0), pdf, 0.0)

    def to_string(self):
        return "MyDiffuseBSDF[reflectance = %s]".format(self.m_reflectance.to_string())

# Register our BSDF such that the XML file loader can instantiate it when loading a scene
register_bsdf("mydiffusebsdf", lambda props: MyDiffuseBSDF(props))

# Load an XML file which specifies "mydiffusebsdf" as material
filename = 'path/to/my/scene.xml'
Thread.thread().file_resolver().append(os.path.dirname(filename))
scene = load_file(filename)

scene.integrator().render(scene, scene.sensors()[0])

film = scene.sensors()[0].film()
film.set_destination_file('my-diffuse-bsdf.exr')
film.develop()
