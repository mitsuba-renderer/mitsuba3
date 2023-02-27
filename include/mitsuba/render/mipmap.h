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
            Struct::Type /*componentFormat*/, // channel format
            const ref<Bitmap::ReconstructionFilter> rfilter,
            dr::WrapMode wrap_mode,
            dr::FilterMode tex_filter,
            MIPFilterType mip_filter,
            size_t channels = 3,
            Float maxAnisotropy = 20.0f,
            bool accel = true,
            ScalarFloat /*maxValue*/ = 1.0f
            ):
                m_pixelFormat(pixelFormat), 
                m_texture_filter(tex_filter),
                m_mipmap_filter(mip_filter),
                m_bc(wrap_mode),
                m_maxAnisotropy(maxAnisotropy),
                m_accel(accel),
                m_channels(channels),
                m_weightLut(NULL) {

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
        resolution.resize(m_levels);
        
        // Initialize level 0
        ScalarVector2u res = ScalarVector2u(bitmap->size());
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), channels };
        m_pyramid[0] = Texture2f(TensorXf(bitmap->data(), 3, shape), m_accel, m_accel, tex_filter, wrap_mode);

        resolution[0] = res;

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

                resolution[m_levels] = size; 

                // Test if the pyramid is built
                // FileStream* str = new FileStream(std::to_string(m_levels)+".exr", FileStream::ETruncReadWrite);
                // bitmap->write(str);

                ++m_levels;
            }            
        }

        if (m_mipmap_filter == MIPFilterType::EWA){
            // TODO: ewa weights
        }
        std::cout<<"MIPMAP BUILT SUCCESS"<<std::endl;
    }

    void rebuild_pyramid(){
        // TODO: 
        std::cout<<"Not implemented yet..."<<std::endl;
    }


    Float eval_1(const Point2f &uv, const Vector2f &d0, const Vector2f &d1, Mask active) const{
        Float out = 0;
        if (m_mipmap_filter == MIPFilterType::Nearest || m_mipmap_filter == MIPFilterType::Bilinear){
            if (m_accel)
                m_pyramid[0].eval(uv, &out, active);
            else
                m_pyramid[0].eval_nonaccel(uv, &out, active);
            return out;         
        }
        const ScalarVector2u size = resolution[0];
        Float du0 = d0.x() * size.x(), dv0 = d0.y() * size.y(),
              du1 = d1.x() * size.x(), dv1 = d1.y() * size.y();

        Float A = dv0*dv0 + dv1*dv1,
              B = -2.0f * (du0*dv0 + du1*dv1),
              C = du0*du0 + du1*du1,
              F = A*C - B*B*0.25f;

        Float root = dr::hypot(A-C, B),
              Aprime = 0.5f * (A + C - root),
              Cprime = 0.5f * (A + C + root),
              majorRadius = dr::select(dr::neq(Aprime, 0), dr::sqrt(F / Aprime), 0),
              minorRadius = dr::select(dr::neq(Cprime, 0), dr::sqrt(F / Cprime), 0);

            // If isTri, perform trilinear filter
            Mask isTri = (m_mipmap_filter == Trilinear || !(minorRadius > 0) || !(majorRadius > 0) || F < 0);

            Float level = dr::log2(dr::maximum(majorRadius, dr::Epsilon<Float>));
            Int32 lower = dr::floor2int<Int32>(level);
            Float alpha = level - lower;

            // defalt level: 0
            Mask isZero = lower < 0;

            Float c_lower = 1;
            Float c_upper = 1;
            Float c_tmp = 0;

            // For level < 0
            if (m_accel){
                m_pyramid[0].eval(uv, &out, active & isTri & isZero);
            }
            else{
                m_pyramid[0].eval_nonaccel(uv, &out, active & isTri & isZero);
            }

            // For level >= 0
            for(int i = 0; i<m_levels-1; i++){
                if (m_accel){
                    m_pyramid[i].eval(uv, &c_tmp, active);
                    dr::masked(c_lower, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;

                    m_pyramid[i+1].eval(uv, &c_tmp, active);
                    dr::masked(c_upper, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;
                }
                else{
                    m_pyramid[i].eval_nonaccel(uv, &c_tmp, active);
                    dr::masked(c_lower, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;

                    m_pyramid[i+1].eval_nonaccel(uv, &c_tmp, active);
                    dr::masked(c_upper, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;
                }
            }

            // std::cout<<out<<std::endl;
            dr::masked(out, active & isTri & !isZero)  = c_upper * alpha + c_lower * (1.0 - alpha);
            // std::cout<<out<<std::endl;
            // std::cin.get();

            // TODO: EWA
            return out;  
    }

    Color3f eval_3(const Point2f &uv, const Vector2f &d0, const Vector2f &d1, Mask active) const{
        Color3f out(0);
        if (m_mipmap_filter == MIPFilterType::Nearest || m_mipmap_filter == MIPFilterType::Bilinear){
            if (m_accel)
                m_pyramid[0].eval(uv, out.data(), active);
            else
                m_pyramid[0].eval_nonaccel(uv, out.data(), active);
            return out;         
        }
        const ScalarVector2u size = resolution[0];
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

            Float level = dr::log2(dr::maximum(dr::maximum(dr::maximum(du0, du1), dr::maximum(dv0, dv1)), dr::Epsilon<Float>));
            Int32 lower = dr::floor2int<Int32>(level);

            // For test
            Float alpha = level - lower;

            // defalt level: 0
            Mask isZero = dr::select(lower < 0, true, false);

            Color3f c_lower = 0;
            Color3f c_upper = 0;
            Color3f c_tmp = 0;

            // For level < 0
            if (m_accel){
                m_pyramid[0].eval(uv, out.data(), active & isTri & isZero);
            }
            else{
                m_pyramid[0].eval_nonaccel(uv, out.data(), active & isTri & isZero);
            }

            // For level >= 0
            for(int i = 0; i<m_levels-1; i++){
                if (m_accel){
                    m_pyramid[i].eval(uv, c_tmp.data(), active);
                    dr::masked(c_lower, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;

                    m_pyramid[i+1].eval(uv, c_tmp.data(), active);
                    dr::masked(c_upper, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;
                }
                else{
                    m_pyramid[i].eval_nonaccel(uv, c_tmp.data(), active);
                    dr::masked(c_lower, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;

                    m_pyramid[i+1].eval_nonaccel(uv, c_tmp.data(), active);
                    dr::masked(c_upper, dr::eq(i, lower) & active & isTri & !isZero) = c_tmp;
                }
            }

            dr::masked(out, active & isTri & !isZero)  = c_upper * (1.0f - alpha) + c_lower * alpha;

            // TODO: EWA
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
    std::vector<ScalarVector2u> resolution;
};

NAMESPACE_END(mitsuba)
