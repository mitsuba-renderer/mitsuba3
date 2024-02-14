#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Scalar>
void spectrum_from_file(const fs::path &path, std::vector<Scalar> &wavelengths,
                        std::vector<Scalar> &values) {

    auto fs = Thread::thread()->file_resolver();
    fs::path file_path = fs->resolve(path);
    if (!fs::exists(file_path))
        Log(Error, "\"%s\": file does not exist!", file_path);

    Log(Info, "Loading spectral data file \"%s\" ..", file_path);
    std::string extension = string::to_lower(file_path.extension().string());
    if (extension == ".spd") {
        ref<MemoryMappedFile> mmap = new MemoryMappedFile(file_path, false);
        char *current = (char *) mmap->data(),
             *end     = current + mmap->size(),
             *tmp;
        bool comment = false;
        size_t counter = 0;
        while (current != end) {
            char c = *current;
            if (c == '#') {
                comment = true;
                current++;
            } else if (c == '\n') {
                comment = false;
                counter = 0;
                current++;
            } else if (!comment && c != ' ' && c != '\r') {
                Scalar val = string::parse_float<Scalar>(current, end, &tmp);
                current = tmp;
                if (counter == 0)
                    wavelengths.push_back(val);
                else if (counter == 1)
                    values.push_back(val);
                else
                    Log(Error, "While parsing the file, more than two elements were defined in a line");
                counter++;
            } else {
                current++;
            }
        }
    } else {
        Log(Error, "You need to provide a valid extension like \".spd\" to read"
                   "the information from an ASCII file. You used \"%s\"", extension);
    }
}

template <typename Scalar>
void spectrum_to_file(const fs::path &path, const std::vector<Scalar> &wavelengths,
                      const std::vector<Scalar> &values) {

    auto fs = Thread::thread()->file_resolver();
    fs::path file_path = fs->resolve(path);

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
    } else {
        Log(Error, "You need to provide a valid extension like \".spd\" to store"
                   "the information in an ASCII file. You used \"%s\"", extension);
    }
}

template <typename Scalar>
Color<Scalar, 3> spectrum_list_to_srgb(const std::vector<Scalar> &wavelengths,
                                       const std::vector<Scalar> &values,
                                       bool bounded, bool d65) {
    Color<Scalar, 3> xyz = (Scalar) 0.f;

    const int steps = 1000;
    for (int i = 0; i < steps; ++i) {
        Scalar w = (Scalar) MI_CIE_MIN +
                   (i / (Scalar)(steps - 1)) * ((Scalar) MI_CIE_MAX -
                                                (Scalar) MI_CIE_MIN);

        if (w < wavelengths.front() || w > wavelengths.back())
            continue;

        // Find interval containing 'x'
        uint32_t index = math::find_interval<uint32_t>(
            (uint32_t) wavelengths.size(),
            [&](uint32_t idx) {
                return wavelengths[idx] <= w;
            });

        Scalar w0 = wavelengths[index],
               w1 = wavelengths[index + 1],
               v0 = values[index],
               v1 = values[index + 1];

        // Linear interpolant at 'w'
        Scalar v = ((w - w1) * v0 + (w0 - w) * v1) / (w0 - w1);
        xyz += cie1931_xyz(w) * v * (d65 ? cie_d65(w) : Scalar(1));
    }

    // Last specified value repeats implicitly
    xyz *= ((Scalar) MI_CIE_MAX - (Scalar) MI_CIE_MIN) / (Scalar) steps;
    Color<Scalar, 3> rgb = xyz_to_srgb(xyz);

    if (bounded && dr::any(rgb < (Scalar) 0.f || rgb > (Scalar) 1.f)) {
        Log(Warn, "Spectrum: clamping out-of-gamut color %s", rgb);
        rgb = clip(rgb, (Scalar) 0.f, (Scalar) 1.f);
    } else if (!bounded && dr::any(rgb < (Scalar) 0.f)) {
        Log(Warn, "Spectrum: clamping out-of-gamut color %s", rgb);
        rgb = dr::maximum(rgb, (Scalar) 0.f);
    }

    return rgb;
}

/// Explicit instantiations
template MI_EXPORT_LIB void spectrum_from_file(const fs::path &path,
                                               std::vector<float> &wavelengths,
                                               std::vector<float> &values);
template MI_EXPORT_LIB void spectrum_from_file(const fs::path &path,
                                               std::vector<double> &wavelengths,
                                               std::vector<double> &values);

template MI_EXPORT_LIB void spectrum_to_file(const fs::path &path,
                                             const std::vector<float> &wavelengths,
                                             const std::vector<float> &values);
template MI_EXPORT_LIB void spectrum_to_file(const fs::path &path,
                                             const std::vector<double> &wavelengths,
                                             const std::vector<double> &values);

template MI_EXPORT_LIB Color<float, 3>
spectrum_list_to_srgb(const std::vector<float> &wavelengths,
                      const std::vector<float> &values, bool bounded, bool d65);
template MI_EXPORT_LIB Color<double, 3>
spectrum_list_to_srgb(const std::vector<double> &wavelengths,
                      const std::vector<double> &values, bool bounded, bool d65);

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
