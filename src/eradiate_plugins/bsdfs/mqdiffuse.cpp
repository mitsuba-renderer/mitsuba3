#include <drjit/dynamic.h>
#include <drjit/texture.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/volumegrid.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-mqdiffuse:

Measured quasi-diffuse material (:monosp:`mqdiffuse`)
-----------------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the volume to be loaded.

 * - grid
   - :monosp:`VolumeGrid object`
   - When creating a grid volume at runtime, *e.g.* from Python or C++,
     an existing ``VolumeGrid`` instance can be passed directly rather than
     loading it from the filesystem with :paramtype:`filename`.

 * - accel
   - |bool|
   - Hardware acceleration features can be used in CUDA mode. These features can
     cause small differences as hardware interpolation methods typically have a
     loss of precision (not exactly 32-bit arithmetic). (Default: true)

This plugin models the reflection of light by opaque materials with a behaviour
close to diffuse, *i.e* with no strong scattering lobe. Assumptions are as
follows:

* The material is isotropic. Consequently, only the azimuth difference matters.
* The material is gray. Consequently, no spectral dimension is used.

The data dimension order is (``cos_theta_o``, ``phi_d``, ``cos_theta_i``).
The sampling routine is uniform cosine-weighted (*i.e.* the same as for the
``diffuse`` plugin).

.. warning::
   Table values are not checked internally: ensuring that the data is consistent
   (*e.g* that the corresponding reflectance is not greater than 1) is the
   user's responsibility.
*/
template <typename Float, typename Spectrum>
class MeasuredQuasiDiffuse final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(VolumeGrid)

    MeasuredQuasiDiffuse(const Properties &props) : Base(props) {

        if (props.has_property("grid")) {
            // Creates a Bitmap texture directly from an existing Bitmap object
            if (props.has_property("filename"))
                Throw("Cannot specify both \"grid\" and \"filename\".");
            Log(Debug, "Loading volume grid from memory...");
            // Note: ref-counted, so we don't have to worry about lifetime
            ref<Object> other = props.object("grid");
            VolumeGrid *volume_grid = dynamic_cast<VolumeGrid *>(other.get());
            if (!volume_grid)
                Throw("Property \"grid\" must be a VolumeGrid instance.");
            m_volume_grid = volume_grid;
        } else {
            FileResolver *fs = Thread::thread()->file_resolver();
            fs::path file_path = fs->resolve(props.string("filename"));
            if (!fs::exists(file_path))
                Log(Error, "\"%s\": file does not exist!", file_path);
            m_volume_grid = new VolumeGrid(file_path);
        }

        m_accel = props.get<bool>("accel", true);

        ScalarVector3i res = m_volume_grid->size();
        size_t shape[4] = { (size_t) res.z(), (size_t) res.y(),
                            (size_t) res.x(), m_volume_grid->channel_count() };
        m_texture =
            Texture3f(TensorXf(m_volume_grid->data(), 4, shape), m_accel,
                      m_accel, dr::FilterMode::Linear, dr::WrapMode::Clamp);

        m_flags = BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide;
        dr::set_attr(this, "flags", m_flags);
        m_components.push_back(m_flags);
    }

    /// Evaluate underlying Dr.Jit texture (doesn't include foreshortening
    /// factor)
    void eval_texture(Float &value, const Float &cos_theta_o,
                      const Float &phi_d, const Float &cos_theta_i,
                      Mask active) const {
        Point3f p{ cos_theta_o, (phi_d / dr::TwoPi<Float>), cos_theta_i };
        ScalarPoint3f pixel_size = 1.f / m_volume_grid->size();
        p *= 1.f - pixel_size;
        p += 0.5 * pixel_size;

        if (m_accel)
            m_texture.eval(p, &value, active);
        else
            m_texture.eval_nonaccel(p, &value, active);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* sample1 */,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::DiffuseReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::DiffuseReflection;
        bs.sampled_component = 0;

        Float cos_theta_o = Frame3f::cos_theta(bs.wo),
              phi_d = dr::fmod(dr::atan2(bs.wo.y(), bs.wo.x()) -
                                   dr::atan2(si.wi.y(), si.wi.x()),
                               dr::TwoPi<Float>);
        Float value;
        eval_texture(value, cos_theta_o, phi_d, cos_theta_i, active);
        value *= cos_theta_o / bs.pdf;

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo),
              phi_d = dr::fmod(dr::atan2(wo.y(), wo.x()) -
                                   dr::atan2(si.wi.y(), si.wi.x()),
                               dr::TwoPi<Float>);
        dr::masked(phi_d, phi_d < 0.f) += dr::TwoPi<Float>;

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        Float value;
        eval_texture(value, cos_theta_o, phi_d, cos_theta_i, active);
        value *= cos_theta_o;

        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return dr::select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);

        NotImplementedError("pdf");
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MeasuredQuasiDiffuse[" << std::endl
            << "  volume_grid_size = " << m_volume_grid->size() << ","
            << std::endl
            << "  volume_grid_data = [ " << util::mem_string(buffer_size())
            << " of volume data ]" << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    /// Return the volume grid size in bytes (excluding metadata)
    size_t buffer_size() const {
        size_t bytes_per_voxel =
            sizeof(ScalarFloat) * m_volume_grid->channel_count();
        return dr::prod(m_volume_grid->size()) * bytes_per_voxel;
    }

private:
    ref<VolumeGrid> m_volume_grid;
    Texture3f m_texture;
    bool m_accel;
};

MI_IMPLEMENT_CLASS_VARIANT(MeasuredQuasiDiffuse, BSDF)
MI_EXPORT_PLUGIN(MeasuredQuasiDiffuse, "Measure quasi-diffuse material")
NAMESPACE_END(mitsuba)
