// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::Mesh)
    ENOKI_CALL_SUPPORT_METHOD(fill_surface_interaction)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(faces, m_faces, uint8_t*)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(vertices, m_vertices, uint8_t*)

    //ENOKI_CALL_SUPPORT(face)
    //ENOKI_CALL_SUPPORT(vertex)

    //ENOKI_CALL_SUPPORT(has_vertex_normals)
    //ENOKI_CALL_SUPPORT(vertex_position)
    //ENOKI_CALL_SUPPORT(vertex_normal)
ENOKI_CALL_SUPPORT_END(mitsuba::Mesh)

//! @}
// -----------------------------------------------------------------------
