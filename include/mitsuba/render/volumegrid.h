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
class MTS_EXPORT_RENDER VolumeGrid : public Object {
public:
    using Float = float;
    MTS_IMPORT_CORE_TYPES()

    /// Estimates the transformation from a unit axis-aligned bounding box to the given one.
    ScalarTransform4f bbox_transform() const {
        MTS_IMPORT_CORE_TYPES()
        ScalarVector3f d  = ek::rcp(m_bbox.max - m_bbox.min);
        auto scale_transf = ScalarTransform4f::scale(d);
        ScalarVector3f t  = -1.f * ScalarVector3f(m_bbox.min.x(), m_bbox.min.y(), m_bbox.min.z());
        auto translation  = ScalarTransform4f::translate(t);
        return scale_transf * translation;
    }

    /**
     * \brief Load a VolumeGrid from a given filename
     *
     * \param path
     *    Name of the file to be loaded
     *
     */
    VolumeGrid(const fs::path &path);

    /**
     * \brief Load a VolumeGrid from an arbitrary stream data source
     *
     * \param stream
     *    Pointer to an arbitrary stream data source
     *
     */
    VolumeGrid(Stream *stream);

    VolumeGrid(ScalarVector3i size, ScalarUInt32 channel_count);

    /// Return a pointer to the underlying volume storage
    Float *data() { return m_data.get(); }

    /// Return a pointer to the underlying volume storage
    const Float *data() const { return m_data.get(); }

    /// Return the resolution of the voxel grid
    ScalarVector3i size() const { return m_size; }

    /// Return the number of channels
    size_t channel_count() const { return m_channel_count; }

    /// Return the precomputed maximum over the volume grid
    ScalarFloat max() const { return m_max; }

    /// Set the precomputed maximum over the volume grid
    void set_max(ScalarFloat max) { m_max = max; }

    /// Return the number bytes of storage used per voxel
    size_t bytes_per_voxel() const { return sizeof(ScalarFloat) * channel_count(); }

    /// Return the volume grid size in bytes (excluding metadata)
    size_t buffer_size() const { return m_size.x() * m_size.y() * m_size.z() * bytes_per_voxel(); }

    /**
     * Write an encoded form of the bitmap to a binary volume file
     *
     * \param path
     *    Target file name (expected to end in ".vol")
     *
     */
    void write(const fs::path &path) const;

    /**
     * Write an encoded form of the volume grid to a stream
     *
     * \param stream
     *    Target stream that will receive the encoded output
     *
     */
    void write(Stream *stream) const;

    /// Return a human-readable summary of this volume grid
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    void read(Stream *stream);

protected:
    std::unique_ptr<ScalarFloat[]> m_data;

    ScalarVector3i m_size;
    ScalarUInt32 m_channel_count;
    BoundingBox3f m_bbox;
    ScalarFloat m_max;
};


NAMESPACE_END(mitsuba)
