#pragma once

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Class to read and write 3D volume grids
 *
 * This class handles loading of volumes in the Mitsuba volume file format
 * Please see the documentation of gridvolume (grid3d.cpp) for the file format
 * specification.
 */
MI_VARIANT
class MI_EXPORT_LIB VolumeGrid : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    /// Estimates the transformation from a unit axis-aligned bounding box to the given one.
    ScalarTransform4f bbox_transform() const {
        auto scale_transf = ScalarTransform4f::scale(dr::rcp(m_bbox.extents()));
        auto translation  = ScalarTransform4f::translate(-m_bbox.min);
        return scale_transf * translation;
    }

    /**
     * \brief Load a VolumeGrid from a given filename
     *
     * \param path
     *    Name of the file to be loaded
     */
    VolumeGrid(const fs::path &path);

    /**
     * \brief Load a VolumeGrid from an arbitrary stream data source
     *
     * \param stream
     *    Pointer to an arbitrary stream data source
     */
    VolumeGrid(Stream *stream);

    VolumeGrid(ScalarVector3u size, ScalarUInt32 channel_count);

    /// Return a pointer to the underlying volume storage
    ScalarFloat *data() { return m_data.get(); }

    /// Return a pointer to the underlying volume storage
    const ScalarFloat *data() const { return m_data.get(); }

    /// Return the resolution of the voxel grid
    ScalarVector3u size() const { return m_size; }

    /// Return the number of channels
    size_t channel_count() const { return m_channel_count; }

    /// Return the precomputed maximum over the volume grid
    ScalarFloat max() const { return m_max; }

    /**
     * \brief Return the precomputed maximum over the volume grid per channel
     *
     * Pointer allocation/deallocation must be performed by the caller.
     */
    void max_per_channel(ScalarFloat *out) const;

    /// Set the precomputed maximum over the volume grid
    void set_max(ScalarFloat max) { m_max = max; }

    /**
     * \brief Set the precomputed maximum over the volume grid per channel
     *
     * Pointer allocation/deallocation must be performed by the caller.
     */
    void set_max_per_channel(ScalarFloat *max) {
        for (size_t i=0; i<m_channel_count; ++i)
            m_max_per_channel[i] = max[i];
    }

    /// Return the number bytes of storage used per voxel
    size_t bytes_per_voxel() const { return sizeof(ScalarFloat) * channel_count(); }

    /// Return the volume grid size in bytes (excluding metadata)
    size_t buffer_size() const { return dr::prod(m_size) * bytes_per_voxel(); }

    /**
     * Write an encoded form of the bitmap to a binary volume file
     *
     * \param path
     *    Target file name (expected to end in ".vol")
     */
    void write(const fs::path &path) const;

    /**
     * Write an encoded form of the volume grid to a stream
     *
     * \param stream
     *    Target stream that will receive the encoded output
     */
    void write(Stream *stream) const;

    /// Return a human-readable summary of this volume grid
    virtual std::string to_string() const override;

    MI_DECLARE_CLASS()

protected:
    void read(Stream *stream);

protected:
    std::unique_ptr<ScalarFloat[]> m_data;

    ScalarVector3u m_size;
    ScalarUInt32 m_channel_count;
    ScalarBoundingBox3f m_bbox;
    ScalarFloat m_max;
    std::vector<ScalarFloat> m_max_per_channel;
};

MI_EXTERN_CLASS(VolumeGrid)
NAMESPACE_END(mitsuba)
