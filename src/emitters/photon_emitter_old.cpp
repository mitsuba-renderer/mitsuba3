#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/fresolver.h>
#include <vector>

// std::cout << "!!!" << std::endl;

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class PhotonEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_medium, m_to_world)
    MI_IMPORT_TYPES(Scene, Texture)

    PhotonEmitter(const Properties &props) : Base(props) {
        m_flags = +EmitterFlags::DeltaPosition;
        m_intensity = props.texture_d65<Texture>("intensity", 1.f);

        // Read the file
        FileResolver *fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_filename = file_path.filename().string();
        ref<FileStream> binaryStream = new FileStream(file_path, FileStream::ERead);
        binaryStream -> set_byte_order(Stream::ELittleEndian);

        // The first line of the file shows the number of the photons
        size_t count;
        binaryStream->read(&count, sizeof(size_t));
        size_t counter = 0.;
        // Create vectors to store the coodrinations
        std::vector<float> origin_x, origin_y, origin_z, target_x, target_y, target_z;
        while (counter != count) {
            // Declare three float variables
            float x,y,z;
            binaryStream->read(&x, sizeof(float));
            binaryStream->read(&y, sizeof(float));
            binaryStream->read(&z, sizeof(float));
            origin_x.push_back(x);
            origin_y.push_back(y);
            origin_z.push_back(z);
            // Declare three float variables
            float a,b,c;
            binaryStream->read(&a, sizeof(float));
            binaryStream->read(&b, sizeof(float));
            binaryStream->read(&c, sizeof(float));
            target_x.push_back(a);
            target_y.push_back(b);
            target_z.push_back(c);
            counter ++;
        }
        binaryStream->close();
        // Load them each into separate Float variables
        Float float_origin_x = dr::load<Float>(origin_x.data(), count);
        Float float_origin_y = dr::load<Float>(origin_y.data(), count);
        Float float_origin_z = dr::load<Float>(origin_z.data(), count);
        Float float_target_x = dr::load<Float>(target_x.data(), count);
        Float float_target_y = dr::load<Float>(target_y.data(), count);
        Float float_target_z = dr::load<Float>(target_z.data(), count);
        // Create two Point3f and one Vector3f for origins, targets and ups from these Float variables
        Point3f origin(float_origin_x, float_origin_y, float_origin_z);
        Point3f target(float_target_x, float_target_y, float_target_z);
        Vector3f up(0, 0, 1);
        Transform4f camera_coord = Transform4f::look_at(origin, target, up);
        // Get the matrix Matrix4f from the Transform4f
        m_transforms = camera_coord.matrix;

        if (m_intensity->is_spatially_varying())
            Throw("The parameter 'intensity' cannot be spatially varying (e.g. bitmap type)!");
        // dr::set_attr(this, "flags", m_flags);
        // degree to radiance: degree * pi / 180
        m_cutoff_angle = dr::deg_to_rad(0.01f);
        m_beam_width   = dr::deg_to_rad(0.01f*3.0f / 4.0f);
        // if the m_cutoff_angle is equal to m_beam_width, the denominator will be 0, it's impossible!
        m_inv_transition_width = 1.0f / (m_cutoff_angle - m_beam_width);
        m_cos_cutoff_angle = dr::cos(m_cutoff_angle);
        m_cos_beam_width   = dr::cos(m_beam_width);
        Assert(dr::all(m_cutoff_angle >= m_beam_width));
        // Avoid baking
        dr::make_opaque(m_beam_width, m_cutoff_angle,
                        m_cos_beam_width, m_cos_cutoff_angle,
                        m_inv_transition_width, m_transforms);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &/*patial_sample*/,
                                          const Point2f & /*dir_sample*/,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        // 1. Sample directional component
        ScalarVector3f local_dir =  ScalarVector3f(0.f, 0.f, 1.f);
        Float pdf_dir = 445029;
        // Uniformly sample the light rays
        // print the width of m_transforms
        UInt32 index =  dr::arange<UInt32>(dr::width(wavelength_sample)) % dr::width(m_transforms);
        Matrix4f transforms = dr::gather<Matrix4f>(m_transforms, index);
        // Create the direction vector
        Vector3f new_dir = Transform4f(transforms).transform_affine(local_dir);

        // 2. Sample spectrum
        auto si = dr::zeros<SurfaceInteraction3f>();
        si.time = time;
        si.p    = Transform4f(transforms).translation();
        si.uv   = Point2f(0.5,0.5);
        // generate a set of random wavelengths and the corresponding spectral weight
        auto [wavelengths, spec_weight] =
            sample_wavelengths(si, wavelength_sample, active);
        Float falloff = 1.0f;
        Ray3f result = Ray3f(si.p, new_dir, time, wavelengths);
        return {result, depolarizer<Spectrum>(spec_weight * falloff / pdf_dir)};
    } 

    std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                            const Point2f &sample,
                                                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        UInt32 index =  dr::arange<UInt32>(dr::width(sample)) % dr::width(m_transforms);
        Matrix4f transforms = dr::gather<Matrix4f>(m_transforms, index);
        DirectionSample3f ds;
        ds.p        = Transform4f(transforms).translation();
        // ds.p        = m_to_world.value().translation();
        ds.n        = 0.f;
        ds.uv       = 0.f;
        ds.pdf      = 1.f;
        ds.time     = it.time;
        ds.delta    = true; 
        ds.emitter  = this;
        ds.d        = ds.p - it.p;
        ds.dist     = dr::norm(ds.d);
        Float inv_dist = dr::rcp(ds.dist);
        ds.d        *= inv_dist;
        // Vector3f local_d = m_to_world.value().inverse() * -ds.d;
        Float falloff = 1.0f;
        active &= falloff > 0.f;  // Avoid invalid texture lookups

        SurfaceInteraction3f si      = dr::zeros<SurfaceInteraction3f>();
        si.t                         = 0.f;
        si.time                      = it.time;
        si.wavelengths               = it.wavelengths;
        si.p                         = ds.p;
        UnpolarizedSpectrum radiance = m_intensity->eval(si, active);

        return { ds, depolarizer<Spectrum>(radiance & active) * (falloff * dr::square(inv_dist))};
    }

    Float pdf_direction(const Interaction3f &,
                        const DirectionSample3f &, Mask) const override {
        return 0.f;
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f & /*sample*/,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);

        Vector3f center_dir = m_to_world.value() * ScalarVector3f(0.f, 0.f, 1.f);
        PositionSample3f ps(
            /* position */ m_to_world.value().translation(), center_dir,
            /*uv*/ Point2f(0.5f), time, /*pdf*/ 1.f, /*delta*/ true
        );
        return { ps, Float(1.f) };
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        Wavelength wav;
        Spectrum weight;
        std::tie(wav, weight) = m_intensity->sample_spectrum(
                si, math::sample_shifted<Wavelength>(sample), active);;

        return { wav, weight };
    }

    Spectrum eval(const SurfaceInteraction3f &, Mask) const override {
        return 0.f;
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarPoint3f p = m_to_world.scalar() * ScalarPoint3f(0.f);
        return ScalarBoundingBox3f(p, p);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PhotonEmitter[" << std::endl
            << "  to_world = " << string::indent(m_to_world) << "," << std::endl
            << "  intensity = " << m_intensity << "," << std::endl
            << "  cutoff_angle = " << m_cutoff_angle << "," << std::endl
            << "  beam_width = " << m_beam_width << "," << std::endl
            << "  medium = " << (m_medium ? string::indent(m_medium) : "")
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    Matrix4f m_transforms;
    std::string m_filename;
    ref<Texture> m_intensity;
    Float m_beam_width, m_cutoff_angle;
    Float m_cos_beam_width, m_cos_cutoff_angle, m_inv_transition_width;
};


MI_IMPLEMENT_CLASS_VARIANT(PhotonEmitter, Emitter)
MI_EXPORT_PLUGIN(PhotonEmitter, "Photon emitter")
NAMESPACE_END(mitsuba)
