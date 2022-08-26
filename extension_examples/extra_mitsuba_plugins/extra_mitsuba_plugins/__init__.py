import mitsuba as mi
import drjit as dr

class TintedDielectric(mi.BSDF):
    def __init__(self, props):
        mi.BSDF.__init__(self, props)

        # Read 'eta' and 'tint' properties from `props`
        self.eta = 1.33
        if props.has_property('eta'):
            self.eta = props['eta']

        self.tint = props['tint']

        # Set the BSDF flags
        reflection_flags   = mi.BSDFFlags.DeltaReflection   | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
        transmission_flags = mi.BSDFFlags.DeltaTransmission | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
        self.m_components  = [reflection_flags, transmission_flags]
        self.m_flags = reflection_flags | transmission_flags

    def sample(self, ctx, si, sample1, sample2, active):
        # Compute Fresnel terms
        cos_theta_i = mi.Frame3f.cos_theta(si.wi)
        r_i, cos_theta_t, eta_it, eta_ti = mi.fresnel(cos_theta_i, self.eta)
        t_i = dr.maximum(1.0 - r_i, 0.0)

        # Pick between reflection and transmission
        selected_r = (sample1 <= r_i) & active

        # Fill up the BSDFSample struct
        bs = mi.BSDFSample3f()
        bs.pdf = dr.select(selected_r, r_i, t_i)
        bs.sampled_component = dr.select(selected_r, mi.UInt32(0), mi.UInt32(1))
        bs.sampled_type      = dr.select(selected_r, mi.UInt32(+mi.BSDFFlags.DeltaReflection),
                                                     mi.UInt32(+mi.BSDFFlags.DeltaTransmission))
        bs.wo = dr.select(selected_r,
                          mi.reflect(si.wi),
                          mi.refract(si.wi, cos_theta_t, eta_ti))
        bs.eta = dr.select(selected_r, 1.0, eta_it)

        # For reflection, tint based on the incident angle (more tint at grazing angle)
        value_r = dr.lerp(mi.Color3f(self.tint), mi.Color3f(1.0), dr.clamp(cos_theta_i, 0.0, 1.0))

        # For transmission, radiance must be scaled to account for the solid angle compression
        value_t = mi.Color3f(1.0) * dr.sqr(eta_ti)

        value = dr.select(selected_r, value_r, value_t)

        return (bs, value)

    def eval(self, ctx, si, wo, active):
        return 0.0

    def pdf(self, ctx, si, wo, active):
        return 0.0

    def eval_pdf(self, ctx, si, wo, active):
        return 0.0, 0.0

    def traverse(self, callback):
        callback.put_parameter('tint', self.tint, mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        print("There is nothing to do here!")

    def to_string(self):
        return ('TintedDielectric[\n'
                '    eta=%s,\n'
                '    tint=%s,\n'
                ']' % (self.eta, self.tint))

class WeirdDielectric(mi.BSDF):
    def __init__(self, props):
        mi.BSDF.__init__(self, props)

        # Read 'eta' and 'tint' properties from `props`
        self.other_eta = 1.33
        if props.has_property('other_eta'):
            self.other_eta = props['other_eta']

        self.other_tint = props['other_tint']

        # Set the BSDF flags
        reflection_flags   = mi.BSDFFlags.DeltaReflection   | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
        transmission_flags = mi.BSDFFlags.DeltaTransmission | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
        self.m_components  = [reflection_flags, transmission_flags]
        self.m_flags = reflection_flags | transmission_flags

    def sample(self, ctx, si, sample1, sample2, active):
        # Compute Fresnel terms
        cos_theta_i = mi.Frame3f.cos_theta(si.wi)
        r_i, cos_theta_t, eta_it, eta_ti = mi.fresnel(cos_theta_i, self.other_eta)
        t_i = dr.maximum(1.0 - r_i, 0.0)

        # Pick between reflection and transmission
        selected_r = (sample1 <= r_i) & active

        # Fill up the BSDFSample struct
        bs = mi.BSDFSample3f()
        bs.pdf = dr.select(selected_r, r_i, t_i)
        bs.sampled_component = dr.select(selected_r, mi.UInt32(0), mi.UInt32(1))
        bs.sampled_type      = dr.select(selected_r, mi.UInt32(+mi.BSDFFlags.DeltaReflection),
                                                     mi.UInt32(+mi.BSDFFlags.DeltaTransmission))
        bs.wo = dr.select(selected_r,
                          mi.reflect(si.wi),
                          mi.refract(si.wi, cos_theta_t, eta_ti))
        bs.eta = dr.select(selected_r, 1.0, eta_it)

        # For reflection, tint based on the incident angle (more tint at grazing angle)
        value_r = dr.lerp(mi.Color3f(self.other_tint), mi.Color3f(1.0), dr.clamp(cos_theta_i, 0.0, 1.0))

        # For transmission, radiance must be scaled to account for the solid angle compression
        value_t = mi.Color3f(1.0) * dr.sqr(eta_ti)

        value = dr.select(selected_r, value_r, value_t)

        return (bs, value)

    def eval(self, ctx, si, wo, active):
        return 0.0

    def pdf(self, ctx, si, wo, active):
        return 0.0

    def eval_pdf(self, ctx, si, wo, active):
        return 0.0, 0.0

    def traverse(self, callback):
        callback.put_parameter('other_tint', self.other_tint, mi.ParamFlags.Differentiable)

    def parameters_changed(self, keys):
        print("There is nothing to do here!")

    def to_string(self):
        return ('WeirdDielectric[\n'
                '    other_eta=%s,\n'
                '    other_tint=%s,\n'
                ']' % (self.other_eta, self.other_tint))
