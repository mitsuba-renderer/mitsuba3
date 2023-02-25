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

    MIPMap(Bitmap *bitmap,
            Bitmap::PixelFormat pixelFormat, // channels
            Struct::Type componentFormat, // channel format
            const ReconstructionFilter<Float, Spectrum> *rfilter,
            FilterBoundaryCondition bc,
            MIPFilterType filterType,
            Float maxAnisotropy = 20.0f,
            bool accel = true
            ):
                m_pixelFormat(pixelFormat), 
                m_filterType(filterType),
                m_bc(bc),
                m_weightLut(NULL), 
                m_maxAnisotropy(maxAnisotropy),
                m_accel(accel){

        // TODO: resize to be power of two : seems to be done by bitmap resample!!
        m_levels = 1;
        
        ScalarVector2u size = ScalarVector2u(bitmap->size());
        ScalarUInt64 pyramid_height = size.y;
        ScalarUInt64 pyramid_width = size.x;
        if (m_filterType != Nearest && m_filterType != Bilinear){
            while(size.x > 1 || size.y > 1){
                size.x = dr::max(1, (size.x + 1) / 2);
                size.y = dr::max(1, (size.y + 1) / 2);
                ++m_levels;
            }
        }

        // allocate pyramid
        m_pyramid.init_(m_levels);
        std::cout<<m_pyramid.size()<<std::endl;

        dr::FilterMode filter_mode;
        dr::WrapMode wrap_mode;
        size_t channels = bitmap->channel_count();

        ScalarVector2i res = ScalarVector2i(bitmap->size());
        size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), channels };
        m_pyramid[0] = Texture2f(TensorXf(bitmap->data(), 3, shape), m_accel, m_accel, filter_mode, wrap_mode);

        if (m_filterType != Nearest && m_filterType != Bilinear){
            Vector2i size = bitmap->size();
            m_levels = 1;
            while (size.x > 1 || size.y > 1) {
                /* Compute the size of the next downsampled layer */
                size.x = std::max(1, (size.x + 1) / 2);
                size.y = std::max(1, (size.y + 1) / 2);

                bitmap = bitmap->resample(size, rfilter, m_bc, {m_minimum, m_maximum});
                ScalarVector2i res = ScalarVector2i(bitmap->size());
                size_t shape[3] = { (size_t) res.y(), (size_t) res.x(), channels };

                m_pyramid[m_levels] = Texture2f(TensorXf(bitmap->data(), 3, shape), m_accel, m_accel, filter_mode, wrap_mode);
                ++m_levels;
            }            
        }
    }

    

private:
    Bitmap::PixelFormat m_pixelFormat;
    MIPFilterType m_filterType;
    FilterBoundaryCondition m_bc;
    Float m_maxAnisotropy;
    bool m_accel;

    dr::DynamicArray<Texture2f> m_pyramid;
    Float *m_weightLut;
    int m_levels;
    
    ScalarFloat m_minimum; // this is got from bitmap
    ScalarFloat m_maximum;
    Float m_average;
    Point2i resolution;
};


NAMESPACE_END(mitsuba)