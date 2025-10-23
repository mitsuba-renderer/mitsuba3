// TODO: check which of these includes are needed
//       (This is effectively a "simplified" version of spot.cpp
//        so it shouldnt need all the extra headers?)
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/volumegrid.h>
#include <drjit/dynamic.h>
#include <drjit/texture.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-photon:

Photon light source (:monosp:`photon`)
--------------------------------------

.. pluginparameters::

 * - intensity
   - |spectrum|
   - Specifies the maximum radiant intensity at the center in units of power per unit steradian. (Default: 1).
     This cannot be spatially varying (e.g. have bitmap as type).
   - |exposed|, |differentiable|

 * - filename
   - |string|
   - Specifies a binary file from which photon ray locations are loaded in the form origin x,y,z, target x,y,z

 * - photon_list
   - |VolumeGrid|
   - Specifies a mitsuba VolumeGrid object from which photon ray locations can be loaded in the form origin x,y,z, target x,y,z

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)
   - |exposed|

This plugin provides a photon light source. The coordinates of rays associated with photons can be loaded
in from either a binary file (see tests for an example) using the 'filename' parameter in a dict or XML, or 
optionally from a VolumeGrid code object when using a scene description (Python) dictionary.

.. tabs::
    .. code-tab:: xml
        :name: photon-emitter

        <emitter type="photon">
            <rgb name="intensity" value="1.0"/>
            <string name="filename" value="bintxt/photon_geometry.bin"/>
        </emitter>

    .. code-tab:: python

        'type': 'photon',
        'photon_list': photon_list,
        'intensity': intensity,

The intensity is a fixed value.

TODO: Add some figures showing what's happening here?
.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/emitter_spot_no_texture.jpg
   :caption: Two spot lights with different colors and no texture specified.
.. subfigure:: ../../resources/data/docs/images/render/emitter_spot_texture.jpg
   :caption: A spot light with a texture specified.
.. subfigend::
   :label: fig-spot-light

 */

template <typename Float, typename Spectrum>
class PhotonEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_medium, m_to_world)
    MI_IMPORT_TYPES(Scene, Texture, VolumeGrid)

    PhotonEmitter(const Properties &props) : Base(props) {
        m_flags = +EmitterFlags::DeltaPosition;
        m_intensity = props.get_emissive_texture<Texture>("intensity", 1.f);

        // Declare arrays for origin and target coordinates
        Float float_origin_x, float_origin_y, float_origin_z, float_target_x, float_target_y, float_target_z;
        std::vector<float> origin_x, origin_y, origin_z, target_x, target_y, target_z;
        int count;

        // If a photon_list property exists then load as a VolumeGrid code object
        if (props.has_property("photon_list")) {
            // If filename has also been specified then ignore it
            if (props.has_property("filename")) {
                // If filename has been specified it needs to be resolved otherwise there will be a RuntimeError
                FileResolver *fs = Thread::thread()->file_resolver();
                fs::path file_path = fs->resolve(props.get<std::string>("filename"));
                Log(Info, "The parameters 'filename' and 'photon_list' were both specified; ignoring 'filename'.");
            }

            ref<Object> other = props.get<ref<Object>>("photon_list");
            VolumeGrid *volume_grid = dynamic_cast<VolumeGrid *>(other.get());
            float *ptr = volume_grid->data();
            count = ptr[0];
            int counter = 0.;
            while (counter != count) {
                float x = ptr[1+counter*6];
                float y = ptr[2+counter*6];
                float z = ptr[3+counter*6];

                origin_x.push_back(x);
                origin_y.push_back(y);
                origin_z.push_back(z);
                // Declare three float variables
                float a = ptr[4+counter*6];
                float b = ptr[5+counter*6];
                float c = ptr[6+counter*6];

                target_x.push_back(a);
                target_y.push_back(b);
                target_z.push_back(c);
                counter ++;
            }
        // Otherwise look for a filename property and load that
        } else if (props.has_property("filename")) {
            // Read the file
            FileResolver *fs = Thread::thread()->file_resolver();
            fs::path file_path = fs->resolve(props.get<std::string>("filename"));
            m_filename = file_path.filename().string();
            ref<FileStream> binaryStream = new FileStream(file_path, FileStream::ERead);
            binaryStream -> set_byte_order(Stream::ELittleEndian);

            // The first line of the file shows the number of the photons
            size_t count_size;
            binaryStream->read(&count_size, sizeof(size_t));
            count = int(count_size);
            // std::cout << "The number of photons is: " << count << std::endl;
            size_t counter = 0.;
            while (counter != count_size) {
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
        } else {
            Throw("A photon emitter requires one of 'photon_list' (VolumeGrid code object) or 'filename' (binary file) to be specified.");
        }

        // Load them each into separate Float variables
        float_origin_x = dr::load<Float>(origin_x.data(), count);
        float_origin_y = dr::load<Float>(origin_y.data(), count);
        float_origin_z = dr::load<Float>(origin_z.data(), count);
        float_target_x = dr::load<Float>(target_x.data(), count);
        float_target_y = dr::load<Float>(target_y.data(), count);
        float_target_z = dr::load<Float>(target_z.data(), count);

        // Create two Point3f and one Vector3f for origins, targets and ups from these Float variables
        Point3f origin(float_origin_x, float_origin_y, float_origin_z);
        Point3f target(float_target_x, float_target_y, float_target_z);
        Vector3f up(0, 0, 1);
        AffineTransform4f camera_coord = AffineTransform4f::look_at(origin, target, up);
        // Get the matrix Matrix4f from the AffineTransform4f
        m_transforms = camera_coord.matrix;

        if (m_intensity->is_spatially_varying())
            Throw("The parameter 'intensity' cannot be spatially varying (e.g. bitmap type)!");

        // Avoid baking
        dr::make_opaque(m_transforms);
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
        Vector3f new_dir = AffineTransform4f(transforms) * local_dir;

        // 2. Sample spectrum
        auto si = dr::zeros<SurfaceInteraction3f>();
        si.time = time;
        si.p    = AffineTransform4f(transforms).translation();
        si.uv   = Point2f(0.5f,0.5f);
        // generate a set of random wavelengths and the corresponding spectral weight
        auto [wavelengths, spec_weight] =
            sample_wavelengths(si, wavelength_sample, active);
        // TODO: rename this to something other than "falloff"
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
        ds.p        = AffineTransform4f(transforms).translation();
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
        // TODO: rename this to something other than "falloff"
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
            << "  medium = " << (m_medium ? string::indent(m_medium) : "")
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(PhotonEmitter)
private:
    Matrix4f m_transforms;
    std::string m_filename;
    ref<Texture> m_intensity;

    MI_TRAVERSE_CB(Base, m_transforms, m_filename, m_intensity)
};


// MI_IMPLEMENT_CLASS_VARIANT(PhotonEmitter, Emitter)
MI_EXPORT_PLUGIN(PhotonEmitter)
NAMESPACE_END(mitsuba)
