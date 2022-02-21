#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Scalar>
void spectrum_from_file(const std::string &filename, std::vector<Scalar> &wavelengths,
                        std::vector<Scalar> &values) {
    using TYPE_FORMAT = float;

    auto fs = Thread::thread()->file_resolver();
    fs::path file_path = fs->resolve(filename);
    if (!fs::exists(file_path))
        Log(Error, "\"%s\": file does not exist!", file_path);

    Log(Info, "Loading spectral data file \"%s\" ..", file_path);
    ref<FileStream> file = new FileStream(file_path);

    std::string extension = string::to_lower(file_path.extension().string());
    if (extension == ".spd") {
        std::string line;
        while (true) {
            try {
                line = file->read_line();
                if (line.size() == 0 || line[0] == '#')
                    continue;
                std::vector<std::string> tokens = string::tokenize(line, " ");
                if (tokens.size() == 0 || tokens.size() > 2)
                    Log(Error, "\"%s\": expected wavelength <space> value , but found:\n%s!", file_path, line);
                Scalar wav = string::stof<Scalar>(tokens[0]);
                Scalar value = string::stof<Scalar>(tokens[1]);
                wavelengths.push_back(wav);
                values.push_back(value);
            } catch (std::exception &) {
                break;
            }
        }
    } else if (extension == ".spb") {
        char header[3];
        file->read(header, 3);
        if (header[0] != 'S' || header[1] != 'P' || header[2] != 'B')
            Throw("Invalid binary spectra file!");

        uint8_t version;
        file->read(version);
        if (version != 1)
            Throw("Invalid version, currently only version 1 is supported (found %d)", version);

        size_t n_values;
        file->read(n_values);
        wavelengths.resize(n_values);
        values.resize(n_values);

        if constexpr (std::is_same_v<Scalar, TYPE_FORMAT>) {
            file->read_array(wavelengths.data(), n_values);
            file->read_array(values.data(), n_values);
        } else {
            std::vector<TYPE_FORMAT> input1(n_values), input2(n_values);
            file->read_array(input1.data(), n_values);
            file->read_array(input2.data(), n_values);

            for (size_t i=0; i<n_values; ++i) {
                wavelengths[i] = (Scalar) input1[i];
                values[i] = (Scalar) input2[i];
            }
        }
    } else {
        Log(Error, "You need to provide a valid extension like \".spd\" to read"
                   "the information in textual or \".spb\" in binary.");
    }
}

template <typename Scalar>
void spectrum_to_file(const std::string &filename, const std::vector<Scalar> &wavelengths,
                      const std::vector<Scalar> &values) {
    using TYPE_FORMAT = float;

    auto fs = Thread::thread()->file_resolver();
    fs::path file_path = fs->resolve(filename);

    if (wavelengths.size() != values.size())
        Log(Error, "Wavelengths size (%u) need to be equal to values size (%u)",
            wavelengths.size(), values.size());

    Log(Info, "Writing spectral data to file \"%s\" ..", file_path);
    ref<FileStream> file = new FileStream(file_path, FileStream::ETruncReadWrite);
    std::string extension = string::to_lower(file_path.extension().string());

    if (extension == ".spd") {
        // Write file with textual spectra format
        for (size_t i = 0; i < wavelengths.size(); ++i) {
            std::ostringstream oss;
            oss << wavelengths[i] << " " << values[i];
            file->write_line(oss.str());
        }
    } else if (extension == ".spb") {
        // Write file with binary spectra format
        file->write("SPB", 3);                              // FORMAT NAME
        file->write(uint8_t(1));                            // FORMAT VERSION
        file->write(size_t(wavelengths.size()));            // NUMBER ELEMENTS

        if constexpr (std::is_same_v<Scalar, TYPE_FORMAT>) {
            file->write_array(wavelengths.data(), wavelengths.size());
            file->write_array(values.data(), values.size());
        } else {
            // Need to convert data to single precision before writing to disk
            std::vector<TYPE_FORMAT> output1(wavelengths.size()), output2(values.size());
            for (size_t i = 0; i < wavelengths.size(); ++i) {
                output1[i] = (TYPE_FORMAT) wavelengths[i];
                output2[i] = (TYPE_FORMAT) values[i];
            }
            file->write_array(output1.data(), output1.size());
            file->write_array(output2.data(), output2.size());
        }
    } else {
        Log(Error, "You need to provide a valid extension like \".spd\" to store"
                   "the information in textual or \".spb\" in binary.");
    }
}

template <typename Scalar>
Color<Scalar, 3> spectrum_list_to_srgb(const std::vector<Scalar> &wavelengths,
                                       const std::vector<Scalar> &values,
                                       bool bounded) {
    Color<Scalar, 3> color = (Scalar) 0.f;

    const int steps = 1000;
    for (int i = 0; i < steps; ++i) {
        Scalar x = (Scalar) MI_CIE_MIN +
                   (i / (Scalar)(steps - 1)) * ((Scalar) MI_CIE_MAX -
                                                (Scalar) MI_CIE_MIN);

        if (x < wavelengths.front() || x > wavelengths.back())
            continue;

        // Find interval containing 'x'
        uint32_t index = math::find_interval<uint32_t>(
            (uint32_t) wavelengths.size(),
            [&](uint32_t idx) {
                return wavelengths[idx] <= x;
            });

        Scalar x0 = wavelengths[index],
               x1 = wavelengths[index + 1],
               y0 = values[index],
               y1 = values[index + 1];

        // Linear interpolant at 'x'
        Scalar y = (x*y0 - x1*y0 - x*y1 + x0*y1) / (x0 - x1);

        Color<Scalar, 3> xyz = cie1931_xyz(x);
        color += xyz * y;
    }

    // Last specified value repeats implicitly
    color *= ((Scalar) MI_CIE_MAX - (Scalar) MI_CIE_MIN) / (Scalar) steps;
    color = xyz_to_srgb(color);

    if (bounded && dr::any(color < (Scalar) 0.f || color > (Scalar) 1.f)) {
        Log(Warn, "Spectrum: clamping out-of-gamut color %s", color);
        color = clamp(color, (Scalar) 0.f, (Scalar) 1.f);
    } else if (!bounded && dr::any(color < (Scalar) 0.f)) {
        Log(Warn, "Spectrum: clamping out-of-gamut color %s", color);
        color = dr::max(color, (Scalar) 0.f);
    }

    return color;
}

/// Explicit instantiations
template MI_EXPORT_LIB void spectrum_from_file(const std::string &filename,
                                            std::vector<float> &wavelengths,
                                            std::vector<float> &values);
template MI_EXPORT_LIB void spectrum_from_file(const std::string &filename,
                                            std::vector<double> &wavelengths,
                                            std::vector<double> &values);

template MI_EXPORT_LIB void spectrum_to_file(const std::string &filename,
                                          const std::vector<float> &wavelengths,
                                          const std::vector<float> &values);
template MI_EXPORT_LIB void spectrum_to_file(const std::string &filename,
                                          const std::vector<double> &wavelengths,
                                          const std::vector<double> &values);

template MI_EXPORT_LIB Color<float, 3>
spectrum_list_to_srgb(const std::vector<float> &wavelengths,
                      const std::vector<float> &values, bool bounded);
template MI_EXPORT_LIB Color<double, 3>
spectrum_list_to_srgb(const std::vector<double> &wavelengths,
                      const std::vector<double> &values, bool bounded);

// =======================================================================
//! @{ \name CIE 1931 2 degree observer implementation
// =======================================================================
using Float = float;

static const Float cie1931_tbl[MI_CIE_SAMPLES * 3] = {
    Float(0.000129900000), Float(0.000232100000), Float(0.000414900000), Float(0.000741600000),
    Float(0.001368000000), Float(0.002236000000), Float(0.004243000000), Float(0.007650000000),
    Float(0.014310000000), Float(0.023190000000), Float(0.043510000000), Float(0.077630000000),
    Float(0.134380000000), Float(0.214770000000), Float(0.283900000000), Float(0.328500000000),
    Float(0.348280000000), Float(0.348060000000), Float(0.336200000000), Float(0.318700000000),
    Float(0.290800000000), Float(0.251100000000), Float(0.195360000000), Float(0.142100000000),
    Float(0.095640000000), Float(0.057950010000), Float(0.032010000000), Float(0.014700000000),
    Float(0.004900000000), Float(0.002400000000), Float(0.009300000000), Float(0.029100000000),
    Float(0.063270000000), Float(0.109600000000), Float(0.165500000000), Float(0.225749900000),
    Float(0.290400000000), Float(0.359700000000), Float(0.433449900000), Float(0.512050100000),
    Float(0.594500000000), Float(0.678400000000), Float(0.762100000000), Float(0.842500000000),
    Float(0.916300000000), Float(0.978600000000), Float(1.026300000000), Float(1.056700000000),
    Float(1.062200000000), Float(1.045600000000), Float(1.002600000000), Float(0.938400000000),
    Float(0.854449900000), Float(0.751400000000), Float(0.642400000000), Float(0.541900000000),
    Float(0.447900000000), Float(0.360800000000), Float(0.283500000000), Float(0.218700000000),
    Float(0.164900000000), Float(0.121200000000), Float(0.087400000000), Float(0.063600000000),
    Float(0.046770000000), Float(0.032900000000), Float(0.022700000000), Float(0.015840000000),
    Float(0.011359160000), Float(0.008110916000), Float(0.005790346000), Float(0.004109457000),
    Float(0.002899327000), Float(0.002049190000), Float(0.001439971000), Float(0.000999949300),
    Float(0.000690078600), Float(0.000476021300), Float(0.000332301100), Float(0.000234826100),
    Float(0.000166150500), Float(0.000117413000), Float(0.000083075270), Float(0.000058706520),
    Float(0.000041509940), Float(0.000029353260), Float(0.000020673830), Float(0.000014559770),
    Float(0.000010253980), Float(0.000007221456), Float(0.000005085868), Float(0.000003581652),
    Float(0.000002522525), Float(0.000001776509), Float(0.000001251141),

    Float(0.000003917000), Float(0.000006965000), Float(0.000012390000), Float(0.000022020000),
    Float(0.000039000000), Float(0.000064000000), Float(0.000120000000), Float(0.000217000000),
    Float(0.000396000000), Float(0.000640000000), Float(0.001210000000), Float(0.002180000000),
    Float(0.004000000000), Float(0.007300000000), Float(0.011600000000), Float(0.016840000000),
    Float(0.023000000000), Float(0.029800000000), Float(0.038000000000), Float(0.048000000000),
    Float(0.060000000000), Float(0.073900000000), Float(0.090980000000), Float(0.112600000000),
    Float(0.139020000000), Float(0.169300000000), Float(0.208020000000), Float(0.258600000000),
    Float(0.323000000000), Float(0.407300000000), Float(0.503000000000), Float(0.608200000000),
    Float(0.710000000000), Float(0.793200000000), Float(0.862000000000), Float(0.914850100000),
    Float(0.954000000000), Float(0.980300000000), Float(0.994950100000), Float(1.000000000000),
    Float(0.995000000000), Float(0.978600000000), Float(0.952000000000), Float(0.915400000000),
    Float(0.870000000000), Float(0.816300000000), Float(0.757000000000), Float(0.694900000000),
    Float(0.631000000000), Float(0.566800000000), Float(0.503000000000), Float(0.441200000000),
    Float(0.381000000000), Float(0.321000000000), Float(0.265000000000), Float(0.217000000000),
    Float(0.175000000000), Float(0.138200000000), Float(0.107000000000), Float(0.081600000000),
    Float(0.061000000000), Float(0.044580000000), Float(0.032000000000), Float(0.023200000000),
    Float(0.017000000000), Float(0.011920000000), Float(0.008210000000), Float(0.005723000000),
    Float(0.004102000000), Float(0.002929000000), Float(0.002091000000), Float(0.001484000000),
    Float(0.001047000000), Float(0.000740000000), Float(0.000520000000), Float(0.000361100000),
    Float(0.000249200000), Float(0.000171900000), Float(0.000120000000), Float(0.000084800000),
    Float(0.000060000000), Float(0.000042400000), Float(0.000030000000), Float(0.000021200000),
    Float(0.000014990000), Float(0.000010600000), Float(0.000007465700), Float(0.000005257800),
    Float(0.000003702900), Float(0.000002607800), Float(0.000001836600), Float(0.000001293400),
    Float(0.000000910930), Float(0.000000641530), Float(0.000000451810),

    Float(0.000606100000), Float(0.001086000000), Float(0.001946000000), Float(0.003486000000),
    Float(0.006450001000), Float(0.010549990000), Float(0.020050010000), Float(0.036210000000),
    Float(0.067850010000), Float(0.110200000000), Float(0.207400000000), Float(0.371300000000),
    Float(0.645600000000), Float(1.039050100000), Float(1.385600000000), Float(1.622960000000),
    Float(1.747060000000), Float(1.782600000000), Float(1.772110000000), Float(1.744100000000),
    Float(1.669200000000), Float(1.528100000000), Float(1.287640000000), Float(1.041900000000),
    Float(0.812950100000), Float(0.616200000000), Float(0.465180000000), Float(0.353300000000),
    Float(0.272000000000), Float(0.212300000000), Float(0.158200000000), Float(0.111700000000),
    Float(0.078249990000), Float(0.057250010000), Float(0.042160000000), Float(0.029840000000),
    Float(0.020300000000), Float(0.013400000000), Float(0.008749999000), Float(0.005749999000),
    Float(0.003900000000), Float(0.002749999000), Float(0.002100000000), Float(0.001800000000),
    Float(0.001650001000), Float(0.001400000000), Float(0.001100000000), Float(0.001000000000),
    Float(0.000800000000), Float(0.000600000000), Float(0.000340000000), Float(0.000240000000),
    Float(0.000190000000), Float(0.000100000000), Float(0.000049999990), Float(0.000030000000),
    Float(0.000020000000), Float(0.000010000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000)
};

NAMESPACE_BEGIN(detail)
CIE1932Tables<float> color_space_tables_scalar;
#if defined(MI_ENABLE_LLVM)
CIE1932Tables<dr::LLVMArray<float>> color_space_tables_llvm;
#endif
#if defined(MI_ENABLE_CUDA)
CIE1932Tables<dr::CUDAArray<float>> color_space_tables_cuda;
#endif
NAMESPACE_END(detail)

void color_management_static_initialization(bool cuda, bool llvm) {
    detail::color_space_tables_scalar.initialize(cie1931_tbl);
#if defined(MI_ENABLE_LLVM)
    if (llvm)
        detail::color_space_tables_llvm.initialize(cie1931_tbl);
#endif
#if defined(MI_ENABLE_CUDA)
    if (cuda)
        detail::color_space_tables_cuda.initialize(cie1931_tbl);
#endif
    (void) cuda; (void) llvm;
}

void color_management_static_shutdown() {
    detail::color_space_tables_scalar.release();
#if defined(MI_ENABLE_LLVM)
    detail::color_space_tables_llvm.release();
#endif
#if defined(MI_ENABLE_CUDA)
    detail::color_space_tables_cuda.release();
#endif
}

//! @}
// =======================================================================

NAMESPACE_END(mitsuba)
