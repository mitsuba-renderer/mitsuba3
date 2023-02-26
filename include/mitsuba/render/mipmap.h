#pragma once

#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/timer.h>
#include <boost/filesystem/fstream.hpp>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <drjit/struct.h>


NAMESPACE_BEGIN(mitsuba)

/// Specifies the desired antialiasing filter
enum MIPFilterType {
    /// No filtering, nearest neighbor lookups
    Nearest = 0,
    /// No filtering, only bilinear interpolation
    Bilinear = 1,
    /// Basic trilinear filtering
    Trilinear = 2,
    /// Elliptically weighted average
    EWA = 3
};


template <typename Float, typename Spectrum>
class MIPMap : public Object{

public:
    MI_IMPORT_TYPES(Texture)

    MIPMap(ref<Bitmap> bitmap,
            Bitmap::PixelFormat pixelFormat, // channels
            Struct::Type componentFormat, // channel format
            const ref<Bitmap::ReconstructionFilter> rfilter,
            dr::WrapMode wrap_mode,
            dr::FilterMode tex_filter,
            MIPFilterType mip_filter,
            size_t channels = 3,
            Float maxAnisotropy = 20.0f,
            bool accel = true,
            ScalarFloat maxValue = 1.0f
            ):
                m_pixelFormat(pixelFormat), 
                m_texture_filter(tex_filter),
                m_mipmap_filter(mip_filter),
                m_bc(wrap_mode),
                m_weightLut(NULL), 
                m_maxAnisotropy(maxAnisotropy),
                m_accel(accel),
                m_channels(channels){

        // Compuate the total levles
        channels = bitmap->channel_count();
        m_levels = 1;
        if (mip_filter != MIPFilterType::Nearest && mip_filter != MIPFilterType::Bilinear){
            ScalarVector2u size = ScalarVector2u(bitmap->size());
            while(size.x() > 1 || size.y() > 1){
                size.x() = dr::maximum(1, (size.x() + 1) / 2);
                size.y() = dr::maximum(1, (size.y() + 1) / 2);
                ++m_levels;
            }
        }

        // Allocate pyramid
        m_pyramid.init_(m_levels);
        
        // Initialize level 0
        ScalarVector2u res = ScalarVector2u(bitmap->size());
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), channels };
        m_pyramid[0] = Texture2f(TensorXf(bitmap->data(), 3, shape), m_accel, m_accel, tex_filter, wrap_mode);

        // Downsample until 1x1
        if (mip_filter != MIPFilterType::Nearest && mip_filter != MIPFilterType::Bilinear){
            ScalarVector2u size = bitmap->size();
            m_levels = 1;
            while (size.x() > 1 || size.y() > 1) {
                /* Compute the size of the next downsampled layer */
                size.x() = dr::maximum(1, (size.x() + 1) / 2);
                size.y() = dr::maximum(1, (size.y() + 1) / 2);

                // Resample to be new size
                bitmap = bitmap->resample(size, rfilter);

                size_t shape[3] = { (size_t) size.y(), (size_t) size.x(), channels };
                m_pyramid[m_levels] = Texture2f(TensorXf(bitmap->data(), 3, shape), m_accel, m_accel, tex_filter, wrap_mode);

                // Test if the pyramid is built
                // FileStream* str = new FileStream(std::to_string(m_levels)+".exr", FileStream::ETruncReadWrite);
                // bitmap->write(str);

                ++m_levels;
            }            
        }

        if (m_mipmap_filter == MIPFilterType::EWA){
            // TODO: ewa weights
        }
    }

    void rebuild_pyramid(){
        // TODO: 
        std::cout<<"Not implemented yet..."<<std::endl;
    }

    Float evalTexel(int level, Point2f uv, Mask active){
        Color3f out;
        if (m_accel){
            m_pyramid[level].eval(uv, out.data(), active);
        }
        else{
            m_pyramid[level].eval_nonaccel(uv, out.data(), active);
        }
    }

    Float eval_1(const Point2f &uv, const Vector2f &d0, const Vector2f &d1, Mask active) const{
        const Vector2i &size = m_pyramid[0].tensor().size();
        Float du0 = d0.x() * size.x(), dv0 = d0.y() * size.y(),
              du1 = d1.x() * size.x(), dv1 = d1.y() * size.y();

        Float A = dv0*dv0 + dv1*dv1,
              B = -2.0f * (du0*dv0 + du1*dv1),
              C = du0*du0 + du1*du1,
              F = A*C - B*B*0.25f;

        Float root = dr::hypot(A-C, B),
              Aprime = 0.5f * (A + C - root),
              Cprime = 0.5f * (A + C + root),
              majorRadius = dr::select(Aprime != 0, dr::sqrt(F / Aprime), 0),
              minorRadius = dr::select(Cprime != 0, dr::sqrt(F / Cprime), 0);

            // If isTri, perform trilinear filter
            Mask isTri = (m_mipmap_filter == Trilinear || !(minorRadius > 0) || !(majorRadius > 0) || F < 0);

            Float level = dr::log2(dr::maximum(majorRadius, dr::Epsilon<Float>));
            Int32 ilevel = dr::floor2int<Int32>(level);

            Mask isZero = false;
            isZero = dr::select(ilevel < 0, true, false);


            /* Bilinear interpolation (lookup is smaller than 1 pixel) */
            // Float out_zero;
            // if (m_accel)
            //     dr::gather<Texture2f>(m_pyramid, Int32(0), active & isZero); //.eval(uv, &out_zero, active);
            // else
            //     dr::gather<Texture2f>(m_pyramid, Int32(0), active & isZero); //.eval_nonaccel(uv, &out_zero, active);


            // /* Trilinear interpolation between two mipmap levels */
            // Float a = level - ilevel;
            // Float out_upper;
            // if (m_accel)
            //     dr::gather<Texture2f>(m_pyramid, ilevel+1, active & !isZero); //.eval(uv, &out_upper, active);
            // else
            //     dr::gather<Texture2f>(m_pyramid, ilevel+1, active & !isZero); //.eval_nonaccel(uv, &out_upper, active);

            // Float out_lower;
            // if (m_accel)
            //     dr::gather<Texture2f>(m_pyramid, ilevel, active & !isZero); //.eval(uv, &out_lower, active);
            // else
            //     dr::gather<Texture2f>(m_pyramid, ilevel, active & !isZero); //.eval_nonaccel(uv, &out_lower, active);

            // Float out = out_upper * (1.0f - a) + out_lower * a;
            // dr::select(out, isZero, out_zero);

            Float out;
            return out;
                
    }


private:
    Bitmap::PixelFormat m_pixelFormat;
    dr::FilterMode m_texture_filter;
    MIPFilterType m_mipmap_filter;
    dr::WrapMode m_bc;
    Float m_maxAnisotropy;
    bool m_accel;
    size_t m_channels;

    dr::DynamicArray<Texture2f> m_pyramid;
    dr::DynamicArray<Float> m_weightLut;
    int m_levels;
    
    ScalarFloat m_minimum; // this is got from bitmap
    ScalarFloat m_maximum;
    Float m_average;
    Point2i resolution;
};


NAMESPACE_END(mitsuba)