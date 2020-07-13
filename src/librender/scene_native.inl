NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &props) {
    ShapeKDTree *kdtree = new ShapeKDTree(props);
    kdtree->inc_ref();
    for (Shape *shape : m_shapes)
        kdtree->add_shape(shape);
    kdtree->build();
    m_accel = kdtree;
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    ((ShapeKDTree *) m_accel)->dec_ref();
    m_accel = nullptr;
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray, Mask active) const {
    const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;
    return kdtree->template ray_intersect_preliminary<false>(ray, active);
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, HitComputeFlags flags, Mask active) const {
    const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;

    PreliminaryIntersection3f pi = kdtree->template ray_intersect_preliminary<false>(ray, active);
    active &= pi.is_valid();

    SurfaceInteraction3f si;
    if (likely(any(active))) {
        ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
        si = pi.compute_surface_interaction(ray, flags, active);
    } else {
        si.wavelengths = ray.wavelengths;
        si.wi = -ray.d;
        si.t = math::Infinity<Float>;
    }

    return si;
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive_cpu(const Ray3f &ray, Mask active) const {
    const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;

    auto pi = kdtree->template ray_intersect_naive<false>(ray, active);
    active &= pi.is_valid();

    SurfaceInteraction3f si;
    if (likely(any(active))) {
        ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
        si = pi.compute_surface_interaction(ray, HitComputeFlags::All, active);
    } else {
        si.wavelengths = ray.wavelengths;
        si.wi = -ray.d;
        si.t = math::Infinity<Float>;
    }

    return si;
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, Mask active) const {
    const ShapeKDTree *kdtree = (ShapeKDTree *) m_accel;
    return kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
}

NAMESPACE_END(mitsuba)
