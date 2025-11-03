/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define __EXPAND(x)                                      x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define __VA_SIZE(...)                                   __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1))
#define __CAT1(a, b)                                     a ## b
#define __CAT2(a, b)                                     __CAT1(a, b)
#define __DOC1(n1)                                       __doc_##n1
#define __DOC2(n1, n2)                                   __doc_##n1##_##n2
#define __DOC3(n1, n2, n3)                               __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4)                           __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5)                       __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6)                   __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7)               __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                         __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *__doc_EmptySbtRecord = R"doc()doc";

static const char *__doc_EmptySbtRecord_header = R"doc()doc";

static const char *__doc_OptixAccelBufferSizes = R"doc()doc";

static const char *__doc_OptixAccelBufferSizes_outputSizeInBytes = R"doc()doc";

static const char *__doc_OptixAccelBufferSizes_tempSizeInBytes = R"doc()doc";

static const char *__doc_OptixAccelBufferSizes_tempUpdateSizeInBytes = R"doc()doc";

static const char *__doc_OptixAccelBuildOptions = R"doc()doc";

static const char *__doc_OptixAccelBuildOptions_buildFlags = R"doc()doc";

static const char *__doc_OptixAccelBuildOptions_motionOptions = R"doc()doc";

static const char *__doc_OptixAccelBuildOptions_operation = R"doc()doc";

static const char *__doc_OptixAccelEmitDesc = R"doc()doc";

static const char *__doc_OptixAccelEmitDesc_result = R"doc()doc";

static const char *__doc_OptixAccelEmitDesc_type = R"doc()doc";

static const char *__doc_OptixBuildInput = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_curveType = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_endcapFlags = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_flag = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_indexBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_indexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_normalBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_normalStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_numPrimitives = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_numVertices = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_primitiveIndexOffset = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_vertexBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_vertexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_widthBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputCurveArray_widthStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_aabbBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_flags = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_numPrimitives = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_numSbtRecords = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_primitiveIndexOffset = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_sbtIndexOffsetBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_sbtIndexOffsetSizeInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_sbtIndexOffsetStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputCustomPrimitiveArray_strideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_displacementMicromapArray = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_displacementMicromapIndexBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_displacementMicromapIndexOffset = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_displacementMicromapIndexSizeInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_displacementMicromapIndexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_displacementMicromapUsageCounts = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_indexingMode = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_numDisplacementMicromapUsageCounts = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_triangleFlagsBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_triangleFlagsStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_vertexBiasAndScaleBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_vertexBiasAndScaleFormat = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_vertexBiasAndScaleStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_vertexDirectionFormat = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_vertexDirectionStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputDisplacementMicromap_vertexDirectionsBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputInstanceArray = R"doc()doc";

static const char *__doc_OptixBuildInputInstanceArray_instanceStride = R"doc()doc";

static const char *__doc_OptixBuildInputInstanceArray_instances = R"doc()doc";

static const char *__doc_OptixBuildInputInstanceArray_numInstances = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_indexBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_indexOffset = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_indexSizeInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_indexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_indexingMode = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_micromapUsageCounts = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_numMicromapUsageCounts = R"doc()doc";

static const char *__doc_OptixBuildInputOpacityMicromap_opacityMicromapArray = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_flags = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_numSbtRecords = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_numVertices = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_primitiveIndexOffset = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_radiusBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_radiusStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_sbtIndexOffsetBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_sbtIndexOffsetSizeInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_sbtIndexOffsetStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_singleRadius = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_vertexBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputSphereArray_vertexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_displacementMicromap = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_flags = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_indexBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_indexFormat = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_indexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_numIndexTriplets = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_numSbtRecords = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_numVertices = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_opacityMicromap = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_preTransform = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_primitiveIndexOffset = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_sbtIndexOffsetBuffer = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_sbtIndexOffsetSizeInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_sbtIndexOffsetStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_transformFormat = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_vertexBuffers = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_vertexFormat = R"doc()doc";

static const char *__doc_OptixBuildInputTriangleArray_vertexStrideInBytes = R"doc()doc";

static const char *__doc_OptixBuildInput_type = R"doc()doc";

static const char *__doc_OptixBuiltinISOptions = R"doc()doc";

static const char *__doc_OptixBuiltinISOptions_buildFlags = R"doc()doc";

static const char *__doc_OptixBuiltinISOptions_builtinISModuleType = R"doc()doc";

static const char *__doc_OptixBuiltinISOptions_curveEndcapFlags = R"doc()doc";

static const char *__doc_OptixBuiltinISOptions_usesMotionBlur = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType_OPTIX_DENOISER_AOV_TYPE_BEAUTY = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType_OPTIX_DENOISER_AOV_TYPE_DIFFUSE = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType_OPTIX_DENOISER_AOV_TYPE_NONE = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType_OPTIX_DENOISER_AOV_TYPE_REFLECTION = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType_OPTIX_DENOISER_AOV_TYPE_REFRACTION = R"doc()doc";

static const char *__doc_OptixDenoiserAOVType_OPTIX_DENOISER_AOV_TYPE_SPECULAR = R"doc()doc";

static const char *__doc_OptixDenoiserAlphaMode = R"doc()doc";

static const char *__doc_OptixDenoiserAlphaMode_OPTIX_DENOISER_ALPHA_MODE_COPY = R"doc()doc";

static const char *__doc_OptixDenoiserAlphaMode_OPTIX_DENOISER_ALPHA_MODE_DENOISE = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer_albedo = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer_flow = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer_flowTrustworthiness = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer_normal = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer_outputInternalGuideLayer = R"doc()doc";

static const char *__doc_OptixDenoiserGuideLayer_previousOutputInternalGuideLayer = R"doc()doc";

static const char *__doc_OptixDenoiserLayer = R"doc()doc";

static const char *__doc_OptixDenoiserLayer_input = R"doc()doc";

static const char *__doc_OptixDenoiserLayer_output = R"doc()doc";

static const char *__doc_OptixDenoiserLayer_previousOutput = R"doc()doc";

static const char *__doc_OptixDenoiserLayer_type = R"doc()doc";

static const char *__doc_OptixDenoiserModelKind = R"doc()doc";

static const char *__doc_OptixDenoiserModelKind_OPTIX_DENOISER_MODEL_KIND_HDR = R"doc()doc";

static const char *__doc_OptixDenoiserModelKind_OPTIX_DENOISER_MODEL_KIND_TEMPORAL = R"doc()doc";

static const char *__doc_OptixDenoiserOptions = R"doc()doc";

static const char *__doc_OptixDenoiserOptions_denoiseAlpha = R"doc()doc";

static const char *__doc_OptixDenoiserOptions_guideAlbedo = R"doc()doc";

static const char *__doc_OptixDenoiserOptions_guideNormal = R"doc()doc";

static const char *__doc_OptixDenoiserParams = R"doc()doc";

static const char *__doc_OptixDenoiserParams_blendFactor = R"doc()doc";

static const char *__doc_OptixDenoiserParams_hdrAverageColor = R"doc()doc";

static const char *__doc_OptixDenoiserParams_hdrIntensity = R"doc()doc";

static const char *__doc_OptixDenoiserParams_temporalModeUsePreviousLayers = R"doc()doc";

static const char *__doc_OptixDenoiserSizes = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_computeAverageColorSizeInBytes = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_computeIntensitySizeInBytes = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_internalGuideLayerPixelSizeInBytes = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_overlapWindowSizeInPixels = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_stateSizeInBytes = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_withOverlapScratchSizeInBytes = R"doc()doc";

static const char *__doc_OptixDenoiserSizes_withoutOverlapScratchSizeInBytes = R"doc()doc";

static const char *__doc_OptixDisplacementMicromapUsageCount = R"doc()doc";

static const char *__doc_OptixDisplacementMicromapUsageCount_count = R"doc()doc";

static const char *__doc_OptixDisplacementMicromapUsageCount_format = R"doc()doc";

static const char *__doc_OptixDisplacementMicromapUsageCount_subdivisionLevel = R"doc()doc";

static const char *__doc_OptixHitGroupData = R"doc(Stores information about a Shape on the Optix side)doc";

static const char *__doc_OptixHitGroupData_data = R"doc(Pointer to the memory region of Shape data (e.g. ``OptixSphereData`` ))doc";

static const char *__doc_OptixHitGroupData_shape_registry_id = R"doc(Shape id in Dr.Jit's pointer registry)doc";

static const char *__doc_OptixImage2D = R"doc()doc";

static const char *__doc_OptixImage2D_data = R"doc()doc";

static const char *__doc_OptixImage2D_format = R"doc()doc";

static const char *__doc_OptixImage2D_height = R"doc()doc";

static const char *__doc_OptixImage2D_pixelStrideInBytes = R"doc()doc";

static const char *__doc_OptixImage2D_rowStrideInBytes = R"doc()doc";

static const char *__doc_OptixImage2D_width = R"doc()doc";

static const char *__doc_OptixInstance = R"doc()doc";

static const char *__doc_OptixInstance_flags = R"doc()doc";

static const char *__doc_OptixInstance_instanceId = R"doc()doc";

static const char *__doc_OptixInstance_pad = R"doc()doc";

static const char *__doc_OptixInstance_sbtOffset = R"doc()doc";

static const char *__doc_OptixInstance_transform = R"doc()doc";

static const char *__doc_OptixInstance_traversableHandle = R"doc()doc";

static const char *__doc_OptixInstance_visibilityMask = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_boundValues = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_debugLevel = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_maxRegisterCount = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_numBoundValues = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_numPayloadTypes = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_optLevel = R"doc()doc";

static const char *__doc_OptixModuleCompileOptions_payloadTypes = R"doc()doc";

static const char *__doc_OptixMotionOptions = R"doc()doc";

static const char *__doc_OptixMotionOptions_flags = R"doc()doc";

static const char *__doc_OptixMotionOptions_numKeys = R"doc()doc";

static const char *__doc_OptixMotionOptions_timeBegin = R"doc()doc";

static const char *__doc_OptixMotionOptions_timeEnd = R"doc()doc";

static const char *__doc_OptixOpacityMicromapUsageCount = R"doc()doc";

static const char *__doc_OptixOpacityMicromapUsageCount_count = R"doc()doc";

static const char *__doc_OptixOpacityMicromapUsageCount_format = R"doc()doc";

static const char *__doc_OptixOpacityMicromapUsageCount_subdivisionLevel = R"doc()doc";

static const char *__doc_OptixPayloadType = R"doc()doc";

static const char *__doc_OptixPayloadType_numPayloadValues = R"doc()doc";

static const char *__doc_OptixPayloadType_payloadSemantics = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_allowClusteredGeometry = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_allowOpacityMicromaps = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_exceptionFlags = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_numAttributeValues = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_numPayloadValues = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_pipelineLaunchParamsVariableName = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_traversableGraphFlags = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_usesMotionBlur = R"doc()doc";

static const char *__doc_OptixPipelineCompileOptions_usesPrimitiveTypeFlags = R"doc()doc";

static const char *__doc_OptixPixelFormat = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_FLOAT2 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_FLOAT3 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_FLOAT4 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_HALF2 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_HALF3 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_HALF4 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_UCHAR3 = R"doc()doc";

static const char *__doc_OptixPixelFormat_OPTIX_PIXEL_FORMAT_UCHAR4 = R"doc()doc";

static const char *__doc_OptixProgramGroupCallables = R"doc()doc";

static const char *__doc_OptixProgramGroupCallables_entryFunctionNameCC = R"doc()doc";

static const char *__doc_OptixProgramGroupCallables_entryFunctionNameDC = R"doc()doc";

static const char *__doc_OptixProgramGroupCallables_moduleCC = R"doc()doc";

static const char *__doc_OptixProgramGroupCallables_moduleDC = R"doc()doc";

static const char *__doc_OptixProgramGroupDesc = R"doc()doc";

static const char *__doc_OptixProgramGroupDesc_flags = R"doc()doc";

static const char *__doc_OptixProgramGroupDesc_kind = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup_entryFunctionNameAH = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup_entryFunctionNameCH = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup_entryFunctionNameIS = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup_moduleAH = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup_moduleCH = R"doc()doc";

static const char *__doc_OptixProgramGroupHitgroup_moduleIS = R"doc()doc";

static const char *__doc_OptixProgramGroupOptions = R"doc()doc";

static const char *__doc_OptixProgramGroupOptions_payloadType = R"doc()doc";

static const char *__doc_OptixProgramGroupSingleModule = R"doc()doc";

static const char *__doc_OptixProgramGroupSingleModule_entryFunctionName = R"doc()doc";

static const char *__doc_OptixProgramGroupSingleModule_module = R"doc()doc";

static const char *__doc_OptixShaderBindingTable = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_callablesRecordBase = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_callablesRecordCount = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_callablesRecordStrideInBytes = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_exceptionRecord = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_hitgroupRecordBase = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_hitgroupRecordCount = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_hitgroupRecordStrideInBytes = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_missRecordBase = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_missRecordCount = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_missRecordStrideInBytes = R"doc()doc";

static const char *__doc_OptixShaderBindingTable_raygenRecord = R"doc()doc";

static const char *__doc_SbtRecord = R"doc()doc";

static const char *__doc_SbtRecord_SbtRecord = R"doc()doc";

static const char *__doc_SbtRecord_data = R"doc()doc";

static const char *__doc_SbtRecord_header = R"doc()doc";

static const char *__doc_drjit_operator_lshift = R"doc(Prints the canonical representation of a PCG32 object.)doc";

static const char *__doc_mitsuba_AdjointIntegrator =
R"doc(Abstract adjoint integrator that performs Monte Carlo sampling
starting from the emitters.

Subclasses of this interface must implement the sample() method, which
performs recursive Monte Carlo integration starting from an emitter
and directly accumulates the product of radiance and importance into
the film. The render() method then repeatedly invokes this estimator
to compute the rendered image.

Remark:
    The adjoint integrator does not support renderings with arbitrary
    output variables (AOVs).)doc";

static const char *__doc_mitsuba_AdjointIntegrator_2 = R"doc()doc";

static const char *__doc_mitsuba_AdjointIntegrator_3 = R"doc()doc";

static const char *__doc_mitsuba_AdjointIntegrator_4 = R"doc()doc";

static const char *__doc_mitsuba_AdjointIntegrator_5 = R"doc()doc";

static const char *__doc_mitsuba_AdjointIntegrator_6 = R"doc()doc";

static const char *__doc_mitsuba_AdjointIntegrator_AdjointIntegrator = R"doc(Create an integrator)doc";

static const char *__doc_mitsuba_AdjointIntegrator_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_AdjointIntegrator_m_max_depth =
R"doc(Longest visualized path depth (\c -1 = infinite). A value of ``1``
will visualize only directly visible light sources. ``2`` will lead to
single-bounce (direct-only) illumination, and so on.)doc";

static const char *__doc_mitsuba_AdjointIntegrator_m_rr_depth = R"doc(Depth to begin using russian roulette)doc";

static const char *__doc_mitsuba_AdjointIntegrator_m_samples_per_pass =
R"doc(Number of samples to compute for each pass over the image blocks.

Must be a multiple of the total sample count per pixel. If set to
(size_t) -1, all the work is done in a single pass (default).)doc";

static const char *__doc_mitsuba_AdjointIntegrator_render = R"doc(//! @{ \name Integrator interface implementation)doc";

static const char *__doc_mitsuba_AdjointIntegrator_sample =
R"doc(Sample the incident importance and splat the product of importance and
radiance to the film.

Parameter ``scene``:
    The underlying scene

Parameter ``sensor``:
    A sensor from which rays should be sampled

Parameter ``sampler``:
    A source of (pseudo-/quasi-) random numbers

Parameter ``block``:
    An image block that will be updated during the sampling process

Parameter ``sample_scale``:
    A scale factor that must be applied to each sample to account for
    the film resolution and number of samples.)doc";

static const char *__doc_mitsuba_AdjointIntegrator_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_AdjointIntegrator_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Any =
R"doc(Type-erased storage for arbitrary objects

This class resembles ``std::any`` but supports advanced customization
by exposing the underlying type-erased storage implementation
Any::Base. The Mitsuba Property class uses Any when it needs to store
things that aren't part of the supported set of property types, such
as Dr.Jit tensor objects or other specialized types. The main reason
for using Any (as opposed to the builtin ``std::any``) is to enable
seamless use in Python bindings, which requires access to Any::Base
(see ``src/core/python/properties.cpp`` for what this entails).

Instances of this type are copyable with reference semantics. The
class is not thread-safe (i.e., it may not be copied concurrently).)doc";

static const char *__doc_mitsuba_Any_Any = R"doc()doc";

static const char *__doc_mitsuba_Any_Any_2 = R"doc()doc";

static const char *__doc_mitsuba_Any_Any_3 = R"doc()doc";

static const char *__doc_mitsuba_Any_Any_4 = R"doc()doc";

static const char *__doc_mitsuba_Any_Any_5 = R"doc()doc";

static const char *__doc_mitsuba_Any_Base = R"doc()doc";

static const char *__doc_mitsuba_Any_Base_dec_ref = R"doc()doc";

static const char *__doc_mitsuba_Any_Base_inc_ref = R"doc()doc";

static const char *__doc_mitsuba_Any_Base_ptr = R"doc()doc";

static const char *__doc_mitsuba_Any_Base_ref_count = R"doc()doc";

static const char *__doc_mitsuba_Any_Base_type = R"doc()doc";

static const char *__doc_mitsuba_Any_Storage = R"doc()doc";

static const char *__doc_mitsuba_Any_Storage_Storage = R"doc()doc";

static const char *__doc_mitsuba_Any_Storage_ptr = R"doc()doc";

static const char *__doc_mitsuba_Any_Storage_type = R"doc()doc";

static const char *__doc_mitsuba_Any_Storage_value = R"doc()doc";

static const char *__doc_mitsuba_Any_data = R"doc()doc";

static const char *__doc_mitsuba_Any_data_2 = R"doc()doc";

static const char *__doc_mitsuba_Any_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Any_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Any_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_Any_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_Any_p = R"doc()doc";

static const char *__doc_mitsuba_Any_release = R"doc()doc";

static const char *__doc_mitsuba_Any_type = R"doc()doc";

static const char *__doc_mitsuba_Appender =
R"doc(This class defines an abstract destination for logging-relevant
information)doc";

static const char *__doc_mitsuba_Appender_append = R"doc(Append a line of text with the given log level)doc";

static const char *__doc_mitsuba_Appender_class_name = R"doc()doc";

static const char *__doc_mitsuba_Appender_log_progress =
R"doc(Process a progress message

Parameter ``progress``:
    Percentage value in [0, 100]

Parameter ``name``:
    Title of the progress message

Parameter ``formatted``:
    Formatted string representation of the message

Parameter ``eta``:
    Estimated time until 100% is reached.

Parameter ``ptr``:
    Custom pointer payload. This is used to express the context of a
    progress message.)doc";

static const char *__doc_mitsuba_ArgParser =
R"doc(Minimal command line argument parser

This class provides a minimal cross-platform command line argument
parser in the spirit of to GNU getopt. Both short and long arguments
that accept an optional extra value are supported.

The typical usage is

```
ArgParser p;
auto arg0 = p.register("--myParameter");
auto arg1 = p.register("-f", true);
p.parse(argc, argv);
if (*arg0)
    std::cout << "Got --myParameter" << std::endl;
if (*arg1)
    std::cout << "Got -f " << arg1->value() << std::endl;
```)doc";

static const char *__doc_mitsuba_ArgParser_Arg = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_Arg =
R"doc(Construct a new argument with the given prefixes

Parameter ``prefixes``:
    A list of command prefixes (i.e. {"-f", "--fast"})

Parameter ``extra``:
    Indicates whether the argument accepts an extra argument value)doc";

static const char *__doc_mitsuba_ArgParser_Arg_Arg_2 = R"doc(Copy constructor (does not copy argument values))doc";

static const char *__doc_mitsuba_ArgParser_Arg_append = R"doc(Append a argument value at the end)doc";

static const char *__doc_mitsuba_ArgParser_Arg_as_float = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_as_int = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_as_string = R"doc(Return the extra argument associated with this argument)doc";

static const char *__doc_mitsuba_ArgParser_Arg_count = R"doc(Specifies how many times the argument has been specified)doc";

static const char *__doc_mitsuba_ArgParser_Arg_extra = R"doc(Specifies whether the argument takes an extra value)doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_extra = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_next = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_prefixes = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_present = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_m_value = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_Arg_next =
R"doc(For arguments that are specified multiple times, advance to the next
one.)doc";

static const char *__doc_mitsuba_ArgParser_Arg_operator_bool = R"doc(Returns whether the argument has been specified)doc";

static const char *__doc_mitsuba_ArgParser_add =
R"doc(Register a new argument with the given prefix

Parameter ``prefix``:
    A single command prefix (i.e. "-f")

Parameter ``extra``:
    Indicates whether the argument accepts an extra argument value)doc";

static const char *__doc_mitsuba_ArgParser_add_2 =
R"doc(Register a new argument with the given list of prefixes

Parameter ``prefixes``:
    A list of command prefixes (i.e. {"-f", "--fast"})

Parameter ``extra``:
    Indicates whether the argument accepts an extra argument value)doc";

static const char *__doc_mitsuba_ArgParser_executable_name = R"doc(Return the name of the invoked application executable)doc";

static const char *__doc_mitsuba_ArgParser_m_args = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_m_executable_name = R"doc()doc";

static const char *__doc_mitsuba_ArgParser_parse = R"doc(Parse the given set of command line arguments)doc";

static const char *__doc_mitsuba_ArgParser_parse_2 = R"doc(Parse the given set of command line arguments)doc";

static const char *__doc_mitsuba_BSDF =
R"doc(Bidirectional Scattering Distribution Function (BSDF) interface

This class provides an abstract interface to all %BSDF plugins in
Mitsuba. It exposes functions for evaluating and sampling the model,
and for querying associated probability densities.

By default, functions in class sample and evaluate the complete BSDF,
but it also allows to pick and choose individual components of multi-
lobed BSDFs based on their properties and component indices. This
selection is specified using a context data structure that is provided
along with every operation.

When polarization is enabled, BSDF sampling and evaluation returns 4x4
Mueller matrices that describe how scattering changes the polarization
state of incident light. Mueller matrices (e.g. for mirrors) are
expressed with respect to a reference coordinate system for the
incident and outgoing direction. The convention used here is that
these coordinate systems are given by ``coordinate_system(wi)`` and
``coordinate_system(wo)``, where 'wi' and 'wo' are the incident and
outgoing direction in local coordinates.

See also:
    mitsuba.BSDFContext

See also:
    mitsuba.BSDFSample3f)doc";

static const char *__doc_mitsuba_BSDF_2 = R"doc()doc";

static const char *__doc_mitsuba_BSDF_3 = R"doc()doc";

static const char *__doc_mitsuba_BSDF_4 = R"doc()doc";

static const char *__doc_mitsuba_BSDF_5 = R"doc()doc";

static const char *__doc_mitsuba_BSDF_6 = R"doc()doc";

static const char *__doc_mitsuba_BSDFContext =
R"doc(Context data structure for BSDF evaluation and sampling

BSDF models in Mitsuba can be queried and sampled using a variety of
different modes -- for instance, a rendering algorithm can indicate
whether radiance or importance is being transported, and it can also
restrict evaluation and sampling to a subset of lobes in a multi-lobe
BSDF model.

The BSDFContext data structure encodes these preferences and is
supplied to most BSDF methods.)doc";

static const char *__doc_mitsuba_BSDFContext_BSDFContext = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFContext_BSDFContext_2 = R"doc()doc";

static const char *__doc_mitsuba_BSDFContext_component =
R"doc(Integer value of requested BSDF component index to be
sampled/evaluated.)doc";

static const char *__doc_mitsuba_BSDFContext_is_enabled =
R"doc(Checks whether a given BSDF component type and index are enabled in
this context.)doc";

static const char *__doc_mitsuba_BSDFContext_mode = R"doc(Transported mode (radiance or importance))doc";

static const char *__doc_mitsuba_BSDFContext_reverse =
R"doc(Reverse the direction of light transport in the record

This updates the transport mode (radiance to importance and vice
versa).)doc";

static const char *__doc_mitsuba_BSDFContext_type_mask = R"doc()doc";

static const char *__doc_mitsuba_BSDFFlags =
R"doc(This list of flags is used to classify the different types of lobes
that are implemented in a BSDF instance.

They are also useful for picking out individual components, e.g., by
setting combinations in BSDFContext::type_mask.)doc";

static const char *__doc_mitsuba_BSDFFlags_All = R"doc(Any kind of scattering)doc";

static const char *__doc_mitsuba_BSDFFlags_Anisotropic = R"doc(The lobe is not invariant to rotation around the normal)doc";

static const char *__doc_mitsuba_BSDFFlags_BackSide = R"doc(Supports interactions on the back-facing side)doc";

static const char *__doc_mitsuba_BSDFFlags_Delta = R"doc(Scattering into a discrete set of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_Delta1D = R"doc(Scattering into a 1D space of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_Delta1DReflection = R"doc(Reflection into a 1D space of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_Delta1DTransmission = R"doc(Transmission into a 1D space of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_DeltaReflection = R"doc(Reflection into a discrete set of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_DeltaTransmission = R"doc(Transmission into a discrete set of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_Diffuse = R"doc(Diffuse scattering into a 2D set of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_DiffuseReflection = R"doc(Ideally diffuse reflection)doc";

static const char *__doc_mitsuba_BSDFFlags_DiffuseTransmission = R"doc(Ideally diffuse transmission)doc";

static const char *__doc_mitsuba_BSDFFlags_Empty = R"doc(No flags set (default value))doc";

static const char *__doc_mitsuba_BSDFFlags_FrontSide = R"doc(Supports interactions on the front-facing side)doc";

static const char *__doc_mitsuba_BSDFFlags_Glossy = R"doc(Non-diffuse scattering into a 2D set of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_GlossyReflection = R"doc(Glossy reflection)doc";

static const char *__doc_mitsuba_BSDFFlags_GlossyTransmission = R"doc(Glossy transmission)doc";

static const char *__doc_mitsuba_BSDFFlags_NeedsDifferentials = R"doc(Does the implementation require access to texture-space differentials)doc";

static const char *__doc_mitsuba_BSDFFlags_NonSymmetric = R"doc(Flags non-symmetry (e.g. transmission in dielectric materials))doc";

static const char *__doc_mitsuba_BSDFFlags_Null = R"doc('null' scattering event, i.e. particles do not undergo deflection)doc";

static const char *__doc_mitsuba_BSDFFlags_Reflection =
R"doc(Any reflection component (scattering into discrete, 1D, or 2D set of
directions))doc";

static const char *__doc_mitsuba_BSDFFlags_Smooth = R"doc(Scattering into a 2D set of directions)doc";

static const char *__doc_mitsuba_BSDFFlags_SpatiallyVarying = R"doc(The BSDF depends on the UV coordinates)doc";

static const char *__doc_mitsuba_BSDFFlags_Transmission =
R"doc(Any transmission component (scattering into discrete, 1D, or 2D set of
directions))doc";

static const char *__doc_mitsuba_BSDFSample3 = R"doc(Data structure holding the result of BSDF sampling operations.)doc";

static const char *__doc_mitsuba_BSDFSample3_BSDFSample3 =
R"doc(Given a surface interaction and an incident/exitant direction pair
(wi, wo), create a query record to evaluate the BSDF or its sampling
density.

By default, all components will be sampled regardless of what measure
they live on.

Parameter ``wo``:
    An outgoing direction in local coordinates. This should be a
    normalized direction vector that points *away* from the scattering
    event.)doc";

static const char *__doc_mitsuba_BSDFSample3_BSDFSample3_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_BSDFSample3_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_BSDFSample3_4 = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_eta = R"doc(Relative index of refraction in the sampled direction)doc";

static const char *__doc_mitsuba_BSDFSample3_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDFSample3_pdf = R"doc(Probability density at the sample)doc";

static const char *__doc_mitsuba_BSDFSample3_sampled_component = R"doc(Stores the component index that was sampled by BSDF::sample())doc";

static const char *__doc_mitsuba_BSDFSample3_sampled_type = R"doc(Stores the component type that was sampled by BSDF::sample())doc";

static const char *__doc_mitsuba_BSDFSample3_wo = R"doc(Normalized outgoing direction in local coordinates)doc";

static const char *__doc_mitsuba_BSDF_BSDF = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDF_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDF_component_count = R"doc(Number of components this BSDF is comprised of.)doc";

static const char *__doc_mitsuba_BSDF_eval =
R"doc(Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo) and
multiply by the cosine foreshortening term.

Based on the information in the supplied query context ``ctx``, this
method will either evaluate the entire BSDF or query individual
components (e.g. the diffuse lobe). Only smooth (i.e. non-Dirac-delta)
components are supported: calling ``eval()`` on a perfectly specular
material will return zero.

Note that the incident direction does not need to be explicitly
specified. It is obtained from the field ``si.wi``.

Parameter ``ctx``:
    A context data structure describing which lobes to evaluate, and
    whether radiance or importance are being transported.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position. The incident direction is obtained from the
    field ``si.wi``.

Parameter ``wo``:
    The outgoing direction)doc";

static const char *__doc_mitsuba_BSDF_eval_attribute =
R"doc(Evaluate a specific BSDF attribute at the given surface interaction.

BSDF attributes are user-provided fields that provide extra
information at an intersection. An example of this would be a per-
vertex or per-face color on a triangle mesh.

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    An unpolarized spectral power distribution or reflectance value)doc";

static const char *__doc_mitsuba_BSDF_eval_attribute_1 =
R"doc(Monochromatic evaluation of a BSDF attribute at the given surface
interaction

This function differs from eval_attribute() in that it provided raw
access to scalar intensity/reflectance values without any color
processing (e.g. spectral upsampling).

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    An scalar intensity or reflectance value)doc";

static const char *__doc_mitsuba_BSDF_eval_attribute_3 =
R"doc(Trichromatic evaluation of a BSDF attribute at the given surface
interaction

This function differs from eval_attribute() in that it provided raw
access to RGB intensity/reflectance values without any additional
color processing (e.g. RGB-to-spectral upsampling).

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    An trichromatic intensity or reflectance value)doc";

static const char *__doc_mitsuba_BSDF_eval_diffuse_reflectance =
R"doc(Evaluate the diffuse reflectance

This method approximates the total diffuse reflectance for a given
direction. For some materials, an exact value can be computed
inexpensively. When this is not possible, the value is approximated by
evaluating the BSDF for a normal outgoing direction and returning this
value multiplied by pi. This is the default behaviour of this method.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position.)doc";

static const char *__doc_mitsuba_BSDF_eval_null_transmission =
R"doc(Evaluate un-scattered transmission component of the BSDF

This method will evaluate the un-scattered transmission
(BSDFFlags::Null) of the BSDF for light arriving from direction ``w``.
The default implementation returns zero.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position. The incident direction is obtained from the
    field ``si.wi``.)doc";

static const char *__doc_mitsuba_BSDF_eval_pdf =
R"doc(Jointly evaluate the BSDF f(wi, wo) and the probability per unit solid
angle of sampling the given direction. The result from the evaluated
BSDF is multiplied by the cosine foreshortening term.

Based on the information in the supplied query context ``ctx``, this
method will either evaluate the entire BSDF or query individual
components (e.g. the diffuse lobe). Only smooth (i.e. non-Dirac-delta)
components are supported: calling ``eval()`` on a perfectly specular
material will return zero.

This method provides access to the probability density that would
result when supplying the same BSDF context and surface interaction
data structures to the sample() method. It correctly handles changes
in probability when only a subset of the components is chosen for
sampling (this can be done using the BSDFContext::component and
BSDFContext::type_mask fields).

Note that the incident direction does not need to be explicitly
specified. It is obtained from the field ``si.wi``.

Parameter ``ctx``:
    A context data structure describing which lobes to evaluate, and
    whether radiance or importance are being transported.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position. The incident direction is obtained from the
    field ``si.wi``.

Parameter ``wo``:
    The outgoing direction)doc";

static const char *__doc_mitsuba_BSDF_eval_pdf_sample =
R"doc(Jointly evaluate the BSDF f(wi, wo), the probability per unit solid
angle of sampling the given direction ``wo`` and importance sample the
BSDF model.

This is simply a wrapper around two separate function calls to
eval_pdf() and sample(). This function exists to reduce the number of
virtual function calls, which has some performance benefits on highly
vectorized JIT variants of the renderer. (A ~20% performance
improvement for the basic path tracer on CUDA)

Parameter ``ctx``:
    A context data structure describing which lobes to evaluate, and
    whether radiance or importance are being transported.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position. The incident direction is obtained from the
    field ``si.wi``.

Parameter ``wo``:
    The outgoing direction

Parameter ``sample1``:
    A uniformly distributed sample on :math:`[0,1]`. It is used to
    select the BSDF lobe in multi-lobe models.

Parameter ``sample2``:
    A uniformly distributed sample on :math:`[0,1]^2`. It is used to
    generate the sampled direction.)doc";

static const char *__doc_mitsuba_BSDF_flags = R"doc(Flags for all components combined.)doc";

static const char *__doc_mitsuba_BSDF_flags_2 = R"doc(Flags for a specific component of this BSDF.)doc";

static const char *__doc_mitsuba_BSDF_has_attribute =
R"doc(Returns whether this BSDF contains the specified attribute.

Parameter ``name``:
    Name of the attribute)doc";

static const char *__doc_mitsuba_BSDF_m_components = R"doc(Flags for each component of this BSDF.)doc";

static const char *__doc_mitsuba_BSDF_m_flags = R"doc(Combined flags for all components of this BSDF.)doc";

static const char *__doc_mitsuba_BSDF_needs_differentials = R"doc(Does the implementation require access to texture-space differentials?)doc";

static const char *__doc_mitsuba_BSDF_pdf =
R"doc(Compute the probability per unit solid angle of sampling a given
direction

This method provides access to the probability density that would
result when supplying the same BSDF context and surface interaction
data structures to the sample() method. It correctly handles changes
in probability when only a subset of the components is chosen for
sampling (this can be done using the BSDFContext::component and
BSDFContext::type_mask fields).

Note that the incident direction does not need to be explicitly
specified. It is obtained from the field ``si.wi``.

Parameter ``ctx``:
    A context data structure describing which lobes to evaluate, and
    whether radiance or importance are being transported.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position. The incident direction is obtained from the
    field ``si.wi``.

Parameter ``wo``:
    The outgoing direction)doc";

static const char *__doc_mitsuba_BSDF_sample =
R"doc(Importance sample the BSDF model

The function returns a sample data structure along with the importance
weight, which is the value of the BSDF divided by the probability
density, and multiplied by the cosine foreshortening factor (if needed
--- it is omitted for degenerate BSDFs like smooth
mirrors/dielectrics).

If the supplied context data structures selects subset of components
in a multi-lobe BRDF model, the sampling is restricted to this subset.
Depending on the provided transport type, either the BSDF or its
adjoint version is sampled.

When sampling a continuous/non-delta component, this method also
multiplies by the cosine foreshortening factor with respect to the
sampled direction.

Parameter ``ctx``:
    A context data structure describing which lobes to sample, and
    whether radiance or importance are being transported.

Parameter ``si``:
    A surface interaction data structure describing the underlying
    surface position. The incident direction is obtained from the
    field ``si.wi``.

Parameter ``sample1``:
    A uniformly distributed sample on :math:`[0,1]`. It is used to
    select the BSDF lobe in multi-lobe models.

Parameter ``sample2``:
    A uniformly distributed sample on :math:`[0,1]^2`. It is used to
    generate the sampled direction.

Returns:
    A pair (bs, value) consisting of

bs: Sampling record, indicating the sampled direction, PDF values and
other information. The contents are undefined if sampling failed.

value: The BSDF value divided by the probability (multiplied by the
cosine foreshortening factor when a non-delta component is sampled). A
zero spectrum indicates that sampling failed.)doc";

static const char *__doc_mitsuba_BSDF_sh_frame =
R"doc(Returns the shading frame accounting for any pertubations that may
performed by the BSDF during evaluation.

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    The perturbed shading frame. By default simply returns the surface
    interaction shading frame.)doc";

static const char *__doc_mitsuba_BSDF_to_string = R"doc(Return a human-readable representation of the BSDF)doc";

static const char *__doc_mitsuba_BSDF_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_BSDF_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_BSDF_type = R"doc(//! @})doc";

static const char *__doc_mitsuba_BSDF_variant_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_BaseSunskyEmitter = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_BaseSunskyEmitter = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_bbox =
R"doc(This emitter does not occupy any particular region of space, return an
invalid bounding box)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_bilinear_interp =
R"doc(Interpolates the given tensor on turbidity then albedo

Parameter ``dataset``:
    The original dataset from file

Parameter ``albedo``:
    The albedo values for each channels

Parameter ``turbidity``:
    The turbidity value

Returns:
    The collapsed dataset interpolated linearly)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_compute_pdfs =
R"doc(Computes the PDFs of the sky and sun for the given local direction

Parameter ``local_dir``:
    Local direction in the sky

Parameter ``sun_angles``:
    Azimuth and elevation angles of the sun

Parameter ``hit_sun``:
    Indicates if the sun's intersection should be tested

Parameter ``active``:
    Mask for the active lanes

Returns:
    The sky and sun PDFs)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_compute_sun_ld =
R"doc(Computes the limb darkening of the sun for a given gamma.

Only works for spectral mode since limb darkening is baked into the
RGB model

Template parameter ``Spec``:
    Spectral type to render (adapts the number of channels)

Parameter ``channel_idx_low``:
    Indices of the lower wavelengths

Parameter ``channel_idx_high``:
    Indices of the upper wavelengths

Parameter ``lerp_f``:
    Linear interpolation factor for wavelength

Parameter ``gamma``:
    Angle between the sun's center and the viewing ray

Parameter ``active``:
    Indicates if the channel indices are valid and that the sun was
    hit

Returns:
    The spectral values of limb darkening to apply to the sun's
    radiance by multiplication)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_eval = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_eval_2 =
R"doc(This method should be used when the decision of hitting the sun was
already made. This avoids transforming coordinates to world space and
making that decision a second time since numerical precision can cause
discrepencies between the two decisions.

Parameter ``local_wo``:
    Shading direction in local space

Parameter ``wavelengths``:
    Wavelengths to evaluate

Parameter ``sun_angles``:
    Sun azimuth and elevation angles

Parameter ``hit_sun``:
    Indicates if the sun was hit

Parameter ``active``:
    Indicates active lanes

Returns:
    The radiance for the given conditions)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_eval_direction = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_eval_sky =
R"doc(Evaluate the sky model for the given channel indices and angles

Based on the Hosek-Wilkie skylight model https://cgg.mff.cuni.cz/proje
cts/SkylightModelling/HosekWilkie_SkylightModel_SIGGRAPH2012_Preprint_
lowres.pdf

Parameter ``cos_theta``:
    Cosine of the angle between the z-axis (up) and the viewing
    direction

Parameter ``gamma``:
    Angle between the sun and the viewing direction

Parameter ``sky_params``:
    Sky parameters for the model

Parameter ``sky_radiance``:
    Sky radiance for the model

Returns:
    Indirect sun illumination)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_eval_sun =
R"doc(Evaluates the sun model for the given channel indices and angles

The template parameter is used to render the full 11 wavelengths at
once in pre-computations

Based on the Hosek-Wilkie sun model
https://cgg.mff.cuni.cz/publications/adding-a-solar-radiance-function-
to-the-hosek-wilkie-skylight-model/

Parameter ``channel_idx``:
    Indices of the channels to render

Parameter ``sun_theta``:
    Elevation angle of the sun

Parameter ``cos_theta``:
    Cosine of the angle between the z-axis (up) and the viewing
    direction

Parameter ``gamma``:
    Angle between the sun and the viewing direction

Parameter ``active``:
    Mask for the active lanes and channel indices

Returns:
    Direct sun illumination)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_extract_albedo =
R"doc(Extract the albedo values for the required wavelengths/channels

Parameter ``albedo_tex``:
    Texture to extract the albedo from

Returns:
    The buffer with the extracted albedo values for the needed
    wavelengths/channels)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_gaussian_cdf =
R"doc(Computes the gaussian cdf for the given parameters

Parameter ``mu``:
    Gaussian average

Parameter ``sigma``:
    Gaussian standard deviation

Parameter ``x``:
    Point to evaluate the CDF at

Returns:
    The cdf value of the given point)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_get_area_ratio =
R"doc(Scaling factor to keep sun irradiance constant across aperture angles

Parameter ``custom_half_aperture``:
    Half aperture angle used

Returns:
    The appropriate scaling factor)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_get_sky_datasets =
R"doc(Getter sky radiance datasets for the given wavelengths and sun angles

Parameter ``sun_theta``:
    The angle between the sun's direction and the z axis

Parameter ``channel_idx``:
    Indices of the queried channels

Parameter ``active``:
    Indicates which channels are valid indices

Returns:
    The sky mean radiance dataset and the sky parameters for the
    Wilkie-Hosek 2012 sky model)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_get_sky_sampling_weight =
R"doc(Getter fot the probability of sampling the sky for a given sun
position

Parameter ``sun_theta``:
    The angle between the sun's direction and the z axis

Parameter ``active``:
    Indicates active lanes

Returns:
    The probability of sampling the sky over the sun)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_get_sun_angles =
R"doc(Getter for the sun angles at the given time

Parameter ``time``:
    Time value in [0, 1]

Returns:
    The sun azimuth and elevation angle)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_get_sun_irradiance =
R"doc(Getter for the sun's irradiance at a given sun elevation and
wavelengths

Parameter ``sun_theta``:
    The angle between the sun's direction and the z axis

Parameter ``channel_idx``:
    The indices of the queried channels

Parameter ``active``:
    Indicates active lanes

Returns:
    The sun irradiance for the given conditions)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_get_tgmm_data =
R"doc(Getter for the 4 interpolation points on turbidity and elevation of
the Truncated Gaussian Mixtures

Parameter ``sun_theta``:
    The angle between the sun's direction and the z axis

Returns:
    The 4 interpolation weights and the 4 indices corresponding to the
    start of the TGMMs in memory)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_hit_sun = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_hit_sun_2 = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_load_field =
R"doc(Loads a tensor from the given tensor file

Template parameter ``FileTensor``:
    Tensor and type stored in the file

Parameter ``tensor_file``:
    File wrapper

Parameter ``tensor_name``:
    Name of the tensor to extract

Returns:
    The queried tensor)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_albedo = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_albedo_tex = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_bsphere = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_complex_sun = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sky_irrad_dataset = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sky_params_dataset = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sky_rad_dataset = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sky_scale = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sun_half_aperture = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sun_irrad_dataset = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sun_ld = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sun_rad_dataset = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sun_radiance = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_sun_scale = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_tgmm_tables = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_m_turbidity = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_pdf_direction = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_direction = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_position = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_ray = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_reuse_tgmm =
R"doc(Samples a gaussian from the Truncated Gaussian mixture

Parameter ``sample``:
    Sample in [0, 1]

Parameter ``sun_theta``:
    The angle between the sun's direction and the z axis

Parameter ``active``:
    Indicates active lanes

Returns:
    The index of the chosen gaussian and the sample to be reused)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_sky =
R"doc(Samples the sky from the truncated gaussian mixture with the given
sample.

Based on the Truncated Gaussian Mixture Model (TGMM) for sky dome by
N. Vitsas and K. Vardis
https://diglib.eg.org/items/b3f1efca-1d13-44d0-ad60-741c4abe3d21

Parameter ``sample``:
    Sample uniformly distributed in [0, 1]^2

Parameter ``sun_angles``:
    Azimuth and elevation angles of the sun

Parameter ``active``:
    Mask for the active lanes

Returns:
    The sampled direction in the sky)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_sun =
R"doc(Samples the sun from a uniform cone with the given sample

Parameter ``sample``:
    Sample to generate the sun ray

Parameter ``sun_angles``:
    Azimuth and elevation angles of the sun

Returns:
    The sampled sun direction)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_wavelengths = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sample_wlgth =
R"doc(Samples wavelengths and evaluates the associated weights

Parameter ``sample``:
    Floating point value in [0, 1] to sample the wavelengths

Parameter ``active``:
    Indicates active lanes

Returns:
    The chosen wavelengths and their inverse pdf)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_set_scene = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sun_coordinates =
R"doc(Compute the elevation and azimuth of the sun as seen by an observer at
``location`` at the date and time specified in ``dateTime``.

Returns:
    The pair containing the polar angle and the azimuth

Based on "Computing the Solar Vector" by Manuel Blanco-Muriel, Diego
C. Alarcon-Padilla, Teodoro Lopez-Moratalla, and Martin Lara-Coira, in
"Solar energy", vol 27, number 5, 2001 by Pergamon Press.)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_sun_cos_psi =
R"doc(Computes the cosine of the angle made between the sun's radius and the
viewing direction

Parameter ``gamma``:
    Angle between the sun's center and the viewing direction

Returns:
    The cosine of the angle between the sun's radius and the viewing
    direction)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_tgmm_pdf =
R"doc(Computes the PDF from the TGMM for the given viewing angles

Parameter ``angles``:
    Angles of the view direction in local coordinates (phi, theta)

Parameter ``sun_angles``:
    Azimuth and elevation angles of the sun

Parameter ``active``:
    Mask for the active lanes and valid rays

Returns:
    The PDF of the sky for the given angles with no sin(theta) factor)doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_to_string = R"doc()doc";

static const char *__doc_mitsuba_BaseSunskyEmitter_traverse = R"doc()doc";

static const char *__doc_mitsuba_Bitmap =
R"doc(General-purpose bitmap class with read and write support for several
common file formats.

This class handles loading of PNG, JPEG, BMP, TGA, as well as OpenEXR
files, and it supports writing of PNG, JPEG and OpenEXR files.

PNG and OpenEXR files are optionally annotated with string-valued
metadata, and the gamma setting can be stored as well. Please see the
class methods and enumerations for further detail.)doc";

static const char *__doc_mitsuba_Bitmap_AlphaTransform = R"doc(Type of alpha transformation)doc";

static const char *__doc_mitsuba_Bitmap_AlphaTransform_Empty = R"doc(No transformation (default))doc";

static const char *__doc_mitsuba_Bitmap_AlphaTransform_Premultiply = R"doc(Premultiply alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_AlphaTransform_Unpremultiply = R"doc(Unpremultiply alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_Bitmap =
R"doc(Create a bitmap of the specified type and allocate the necessary
amount of memory

Parameter ``pixel_format``:
    Specifies the pixel format (e.g. RGBA or Luminance-only)

Parameter ``component_format``:
    Specifies how the per-pixel components are encoded (e.g. unsigned
    8 bit integers or 32-bit floating point values). The component
    format struct_type_v<Float> will be translated to the
    corresponding compile-time precision type (Float32 or Float64).

Parameter ``size``:
    Specifies the horizontal and vertical bitmap size in pixels

Parameter ``channel_count``:
    Channel count of the image. This parameter is only required when
    ``pixel_format`` = PixelFormat::MultiChannel

Parameter ``channel_names``:
    Channel names of the image. This parameter is optional, and only
    used when ``pixel_format`` = PixelFormat::MultiChannel

Parameter ``data``:
    External pointer to the image data. If set to ``nullptr``, the
    implementation will allocate memory itself.)doc";

static const char *__doc_mitsuba_Bitmap_Bitmap_2 =
R"doc(Load a bitmap from an arbitrary stream data source

Parameter ``stream``:
    Pointer to an arbitrary stream data source

Parameter ``format``:
    File format to be read (PNG/EXR/Auto-detect ...))doc";

static const char *__doc_mitsuba_Bitmap_Bitmap_3 =
R"doc(Load a bitmap from a given filename

Parameter ``path``:
    Name of the file to be loaded

Parameter ``format``:
    File format to be read (PNG/EXR/Auto-detect ...))doc";

static const char *__doc_mitsuba_Bitmap_Bitmap_4 = R"doc(Copy constructor (copies the image contents))doc";

static const char *__doc_mitsuba_Bitmap_Bitmap_5 = R"doc(Move constructor)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat = R"doc(Supported image file formats)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_Auto =
R"doc(Automatically detect the file format

Note: this flag only applies when loading a file. In this case, the
source stream must support the ``seek()`` operation.)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_BMP =
R"doc(Windows Bitmap file format

The following is supported:

* Loading of uncompressed 8-bit luminance and RGBA bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_JPEG =
R"doc(Joint Photographic Experts Group file format

The following is supported:

* Loading and saving of 8 bit per component RGB and luminance bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_OpenEXR =
R"doc(OpenEXR high dynamic range file format developed by Industrial Light &
Magic (ILM)

The following is supported:

* Loading and saving of Float16 / Float32/ UInt32 bitmaps with all
supported RGB/Luminance/Alpha combinations

* Loading and saving of spectral bitmaps

* Loading and saving of XYZ tristimulus bitmaps

* Loading and saving of string-valued metadata fields

The following is *not* supported:

* Saving of tiled images, tile-based read access

* Display windows that are different than the data window

* Loading of spectrum-valued bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_PFM =
R"doc(PFM (Portable Float Map) image format

The following is supported

* Loading and saving of Float32 - based Luminance or RGB bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_PNG =
R"doc(Portable network graphics

The following is supported:

* Loading and saving of 8/16-bit per component bitmaps for all pixel
formats (Y, YA, RGB, RGBA)

* Loading and saving of 1-bit per component mask bitmaps

* Loading and saving of string-valued metadata fields)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_PPM =
R"doc(PPM (Portable Pixel Map) image format

The following is supported

* Loading and saving of UInt8 and UInt16 - based RGB bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_RGBE =
R"doc(RGBE image format by Greg Ward

The following is supported

* Loading and saving of Float32 - based RGB bitmaps)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_TGA =
R"doc(Truevision Advanced Raster Graphics Array file format

The following is supported:

* Loading of uncompressed 8-bit RGB/RGBA files)doc";

static const char *__doc_mitsuba_Bitmap_FileFormat_Unknown = R"doc(Unknown file format)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat =
R"doc(This enumeration lists all pixel format types supported by the Bitmap
class. This both determines the number of channels, and how they
should be interpreted)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_MultiChannel = R"doc(Arbitrary multi-channel bitmap without a fixed interpretation)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_RGB = R"doc(RGB bitmap)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_RGBA = R"doc(RGB bitmap + alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_RGBAW = R"doc(RGB bitmap + alpha channel + weight (used by ImageBlock))doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_RGBW = R"doc(RGB bitmap + weight (used by ImageBlock))doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_XYZ = R"doc(XYZ tristimulus bitmap)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_XYZA = R"doc(XYZ tristimulus + alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_Y = R"doc(Single-channel luminance bitmap)doc";

static const char *__doc_mitsuba_Bitmap_PixelFormat_YA = R"doc(Two-channel luminance + alpha bitmap)doc";

static const char *__doc_mitsuba_Bitmap_accumulate =
R"doc(Accumulate the contents of another bitmap into the region with the
specified offset

Out-of-bounds regions are safely ignored. It is assumed that ``bitmap
!= this``.

Remark:
    This function throws an exception when the bitmaps use different
    component formats or channels.)doc";

static const char *__doc_mitsuba_Bitmap_accumulate_2 =
R"doc(Accumulate the contents of another bitmap into the region with the
specified offset

This convenience function calls the main ``accumulate()``
implementation with ``size`` set to ``bitmap->size()`` and
``source_offset`` set to zero. Out-of-bounds regions are ignored. It
is assumed that ``bitmap != this``.

Remark:
    This function throws an exception when the bitmaps use different
    component formats or channels.)doc";

static const char *__doc_mitsuba_Bitmap_accumulate_3 =
R"doc(Accumulate the contents of another bitmap into the region with the
specified offset

This convenience function calls the main ``accumulate()``
implementation with ``size`` set to ``bitmap->size()`` and
``source_offset`` and ``target_offset`` set to zero. Out-of-bounds
regions are ignored. It is assumed that ``bitmap != this``.

Remark:
    This function throws an exception when the bitmaps use different
    component formats or channels.)doc";

static const char *__doc_mitsuba_Bitmap_buffer_size = R"doc(Return the bitmap size in bytes (excluding metadata))doc";

static const char *__doc_mitsuba_Bitmap_bytes_per_pixel = R"doc(Return the number bytes of storage used per pixel)doc";

static const char *__doc_mitsuba_Bitmap_channel_count = R"doc(Return the number of channels used by this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_class_name = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_clear = R"doc(Clear the bitmap to zero)doc";

static const char *__doc_mitsuba_Bitmap_component_format = R"doc(Return the component format of this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_convert =
R"doc(Convert the bitmap into another pixel and/or component format

This helper function can be used to efficiently convert a bitmap
between different underlying representations. For instance, it can
translate a uint8 sRGB bitmap to a linear float32 XYZ bitmap based on
half-, single- or double-precision floating point-backed storage.

This function roughly does the following:

* For each pixel and channel, it converts the associated value into a
normalized linear-space form (any gamma of the source bitmap is
removed)

* gamma correction (sRGB ramp) is applied if ``srgb_gamma`` is
``True``

* The corrected value is clamped against the representable range of
the desired component format.

* The clamped gamma-corrected value is then written to the new bitmap

If the pixel formats differ, this function will also perform basic
conversions (e.g. spectrum to rgb, luminance to uniform spectrum
values, etc.)

Note that the alpha channel is assumed to be linear in both the source
and target bitmap, therefore it won't be affected by any gamma-related
transformations.

Remark:
    This ``convert()`` variant usually returns a new bitmap instance.
    When the conversion would just involve copying the original
    bitmap, the function becomes a no-op and returns the current
    instance.

pixel_format Specifies the desired pixel format

component_format Specifies the desired component format

srgb_gamma Specifies whether a sRGB gamma ramp should be applied to
the output values.)doc";

static const char *__doc_mitsuba_Bitmap_convert_2 = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_data = R"doc(Return a pointer to the underlying bitmap storage)doc";

static const char *__doc_mitsuba_Bitmap_data_2 = R"doc(Return a pointer to the underlying bitmap storage)doc";

static const char *__doc_mitsuba_Bitmap_detect_file_format = R"doc(Attempt to detect the bitmap file format in a given stream)doc";

static const char *__doc_mitsuba_Bitmap_has_alpha = R"doc(Return whether this image has an alpha channel)doc";

static const char *__doc_mitsuba_Bitmap_height = R"doc(Return the bitmap's height in pixels)doc";

static const char *__doc_mitsuba_Bitmap_m_component_format = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_data = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_metadata = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_owns_data = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_pixel_format = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_premultiplied_alpha = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_size = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_srgb_gamma = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_m_struct = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_metadata = R"doc(Return a Properties object containing the image metadata)doc";

static const char *__doc_mitsuba_Bitmap_metadata_2 =
R"doc(Return a Properties object containing the image metadata (const
version))doc";

static const char *__doc_mitsuba_Bitmap_operator_eq = R"doc(Equality comparison operator)doc";

static const char *__doc_mitsuba_Bitmap_operator_ne = R"doc(Inequality comparison operator)doc";

static const char *__doc_mitsuba_Bitmap_pixel_count = R"doc(Return the total number of pixels)doc";

static const char *__doc_mitsuba_Bitmap_pixel_format = R"doc(Return the pixel format of this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_premultiplied_alpha = R"doc(Return whether the bitmap uses premultiplied alpha)doc";

static const char *__doc_mitsuba_Bitmap_read = R"doc(Read a file from a stream)doc";

static const char *__doc_mitsuba_Bitmap_read_bmp = R"doc(Read a file encoded using the BMP file format)doc";

static const char *__doc_mitsuba_Bitmap_read_exr = R"doc(Read a file encoded using the OpenEXR file format)doc";

static const char *__doc_mitsuba_Bitmap_read_jpeg = R"doc(Read a file encoded using the JPEG file format)doc";

static const char *__doc_mitsuba_Bitmap_read_pfm = R"doc(Read a file encoded using the PFM file format)doc";

static const char *__doc_mitsuba_Bitmap_read_png = R"doc(Read a file encoded using the PNG file format)doc";

static const char *__doc_mitsuba_Bitmap_read_ppm = R"doc(Read a file encoded using the PPM file format)doc";

static const char *__doc_mitsuba_Bitmap_read_rgbe = R"doc(Read a file encoded using the RGBE file format)doc";

static const char *__doc_mitsuba_Bitmap_read_tga = R"doc(Read a file encoded using the TGA file format)doc";

static const char *__doc_mitsuba_Bitmap_rebuild_struct = R"doc(Rebuild the 'm_struct' field based on the pixel format etc.)doc";

static const char *__doc_mitsuba_Bitmap_resample =
R"doc(Up- or down-sample this image to a different resolution

Uses the provided reconstruction filter and accounts for the requested
horizontal and vertical boundary conditions when looking up data
outside of the input domain.

A minimum and maximum image value can be specified to prevent to
prevent out-of-range values that are created by the resampling
process.

The optional ``temp`` parameter can be used to pass an image of
resolution ``Vector2u(target->width(), this->height())`` to avoid
intermediate memory allocations.

Parameter ``target``:
    Pre-allocated bitmap of the desired target resolution

Parameter ``rfilter``:
    A separable image reconstruction filter (default: 2-lobe Lanczos
    filter)

Parameter ``bch``:
    Horizontal and vertical boundary conditions (default: clamp)

Parameter ``clamp``:
    Filtered image pixels will be clamped to the following range.
    Default: -infinity..infinity (i.e. no clamping is used)

Parameter ``temp``:
    Optional: image for intermediate computations)doc";

static const char *__doc_mitsuba_Bitmap_resample_2 =
R"doc(Up- or down-sample this image to a different resolution

This version is similar to the above resample() function -- the main
difference is that it does not work with preallocated bitmaps and
takes the desired output resolution as first argument.

Uses the provided reconstruction filter and accounts for the requested
horizontal and vertical boundary conditions when looking up data
outside of the input domain.

A minimum and maximum image value can be specified to prevent to
prevent out-of-range values that are created by the resampling
process.

Parameter ``res``:
    Desired output resolution

Parameter ``rfilter``:
    A separable image reconstruction filter (default: 2-lobe Lanczos
    filter)

Parameter ``bch``:
    Horizontal and vertical boundary conditions (default: clamp)

Parameter ``clamp``:
    Filtered image pixels will be clamped to the following range.
    Default: -infinity..infinity (i.e. no clamping is used))doc";

static const char *__doc_mitsuba_Bitmap_set_metadata = R"doc(Set the a Properties object containing the image metadata)doc";

static const char *__doc_mitsuba_Bitmap_set_premultiplied_alpha = R"doc(Specify whether the bitmap uses premultiplied alpha)doc";

static const char *__doc_mitsuba_Bitmap_set_srgb_gamma = R"doc(Specify whether the bitmap uses an sRGB gamma encoding)doc";

static const char *__doc_mitsuba_Bitmap_size = R"doc(Return the bitmap dimensions in pixels)doc";

static const char *__doc_mitsuba_Bitmap_split =
R"doc(Split an multi-channel image buffer (e.g. from an OpenEXR image with
lots of AOVs) into its constituent layers)doc";

static const char *__doc_mitsuba_Bitmap_srgb_gamma = R"doc(Return whether the bitmap uses an sRGB gamma encoding)doc";

static const char *__doc_mitsuba_Bitmap_static_initialization =
R"doc(Static initialization of bitmap-related data structures (thread pools,
etc.))doc";

static const char *__doc_mitsuba_Bitmap_static_shutdown = R"doc(Free the resources used by static_initialization())doc";

static const char *__doc_mitsuba_Bitmap_struct =
R"doc(Return a ``Struct`` instance describing the contents of the bitmap
(const version))doc";

static const char *__doc_mitsuba_Bitmap_struct_2 = R"doc(Return a ``Struct`` instance describing the contents of the bitmap)doc";

static const char *__doc_mitsuba_Bitmap_to_string = R"doc(Return a human-readable summary of this bitmap)doc";

static const char *__doc_mitsuba_Bitmap_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Bitmap_uint8_data = R"doc(Return a pointer to the underlying data)doc";

static const char *__doc_mitsuba_Bitmap_uint8_data_2 = R"doc(Return a pointer to the underlying data (const))doc";

static const char *__doc_mitsuba_Bitmap_vflip = R"doc(Vertically flip the bitmap)doc";

static const char *__doc_mitsuba_Bitmap_width = R"doc(Return the bitmap's width in pixels)doc";

static const char *__doc_mitsuba_Bitmap_write =
R"doc(Write an encoded form of the bitmap to a stream using the specified
file format

Parameter ``stream``:
    Target stream that will receive the encoded output

Parameter ``format``:
    Target file format (OpenEXR, PNG, etc.) Detected from the filename
    by default.

Parameter ``quality``:
    Depending on the file format, this parameter takes on a slightly
    different meaning:

* PNG images: Controls how much libpng will attempt to compress the
output (with 1 being the lowest and 9 denoting the highest
compression). The default argument uses the compression level 5.

* JPEG images: denotes the desired quality (between 0 and 100). The
default argument (-1) uses the highest quality (100).

* OpenEXR images: denotes the quality level of the DWAB compressor,
with higher values corresponding to a lower quality. A value of 45 is
recommended as the default for lossy compression. The default argument
(-1) causes the implementation to switch to the lossless PIZ
compressor.)doc";

static const char *__doc_mitsuba_Bitmap_write_2 =
R"doc(Write an encoded form of the bitmap to a file using the specified file
format

Parameter ``path``:
    Target file path on disk

Parameter ``format``:
    Target file format (FileFormat::OpenEXR, FileFormat::PNG, etc.)
    Detected from the filename by default.

Parameter ``quality``:
    Depending on the file format, this parameter takes on a slightly
    different meaning:

* PNG images: Controls how much libpng will attempt to compress the
output (with 1 being the lowest and 9 denoting the highest
compression). The default argument uses the compression level 5.

* JPEG images: denotes the desired quality (between 0 and 100). The
default argument (-1) uses the highest quality (100).

* OpenEXR images: denotes the quality level of the DWAB compressor,
with higher values corresponding to a lower quality. A value of 45 is
recommended as the default for lossy compression. The default argument
(-1) causes the implementation to switch to the lossless PIZ
compressor.)doc";

static const char *__doc_mitsuba_Bitmap_write_async =
R"doc(Equivalent to write(), but executes asynchronously on a different
thread)doc";

static const char *__doc_mitsuba_Bitmap_write_exr = R"doc(Write a file using the OpenEXR file format)doc";

static const char *__doc_mitsuba_Bitmap_write_jpeg = R"doc(Save a file using the JPEG file format)doc";

static const char *__doc_mitsuba_Bitmap_write_pfm = R"doc(Save a file using the PFM file format)doc";

static const char *__doc_mitsuba_Bitmap_write_png = R"doc(Save a file using the PNG file format)doc";

static const char *__doc_mitsuba_Bitmap_write_ppm = R"doc(Save a file using the PPM file format)doc";

static const char *__doc_mitsuba_Bitmap_write_rgbe = R"doc(Save a file using the RGBE file format)doc";

static const char *__doc_mitsuba_BoundingBox =
R"doc(Generic n-dimensional bounding box data structure

Maintains a minimum and maximum position along each dimension and
provides various convenience functions for querying and modifying
them.

This class is parameterized by the underlying point data structure,
which permits the use of different scalar types and dimensionalities,
e.g.

```
BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));
```

Template parameter ``T``:
    The underlying point data type (e.g. ``Point2d``))doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox =
R"doc(Create a new invalid bounding box

Initializes the components of the minimum and maximum position to
:math:`\infty` and :math:`-\infty`, respectively.)doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox_2 = R"doc(Create a collapsed bounding box from a single point)doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox_3 = R"doc(Create a bounding box from two positions)doc";

static const char *__doc_mitsuba_BoundingBox_BoundingBox_4 =
R"doc(Create a bounding box from a smaller type (e.g. vectorized from
scalar).)doc";

static const char *__doc_mitsuba_BoundingBox_bounding_sphere = R"doc(Create a bounding sphere, which contains the axis-aligned box)doc";

static const char *__doc_mitsuba_BoundingBox_center = R"doc(Return the center point)doc";

static const char *__doc_mitsuba_BoundingBox_clip = R"doc(Clip this bounding box to another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_collapsed =
R"doc(Check whether this bounding box has collapsed to a point, line, or
plane)doc";

static const char *__doc_mitsuba_BoundingBox_contains =
R"doc(Check whether a point lies *on* or *inside* the bounding box

Parameter ``p``:
    The point to be tested

Template parameter ``Strict``:
    Set this parameter to ``True`` if the bounding box boundary should
    be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.)doc";

static const char *__doc_mitsuba_BoundingBox_contains_2 =
R"doc(Check whether a specified bounding box lies *on* or *within* the
current bounding box

Note that by definition, an 'invalid' bounding box (where
min=:math:`\infty` and max=:math:`-\infty`) does not cover any space.
Hence, this method will always return *true* when given such an
argument.

Template parameter ``Strict``:
    Set this parameter to ``True`` if the bounding box boundary should
    be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.)doc";

static const char *__doc_mitsuba_BoundingBox_corner = R"doc(Return the position of a bounding box corner)doc";

static const char *__doc_mitsuba_BoundingBox_distance =
R"doc(Calculate the shortest distance between the axis-aligned bounding box
and the point ``p``.)doc";

static const char *__doc_mitsuba_BoundingBox_distance_2 =
R"doc(Calculate the shortest distance between the axis-aligned bounding box
and ``bbox``.)doc";

static const char *__doc_mitsuba_BoundingBox_expand = R"doc(Expand the bounding box to contain another point)doc";

static const char *__doc_mitsuba_BoundingBox_expand_2 = R"doc(Expand the bounding box to contain another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_extents =
R"doc(Calculate the bounding box extents

Returns:
    ``max - min``)doc";

static const char *__doc_mitsuba_BoundingBox_major_axis = R"doc(Return the dimension index with the index associated side length)doc";

static const char *__doc_mitsuba_BoundingBox_max = R"doc(Component-wise minimum)doc";

static const char *__doc_mitsuba_BoundingBox_merge = R"doc(Merge two bounding boxes)doc";

static const char *__doc_mitsuba_BoundingBox_min = R"doc()doc";

static const char *__doc_mitsuba_BoundingBox_minor_axis = R"doc(Return the dimension index with the shortest associated side length)doc";

static const char *__doc_mitsuba_BoundingBox_operator_eq = R"doc(Test for equality against another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_operator_ne = R"doc(Test for inequality against another bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_overlaps =
R"doc(Check two axis-aligned bounding boxes for possible overlap.

Parameter ``Strict``:
    Set this parameter to ``True`` if the bounding box boundary should
    be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.

Returns:
    ``True`` If overlap was detected.)doc";

static const char *__doc_mitsuba_BoundingBox_ray_intersect =
R"doc(Check if a ray intersects a bounding box

Note that this function ignores the ``maxt`` value associated with the
ray.)doc";

static const char *__doc_mitsuba_BoundingBox_reset =
R"doc(Mark the bounding box as invalid.

This operation sets the components of the minimum and maximum position
to :math:`\infty` and :math:`-\infty`, respectively.)doc";

static const char *__doc_mitsuba_BoundingBox_squared_distance =
R"doc(Calculate the shortest squared distance between the axis-aligned
bounding box and the point ``p``.)doc";

static const char *__doc_mitsuba_BoundingBox_squared_distance_2 =
R"doc(Calculate the shortest squared distance between the axis-aligned
bounding box and ``bbox``.)doc";

static const char *__doc_mitsuba_BoundingBox_surface_area = R"doc(Calculate the 2-dimensional surface area of a 3D bounding box)doc";

static const char *__doc_mitsuba_BoundingBox_valid =
R"doc(Check whether this is a valid bounding box

A bounding box ``bbox`` is considered to be valid when

```
bbox.min[i] <= bbox.max[i]
```

holds for each component ``i``.)doc";

static const char *__doc_mitsuba_BoundingBox_volume = R"doc(Calculate the n-dimensional volume of the bounding box)doc";

static const char *__doc_mitsuba_BoundingSphere = R"doc(Generic n-dimensional bounding sphere data structure)doc";

static const char *__doc_mitsuba_BoundingSphere_BoundingSphere = R"doc(Construct bounding sphere(s) at the origin having radius zero)doc";

static const char *__doc_mitsuba_BoundingSphere_BoundingSphere_2 =
R"doc(Create bounding sphere(s) from given center point(s) with given
size(s))doc";

static const char *__doc_mitsuba_BoundingSphere_center = R"doc()doc";

static const char *__doc_mitsuba_BoundingSphere_contains =
R"doc(Check whether a point lies *on* or *inside* the bounding sphere

Parameter ``p``:
    The point to be tested

Template parameter ``Strict``:
    Set this parameter to ``True`` if the bounding sphere boundary
    should be excluded in the test

Remark:
    In the Python bindings, the 'Strict' argument is a normal function
    parameter with default value ``False``.)doc";

static const char *__doc_mitsuba_BoundingSphere_empty = R"doc(Return whether this bounding sphere has a radius of zero or less.)doc";

static const char *__doc_mitsuba_BoundingSphere_expand = R"doc(Expand the bounding sphere radius to contain another point)doc";

static const char *__doc_mitsuba_BoundingSphere_operator_eq = R"doc(Equality test against another bounding sphere)doc";

static const char *__doc_mitsuba_BoundingSphere_operator_ne = R"doc(Inequality test against another bounding sphere)doc";

static const char *__doc_mitsuba_BoundingSphere_radius = R"doc()doc";

static const char *__doc_mitsuba_BoundingSphere_ray_intersect = R"doc(Check if a ray intersects a bounding box)doc";

static const char *__doc_mitsuba_BoundingSphere_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_BoundingSphere_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Color = R"doc(//! @{ \name Data types for RGB data)doc";

static const char *__doc_mitsuba_Color_Color = R"doc()doc";

static const char *__doc_mitsuba_Color_Color_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_Color_3 = R"doc()doc";

static const char *__doc_mitsuba_Color_a = R"doc()doc";

static const char *__doc_mitsuba_Color_a_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_b = R"doc()doc";

static const char *__doc_mitsuba_Color_b_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_g = R"doc()doc";

static const char *__doc_mitsuba_Color_g_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Color_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Color_r = R"doc()doc";

static const char *__doc_mitsuba_Color_r_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D =
R"doc(Conditional 1D irregular distribution

Similar to the irregular 1D distribution, but this class represents an
N-Dimensional irregular one (with the extra conditional dimensions
being also irregular).

As an example, assume you have a 3D distribution P(x,y,z), with
leading dimension X. This class would allow you to obtain the linear
interpolated value of the PDF for ``x`` given ``y`` and ``z``.
Additionally, it allows you to sample from the distribution
P(x|Y=y,Z=z) for a given ``y`` and ``z``.

It assumes every conditioned PDF has the same size. If the user
requests a method that needs the integral, it will schedule its
computation.

This distribution can be used in the context of spectral rendering,
where each wavelength conditions the underlying distribution.)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_ConditionalIrregular1D = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_ConditionalIrregular1D_2 =
R"doc(Construct a conditional irregular 1D distribution

Parameter ``nodes``:
    Points where the leading dimension N is defined

Parameter ``pdf``:
    Flattened array of shape [D1, D2, ..., Dn, N], containing the PDFs

Parameter ``nodes_cond``:
    Arrays containing points where each conditional dimension is
    evaluated)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_ConditionalIrregular1D_3 =
R"doc(Construct a conditional irregular 1D distribution

Parameter ``nodes``:
    Points where the leading dimension N is defined

Parameter ``pdf``:
    Tensor containing the values of the PDF of shape [D1, D2, ..., Dn,
    N]

Parameter ``nodes_cond``:
    Arrays containing points where each conditional dimension is
    evaluated)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_ConditionalIrregular1D_4 =
R"doc(Construct a conditional irregular 1D distribution

Parameter ``nodes``:
    Points where the PDFs are evaluated

Parameter ``size_nodes``:
    Size of the nodes array

Parameter ``pdf``:
    Flattened array of shape [D1, D2, ..., Dn, N], containing the PDFs

Parameter ``size_pdf``:
    Size of the pdf array

Parameter ``nodes_cond``:
    Arrays containing points where the conditional is evaluated

Parameter ``sizes_cond``:
    Array with the sizes of the conditional nodes arrays)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_cdf_array = R"doc(Return the CDF)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_cdf_array_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_compute_cdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_compute_cdf_scalar = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_empty = R"doc(Is the distribution object empty/uninitialized?)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_ensure_cdf_computed = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_eval_pdf =
R"doc(Evaluate the unnormalized probability density function (PDF) at
position ``pos``, conditioned on ``cond``

Parameter ``pos``:
    Position where the PDF is evaluated

Parameter ``cond``:
    Array of values where the conditionals are evaluated

Parameter ``active``:
    Mask of active lanes

Returns:
    The value of the PDF at position ``pos``, conditioned on ``cond``)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_eval_pdf_normalized =
R"doc(Evaluate the normalized probability density function (PDF) at position
``pos``, conditioned on ``cond``

Parameter ``pos``:
    Position where the PDF is evaluated

Parameter ``cond``:
    Array of values where the conditionals are evaluated

Parameter ``active``:
    Mask of active lanes

Returns:
    The value of the normalized PDF at position ``pos``, conditioned
    on ``cond``)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_integral =
R"doc(Return the integral of the distribution conditioned on ``cond``

Parameter ``cond``:
    Conditionals that define the distribution

Returns:
    The integral of the distribution)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_integral_array = R"doc(Return the integral array)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_integral_array_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_lookup = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_lookup_fill = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_lookup_sample = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_m_integral = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_m_max = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_m_nodes = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_m_nodes_cond = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_m_pdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_max = R"doc(Return the maximum value of the distribution)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_nodes = R"doc(Return the nodes of the underlying discretization)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_nodes_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_nodes_cond = R"doc(Return the conditional nodes of the underlying discretization)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_nodes_cond_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_pdf = R"doc(Return the underlying tensor storing the distribution values)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_pdf_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_sample_pdf =
R"doc(Sample the distribution given a uniform sample ``u``, conditioned on
``cond``

Parameter ``u``:
    Uniform sample

Parameter ``cond``:
    Conditionals where the PDF is sampled

Parameter ``active``:
    Mask of active lanes

Returns:
    A pair where the first element is the sampled position and the
    second element the value of the normalized PDF at that position
    conditioned on ``cond``)doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_ConditionalIrregular1D_update = R"doc(Update the internal state. Must be invoked when changing the pdf.)doc";

static const char *__doc_mitsuba_ConditionalRegular1D =
R"doc(Conditional 1D regular distribution

Similar to the regular 1D distribution, but this class represents an
N-Dimensional regular one (with the extra conditional dimensions being
also regular).

As an example, assume you have a 3D distribution P(x,y,z), with
leading dimension X. This class would allow you to obtain the linear
interpolated value of the PDF for ``x`` given ``y`` and ``z``.
Additionally, it allows you to sample from the distribution
P(x|Y=y,Z=z) for a given ``y`` and ``z``.

It assumes every conditioned PDF has the same size. If the user
requests a method that needs the integral, it will schedule its
computation.

This distribution can be used in the context of spectral rendering,
where each wavelength conditions the underlying distribution.)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_ConditionalRegular1D = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_ConditionalRegular1D_2 =
R"doc(Construct a conditional regular 1D distribution

Parameter ``pdf``:
    Flattened array of shape [D1, D2, ..., Dn, N] containing the PDFs

Parameter ``range``:
    Range where the leading dimension N is defined

Parameter ``range_cond``:
    Array of ranges where the dimensional conditionals are defined

Parameter ``size_cond``:
    Array with the size of each conditional dimension)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_ConditionalRegular1D_3 =
R"doc(Construct a conditional regular 1D distribution

Parameter ``pdf``:
    Tensor containing the values of the PDF of shape [D1, D2, ..., Dn,
    N]

Parameter ``range``:
    Range where the leading dimension N is defined

Parameter ``range_cond``:
    Array of ranges where the dimensional conditionals are defined)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_ConditionalRegular1D_4 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_cdf_array = R"doc(Return the cdf array of the distribution)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_cdf_array_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_compute_cdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_compute_cdf_scalar = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_empty = R"doc(Is the distribution object empty/uninitialized?)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_ensure_cdf_computed = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_eval_pdf =
R"doc(Evaluate the unnormalized probability density function (PDF) at
position ``x``, conditioned on ``cond``

Parameter ``x``:
    Position where the PDF is evaluated

Parameter ``cond``:
    Conditionals where the PDF is evaluated

Parameter ``active``:
    Mask of active lanes)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_eval_pdf_normalized =
R"doc(Evaluate the normalized probability density function (PDF) at position
``x``, conditioned on ``cond``

Parameter ``x``:
    Position where the PDF is evaluated

Parameter ``cond``:
    Conditionals where the PDF is evaluated

Parameter ``active``:
    Mask of active lanes)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_integral = R"doc(Return the integral of the distribution conditioned on ``cond``)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_integral_array = R"doc(Return the integral array of the distribution)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_integral_array_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_lookup = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_lookup_fill = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_lookup_sample = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_integral = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_interval = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_interval_scalar = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_inv_interval = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_inv_interval_cond = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_max = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_pdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_range = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_range_cond = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_size_cond = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_m_size_nodes = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_max = R"doc(Return the maximum value of the distribution)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_pdf = R"doc(Return the underlying tensor storing the distribution values)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_pdf_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_prepare_cdf = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_prepare_distribution = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_range = R"doc(Return the range where the distribution is defined)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_range_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_range_cond = R"doc(Return the conditional range where the distribution is defined)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_range_cond_2 = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_sample_pdf =
R"doc(Sample the distribution given a uniform sample ``u``, conditioned on
``cond``

Parameter ``u``:
    Uniform sample

Parameter ``cond``:
    Conditionals where the PDF is sampled

Parameter ``active``:
    Mask of active lanes)doc";

static const char *__doc_mitsuba_ConditionalRegular1D_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_ConditionalRegular1D_update =
R"doc(Update the internal state. Must be invoked when changing the
distribution.)doc";

static const char *__doc_mitsuba_ContinuousDistribution =
R"doc(Continuous 1D probability distribution defined in terms of a regularly
sampled linear interpolant

This data structure represents a continuous 1D probability
distribution that is defined as a linear interpolant of a regularly
discretized signal. The class provides various routines for
transforming uniformly distributed samples so that they follow the
stored distribution. Note that unnormalized probability density
functions (PDFs) will automatically be normalized during
initialization. The associated scale factor can be retrieved using the
function normalization().)doc";

static const char *__doc_mitsuba_ContinuousDistribution_ContinuousDistribution = R"doc(Create an uninitialized ContinuousDistribution instance)doc";

static const char *__doc_mitsuba_ContinuousDistribution_ContinuousDistribution_2 = R"doc(Initialize from a given density function on the interval ``range``)doc";

static const char *__doc_mitsuba_ContinuousDistribution_ContinuousDistribution_3 = R"doc(Initialize from a given density function (rvalue version))doc";

static const char *__doc_mitsuba_ContinuousDistribution_ContinuousDistribution_4 = R"doc(Initialize from a given floating point array)doc";

static const char *__doc_mitsuba_ContinuousDistribution_cdf =
R"doc(Return the unnormalized discrete cumulative distribution function over
intervals)doc";

static const char *__doc_mitsuba_ContinuousDistribution_cdf_2 =
R"doc(Return the unnormalized discrete cumulative distribution function over
intervals (const version))doc";

static const char *__doc_mitsuba_ContinuousDistribution_compute_cdf = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_compute_cdf_scalar = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_empty = R"doc(Is the distribution object empty/uninitialized?)doc";

static const char *__doc_mitsuba_ContinuousDistribution_eval_cdf =
R"doc(Evaluate the unnormalized cumulative distribution function (CDF) at
position ``p``)doc";

static const char *__doc_mitsuba_ContinuousDistribution_eval_cdf_normalized =
R"doc(Evaluate the unnormalized cumulative distribution function (CDF) at
position ``p``)doc";

static const char *__doc_mitsuba_ContinuousDistribution_eval_pdf =
R"doc(Evaluate the unnormalized probability mass function (PDF) at position
``x``)doc";

static const char *__doc_mitsuba_ContinuousDistribution_eval_pdf_normalized =
R"doc(Evaluate the normalized probability mass function (PDF) at position
``x``)doc";

static const char *__doc_mitsuba_ContinuousDistribution_integral = R"doc(Return the original integral of PDF entries before normalization)doc";

static const char *__doc_mitsuba_ContinuousDistribution_interval_resolution = R"doc(Return the minimum resolution of the discretization)doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_integral = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_interval_size = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_interval_size_scalar = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_inv_interval_size = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_max = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_normalization = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_pdf = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_range = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_m_valid = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_max = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_normalization = R"doc(Return the normalization factor (i.e. the inverse of sum()))doc";

static const char *__doc_mitsuba_ContinuousDistribution_pdf = R"doc(Return the unnormalized discretized probability density function)doc";

static const char *__doc_mitsuba_ContinuousDistribution_pdf_2 =
R"doc(Return the unnormalized discretized probability density function
(const version))doc";

static const char *__doc_mitsuba_ContinuousDistribution_range = R"doc(Return the range of the distribution)doc";

static const char *__doc_mitsuba_ContinuousDistribution_range_2 = R"doc(Return the range of the distribution (const version))doc";

static const char *__doc_mitsuba_ContinuousDistribution_sample =
R"doc(Transform a uniformly distributed sample to the stored distribution

Parameter ``sample``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    The sampled position.)doc";

static const char *__doc_mitsuba_ContinuousDistribution_sample_pdf =
R"doc(Transform a uniformly distributed sample to the stored distribution

Parameter ``sample``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    A tuple consisting of

1. the sampled position. 2. the normalized probability density of the
sample.)doc";

static const char *__doc_mitsuba_ContinuousDistribution_size = R"doc(Return the number of discretizations)doc";

static const char *__doc_mitsuba_ContinuousDistribution_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_ContinuousDistribution_update = R"doc(Update the internal state. Must be invoked when changing the pdf.)doc";

static const char *__doc_mitsuba_DateTimeRecord = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_DateTimeRecord = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_DateTimeRecord_2 = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_DateTimeRecord_3 = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_DateTimeRecord_4 = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_day = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_fields = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_get_days_between = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_hour = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_labels = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_minute = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_month = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_name = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_operator_assign_3 = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_second = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_to_elapsed_julian_date =
R"doc(Calculate difference in days between the current Julian Day and JD
2451545.0, which is noon 1 January 2000 Universal Time

Parameter ``timezone``:
    (Float) Timezone to use for conversion

Returns:
    The elapsed time in julian days since New Year of 2000.)doc";

static const char *__doc_mitsuba_DateTimeRecord_to_string = R"doc()doc";

static const char *__doc_mitsuba_DateTimeRecord_year = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter =
R"doc(The default formatter used to turn log messages into a human-readable
form)doc";

static const char *__doc_mitsuba_DefaultFormatter_DefaultFormatter = R"doc(Create a new default formatter)doc";

static const char *__doc_mitsuba_DefaultFormatter_class_name = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_format = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_has_class =
R"doc(See also:
    set_has_class)doc";

static const char *__doc_mitsuba_DefaultFormatter_has_date =
R"doc(See also:
    set_has_date)doc";

static const char *__doc_mitsuba_DefaultFormatter_has_log_level =
R"doc(See also:
    set_has_log_level)doc";

static const char *__doc_mitsuba_DefaultFormatter_has_thread =
R"doc(See also:
    set_has_thread)doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_class = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_date = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_log_level = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_m_has_thread = R"doc()doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_class = R"doc(Should class information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_date = R"doc(Should date information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_log_level = R"doc(Should log level information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DefaultFormatter_set_has_thread = R"doc(Should thread information be included? The default is yes.)doc";

static const char *__doc_mitsuba_DirectionSample =
R"doc(Record for solid-angle based area sampling techniques

This data structure is used in techniques that sample positions
relative to a fixed reference position in the scene. For instance,
*direct illumination strategies* importance sample the incident
radiance received by a given surface location. Mitsuba uses this
approach in a wider bidirectional sense: sampling the incident
importance due to a sensor also uses the same data structures and
strategies, which are referred to as *direct sampling*.

This record inherits all fields from PositionSample and extends it
with two useful quantities that are cached so that they don't need to
be recomputed: the unit direction and distance from the reference
position to the sampled point.)doc";

static const char *__doc_mitsuba_DirectionSample_DirectionSample =
R"doc(Create a direct sampling record, which can be used to *query* the
density of a surface position with respect to a given reference
position.

Direction 's' is set so that it points from the reference surface to
the intersected surface, as required when using e.g. the Endpoint
interface to compute PDF values.

Parameter ``scene``:
    Pointer to the scene, which is needed to extract information about
    the environment emitter (if applicable)

Parameter ``it``:
    Surface interaction

Parameter ``ref``:
    Reference position)doc";

static const char *__doc_mitsuba_DirectionSample_DirectionSample_2 = R"doc(Element-by-element constructor)doc";

static const char *__doc_mitsuba_DirectionSample_DirectionSample_3 = R"doc(Construct from a position sample)doc";

static const char *__doc_mitsuba_DirectionSample_DirectionSample_4 = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_DirectionSample_5 = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_DirectionSample_6 = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_d = R"doc(Unit direction from the reference point to the target shape)doc";

static const char *__doc_mitsuba_DirectionSample_dist = R"doc(Distance from the reference point to the target shape)doc";

static const char *__doc_mitsuba_DirectionSample_emitter =
R"doc(Optional: pointer to an associated object

In some uses of this record, sampling a position also involves
choosing one of several objects (shapes, emitters, ..) on which the
position lies. In that case, the ``object`` attribute stores a pointer
to this object.)doc";

static const char *__doc_mitsuba_DirectionSample_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_operator_array = R"doc(Convenience operator for masking)doc";

static const char *__doc_mitsuba_DirectionSample_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_DirectionSample_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_DiscontinuityFlags =
R"doc(This list of flags is used to control the behavior of discontinuity
related routines.)doc";

static const char *__doc_mitsuba_DiscontinuityFlags_AllTypes = R"doc(All types of discontinuities)doc";

static const char *__doc_mitsuba_DiscontinuityFlags_DirectionLune = R"doc(//! Encoding and projection flags)doc";

static const char *__doc_mitsuba_DiscontinuityFlags_DirectionSphere = R"doc(//! Encoding and projection flags)doc";

static const char *__doc_mitsuba_DiscontinuityFlags_Empty = R"doc(No flags set (default value))doc";

static const char *__doc_mitsuba_DiscontinuityFlags_HeuristicWalk = R"doc(//! Encoding and projection flags)doc";

static const char *__doc_mitsuba_DiscontinuityFlags_InteriorType = R"doc(Smooth normal type of discontinuity)doc";

static const char *__doc_mitsuba_DiscontinuityFlags_PerimeterType = R"doc(Open boundary or jumping normal type of discontinuity)doc";

static const char *__doc_mitsuba_DiscreteDistribution =
R"doc(Discrete 1D probability distribution

This data structure represents a discrete 1D probability distribution
and provides various routines for transforming uniformly distributed
samples so that they follow the stored distribution. Note that
unnormalized probability mass functions (PMFs) will automatically be
normalized during initialization. The associated scale factor can be
retrieved using the function normalization().)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D =
R"doc(======================================================================
= @{ \name Data-driven warping techniques for two dimensions

This file provides three different approaches for importance sampling
2D functions discretized on a regular grid. All functionality is
written in a generic fashion and works in scalar mode, packet mode,
and the just-in-time compiler (in particular, the complete sampling
procedure is designed to be JIT-compiled to a single CUDA or LLVM
kernel without any intermediate synchronization steps.)

The first class ``DiscreteDistribution2D`` generates samples
proportional to a *discrete* 2D function sampled on a regular grid by
sampling the marginal distribution to choose a row, then a conditional
distribution to choose a column. This is a very simple ingredient that
can be used to build more advanced kinds of sampling schemes.

The other two classes ``Hierarchical2D`` and ``Marginal2D`` are
significantly more complex and target sampling of *linear
interpolants*, which means that the sampling procedure is a function
with floating point inputs and outputs. The mapping is bijective and
can be evaluated in *both directions*. The implementations also
supports *conditional distributions*, i.e., 2D distributions that
depend on an arbitrary number of parameters (indicated via the
``Dimension`` template parameter). In this case, a higher-dimensional
discretization must be provided that will also be linearly
interpolated in these extra dimensions.

Both approaches will produce exactly the same probability density, but
the mapping from random numbers to samples tends to be very different,
which can play an important role in certain applications. In
particular:

``Hierarchical2D`` generates samples using hierarchical sample
warping, which is essentially a coarse-to-fine traversal of a MIP map.
It generates a mapping with very little shear/distortion, but it has
numerous discontinuities that can be problematic for some
applications.

``Marginal2D`` is similar to ``DiscreteDistribution2D``, in that it
samples the marginal, then the conditional. In contrast to
``DiscreteDistribution2D``, the mapping provides fractional outputs.
In contrast to ``Hierarchical2D``, the mapping is guaranteed to not
contain any discontinuities but tends to have significant
shear/distortion when the distribution contains isolated regions with
very high probability densities.

There are actually two variants of ``Marginal2D:`` when
``Continuous=false``, discrete marginal/conditional distributions are
used to select a bilinear bilinear patch, followed by a continuous
sampling step that chooses a specific position inside the patch. When
``Continuous=true``, continuous marginal/conditional distributions are
used instead, and the second step is no longer needed. The latter
scheme requires more computation and memory accesses but produces an
overall smoother mapping. The continuous version of ``Marginal2D`` may
be beneficial when this method is not used as a sampling scheme, but
rather to generate very high-quality parameterizations.

======================================================================
=)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_DiscreteDistribution2D = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_DiscreteDistribution2D_2 =
R"doc(Construct a marginal sample warping scheme for floating point data of
resolution ``size``.)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_eval = R"doc(Evaluate the function value at the given integer position)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_m_cond_cdf = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_m_data = R"doc(Density values)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_m_inv_normalization = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_m_marg_cdf = R"doc(Marginal and conditional PDFs)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_m_normalization = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_m_size = R"doc(Resolution of the discretized density function)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_pdf = R"doc(Evaluate the normalized function value at the given integer position)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_sample =
R"doc(Given a uniformly distributed 2D sample, draw a sample from the
distribution

Returns the integer position, the normalized probability value, and
re-uniformized random variate that can be used for further sampling
steps.)doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_to_string = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution2D_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_DiscreteDistribution = R"doc(Create an uninitialized DiscreteDistribution instance)doc";

static const char *__doc_mitsuba_DiscreteDistribution_DiscreteDistribution_2 = R"doc(Initialize from a given probability mass function)doc";

static const char *__doc_mitsuba_DiscreteDistribution_DiscreteDistribution_3 = R"doc(Initialize from a given probability mass function (rvalue version))doc";

static const char *__doc_mitsuba_DiscreteDistribution_DiscreteDistribution_4 = R"doc(Initialize from a given floating point array)doc";

static const char *__doc_mitsuba_DiscreteDistribution_cdf = R"doc(Return the unnormalized cumulative distribution function)doc";

static const char *__doc_mitsuba_DiscreteDistribution_cdf_2 =
R"doc(Return the unnormalized cumulative distribution function (const
version))doc";

static const char *__doc_mitsuba_DiscreteDistribution_compute_cdf = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_compute_cdf_scalar = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_empty = R"doc(Is the distribution object empty/uninitialized?)doc";

static const char *__doc_mitsuba_DiscreteDistribution_eval_cdf =
R"doc(Evaluate the unnormalized cumulative distribution function (CDF) at
index ``index``)doc";

static const char *__doc_mitsuba_DiscreteDistribution_eval_cdf_normalized =
R"doc(Evaluate the normalized cumulative distribution function (CDF) at
index ``index``)doc";

static const char *__doc_mitsuba_DiscreteDistribution_eval_pmf =
R"doc(Evaluate the unnormalized probability mass function (PMF) at index
``index``)doc";

static const char *__doc_mitsuba_DiscreteDistribution_eval_pmf_normalized =
R"doc(Evaluate the normalized probability mass function (PMF) at index
``index``)doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_normalization = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_pmf = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_sum = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_m_valid = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_normalization = R"doc(Return the normalization factor (i.e. the inverse of sum()))doc";

static const char *__doc_mitsuba_DiscreteDistribution_pmf = R"doc(Return the unnormalized probability mass function)doc";

static const char *__doc_mitsuba_DiscreteDistribution_pmf_2 = R"doc(Return the unnormalized probability mass function (const version))doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample =
R"doc(Transform a uniformly distributed sample to the stored distribution

Parameter ``sample``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    The discrete index associated with the sample)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample_pmf =
R"doc(Transform a uniformly distributed sample to the stored distribution

Parameter ``value``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    A tuple consisting of

1. the discrete index associated with the sample, and 2. the
normalized probability value of the sample.)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample_reuse =
R"doc(Transform a uniformly distributed sample to the stored distribution

The original sample is value adjusted so that it can be reused as a
uniform variate.

Parameter ``value``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    A tuple consisting of

1. the discrete index associated with the sample, and 2. the re-scaled
sample value.)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sample_reuse_pmf =
R"doc(Transform a uniformly distributed sample to the stored distribution.

The original sample is value adjusted so that it can be reused as a
uniform variate.

Parameter ``value``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    A tuple consisting of

1. the discrete index associated with the sample 2. the re-scaled
sample value 3. the normalized probability value of the sample)doc";

static const char *__doc_mitsuba_DiscreteDistribution_size = R"doc(Return the number of entries)doc";

static const char *__doc_mitsuba_DiscreteDistribution_sum = R"doc(Return the original sum of PMF entries before normalization)doc";

static const char *__doc_mitsuba_DiscreteDistribution_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_DiscreteDistribution_update = R"doc(Update the internal state. Must be invoked when changing the pmf.)doc";

static const char *__doc_mitsuba_Distribution2D = R"doc(Base class of Hierarchical2D and Marginal2D with common functionality)doc";

static const char *__doc_mitsuba_Distribution2D_Distribution2D = R"doc()doc";

static const char *__doc_mitsuba_Distribution2D_Distribution2D_2 = R"doc()doc";

static const char *__doc_mitsuba_Distribution2D_interpolate_weights = R"doc()doc";

static const char *__doc_mitsuba_Distribution2D_m_inv_patch_size = R"doc(Inverse of the above)doc";

static const char *__doc_mitsuba_Distribution2D_m_param_strides = R"doc(Stride per parameter in units of sizeof(ScalarFloat))doc";

static const char *__doc_mitsuba_Distribution2D_m_param_values = R"doc(Discretization of each parameter domain)doc";

static const char *__doc_mitsuba_Distribution2D_m_patch_size = R"doc(Size of a bilinear patch in the unit square)doc";

static const char *__doc_mitsuba_Distribution2D_m_slices = R"doc(Total number of slices (in case Dimension > 1))doc";

static const char *__doc_mitsuba_Distribution2D_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Distribution2D_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_DummyStream =
R"doc(Stream implementation that never writes to disk, but keeps track of
the size of the content being written. It can be used, for example, to
measure the precise amount of memory needed to store serialized
content.)doc";

static const char *__doc_mitsuba_DummyStream_DummyStream = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_can_read = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_can_write = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_DummyStream_close =
R"doc(Closes the stream. No further read or write operations are permitted.

This function is idempotent. It may be called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_DummyStream_flush = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_DummyStream_m_is_closed = R"doc(Whether the stream has been closed.)doc";

static const char *__doc_mitsuba_DummyStream_m_pos =
R"doc(Current position in the "virtual" stream (even though nothing is ever
written, we need to maintain consistent positioning).)doc";

static const char *__doc_mitsuba_DummyStream_m_size = R"doc(Size of all data written to the stream)doc";

static const char *__doc_mitsuba_DummyStream_read = R"doc(//! @{ \name Implementation of the Stream interface)doc";

static const char *__doc_mitsuba_DummyStream_seek = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_size = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_tell = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_truncate = R"doc()doc";

static const char *__doc_mitsuba_DummyStream_write = R"doc()doc";

static const char *__doc_mitsuba_EOFException = R"doc()doc";

static const char *__doc_mitsuba_EOFException_EOFException = R"doc()doc";

static const char *__doc_mitsuba_EOFException_gcount = R"doc()doc";

static const char *__doc_mitsuba_EOFException_m_gcount = R"doc()doc";

static const char *__doc_mitsuba_Emitter = R"doc()doc";

static const char *__doc_mitsuba_Emitter_2 = R"doc()doc";

static const char *__doc_mitsuba_Emitter_3 = R"doc()doc";

static const char *__doc_mitsuba_Emitter_4 = R"doc()doc";

static const char *__doc_mitsuba_Emitter_5 = R"doc()doc";

static const char *__doc_mitsuba_Emitter_6 = R"doc()doc";

static const char *__doc_mitsuba_EmitterFlags =
R"doc(This list of flags is used to classify the different types of
emitters.)doc";

static const char *__doc_mitsuba_EmitterFlags_Delta = R"doc(Delta function in either position or direction)doc";

static const char *__doc_mitsuba_EmitterFlags_DeltaDirection = R"doc(The emitter emits light in a single direction)doc";

static const char *__doc_mitsuba_EmitterFlags_DeltaPosition = R"doc(The emitter lies at a single point in space)doc";

static const char *__doc_mitsuba_EmitterFlags_Empty = R"doc(No flags set (default value))doc";

static const char *__doc_mitsuba_EmitterFlags_Infinite = R"doc(The emitter is placed at infinity (e.g. environment maps))doc";

static const char *__doc_mitsuba_EmitterFlags_SpatiallyVarying = R"doc(The emission depends on the UV coordinates)doc";

static const char *__doc_mitsuba_EmitterFlags_Surface = R"doc(The emitter is attached to a surface (e.g. area emitters))doc";

static const char *__doc_mitsuba_Emitter_Emitter = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Emitter_class_name = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Emitter_dirty = R"doc(Return whether the emitter parameters have changed)doc";

static const char *__doc_mitsuba_Emitter_flags = R"doc(Flags for all components combined.)doc";

static const char *__doc_mitsuba_Emitter_is_environment = R"doc(Is this an environment map light emitter?)doc";

static const char *__doc_mitsuba_Emitter_m_dirty = R"doc(True if the emitter's parameters have changed)doc";

static const char *__doc_mitsuba_Emitter_m_flags = R"doc(Combined flags for all properties of this emitter.)doc";

static const char *__doc_mitsuba_Emitter_m_sampling_weight = R"doc(Sampling weight)doc";

static const char *__doc_mitsuba_Emitter_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_Emitter_sampling_weight = R"doc(The emitter's sampling weight.)doc";

static const char *__doc_mitsuba_Emitter_set_dirty = R"doc(Modify the emitter's "dirty" flag)doc";

static const char *__doc_mitsuba_Emitter_traverse = R"doc()doc";

static const char *__doc_mitsuba_Emitter_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Emitter_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Emitter_type = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Emitter_variant_name = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Endpoint =
R"doc(Abstract interface subsuming emitters and sensors in Mitsuba.

This class provides an abstract interface to emitters and sensors in
Mitsuba, which are named *endpoints* since they represent the first
and last vertices of a light path. Thanks to symmetries underlying the
equations of light transport and scattering, sensors and emitters can
be treated as essentially the same thing, their main difference being
type of emitted radiation: light sources emit *radiance*, while
sensors emit a conceptual radiation named *importance*. This class
casts these symmetries into a unified API that enables access to both
types of endpoints using the same set of functions.

Subclasses of this interface must implement functions to evaluate and
sample the emission/response profile, and to compute probability
densities associated with the provided sampling techniques.

In addition to :py:meth:`mitsuba.Endpoint.sample_ray`, which generates
a sample from the profile, subclasses also provide a specialized
*direction sampling* method in
:py:meth:`mitsuba.Endpoint.sample_direction`. This is a generalization
of direct illumination techniques to both emitters *and* sensors. A
direction sampling method is given an arbitrary reference position in
the scene and samples a direction from the reference point towards the
endpoint (ideally proportional to the emission/sensitivity profile).
This reduces the sampling domain from 4D to 2D, which often enables
the construction of smarter specialized sampling techniques.

When rendering scenes involving participating media, it is important
to know what medium surrounds the sensors and emitters. For this
reason, every endpoint instance keeps a reference to a medium (which
may be set to ``nullptr`` when the endpoint is surrounded by vacuum).

In the context of polarized simulation, the perfect symmetry between
emitters and sensors technically breaks down: the former emit 4D
*Stokes vectors* encoding the polarization state of light, while
sensors are characterized by 4x4 *Mueller matrices* that transform the
incident polarization prior to measurement. We sidestep this non-
symmetry by simply using Mueller matrices everywhere: in the case of
emitters, only the first column will be used (the remainder being
filled with zeros). This API simplification comes at a small extra
cost in terms of register usage and arithmetic. The JIT (LLVM, CUDA)
variants of Mitsuba can recognize these redundancies and remove them
retroactively.)doc";

static const char *__doc_mitsuba_Endpoint_2 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_3 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_4 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_5 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_6 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_Endpoint = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_Endpoint_2 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_bbox = R"doc(Return an axis-aligned box bounding the spatial extents of the emitter)doc";

static const char *__doc_mitsuba_Endpoint_class_name = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_eval =
R"doc(Given a ray-surface intersection, return the emitted radiance or
importance traveling along the reverse direction

This function is e.g. used when an area light source has been hit by a
ray in a path tracing-style integrator, and it subsequently needs to
be queried for the emitted radiance along the negative ray direction.
The default implementation throws an exception, which states that the
method is not implemented.

Parameter ``si``:
    An intersect record that specifies both the query position and
    direction (using the ``si.wi`` field)

Returns:
    The emitted radiance or importance)doc";

static const char *__doc_mitsuba_Endpoint_eval_direction =
R"doc(Re-evaluate the incident direct radiance/importance of the
sample_direction() method.

This function re-evaluates the incident direct radiance or importance
and sample probability due to the endpoint so that division by
``ds.pdf`` equals the sampling weight returned by sample_direction().
This may appear redundant, and indeed such a function would not find
use in "normal" rendering algorithms.

However, the ability to re-evaluate the contribution of a generated
sample is important for differentiable rendering. For example, we
might want to track derivatives in the sampled direction (``ds.d``)
without also differentiating the sampling technique.

In contrast to pdf_direction(), evaluating this function can yield a
nonzero result in the case of emission profiles containing a Dirac
delta term (e.g. point or directional lights).

Parameter ``ref``:
    A 3D reference location within the scene, which may influence the
    sampling process.

Parameter ``ds``:
    A direction sampling record, which specifies the query location.

Returns:
    The incident direct radiance/importance associated with the
    sample.)doc";

static const char *__doc_mitsuba_Endpoint_m_medium = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_m_needs_sample_2 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_m_needs_sample_3 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_m_shape = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_m_to_world = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_medium = R"doc(Return a pointer to the medium that surrounds the emitter)doc";

static const char *__doc_mitsuba_Endpoint_medium_2 =
R"doc(Return a pointer to the medium that surrounds the emitter (const
version))doc";

static const char *__doc_mitsuba_Endpoint_needs_sample_2 =
R"doc(Does the method sample_ray() require a uniformly distributed 2D sample
for the ``sample2`` parameter?)doc";

static const char *__doc_mitsuba_Endpoint_needs_sample_3 =
R"doc(Does the method sample_ray() require a uniformly distributed 2D sample
for the ``sample3`` parameter?)doc";

static const char *__doc_mitsuba_Endpoint_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_pdf_direction =
R"doc(Evaluate the probability density of the *direct* sampling method
implemented by the sample_direction() method.

The returned probability will always be zero when the
emission/sensitivity profile contains a Dirac delta term (e.g. point
or directional emitters/sensors).

Parameter ``ds``:
    A direct sampling record, which specifies the query location.)doc";

static const char *__doc_mitsuba_Endpoint_pdf_position =
R"doc(Evaluate the probability density of the position sampling method
implemented by sample_position().

In simple cases, this will be the reciprocal of the endpoint's surface
area.

Parameter ``ps``:
    The sampled position record.

Returns:
    The corresponding sampling density.)doc";

static const char *__doc_mitsuba_Endpoint_pdf_wavelengths =
R"doc(Evaluate the probability density of the wavelength sampling method
implemented by sample_wavelengths().

Parameter ``wavelengths``:
    The sampled wavelengths.

Returns:
    The corresponding sampling density per wavelength (units of 1/nm).)doc";

static const char *__doc_mitsuba_Endpoint_sample_direction =
R"doc(Given a reference point in the scene, sample a direction from the
reference point towards the endpoint (ideally proportional to the
emission/sensitivity profile)

This operation is a generalization of direct illumination techniques
to both emitters *and* sensors. A direction sampling method is given
an arbitrary reference position in the scene and samples a direction
from the reference point towards the endpoint (ideally proportional to
the emission/sensitivity profile). This reduces the sampling domain
from 4D to 2D, which often enables the construction of smarter
specialized sampling techniques.

Ideally, the implementation should importance sample the product of
the emission profile and the geometry term between the reference point
and the position on the endpoint.

The default implementation throws an exception.

Parameter ``ref``:
    A reference position somewhere within the scene.

Parameter ``sample``:
    A uniformly distributed 2D point on the domain ``[0,1]^2``.

Returns:
    A DirectionSample instance describing the generated sample along
    with a spectral importance weight.)doc";

static const char *__doc_mitsuba_Endpoint_sample_position =
R"doc(Importance sample the spatial component of the emission or importance
profile of the endpoint.

The default implementation throws an exception.

Parameter ``time``:
    The scene time associated with the position to be sampled.

Parameter ``sample``:
    A uniformly distributed 2D point on the domain ``[0,1]^2``.

Returns:
    A PositionSample instance describing the generated sample along
    with an importance weight.)doc";

static const char *__doc_mitsuba_Endpoint_sample_ray =
R"doc(Importance sample a ray proportional to the endpoint's
sensitivity/emission profile.

The endpoint profile is a six-dimensional quantity that depends on
time, wavelength, surface position, and direction. This function takes
a given time value and five uniformly distributed samples on the
interval [0, 1] and warps them so that the returned ray follows the
profile. Any discrepancies between ideal and actual sampled profile
are absorbed into a spectral importance weight that is returned along
with the ray.

Parameter ``time``:
    The scene time associated with the ray to be sampled

Parameter ``sample1``:
    A uniformly distributed 1D value that is used to sample the
    spectral dimension of the emission profile.

Parameter ``sample2``:
    A uniformly distributed sample on the domain ``[0,1]^2``. For
    sensor endpoints, this argument corresponds to the sample position
    in fractional pixel coordinates relative to the crop window of the
    underlying film. This argument is ignored if ``needs_sample_2() ==
    false``.

Parameter ``sample3``:
    A uniformly distributed sample on the domain ``[0,1]^2``. For
    sensor endpoints, this argument determines the position on the
    aperture of the sensor. This argument is ignored if
    ``needs_sample_3() == false``.

Returns:
    The sampled ray and (potentially spectrally varying) importance
    weights. The latter account for the difference between the profile
    and the actual used sampling density function.)doc";

static const char *__doc_mitsuba_Endpoint_sample_wavelengths =
R"doc(Importance sample a set of wavelengths according to the endpoint's
sensitivity/emission spectrum.

This function takes a uniformly distributed 1D sample and generates a
sample that is approximately distributed according to the endpoint's
spectral sensitivity/emission profile.

For this, the input 1D sample is first replicated into
``Spectrum::Size`` separate samples using simple arithmetic
transformations (see math::sample_shifted()), which can be interpreted
as a type of Quasi-Monte-Carlo integration scheme. Following this, a
standard technique (e.g. inverse transform sampling) is used to find
the corresponding wavelengths. Any discrepancies between ideal and
actual sampled profile are absorbed into a spectral importance weight
that is returned along with the wavelengths.

This function should not be called in RGB or monochromatic modes.

Parameter ``si``:
    In the case of a spatially-varying spectral sensitivity/emission
    profile, this parameter conditions sampling on a specific spatial
    position. The ``si.uv`` field must be specified in this case.

Parameter ``sample``:
    A 1D uniformly distributed random variate

Returns:
    The set of sampled wavelengths and (potentially spectrally
    varying) importance weights. The latter account for the difference
    between the profile and the actual used sampling density function.
    In the case of emitters, the weight will include the emitted
    radiance.)doc";

static const char *__doc_mitsuba_Endpoint_set_medium = R"doc(Set the medium that surrounds the emitter.)doc";

static const char *__doc_mitsuba_Endpoint_set_scene =
R"doc(Inform the emitter about the properties of the scene

Various emitters that surround the scene (e.g. environment emitters)
must be informed about the scene dimensions to operate correctly. This
function is invoked by the Scene constructor.)doc";

static const char *__doc_mitsuba_Endpoint_set_shape = R"doc(Set the shape associated with this endpoint.)doc";

static const char *__doc_mitsuba_Endpoint_shape = R"doc(Return the shape to which the emitter is currently attached)doc";

static const char *__doc_mitsuba_Endpoint_shape_2 =
R"doc(Return the shape to which the emitter is currently attached (const
version))doc";

static const char *__doc_mitsuba_Endpoint_traverse = R"doc(//! @})doc";

static const char *__doc_mitsuba_Endpoint_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Endpoint_world_transform = R"doc(Return the local space to world space transformation)doc";

static const char *__doc_mitsuba_FileResolver =
R"doc(Simple class for resolving paths on Linux/Windows/Mac OS

This convenience class looks for a file or directory given its name
and a set of search paths. The implementation walks through the search
paths in order and stops once the file is found.)doc";

static const char *__doc_mitsuba_FileResolver_FileResolver = R"doc(Initialize a new file resolver with the current working directory)doc";

static const char *__doc_mitsuba_FileResolver_FileResolver_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_FileResolver_append = R"doc(Append an entry to the end of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_begin = R"doc(Return an iterator at the beginning of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_begin_2 =
R"doc(Return an iterator at the beginning of the list of search paths
(const))doc";

static const char *__doc_mitsuba_FileResolver_class_name = R"doc()doc";

static const char *__doc_mitsuba_FileResolver_clear = R"doc(Clear the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_contains = R"doc(Check if a given path is included in the search path list)doc";

static const char *__doc_mitsuba_FileResolver_end = R"doc(Return an iterator at the end of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_end_2 = R"doc(Return an iterator at the end of the list of search paths (const))doc";

static const char *__doc_mitsuba_FileResolver_erase = R"doc(Erase the entry at the given iterator position)doc";

static const char *__doc_mitsuba_FileResolver_erase_2 = R"doc(Erase the search path from the list)doc";

static const char *__doc_mitsuba_FileResolver_m_paths = R"doc()doc";

static const char *__doc_mitsuba_FileResolver_operator_array = R"doc(Return an entry from the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_operator_array_2 = R"doc(Return an entry from the list of search paths (const))doc";

static const char *__doc_mitsuba_FileResolver_prepend = R"doc(Prepend an entry at the beginning of the list of search paths)doc";

static const char *__doc_mitsuba_FileResolver_resolve =
R"doc(Walk through the list of search paths and try to resolve the input
path)doc";

static const char *__doc_mitsuba_FileResolver_size = R"doc(Return the number of search paths)doc";

static const char *__doc_mitsuba_FileResolver_to_string = R"doc(Return a human-readable representation of this instance)doc";

static const char *__doc_mitsuba_FileStream =
R"doc(Simple Stream implementation backed-up by a file.

The underlying file abstraction is ``std::fstream``, and so most
operations can be expected to behave similarly.)doc";

static const char *__doc_mitsuba_FileStream_EMode = R"doc()doc";

static const char *__doc_mitsuba_FileStream_EMode_ERead = R"doc(Opens a file in (binary) read-only mode)doc";

static const char *__doc_mitsuba_FileStream_EMode_EReadWrite = R"doc(Opens (but never creates) a file in (binary) read-write mode)doc";

static const char *__doc_mitsuba_FileStream_EMode_ETruncReadWrite = R"doc(Opens (and truncates) a file in (binary) read-write mode)doc";

static const char *__doc_mitsuba_FileStream_FileStream =
R"doc(Constructs a new FileStream by opening the file pointed by ``p``.

The file is opened in read-only or read/write mode as specified by
``mode``.

Throws if trying to open a non-existing file in with write disabled.
Throws an exception if the file cannot be opened / created.)doc";

static const char *__doc_mitsuba_FileStream_can_read = R"doc(True except if the stream was closed.)doc";

static const char *__doc_mitsuba_FileStream_can_write = R"doc(Whether the field was open in write-mode (and was not closed))doc";

static const char *__doc_mitsuba_FileStream_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_FileStream_close =
R"doc(Closes the stream and the underlying file. No further read or write
operations are permitted.

This function is idempotent. It is called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_FileStream_flush = R"doc(Flushes any buffered operation to the underlying file.)doc";

static const char *__doc_mitsuba_FileStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_FileStream_m_file = R"doc()doc";

static const char *__doc_mitsuba_FileStream_m_mode = R"doc(//! @})doc";

static const char *__doc_mitsuba_FileStream_m_path = R"doc()doc";

static const char *__doc_mitsuba_FileStream_native = R"doc(Return the "native" std::fstream associated with this FileStream)doc";

static const char *__doc_mitsuba_FileStream_path = R"doc(Return the path descriptor associated with this FileStream)doc";

static const char *__doc_mitsuba_FileStream_read =
R"doc(Reads a specified amount of data from the stream. Throws an exception
when the stream ended prematurely.)doc";

static const char *__doc_mitsuba_FileStream_read_line = R"doc(Convenience function for reading a line of text from an ASCII file)doc";

static const char *__doc_mitsuba_FileStream_seek =
R"doc(Seeks to a position inside the stream. May throw if the resulting
state is invalid.)doc";

static const char *__doc_mitsuba_FileStream_size =
R"doc(Returns the size of the file. \note After a write, the size may not be
updated until a flush is performed.)doc";

static const char *__doc_mitsuba_FileStream_tell = R"doc(Gets the current position inside the file)doc";

static const char *__doc_mitsuba_FileStream_to_string = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_FileStream_truncate =
R"doc(Truncates the file to a given size. Automatically flushes the stream
before truncating the file. The position is updated to
``min(old_position, size)``.

Throws an exception if in read-only mode.)doc";

static const char *__doc_mitsuba_FileStream_write =
R"doc(Writes a specified amount of data into the stream. Throws an exception
when not all data could be written.)doc";

static const char *__doc_mitsuba_Film =
R"doc(Abstract film base class - used to store samples generated by
Integrator implementations.

To avoid lock-related bottlenecks when rendering with many cores,
rendering threads first store results in an "image block", which is
then committed to the film using the put() method.)doc";

static const char *__doc_mitsuba_Film_2 = R"doc()doc";

static const char *__doc_mitsuba_Film_3 = R"doc()doc";

static const char *__doc_mitsuba_Film_4 = R"doc()doc";

static const char *__doc_mitsuba_Film_5 = R"doc()doc";

static const char *__doc_mitsuba_Film_6 = R"doc()doc";

static const char *__doc_mitsuba_FilmFlags = R"doc(This list of flags is used to classify the different types of films.)doc";

static const char *__doc_mitsuba_FilmFlags_Alpha = R"doc(The film stores an alpha channel)doc";

static const char *__doc_mitsuba_FilmFlags_Empty = R"doc(No flags set (default value))doc";

static const char *__doc_mitsuba_FilmFlags_Special =
R"doc(The film provides a customized prepare_sample() routine that
implements a special treatment of the samples before storing them in
the Image Block.)doc";

static const char *__doc_mitsuba_FilmFlags_Spectral = R"doc(The film stores a spectral representation of the image)doc";

static const char *__doc_mitsuba_Film_Film = R"doc(Create a film)doc";

static const char *__doc_mitsuba_Film_base_channels_count = R"doc(Return the number of channels for the developed image (excluding AOVs))doc";

static const char *__doc_mitsuba_Film_bitmap = R"doc(Return a bitmap object storing the developed contents of the film)doc";

static const char *__doc_mitsuba_Film_class_name = R"doc()doc";

static const char *__doc_mitsuba_Film_clear = R"doc(Clear the film contents to zero.)doc";

static const char *__doc_mitsuba_Film_create_block =
R"doc(Return an ImageBlock instance, whose internal representation is
compatible with that of the film.

Image blocks created using this method can later be merged into the
film using put_block().

Parameter ``size``:
    Desired size of the returned image block.

Parameter ``normalize``:
    Force normalization of filter weights in ImageBlock::put()? See
    the ImageBlock constructor for details.

Parameter ``border``:
    Should ``ImageBlock`` add an additional border region around
    around the image boundary? See the ImageBlock constructor for
    details.)doc";

static const char *__doc_mitsuba_Film_crop_offset = R"doc(Return the offset of the crop window)doc";

static const char *__doc_mitsuba_Film_crop_size = R"doc(Return the size of the crop window)doc";

static const char *__doc_mitsuba_Film_develop = R"doc(Return a image buffer object storing the developed image)doc";

static const char *__doc_mitsuba_Film_flags = R"doc(Flags for all properties combined.)doc";

static const char *__doc_mitsuba_Film_m_crop_offset = R"doc()doc";

static const char *__doc_mitsuba_Film_m_crop_size = R"doc()doc";

static const char *__doc_mitsuba_Film_m_filter = R"doc()doc";

static const char *__doc_mitsuba_Film_m_flags = R"doc(Combined flags for all properties of this film.)doc";

static const char *__doc_mitsuba_Film_m_sample_border = R"doc()doc";

static const char *__doc_mitsuba_Film_m_size = R"doc()doc";

static const char *__doc_mitsuba_Film_m_srf = R"doc()doc";

static const char *__doc_mitsuba_Film_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_Film_prepare =
R"doc(Configure the film for rendering a specified set of extra channels
(AOVs). Returns the total number of channels that the film will store)doc";

static const char *__doc_mitsuba_Film_prepare_sample =
R"doc(Prepare spectrum samples to be in the format expected by the film

It will be used if the Film contains the ``Special`` flag enabled.

This method should be applied with films that deviate from HDR film
behavior. Normally ``Films`` will store within the ``ImageBlock`` the
samples following an RGB shape. But ``Films`` may want to store the
samples with other structures (e.g. store several channels containing
monochromatic information). In that situation, this method allows
transforming the sample format generated by the integrators to the one
that the Film will store inside the ImageBlock.

Parameter ``spec``:
    Sample value associated with the specified wavelengths

Parameter ``wavelengths``:
    Sample wavelengths in nanometers

Parameter ``aovs``:
    Points to an array of length equal to the number of spectral
    sensitivities of the film, which specifies the sample value for
    each channel.

Parameter ``weight``:
    Value to be added to the weight channel of the sample

Parameter ``alpha``:
    Alpha value of the sample

Parameter ``active``:
    Mask indicating if the lanes are active)doc";

static const char *__doc_mitsuba_Film_put_block =
R"doc(Merge an image block into the film. This methods should be thread-
safe.)doc";

static const char *__doc_mitsuba_Film_rfilter = R"doc(Return the image reconstruction filter (const version))doc";

static const char *__doc_mitsuba_Film_sample_border =
R"doc(Should regions slightly outside the image plane be sampled to improve
the quality of the reconstruction at the edges? This only makes sense
when reconstruction filters other than the box filter are used.)doc";

static const char *__doc_mitsuba_Film_schedule_storage = R"doc(dr::schedule() variables that represent the internal film storage)doc";

static const char *__doc_mitsuba_Film_sensor_response_function = R"doc(Returns the specific Sensor Response Function (SRF) used by the film)doc";

static const char *__doc_mitsuba_Film_set_crop_window =
R"doc(Set the size and offset of the film's crop window.

The caller is responsible to invoke the parameters_changed() method on
any sensor referring to this film.

This function will throw an exception if crop_offset + crop_size >
size.

Parameter ``crop_offset``:
    The offset of the crop window.

Parameter ``crop_size``:
    The size of the crop window.)doc";

static const char *__doc_mitsuba_Film_set_size =
R"doc(Set the size of the film.

This method will reset the crop window to the full image.

The caller is responsible to invoke the parameters_changed() method on
any sensor referring to this film.

Parameter ``size``:
    The new size of the film.)doc";

static const char *__doc_mitsuba_Film_size =
R"doc(Ignoring the crop window, return the resolution of the underlying
sensor)doc";

static const char *__doc_mitsuba_Film_to_string = R"doc(//! @})doc";

static const char *__doc_mitsuba_Film_traverse = R"doc()doc";

static const char *__doc_mitsuba_Film_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_Film_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Film_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Film_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Film_type = R"doc()doc";

static const char *__doc_mitsuba_Film_variant_name = R"doc()doc";

static const char *__doc_mitsuba_Film_write = R"doc(Write the developed contents of the film to a file on disk)doc";

static const char *__doc_mitsuba_FilterBoundaryCondition =
R"doc(When resampling data to a different resolution using
Resampler::resample(), this enumeration specifies how lookups
*outside* of the input domain are handled.

See also:
    Resampler)doc";

static const char *__doc_mitsuba_FilterBoundaryCondition_Clamp = R"doc(Clamp to the outermost sample position (default))doc";

static const char *__doc_mitsuba_FilterBoundaryCondition_Mirror = R"doc(Assume that the input is mirrored along the boundary)doc";

static const char *__doc_mitsuba_FilterBoundaryCondition_One =
R"doc(Assume that the input function is equal to one outside of the defined
domain)doc";

static const char *__doc_mitsuba_FilterBoundaryCondition_Repeat = R"doc(Assume that the input repeats in a periodic fashion)doc";

static const char *__doc_mitsuba_FilterBoundaryCondition_Zero = R"doc(Assume that the input function is zero outside of the defined domain)doc";

static const char *__doc_mitsuba_Formatter =
R"doc(Abstract interface for converting log information into a human-
readable format)doc";

static const char *__doc_mitsuba_Formatter_class_name = R"doc()doc";

static const char *__doc_mitsuba_Formatter_format =
R"doc(Turn a log message into a human-readable format

Parameter ``level``:
    The importance of the debug message

Parameter ``cname``:
    Name of the class (if present)

Parameter ``fname``:
    Source location (file)

Parameter ``line``:
    Source location (line number)

Parameter ``msg``:
    Text content associated with the log message)doc";

static const char *__doc_mitsuba_Frame =
R"doc(Stores a three-dimensional orthonormal coordinate frame

This class is used to convert between different cartesian coordinate
systems and to efficiently evaluate trigonometric functions in a
spherical coordinate system whose pole is aligned with the ``n`` axis
(e.g. cos_theta(), sin_phi(), etc.).)doc";

static const char *__doc_mitsuba_Frame_Frame = R"doc(Construct a new coordinate frame from a single vector)doc";

static const char *__doc_mitsuba_Frame_Frame_2 = R"doc(Construct a new coordinate frame from three vectors)doc";

static const char *__doc_mitsuba_Frame_Frame_3 = R"doc()doc";

static const char *__doc_mitsuba_Frame_Frame_4 = R"doc()doc";

static const char *__doc_mitsuba_Frame_Frame_5 = R"doc()doc";

static const char *__doc_mitsuba_Frame_cos_phi =
R"doc(Give a unit direction, this function returns the cosine of the azimuth
in a reference spherical coordinate system (see the Frame description))doc";

static const char *__doc_mitsuba_Frame_cos_phi_2 =
R"doc(Give a unit direction, this function returns the squared cosine of the
azimuth in a reference spherical coordinate system (see the Frame
description))doc";

static const char *__doc_mitsuba_Frame_cos_theta =
R"doc(Give a unit direction, this function returns the cosine of the
elevation angle in a reference spherical coordinate system (see the
Frame description))doc";

static const char *__doc_mitsuba_Frame_cos_theta_2 =
R"doc(Give a unit direction, this function returns the square cosine of the
elevation angle in a reference spherical coordinate system (see the
Frame description))doc";

static const char *__doc_mitsuba_Frame_fields = R"doc()doc";

static const char *__doc_mitsuba_Frame_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Frame_labels = R"doc()doc";

static const char *__doc_mitsuba_Frame_n = R"doc()doc";

static const char *__doc_mitsuba_Frame_name = R"doc()doc";

static const char *__doc_mitsuba_Frame_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Frame_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Frame_operator_eq = R"doc(Equality test)doc";

static const char *__doc_mitsuba_Frame_operator_ne = R"doc(Inequality test)doc";

static const char *__doc_mitsuba_Frame_s = R"doc()doc";

static const char *__doc_mitsuba_Frame_sin_phi =
R"doc(Give a unit direction, this function returns the sine of the azimuth
in a reference spherical coordinate system (see the Frame description))doc";

static const char *__doc_mitsuba_Frame_sin_phi_2 =
R"doc(Give a unit direction, this function returns the squared sine of the
azimuth in a reference spherical coordinate system (see the Frame
description))doc";

static const char *__doc_mitsuba_Frame_sin_theta =
R"doc(Give a unit direction, this function returns the sine of the elevation
angle in a reference spherical coordinate system (see the Frame
description))doc";

static const char *__doc_mitsuba_Frame_sin_theta_2 =
R"doc(Give a unit direction, this function returns the square sine of the
elevation angle in a reference spherical coordinate system (see the
Frame description))doc";

static const char *__doc_mitsuba_Frame_sincos_phi =
R"doc(Give a unit direction, this function returns the sine and cosine of
the azimuth in a reference spherical coordinate system (see the Frame
description))doc";

static const char *__doc_mitsuba_Frame_sincos_phi_2 =
R"doc(Give a unit direction, this function returns the squared sine and
cosine of the azimuth in a reference spherical coordinate system (see
the Frame description))doc";

static const char *__doc_mitsuba_Frame_t = R"doc()doc";

static const char *__doc_mitsuba_Frame_tan_theta =
R"doc(Give a unit direction, this function returns the tangent of the
elevation angle in a reference spherical coordinate system (see the
Frame description))doc";

static const char *__doc_mitsuba_Frame_tan_theta_2 =
R"doc(Give a unit direction, this function returns the square tangent of the
elevation angle in a reference spherical coordinate system (see the
Frame description))doc";

static const char *__doc_mitsuba_Frame_to_local = R"doc(Convert from world coordinates to local coordinates)doc";

static const char *__doc_mitsuba_Frame_to_world = R"doc(Convert from local coordinates to world coordinates)doc";

static const char *__doc_mitsuba_GPUTexture =
R"doc(Defines an abstraction for textures that works with OpenGL, OpenGL ES,
and Metal.

Wraps nanogui::Texture and adds a new constructor for creating
textures from mitsuba::Bitmap instances.)doc";

static const char *__doc_mitsuba_GPUTexture_GPUTexture = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D =
R"doc(Implements a hierarchical sample warping scheme for 2D distributions
with linear interpolation and an optional dependence on additional
parameters

This class takes a rectangular floating point array as input and
constructs internal data structures to efficiently map uniform
variates from the unit square ``[0, 1]^2`` to a function on ``[0,
1]^2`` that linearly interpolates the input array.

The mapping is constructed from a sequence of ``log2(max(res))``
hierarchical sample warping steps, where ``res`` is the input array
resolution. It is bijective and generally very well-behaved (i.e. low
distortion), which makes it a good choice for structured point sets
such as the Halton or Sobol sequence.

The implementation also supports *conditional distributions*, i.e. 2D
distributions that depend on an arbitrary number of parameters
(indicated via the ``Dimension`` template parameter).

In this case, the input array should have dimensions ``N0 x N1 x ... x
Nn x res.y() x res.x()`` (where the last dimension is contiguous in
memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
}``, and ``param_values`` should contain the parameter values where
the distribution is discretized. Linear interpolation is used when
sampling or evaluating the distribution for in-between parameter
values.

Remark:
    The Python API exposes explicitly instantiated versions of this
    class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
    for data that depends on 0, 1, and 2 parameters, respectively.)doc";

static const char *__doc_mitsuba_Hierarchical2D_Hierarchical2D = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Hierarchical2D_2 =
R"doc(Construct a hierarchical sample warping scheme for floating point data
of resolution ``size``.

``param_res`` and ``param_values`` are only needed for conditional
distributions (see the text describing the Hierarchical2D class).

If ``normalize`` is set to ``False``, the implementation will not re-
scale the distribution so that it integrates to ``1``. It can still be
sampled (proportionally), but returned density values will reflect the
unnormalized values.

If ``enable_sampling`` is set to ``False``, the implementation will
not construct the hierarchy needed for sample warping, which saves
memory in case this functionality is not needed (e.g. if only the
interpolation in ``eval()`` is used). In this case, ``sample()`` and
``invert()`` can still be called without triggering undefined
behavior, but they will not return meaningful results.)doc";

static const char *__doc_mitsuba_Hierarchical2D_Level = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_Level = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_Level_2 = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_Level_3 = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_Level_4 = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_data = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_fields = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_index =
R"doc(Convert from 2D pixel coordinates to an index indicating how the data
is laid out in memory.

The implementation stores 2x2 patches contiguously in memory to
improve cache locality during hierarchical traversals)doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_labels = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_lookup = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_name = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_ready = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_size = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_Level_width = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_eval =
R"doc(Evaluate the density at position ``pos``. The distribution is
parameterized by ``param`` if applicable.)doc";

static const char *__doc_mitsuba_Hierarchical2D_invert = R"doc(Inverse of the mapping implemented in ``sample()``)doc";

static const char *__doc_mitsuba_Hierarchical2D_m_levels = R"doc(MIP hierarchy over linearly interpolated patches)doc";

static const char *__doc_mitsuba_Hierarchical2D_m_max_patch_index = R"doc(Number of bilinear patches in the X/Y dimension - 1)doc";

static const char *__doc_mitsuba_Hierarchical2D_sample =
R"doc(Given a uniformly distributed 2D sample, draw a sample from the
distribution (parameterized by ``param`` if applicable)

Returns the warped sample and associated probability density.)doc";

static const char *__doc_mitsuba_Hierarchical2D_to_string = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Hierarchical2D_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_IOREntry = R"doc()doc";

static const char *__doc_mitsuba_IOREntry_name = R"doc()doc";

static const char *__doc_mitsuba_IOREntry_value = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock =
R"doc(Intermediate storage for an image or image sub-region being rendered

This class facilitates parallel rendering of images in both scalar and
JIT-based variants of Mitsuba.

In scalar mode, image blocks represent independent rectangular image
regions that are simultaneously processed by worker threads. They are
finally merged into a master ImageBlock controlled by the Film
instance via the put_block() method. The smaller image blocks can
include a border region storing contributions that are slightly
outside of the block, which is required to correctly account for image
reconstruction filters.

In JIT variants there is only a single ImageBlock, whose contents are
computed in parallel. A border region is usually not needed in this
case.

In addition to receiving samples via the put() method, the image block
can also be queried via the read() method, in which case the
reconstruction filter is used to compute suitable interpolation
weights. This is feature is useful for differentiable rendering, where
one needs to evaluate the reverse-mode derivative of the put() method.)doc";

static const char *__doc_mitsuba_ImageBlock_2 = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_3 = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_4 = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_5 = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_6 = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_ImageBlock =
R"doc(Construct a zero-initialized image block with the desired shape and
channel count

Parameter ``size``:
    Specifies the desired horizontal and vertical block size. This
    value excludes additional border pixels that ``ImageBlock`` will
    internally add to support image reconstruction filters (if
    ``border=true`` and a reconstruction filter is furthermore
    specified)

Parameter ``offset``:
    Specifies the offset in case this block represents sub-region of a
    larger image. Otherwise, this can be set to zero.

Parameter ``channel_count``:
    Specifies the desired number of image channels.

Parameter ``rfilter``:
    The desired reconstruction filter to be used in read() and put().
    A box filter will be used if when ``rfilter==nullptr``.

Parameter ``border``:
    Should ``ImageBlock`` add an additional border region around
    around the image boundary to capture contributions to neighboring
    pixels caused by a nontrivial (non-box) reconstruction filter?
    This is mainly useful when a larger image is partitioned into
    smaller blocks that are rendered in parallel. Enabled by default
    in scalar variants.

Parameter ``normalize``:
    This parameter affects the behavior of read() and put().

If set to ``True``, each call to put() explicitly normalizes the
computed filter weights so that the operation adds a unit amount of
energy to the image. This is useful for methods like particle tracing
that invoke put() with an arbitrary distribution of image positions.
Other methods (e.g., path tracing) that uniformly sample positions in
image space should set this parameter to ``False``, since the image
block contents will eventually be divided by a dedicated channel
tracking the accumulated sample weight to remove any non-uniformity.

Furthermore, if ``normalize`` is set to ``True``, the read() operation
will normalize the filter weights to compute a convex combination of
pixel values.

Disabled by default.

Parameter ``coalesce``:
    This parameter is only relevant for JIT variants, where it subtly
    affects the behavior of the performance-critical put() method.

In coalesced mode, put() conservatively bounds the footprint and
traverses it in lockstep across the whole wavefront. This causes
unnecessary atomic memory operations targeting pixels with a zero
filter weight. At the same time, this greatly reduces thread
divergence and can lead to significant speedups when put() writes to
pixels in a regular (e.g., scanline) order. Coalesced mode is
preferable for rendering algorithms like path tracing that uniformly
generate samples within pixels on the sensor.

In contrast, non-coalesced mode is preferable when the input positions
are random and will in any case be subject to thread divergence (e.g.
in a particle tracer that makes random connections to the sensor).

Parameter ``compensate``:
    If set to ``True``, the implementation internally switches to
    Kahan-style error-compensated floating point accumulation. This is
    useful when accumulating many samples into a single precision
    image block. Note that this is currently only supported in JIT
    modes, and that it can make the accumulation quite a bit more
    expensive. The default is ``False``.

Parameter ``warn_negative``:
    If set to ``True``, put() will warn when writing samples with
    negative components. This test is only enabled in scalar variants
    by default, since checking/error reporting is relatively costly in
    JIT-compiled modes.

Parameter ``warn_invalid``:
    If set to ``True``, put() will warn when writing samples with NaN
    (not a number) or positive/negative infinity component values.
    This test is only enabled in scalar variants by default, since
    checking/error reporting is relatively costly in JIT-compiled
    modes.)doc";

static const char *__doc_mitsuba_ImageBlock_ImageBlock_2 =
R"doc(Construct an image block from an existing image tensor

In contrast to the above constructor, this one infers the block size
and channel count from a provided 3D image tensor of shape ``(height,
width, channels)``. It then initializes the image block contents with
a copy of the tensor. This is useful for differentiable rendering
phases that want to query an existing image using a pixel filter via
the read() function.

See the other constructor for an explanation of the parameters.)doc";

static const char *__doc_mitsuba_ImageBlock_accum = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_border_size = R"doc(Return the border region used by the reconstruction filter)doc";

static const char *__doc_mitsuba_ImageBlock_channel_count = R"doc(Return the number of channels stored by the image block)doc";

static const char *__doc_mitsuba_ImageBlock_class_name = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_clear = R"doc(Clear the image block contents to zero.)doc";

static const char *__doc_mitsuba_ImageBlock_coalesce = R"doc(Try to coalesce reads/writes in JIT modes?)doc";

static const char *__doc_mitsuba_ImageBlock_compensate = R"doc(Use Kahan-style error-compensated floating point accumulation?)doc";

static const char *__doc_mitsuba_ImageBlock_has_border = R"doc(Does the image block have a border region?)doc";

static const char *__doc_mitsuba_ImageBlock_height = R"doc(Return the bitmap's height in pixels)doc";

static const char *__doc_mitsuba_ImageBlock_m_border_size = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_channel_count = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_coalesce = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_compensate = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_normalize = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_offset = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_rfilter = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_size = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_tensor = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_tensor_compensation = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_warn_invalid = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_m_warn_negative = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_normalize = R"doc(Re-normalize filter weights in put() and read())doc";

static const char *__doc_mitsuba_ImageBlock_offset = R"doc(Return the current block offset)doc";

static const char *__doc_mitsuba_ImageBlock_put =
R"doc(Accumulate a single sample or a wavefront of samples into the image
block.

Remark:
    This variant of the put() function assumes that the ImageBlock has
    a standard layout, namely: ``RGB``, potentially ``alpha``, and a
    ``weight`` channel. Use the other variant if the channel
    configuration deviations from this default.

Parameter ``pos``:
    Denotes the sample position in fractional pixel coordinates

Parameter ``wavelengths``:
    Sample wavelengths in nanometers

Parameter ``value``:
    Sample value associated with the specified wavelengths

Parameter ``alpha``:
    Alpha value associated with the sample)doc";

static const char *__doc_mitsuba_ImageBlock_put_2 =
R"doc(Accumulate a single sample or a wavefront of samples into the image
block.

Parameter ``pos``:
    Denotes the sample position in fractional pixel coordinates

Parameter ``values``:
    Points to an array of length channel_count(), which specifies the
    sample value for each channel.)doc";

static const char *__doc_mitsuba_ImageBlock_put_block = R"doc(Accumulate another image block into this one)doc";

static const char *__doc_mitsuba_ImageBlock_read =
R"doc(Fetch a single sample or a wavefront of samples from the image block.

This function is the opposite of put(): instead of performing a
weighted accumulation of sample values based on the reconstruction
filter, it performs a weighted interpolation using gather operations.

Parameter ``pos``:
    Denotes the sample position in fractional pixel coordinates

Parameter ``values``:
    Points to an array of length channel_count(), which will receive
    the result of the read operation for each channel. In Python, the
    function returns these values as a list.)doc";

static const char *__doc_mitsuba_ImageBlock_rfilter = R"doc(Return the image reconstruction filter underlying the ImageBlock)doc";

static const char *__doc_mitsuba_ImageBlock_set_coalesce = R"doc(Try to coalesce reads/writes in JIT modes?)doc";

static const char *__doc_mitsuba_ImageBlock_set_compensate = R"doc(Use Kahan-style error-compensated floating point accumulation?)doc";

static const char *__doc_mitsuba_ImageBlock_set_normalize = R"doc(Re-normalize filter weights in put() and read())doc";

static const char *__doc_mitsuba_ImageBlock_set_offset =
R"doc(Set the current block offset.

This corresponds to the offset from the top-left corner of a larger
image (e.g. a Film) to the top-left corner of this ImageBlock
instance.)doc";

static const char *__doc_mitsuba_ImageBlock_set_size = R"doc(Set the block size. This potentially destroys the block's content.)doc";

static const char *__doc_mitsuba_ImageBlock_set_warn_invalid = R"doc(Warn when writing invalid (NaN, +/- infinity) sample values?)doc";

static const char *__doc_mitsuba_ImageBlock_set_warn_negative = R"doc(Warn when writing negative sample values?)doc";

static const char *__doc_mitsuba_ImageBlock_size = R"doc(Return the current block size)doc";

static const char *__doc_mitsuba_ImageBlock_tensor = R"doc(Return the underlying image tensor)doc";

static const char *__doc_mitsuba_ImageBlock_tensor_2 = R"doc(Return the underlying image tensor (const version))doc";

static const char *__doc_mitsuba_ImageBlock_to_string = R"doc(//! @})doc";

static const char *__doc_mitsuba_ImageBlock_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_ImageBlock_warn_invalid = R"doc(Warn when writing invalid (NaN, +/- infinity) sample values?)doc";

static const char *__doc_mitsuba_ImageBlock_warn_negative = R"doc(Warn when writing negative sample values?)doc";

static const char *__doc_mitsuba_ImageBlock_width = R"doc(Return the bitmap's width in pixels)doc";

static const char *__doc_mitsuba_Integrator =
R"doc(Abstract integrator base class, which does not make any assumptions
with regards to how radiance is computed.

In Mitsuba, the different rendering techniques are collectively
referred to as *integrators*, since they perform integration over a
high-dimensional space. Each integrator represents a specific approach
for solving the light transport equation---usually favored in certain
scenarios, but at the same time affected by its own set of intrinsic
limitations. Therefore, it is important to carefully select an
integrator based on user-specified accuracy requirements and
properties of the scene to be rendered.

This is the base class of all integrators; it does not make any
assumptions on how radiance is computed, which allows for many
different kinds of implementations.)doc";

static const char *__doc_mitsuba_Integrator_2 = R"doc()doc";

static const char *__doc_mitsuba_Integrator_3 = R"doc()doc";

static const char *__doc_mitsuba_Integrator_4 = R"doc()doc";

static const char *__doc_mitsuba_Integrator_5 = R"doc()doc";

static const char *__doc_mitsuba_Integrator_6 = R"doc()doc";

static const char *__doc_mitsuba_Integrator_Integrator = R"doc(Create an integrator)doc";

static const char *__doc_mitsuba_Integrator_aov_names =
R"doc(For integrators that return one or more arbitrary output variables
(AOVs), this function specifies a list of associated channel names.
The default implementation simply returns an empty vector.)doc";

static const char *__doc_mitsuba_Integrator_cancel = R"doc(Cancel a running render job (e.g. after receiving Ctrl-C))doc";

static const char *__doc_mitsuba_Integrator_class_name = R"doc()doc";

static const char *__doc_mitsuba_Integrator_m_hide_emitters = R"doc(Flag for disabling direct visibility of emitters)doc";

static const char *__doc_mitsuba_Integrator_m_id = R"doc(Identifier (if available))doc";

static const char *__doc_mitsuba_Integrator_m_render_timer = R"doc(Timer used to enforce the timeout.)doc";

static const char *__doc_mitsuba_Integrator_m_stop = R"doc(Integrators should stop all work when this flag is set to true.)doc";

static const char *__doc_mitsuba_Integrator_m_timeout =
R"doc(Maximum amount of time to spend rendering (excluding scene parsing).

Specified in seconds. A negative values indicates no timeout.)doc";

static const char *__doc_mitsuba_Integrator_render =
R"doc(Render the scene

This function renders the scene from the viewpoint of ``sensor``. All
other parameters are optional and control different aspects of the
rendering process. In particular:

Parameter ``seed``:
    This parameter controls the initialization of the random number
    generator. It is crucial that you specify different seeds (e.g.,
    an increasing sequence) if subsequent ``render``() calls should
    produce statistically independent images.

Parameter ``spp``:
    Set this parameter to a nonzero value to override the number of
    samples per pixel. This value then takes precedence over whatever
    was specified in the construction of ``sensor->sampler()``. This
    parameter may be useful in research applications where an image
    must be rendered multiple times using different quality levels.

Parameter ``develop``:
    If set to ``True``, the implementation post-processes the data
    stored in ``sensor->film()``, returning the resulting image as a
    TensorXf. Otherwise, it returns an empty tensor.

Parameter ``evaluate``:
    This parameter is only relevant for JIT variants of Mitsuba (LLVM,
    CUDA). If set to ``True``, the rendering step evaluates the
    generated image and waits for its completion. A log message also
    denotes the rendering time. Otherwise, the returned tensor
    (``develop=true``) or modified film (``develop=false``) represent
    the rendering task as an unevaluated computation graph.)doc";

static const char *__doc_mitsuba_Integrator_render_2 =
R"doc(Render the scene

This function is just a thin wrapper around the previous render()
overload. It accepts a sensor *index* instead and renders the scene
using sensor 0 by default.)doc";

static const char *__doc_mitsuba_Integrator_render_backward =
R"doc(Evaluates the reverse-mode derivative of the rendering step.

Reverse-mode differentiation transforms image-space gradients into
scene parameter gradients, enabling simultaneous optimization of
scenes with millions of differentiable parameters. The function is
invoked with an input *gradient image* (``grad_in``) and transforms
and accumulates these into the gradient arrays of scene parameters
that previously had gradient tracking enabled.

Before calling this function, you must first enable gradient tracking
for one or more scene parameters, or the function will not do
anything. This is typically done by invoking ``dr.enable_grad()`` on
elements of the ``SceneParameters`` data structure that can be
obtained via a call to ``mi.traverse()``. Use ``dr.grad()`` to query
the resulting gradients of these parameters once ``render_backward()``
returns.

Note the default implementation of this functionality relies on naive
automatic differentiation (AD), which records a computation graph of
the primal rendering step that is subsequently traversed to propagate
derivatives. This tends to be relatively inefficient due to the need
to track intermediate program state. In particular, it means that
differentiation of nontrivial scenes at high sample counts will often
run out of memory. Integrators like ``rb`` (Radiative Backpropagation)
and ``prb`` (Path Replay Backpropagation) that are specifically
designed for differentiation can be significantly more efficient.

Parameter ``scene``:
    The scene to be rendered differentially.

Parameter ``params``:
    An arbitrary container of scene parameters that should receive
    gradients. Typically this will be an instance of type
    ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it
    could also be a Python list/dict/object tree (DrJit will traverse
    it to find all parameters). Gradient tracking must be explicitly
    enabled for each of these parameters using
    ``dr.enable_grad(params['parameter_name'])`` (i.e.
    ``render_backward()`` will not do this for you).

Parameter ``grad_in``:
    Gradient image that should be back-propagated.

Parameter ``sensor``:
    Specify a sensor or a (sensor index) to render the scene from a
    different viewpoint. By default, the first sensor within the scene
    description (index 0) will take precedence.

Parameter ``seed``:
    This parameter controls the initialization of the random number
    generator. It is crucial that you specify different seeds (e.g.,
    an increasing sequence) if subsequent calls should produce
    statistically independent images (e.g. to de-correlate gradient-
    based optimization steps).

Parameter ``spp``:
    Optional parameter to override the number of samples per pixel for
    the differential rendering step. The value provided within the
    original scene specification takes precedence if ``spp=0``.)doc";

static const char *__doc_mitsuba_Integrator_render_backward_2 =
R"doc(Evaluates the reverse-mode derivative of the rendering step.

This function is just a thin wrapper around the previous
render_backward() function. It accepts a sensor *index* instead and
renders the scene using sensor 0 by default.)doc";

static const char *__doc_mitsuba_Integrator_render_forward =
R"doc(Evaluates the forward-mode derivative of the rendering step.

Forward-mode differentiation propagates gradients from scene
parameters through the simulation, producing a *gradient image* (i.e.,
the derivative of the rendered image with respect to those scene
parameters). The gradient image is very helpful for debugging, for
example to inspect the gradient variance or visualize the region of
influence of a scene parameter. It is not particularly useful for
simultaneous optimization of many parameters, since multiple
differentiation passes are needed to obtain separate derivatives for
each scene parameter. See ``Integrator.render_backward()`` for an
efficient way of obtaining all parameter derivatives at once, or
simply use the ``mi.render()`` abstraction that hides both
``Integrator.render_forward()`` and ``Integrator.render_backward()``
behind a unified interface.

Before calling this function, you must first enable gradient tracking
and furthermore associate concrete input gradients with one or more
scene parameters, or the function will just return a zero-valued
gradient image. This is typically done by invoking
``dr.enable_grad()`` and ``dr.set_grad()`` on elements of the
``SceneParameters`` data structure that can be obtained via a call to
``mi.traverse()``.

Note the default implementation of this functionality relies on naive
automatic differentiation (AD), which records a computation graph of
the primal rendering step that is subsequently traversed to propagate
derivatives. This tends to be relatively inefficient due to the need
to track intermediate program state. In particular, it means that
differentiation of nontrivial scenes at high sample counts will often
run out of memory. Integrators like ``rb`` (Radiative Backpropagation)
and ``prb`` (Path Replay Backpropagation) that are specifically
designed for differentiation can be significantly more efficient.

Parameter ``scene``:
    The scene to be rendered differentially.

Parameter ``params``:
    An arbitrary container of scene parameters that should receive
    gradients. Typically this will be an instance of type
    ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it
    could also be a Python list/dict/object tree (DrJit will traverse
    it to find all parameters). Gradient tracking must be explicitly
    enabled for each of these parameters using
    ``dr.enable_grad(params['parameter_name'])`` (i.e.
    ``render_forward()`` will not do this for you). Furthermore,
    ``dr.set_grad(...)`` must be used to associate specific gradient
    values with each parameter.

Parameter ``sensor``:
    Specify a sensor or a (sensor index) to render the scene from a
    different viewpoint. By default, the first sensor within the scene
    description (index 0) will take precedence.

Parameter ``seed``:
    This parameter controls the initialization of the random number
    generator. It is crucial that you specify different seeds (e.g.,
    an increasing sequence) if subsequent calls should produce
    statistically independent images (e.g. to de-correlate gradient-
    based optimization steps).

\param ``spp`` (``int``): Optional parameter to override the number of
samples per pixel for the differential rendering step. The value
provided within the original scene specification takes precedence if
``spp=0``.)doc";

static const char *__doc_mitsuba_Integrator_render_forward_2 =
R"doc(Evaluates the forward-mode derivative of the rendering step.

This function is just a thin wrapper around the previous
render_forward() function. It accepts a sensor *index* instead and
renders the scene using sensor 0 by default.)doc";

static const char *__doc_mitsuba_Integrator_should_stop =
R"doc(Indicates whether cancel() or a timeout have occurred. Should be
checked regularly in the integrator's main loop so that timeouts are
enforced accurately.

Note that accurate timeouts rely on m_render_timer, which needs to be
reset at the beginning of the rendering phase.)doc";

static const char *__doc_mitsuba_Integrator_skip_area_emitters =
R"doc(Traces a ray in the scene and returns the first intersection that is
not an area emitter.

This is a helper method for when the `hide_emitters` flag is set.

Parameter ``scene``:
    The scene that the ray will intersect.

Parameter ``ray``:
    The ray that determines the direction in which to trace new rays

Parameter ``coherent``:
    Setting this flag to ``True`` can noticeably improve performance
    when ``ray`` contains a coherent set of rays (e.g. primary camera
    rays), and when using ``llvm_*`` variants of the renderer along
    with Embree. It has no effect in scalar or CUDA/OptiX variants.
    (Default: False)

Parameter ``active``:
    A mask that indicates which lanes are active. Typically, this
    should be set to ``True`` for any lane where the current depth is
    0 (for ``hide_emitters``). (Default: True)

Returns:
    The first intersection that is not an area emitter anlong the
    ``ray``.)doc";

static const char *__doc_mitsuba_Integrator_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Integrator_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Integrator_type = R"doc()doc";

static const char *__doc_mitsuba_Integrator_variant_name = R"doc()doc";

static const char *__doc_mitsuba_Interaction = R"doc(Generic surface interaction data structure)doc";

static const char *__doc_mitsuba_Interaction_Interaction = R"doc(Constructor)doc";

static const char *__doc_mitsuba_Interaction_Interaction_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_Interaction_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_Interaction_4 = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_is_valid = R"doc(Is the current interaction valid?)doc";

static const char *__doc_mitsuba_Interaction_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_n = R"doc(Geometric normal (only valid for ``SurfaceInteraction``))doc";

static const char *__doc_mitsuba_Interaction_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_offset_p =
R"doc(Compute an offset position, used when spawning a ray from this
interaction. When the interaction is on the surface of a shape, the
position is offset along the surface normal to prevent self
intersection.)doc";

static const char *__doc_mitsuba_Interaction_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_Interaction_p = R"doc(Position of the interaction in world coordinates)doc";

static const char *__doc_mitsuba_Interaction_spawn_ray = R"doc(Spawn a semi-infinite ray towards the given direction)doc";

static const char *__doc_mitsuba_Interaction_spawn_ray_to = R"doc(Spawn a finite ray towards the given position)doc";

static const char *__doc_mitsuba_Interaction_t = R"doc(Distance traveled along the ray)doc";

static const char *__doc_mitsuba_Interaction_time = R"doc(Time value associated with the interaction)doc";

static const char *__doc_mitsuba_Interaction_wavelengths = R"doc(Wavelengths associated with the ray that produced this interaction)doc";

static const char *__doc_mitsuba_Interaction_zero =
R"doc(This callback method is invoked by dr::zeros<>, and takes care of
fields that deviate from the standard zero-initialization convention.
In this particular class, the ``t`` field should be set to an infinite
value to mark invalid intersection records.)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution =
R"doc(Continuous 1D probability distribution defined in terms of an
*irregularly* sampled linear interpolant

This data structure represents a continuous 1D probability
distribution that is defined as a linear interpolant of an irregularly
discretized signal. The class provides various routines for
transforming uniformly distributed samples so that they follow the
stored distribution. Note that unnormalized probability density
functions (PDFs) will automatically be normalized during
initialization. The associated scale factor can be retrieved using the
function normalization().)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_IrregularContinuousDistribution = R"doc(Create an uninitialized IrregularContinuousDistribution instance)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_IrregularContinuousDistribution_2 =
R"doc(Initialize from a given density function discretized on nodes
``nodes``)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_IrregularContinuousDistribution_3 = R"doc(Initialize from a given density function (rvalue version))doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_IrregularContinuousDistribution_4 = R"doc(Initialize from a given floating point array)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_cdf =
R"doc(Return the unnormalized discrete cumulative distribution function over
intervals)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_cdf_2 =
R"doc(Return the unnormalized discrete cumulative distribution function over
intervals (const version))doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_compute_cdf = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_compute_cdf_scalar = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_empty = R"doc(Is the distribution object empty/uninitialized?)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_eval_cdf =
R"doc(Evaluate the unnormalized cumulative distribution function (CDF) at
position ``p``)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_eval_cdf_normalized =
R"doc(Evaluate the unnormalized cumulative distribution function (CDF) at
position ``p``)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_eval_pdf =
R"doc(Evaluate the unnormalized probability mass function (PDF) at position
``x``)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_eval_pdf_normalized =
R"doc(Evaluate the normalized probability mass function (PDF) at position
``x``)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_integral = R"doc(Return the original integral of PDF entries before normalization)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_interval_resolution = R"doc(Return the minimum resolution of the discretization)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_cdf = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_integral = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_interval_size = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_max = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_nodes = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_normalization = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_pdf = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_range = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_m_valid = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_max = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_nodes = R"doc(Return the nodes of the underlying discretization)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_nodes_2 = R"doc(Return the nodes of the underlying discretization (const version))doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_normalization = R"doc(Return the normalization factor (i.e. the inverse of sum()))doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_pdf = R"doc(Return the unnormalized discretized probability density function)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_pdf_2 =
R"doc(Return the unnormalized discretized probability density function
(const version))doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_range = R"doc(Return the range of the distribution)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_range_2 = R"doc(Return the range of the distribution (const version))doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_sample =
R"doc(Transform a uniformly distributed sample to the stored distribution

Parameter ``sample``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    The sampled position.)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_sample_pdf =
R"doc(Transform a uniformly distributed sample to the stored distribution

Parameter ``sample``:
    A uniformly distributed sample on the interval [0, 1].

Returns:
    A tuple consisting of

1. the sampled position. 2. the normalized probability density of the
sample.)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_size = R"doc(Return the number of discretizations)doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_IrregularContinuousDistribution_update =
R"doc(Update the internal state. Must be invoked when changing the pdf or
range.)doc";

static const char *__doc_mitsuba_Jit = R"doc()doc";

static const char *__doc_mitsuba_JitObject =
R"doc(CRTP base class for JIT-registered objects

This class provides automatic registration/deregistration with
Dr.Jit's instance registry for JIT-compiled variants. It uses the
Curiously Recurring Template Pattern (CRTP) to access static members
of the derived class.

The derived class must provide: - `static constexpr const char
*Variant`: variant name string - `static constexpr ObjectType Type`:
object type enumeration value - `static constexpr const char *Domain`:
string that represents the plugin category - `using UInt32 = ...`:
type that indicates whether this is a JIT variant

The `MI_DECLARE_PLUGIN_BASE_CLASS()` macro ensures that these
attributes are present.)doc";

static const char *__doc_mitsuba_JitObject_JitObject = R"doc(Constructor with ID and optional ObjectType)doc";

static const char *__doc_mitsuba_JitObject_JitObject_2 = R"doc(Copy constructor (registers the new instance))doc";

static const char *__doc_mitsuba_JitObject_JitObject_3 = R"doc(Move constructor (registers the new instance))doc";

static const char *__doc_mitsuba_JitObject_id = R"doc(Return the identifier of this instance)doc";

static const char *__doc_mitsuba_JitObject_m_id = R"doc(Stores the identifier of this instance)doc";

static const char *__doc_mitsuba_JitObject_set_id = R"doc(Set the identifier of this instance)doc";

static const char *__doc_mitsuba_Jit_Jit = R"doc()doc";

static const char *__doc_mitsuba_Jit_Jit_2 = R"doc()doc";

static const char *__doc_mitsuba_Jit_get_instance = R"doc()doc";

static const char *__doc_mitsuba_Jit_mutex = R"doc()doc";

static const char *__doc_mitsuba_Jit_runtime = R"doc()doc";

static const char *__doc_mitsuba_Jit_static_initialization =
R"doc(Statically initialize the JIT runtime

This function also does a runtime-check to ensure that the host
processor supports all instruction sets which were selected at compile
time. If not, the application is terminated via ``abort()``.)doc";

static const char *__doc_mitsuba_Jit_static_shutdown = R"doc(Release all memory used by JIT-compiled routines)doc";

static const char *__doc_mitsuba_LocationRecord = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_LocationRecord = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_LocationRecord_2 = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_LocationRecord_3 = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_LocationRecord_4 = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_fields = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_labels = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_latitude = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_longitude = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_name = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_operator_assign_3 = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_timezone = R"doc()doc";

static const char *__doc_mitsuba_LocationRecord_to_string = R"doc()doc";

static const char *__doc_mitsuba_LogLevel = R"doc(Available Log message types)doc";

static const char *__doc_mitsuba_LogLevel_Debug = R"doc(Trace message, for extremely verbose debugging)doc";

static const char *__doc_mitsuba_LogLevel_Error = R"doc(Warning message)doc";

static const char *__doc_mitsuba_LogLevel_Info = R"doc(Debug message, usually turned off)doc";

static const char *__doc_mitsuba_LogLevel_Trace = R"doc()doc";

static const char *__doc_mitsuba_LogLevel_Warn = R"doc(More relevant debug / information message)doc";

static const char *__doc_mitsuba_Logger =
R"doc(Responsible for processing log messages

Upon receiving a log message, the Logger class invokes a Formatter to
convert it into a human-readable form. Following that, it sends this
information to every registered Appender.)doc";

static const char *__doc_mitsuba_Logger_Logger = R"doc(Construct a new logger with the given minimum log level)doc";

static const char *__doc_mitsuba_Logger_LoggerPrivate = R"doc()doc";

static const char *__doc_mitsuba_Logger_add_appender = R"doc(Add an appender to this logger)doc";

static const char *__doc_mitsuba_Logger_appender = R"doc(Return one of the appenders)doc";

static const char *__doc_mitsuba_Logger_appender_2 = R"doc(Return one of the appenders)doc";

static const char *__doc_mitsuba_Logger_appender_count = R"doc(Return the number of registered appenders)doc";

static const char *__doc_mitsuba_Logger_class_name = R"doc()doc";

static const char *__doc_mitsuba_Logger_clear_appenders = R"doc(Remove all appenders from this logger)doc";

static const char *__doc_mitsuba_Logger_d = R"doc()doc";

static const char *__doc_mitsuba_Logger_error_level = R"doc(Return the current error level)doc";

static const char *__doc_mitsuba_Logger_formatter = R"doc(Return the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_formatter_2 = R"doc(Return the logger's formatter implementation (const))doc";

static const char *__doc_mitsuba_Logger_log =
R"doc(Process a log message

Parameter ``level``:
    Log level of the message

Parameter ``cname``:
    Name of the class (if present)

Parameter ``fname``:
    Source filename

Parameter ``line``:
    Source line number

Parameter ``msg``:
    Log message

\note This function is not exposed in the Python bindings. Instead,
please use \cc mitsuba.core.Log)doc";

static const char *__doc_mitsuba_Logger_log_level = R"doc(Return the current log level)doc";

static const char *__doc_mitsuba_Logger_log_progress =
R"doc(Process a progress message

Parameter ``progress``:
    Percentage value in [0, 100]

Parameter ``name``:
    Title of the progress message

Parameter ``formatted``:
    Formatted string representation of the message

Parameter ``eta``:
    Estimated time until 100% is reached.

Parameter ``ptr``:
    Custom pointer payload. This is used to express the context of a
    progress message.)doc";

static const char *__doc_mitsuba_Logger_m_log_level = R"doc()doc";

static const char *__doc_mitsuba_Logger_read_log =
R"doc(Return the contents of the log file as a string

Throws a runtime exception upon failure)doc";

static const char *__doc_mitsuba_Logger_remove_appender = R"doc(Remove an appender from this logger)doc";

static const char *__doc_mitsuba_Logger_set_error_level =
R"doc(Set the error log level (this level and anything above will throw
exceptions).

The value provided here can be used for instance to turn warnings into
errors. But *level* must always be less than Error, i.e. it isn't
possible to cause errors not to throw an exception.)doc";

static const char *__doc_mitsuba_Logger_set_formatter = R"doc(Set the logger's formatter implementation)doc";

static const char *__doc_mitsuba_Logger_set_log_level = R"doc(Set the log level (everything below will be ignored))doc";

static const char *__doc_mitsuba_Logger_static_initialization = R"doc(Initialize logging)doc";

static const char *__doc_mitsuba_Logger_static_shutdown = R"doc(Shutdown logging)doc";

static const char *__doc_mitsuba_Marginal2D =
R"doc(Implements a marginal sample warping scheme for 2D distributions with
linear interpolation and an optional dependence on additional
parameters

This class takes a rectangular floating point array as input and
constructs internal data structures to efficiently map uniform
variates from the unit square ``[0, 1]^2`` to a function on ``[0,
1]^2`` that linearly interpolates the input array.

The mapping is constructed via the inversion method, which is applied
to a marginal distribution over rows, followed by a conditional
distribution over columns.

The implementation also supports *conditional distributions*, i.e. 2D
distributions that depend on an arbitrary number of parameters
(indicated via the ``Dimension`` template parameter).

In this case, the input array should have dimensions ``N0 x N1 x ... x
Nn x res.y() x res.x()`` (where the last dimension is contiguous in
memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
}``, and ``param_values`` should contain the parameter values where
the distribution is discretized. Linear interpolation is used when
sampling or evaluating the distribution for in-between parameter
values.

There are two variants of ``Marginal2D:`` when ``Continuous=false``,
discrete marginal/conditional distributions are used to select a
bilinear bilinear patch, followed by a continuous sampling step that
chooses a specific position inside the patch. When
``Continuous=true``, continuous marginal/conditional distributions are
used instead, and the second step is no longer needed. The latter
scheme requires more computation and memory accesses but produces an
overall smoother mapping.

Remark:
    The Python API exposes explicitly instantiated versions of this
    class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
    ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
    that depends on 0 to 3 parameters.)doc";

static const char *__doc_mitsuba_Marginal2D_Marginal2D = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_Marginal2D_2 =
R"doc(Construct a marginal sample warping scheme for floating point data of
resolution ``size``.

``param_res`` and ``param_values`` are only needed for conditional
distributions (see the text describing the Marginal2D class).

If ``normalize`` is set to ``False``, the implementation will not re-
scale the distribution so that it integrates to ``1``. It can still be
sampled (proportionally), but returned density values will reflect the
unnormalized values.

If ``enable_sampling`` is set to ``False``, the implementation will
not construct the cdf needed for sample warping, which saves memory in
case this functionality is not needed (e.g. if only the interpolation
in ``eval()`` is used).)doc";

static const char *__doc_mitsuba_Marginal2D_eval =
R"doc(Evaluate the density at position ``pos``. The distribution is
parameterized by ``param`` if applicable.)doc";

static const char *__doc_mitsuba_Marginal2D_invert = R"doc(Inverse of the mapping implemented in ``sample()``)doc";

static const char *__doc_mitsuba_Marginal2D_invert_continuous = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_invert_discrete = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_invert_segment = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_lookup = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_m_cond_cdf = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_m_data = R"doc(Density values)doc";

static const char *__doc_mitsuba_Marginal2D_m_marg_cdf = R"doc(Marginal and conditional PDFs)doc";

static const char *__doc_mitsuba_Marginal2D_m_normalized = R"doc(Are the probability values normalized?)doc";

static const char *__doc_mitsuba_Marginal2D_m_size = R"doc(Resolution of the discretized density function)doc";

static const char *__doc_mitsuba_Marginal2D_sample =
R"doc(Given a uniformly distributed 2D sample, draw a sample from the
distribution (parameterized by ``param`` if applicable)

Returns the warped sample and associated probability density.)doc";

static const char *__doc_mitsuba_Marginal2D_sample_continuous = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_sample_discrete = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_sample_segment = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_to_string = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Marginal2D_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Medium = R"doc()doc";

static const char *__doc_mitsuba_Medium_2 = R"doc()doc";

static const char *__doc_mitsuba_Medium_3 = R"doc()doc";

static const char *__doc_mitsuba_Medium_4 = R"doc()doc";

static const char *__doc_mitsuba_Medium_5 = R"doc()doc";

static const char *__doc_mitsuba_Medium_6 = R"doc()doc";

static const char *__doc_mitsuba_MediumInteraction = R"doc(Stores information related to a medium scattering interaction)doc";

static const char *__doc_mitsuba_MediumInteraction_MediumInteraction = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_MediumInteraction_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_MediumInteraction_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_combined_extinction = R"doc()doc";

static const char *__doc_mitsuba_MediumInteraction_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_medium = R"doc(Pointer to the associated medium)doc";

static const char *__doc_mitsuba_MediumInteraction_mint = R"doc(mint used when sampling the given distance ``t``)doc";

static const char *__doc_mitsuba_MediumInteraction_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_MediumInteraction_sh_frame = R"doc(Shading frame)doc";

static const char *__doc_mitsuba_MediumInteraction_sigma_n = R"doc()doc";

static const char *__doc_mitsuba_MediumInteraction_sigma_s = R"doc()doc";

static const char *__doc_mitsuba_MediumInteraction_sigma_t = R"doc()doc";

static const char *__doc_mitsuba_MediumInteraction_to_local =
R"doc(Convert a world-space vector into local shading coordinates (defined
by ``wi``))doc";

static const char *__doc_mitsuba_MediumInteraction_to_world =
R"doc(Convert a local shading-space (defined by ``wi``) vector into world
space)doc";

static const char *__doc_mitsuba_MediumInteraction_wi = R"doc(Incident direction in world frame)doc";

static const char *__doc_mitsuba_MediumInteraction_zero =
R"doc(This callback method is invoked by dr::zeros<>, and takes care of
fields that deviate from the standard zero-initialization convention.)doc";

static const char *__doc_mitsuba_Medium_Medium = R"doc()doc";

static const char *__doc_mitsuba_Medium_Medium_2 = R"doc()doc";

static const char *__doc_mitsuba_Medium_class_name = R"doc()doc";

static const char *__doc_mitsuba_Medium_get_majorant = R"doc(Returns the medium's majorant used for delta tracking)doc";

static const char *__doc_mitsuba_Medium_get_scattering_coefficients =
R"doc(Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
at a given MediumInteraction mi)doc";

static const char *__doc_mitsuba_Medium_has_spectral_extinction = R"doc(Returns whether this medium has a spectrally varying extinction)doc";

static const char *__doc_mitsuba_Medium_intersect_aabb = R"doc(Intersects a ray with the medium's bounding box)doc";

static const char *__doc_mitsuba_Medium_is_homogeneous = R"doc(Returns whether this medium is homogeneous)doc";

static const char *__doc_mitsuba_Medium_m_has_spectral_extinction = R"doc()doc";

static const char *__doc_mitsuba_Medium_m_is_homogeneous = R"doc()doc";

static const char *__doc_mitsuba_Medium_m_phase_function = R"doc()doc";

static const char *__doc_mitsuba_Medium_m_sample_emitters = R"doc()doc";

static const char *__doc_mitsuba_Medium_phase_function = R"doc(Return the phase function of this medium)doc";

static const char *__doc_mitsuba_Medium_sample_interaction =
R"doc(Sample a free-flight distance in the medium.

This function samples a (tentative) free-flight distance according to
an exponential transmittance. It is then up to the integrator to then
decide whether the MediumInteraction corresponds to a real or null
scattering event.

Parameter ``ray``:
    Ray, along which a distance should be sampled

Parameter ``sample``:
    A uniformly distributed random sample

Parameter ``channel``:
    The channel according to which we will sample the free-flight
    distance. This argument is only used when rendering in RGB modes.

Returns:
    This method returns a MediumInteraction. The MediumInteraction
    will always be valid, except if the ray missed the Medium's
    bounding box.)doc";

static const char *__doc_mitsuba_Medium_to_string = R"doc(Return a human-readable representation of the Medium)doc";

static const char *__doc_mitsuba_Medium_transmittance_eval_pdf =
R"doc(Compute the transmittance and PDF

This function evaluates the transmittance and PDF of sampling a
certain free-flight distance The returned PDF takes into account if a
medium interaction occurred (mi.t <= si.t) or the ray left the medium
(mi.t > si.t)

The evaluated PDF is spectrally varying. This allows to account for
the fact that the free-flight distance sampling distribution can
depend on the wavelength.

Returns:
    This method returns a pair of (Transmittance, PDF).)doc";

static const char *__doc_mitsuba_Medium_traverse = R"doc()doc";

static const char *__doc_mitsuba_Medium_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_Medium_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Medium_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Medium_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Medium_type = R"doc()doc";

static const char *__doc_mitsuba_Medium_use_emitter_sampling = R"doc(Returns whether this specific medium instance uses emitter sampling)doc";

static const char *__doc_mitsuba_Medium_variant_name = R"doc()doc";

static const char *__doc_mitsuba_MemoryMappedFile =
R"doc(Basic cross-platform abstraction for memory mapped files

Remark:
    The Python API has one additional constructor
    <tt>MemoryMappedFile(filename, array)<tt>, which creates a new
    file, maps it into memory, and copies the array contents.)doc";

static const char *__doc_mitsuba_MemoryMappedFile_MemoryMappedFile = R"doc(Create a new memory-mapped file of the specified size)doc";

static const char *__doc_mitsuba_MemoryMappedFile_MemoryMappedFile_2 = R"doc(Map the specified file into memory)doc";

static const char *__doc_mitsuba_MemoryMappedFile_MemoryMappedFile_3 = R"doc(Internal constructor)doc";

static const char *__doc_mitsuba_MemoryMappedFile_MemoryMappedFilePrivate = R"doc()doc";

static const char *__doc_mitsuba_MemoryMappedFile_can_write = R"doc(Return whether the mapped memory region can be modified)doc";

static const char *__doc_mitsuba_MemoryMappedFile_class_name = R"doc()doc";

static const char *__doc_mitsuba_MemoryMappedFile_create_temporary =
R"doc(Create a temporary memory-mapped file

Remark:
    When closing the mapping, the file is automatically deleted.
    Mitsuba additionally informs the OS that any outstanding changes
    that haven't yet been written to disk can be discarded (Linux/OSX
    only).)doc";

static const char *__doc_mitsuba_MemoryMappedFile_d = R"doc()doc";

static const char *__doc_mitsuba_MemoryMappedFile_data = R"doc(Return a pointer to the file contents in memory)doc";

static const char *__doc_mitsuba_MemoryMappedFile_data_2 = R"doc(Return a pointer to the file contents in memory (const version))doc";

static const char *__doc_mitsuba_MemoryMappedFile_filename = R"doc(Return the associated filename)doc";

static const char *__doc_mitsuba_MemoryMappedFile_resize =
R"doc(Resize the memory-mapped file

This involves remapping the file, which will generally change the
pointer obtained via data())doc";

static const char *__doc_mitsuba_MemoryMappedFile_size = R"doc(Return the size of the mapped region)doc";

static const char *__doc_mitsuba_MemoryMappedFile_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_MemoryStream =
R"doc(Simple memory buffer-based stream with automatic memory management. It
always has read & write capabilities.

The underlying memory storage of this implementation dynamically
expands as data is written to the stream, à la ``std::vector``.)doc";

static const char *__doc_mitsuba_MemoryStream_MemoryStream =
R"doc(Creates a new memory stream, initializing the memory buffer with a
capacity of ``capacity`` bytes. For best performance, set this
argument to the estimated size of the content that will be written to
the stream.)doc";

static const char *__doc_mitsuba_MemoryStream_MemoryStream_2 =
R"doc(Creates a memory stream, which operates on a pre-allocated buffer.

A memory stream created in this way will never resize the underlying
buffer. An exception is thrown e.g. when attempting to extend its
size.

Remark:
    This constructor is not available in the python bindings.)doc";

static const char *__doc_mitsuba_MemoryStream_can_read = R"doc(Always returns true, except if the stream is closed.)doc";

static const char *__doc_mitsuba_MemoryStream_can_write = R"doc(Always returns true, except if the stream is closed.)doc";

static const char *__doc_mitsuba_MemoryStream_capacity = R"doc(Return the current capacity of the underlying memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_MemoryStream_close =
R"doc(Closes the stream. No further read or write operations are permitted.

This function is idempotent. It may be called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_MemoryStream_flush = R"doc(No-op since all writes are made directly to memory)doc";

static const char *__doc_mitsuba_MemoryStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_MemoryStream_m_capacity = R"doc(Current size of the allocated memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_data = R"doc(Pointer to the memory buffer (might not be owned))doc";

static const char *__doc_mitsuba_MemoryStream_m_is_closed = R"doc(Whether the stream has been closed.)doc";

static const char *__doc_mitsuba_MemoryStream_m_owns_buffer = R"doc(False if the MemoryStream was created from a pre-allocated buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_pos = R"doc(Current position inside of the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_m_size = R"doc(Current size of the contents written to the memory buffer)doc";

static const char *__doc_mitsuba_MemoryStream_owns_buffer = R"doc(Return whether or not the memory stream owns the underlying buffer)doc";

static const char *__doc_mitsuba_MemoryStream_raw_buffer = R"doc(Return the underlying raw byte array)doc";

static const char *__doc_mitsuba_MemoryStream_read =
R"doc(Reads a specified amount of data from the stream. Throws an exception
if trying to read further than the current size of the contents.)doc";

static const char *__doc_mitsuba_MemoryStream_resize = R"doc(//! @})doc";

static const char *__doc_mitsuba_MemoryStream_seek =
R"doc(Seeks to a position inside the stream. You may seek beyond the size of
the stream's contents, or even beyond the buffer's capacity. The size
and capacity are **not** affected. A subsequent write would then
expand the size and capacity accordingly. The contents of the memory
that was skipped is undefined.)doc";

static const char *__doc_mitsuba_MemoryStream_size =
R"doc(Returns the size of the contents written to the memory buffer. \note
This is not equal to the size of the memory buffer in general, since
we allocate more capacity at once.)doc";

static const char *__doc_mitsuba_MemoryStream_tell =
R"doc(Gets the current position inside the memory buffer. Note that this
might be further than the stream's size or even capacity.)doc";

static const char *__doc_mitsuba_MemoryStream_to_string = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_MemoryStream_truncate =
R"doc(Truncates the contents **and** the memory buffer's capacity to a given
size. The position is updated to ``min(old_position, size)``.

\note This will throw is the MemoryStream was initialized with a pre-
allocated buffer.)doc";

static const char *__doc_mitsuba_MemoryStream_write =
R"doc(Writes a specified amount of data into the memory buffer. The capacity
of the memory buffer is extended if necessary.)doc";

static const char *__doc_mitsuba_Mesh = R"doc()doc";

static const char *__doc_mitsuba_Mesh_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_3 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_4 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_5 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_6 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_Mesh =
R"doc(Creates a zero-initialized mesh with the given vertex and face counts

The vertex and face buffers can be filled using the ``mi.traverse``
mechanism. When initializing these buffers through another method, an
explicit call to initialize must be made once all buffers are filled.)doc";

static const char *__doc_mitsuba_Mesh_Mesh_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_Mesh_3 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttributeType = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttributeType_Face = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttributeType_Vertex = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_MeshAttribute = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_MeshAttribute_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_buf = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_fields = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_labels = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_migrate = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_name = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_size = R"doc()doc";

static const char *__doc_mitsuba_Mesh_MeshAttribute_type = R"doc()doc";

static const char *__doc_mitsuba_Mesh_add_attribute = R"doc(Add an attribute buffer with the given ``name`` and ``dim``)doc";

static const char *__doc_mitsuba_Mesh_attribute_buffer = R"doc(Return the mesh attribute associated with ``name``)doc";

static const char *__doc_mitsuba_Mesh_barycentric_coordinates = R"doc()doc";

static const char *__doc_mitsuba_Mesh_bbox = R"doc(//! @{ \name Shape interface implementation)doc";

static const char *__doc_mitsuba_Mesh_bbox_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_bbox_3 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_build_directed_edges =
R"doc(Build directed edge data structure to efficiently access adjacent
edges.

This is an implementation of the technique described in:
``https://www.graphics.rwth-aachen.de/media/papers/directed.pdf``.)doc";

static const char *__doc_mitsuba_Mesh_build_indirect_silhouette_distribution =
R"doc(Precompute the set of edges that could contribute to the indirect
discontinuous integral.

This method filters out any concave edges or flat surfaces.

Internally, this method relies on the directed edge data structure. A
call to build_directed_edges before a call to this method is therefore
necessary.)doc";

static const char *__doc_mitsuba_Mesh_build_parameterization =
R"doc(Initialize the ``m_parameterization`` field for mapping UV coordinates
to positions

Internally, the function creates a nested scene to leverage optimized
ray tracing functionality in eval_parameterization())doc";

static const char *__doc_mitsuba_Mesh_build_pmf =
R"doc(Build internal tables for sampling uniformly wrt. area.

Computes the surface area and sets up ``m_area_pmf`` Thread-safe,
since it uses a mutex.)doc";

static const char *__doc_mitsuba_Mesh_class_name = R"doc()doc";

static const char *__doc_mitsuba_Mesh_compute_surface_interaction = R"doc()doc";

static const char *__doc_mitsuba_Mesh_differential_motion = R"doc()doc";

static const char *__doc_mitsuba_Mesh_edge_indices =
R"doc(Returns the vertex indices associated with edge ``edge_index`` (0..2)
of triangle ``tri_index``.)doc";

static const char *__doc_mitsuba_Mesh_embree_geometry = R"doc(Return the Embree version of this shape)doc";

static const char *__doc_mitsuba_Mesh_ensure_pmf_built = R"doc()doc";

static const char *__doc_mitsuba_Mesh_eval_attribute = R"doc()doc";

static const char *__doc_mitsuba_Mesh_eval_attribute_1 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_eval_attribute_3 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_eval_parameterization = R"doc()doc";

static const char *__doc_mitsuba_Mesh_face_count = R"doc(Return the total number of faces)doc";

static const char *__doc_mitsuba_Mesh_face_data_bytes = R"doc()doc";

static const char *__doc_mitsuba_Mesh_face_indices = R"doc(Returns the vertex indices associated with triangle ``index``)doc";

static const char *__doc_mitsuba_Mesh_face_normal = R"doc(Returns the normal direction of the face with index ``index``)doc";

static const char *__doc_mitsuba_Mesh_faces_buffer = R"doc(Return face indices buffer)doc";

static const char *__doc_mitsuba_Mesh_faces_buffer_2 = R"doc(Const variant of faces_buffer.)doc";

static const char *__doc_mitsuba_Mesh_has_attribute = R"doc()doc";

static const char *__doc_mitsuba_Mesh_has_face_normals = R"doc(Does this mesh use face normals?)doc";

static const char *__doc_mitsuba_Mesh_has_flipped_normals = R"doc(Does this shape have flipped normals?)doc";

static const char *__doc_mitsuba_Mesh_has_mesh_attributes = R"doc(Does this mesh have additional mesh attributes?)doc";

static const char *__doc_mitsuba_Mesh_has_vertex_normals = R"doc(Does this mesh have per-vertex normals?)doc";

static const char *__doc_mitsuba_Mesh_has_vertex_texcoords = R"doc(Does this mesh have per-vertex texture coordinates?)doc";

static const char *__doc_mitsuba_Mesh_initialize =
R"doc(Must be called once at the end of the construction of a Mesh

This method computes internal data structures and notifies the parent
sensor or emitter (if there is one) that this instance is their
internal shape.)doc";

static const char *__doc_mitsuba_Mesh_interpolate_attribute = R"doc()doc";

static const char *__doc_mitsuba_Mesh_invert_silhouette_sample = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_E2E = R"doc(Directed edges data structures to support neighbor queries)doc";

static const char *__doc_mitsuba_Mesh_m_E2E_outdated = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_area_pmf = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_face_count = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_face_normals =
R"doc(Flag that can be set by the user to disable loading/computation of
vertex normals)doc";

static const char *__doc_mitsuba_Mesh_m_faces = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_flip_normals = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_mesh_attributes = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_mutex = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_name = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_parameterization = R"doc(Optional: used in eval_parameterization())doc";

static const char *__doc_mitsuba_Mesh_m_scene = R"doc(Pointer to the scene that owns this mesh)doc";

static const char *__doc_mitsuba_Mesh_m_sil_dedge_pmf =
R"doc(Sampling density of silhouette
(build_indirect_silhouette_distribution))doc";

static const char *__doc_mitsuba_Mesh_m_vertex_buffer_ptr = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_count = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_normals = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_positions = R"doc()doc";

static const char *__doc_mitsuba_Mesh_m_vertex_texcoords = R"doc()doc";

static const char *__doc_mitsuba_Mesh_merge = R"doc(Merge two meshes into one)doc";

static const char *__doc_mitsuba_Mesh_moeller_trumbore =
R"doc(Moeller and Trumbore algorithm for computing ray-triangle intersection

Discussed at
``http://www.acm.org/jgt/papers/MollerTrumbore97/code.html``.

Parameter ``ray``:
    The ray segment to be used for the intersection query.

Parameter ``p0``:
    First vertex position of the triangle

Parameter ``p1``:
    Second vertex position of the triangle

Parameter ``p2``:
    Third vertex position of the triangle

Returns:
    Returns an ordered tuple ``(mask, u, v, t)``, where ``mask``
    indicates whether an intersection was found, ``t`` contains the
    distance from the ray origin to the intersection point, and ``u``
    and ``v`` contains the first two components of the intersection in
    barycentric coordinates)doc";

static const char *__doc_mitsuba_Mesh_opposite_dedge =
R"doc(Returns the opposite edge index associated with directed edge
``index``

If the directed edge data structure is not initialized or outdated,
the return value is undefined. Ensure that build_directed_edges() is
called before this method.)doc";

static const char *__doc_mitsuba_Mesh_optix_build_input = R"doc()doc";

static const char *__doc_mitsuba_Mesh_optix_prepare_geometry = R"doc()doc";

static const char *__doc_mitsuba_Mesh_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_Mesh_parameters_grad_enabled = R"doc()doc";

static const char *__doc_mitsuba_Mesh_pdf_position = R"doc()doc";

static const char *__doc_mitsuba_Mesh_precompute_silhouette = R"doc()doc";

static const char *__doc_mitsuba_Mesh_primitive_count = R"doc()doc";

static const char *__doc_mitsuba_Mesh_primitive_silhouette_projection = R"doc()doc";

static const char *__doc_mitsuba_Mesh_ray_intersect_triangle = R"doc()doc";

static const char *__doc_mitsuba_Mesh_ray_intersect_triangle_impl =
R"doc(Ray-triangle intersection test

Uses the algorithm by Moeller and Trumbore discussed at
``http://www.acm.org/jgt/papers/MollerTrumbore97/code.html``.

Parameter ``index``:
    Index of the triangle to be intersected.

Parameter ``ray``:
    The ray segment to be used for the intersection query.

Returns:
    Returns an ordered tuple ``(mask, u, v, t)``, where ``mask``
    indicates whether an intersection was found, ``t`` contains the
    distance from the ray origin to the intersection point, and ``u``
    and ``v`` contains the first two components of the intersection in
    barycentric coordinates)doc";

static const char *__doc_mitsuba_Mesh_ray_intersect_triangle_packet = R"doc()doc";

static const char *__doc_mitsuba_Mesh_ray_intersect_triangle_packet_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_ray_intersect_triangle_packet_3 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_ray_intersect_triangle_scalar = R"doc()doc";

static const char *__doc_mitsuba_Mesh_recompute_bbox = R"doc(Recompute the bounding box (e.g. after modifying the vertex positions))doc";

static const char *__doc_mitsuba_Mesh_recompute_vertex_normals = R"doc(Compute smooth vertex normals and replace the current normal values)doc";

static const char *__doc_mitsuba_Mesh_remove_attribute =
R"doc(Remove an attribute with the given ``name``.

Affects both mesh and texture attributes.

Throws an exception if the attribute was not previously registered.)doc";

static const char *__doc_mitsuba_Mesh_sample_position = R"doc()doc";

static const char *__doc_mitsuba_Mesh_sample_precomputed_silhouette = R"doc()doc";

static const char *__doc_mitsuba_Mesh_sample_silhouette = R"doc()doc";

static const char *__doc_mitsuba_Mesh_set_bsdf = R"doc(Set the shape's BSDF)doc";

static const char *__doc_mitsuba_Mesh_set_scene = R"doc()doc";

static const char *__doc_mitsuba_Mesh_surface_area = R"doc()doc";

static const char *__doc_mitsuba_Mesh_to_string = R"doc(Return a human-readable string representation of the shape contents.)doc";

static const char *__doc_mitsuba_Mesh_traverse = R"doc(@})doc";

static const char *__doc_mitsuba_Mesh_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_Mesh_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Mesh_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Mesh_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Mesh_vertex_count = R"doc(Return the total number of vertices)doc";

static const char *__doc_mitsuba_Mesh_vertex_data_bytes = R"doc()doc";

static const char *__doc_mitsuba_Mesh_vertex_normal = R"doc(Returns the normal direction of the vertex with index ``index``)doc";

static const char *__doc_mitsuba_Mesh_vertex_normals_buffer = R"doc(Return vertex normals buffer)doc";

static const char *__doc_mitsuba_Mesh_vertex_normals_buffer_2 = R"doc(Const variant of vertex_normals_buffer.)doc";

static const char *__doc_mitsuba_Mesh_vertex_position = R"doc(Returns the world-space position of the vertex with index ``index``)doc";

static const char *__doc_mitsuba_Mesh_vertex_positions_buffer = R"doc(Return vertex positions buffer)doc";

static const char *__doc_mitsuba_Mesh_vertex_positions_buffer_2 = R"doc(Const variant of vertex_positions_buffer.)doc";

static const char *__doc_mitsuba_Mesh_vertex_texcoord = R"doc(Returns the UV texture coordinates of the vertex with index ``index``)doc";

static const char *__doc_mitsuba_Mesh_vertex_texcoords_buffer = R"doc(Return vertex texcoords buffer)doc";

static const char *__doc_mitsuba_Mesh_vertex_texcoords_buffer_2 = R"doc(Const variant of vertex_texcoords_buffer.)doc";

static const char *__doc_mitsuba_Mesh_write_ply =
R"doc(Write the mesh to a binary PLY file

Parameter ``filename``:
    Target file path on disk)doc";

static const char *__doc_mitsuba_Mesh_write_ply_2 =
R"doc(Write the mesh encoded in binary PLY format to a stream

Parameter ``stream``:
    Target stream that will receive the encoded output)doc";

static const char *__doc_mitsuba_MiOptixAccelData = R"doc(Stores multiple OptiXTraversables: one for the each type)doc";

static const char *__doc_mitsuba_MiOptixAccelData_HandleData = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_HandleData_buffer = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_HandleData_count = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_HandleData_handle = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_bspline_curves = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_custom_shapes = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_ellipsoids_meshes = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_linear_curves = R"doc()doc";

static const char *__doc_mitsuba_MiOptixAccelData_meshes = R"doc()doc";

static const char *__doc_mitsuba_MicrofacetDistribution =
R"doc(Implementation of the Beckman and GGX / Trowbridge-Reitz microfacet
distributions and various useful sampling routines

Based on the papers

"Microfacet Models for Refraction through Rough Surfaces" by Bruce
Walter, Stephen R. Marschner, Hongsong Li, and Kenneth E. Torrance

and

"Importance Sampling Microfacet-Based BSDFs using the Distribution of
Visible Normals" by Eric Heitz and Eugene D'Eon

The visible normal sampling code was provided by Eric Heitz and Eugene
D'Eon. An improvement of the Beckmann model sampling routine is
discussed in

"An Improved Visible Normal Sampling Routine for the Beckmann
Distribution" by Wenzel Jakob

An improvement of the GGX model sampling routine is discussed in "A
Simpler and Exact Sampling Routine for the GGX Distribution of Visible
Normals" by Eric Heitz)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_G = R"doc(Smith's separable shadowing-masking approximation)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_MicrofacetDistribution =
R"doc(Create an isotropic microfacet distribution of the specified type

Parameter ``type``:
    The desired type of microfacet distribution

Parameter ``alpha``:
    The surface roughness)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_MicrofacetDistribution_2 =
R"doc(Create an anisotropic microfacet distribution of the specified type

Parameter ``type``:
    The desired type of microfacet distribution

Parameter ``alpha_u``:
    The surface roughness in the tangent direction

Parameter ``alpha_v``:
    The surface roughness in the bitangent direction)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_MicrofacetDistribution_3 = R"doc(Create a microfacet distribution from a Property data structure)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_alpha = R"doc(Return the roughness (isotropic case))doc";

static const char *__doc_mitsuba_MicrofacetDistribution_alpha_u = R"doc(Return the roughness along the tangent direction)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_alpha_v = R"doc(Return the roughness along the bitangent direction)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_configure = R"doc()doc";

static const char *__doc_mitsuba_MicrofacetDistribution_eval =
R"doc(Evaluate the microfacet distribution function

Parameter ``m``:
    The microfacet normal)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_is_anisotropic = R"doc(Is this an anisotropic microfacet distribution?)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_is_isotropic = R"doc(Is this an isotropic microfacet distribution?)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_m_alpha_u = R"doc()doc";

static const char *__doc_mitsuba_MicrofacetDistribution_m_alpha_v = R"doc()doc";

static const char *__doc_mitsuba_MicrofacetDistribution_m_sample_visible = R"doc()doc";

static const char *__doc_mitsuba_MicrofacetDistribution_m_type = R"doc()doc";

static const char *__doc_mitsuba_MicrofacetDistribution_pdf =
R"doc(Returns the density function associated with the sample() function.

Parameter ``wi``:
    The incident direction (only relevant if visible normal sampling
    is used)

Parameter ``m``:
    The microfacet normal)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_project_roughness_2 = R"doc(Compute the squared 1D roughness along direction ``v``)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_sample =
R"doc(Draw a sample from the microfacet normal distribution and return the
associated probability density

Parameter ``wi``:
    The incident direction. Only used if visible normal sampling is
    enabled.

Parameter ``sample``:
    A uniformly distributed 2D sample

Returns:
    A tuple consisting of the sampled microfacet normal and the
    associated solid angle density)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_sample_visible = R"doc(Return whether or not only visible normals are sampled?)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_sample_visible_11 = R"doc(Visible normal sampling code for the alpha=1 case)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_scale_alpha = R"doc(Scale the roughness values by some constant)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_smith_g1 =
R"doc(Smith's shadowing-masking function for a single direction

Parameter ``v``:
    An arbitrary direction

Parameter ``m``:
    The microfacet normal)doc";

static const char *__doc_mitsuba_MicrofacetDistribution_type = R"doc(Return the distribution type)doc";

static const char *__doc_mitsuba_MicrofacetType = R"doc(Supported normal distribution functions)doc";

static const char *__doc_mitsuba_MicrofacetType_Beckmann = R"doc(Beckmann distribution derived from Gaussian random surfaces)doc";

static const char *__doc_mitsuba_MicrofacetType_GGX =
R"doc(GGX: Long-tailed distribution for very rough surfaces (aka.
Trowbridge-Reitz distr.))doc";

static const char *__doc_mitsuba_MitsubaViewer = R"doc(Main class of the Mitsuba user interface)doc";

static const char *__doc_mitsuba_MitsubaViewer_MitsubaViewer = R"doc(Create a new viewer interface)doc";

static const char *__doc_mitsuba_MitsubaViewer_Tab = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_append_tab = R"doc(Append an empty tab)doc";

static const char *__doc_mitsuba_MitsubaViewer_close_tab_impl = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_keyboard_event = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_load = R"doc(Load content (a scene or an image) into a tab)doc";

static const char *__doc_mitsuba_MitsubaViewer_m_btn_menu = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_btn_play = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_btn_reload = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_btn_settings = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_btn_stop = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_contents = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_progress_bar = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_progress_panel = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_tab_widget = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_tabs = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_m_view = R"doc()doc";

static const char *__doc_mitsuba_MitsubaViewer_perform_layout = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator =
R"doc(Abstract integrator that performs *recursive* Monte Carlo sampling
starting from the sensor

This class is almost identical to SamplingIntegrator. It stores two
additional fields that are helpful for recursive Monte Carlo
techniques: the maximum path depth, and the depth at which the Russian
Roulette path termination technique should start to become active.)doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_2 = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_3 = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_4 = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_5 = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_6 = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_MonteCarloIntegrator = R"doc(Create an integrator)doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_class_name = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_m_max_depth = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_m_rr_depth = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_MonteCarloIntegrator_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Normal = R"doc()doc";

static const char *__doc_mitsuba_Normal_Normal = R"doc()doc";

static const char *__doc_mitsuba_Normal_Normal_2 = R"doc()doc";

static const char *__doc_mitsuba_Normal_Normal_3 = R"doc()doc";

static const char *__doc_mitsuba_Normal_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Normal_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Object =
R"doc(Object base class with builtin reference counting

This class (in conjunction with the ``ref`` reference counter)
constitutes the foundation of an efficient reference-counted object
hierarchy.

We use an intrusive reference counting approach to avoid various
gnarly issues that arise in combined Python/C++ codebase, see the
following page for details:
https://nanobind.readthedocs.io/en/latest/ownership_adv.html

The counter provided by ``drjit::TraversableBase`` establishes a
unified reference count that is consistent across both C++ and Python.
It is more efficient than ``std::shared_ptr<T>`` (as no external
control block is needed) and works without Python actually being
present.

Object subclasses are *traversable*, that is, they expose methods that
Dr.Jit can use to walk through object graphs, discover attributes, and
potentially change them. This enables function freezing (@dr.freeze)
that must detect and apply changes when executing frozen functions.)doc";

static const char *__doc_mitsuba_ObjectType =
R"doc(Available scene object types

This enumeration lists high-level interfaces that can be implemented
by Mitsuba scene objects. The scene loader uses these to ensure that a
loaded object matches the expected interface.

Note: This enum is forward-declared at the beginning of the file to
allow its usage in macros that appear before the full definition.)doc";

static const char *__doc_mitsuba_ObjectType_BSDF = R"doc(A bidirectional reflectance distribution function)doc";

static const char *__doc_mitsuba_ObjectType_Emitter = R"doc(Emits radiance, subclasses Emitter)doc";

static const char *__doc_mitsuba_ObjectType_Film = R"doc(Storage representation of the sensor)doc";

static const char *__doc_mitsuba_ObjectType_Integrator = R"doc(A rendering algorithm aka. Integrator)doc";

static const char *__doc_mitsuba_ObjectType_Medium = R"doc(A participating medium)doc";

static const char *__doc_mitsuba_ObjectType_PhaseFunction = R"doc(A phase function characterizing scattering in volumes)doc";

static const char *__doc_mitsuba_ObjectType_ReconstructionFilter = R"doc(A filter used to reconstruct/resample images)doc";

static const char *__doc_mitsuba_ObjectType_Sampler = R"doc(Generates sample positions and directions, subclasses Sampler)doc";

static const char *__doc_mitsuba_ObjectType_Scene = R"doc(The top-level scene object. No subclasses exist)doc";

static const char *__doc_mitsuba_ObjectType_Sensor = R"doc(Carries out radiance measurements, subclasses Sensor)doc";

static const char *__doc_mitsuba_ObjectType_Shape = R"doc(Denotes an arbitrary shape (including meshes))doc";

static const char *__doc_mitsuba_ObjectType_Texture = R"doc(A 2D texture data source)doc";

static const char *__doc_mitsuba_ObjectType_Unknown = R"doc(The default returned by Object subclasses)doc";

static const char *__doc_mitsuba_ObjectType_Volume = R"doc(A 3D volume data source)doc";

static const char *__doc_mitsuba_Object_Object = R"doc(Import default constructors)doc";

static const char *__doc_mitsuba_Object_Object_2 = R"doc()doc";

static const char *__doc_mitsuba_Object_Object_3 = R"doc()doc";

static const char *__doc_mitsuba_Object_class_name = R"doc(Return the C++ class name of this object (e.g. "SmoothDiffuse"))doc";

static const char *__doc_mitsuba_Object_expand =
R"doc(Expand the object into a list of sub-objects and return them

In some cases, an Object instance is merely a container for a number
of sub-objects. In the context of Mitsuba, an example would be a
combined sun & sky emitter instantiated via XML, which recursively
expands into a separate sun & sky instance. This functionality is
supported by any Mitsuba object, hence it is located this level.)doc";

static const char *__doc_mitsuba_Object_id = R"doc(Return an identifier of the current instance (or empty if none))doc";

static const char *__doc_mitsuba_Object_parameters_changed =
R"doc(Update internal state after applying changes to parameters

This function should be invoked when attributes (obtained via
traverse) are modified in some way. The object can then update its
internal state so that derived quantities are consistent with the
change.

Parameter ``keys``:
    Optional list of names (obtained via traverse) corresponding to
    the attributes that have been modified. Can also be used to notify
    when this function is called from a parent object by adding a
    "parent" key to the list. When empty, the object should assume
    that any attribute might have changed.

Remark:
    The default implementation does nothing.

See also:
    TraversalCallback)doc";

static const char *__doc_mitsuba_Object_set_id = R"doc(Set the identifier of the current instance (no-op if not supported))doc";

static const char *__doc_mitsuba_Object_to_string =
R"doc(Return a human-readable string representation of the object's
contents.

This function is mainly useful for debugging purposes and should
ideally be implemented by all subclasses. The default implementation
simply returns ``MyObject[<address of 'this' pointer>]``, where
``MyObject`` is the name of the class.)doc";

static const char *__doc_mitsuba_Object_traverse =
R"doc(Traverse the attributes and object graph of this instance

Implementing this function enables recursive traversal of C++ scene
graphs. It is e.g. used to determine the set of differentiable
parameters when using Mitsuba for optimization.

Remark:
    The default implementation does nothing.

See also:
    TraversalCallback)doc";

static const char *__doc_mitsuba_Object_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Object_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Object_type = R"doc(Return the object type. The default is ObjectType::Unknown.)doc";

static const char *__doc_mitsuba_Object_variant_name = R"doc(Return the instance variant (empty if this is not a variant object))doc";

static const char *__doc_mitsuba_OptixDenoiser =
R"doc(Wrapper for the OptiX AI denoiser

The OptiX AI denoiser is wrapped in this object such that it can work
directly with Mitsuba types and its conventions.

The denoiser works best when applied to noisy renderings that were
produced with a Film which used the `box` ReconstructionFilter. With a
filter that spans multiple pixels, the denoiser might identify some
local variance as a feature of the scene and will not denoise it.)doc";

static const char *__doc_mitsuba_OptixDenoiser_2 = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_3 = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_4 = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_5 = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_6 = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_OptixDenoiser =
R"doc(Constructs an OptiX denoiser

Parameter ``input_size``:
    Resolution of noisy images that will be fed to the denoiser.

Parameter ``albedo``:
    Whether or not albedo information will also be given to the
    denoiser. This parameter is optional, by default it is false.

Parameter ``normals``:
    Whether or not shading normals information will also be given to
    the denoiser. This parameter is optional, by default it is false.

Parameter ``temporal``:
    Whether or not temporal information will also be given to the
    denoiser. This parameter is optional, by default it is false.

Parameter ``denoise_alpha``:
    Whether or not the alpha channel (if specified in the noisy input)
    should be denoised too. This parameter is optional, by default it
    is false.

Returns:
    A callable object which will apply the OptiX denoiser.)doc";

static const char *__doc_mitsuba_OptixDenoiser_OptixDenoiser_2 = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_class_name = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_denoiser = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_hdr_intensity = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_input_size = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_options = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_scratch = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_scratch_size = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_state = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_state_size = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_m_temporal = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_operator_call =
R"doc(Apply denoiser on inputs which are TensorXf objects.

Parameter ``noisy``:
    The noisy input. (tensor shape: (width, height, 3 | 4))

Parameter ``albedo``:
    Albedo information of the noisy rendering. This parameter is
    optional unless the OptixDenoiser was built with albedo support.
    (tensor shape: (width, height, 3))

Parameter ``normals``:
    Shading normal information of the noisy rendering. The normals
    must be in the coordinate frame of the sensor which was used to
    render the noisy input. This parameter is optional unless the
    OptixDenoiser was built with normals support. (tensor shape:
    (width, height, 3))

Parameter ``to_sensor``:
    A Transform4f which is applied to the ``normals`` parameter before
    denoising. This should be used to transform the normals into the
    correct coordinate frame. This parameter is optional, by default
    no transformation is applied.

Parameter ``flow``:
    With temporal denoising, this parameter is the optical flow
    between the previous frame and the current one. It should capture
    the 2D motion of each individual pixel. When this parameter is
    unknown, it can be set to a zero-initialized TensorXf of the
    correct size and still produce convincing results. This parameter
    is optional unless the OptixDenoiser was built with temporal
    denoising support. (tensor shape: (width, height, 2))

Parameter ``previous_denoised``:
    With temporal denoising, the previous denoised frame should be
    passed here. For the very first frame, the OptiX documentation
    recommends passing the noisy input for this argument. This
    parameter is optional unless the OptixDenoiser was built with
    temporal denoising support. (tensor shape: (width, height, 3 | 4))

Returns:
    The denoised input.)doc";

static const char *__doc_mitsuba_OptixDenoiser_operator_call_2 =
R"doc(Apply denoiser on inputs which are Bitmap objects.

Parameter ``noisy``:
    The noisy input. When passing additional information like albedo
    or normals to the denoiser, this Bitmap object must be a
    MultiChannel bitmap.

Parameter ``albedo_ch``:
    The name of the channel in the ``noisy`` parameter which contains
    the albedo information of the noisy rendering. This parameter is
    optional unless the OptixDenoiser was built with albedo support.

Parameter ``normals_ch``:
    The name of the channel in the ``noisy`` parameter which contains
    the shading normal information of the noisy rendering. The normals
    must be in the coordinate frame of the sensor which was used to
    render the noisy input. This parameter is optional unless the
    OptixDenoiser was built with normals support.

Parameter ``to_sensor``:
    A Transform4f which is applied to the ``normals`` parameter before
    denoising. This should be used to transform the normals into the
    correct coordinate frame. This parameter is optional, by default
    no transformation is applied.

Parameter ``flow_ch``:
    With temporal denoising, this parameter is name of the channel in
    the ``noisy`` parameter which contains the optical flow between
    the previous frame and the current one. It should capture the 2D
    motion of each individual pixel. When this parameter is unknown,
    it can be set to a zero-initialized TensorXf of the correct size
    and still produce convincing results. This parameter is optional
    unless the OptixDenoiser was built with temporal denoising
    support.

Parameter ``previous_denoised_ch``:
    With temporal denoising, this parameter is name of the channel in
    the ``noisy`` parameter which contains the previous denoised
    frame. For the very first frame, the OptiX documentation
    recommends passing the noisy input for this argument. This
    parameter is optional unless the OptixDenoiser was built with
    temporal denoising support.

Parameter ``noisy_ch``:
    The name of the channel in the ``noisy`` parameter which contains
    the shading normal information of the noisy rendering.

Returns:
    The denoised input.)doc";

static const char *__doc_mitsuba_OptixDenoiser_to_string = R"doc()doc";

static const char *__doc_mitsuba_OptixDenoiser_validate_input = R"doc(Helper function to validate tensor sizes)doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping = R"doc()doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping_OptixProgramGroupMapping = R"doc()doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping_at = R"doc()doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping_index = R"doc()doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping_mapping = R"doc()doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping_operator_array = R"doc()doc";

static const char *__doc_mitsuba_OptixProgramGroupMapping_operator_array_2 = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler =
R"doc(Interface for sampler plugins based on the PCG32 random number
generator)doc";

static const char *__doc_mitsuba_PCG32Sampler_2 = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_3 = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_4 = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_5 = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_6 = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_PCG32Sampler = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_PCG32Sampler_2 = R"doc(Copy state to a new PCG32Sampler object)doc";

static const char *__doc_mitsuba_PCG32Sampler_class_name = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_m_rng = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_schedule_state = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_seed = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_PCG32Sampler_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_ParamFlags =
R"doc(This list of flags is used to classify the different types of
parameters exposed by the plugins.

For instance, in the context of differentiable rendering, it is
important to know which parameters can be differentiated, and which of
those might introduce discontinuities in the Monte Carlo simulation.)doc";

static const char *__doc_mitsuba_ParamFlags_Differentiable = R"doc(Tracking gradients w.r.t. this parameter is allowed)doc";

static const char *__doc_mitsuba_ParamFlags_Discontinuous =
R"doc(Tracking gradients w.r.t. this parameter will introduce
discontinuities)doc";

static const char *__doc_mitsuba_ParamFlags_NonDifferentiable = R"doc(Tracking gradients w.r.t. this parameter is not allowed)doc";

static const char *__doc_mitsuba_ParamFlags_ReadOnly = R"doc(This parameter is read-only)doc";

static const char *__doc_mitsuba_PhaseFunction =
R"doc(Abstract phase function base-class.

This class provides an abstract interface to all Phase function
plugins in Mitsuba. It exposes functions for evaluating and sampling
the model.)doc";

static const char *__doc_mitsuba_PhaseFunction_2 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunction_3 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunction_4 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunction_5 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunction_6 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionContext = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionContext_PhaseFunctionContext = R"doc(//! @})doc";

static const char *__doc_mitsuba_PhaseFunctionContext_PhaseFunctionContext_2 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionContext_PhaseFunctionContext_3 = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionContext_component = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionContext_is_enabled =
R"doc(Checks whether a given phase function component type and index are
enabled in this context.)doc";

static const char *__doc_mitsuba_PhaseFunctionContext_mode = R"doc(Transported mode (radiance or importance))doc";

static const char *__doc_mitsuba_PhaseFunctionContext_reverse =
R"doc(Reverse the direction of light transport in the record

This updates the transport mode (radiance to importance and vice
versa).)doc";

static const char *__doc_mitsuba_PhaseFunctionContext_sampler = R"doc(Sampler object)doc";

static const char *__doc_mitsuba_PhaseFunctionContext_type_mask = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionFlags =
R"doc(This enumeration is used to classify phase functions into different
types, i.e. into isotropic, anisotropic and microflake phase
functions.

This can be used to optimize implementations to for example have less
overhead if the phase function is not a microflake phase function.)doc";

static const char *__doc_mitsuba_PhaseFunctionFlags_Anisotropic = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionFlags_Empty = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionFlags_Isotropic = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunctionFlags_Microflake = R"doc()doc";

static const char *__doc_mitsuba_PhaseFunction_PhaseFunction = R"doc(//! @})doc";

static const char *__doc_mitsuba_PhaseFunction_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_PhaseFunction_component_count = R"doc(Number of components this phase function is comprised of.)doc";

static const char *__doc_mitsuba_PhaseFunction_eval_pdf =
R"doc(Evaluates the phase function model value and PDF

The function returns the value (which often equals the PDF) of the
phase function in the query direction.

Parameter ``ctx``:
    A phase function sampling context, contains information about the
    transport mode

Parameter ``mi``:
    A medium interaction data structure describing the underlying
    medium position. The incident direction is obtained from the field
    ``mi.wi``.

Parameter ``wo``:
    An outgoing direction to evaluate.

Returns:
    The value and the sampling PDF of the phase function in direction
    wo)doc";

static const char *__doc_mitsuba_PhaseFunction_flags = R"doc(Flags for this phase function.)doc";

static const char *__doc_mitsuba_PhaseFunction_flags_2 = R"doc(Flags for a specific component of this phase function.)doc";

static const char *__doc_mitsuba_PhaseFunction_get_flags = R"doc(Return type of phase function)doc";

static const char *__doc_mitsuba_PhaseFunction_m_components = R"doc(Flags for each component of this phase function.)doc";

static const char *__doc_mitsuba_PhaseFunction_m_flags = R"doc(Type of phase function (e.g. anisotropic))doc";

static const char *__doc_mitsuba_PhaseFunction_max_projected_area = R"doc(Return the maximum projected area of the microflake distribution)doc";

static const char *__doc_mitsuba_PhaseFunction_projected_area =
R"doc(Returns the microflake projected area

The function returns the projected area of the microflake distribution
defining the phase function. For non-microflake phase functions, e.g.
isotropic or Henyey-Greenstein, this should return a value of 1.

Parameter ``mi``:
    A medium interaction data structure describing the underlying
    medium position. The incident direction is obtained from the field
    ``mi.wi``.

Returns:
    The projected area in direction ``mi.wi`` at position ``mi.p``)doc";

static const char *__doc_mitsuba_PhaseFunction_sample =
R"doc(Importance sample the phase function model

The function returns a sampled direction.

Parameter ``ctx``:
    A phase function sampling context, contains information about the
    transport mode

Parameter ``mi``:
    A medium interaction data structure describing the underlying
    medium position. The incident direction is obtained from the field
    ``mi.wi``.

Parameter ``sample1``:
    A uniformly distributed sample on :math:`[0,1]`. It is used to
    select the phase function component in multi-component models.

Parameter ``sample2``:
    A uniformly distributed sample on :math:`[0,1]^2`. It is used to
    generate the sampled direction.

Returns:
    A sampled direction wo and its corresponding weight and PDF)doc";

static const char *__doc_mitsuba_PhaseFunction_set_flags = R"doc(Set type of phase function)doc";

static const char *__doc_mitsuba_PhaseFunction_to_string = R"doc(Return a human-readable representation of the phase function)doc";

static const char *__doc_mitsuba_PhaseFunction_type = R"doc(//! @})doc";

static const char *__doc_mitsuba_PhaseFunction_variant_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_PluginManager =
R"doc(Plugin manager

The plugin manager's main feature is the create_object() function that
instantiates scene objects. To do its job, it loads external Mitsuba
plugins as needed.

When used from Python, it is also possible to register external
plugins so that they can be instantiated analogously.)doc";

static const char *__doc_mitsuba_PluginManager_PluginManager = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_PluginManagerPrivate = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_class_name = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_create_object =
R"doc(Create a plugin object with the provided information

This function potentially loads an external plugin module (if not
already present), creates an instance, verifies its type, and finally
returns the newly created object instance.

Parameter ``props``:
    A Properties instance containing all information required to find
    and construct the plugin.

Parameter ``variant``:
    The variant (e.g. 'scalar_rgb') of the plugin to instantiate

Parameter ``type``:
    The expected interface of the instantiated plugin. Mismatches here
    will produce an error message. Pass `ObjectType::Unknown` to
    disable this check.)doc";

static const char *__doc_mitsuba_PluginManager_create_object_2 =
R"doc(Create a plugin object with the provided information

This template function wraps the ordinary ``create_object()`` function
defined above. It automatically infers variant and object type from
the provided class 'T'.)doc";

static const char *__doc_mitsuba_PluginManager_d = R"doc()doc";

static const char *__doc_mitsuba_PluginManager_instance = R"doc(Return the global plugin manager)doc";

static const char *__doc_mitsuba_PluginManager_plugin_type =
R"doc(Get the type of a plugin by name, or return Object::Unknown if
unknown.)doc";

static const char *__doc_mitsuba_PluginManager_register_plugin =
R"doc(Register a new plugin variant with the plugin manager

Re-registering an already available (name, variant) pair is legal and
will release the previously registered variant.

Parameter ``name``:
    The name of the plugin.

Parameter ``variant``:
    The plugin variant (e.g., ``"scalar_rgb"``). Separate plugin
    variants must be registered independently.

Parameter ``instantiate``:
    A callback that creates an instance of the plugin.

Parameter ``release``:
    A callback that releases any (global) plugin state. Will, e.g., be
    called by PluginManager::reset().

Parameter ``payload``:
    An opaque pointer parameter that will be forwarded to both
    ``instantiate`` and ``release``.)doc";

static const char *__doc_mitsuba_PluginManager_release_all =
R"doc(Release registered plugins

This calls the PluginInfo::release callback of all registered plugins,
e.g., to enable garbage collection of classes in python. Note
dynamically loaded shared libraries of native plugins aren't unloaded
until the PluginManager class itself is destructed.)doc";

static const char *__doc_mitsuba_Point = R"doc()doc";

static const char *__doc_mitsuba_Point_Point = R"doc()doc";

static const char *__doc_mitsuba_Point_Point_2 = R"doc()doc";

static const char *__doc_mitsuba_Point_Point_3 = R"doc()doc";

static const char *__doc_mitsuba_Point_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Point_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_PositionSample =
R"doc(Generic sampling record for positions

This sampling record is used to implement techniques that draw a
position from a point, line, surface, or volume domain in 3D and
furthermore provide auxiliary information about the sample.

Apart from returning the position and (optionally) the surface normal,
the responsible sampling method must annotate the record with the
associated probability density and delta.)doc";

static const char *__doc_mitsuba_PositionSample_PositionSample =
R"doc(Create a position sampling record from a surface intersection

This is useful to determine the hypothetical sampling density on a
surface after hitting it using standard ray tracing. This happens for
instance in path tracing with multiple importance sampling.)doc";

static const char *__doc_mitsuba_PositionSample_PositionSample_2 = R"doc(Basic field constructor)doc";

static const char *__doc_mitsuba_PositionSample_PositionSample_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_PositionSample_4 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_PositionSample_5 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_delta =
R"doc(Set if the sample was drawn from a degenerate (Dirac delta)
distribution)doc";

static const char *__doc_mitsuba_PositionSample_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_n = R"doc(Sampled surface normal (if applicable))doc";

static const char *__doc_mitsuba_PositionSample_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PositionSample_p = R"doc(Sampled position)doc";

static const char *__doc_mitsuba_PositionSample_pdf = R"doc(Probability density at the sample)doc";

static const char *__doc_mitsuba_PositionSample_time = R"doc(Associated time value)doc";

static const char *__doc_mitsuba_PositionSample_uv =
R"doc(Optional: 2D sample position associated with the record

In some uses of this record, a sampled position may be associated with
an important 2D quantity, such as the texture coordinates on a
triangle mesh or a position on the aperture of a sensor. When
applicable, such positions are stored in the ``uv`` attribute.)doc";

static const char *__doc_mitsuba_PreliminaryIntersection =
R"doc(Stores preliminary information related to a ray intersection

This data structure is used as return type for the
Shape::ray_intersect_preliminary efficient ray intersection routine.
It stores whether the shape is intersected by a given ray, and cache
preliminary information about the intersection if that is the case.

If the intersection is deemed relevant, detailed intersection
information can later be obtained via the
compute_surface_interaction() method.)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_2 =
R"doc(Stores preliminary information related to a ray intersection

This data structure is used as return type for the
Shape::ray_intersect_preliminary efficient ray intersection routine.
It stores whether the shape is intersected by a given ray, and cache
preliminary information about the intersection if that is the case.

If the intersection is deemed relevant, detailed intersection
information can later be obtained via the
compute_surface_interaction() method.)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_PreliminaryIntersection = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_PreliminaryIntersection_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_PreliminaryIntersection_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_compute_surface_interaction =
R"doc(Compute and return detailed information related to a surface
interaction

Parameter ``ray``:
    Ray associated with the ray intersection

Parameter ``ray_flags``:
    Flags specifying which information should be computed

Returns:
    A data structure containing the detailed information)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_instance = R"doc(Stores a pointer to the parent instance (if applicable))doc";

static const char *__doc_mitsuba_PreliminaryIntersection_is_valid = R"doc(Is the current interaction valid?)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_PreliminaryIntersection_prim_index = R"doc(Primitive index, e.g. the triangle ID (if applicable))doc";

static const char *__doc_mitsuba_PreliminaryIntersection_prim_uv = R"doc(2D coordinates on the primitive surface parameterization)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_shape = R"doc(Pointer to the associated shape)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_shape_index = R"doc(Shape index, e.g. the shape ID in shapegroup (if applicable))doc";

static const char *__doc_mitsuba_PreliminaryIntersection_t = R"doc(Distance traveled along the ray)doc";

static const char *__doc_mitsuba_PreliminaryIntersection_zero =
R"doc(This callback method is invoked by dr::zeros<>, and takes care of
fields that deviate from the standard zero-initialization convention.
In this particular class, the ``t`` field should be set to an infinite
value to mark invalid intersection records.)doc";

static const char *__doc_mitsuba_Profiler = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase =
R"doc(List of 'phases' that are handled by the profiler. Note that a partial
order is assumed -- if a method "B" can occur in a call graph of
another method "A", then "B" must occur after "A" in the list below.)doc";

static const char *__doc_mitsuba_ProfilerPhase_BSDFEvaluate = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_BSDFSample = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_BitmapRead = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_BitmapWrite = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_CreateSurfaceInteraction = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_EndpointEvaluate = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_EndpointSampleDirection = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_EndpointSamplePosition = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_EndpointSampleRay = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_ImageBlockPut = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_InitAccel = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_InitScene = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_LoadGeometry = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_MediumEvaluate = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_MediumSample = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_PhaseFunctionEvaluate = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_PhaseFunctionSample = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_ProfilerPhaseCount = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_RayIntersect = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_RayTest = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_Render = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_SampleEmitter = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_SampleEmitterDirection = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_SampleEmitterRay = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_SamplingIntegratorSample = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_TextureEvaluate = R"doc()doc";

static const char *__doc_mitsuba_ProfilerPhase_TextureSample = R"doc()doc";

static const char *__doc_mitsuba_Profiler_static_initialization = R"doc()doc";

static const char *__doc_mitsuba_Profiler_static_shutdown = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter =
R"doc(General-purpose progress reporter

This class is used to track the progress of various operations that
might take longer than a second or so. It provides interactive
feedback when Mitsuba is run on the console, via the OpenGL GUI, or in
Jupyter Notebook.)doc";

static const char *__doc_mitsuba_ProgressReporter_ProgressReporter =
R"doc(Construct a new progress reporter.

Parameter ``label``:
    An identifying name for the operation taking place (e.g.
    "Rendering")

Parameter ``ptr``:
    Custom pointer payload to be delivered as part of progress
    messages)doc";

static const char *__doc_mitsuba_ProgressReporter_class_name = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_bar_size = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_bar_start = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_label = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_last_progress = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_last_update = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_line = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_payload = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_m_timer = R"doc()doc";

static const char *__doc_mitsuba_ProgressReporter_update =
R"doc(Update the progress to ``progress`` (which should be in the range [0,
1]))doc";

static const char *__doc_mitsuba_ProjectiveCamera =
R"doc(Projective camera interface

This class provides an abstract interface to several types of sensors
that are commonly used in computer graphics, such as perspective and
orthographic camera models.

The interface is meant to be implemented by any kind of sensor, whose
world to clip space transformation can be explained using only linear
operations on homogeneous coordinates.

A useful feature of ProjectiveCamera sensors is that their view can be
rendered using the traditional OpenGL pipeline.)doc";

static const char *__doc_mitsuba_ProjectiveCamera_2 = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_3 = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_4 = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_5 = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_6 = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_ProjectiveCamera = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_class_name = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_far_clip = R"doc(Return the far clip plane distance)doc";

static const char *__doc_mitsuba_ProjectiveCamera_focus_distance = R"doc(Return the distance to the focal plane)doc";

static const char *__doc_mitsuba_ProjectiveCamera_m_far_clip = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_m_focus_distance = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_m_near_clip = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_near_clip = R"doc(Return the near clip plane distance)doc";

static const char *__doc_mitsuba_ProjectiveCamera_projection_transform =
R"doc(Return the projection matrix of this sensor (camera space to sample
space transformation))doc";

static const char *__doc_mitsuba_ProjectiveCamera_traverse = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_ProjectiveCamera_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Properties =
R"doc(Associative container for passing configuration parameters to Mitsuba
plugins.

When Mitsuba scene objects (BSDFs, textures, emitters, etc.) are
instantiated, they receive their configuration through a Properties
object. This container maps parameter names to values of various
types: booleans, integers, floats, strings, colors, transforms, and
references to other scene objects.

The Properties class combines the convenience of a dictionary with
type safety and provides several key features:

- Fast O(1) insertion and lookup by property name. - Traversal of
properties in the original insertion order. - Automatic tracking of
queried properties. - Reference properties that can be used to build
object hierarchies.

## Basic C++ Usage

```
Properties props("plugin_name");

// Write to 'props':
props.set("color_value", ScalarColor3f(0.1f, 0.2f, 0.3f));
props.set("my_bsdf", bsdf); // ref<BSDF> or BSDF*

// Read from 'props':
ScalarColor3f value = props.get<ScalarColor3f>("color_value");
BSDF *bsdf = props.get<BSDF*>("my_bsdf");
```

## Iterating Over Properties

```
// Iterate over all properties
for (const auto &prop : props) {
    std::cout << prop.name() << " = " << prop.type() << std::endl;
}

// Iterate only over object properties
for (const auto &prop : props.objects()) {
    if (BSDF *bsdf = prop.try_get<BSDF>()) {
        // Process BSDF object
    } else if (Texture *texture = prop.try_get<Texture>()) {
        // Process Texture object
    }
}
```

## Iterator stability

It is legal to mutate the container (e.g., by adding/removing
elements) while iterating over its elements.

## Python API

In Python, Property instances implement a dictionary-like interface:

```
props = mi.Properties("plugin_name")

# Write to 'props':
props["color_value"] = mi.ScalarColor3f(0.1, 0.2, 0.3)

for k, v in props.items():
   print(f'{k} = {v}')
```

## Query Tracking

Each property stores a flag that tracks whether it has been accessed.
This helps detect configuration errors such as typos in parameter
names or unused parameters. The get() function automatically marks
parameters as queried.

Use the following methods to work with query tracking: -
was_queried(name): Check if a specific parameter was accessed -
unqueried(): Get a list of all parameters that were never accessed -
mark_queried(name): Manually mark a parameter as accessed

This is particularly useful during plugin initialization to warn users
about potentially misspelled or unnecessary parameters in their scene
descriptions.

## Caveats

Deleting parameters leaves unused entries ("tombstones") that reduce
memory efficiency especially following a large number of deletions.)doc";

static const char *__doc_mitsuba_Properties_Properties = R"doc(Construct an empty and unnamed properties object)doc";

static const char *__doc_mitsuba_Properties_Properties_2 = R"doc(Construct an empty properties object with a specific plugin name)doc";

static const char *__doc_mitsuba_Properties_Properties_3 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Properties_Properties_4 = R"doc(Move constructor)doc";

static const char *__doc_mitsuba_Properties_PropertiesPrivate = R"doc()doc";

static const char *__doc_mitsuba_Properties_Reference = R"doc(Represents an indirect dependence on another object by name)doc";

static const char *__doc_mitsuba_Properties_Reference_Reference = R"doc()doc";

static const char *__doc_mitsuba_Properties_Reference_id = R"doc()doc";

static const char *__doc_mitsuba_Properties_Reference_m_name = R"doc()doc";

static const char *__doc_mitsuba_Properties_Reference_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_Properties_Reference_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_Properties_ResolvedReference =
R"doc(Represents an indirect dependence that has been resolved to a specific
element of ``ParserState::nodes`` (by the
parser::transform_resolve_references pass))doc";

static const char *__doc_mitsuba_Properties_ResolvedReference_ResolvedReference = R"doc()doc";

static const char *__doc_mitsuba_Properties_ResolvedReference_index = R"doc()doc";

static const char *__doc_mitsuba_Properties_ResolvedReference_m_index = R"doc()doc";

static const char *__doc_mitsuba_Properties_ResolvedReference_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_Properties_ResolvedReference_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_Properties_Spectrum =
R"doc(Temporary data structure to store spectral data before expansion into
a plugin like ``RegularSpectrum`` or ``IrregularSpectrum``.)doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum = R"doc(Default constructor (creates empty spectrum))doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_2 = R"doc(Construct uniform spectrum)doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_3 = R"doc(Construct spectrum from wavelength-value pairs)doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_4 =
R"doc(Construct spectrum from string (either single value or
wavelength:value pairs))doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_5 = R"doc(Construct regular spectrum from string of values and wavelength range)doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_6 = R"doc(Construct regular spectrum from vector of values and wavelength range)doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_7 =
R"doc(Construct irregular spectrum from separate wavelength and value
strings)doc";

static const char *__doc_mitsuba_Properties_Spectrum_Spectrum_8 = R"doc(Construct spectrum from file)doc";

static const char *__doc_mitsuba_Properties_Spectrum_is_regular = R"doc(Check if wavelengths are regularly spaced)doc";

static const char *__doc_mitsuba_Properties_Spectrum_is_uniform = R"doc(Check if this is a uniform spectrum (single value, no wavelengths))doc";

static const char *__doc_mitsuba_Properties_Spectrum_m_regular = R"doc(True if wavelengths are regularly spaced)doc";

static const char *__doc_mitsuba_Properties_Spectrum_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_Properties_Spectrum_values = R"doc(Corresponding values, or uniform value if wavelengths is empty)doc";

static const char *__doc_mitsuba_Properties_Spectrum_wavelengths = R"doc(For sampled spectra: wavelength values (in nanometers))doc";

static const char *__doc_mitsuba_Properties_Type = R"doc(Enumeration of representable property types)doc";

static const char *__doc_mitsuba_Properties_Type_Any = R"doc(Generic type wrapper for arbitrary data exchange between plugins)doc";

static const char *__doc_mitsuba_Properties_Type_Bool = R"doc(Boolean value (true/false))doc";

static const char *__doc_mitsuba_Properties_Type_Color = R"doc(Tristimulus color value)doc";

static const char *__doc_mitsuba_Properties_Type_Float = R"doc(Floating point value)doc";

static const char *__doc_mitsuba_Properties_Type_Integer = R"doc(64-bit signed integer)doc";

static const char *__doc_mitsuba_Properties_Type_Object = R"doc(An arbitrary Mitsuba scene object)doc";

static const char *__doc_mitsuba_Properties_Type_Reference = R"doc(Indirect reference to another scene object (by name))doc";

static const char *__doc_mitsuba_Properties_Type_ResolvedReference = R"doc(Indirect reference to another scene object (by index))doc";

static const char *__doc_mitsuba_Properties_Type_Spectrum = R"doc(Spectrum data (uniform value or wavelength-value pairs))doc";

static const char *__doc_mitsuba_Properties_Type_String = R"doc(String)doc";

static const char *__doc_mitsuba_Properties_Type_Transform = R"doc(3x3 or 4x4 homogeneous coordinate transform)doc";

static const char *__doc_mitsuba_Properties_Type_Unknown = R"doc(Unknown/deleted property (used for tombstones))doc";

static const char *__doc_mitsuba_Properties_Type_Vector = R"doc(3D array)doc";

static const char *__doc_mitsuba_Properties_as_string = R"doc(Return one of the parameters (converting it to a string if necessary))doc";

static const char *__doc_mitsuba_Properties_as_string_2 =
R"doc(Return one of the parameters (converting it to a string if necessary,
with default value))doc";

static const char *__doc_mitsuba_Properties_begin = R"doc(Return an iterator representing the beginning of the container)doc";

static const char *__doc_mitsuba_Properties_d = R"doc()doc";

static const char *__doc_mitsuba_Properties_empty = R"doc(Check if there are no entries)doc";

static const char *__doc_mitsuba_Properties_end = R"doc(Return an iterator representing the end of the container)doc";

static const char *__doc_mitsuba_Properties_entry_name = R"doc(Get the name of an entry by index (for error messages))doc";

static const char *__doc_mitsuba_Properties_filter = R"doc(Return a range that only yields properties of the specified type)doc";

static const char *__doc_mitsuba_Properties_filtered_range = R"doc(Helper class to provide a range for filtered iteration)doc";

static const char *__doc_mitsuba_Properties_filtered_range_begin = R"doc()doc";

static const char *__doc_mitsuba_Properties_filtered_range_end = R"doc()doc";

static const char *__doc_mitsuba_Properties_filtered_range_filtered_range = R"doc()doc";

static const char *__doc_mitsuba_Properties_filtered_range_m_filter_type = R"doc()doc";

static const char *__doc_mitsuba_Properties_filtered_range_m_props = R"doc()doc";

static const char *__doc_mitsuba_Properties_get =
R"doc(Retrieve a scalar parameter by name

Look up the property ``name``. Raises an exception if the property
cannot be found, or when it has an incompatible type. Accessing the
parameter automatically marks it as queried (see was_queried).

The template parameter ``T`` may refer to:

- Strings (``std::string``)

- Arithmetic types (``bool``, ``float``, ``double``, ``uint32_t``,
``int32_t``, ``uint64_t``, ``int64_t``, ``size_t``).

- Points/vectors (``ScalarPoint2f``, ``ScalarPoint3f``,
`ScalarVector2f``, or ``ScalarVector3f``).

- Tri-stimulus color values (``ScalarColor3f``).

- Affine transformations (``ScalarTransform3f``,
``ScalarTransform4f``)

- Mitsuba object classes (``ref<BSDF>``, ``BSDF *``, etc.)

Both single/double precision versions of arithmetic types are
supported; the function will convert them as needed. The function
*cannot* be used to obtain vectorized (e.g. JIT-compiled) arrays.)doc";

static const char *__doc_mitsuba_Properties_get_2 =
R"doc(Retrieve a parameter (with default value)

Please see the get() function above for details. The main difference
of this overload is that it automatically substitutes a default value
``def_val`` when the requested parameter cannot be found. It function
raises an error if current parameter value has an incompatible type.)doc";

static const char *__doc_mitsuba_Properties_get_3 =
R"doc(Retrieve a scalar parameter by index

This is the primary implementation. All type conversions and error
checking is done here. The name-based get() is a thin wrapper.)doc";

static const char *__doc_mitsuba_Properties_get_any =
R"doc(Retrieve an arbitrarily typed value for inter-plugin communication

This method enables plugins to exchange custom types that are not
natively supported by the Properties system. It uses type-erased
storage to hold arbitrary C++ objects while preserving type safety
through runtime type checking.

The function raises an exception if the parameter does not exist or
cannot be cast to the requested type. Accessing the parameter
automatically marks it as queried.)doc";

static const char *__doc_mitsuba_Properties_get_emissive_texture =
R"doc(Retrieve an emissive texture parameter with variant-specific
conversions (see get_texture_impl for details))doc";

static const char *__doc_mitsuba_Properties_get_emissive_texture_2 =
R"doc(Retrieve an emissive texture parameter with default value and variant-
specific conversions (see get_texture_impl for details))doc";

static const char *__doc_mitsuba_Properties_get_impl = R"doc(Implementation detail of get())doc";

static const char *__doc_mitsuba_Properties_get_texture =
R"doc(Retrieve a texture parameter with variant-specific conversions (see
get_texture_impl for details))doc";

static const char *__doc_mitsuba_Properties_get_texture_2 =
R"doc(Retrieve a texture parameter with default value and variant-specific
conversions (see get_texture_impl for details))doc";

static const char *__doc_mitsuba_Properties_get_texture_impl =
R"doc(Retrieve a texture parameter (internal method)

This method exposes a low level interface for texture construction, in
general get_texture(), get_emissive_texture(), and
get_unbounded_texture() are preferable.

The method retrieves or construct a texture object (a subclass of
``mitsuba::Texture<...>``).

If the parameter already holds a texture object, this function returns
it directly. Otherwise, it creates an appropriate texture based on the
property type and the current variant. The exact behavior is:

**Float/Integer Values:** - Monochromatic variants: Create ``uniform``
texture with the value. - RGB/spectral variants: - For reflectance
spectra: Create ``uniform`` texture with the value. - For emission
spectra: Create ``d65`` texture with grayscale color.

**Color Values (RGB triplets):** - Monochromatic variants: Compute
luminance and create a ``uniform`` texture. - RGB/spectral variants: -
For emission spectra: Create ``d65`` texture. - For reflectance
spectra: Create ``srgb`` texture.

**Spectrum Values:** *Uniform spectrum (single value):* - RGB
variants: For emission spectra, create ``srgb`` texture with a color
that represents the RGB appearance of a uniform spectral emitter. -
All other cases: Create ``uniform`` texture.

*Wavelength-value pairs:* - Spectral variants: create a ``regular`` or
``irregular`` spectrum texture based on the regularity of the
wavelength-value pairs. - RGB/monochromatic variants: Pre-integrate
against the CIE color matching functions to convert to sRGB color,
then: - Monochromatic: Extract luminance and create ``uniform``
texture. - RGB: Create a ``srgb`` texture with the computed color.

Parameter ``name``:
    The property name to look up

Parameter ``emitter``:
    Set to true when retrieving textures for emission spectra

Parameter ``unbounded``:
    Set this parameter to true if the spectrum is not emissive but may
    still exceed the [0,1] range. An example would be the real or
    imaginary index of refraction. This is important when spectral
    upsampling is involved.)doc";

static const char *__doc_mitsuba_Properties_get_texture_impl_2 =
R"doc(Version of the above that switches to a default initialization when
the property was not specified)doc";

static const char *__doc_mitsuba_Properties_get_unbounded_texture =
R"doc(Retrieve an unbounded texture parameter with default value and
variant-specific conversions (see get_texture_impl for details))doc";

static const char *__doc_mitsuba_Properties_get_unbounded_texture_2 =
R"doc(Retrieve an unbounded texture parameter with variant-specific
conversions (see get_texture_impl for details))doc";

static const char *__doc_mitsuba_Properties_get_volume =
R"doc(Retrieve a volume parameter

This method retrieves a volume parameter, where ``T`` is a subclass of
``mitsuba::Volume<...>``.

Scalar and texture values are also accepted. In this case, the plugin
manager will automatically construct a ``constvolume`` instance.)doc";

static const char *__doc_mitsuba_Properties_get_volume_2 =
R"doc(Retrieve a volume parameter with float default

When the volume parameter doesn't exist, creates a constant volume
with the specified floating point value.)doc";

static const char *__doc_mitsuba_Properties_has_property = R"doc(Verify if a property with the specified name exists)doc";

static const char *__doc_mitsuba_Properties_hash =
R"doc(Compute a hash of the Properties object

This hash is suitable for deduplication and ignores: - The insertion
order of properties - The 'id' field (which assigns a name to the
object elsewhere) - Property names starting with '_arg_' (which are
auto-generated)

The hash function is designed to work with the equality operator for
identifying equivalent Properties objects that can be merged during
scene optimization.)doc";

static const char *__doc_mitsuba_Properties_id =
R"doc(Returns a unique identifier associated with this instance (or an empty
string)

The ID is used to enable named references by other plugins)doc";

static const char *__doc_mitsuba_Properties_key_index =
R"doc(Find the index of a property by name

Returns:
    The index in the internal storage, or ``size_t(-1)`` if not found)doc";

static const char *__doc_mitsuba_Properties_key_index_checked =
R"doc(Find the index of a property by name or raise an exception if the
entry was not found.

Returns:
    The index in the internal storage)doc";

static const char *__doc_mitsuba_Properties_key_iterator = R"doc(Forward declaration of iterator)doc";

static const char *__doc_mitsuba_Properties_mark_queried =
R"doc(Manually mark a certain property as queried

Parameter ``name``:
    The property name

Parameter ``value``:
    Whether to mark as queried (true) or unqueried (false)

Returns:
    ``True`` upon success)doc";

static const char *__doc_mitsuba_Properties_mark_queried_2 =
R"doc(Mark a property as queried by its internal index

This method is used internally by the iterator to mark properties as
accessed only after successful type casting.)doc";

static const char *__doc_mitsuba_Properties_maybe_append =
R"doc(Get or create a property entry by name

This method looks up an existing property by name. If found and
raise_if_exists is true, it raises an exception. If not found, it
creates a new entry with the given name and an empty value.)doc";

static const char *__doc_mitsuba_Properties_merge =
R"doc(Merge another properties record into the current one.

Existing properties will be overwritten with the values from ``props``
if they have the same name.)doc";

static const char *__doc_mitsuba_Properties_objects = R"doc(Return a range that only yields Object-type properties)doc";

static const char *__doc_mitsuba_Properties_operator_assign = R"doc(Copy assignment operator)doc";

static const char *__doc_mitsuba_Properties_operator_assign_2 = R"doc(Move assignment operator)doc";

static const char *__doc_mitsuba_Properties_operator_eq = R"doc(Equality comparison operator)doc";

static const char *__doc_mitsuba_Properties_operator_ne = R"doc(Inequality comparison operator)doc";

static const char *__doc_mitsuba_Properties_plugin_name = R"doc(Get the plugin name)doc";

static const char *__doc_mitsuba_Properties_raise_any_type_error = R"doc(Raise an exception when get_any() has incompatible type)doc";

static const char *__doc_mitsuba_Properties_raise_object_type_error = R"doc(Raise an exception when an object has incompatible type)doc";

static const char *__doc_mitsuba_Properties_remove_property =
R"doc(Remove a property with the specified name

Returns:
    ``True`` upon success)doc";

static const char *__doc_mitsuba_Properties_rename_property =
R"doc(Rename a property

Changes the name of an existing property while preserving its value
and queried status.

Parameter ``old_name``:
    The current name of the property

Parameter ``new_name``:
    The new name for the property

Returns:
    ``True`` upon success, ``False`` if the old property doesn't exist
    or new name already exists)doc";

static const char *__doc_mitsuba_Properties_set =
R"doc(Set a parameter value

When a parameter with a matching names is already present, the method
raises an exception if ``raise_if_exists`` is set (the default).
Otherwise, it replaces the parameter.

The parameter is initially marked as unqueried (see was_queried).)doc";

static const char *__doc_mitsuba_Properties_set_any =
R"doc(Set an arbitrarily typed value for inter-plugin communication

This method allows storing arbitrary data types that cannot be
represented by the standard Properties types. The value is stored in a
type-erased Any container and can be retrieved later using
get_any<T>().)doc";

static const char *__doc_mitsuba_Properties_set_id = R"doc(Set the unique identifier associated with this instance)doc";

static const char *__doc_mitsuba_Properties_set_impl = R"doc(Implementation detail of set())doc";

static const char *__doc_mitsuba_Properties_set_impl_2 = R"doc()doc";

static const char *__doc_mitsuba_Properties_set_plugin_name = R"doc(Set the plugin name)doc";

static const char *__doc_mitsuba_Properties_size = R"doc(Return the number of entries)doc";

static const char *__doc_mitsuba_Properties_try_get =
R"doc(Try to retrieve a property value without implicit conversions

This method attempts to retrieve a property value of type T. Unlike
get<T>(), it returns a pointer to the stored value without performing
any implicit conversions. If the property doesn't exist, has a
different type, or would require conversion, it returns nullptr.

For Object-derived types, dynamic_cast is used to check type
compatibility. The property is only marked as queried if retrieval
succeeds.

Template parameter ``T``:
    The requested type

Parameter ``name``:
    Property name

Returns:
    Pointer to the value if successful, nullptr otherwise)doc";

static const char *__doc_mitsuba_Properties_try_get_2 = R"doc(Try to get a property value by index without implicit conversions)doc";

static const char *__doc_mitsuba_Properties_type =
R"doc(Returns the type of an existing property.

Raises an exception if the property does not exist.)doc";

static const char *__doc_mitsuba_Properties_unqueried = R"doc(Return the list of unqueried attributed)doc";

static const char *__doc_mitsuba_Properties_was_queried =
R"doc(Check if a certain property was queried

Mitsuba assigns a queried bit with every parameter. Unqueried
parameters are detected to issue warnings, since this is usually
indicative of typos.)doc";

static const char *__doc_mitsuba_RadicalInverse =
R"doc(Efficient implementation of a radical inverse function with prime
bases including scrambled versions.

This class is used to implement Halton and Hammersley sequences for
QMC integration in Mitsuba.)doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_divisor = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_recip = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_PrimeBase_value = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_RadicalInverse =
R"doc(Precompute data structures that are used to evaluate the radical
inverse and scrambled radical inverse function

Parameter ``max_base``:
    Sets the value of the largest prime number base. The default
    interval [2, 8161] contains exactly 1024 prime bases.

Parameter ``scramble``:
    Selects the desired permutation type, where ``-1`` denotes the
    Faure permutations; any other number causes a pseudorandom
    permutation to be built seeded by the value of ``scramble``.)doc";

static const char *__doc_mitsuba_RadicalInverse_base =
R"doc(Returns the n-th prime base used by the sequence

These prime numbers are used as bases in the radical inverse function
implementation.)doc";

static const char *__doc_mitsuba_RadicalInverse_bases =
R"doc(Return the number of prime bases for which precomputed tables are
available)doc";

static const char *__doc_mitsuba_RadicalInverse_class_name = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_compute_faure_permutations =
R"doc(Compute the Faure permutations using dynamic programming

For reference, see "Good permutations for extreme discrepancy" by
Henri Faure, Journal of Number Theory, Vol. 42, 1, 1992.)doc";

static const char *__doc_mitsuba_RadicalInverse_eval =
R"doc(Calculate the value of the radical inverse function

This function is used as a building block to construct Halton and
Hammersley sequences. Roughly, it computes a b-ary representation of
the input value ``index``, mirrors it along the decimal point, and
returns the resulting fractional value. The implementation here uses
prime numbers for ``b``.

Parameter ``base_index``:
    Selects the n-th prime that is used as a base when computing the
    radical inverse function (0 corresponds to 2, 1->3, 2->5, etc.).
    The value specified here must be between 0 and 1023.

Parameter ``index``:
    Denotes the index that should be mapped through the radical
    inverse function)doc";

static const char *__doc_mitsuba_RadicalInverse_eval_scrambled =
R"doc(Calculate a scrambled radical inverse function

This function is used as a building block to construct permuted Halton
and Hammersley sequence variants. It works like the normal radical
inverse function eval(), except that every digit is run through an
extra scrambling permutation.)doc";

static const char *__doc_mitsuba_RadicalInverse_inverse_permutation =
R"doc(Return the inverse permutation corresponding to the given prime number
basis)doc";

static const char *__doc_mitsuba_RadicalInverse_invert_permutation = R"doc(Invert one of the permutations)doc";

static const char *__doc_mitsuba_RadicalInverse_m_base = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_base_count = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_inv_permutation_storage = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_inv_permutations = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_permutation_storage = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_permutations = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_m_scramble = R"doc()doc";

static const char *__doc_mitsuba_RadicalInverse_permutation = R"doc(Return the permutation corresponding to the given prime number basis)doc";

static const char *__doc_mitsuba_RadicalInverse_scramble = R"doc(Return the original scramble value)doc";

static const char *__doc_mitsuba_RadicalInverse_to_string = R"doc(Return a human-readable string representation)doc";

static const char *__doc_mitsuba_Ray =
R"doc(Simple n-dimensional ray segment data structure

Along with the ray origin and direction, this data structure
additionally stores a maximum ray position ``maxt``, a time value
``time`` as well a the wavelength information associated with the ray.)doc";

static const char *__doc_mitsuba_RayDifferential =
R"doc(Ray differential -- enhances the basic ray class with offset rays for
two adjacent pixels on the view plane)doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential = R"doc(Construct from a Ray instance)doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential_2 = R"doc(Construct a new ray (o, d) at time 'time')doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential_3 = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential_4 = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_RayDifferential_5 = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_d_x = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_d_y = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_fields = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_has_differentials = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_labels = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_name = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_o_x = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_o_y = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_RayDifferential_scale_differential = R"doc()doc";

static const char *__doc_mitsuba_RayFlags =
R"doc(This list of flags is used to determine which members of
SurfaceInteraction should be computed when calling
compute_surface_interaction().

It also specifies whether the SurfaceInteraction should be
differentiable with respect to the shapes parameters.)doc";

static const char *__doc_mitsuba_RayFlags_All = R"doc(//! Compound compute flags)doc";

static const char *__doc_mitsuba_RayFlags_AllNonDifferentiable = R"doc(Compute all fields of the surface interaction ignoring shape's motion)doc";

static const char *__doc_mitsuba_RayFlags_DetachShape = R"doc(Derivatives of the SurfaceInteraction fields ignore shape's motion)doc";

static const char *__doc_mitsuba_RayFlags_Empty = R"doc(No flags set)doc";

static const char *__doc_mitsuba_RayFlags_FollowShape = R"doc(Derivatives of the SurfaceInteraction fields follow shape's motion)doc";

static const char *__doc_mitsuba_RayFlags_Minimal = R"doc(Compute position and geometric normal)doc";

static const char *__doc_mitsuba_RayFlags_ShadingFrame = R"doc(Compute shading normal and shading frame)doc";

static const char *__doc_mitsuba_RayFlags_UV = R"doc(Compute UV coordinates)doc";

static const char *__doc_mitsuba_RayFlags_dNGdUV = R"doc(Compute the geometric normal partials wrt. the UV coordinates)doc";

static const char *__doc_mitsuba_RayFlags_dNSdUV = R"doc(Compute the shading normal partials wrt. the UV coordinates)doc";

static const char *__doc_mitsuba_RayFlags_dPdUV = R"doc(Compute position partials wrt. UV coordinates)doc";

static const char *__doc_mitsuba_Ray_Ray = R"doc(Construct a new ray (o, d) at time 'time')doc";

static const char *__doc_mitsuba_Ray_Ray_2 = R"doc(Construct a new ray (o, d) with time)doc";

static const char *__doc_mitsuba_Ray_Ray_3 = R"doc(Construct a new ray (o, d) with bounds)doc";

static const char *__doc_mitsuba_Ray_Ray_4 = R"doc(Copy a ray, but change the maxt value)doc";

static const char *__doc_mitsuba_Ray_Ray_5 = R"doc()doc";

static const char *__doc_mitsuba_Ray_Ray_6 = R"doc()doc";

static const char *__doc_mitsuba_Ray_Ray_7 = R"doc()doc";

static const char *__doc_mitsuba_Ray_d = R"doc(Ray direction)doc";

static const char *__doc_mitsuba_Ray_fields = R"doc()doc";

static const char *__doc_mitsuba_Ray_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Ray_labels = R"doc()doc";

static const char *__doc_mitsuba_Ray_maxt = R"doc(Maximum position on the ray segment)doc";

static const char *__doc_mitsuba_Ray_name = R"doc()doc";

static const char *__doc_mitsuba_Ray_o = R"doc(Ray origin)doc";

static const char *__doc_mitsuba_Ray_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Ray_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Ray_operator_call = R"doc(Return the position of a point along the ray)doc";

static const char *__doc_mitsuba_Ray_reverse = R"doc(Return a ray that points into the opposite direction)doc";

static const char *__doc_mitsuba_Ray_time = R"doc(Time value associated with this ray)doc";

static const char *__doc_mitsuba_Ray_wavelengths = R"doc(Wavelength associated with the ray)doc";

static const char *__doc_mitsuba_ReconstructionFilter =
R"doc(Generic interface to separable image reconstruction filters

When resampling bitmaps or adding samples to a rendering in progress,
Mitsuba first convolves them with a image reconstruction filter.
Various kinds are implemented as subclasses of this interface.

Because image filters are generally too expensive to evaluate for each
sample, the implementation of this class internally precomputes an
discrete representation, whose resolution given by
MI_FILTER_RESOLUTION.)doc";

static const char *__doc_mitsuba_ReconstructionFilter_2 = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_3 = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_4 = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_5 = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_6 = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_ReconstructionFilter = R"doc(Create a new reconstruction filter)doc";

static const char *__doc_mitsuba_ReconstructionFilter_border_size = R"doc(Return the block border size required when rendering with this filter)doc";

static const char *__doc_mitsuba_ReconstructionFilter_class_name = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_eval = R"doc(Evaluate the filter function)doc";

static const char *__doc_mitsuba_ReconstructionFilter_eval_discretized =
R"doc(Evaluate a discretized version of the filter (generally faster than
'eval'))doc";

static const char *__doc_mitsuba_ReconstructionFilter_init_discretization = R"doc(Mandatory initialization prior to calls to eval_discretized())doc";

static const char *__doc_mitsuba_ReconstructionFilter_is_box_filter = R"doc(Check whether this is a box filter?)doc";

static const char *__doc_mitsuba_ReconstructionFilter_m_border_size = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_m_radius = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_m_scale_factor = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_m_values = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_radius = R"doc(Return the filter's width)doc";

static const char *__doc_mitsuba_ReconstructionFilter_type = R"doc()doc";

static const char *__doc_mitsuba_ReconstructionFilter_variant_name = R"doc()doc";

static const char *__doc_mitsuba_Resampler =
R"doc(Utility class for efficiently resampling discrete datasets to
different resolutions

Template parameter ``Scalar``:
    Denotes the underlying floating point data type (i.e. ``half``,
    ``float``, or ``double``))doc";

static const char *__doc_mitsuba_Resampler_Resampler =
R"doc(Create a new Resampler object that transforms between the specified
resolutions

This constructor precomputes all information needed to efficiently
perform the desired resampling operation. For that reason, it is most
efficient if it can be used repeatedly (e.g. to resample the equal-
sized rows of a bitmap)

Parameter ``source_res``:
    Source resolution

Parameter ``target_res``:
    Desired target resolution)doc";

static const char *__doc_mitsuba_Resampler_boundary_condition =
R"doc(Return the boundary condition that should be used when looking up
samples outside of the defined input domain)doc";

static const char *__doc_mitsuba_Resampler_clamp =
R"doc(Returns the range to which resampled values will be clamped

The default is -infinity to infinity (i.e. no clamping is used))doc";

static const char *__doc_mitsuba_Resampler_lookup = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_bc = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_clamp = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_fast_end = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_fast_start = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_source_res = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_start = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_taps = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_target_res = R"doc()doc";

static const char *__doc_mitsuba_Resampler_m_weights = R"doc()doc";

static const char *__doc_mitsuba_Resampler_resample =
R"doc(Resample a multi-channel array and clamp the results to a specified
valid range

Parameter ``source``:
    Source array of samples

Parameter ``target``:
    Target array of samples

Parameter ``source_stride``:
    Stride of samples in the source array. A value of '1' implies that
    they are densely packed.

Parameter ``target_stride``:
    Stride of samples in the source array. A value of '1' implies that
    they are densely packed.

Parameter ``channels``:
    Number of channels to be resampled)doc";

static const char *__doc_mitsuba_Resampler_resample_internal = R"doc()doc";

static const char *__doc_mitsuba_Resampler_set_boundary_condition =
R"doc(Set the boundary condition that should be used when looking up samples
outside of the defined input domain

The default is FilterBoundaryCondition::Clamp)doc";

static const char *__doc_mitsuba_Resampler_set_clamp = R"doc(If specified, resampled values will be clamped to the given range)doc";

static const char *__doc_mitsuba_Resampler_source_resolution = R"doc(Return the reconstruction filter's source resolution)doc";

static const char *__doc_mitsuba_Resampler_taps = R"doc(Return the number of taps used by the reconstruction filter)doc";

static const char *__doc_mitsuba_Resampler_target_resolution = R"doc(Return the reconstruction filter's target resolution)doc";

static const char *__doc_mitsuba_Resampler_to_string = R"doc(Return a human-readable summary)doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams =
R"doc(The parameters of the SGGX phase function stored as a pair of 3D
vectors [[S_xx, S_yy, S_zz], [S_xy, S_xz, S_yz]])doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_SGGXPhaseFunctionParams =
R"doc(Construct from a pair of 3D vectors [S_xx, S_yy, S_zz] and [S_xy,
S_xz, S_yz] that correspond to the entries of a symmetric positive-
definite 3x3 matrix.)doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_SGGXPhaseFunctionParams_2 = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_SGGXPhaseFunctionParams_3 = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_SGGXPhaseFunctionParams_4 = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_diag = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_fields = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_labels = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_name = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_off_diag = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_SGGXPhaseFunctionParams_operator_const_Array = R"doc()doc";

static const char *__doc_mitsuba_Sampler =
R"doc(Base class of all sample generators.

A *sampler* provides a convenient abstraction around methods that
generate uniform pseudo- or quasi-random points within a conceptual
infinite-dimensional unit hypercube \f$[0,1]^\infty$\f. This involves
two main operations: by querying successive component values of such
an infinite-dimensional point (next_1d(), next_2d()), or by discarding
the current point and generating another one (advance()).

Scalar and vectorized rendering algorithms interact with the sampler
interface in a slightly different way:

Scalar rendering algorithm:

1. The rendering algorithm first invokes seed() to initialize the
sampler state.

2. The first pixel sample can now be computed, after which advance()
needs to be invoked. This repeats until all pixel samples have been
generated. Note that some implementations need to be configured for a
certain number of pixel samples, and exceeding these will lead to an
exception being thrown.

3. While computing a pixel sample, the rendering algorithm usually
requests 1D or 2D component blocks using the next_1d() and next_2d()
functions before moving on to the next sample.

A vectorized rendering algorithm effectively queries multiple sample
generators that advance in parallel. This involves the following
steps:

1. The rendering algorithm invokes set_samples_per_wavefront() if each
rendering step is split into multiple passes (in which case fewer
samples should be returned per sample_1d() or sample_2d() call).

2. The rendering algorithm then invokes seed() to initialize the
sampler state, and to inform the sampler of the wavefront size, i.e.,
how many sampler evaluations should be performed in parallel,
accounting for all passes. The initialization ensures that the set of
parallel samplers is mutually statistically independent (in a
pseudo/quasi-random sense).

3. advance() can be used to advance to the next point.

4. As in the scalar approach, the rendering algorithm can request
batches of (pseudo-) random numbers using the next_1d() and next_2d()
functions.)doc";

static const char *__doc_mitsuba_Sampler_2 = R"doc()doc";

static const char *__doc_mitsuba_Sampler_3 = R"doc()doc";

static const char *__doc_mitsuba_Sampler_4 = R"doc()doc";

static const char *__doc_mitsuba_Sampler_5 = R"doc()doc";

static const char *__doc_mitsuba_Sampler_6 = R"doc()doc";

static const char *__doc_mitsuba_Sampler_Sampler = R"doc()doc";

static const char *__doc_mitsuba_Sampler_Sampler_2 = R"doc(Copy state to a new sampler object)doc";

static const char *__doc_mitsuba_Sampler_advance =
R"doc(Advance to the next sample.

A subsequent call to ``next_1d`` or ``next_2d`` will access the first
1D or 2D components of this sample.)doc";

static const char *__doc_mitsuba_Sampler_class_name = R"doc()doc";

static const char *__doc_mitsuba_Sampler_clone =
R"doc(Create a clone of this sampler.

Subsequent calls to the cloned sampler will produce the same random
numbers as the original sampler.

Remark:
    This method relies on the overload of the copy constructor.

May throw an exception if not supported.)doc";

static const char *__doc_mitsuba_Sampler_compute_per_sequence_seed =
R"doc(Generates a array of seeds where the seed values are unique per
sequence)doc";

static const char *__doc_mitsuba_Sampler_current_sample_index = R"doc(Return the index of the current sample)doc";

static const char *__doc_mitsuba_Sampler_fork =
R"doc(Create a fork of this sampler.

A subsequent call to ``seed`` is necessary to properly initialize the
internal state of the sampler.

May throw an exception if not supported.)doc";

static const char *__doc_mitsuba_Sampler_m_base_seed = R"doc(Base seed value)doc";

static const char *__doc_mitsuba_Sampler_m_dimension_index = R"doc(Index of the current dimension in the sample)doc";

static const char *__doc_mitsuba_Sampler_m_sample_count = R"doc(Number of samples per pixel)doc";

static const char *__doc_mitsuba_Sampler_m_sample_index = R"doc(Index of the current sample in the sequence)doc";

static const char *__doc_mitsuba_Sampler_m_samples_per_wavefront = R"doc(Number of samples per pass in wavefront modes (default is 1))doc";

static const char *__doc_mitsuba_Sampler_m_wavefront_size = R"doc(Size of the wavefront (or 0, if not seeded))doc";

static const char *__doc_mitsuba_Sampler_next_1d = R"doc(Retrieve the next component value from the current sample)doc";

static const char *__doc_mitsuba_Sampler_next_2d = R"doc(Retrieve the next two component values from the current sample)doc";

static const char *__doc_mitsuba_Sampler_sample_count = R"doc(Return the number of samples per pixel)doc";

static const char *__doc_mitsuba_Sampler_schedule_state = R"doc(dr::schedule() variables that represent the internal sampler state)doc";

static const char *__doc_mitsuba_Sampler_seed =
R"doc(Deterministically seed the underlying RNG, if applicable.

In the context of wavefront ray tracing & dynamic arrays, this
function must be called with ``wavefront_size`` matching the size of
the wavefront.)doc";

static const char *__doc_mitsuba_Sampler_seeded = R"doc(Return whether the sampler was seeded)doc";

static const char *__doc_mitsuba_Sampler_set_sample_count = R"doc(Set the number of samples per pixel)doc";

static const char *__doc_mitsuba_Sampler_set_samples_per_wavefront =
R"doc(Set the number of samples per pixel per pass in wavefront modes
(default is 1))doc";

static const char *__doc_mitsuba_Sampler_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Sampler_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Sampler_type = R"doc()doc";

static const char *__doc_mitsuba_Sampler_variant_name = R"doc()doc";

static const char *__doc_mitsuba_Sampler_wavefront_size = R"doc(Return the size of the wavefront (or 0, if not seeded))doc";

static const char *__doc_mitsuba_SamplingIntegrator =
R"doc(Abstract integrator that performs Monte Carlo sampling starting from
the sensor

Subclasses of this interface must implement the sample() method, which
performs Monte Carlo integration to return an unbiased statistical
estimate of the radiance value along a given ray.

The render() method then repeatedly invokes this estimator to compute
all pixels of the image.)doc";

static const char *__doc_mitsuba_SamplingIntegrator_2 = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_3 = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_4 = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_5 = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_6 = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_SamplingIntegrator = R"doc(//! @})doc";

static const char *__doc_mitsuba_SamplingIntegrator_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_SamplingIntegrator_m_block_size = R"doc(Size of (square) image blocks to render in parallel (in scalar mode))doc";

static const char *__doc_mitsuba_SamplingIntegrator_m_samples_per_pass =
R"doc(Number of samples to compute for each pass over the image blocks.

Must be a multiple of the total sample count per pixel. If set to
(uint32_t) -1, all the work is done in a single pass (default).)doc";

static const char *__doc_mitsuba_SamplingIntegrator_render = R"doc(//! @{ \name Integrator interface implementation)doc";

static const char *__doc_mitsuba_SamplingIntegrator_render_block = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_render_sample = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_sample =
R"doc(Sample the incident radiance along a ray.

Parameter ``scene``:
    The underlying scene in which the radiance function should be
    sampled

Parameter ``sampler``:
    A source of (pseudo-/quasi-) random numbers

Parameter ``ray``:
    A ray, optionally with differentials

Parameter ``medium``:
    If the ray is inside a medium, this parameter holds a pointer to
    that medium

Parameter ``aov``:
    Integrators may return one or more arbitrary output variables
    (AOVs) via this parameter. If ``nullptr`` is provided to this
    argument, no AOVs should be returned. Otherwise, the caller
    guarantees that space for at least ``aov_names().size()`` entries
    has been allocated.

Parameter ``active``:
    A mask that indicates which SIMD lanes are active

Returns:
    A pair containing a spectrum and a mask specifying whether a
    surface or medium interaction was sampled. False mask entries
    indicate that the ray "escaped" the scene, in which case the
    returned spectrum contains the contribution of environment maps,
    if present. The mask can be used to estimate a suitable alpha
    channel of a rendered image.

Remark:
    In the Python bindings, this function returns the ``aov`` output
    argument as an additional return value. In other words:

```
(spec, mask, aov) = integrator.sample(scene, sampler, ray, medium, active)
```)doc";

static const char *__doc_mitsuba_SamplingIntegrator_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_SamplingIntegrator_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Scene =
R"doc(Central scene data structure

Mitsuba's scene class encapsulates a tree of mitsuba Object instances
including emitters, sensors, shapes, materials, participating media,
the integrator (i.e. the method used to render the image) etc.

It organizes these objects into groups that can be accessed through
getters (see shapes(), emitters(), sensors(), etc.), and it provides
three key abstractions implemented on top of these groups,
specifically:

* Ray intersection queries and shadow ray tests (See
\ray_intersect_preliminary(), ray_intersect(), and ray_test()).

* Sampling rays approximately proportional to the emission profile of
light sources in the scene (see sample_emitter_ray())

* Sampling directions approximately proportional to the direct
radiance from emitters received at a given scene location (see
sample_emitter_direction()).)doc";

static const char *__doc_mitsuba_Scene_2 = R"doc()doc";

static const char *__doc_mitsuba_Scene_3 = R"doc()doc";

static const char *__doc_mitsuba_Scene_4 = R"doc()doc";

static const char *__doc_mitsuba_Scene_5 = R"doc()doc";

static const char *__doc_mitsuba_Scene_6 = R"doc()doc";

static const char *__doc_mitsuba_Scene_Scene = R"doc(Instantiate a scene from a Properties object)doc";

static const char *__doc_mitsuba_Scene_accel_init_cpu = R"doc(Create the ray-intersection acceleration data structure)doc";

static const char *__doc_mitsuba_Scene_accel_init_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_accel_parameters_changed_cpu = R"doc(Updates the ray-intersection acceleration data structure)doc";

static const char *__doc_mitsuba_Scene_accel_parameters_changed_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_accel_release_cpu = R"doc(Release the ray-intersection acceleration data structure)doc";

static const char *__doc_mitsuba_Scene_accel_release_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_bbox = R"doc(Return a bounding box surrounding the scene)doc";

static const char *__doc_mitsuba_Scene_class_name = R"doc()doc";

static const char *__doc_mitsuba_Scene_clear_shapes_dirty = R"doc(Unmarks all shapes as dirty)doc";

static const char *__doc_mitsuba_Scene_emitters = R"doc(Return the list of emitters)doc";

static const char *__doc_mitsuba_Scene_emitters_2 = R"doc(Return the list of emitters (const version))doc";

static const char *__doc_mitsuba_Scene_emitters_dr = R"doc(Return the list of emitters as a Dr.Jit array)doc";

static const char *__doc_mitsuba_Scene_environment = R"doc(Return the environment emitter (if any))doc";

static const char *__doc_mitsuba_Scene_eval_emitter_direction =
R"doc(Re-evaluate the incident direct radiance of the
sample_emitter_direction() method.

This function re-evaluates the incident direct radiance and sample
probability due to the emitter *so that division by * ``ds.pdf``
equals the sampling weight returned by sample_emitter_direction().
This may appear redundant, and indeed such a function would not find
use in "normal" rendering algorithms.

However, the ability to re-evaluate the contribution of a direct
illumination sample is important for differentiable rendering. For
example, we might want to track derivatives in the sampled direction
(``ds.d``) without also differentiating the sampling technique.

In contrast to pdf_emitter_direction(), evaluating this function can
yield a nonzero result in the case of emission profiles containing a
Dirac delta term (e.g. point or directional lights).

Parameter ``ref``:
    A 3D reference location within the scene, which may influence the
    sampling process.

Parameter ``ds``:
    A direction sampling record, which specifies the query location.

Returns:
    The incident radiance and discrete or solid angle density of the
    sample.)doc";

static const char *__doc_mitsuba_Scene_integrator = R"doc(Return the scene's integrator)doc";

static const char *__doc_mitsuba_Scene_integrator_2 = R"doc(Return the scene's integrator)doc";

static const char *__doc_mitsuba_Scene_invert_silhouette_sample =
R"doc(Map a silhouette segment to a point in boundary sample space

This method is the inverse of sample_silhouette(). The mapping from
boundary sample space to boundary segments is bijective.

Parameter ``ss``:
    The sampled boundary segment

Returns:
    The corresponding boundary sample space point)doc";

static const char *__doc_mitsuba_Scene_m_accel = R"doc(Acceleration data structure (IAS) (type depends on implementation))doc";

static const char *__doc_mitsuba_Scene_m_accel_handle = R"doc(Handle to the IAS used to ensure its lifetime in jit variants)doc";

static const char *__doc_mitsuba_Scene_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_children = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_emitter_distr = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_emitter_pmf = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_emitters = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_emitters_dr = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_environment = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_integrator = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_sensors = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_sensors_dr = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_shapegroups = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_shapes = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_shapes_dr = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_shapes_grad_enabled = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_silhouette_distr = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_silhouette_shapes = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_silhouette_shapes_dr = R"doc()doc";

static const char *__doc_mitsuba_Scene_m_thread_reordering = R"doc()doc";

static const char *__doc_mitsuba_Scene_parameters_changed = R"doc(Update internal state following a parameter update)doc";

static const char *__doc_mitsuba_Scene_pdf_emitter =
R"doc(Evaluate the discrete probability of the sample_emitter() technique
for the given a emitter index.)doc";

static const char *__doc_mitsuba_Scene_pdf_emitter_direction =
R"doc(Evaluate the PDF of direct illumination sampling

This function evaluates the probability density (per unit solid angle)
of the sampling technique implemented by the sample_emitter_direct()
function. The returned probability will always be zero when the
emission profile contains a Dirac delta term (e.g. point or
directional emitters/sensors).

Parameter ``ref``:
    A 3D reference location within the scene, which may influence the
    sampling process.

Parameter ``ds``:
    A direction sampling record, which specifies the query location.

Returns:
    The solid angle density of the sample)doc";

static const char *__doc_mitsuba_Scene_ray_intersect =
R"doc(Intersect a ray with the shapes comprising the scene and return a
detailed data structure describing the intersection, if one is found.

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of surface
interactions following the usual conventions.

This method is a convenience wrapper of the generalized version of
``ray_intersect``() below. It assumes that incoherent rays are being
traced, that the user desires access to all fields of the
SurfaceInteraction, and that no thread reordering is requested. In
other words, it simply invokes the general ``ray_intersect``()
overload with ``coherent=false``, ``ray_flags`` equal to
RayFlags::All, and ``reorder=false``.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Returns:
    A detailed surface interaction record. Its ``is_valid()`` method
    should be queried to check if an intersection was actually found.)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_2 =
R"doc(Intersect a ray with the shapes comprising the scene and return a
detailed data structure describing the intersection, if one is found

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of surface
interactions following the usual conventions.

This ray intersection method exposes two additional flags to control
the intersection process. Internally, it is split into two steps:

<ol>

* Finding a PreliminaryInteraction using the ray tracing backend
underlying the current variant (i.e., Mitsuba's builtin kd-tree,
Embree, or OptiX). This is done using the ray_intersect_preliminary()
function that is also available directly below (and preferable if a
full SurfaceInteraction is not needed.).

* Expanding the PreliminaryInteraction into a full SurfaceInteraction
(this part happens within Mitsuba/Dr.Jit and tracks derivative
information in AD variants of the system).

</ol>

The SurfaceInteraction data structure is large, and computing its
contents in the second step requires a non-trivial amount of
computation and sequence of memory accesses. The ``ray_flags``
parameter can be used to specify that only a sub-set of the full
intersection data structure actually needs to be computed, which can
improve performance.

In the context of differentiable rendering, the ``ray_flags``
parameter also influences how derivatives propagate between the input
ray, the shape parameters, and the computed intersection (see
RayFlags::FollowShape and RayFlags::DetachShape for details on this).
The default, RayFlags::All, propagates derivatives through all steps
of the intersection computation.

The ``coherent`` flag is a hint that can improve performance in the
first step of finding the PreliminaryInteraction if the input set of
rays is coherent (e.g., when they are generated by
Sensor::sample_ray(), which means that adjacent rays will traverse
essentially the same region of space). This flag is currently only
used by the combination of ``llvm_*`` variants and the Embree ray
tracing backend.

This method is a convenience wrapper of the generalized
``ray_intersect``() method below. It assumes that ``reorder=false``.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Parameter ``ray_flags``:
    An integer combining flag bits from RayFlags (merged using binary
    or).

Parameter ``coherent``:
    Setting this flag to ``True`` can noticeably improve performance
    when ``ray`` contains a coherent set of rays (e.g. primary camera
    rays), and when using ``llvm_*`` variants of the renderer along
    with Embree. It has no effect in scalar or CUDA/OptiX variants.

Returns:
    A detailed surface interaction record. Its ``is_valid()`` method
    should be queried to check if an intersection was actually found.)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_3 =
R"doc(Intersect a ray with the shapes comprising the scene and return a
detailed data structure describing the intersection, if one is found

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of surface
interactions following the usual conventions.

This generalized ray intersection method exposes two additional flags
to control the intersection process. Internally, it is split into two
steps:

<ol>

* Finding a PreliminaryInteraction using the ray tracing backend
underlying the current variant (i.e., Mitsuba's builtin kd-tree,
Embree, or OptiX). This is done using the ray_intersect_preliminary()
function that is also available directly below (and preferable if a
full SurfaceInteraction is not needed.).

* Expanding the PreliminaryInteraction into a full SurfaceInteraction
(this part happens within Mitsuba/Dr.Jit and tracks derivative
information in AD variants of the system).

</ol>

The SurfaceInteraction data structure is large, and computing its
contents in the second step requires a non-trivial amount of
computation and sequence of memory accesses. The ``ray_flags``
parameter can be used to specify that only a sub-set of the full
intersection data structure actually needs to be computed, which can
improve performance.

In the context of differentiable rendering, the ``ray_flags``
parameter also influences how derivatives propagate between the input
ray, the shape parameters, and the computed intersection (see
RayFlags::FollowShape and RayFlags::DetachShape for details on this).
The default, RayFlags::All, propagates derivatives through all steps
of the intersection computation.

The ``coherent`` flag is a hint that can improve performance in the
first step of finding the PreliminaryInteraction if the input set of
rays is coherent (e.g., when they are generated by
Sensor::sample_ray(), which means that adjacent rays will traverse
essentially the same region of space). This flag is currently only
used by the combination of ``llvm_*`` variants and the Embree ray
tracing backend.

The ``reorder`` flag is a trigger for the Shader Execution Reordering
(SER) feature on NVIDIA GPUs. It can improve performance in highly
divergent workloads by shuffling threads into coherent warps. This
shuffling operation uses the result of the intersection (the shape ID)
as a sorting key to group threads into coherent warps.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Parameter ``ray_flags``:
    An integer combining flag bits from RayFlags (merged using binary
    or).

Parameter ``coherent``:
    Setting this flag to ``True`` can noticeably improve performance
    when ``ray`` contains a coherent set of rays (e.g. primary camera
    rays), and when using ``llvm_*`` variants of the renderer along
    with Embree. It has no effect in scalar or CUDA/OptiX variants.

Parameter ``reorder``:
    Setting this flag to ``True`` will trigger a reordering of the
    threads using the GPU's Shader Execution Reordering (SER)
    functionality if the scene's ``allow_thread_reordering`` flag was
    also set. This flag has no effect in scalar or LLVM variants.

Parameter ``reorder_hint``:
    The reordering will always shuffle the threads based on the shape
    the thread's ray intersected. However, additional granularity can
    be achieved by providing an extra sorting key with this parameter.
    This flag has no effect in scalar or LLVM variants, or if the
    ``reorder`` parameter is ``False``.

Parameter ``reorder_hint_bits``:
    Number of bits from the ``reorder_hint`` to use (starting from the
    least significant bit). It is recommended to use as few as
    possible. At most, 16 bits can be used. This flag has no effect in
    scalar or LLVM variants, or if the ``reorder`` parameter is
    ``False``.

Returns:
    A detailed surface interaction record. Its ``is_valid()`` method
    should be queried to check if an intersection was actually found.)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_cpu = R"doc(Trace a ray)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_ray_intersect_naive =
R"doc(Ray intersection using a brute force search. Used in unit tests to
validate the kdtree-based ray tracer.

Remark:
    Not implemented by the Embree/OptiX backends)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_naive_cpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_ray_intersect_preliminary =
R"doc(Intersect a ray with the shapes comprising the scene and return
preliminary information, if one is found

This function invokes the ray tracing backend underlying the current
variant (i.e., Mitsuba's builtin kd-tree, Embree, or OptiX) and
returns preliminary intersection information consisting of

* the ray distance up to the intersection (if one is found).

* the intersected shape and primitive index.

* local UV coordinates of the intersection within the primitive.

* A pointer to the intersected shape or instance.

The information is only preliminary at this point, because it lacks
various other information (geometric and shading frame, texture
coordinates, curvature, etc.) that is generally needed by shading
models. In variants of Mitsuba that perform automatic differentiation,
it is important to know that computation done by the ray tracing
backend is not reflected in Dr.Jit's computation graph. The
ray_intersect() method will re-evaluate certain parts of the
computation with derivative tracking to rectify this.

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of preliminary
intersection records following the usual conventions.

This method is a convenience wrapper of the generalized version of
``ray_intersect_preliminary``() below, which assumes that no
reordering is requested. In other words, it simply invokes the general
``ray_intersect_preliminary``() overload with ``reorder=false``.

The ``coherent`` flag is a hint that can improve performance if the
input set of rays is coherent (e.g., when they are generated by
Sensor::sample_ray(), which means that adjacent rays will traverse
essentially the same region of space). This flag is currently only
used by the combination of ``llvm_*`` variants and the Embree ray
intersector.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Parameter ``coherent``:
    Setting this flag to ``True`` can noticeably improve performance
    when ``ray`` contains a coherent set of rays (e.g. primary camera
    rays), and when using ``llvm_*`` variants of the renderer along
    with Embree. It has no effect in scalar or CUDA/OptiX variants.

Returns:
    A preliminary surface interaction record. Its ``is_valid()``
    method should be queried to check if an intersection was actually
    found.)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_preliminary_2 =
R"doc(Intersect a ray with the shapes comprising the scene and return
preliminary information, if one is found

This function invokes the ray tracing backend underlying the current
variant (i.e., Mitsuba's builtin kd-tree, Embree, or OptiX) and
returns preliminary intersection information consisting of

* the ray distance up to the intersection (if one is found).

* the intersected shape and primitive index.

* local UV coordinates of the intersection within the primitive.

* A pointer to the intersected shape or instance.

The information is only preliminary at this point, because it lacks
various other information (geometric and shading frame, texture
coordinates, curvature, etc.) that is generally needed by shading
models. In variants of Mitsuba that perform automatic differentiation,
it is important to know that computation done by the ray tracing
backend is not reflected in Dr.Jit's computation graph. The
ray_intersect() method will re-evaluate certain parts of the
computation with derivative tracking to rectify this.

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of preliminary
intersection records following the usual conventions.

The ``coherent`` flag is a hint that can improve performance if the
input set of rays is coherent (e.g., when they are generated by
Sensor::sample_ray(), which means that adjacent rays will traverse
essentially the same region of space). This flag is currently only
used by the combination of ``llvm_*`` variants and the Embree ray
intersector.

The ``reorder`` flag is a trigger for the Shader Execution Reordering
(SER) feature on NVIDIA GPUs. It can improve performance in highly
divergent workloads by shuffling threads into coherent warps. This
shuffling operation uses the result of the intersection (the shape ID)
as a sorting key to group threads into coherent warps.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Parameter ``coherent``:
    Setting this flag to ``True`` can noticeably improve performance
    when ``ray`` contains a coherent set of rays (e.g. primary camera
    rays), and when using ``llvm_*`` variants of the renderer along
    with Embree. It has no effect in scalar or CUDA/OptiX variants.

Parameter ``reorder``:
    Setting this flag to ``True`` will trigger a reordering of the
    threads using the GPU's Shader Execution Reordering (SER)
    functionality if the scene's ``allow_thread_reordering`` flag was
    also set. This flag has no effect in scalar or LLVM variants.

Parameter ``reorder_hint``:
    The reordering will always shuffle the threads based on the shape
    the thread's ray intersected. However, additional granularity can
    be achieved by providing an extra sorting key with this parameter.
    This flag has no effect in scalar or LLVM variants, or if the
    ``reorder`` parameter is ``False``.

Parameter ``reorder_hint_bits``:
    Number of bits from the ``reorder_hint`` to use (starting from the
    least significant bit). It is recommended to use as few as
    possible. At most, 16 bits can be used. This flag has no effect in
    scalar or LLVM variants, or if the ``reorder`` parameter is
    ``False``.

Returns:
    A preliminary surface interaction record. Its ``is_valid()``
    method should be queried to check if an intersection was actually
    found.)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_preliminary_cpu = R"doc(Trace a ray and only return a preliminary intersection data structure)doc";

static const char *__doc_mitsuba_Scene_ray_intersect_preliminary_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_ray_test =
R"doc(Intersect a ray with the shapes comprising the scene and return a
boolean specifying whether or not an intersection was found.

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of booleans
following the usual conventions.

Testing for the mere presence of intersections is considerably faster
than finding an actual intersection, hence this function should be
preferred over ray_intersect() when geometric information about the
first visible intersection is not needed.

This method is a convenience wrapper of the generalized version of
``ray_test``() below, which assumes that incoherent rays are being
traced. In other words, it simply invokes the general ``ray_test``()
overload with ``coherent=false``.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Returns:
    ``True`` if an intersection was found)doc";

static const char *__doc_mitsuba_Scene_ray_test_2 =
R"doc(Intersect a ray with the shapes comprising the scene and return a
boolean specifying whether or not an intersection was found.

In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
function processes arrays of rays and returns arrays of booleans
following the usual conventions.

Testing for the mere presence of intersections is considerably faster
than finding an actual intersection, hence this function should be
preferred over ray_intersect() when geometric information about the
first visible intersection is not needed.

The ``coherent`` flag is a hint that can improve performance in the
first step of finding the PreliminaryInteraction if the input set of
rays is coherent, which means that adjacent rays will traverse
essentially the same region of space. This flag is currently only used
by the combination of ``llvm_*`` variants and the Embree ray tracing
backend.

Parameter ``ray``:
    A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
    information, which matters when the shapes are in motion

Parameter ``coherent``:
    Setting this flag to ``True`` can noticeably improve performance
    when ``ray`` contains a coherent set of rays (e.g. primary camera
    rays), and when using ``llvm_*`` variants of the renderer along
    with Embree. It has no effect in scalar or CUDA/OptiX variants.

Returns:
    ``True`` if an intersection was found)doc";

static const char *__doc_mitsuba_Scene_ray_test_cpu = R"doc(Trace a shadow ray)doc";

static const char *__doc_mitsuba_Scene_ray_test_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_sample_emitter =
R"doc(Sample one emitter in the scene and rescale the input sample for
reuse.

Currently, the sampling scheme implemented by the Scene class is very
simplistic (uniform).

Parameter ``sample``:
    A uniformly distributed number in [0, 1).

Returns:
    The index of the chosen emitter along with the sampling weight
    (equal to the inverse PDF), and the transformed random sample for
    reuse.)doc";

static const char *__doc_mitsuba_Scene_sample_emitter_direction =
R"doc(Direct illumination sampling routine

This method implements stochastic connections to emitters, which is
variously known as *emitter sampling*, *direct illumination sampling*,
or *next event estimation*.

The function expects a 3D reference location ``ref`` as input, which
may influence the sampling process. Normally, this would be the
location of a surface position being shaded. Ideally, the
implementation of this function should then draw samples proportional
to the scene's emission profile and the inverse square distance
between the reference point and the sampled emitter position. However,
approximations are acceptable as long as these are reflected in the
returned Monte Carlo sampling weight.

Parameter ``ref``:
    A 3D reference location within the scene, which may influence the
    sampling process.

Parameter ``sample``:
    A uniformly distributed 2D random variate

Parameter ``test_visibility``:
    When set to ``True``, a shadow ray will be cast to ensure that the
    sampled emitter position and the reference point are mutually
    visible.

Returns:
    A tuple ``(ds, spec)`` where

* ``ds`` is a fully populated DirectionSample3f data structure, which
provides further detail about the sampled emitter position (e.g. its
surface normal, solid angle density, whether Dirac delta distributions
were involved, etc.)

* ``spec`` is a Monte Carlo sampling weight specifying the ratio of
the radiance incident from the emitter and the sample probability per
unit solid angle.)doc";

static const char *__doc_mitsuba_Scene_sample_emitter_ray =
R"doc(Sample a ray according to the emission profile of scene emitters

This function combines both steps of choosing a ray origin on a light
source and an outgoing ray direction. It does not return any auxiliary
sampling information and is mainly meant to be used by unidirectional
rendering techniques like particle tracing.

Sampling is ideally perfectly proportional to the emission profile,
though approximations are acceptable as long as these are reflected in
the returned Monte Carlo sampling weight.

Parameter ``time``:
    The scene time associated with the ray to be sampled.

Parameter ``sample1``:
    A uniformly distributed 1D value that is used to sample the
    spectral dimension of the emission profile.

Parameter ``sample2``:
    A uniformly distributed sample on the domain ``[0,1]^2``.

Parameter ``sample3``:
    A uniformly distributed sample on the domain ``[0,1]^2``.

Returns:
    A tuple ``(ray, weight, emitter, radiance)``, where

* ``ray`` is the sampled ray (e.g. starting on the surface of an area
emitter)

* ``weight`` returns the emitted radiance divided by the spatio-
directional sampling density

* ``emitter`` is a pointer specifying the sampled emitter)doc";

static const char *__doc_mitsuba_Scene_sample_silhouette =
R"doc(Map a point sample in boundary sample space to a silhouette segment

This method will sample a SilhouetteSample3f object from all the
shapes in the scene that are being differentiated and have non-zero
sampling weight (see Shape::silhouette_sampling_weight).

Parameter ``sample``:
    The boundary space sample (a point in the unit cube).

Parameter ``flags``:
    Flags to select the type of silhouettes to sample from (see
    DiscontinuityFlags). Multiple types of discontinuities can be
    sampled in a single call. If a single type of silhouette is
    specified, shapes that do not have that types might still be
    sampled. In which case, the SilhouetteSample3f field
    ``discontinuity_type`` will be DiscontinuityFlags::Empty.

Returns:
    Silhouette sample record.)doc";

static const char *__doc_mitsuba_Scene_sensors = R"doc(Return the list of sensors)doc";

static const char *__doc_mitsuba_Scene_sensors_2 = R"doc(Return the list of sensors (const version))doc";

static const char *__doc_mitsuba_Scene_sensors_dr = R"doc(Return the list of sensors as a Dr.Jit array)doc";

static const char *__doc_mitsuba_Scene_shape_types =
R"doc(Returns a union of ShapeType flags denoting what is present in the
ShapeGroup)doc";

static const char *__doc_mitsuba_Scene_shapes = R"doc(Return the list of shapes)doc";

static const char *__doc_mitsuba_Scene_shapes_2 = R"doc(Return the list of shapes)doc";

static const char *__doc_mitsuba_Scene_shapes_dr = R"doc(Return the list of shapes as a Dr.Jit array)doc";

static const char *__doc_mitsuba_Scene_shapes_grad_enabled =
R"doc(Specifies whether any of the scene's shape parameters have gradient
tracking enabled)doc";

static const char *__doc_mitsuba_Scene_silhouette_shapes = R"doc(Return the list of shapes that can have their silhouette sampled)doc";

static const char *__doc_mitsuba_Scene_static_accel_initialization = R"doc(Static initialization of ray-intersection acceleration data structure)doc";

static const char *__doc_mitsuba_Scene_static_accel_initialization_cpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_static_accel_initialization_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_static_accel_shutdown = R"doc(Static shutdown of ray-intersection acceleration data structure)doc";

static const char *__doc_mitsuba_Scene_static_accel_shutdown_cpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_static_accel_shutdown_gpu = R"doc()doc";

static const char *__doc_mitsuba_Scene_to_string = R"doc(Return a human-readable string representation of the scene contents.)doc";

static const char *__doc_mitsuba_Scene_traverse = R"doc(Traverse the scene graph and invoke the given callback for each object)doc";

static const char *__doc_mitsuba_Scene_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_Scene_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Scene_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Scene_traverse_1_cb_ro_cpu =
R"doc(When the scene is defined on the CPU, traversal of the acceleration
structure has to be handled separately. These functions are defined
either for the Embree or native version of the scene, and handle its
traversal.)doc";

static const char *__doc_mitsuba_Scene_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Scene_traverse_1_cb_rw_cpu =
R"doc(When the scene is defined on the CPU, traversal of the acceleration
structure has to be handled separately. These functions are defined
either for the Embree or native version of the scene, and handle its
traversal.)doc";

static const char *__doc_mitsuba_Scene_type = R"doc()doc";

static const char *__doc_mitsuba_Scene_update_emitter_sampling_distribution = R"doc(Updates the discrete distribution used to select an emitter)doc";

static const char *__doc_mitsuba_Scene_update_silhouette_sampling_distribution = R"doc(Updates the discrete distribution used to select a shape's silhouette)doc";

static const char *__doc_mitsuba_Scene_variant_name = R"doc()doc";

static const char *__doc_mitsuba_ScopedPhase = R"doc()doc";

static const char *__doc_mitsuba_ScopedPhase_ScopedPhase = R"doc()doc";

static const char *__doc_mitsuba_ScopedPhase_ScopedPhase_2 = R"doc()doc";

static const char *__doc_mitsuba_ScopedPhase_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Sensor = R"doc()doc";

static const char *__doc_mitsuba_Sensor_2 = R"doc()doc";

static const char *__doc_mitsuba_Sensor_3 = R"doc()doc";

static const char *__doc_mitsuba_Sensor_4 = R"doc()doc";

static const char *__doc_mitsuba_Sensor_5 = R"doc()doc";

static const char *__doc_mitsuba_Sensor_6 = R"doc()doc";

static const char *__doc_mitsuba_Sensor_Sensor = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Sensor_class_name = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Sensor_film = R"doc(Return the Film instance associated with this sensor)doc";

static const char *__doc_mitsuba_Sensor_film_2 = R"doc(Return the Film instance associated with this sensor (const))doc";

static const char *__doc_mitsuba_Sensor_m_alpha = R"doc()doc";

static const char *__doc_mitsuba_Sensor_m_film = R"doc()doc";

static const char *__doc_mitsuba_Sensor_m_resolution = R"doc()doc";

static const char *__doc_mitsuba_Sensor_m_sampler = R"doc()doc";

static const char *__doc_mitsuba_Sensor_m_shutter_open = R"doc()doc";

static const char *__doc_mitsuba_Sensor_m_shutter_open_time = R"doc()doc";

static const char *__doc_mitsuba_Sensor_m_srf = R"doc()doc";

static const char *__doc_mitsuba_Sensor_needs_aperture_sample =
R"doc(Does the sampling technique require a sample for the aperture
position?)doc";

static const char *__doc_mitsuba_Sensor_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_Sensor_sample_ray_differential =
R"doc(Importance sample a ray differential proportional to the sensor's
sensitivity profile.

The sensor profile is a six-dimensional quantity that depends on time,
wavelength, surface position, and direction. This function takes a
given time value and five uniformly distributed samples on the
interval [0, 1] and warps them so that the returned ray the profile.
Any discrepancies between ideal and actual sampled profiles are
absorbed into a spectral importance weight that is returned along with
the ray.

In contrast to Endpoint::sample_ray(), this function returns
differentials with respect to the X and Y axis in screen space.

Parameter ``time``:
    The scene time associated with the ray_differential to be sampled

Parameter ``sample1``:
    A uniformly distributed 1D value that is used to sample the
    spectral dimension of the sensitivity profile.

Parameter ``sample2``:
    This argument corresponds to the sample position in fractional
    pixel coordinates relative to the crop window of the underlying
    film.

Parameter ``sample3``:
    A uniformly distributed sample on the domain ``[0,1]^2``. This
    argument determines the position on the aperture of the sensor.
    This argument is ignored if ``needs_sample_3() == false``.

Returns:
    The sampled ray differential and (potentially spectrally varying)
    importance weights. The latter account for the difference between
    the sensor profile and the actual used sampling density function.)doc";

static const char *__doc_mitsuba_Sensor_sample_wavelengths =
R"doc(Importance sample a set of wavelengths proportional to the sensitivity
spectrum.

Any discrepancies between ideal and actual sampled profile are
absorbed into a spectral importance weight that is returned along with
the wavelengths.

In RGB and monochromatic modes, since no wavelengths need to be
sampled, this simply returns an empty vector and the value 1.

Parameter ``sample``:
    A uniformly distributed 1D value that is used to sample the
    spectral dimension of the sensitivity profile.

Returns:
    The set of sampled wavelengths and importance weights. The latter
    account for the difference between the profile and the actual used
    sampling density function.)doc";

static const char *__doc_mitsuba_Sensor_sampler =
R"doc(Return the sensor's sample generator

This is the *root* sampler, which will later be forked a number of
times to provide each participating worker thread with its own
instance (see Scene::sampler()). Therefore, this sampler should never
be used for anything except creating forks.)doc";

static const char *__doc_mitsuba_Sensor_sampler_2 =
R"doc(Return the sensor's sampler (const version).

This is the *root* sampler, which will later be cloned a number of
times to provide each participating worker thread with its own
instance (see Scene::sampler()). Therefore, this sampler should never
be used for anything except creating clones.)doc";

static const char *__doc_mitsuba_Sensor_shutter_open = R"doc(Return the time value of the shutter opening event)doc";

static const char *__doc_mitsuba_Sensor_shutter_open_time = R"doc(Return the length, for which the shutter remains open)doc";

static const char *__doc_mitsuba_Sensor_traverse = R"doc(//! @})doc";

static const char *__doc_mitsuba_Sensor_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Sensor_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Sensor_type = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Sensor_variant_name = R"doc(This is both a class and the base of various Mitsuba plugins)doc";

static const char *__doc_mitsuba_Shape = R"doc(Forward declaration for `SilhouetteSample`)doc";

static const char *__doc_mitsuba_Shape_2 = R"doc(Forward declaration for `SilhouetteSample`)doc";

static const char *__doc_mitsuba_Shape_3 = R"doc()doc";

static const char *__doc_mitsuba_Shape_4 = R"doc()doc";

static const char *__doc_mitsuba_Shape_5 = R"doc()doc";

static const char *__doc_mitsuba_Shape_6 = R"doc()doc";

static const char *__doc_mitsuba_Shape_7 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_2 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_3 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_4 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_5 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_6 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_ShapeGroup = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_bbox = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_class_name = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_compute_surface_interaction = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_effective_primitive_count = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_embree_geometry = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_accel = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_accel_handles = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_embree_geometries = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_embree_scene = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_sbt_offset = R"doc(OptiX hitgroup sbt offset)doc";

static const char *__doc_mitsuba_ShapeGroup_m_shape_types = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_shapes = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_m_shapes_registry_ids = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_optix_build_gas = R"doc(Build OptiX geometry acceleration structures for this group's shapes)doc";

static const char *__doc_mitsuba_ShapeGroup_optix_fill_hitgroup_records = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_optix_prepare_geometry = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_optix_prepare_ias = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_parameters_grad_enabled = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_primitive_count = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_shape_types =
R"doc(Returns a union of ShapeType flags denoting what is present in the
ShapeGroup)doc";

static const char *__doc_mitsuba_ShapeGroup_surface_area = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_to_string = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_traverse = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_ShapeGroup_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_2 = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_3 = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_4 = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_5 = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_6 = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_ShapeKDTree =
R"doc(Create an empty kd-tree and take build-related parameters from
``props``.)doc";

static const char *__doc_mitsuba_ShapeKDTree_add_shape = R"doc(Register a new shape with the kd-tree (to be called before build()))doc";

static const char *__doc_mitsuba_ShapeKDTree_bbox = R"doc(Return the bounding box of the i-th primitive)doc";

static const char *__doc_mitsuba_ShapeKDTree_bbox_2 = R"doc(Return the (clipped) bounding box of the i-th primitive)doc";

static const char *__doc_mitsuba_ShapeKDTree_build = R"doc(Build the kd-tree)doc";

static const char *__doc_mitsuba_ShapeKDTree_class_name = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_clear = R"doc(Clear the kd-tree (build-related parameters remain))doc";

static const char *__doc_mitsuba_ShapeKDTree_find_shape =
R"doc(Map an abstract TShapeKDTree primitive index to a specific shape
managed by the ShapeKDTree.

The function returns the shape index and updates the *idx* parameter
to point to the primitive index (e.g. triangle ID) within the shape.)doc";

static const char *__doc_mitsuba_ShapeKDTree_intersect_prim =
R"doc(Check whether a primitive is intersected by the given ray.

Some temporary space is supplied to store data that can later be used
to create a detailed intersection record.)doc";

static const char *__doc_mitsuba_ShapeKDTree_m_primitive_map = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_m_shapes = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_primitive_count = R"doc(Return the number of registered primitives)doc";

static const char *__doc_mitsuba_ShapeKDTree_ray_intersect_naive = R"doc(Brute force intersection routine for debugging purposes)doc";

static const char *__doc_mitsuba_ShapeKDTree_ray_intersect_preliminary = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_ray_intersect_scalar = R"doc()doc";

static const char *__doc_mitsuba_ShapeKDTree_shape = R"doc(Return the i-th shape (const version))doc";

static const char *__doc_mitsuba_ShapeKDTree_shape_2 = R"doc(Return the i-th shape)doc";

static const char *__doc_mitsuba_ShapeKDTree_shape_count = R"doc(Return the number of registered shapes)doc";

static const char *__doc_mitsuba_ShapeKDTree_to_string = R"doc(Return a human-readable string representation of the scene contents.)doc";

static const char *__doc_mitsuba_ShapeType = R"doc(Enumeration of all shape types in Mitsuba)doc";

static const char *__doc_mitsuba_ShapeType_BSplineCurve = R"doc(B-Spline curves (`bsplinecurve`))doc";

static const char *__doc_mitsuba_ShapeType_Cylinder = R"doc(Cylinders (`cylinder`))doc";

static const char *__doc_mitsuba_ShapeType_Disk = R"doc(Disks (`disk`))doc";

static const char *__doc_mitsuba_ShapeType_Ellipsoids = R"doc(Ellipsoids (`ellipsoids`))doc";

static const char *__doc_mitsuba_ShapeType_EllipsoidsMesh = R"doc(Ellipsoids (`ellipsoidsmesh`))doc";

static const char *__doc_mitsuba_ShapeType_Instance = R"doc(Instance (`instance`))doc";

static const char *__doc_mitsuba_ShapeType_Invalid = R"doc(Invalid for default initialization)doc";

static const char *__doc_mitsuba_ShapeType_LinearCurve = R"doc(Linear curves (`linearcurve`))doc";

static const char *__doc_mitsuba_ShapeType_Mesh = R"doc(Meshes (`ply`, `obj`, `serialized`))doc";

static const char *__doc_mitsuba_ShapeType_Rectangle = R"doc(Rectangle: a particular type of mesh)doc";

static const char *__doc_mitsuba_ShapeType_SDFGrid = R"doc(SDF Grids (`sdfgrid`))doc";

static const char *__doc_mitsuba_ShapeType_ShapeGroup = R"doc(ShapeGroup (`shapegroup`))doc";

static const char *__doc_mitsuba_ShapeType_Sphere = R"doc(Spheres (`sphere`))doc";

static const char *__doc_mitsuba_Shape_Shape = R"doc(//! @})doc";

static const char *__doc_mitsuba_Shape_Shape_2 = R"doc()doc";

static const char *__doc_mitsuba_Shape_add_texture_attribute =
R"doc(Add a texture attribute with the given ``name``.

If an attribute with the same name already exists, it is replaced.

Note that ``Mesh`` shapes can additionally handle per-vertex and per-
face attributes via the ``Mesh::add_attribute`` method.

Parameter ``name``:
    Name of the attribute

Parameter ``texture``:
    Texture to store. The dimensionality of the attribute is simply
    the channel count of the texture.)doc";

static const char *__doc_mitsuba_Shape_bbox =
R"doc(Return an axis aligned box that bounds all shape primitives (including
any transformations that may have been applied to them))doc";

static const char *__doc_mitsuba_Shape_bbox_2 =
R"doc(Return an axis aligned box that bounds a single shape primitive
(including any transformations that may have been applied to it)

Remark:
    The default implementation simply calls bbox())doc";

static const char *__doc_mitsuba_Shape_bbox_3 =
R"doc(Return an axis aligned box that bounds a single shape primitive after
it has been clipped to another bounding box.

This is extremely important to construct high-quality kd-trees. The
default implementation just takes the bounding box returned by
bbox(ScalarIndex index) and clips it to *clip*.)doc";

static const char *__doc_mitsuba_Shape_bsdf = R"doc(Return the shape's BSDF)doc";

static const char *__doc_mitsuba_Shape_bsdf_2 = R"doc(Return the shape's BSDF)doc";

static const char *__doc_mitsuba_Shape_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_Shape_compute_surface_interaction =
R"doc(Compute and return detailed information related to a surface
interaction

The implementation should at most compute the fields ``p``, ``uv``,
``n``, ``sh_frame``.n, ``dp_du``, ``dp_dv``, ``dn_du`` and ``dn_dv``.
The ``flags`` parameter specifies which of those fields should be
computed.

The fields ``t``, ``time``, ``wavelengths``, ``shape``,
``prim_index``, ``instance``, will already have been initialized by
the caller. The field ``wi`` is initialized by the caller following
the call to compute_surface_interaction(), and ``duv_dx``, and
``duv_dy`` are left uninitialized.

Parameter ``ray``:
    Ray associated with the ray intersection

Parameter ``pi``:
    Data structure carrying information about the ray intersection

Parameter ``ray_flags``:
    Flags specifying which information should be computed

Parameter ``recursion_depth``:
    Integer specifying the recursion depth for nested virtual function
    call to this method (e.g. used for instancing).

Returns:
    A data structure containing the detailed information)doc";

static const char *__doc_mitsuba_Shape_differential_motion =
R"doc(Return the attached (AD) point on the shape's surface

This method is only useful when using automatic differentiation. The
immediate/primal return value of this method is exactly equal to
\`si.p\`.

The input `si` does not need to be explicitly detached, it is done by
the method itself.

If the shape cannot be differentiated, this method will return the
detached input point.

note:: The returned attached point is exactly the same as a point
which is computed by calling compute_surface_interaction with the
RayFlags::FollowShape flag.

Parameter ``si``:
    The surface point for which the function will be evaluated.

Not all fields of the object need to be filled. Only the `prim_index`,
`p` and `uv` fields are required. Certain shapes will only use a
subset of these.

Returns:
    The same surface point as the input but attached (AD) to the
    shape's parameters.)doc";

static const char *__doc_mitsuba_Shape_dirty = R"doc(Return whether the shape's geometry has changed)doc";

static const char *__doc_mitsuba_Shape_effective_primitive_count =
R"doc(Return the number of primitives (triangles, hairs, ..) contributed to
the scene by this shape

Includes instanced geometry. The default implementation simply returns
the same value as primitive_count().)doc";

static const char *__doc_mitsuba_Shape_embree_geometry = R"doc(Return the Embree version of this shape)doc";

static const char *__doc_mitsuba_Shape_emitter = R"doc(Return the area emitter associated with this shape (if any))doc";

static const char *__doc_mitsuba_Shape_emitter_2 = R"doc(Return the area emitter associated with this shape (if any))doc";

static const char *__doc_mitsuba_Shape_eval_attribute =
R"doc(Evaluate a specific shape attribute at the given surface interaction.

Shape attributes are user-provided fields that provide extra
information at an intersection. An example of this would be a per-
vertex or per-face color on a triangle mesh.

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    An unpolarized spectral power distribution or reflectance value)doc";

static const char *__doc_mitsuba_Shape_eval_attribute_1 =
R"doc(Monochromatic evaluation of a shape attribute at the given surface
interaction

This function differs from eval_attribute() in that it provided raw
access to scalar intensity/reflectance values without any color
processing (e.g. spectral upsampling).

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    An scalar intensity or reflectance value)doc";

static const char *__doc_mitsuba_Shape_eval_attribute_3 =
R"doc(Trichromatic evaluation of a shape attribute at the given surface
interaction

This function differs from eval_attribute() in that it provided raw
access to RGB intensity/reflectance values without any additional
color processing (e.g. RGB-to-spectral upsampling).

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    A trichromatic intensity or reflectance value)doc";

static const char *__doc_mitsuba_Shape_eval_attribute_x =
R"doc(Evaluate a dynamically sized shape attribute at the given surface
interaction.

Parameter ``name``:
    Name of the attribute to evaluate

Parameter ``si``:
    Surface interaction associated with the query

Returns:
    A dynamic array of attribute values)doc";

static const char *__doc_mitsuba_Shape_eval_parameterization =
R"doc(Parameterize the mesh using UV values

This function maps a 2D UV value to a surface interaction data
structure. Its behavior is only well-defined in regions where this
mapping is bijective. The default implementation throws.)doc";

static const char *__doc_mitsuba_Shape_exterior_medium = R"doc(Return the medium that lies on the exterior of this shape)doc";

static const char *__doc_mitsuba_Shape_get_children_string = R"doc()doc";

static const char *__doc_mitsuba_Shape_has_attribute =
R"doc(Returns whether this shape contains the specified attribute.

Parameter ``name``:
    Name of the attribute)doc";

static const char *__doc_mitsuba_Shape_has_flipped_normals = R"doc(Does this shape have flipped normals?)doc";

static const char *__doc_mitsuba_Shape_initialize = R"doc()doc";

static const char *__doc_mitsuba_Shape_interior_medium = R"doc(Return the medium that lies on the interior of this shape)doc";

static const char *__doc_mitsuba_Shape_invert_silhouette_sample =
R"doc(Map a silhouette segment to a point in boundary sample space

This method is the inverse of sample_silhouette(). The mapping from/to
boundary sample space to/from boundary segments is bijective.

This method's behavior is undefined when used in non-JIT variants or
when the shape is not being differentiated.

Parameter ``ss``:
    The sampled boundary segment

Returns:
    The corresponding boundary sample space point)doc";

static const char *__doc_mitsuba_Shape_is_ellipsoids = R"doc(Is this shape a ShapeType::Ellipsoids or ShapeType::EllipsoidsMesh)doc";

static const char *__doc_mitsuba_Shape_is_emitter = R"doc(Is this shape also an area emitter?)doc";

static const char *__doc_mitsuba_Shape_is_instance = R"doc(Is this shape an instance?)doc";

static const char *__doc_mitsuba_Shape_is_medium_transition = R"doc(Does the surface of this shape mark a medium transition?)doc";

static const char *__doc_mitsuba_Shape_is_mesh = R"doc(Is this shape a triangle mesh?)doc";

static const char *__doc_mitsuba_Shape_is_sensor = R"doc(Is this shape also an area sensor?)doc";

static const char *__doc_mitsuba_Shape_is_shape_group = R"doc(Is this shape a shape group?)doc";

static const char *__doc_mitsuba_Shape_m_bsdf = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_dirty = R"doc(True if the shape's geometry has changed)doc";

static const char *__doc_mitsuba_Shape_m_discontinuity_types = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_emitter = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_exterior_medium = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_initialized = R"doc(True if the shape has called initialize() at least once)doc";

static const char *__doc_mitsuba_Shape_m_interior_medium = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_is_instance = R"doc(True if the shape is used in a ``ShapeGroup``)doc";

static const char *__doc_mitsuba_Shape_m_optix_data_ptr = R"doc(OptiX hitgroup data buffer)doc";

static const char *__doc_mitsuba_Shape_m_sensor = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_shape_type = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_silhouette_sampling_weight = R"doc(Sampling weight (proportional to scene))doc";

static const char *__doc_mitsuba_Shape_m_texture_attributes = R"doc()doc";

static const char *__doc_mitsuba_Shape_m_to_world = R"doc()doc";

static const char *__doc_mitsuba_Shape_mark_as_instance = R"doc()doc";

static const char *__doc_mitsuba_Shape_mark_dirty = R"doc(Mark that the shape's geometry has changed)doc";

static const char *__doc_mitsuba_Shape_optix_build_input =
R"doc(Fills the OptixBuildInput associated with this shape.

Parameter ``build_input``:
    A reference to the build input to be filled. The field
    build_input.type has to be set, along with the associated members.
    For now, Mitsuba only supports the types
    OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES and
    OPTIX_BUILD_INPUT_TYPE_TRIANGLES.

The default implementation assumes that an implicit Shape (custom
primitive build type) is begin constructed, with its GPU data stored
at m_optix_data_ptr.)doc";

static const char *__doc_mitsuba_Shape_optix_fill_hitgroup_records =
R"doc(Creates and appends the HitGroupSbtRecord(s) associated with this
shape to the provided array.

Remark:
    This method can append multiple hitgroup records to the array (see
    the Shapegroup plugin for an example).

Parameter ``hitgroup_records``:
    The array of hitgroup records where the new HitGroupRecords should
    be appended.

Parameter ``pg``:
    The array of available program groups (used to pack the OptiX
    header at the beginning of the record).

The default implementation creates a new HitGroupSbtRecord and fills
its data field with m_optix_data_ptr. It then calls
optixSbtRecordPackHeader with one of the OptixProgramGroup of the
program_groups array (the actual program group index is inferred by
the type of the Shape, see OptixProgramGroupMapping).)doc";

static const char *__doc_mitsuba_Shape_optix_prepare_geometry =
R"doc(Populates the GPU data buffer, used in the OptiX Hitgroup sbt records.

Remark:
    Actual implementations of this method should allocate the field
    m_optix_data_ptr on the GPU and populate it with the OptiX
    representation of the class.

The default implementation throws an exception.)doc";

static const char *__doc_mitsuba_Shape_optix_prepare_ias =
R"doc(Prepares and fills the OptixInstance(s) associated with this shape.
This process includes generating the OptiX instance acceleration
structure (IAS) represented by this shape, and pushing OptixInstance
structs to the provided instances vector.

Remark:
    This method is currently only implemented for the Instance and
    ShapeGroup plugin.

Parameter ``context``:
    The OptiX context that was used to construct the rest of the
    scene's OptiX representation.

Parameter ``instances``:
    The array to which new OptixInstance should be appended.

Parameter ``instance_id``:
    The instance id, used internally inside OptiX to detect when a
    Shape is part of an Instance.

Parameter ``transf``:
    The current to_world transformation (should allow for recursive
    instancing).

The default implementation throws an exception.)doc";

static const char *__doc_mitsuba_Shape_parameters_changed = R"doc()doc";

static const char *__doc_mitsuba_Shape_parameters_grad_enabled =
R"doc(Return whether any shape's parameters that introduce visibility
discontinuities require gradients (default return false))doc";

static const char *__doc_mitsuba_Shape_pdf_direction =
R"doc(Query the probability density of sample_direction()

Parameter ``it``:
    A reference position somewhere within the scene.

Parameter ``ps``:
    A position record describing the sample in question

Returns:
    The probability density per unit solid angle)doc";

static const char *__doc_mitsuba_Shape_pdf_position =
R"doc(Query the probability density of sample_position() for a particular
point on the surface.

Parameter ``ps``:
    A position record describing the sample in question

Returns:
    The probability density per unit area)doc";

static const char *__doc_mitsuba_Shape_precompute_silhouette =
R"doc(Precompute the visible silhouette of this shape for a given viewpoint.

This method is meant to be used for silhouettes that are shared
between all threads, as is the case for primarily visible derivatives.

The return values are respectively a list of indices and their
corresponding weights. The semantic meaning of these indices is
different for each shape. For example, a triangle mesh will return the
indices of all of its edges that constitute its silhouette. These
indices are meant to be re-used as an argument when calling
sample_precomputed_silhouette.

This method's behavior is undefined when used in non-JIT variants or
when the shape is not being differentiated.

Parameter ``viewpoint``:
    The viewpoint which defines the silhouette of the shape

Returns:
    A list of indices used by the shape internally to represent
    silhouettes, and a list of the same length containing the
    (unnormalized) weights associated to each index.)doc";

static const char *__doc_mitsuba_Shape_primitive_count =
R"doc(Returns the number of sub-primitives that make up this shape

Remark:
    The default implementation simply returns ``1``)doc";

static const char *__doc_mitsuba_Shape_primitive_silhouette_projection =
R"doc(Projects a point on the surface of the shape to its silhouette as seen
from a specified viewpoint.

This method only projects the `si.p` point within its primitive.

Not all of the fields of the SilhouetteSample3f might be filled by
this method. Each shape will at the very least fill its return value
with enough information for it to be used by invert_silhouette_sample.

The projection operation might not find the closest silhouette point
to the given surface point. For example, it can be guided by a random
number ``sample``. Not all shapes types need this random number, each
shape implementation is free to define its own algorithm and
guarantees about the projection operation.

This method's behavior is undefined when used in non-JIT variants or
when the shape is not being differentiated.

Parameter ``viewpoint``:
    The viewpoint which defines the silhouette to project the point
    to.

Parameter ``si``:
    The surface point which will be projected.

Parameter ``flags``:
    Flags to select the type of SilhouetteSample3f to generate from
    the projection. Only one type of discontinuity can be used per
    call.

Parameter ``sample``:
    A random number that can be used to define the projection
    operation.

Returns:
    A boundary segment on the silhouette of the shape as seen from
    ``viewpoint``.)doc";

static const char *__doc_mitsuba_Shape_ray_intersect =
R"doc(Test for an intersection and return detailed information

This operation combines the prior ray_intersect_preliminary() and
compute_surface_interaction() operations.

Parameter ``ray``:
    The ray to be tested for an intersection

Parameter ``flags``:
    Describe how the detailed information should be computed)doc";

static const char *__doc_mitsuba_Shape_ray_intersect_preliminary =
R"doc(Fast ray intersection

Efficiently test whether the shape is intersected by the given ray,
and return preliminary information about the intersection if that is
the case.

If the intersection is deemed relevant (e.g. the closest to the ray
origin), detailed intersection information can later be obtained via
the compute_surface_interaction() method.

Parameter ``ray``:
    The ray to be tested for an intersection

Parameter ``prim_index``:
    Index of the primitive to be intersected. This index is ignored by
    a shape that contains a single primitive. Otherwise, if no index
    is provided, the ray intersection will be performed on the shape's
    first primitive at index 0.)doc";

static const char *__doc_mitsuba_Shape_ray_intersect_preliminary_packet = R"doc()doc";

static const char *__doc_mitsuba_Shape_ray_intersect_preliminary_packet_2 = R"doc()doc";

static const char *__doc_mitsuba_Shape_ray_intersect_preliminary_packet_3 = R"doc()doc";

static const char *__doc_mitsuba_Shape_ray_intersect_preliminary_scalar =
R"doc(Scalar test for an intersection and return detailed information

This operation is used by the KDTree acceleration structure.

Parameter ``ray``:
    The ray to be tested for an intersection

Returns:
    A tuple containing the following field: ``t``, ``uv``,
    ``shape_index``, ``prim_index``. The ``shape_index`` should be
    only used by the ShapeGroup class and be set to \c (uint32_t)-1
    otherwise.)doc";

static const char *__doc_mitsuba_Shape_ray_test =
R"doc(Fast ray shadow test

Efficiently test whether the shape is intersected by the given ray.

No details about the intersection are returned, hence the function is
only useful for visibility queries. For most shapes, the
implementation will simply forward the call to
ray_intersect_preliminary(). When the shape actually contains a nested
kd-tree, some optimizations are possible.

Parameter ``ray``:
    The ray to be tested for an intersection

Parameter ``prim_index``:
    Index of the primitive to be intersected)doc";

static const char *__doc_mitsuba_Shape_ray_test_packet = R"doc()doc";

static const char *__doc_mitsuba_Shape_ray_test_packet_2 = R"doc()doc";

static const char *__doc_mitsuba_Shape_ray_test_packet_3 = R"doc()doc";

static const char *__doc_mitsuba_Shape_ray_test_scalar = R"doc()doc";

static const char *__doc_mitsuba_Shape_remove_attribute =
R"doc(Remove a texture texture with the given ``name``.

Throws an exception if the attribute was not registered.)doc";

static const char *__doc_mitsuba_Shape_sample_direction =
R"doc(Sample a direction towards this shape with respect to solid angles
measured at a reference position within the scene

An ideal implementation of this interface would achieve a uniform
solid angle density within the surface region that is visible from the
reference position ``it.p`` (though such an ideal implementation is
usually neither feasible nor advisable due to poor efficiency).

The function returns the sampled position and the inverse probability
per unit solid angle associated with the sample.

When the Shape subclass does not supply a custom implementation of
this function, the Shape class reverts to a fallback approach that
piggybacks on sample_position(). This will generally lead to a
suboptimal sample placement and higher variance in Monte Carlo
estimators using the samples.

Parameter ``it``:
    A reference position somewhere within the scene.

Parameter ``sample``:
    A uniformly distributed 2D point on the domain ``[0,1]^2``

Returns:
    A DirectionSample instance describing the generated sample)doc";

static const char *__doc_mitsuba_Shape_sample_position =
R"doc(Sample a point on the surface of this shape

The sampling strategy is ideally uniform over the surface, though
implementations are allowed to deviate from a perfectly uniform
distribution as long as this is reflected in the returned probability
density.

Parameter ``time``:
    The scene time associated with the position sample

Parameter ``sample``:
    A uniformly distributed 2D point on the domain ``[0,1]^2``

Returns:
    A PositionSample instance describing the generated sample)doc";

static const char *__doc_mitsuba_Shape_sample_precomputed_silhouette =
R"doc(Samples a boundary segment on the shape's silhouette using precomputed
information computed in precompute_silhouette.

This method is meant to be used for silhouettes that are shared
between all threads, as is the case for primarily visible derivatives.

This method's behavior is undefined when used in non-JIT variants or
when the shape is not being differentiated.

Parameter ``viewpoint``:
    The viewpoint that was used for the precomputed silhouette
    information

Parameter ``sample1``:
    A sampled index from the return values of precompute_silhouette

Parameter ``sample2``:
    A uniformly distributed sample in ``[0,1]``

Returns:
    A boundary segment on the silhouette of the shape as seen from
    ``viewpoint``.)doc";

static const char *__doc_mitsuba_Shape_sample_silhouette =
R"doc(Map a point sample in boundary sample space to a silhouette segment

This method's behavior is undefined when used in non-JIT variants or
when the shape is not being differentiated.

Parameter ``sample``:
    The boundary space sample (a point in the unit cube).

Parameter ``flags``:
    Flags to select the type of silhouettes to sample from (see
    DiscontinuityFlags). Only one type of discontinuity can be sampled
    per call.

Returns:
    Silhouette sample record.)doc";

static const char *__doc_mitsuba_Shape_sensor = R"doc(Return the area sensor associated with this shape (if any))doc";

static const char *__doc_mitsuba_Shape_sensor_2 = R"doc(Return the area sensor associated with this shape (if any))doc";

static const char *__doc_mitsuba_Shape_set_bsdf = R"doc(Set the shape's BSDF)doc";

static const char *__doc_mitsuba_Shape_shape_type = R"doc(Returns the shape type ShapeType of this shape)doc";

static const char *__doc_mitsuba_Shape_silhouette_discontinuity_types = R"doc(//! @{ \name Silhouette sampling routines and other utilities)doc";

static const char *__doc_mitsuba_Shape_silhouette_sampling_weight = R"doc(Return this shape's sampling weight w.r.t. all shapes in the scene)doc";

static const char *__doc_mitsuba_Shape_surface_area =
R"doc(Return the shape's surface area.

The function assumes that the object is not undergoing some kind of
time-dependent scaling.

The default implementation throws an exception.)doc";

static const char *__doc_mitsuba_Shape_texture_attribute = R"doc(Return the texture attribute associated with ``name``.)doc";

static const char *__doc_mitsuba_Shape_texture_attribute_2 = R"doc(Return the texture attribute associated with ``name``.)doc";

static const char *__doc_mitsuba_Shape_traverse = R"doc()doc";

static const char *__doc_mitsuba_Shape_traverse_1_cb_fields = R"doc()doc";

static const char *__doc_mitsuba_Shape_traverse_1_cb_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Shape_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Shape_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Shape_type = R"doc(//! @})doc";

static const char *__doc_mitsuba_Shape_variant_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample =
R"doc(Data structure holding the result of visibility silhouette sampling
operations on geometry.)doc";

static const char *__doc_mitsuba_SilhouetteSample_SilhouetteSample = R"doc(Partially initialize a boundary segment from a position sample)doc";

static const char *__doc_mitsuba_SilhouetteSample_SilhouetteSample_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_SilhouetteSample_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_SilhouetteSample_4 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_d = R"doc(Direction of the boundary segment sample)doc";

static const char *__doc_mitsuba_SilhouetteSample_discontinuity_type = R"doc(Type of discontinuity (DiscontinuityFlags))doc";

static const char *__doc_mitsuba_SilhouetteSample_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_flags =
R"doc(The set of ``DiscontinuityFlags`` that were used to generate this
sample)doc";

static const char *__doc_mitsuba_SilhouetteSample_foreshortening =
R"doc(Local-form boundary foreshortening term.

It stores `sin_phi_B` for perimeter silhouettes or the normal
curvature for interior silhouettes.)doc";

static const char *__doc_mitsuba_SilhouetteSample_is_valid = R"doc(Is the current boundary segment valid?)doc";

static const char *__doc_mitsuba_SilhouetteSample_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_offset =
R"doc(Offset along the boundary segment direction (`d`) to avoid self-
intersections.)doc";

static const char *__doc_mitsuba_SilhouetteSample_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SilhouetteSample_prim_index = R"doc(Primitive index, e.g. the triangle ID (if applicable))doc";

static const char *__doc_mitsuba_SilhouetteSample_projection_index =
R"doc(Projection index indicator

For primitives like triangle meshes, a boundary segment is defined not
only by the triangle index but also the edge index of the selected
triangle. A value larger than 3 indicates a failed projection. For
other primitives, zero indicates a failed projection.

For triangle meshes, index 0 stands for the directed edge p0->p1 (not
the opposite edge p1->p2), index 1 stands for the edge p1->p2, and
index 2 for p2->p0.)doc";

static const char *__doc_mitsuba_SilhouetteSample_scene_index = R"doc(Index of the shape in the scene (if applicable))doc";

static const char *__doc_mitsuba_SilhouetteSample_shape = R"doc(Pointer to the associated shape)doc";

static const char *__doc_mitsuba_SilhouetteSample_silhouette_d = R"doc(Direction of the silhouette curve at the boundary point)doc";

static const char *__doc_mitsuba_SilhouetteSample_spawn_ray =
R"doc(Spawn a ray on the silhouette point in the direction of d

The ray origin is offset in the direction of the segment (d) as well
as in the direction of the silhouette normal (n). Without this
offsetting, during a ray intersection, the ray could potentially find
an intersection point at its origin due to numerical instabilities in
the intersection routines.)doc";

static const char *__doc_mitsuba_Spectrum =
R"doc(//! @{ \name Data types for spectral quantities with sampled
wavelengths)doc";

static const char *__doc_mitsuba_Spectrum_Spectrum = R"doc()doc";

static const char *__doc_mitsuba_Spectrum_Spectrum_2 = R"doc()doc";

static const char *__doc_mitsuba_Spectrum_Spectrum_3 = R"doc()doc";

static const char *__doc_mitsuba_Spectrum_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Spectrum_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Spiral =
R"doc(Generates a spiral of blocks to be rendered.

Author:
    Adam Arbree Aug 25, 2005 RayTracer.java Used with permission.
    Copyright 2005 Program of Computer Graphics, Cornell University)doc";

static const char *__doc_mitsuba_Spiral_Direction = R"doc()doc";

static const char *__doc_mitsuba_Spiral_Direction_Down = R"doc()doc";

static const char *__doc_mitsuba_Spiral_Direction_Left = R"doc()doc";

static const char *__doc_mitsuba_Spiral_Direction_Right = R"doc()doc";

static const char *__doc_mitsuba_Spiral_Direction_Up = R"doc()doc";

static const char *__doc_mitsuba_Spiral_Spiral =
R"doc(Create a new spiral generator for the given size, offset into a larger
frame, and block size)doc";

static const char *__doc_mitsuba_Spiral_block_count = R"doc(Return the total number of blocks)doc";

static const char *__doc_mitsuba_Spiral_class_name = R"doc()doc";

static const char *__doc_mitsuba_Spiral_direction = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_block_count = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_block_counter = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_block_size = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_blocks = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_mutex = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_offset = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_passes_left = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_position = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_size = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_spiral_size = R"doc()doc";

static const char *__doc_mitsuba_Spiral_m_steps_left = R"doc()doc";

static const char *__doc_mitsuba_Spiral_max_block_size = R"doc(Return the maximum block size)doc";

static const char *__doc_mitsuba_Spiral_next_block =
R"doc(Return the offset, size, and unique identifier of the next block.

A size of zero indicates that the spiral traversal is done.)doc";

static const char *__doc_mitsuba_Spiral_reset =
R"doc(Reset the spiral to its initial state. Does not affect the number of
passes.)doc";

static const char *__doc_mitsuba_Stream =
R"doc(Abstract seekable stream class

Specifies all functions to be implemented by stream subclasses and
provides various convenience functions layered on top of on them.

All ``read*()`` and ``write*()`` methods support transparent
conversion based on the endianness of the underlying system and the
value passed to set_byte_order(). Whenever host_byte_order() and
byte_order() disagree, the endianness is swapped.

See also:
    FileStream, MemoryStream, DummyStream)doc";

static const char *__doc_mitsuba_StreamAppender =
R"doc(%Appender implementation, which writes to an arbitrary C++ output
stream)doc";

static const char *__doc_mitsuba_StreamAppender_StreamAppender =
R"doc(Create a new stream appender

Remark:
    This constructor is not exposed in the Python bindings)doc";

static const char *__doc_mitsuba_StreamAppender_StreamAppender_2 = R"doc(Create a new stream appender logging to a file)doc";

static const char *__doc_mitsuba_StreamAppender_append = R"doc(Append a line of text)doc";

static const char *__doc_mitsuba_StreamAppender_class_name = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_log_progress = R"doc(Process a progress message)doc";

static const char *__doc_mitsuba_StreamAppender_logs_to_file = R"doc(Does this appender log to a file)doc";

static const char *__doc_mitsuba_StreamAppender_m_fname = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_is_file = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_last_message_was_progress = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_m_stream = R"doc()doc";

static const char *__doc_mitsuba_StreamAppender_read_log = R"doc(Return the contents of the log file as a string)doc";

static const char *__doc_mitsuba_StreamAppender_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Stream_EByteOrder = R"doc(Defines the byte order (endianness) to use in this Stream)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_EBigEndian = R"doc()doc";

static const char *__doc_mitsuba_Stream_EByteOrder_ELittleEndian = R"doc(PowerPC, SPARC, Motorola 68K)doc";

static const char *__doc_mitsuba_Stream_EByteOrder_ENetworkByteOrder = R"doc(x86, x86_64)doc";

static const char *__doc_mitsuba_Stream_Stream =
R"doc(Creates a new stream.

By default, this function sets the stream byte order to that of the
system (i.e. no conversion is performed))doc";

static const char *__doc_mitsuba_Stream_Stream_2 = R"doc(Copying is disallowed.)doc";

static const char *__doc_mitsuba_Stream_byte_order = R"doc(Returns the byte order of this stream.)doc";

static const char *__doc_mitsuba_Stream_can_read = R"doc(Can we read from the stream?)doc";

static const char *__doc_mitsuba_Stream_can_write = R"doc(Can we write to the stream?)doc";

static const char *__doc_mitsuba_Stream_class_name = R"doc(@})doc";

static const char *__doc_mitsuba_Stream_close =
R"doc(Closes the stream.

No further read or write operations are permitted.

This function is idempotent. It may be called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_Stream_flush = R"doc(Flushes the stream's buffers, if any)doc";

static const char *__doc_mitsuba_Stream_host_byte_order = R"doc(Returns the byte order of the underlying machine.)doc";

static const char *__doc_mitsuba_Stream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_Stream_m_byte_order = R"doc()doc";

static const char *__doc_mitsuba_Stream_needs_endianness_swap =
R"doc(Returns true if we need to perform endianness swapping before writing
or reading.)doc";

static const char *__doc_mitsuba_Stream_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Stream_read =
R"doc(Reads a specified amount of data from the stream. \note This does
**not** handle endianness swapping.

Throws an exception when the stream ended prematurely. Implementations
need to handle endianness swap when appropriate.)doc";

static const char *__doc_mitsuba_Stream_read_2 =
R"doc(Reads one object of type T from the stream at the current position by
delegating to the appropriate ``serialization_helper``.

Endianness swapping is handled automatically if needed.)doc";

static const char *__doc_mitsuba_Stream_read_array =
R"doc(Reads multiple objects of type T from the stream at the current
position by delegating to the appropriate ``serialization_helper``.

Endianness swapping is handled automatically if needed.)doc";

static const char *__doc_mitsuba_Stream_read_line = R"doc(Convenience function for reading a line of text from an ASCII file)doc";

static const char *__doc_mitsuba_Stream_read_token = R"doc(Convenience function for reading a contiguous token from an ASCII file)doc";

static const char *__doc_mitsuba_Stream_seek =
R"doc(Seeks to a position inside the stream.

Seeking beyond the size of the buffer will not modify the length of
its contents. However, a subsequent write should start at the sought
position and update the size appropriately.)doc";

static const char *__doc_mitsuba_Stream_set_byte_order =
R"doc(Sets the byte order to use in this stream.

Automatic conversion will be performed on read and write operations to
match the system's native endianness.

No consistency is guaranteed if this method is called after performing
some read and write operations on the system using a different
endianness.)doc";

static const char *__doc_mitsuba_Stream_size = R"doc(Returns the size of the stream)doc";

static const char *__doc_mitsuba_Stream_skip = R"doc(Skip ahead by a given number of bytes)doc";

static const char *__doc_mitsuba_Stream_tell = R"doc(Gets the current position inside the stream)doc";

static const char *__doc_mitsuba_Stream_to_string = R"doc(Returns a human-readable descriptor of the stream)doc";

static const char *__doc_mitsuba_Stream_truncate =
R"doc(Truncates the stream to a given size.

The position is updated to ``min(old_position, size)``. Throws an
exception if in read-only mode.)doc";

static const char *__doc_mitsuba_Stream_write =
R"doc(Writes a specified amount of data into the stream. \note This does
**not** handle endianness swapping.

Throws an exception when not all data could be written.
Implementations need to handle endianness swap when appropriate.)doc";

static const char *__doc_mitsuba_Stream_write_2 =
R"doc(Reads one object of type T from the stream at the current position by
delegating to the appropriate ``serialization_helper``.

Endianness swapping is handled automatically if needed.)doc";

static const char *__doc_mitsuba_Stream_write_array =
R"doc(Reads multiple objects of type T from the stream at the current
position by delegating to the appropriate ``serialization_helper``.

Endianness swapping is handled automatically if needed.)doc";

static const char *__doc_mitsuba_Stream_write_line = R"doc(Convenience function for writing a line of text to an ASCII file)doc";

static const char *__doc_mitsuba_Struct =
R"doc(Descriptor for specifying the contents and in-memory layout of a POD-
style data record

Remark:
    The python API provides an additional ``dtype()`` method, which
    returns the NumPy ``dtype`` equivalent of a given ``Struct``
    instance.)doc";

static const char *__doc_mitsuba_StructConverter =
R"doc(This class solves the any-to-any problem: efficiently converting from
one kind of structured data representation to another

Graphics applications often need to convert from one kind of
structured representation to another, for instance when loading/saving
image or mesh data. Consider the following data records which both
describe positions tagged with color data.

```
struct Source { // <-- Big endian! :(
   uint8_t r, g, b; // in sRGB
   half x, y, z;
};

struct Target { // <-- Little endian!
   float x, y, z;
   float r, g, b, a; // in linear space
};
```

The record ``Source`` may represent what is stored in a file on disk,
while ``Target`` represents the expected input of the implementation.
Not only are the formats (e.g. float vs half or uint8_t, incompatible
endianness) and encodings different (e.g. gamma correction vs linear
space), but the second record even has a different order and extra
fields that don't exist in the first one.

This class provides a routine convert() which <ol>

* reorders entries

* converts between many different formats (u[int]8-64, float16-64)

* performs endianness conversion

* applies or removes gamma correction

* optionally checks that certain entries have expected default values

* substitutes missing values with specified defaults

* performs linear transformations of groups of fields (e.g. between
different RGB color spaces)

* applies dithering to avoid banding artifacts when converting 2D
images

</ol>

The above operations can be arranged in countless ways, which makes it
hard to provide an efficient generic implementation of this
functionality. For this reason, the implementation of this class
relies on a JIT compiler that generates fast conversion code on demand
for each specific conversion. The function is cached and reused in
case the same conversion is needed later on. Note that JIT compilation
only works on x86_64 processors; other platforms use a slow generic
fallback implementation.)doc";

static const char *__doc_mitsuba_StructConverter_StructConverter =
R"doc(Construct an optimized conversion routine going from ``source`` to
``target``)doc";

static const char *__doc_mitsuba_StructConverter_Value = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_Value_flags = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_Value_type = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_class_name = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_convert = R"doc(Convert ``count`` elements. Returns ``True`` upon success)doc";

static const char *__doc_mitsuba_StructConverter_convert_2d = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_linearize = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_load = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_m_dither = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_m_source = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_m_target = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_save = R"doc()doc";

static const char *__doc_mitsuba_StructConverter_source = R"doc(Return the source ``Struct`` descriptor)doc";

static const char *__doc_mitsuba_StructConverter_static_shutdown = R"doc(Free static resources)doc";

static const char *__doc_mitsuba_StructConverter_target = R"doc(Return the target ``Struct`` descriptor)doc";

static const char *__doc_mitsuba_StructConverter_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_Struct_ByteOrder = R"doc(Byte order of the fields in the ``Struct``)doc";

static const char *__doc_mitsuba_Struct_ByteOrder_BigEndian = R"doc()doc";

static const char *__doc_mitsuba_Struct_ByteOrder_HostByteOrder = R"doc()doc";

static const char *__doc_mitsuba_Struct_ByteOrder_LittleEndian = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field = R"doc(Field specifier with size and offset)doc";

static const char *__doc_mitsuba_Struct_Field_blend =
R"doc(For use with StructConverter::convert()

Specifies a pair of weights and source field names that will be
linearly blended to obtain the output field value. Note that this only
works for floating point fields or integer fields with the
Flags::Normalized flag. Gamma-corrected fields will be blended in
linear space.)doc";

static const char *__doc_mitsuba_Struct_Field_default = R"doc(Default value)doc";

static const char *__doc_mitsuba_Struct_Field_flags = R"doc(Additional flags)doc";

static const char *__doc_mitsuba_Struct_Field_is_float = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_is_integer = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_is_signed = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_is_unsigned = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_name = R"doc(Name of the field)doc";

static const char *__doc_mitsuba_Struct_Field_offset = R"doc(Offset within the ``Struct`` (in bytes))doc";

static const char *__doc_mitsuba_Struct_Field_operator_eq = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_Struct_Field_operator_ne = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_Struct_Field_range = R"doc()doc";

static const char *__doc_mitsuba_Struct_Field_size = R"doc(Size in bytes)doc";

static const char *__doc_mitsuba_Struct_Field_type = R"doc(Type identifier)doc";

static const char *__doc_mitsuba_Struct_Flags = R"doc(Field-specific flags)doc";

static const char *__doc_mitsuba_Struct_Flags_Alpha = R"doc(Specifies whether the field encodes an alpha value)doc";

static const char *__doc_mitsuba_Struct_Flags_Assert =
R"doc(In FieldConverter::convert, check that the field value matches the
specified default value. Otherwise, return a failure)doc";

static const char *__doc_mitsuba_Struct_Flags_Default =
R"doc(In FieldConverter::convert, when the field is missing in the source
record, replace it by the specified default value)doc";

static const char *__doc_mitsuba_Struct_Flags_Empty = R"doc(No flags set (default value))doc";

static const char *__doc_mitsuba_Struct_Flags_Gamma =
R"doc(Specifies whether the field encodes a sRGB gamma-corrected value.
Assumes ``Normalized`` is also specified.)doc";

static const char *__doc_mitsuba_Struct_Flags_Normalized =
R"doc(Specifies whether an integer field encodes a normalized value in the
range [0, 1]. The flag is ignored if specified for floating point
valued fields.)doc";

static const char *__doc_mitsuba_Struct_Flags_PremultipliedAlpha = R"doc(Specifies whether the field encodes an alpha premultiplied value)doc";

static const char *__doc_mitsuba_Struct_Flags_Weight =
R"doc(In FieldConverter::convert, when an input structure contains a weight
field, the value of all entries are considered to be expressed
relative to its value. Converting to an un-weighted structure entails
a division by the weight.)doc";

static const char *__doc_mitsuba_Struct_Struct =
R"doc(Create a new ``Struct`` and indicate whether the contents are packed
or aligned)doc";

static const char *__doc_mitsuba_Struct_Struct_2 = R"doc(Copy constructor)doc";

static const char *__doc_mitsuba_Struct_Type = R"doc(Type of a field in the ``Struct``)doc";

static const char *__doc_mitsuba_Struct_Type_Float16 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Float32 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Float64 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Int16 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Int32 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Int64 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Int8 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_Invalid = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_UInt16 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_UInt32 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_UInt64 = R"doc()doc";

static const char *__doc_mitsuba_Struct_Type_UInt8 = R"doc()doc";

static const char *__doc_mitsuba_Struct_alignment = R"doc(Return the alignment (in bytes) of the data structure)doc";

static const char *__doc_mitsuba_Struct_append =
R"doc(Append a new field to the ``Struct``; determines size and offset
automatically)doc";

static const char *__doc_mitsuba_Struct_append_2 = R"doc(Append a new field to the ``Struct`` (manual version))doc";

static const char *__doc_mitsuba_Struct_begin = R"doc(Return an iterator associated with the first field)doc";

static const char *__doc_mitsuba_Struct_begin_2 = R"doc(Return an iterator associated with the first field)doc";

static const char *__doc_mitsuba_Struct_byte_order = R"doc(Return the byte order of the ``Struct``)doc";

static const char *__doc_mitsuba_Struct_class_name = R"doc()doc";

static const char *__doc_mitsuba_Struct_end = R"doc(Return an iterator associated with the end of the data structure)doc";

static const char *__doc_mitsuba_Struct_end_2 = R"doc(Return an iterator associated with the end of the data structure)doc";

static const char *__doc_mitsuba_Struct_field = R"doc(Look up a field by name (throws an exception if not found))doc";

static const char *__doc_mitsuba_Struct_field_2 = R"doc(Look up a field by name. Throws an exception if not found)doc";

static const char *__doc_mitsuba_Struct_field_count = R"doc(Return the number of fields)doc";

static const char *__doc_mitsuba_Struct_has_field = R"doc(Check if the ``Struct`` has a field of the specified name)doc";

static const char *__doc_mitsuba_Struct_host_byte_order = R"doc(Return the byte order of the host machine)doc";

static const char *__doc_mitsuba_Struct_is_float = R"doc(Check whether the given type is a floating point type)doc";

static const char *__doc_mitsuba_Struct_is_integer = R"doc(Check whether the given type is an integer type)doc";

static const char *__doc_mitsuba_Struct_is_signed = R"doc(Check whether the given type is a signed type)doc";

static const char *__doc_mitsuba_Struct_is_unsigned = R"doc(Check whether the given type is an unsigned type)doc";

static const char *__doc_mitsuba_Struct_m_byte_order = R"doc()doc";

static const char *__doc_mitsuba_Struct_m_fields = R"doc()doc";

static const char *__doc_mitsuba_Struct_m_pack = R"doc()doc";

static const char *__doc_mitsuba_Struct_offset = R"doc(Return the offset of the i-th field)doc";

static const char *__doc_mitsuba_Struct_offset_2 = R"doc(Return the offset of field with the given name)doc";

static const char *__doc_mitsuba_Struct_operator_array = R"doc(Access an individual field entry)doc";

static const char *__doc_mitsuba_Struct_operator_array_2 = R"doc(Access an individual field entry)doc";

static const char *__doc_mitsuba_Struct_operator_eq = R"doc(Equality operator)doc";

static const char *__doc_mitsuba_Struct_operator_ne = R"doc(Inequality operator)doc";

static const char *__doc_mitsuba_Struct_range = R"doc(Return the representable range of the given type)doc";

static const char *__doc_mitsuba_Struct_size = R"doc(Return the size (in bytes) of the data structure, including padding)doc";

static const char *__doc_mitsuba_Struct_to_string = R"doc(Return a string representation)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_SurfaceAreaHeuristic3 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_empty_space_bonus =
R"doc(Return the bonus factor for empty space used by the tree construction
heuristic)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_eval = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_inner_cost =
R"doc(Evaluate the surface area heuristic

Given a split on axis *axis* at position *split*, compute the
probability of traversing the left and right child during a typical
query operation. In the case of the surface area heuristic, this is
simply the ratio of surface areas.)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_leaf_cost = R"doc(Evaluate the cost of a leaf node)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_m_empty_space_bonus = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_m_query_cost = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_m_temp0 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_m_temp1 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_m_temp2 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_m_traversal_cost = R"doc()doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_query_cost =
R"doc(Return the query cost used by the tree construction heuristic

(This is the average cost for testing a shape against a kd-tree query))doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_set_bounding_box =
R"doc(Initialize the surface area heuristic with the bounds of a parent node

Precomputes some information so that traversal probabilities of
potential split planes can be evaluated efficiently)doc";

static const char *__doc_mitsuba_SurfaceAreaHeuristic3_traversal_cost =
R"doc(Get the cost of a traversal operation used by the tree construction
heuristic)doc";

static const char *__doc_mitsuba_SurfaceInteraction = R"doc(Stores information related to a surface scattering interaction)doc";

static const char *__doc_mitsuba_SurfaceInteraction_SurfaceInteraction =
R"doc(Construct from a position sample. Unavailable fields such as `wi` and
the partial derivatives are left uninitialized. The `shape` pointer is
left uninitialized because we can't guarantee that the given
PositionSample::object points to a Shape instance.)doc";

static const char *__doc_mitsuba_SurfaceInteraction_SurfaceInteraction_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_SurfaceInteraction_3 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_SurfaceInteraction_4 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_bsdf =
R"doc(Returns the BSDF of the intersected shape.

The parameter ``ray`` must match the one used to create the
interaction record. This function computes texture coordinate partials
if this is required by the BSDF (e.g. for texture filtering).

Implementation in 'bsdf.h')doc";

static const char *__doc_mitsuba_SurfaceInteraction_bsdf_2 = R"doc()doc";

static const char *__doc_mitsuba_SurfaceInteraction_compute_uv_partials = R"doc(Computes texture coordinate partials)doc";

static const char *__doc_mitsuba_SurfaceInteraction_dn_du = R"doc(Normal partials wrt. the UV parameterization)doc";

static const char *__doc_mitsuba_SurfaceInteraction_dn_dv = R"doc(Normal partials wrt. the UV parameterization)doc";

static const char *__doc_mitsuba_SurfaceInteraction_dp_du = R"doc(Position partials wrt. the UV parameterization)doc";

static const char *__doc_mitsuba_SurfaceInteraction_dp_dv = R"doc(Position partials wrt. the UV parameterization)doc";

static const char *__doc_mitsuba_SurfaceInteraction_duv_dx = R"doc(UV partials wrt. changes in screen-space)doc";

static const char *__doc_mitsuba_SurfaceInteraction_duv_dy = R"doc(UV partials wrt. changes in screen-space)doc";

static const char *__doc_mitsuba_SurfaceInteraction_emitter =
R"doc(Return the emitter associated with the intersection (if any) \note
Defined in scene.h)doc";

static const char *__doc_mitsuba_SurfaceInteraction_fields = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_fields_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_finalize_surface_interaction =
R"doc(Fills uninitialized fields after a call to
Shape::compute_surface_interaction()

Parameter ``pi``:
    Preliminary intersection which was used to during the call to
    Shape::compute_surface_interaction()

Parameter ``ray``:
    Ray associated with the ray intersection

Parameter ``ray_flags``:
    Flags specifying which information should be computed)doc";

static const char *__doc_mitsuba_SurfaceInteraction_has_n_partials = R"doc()doc";

static const char *__doc_mitsuba_SurfaceInteraction_has_uv_partials = R"doc()doc";

static const char *__doc_mitsuba_SurfaceInteraction_initialize_sh_frame = R"doc(Initialize local shading frame using Gram-schmidt orthogonalization)doc";

static const char *__doc_mitsuba_SurfaceInteraction_instance = R"doc(Stores a pointer to the parent instance (if applicable))doc";

static const char *__doc_mitsuba_SurfaceInteraction_is_medium_transition = R"doc(Does the surface mark a transition between two media?)doc";

static const char *__doc_mitsuba_SurfaceInteraction_is_sensor = R"doc(Is the intersected shape also a sensor?)doc";

static const char *__doc_mitsuba_SurfaceInteraction_labels = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_operator_array = R"doc(Convenience operator for masking)doc";

static const char *__doc_mitsuba_SurfaceInteraction_operator_assign = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_operator_assign_2 = R"doc(//! @})doc";

static const char *__doc_mitsuba_SurfaceInteraction_prim_index = R"doc(Primitive index, e.g. the triangle ID (if applicable))doc";

static const char *__doc_mitsuba_SurfaceInteraction_sh_frame = R"doc(Shading frame)doc";

static const char *__doc_mitsuba_SurfaceInteraction_shape = R"doc(Pointer to the associated shape)doc";

static const char *__doc_mitsuba_SurfaceInteraction_target_medium =
R"doc(Determine the target medium

When ``is_medium_transition()`` = ``True``, determine the medium that
contains the ``ray(this->p, d)``)doc";

static const char *__doc_mitsuba_SurfaceInteraction_target_medium_2 =
R"doc(Determine the target medium based on the cosine of the angle between
the geometric normal and a direction

Returns the exterior medium when ``cos_theta > 0`` and the interior
medium when ``cos_theta <= 0``.)doc";

static const char *__doc_mitsuba_SurfaceInteraction_to_local = R"doc(Convert a world-space vector into local shading coordinates)doc";

static const char *__doc_mitsuba_SurfaceInteraction_to_local_mueller =
R"doc(Converts a Mueller matrix defined in world space to a local frame

A Mueller matrix operates from the (implicitly) defined frame
stokes_basis(in_forward) to the frame stokes_basis(out_forward). This
method converts a Mueller matrix defined on directions in world-space
to a Mueller matrix defined in the local frame.

This expands to a no-op in non-polarized modes.

Parameter ``in_forward_local``:
    Incident direction (along propagation direction of light), given
    in world-space coordinates.

Parameter ``wo_local``:
    Outgoing direction (along propagation direction of light), given
    in world-space coordinates.

Returns:
    Equivalent Mueller matrix that operates in local frame
    coordinates.)doc";

static const char *__doc_mitsuba_SurfaceInteraction_to_world = R"doc(Convert a local shading-space vector into world space)doc";

static const char *__doc_mitsuba_SurfaceInteraction_to_world_mueller =
R"doc(Converts a Mueller matrix defined in a local frame to world space

A Mueller matrix operates from the (implicitly) defined frame
stokes_basis(in_forward) to the frame stokes_basis(out_forward). This
method converts a Mueller matrix defined on directions in the local
frame to a Mueller matrix defined on world-space directions.

This expands to a no-op in non-polarized modes.

Parameter ``M_local``:
    The Mueller matrix in local space, e.g. returned by a BSDF.

Parameter ``in_forward_local``:
    Incident direction (along propagation direction of light), given
    in local frame coordinates.

Parameter ``wo_local``:
    Outgoing direction (along propagation direction of light), given
    in local frame coordinates.

Returns:
    Equivalent Mueller matrix that operates in world-space
    coordinates.)doc";

static const char *__doc_mitsuba_SurfaceInteraction_uv = R"doc(UV surface coordinates)doc";

static const char *__doc_mitsuba_SurfaceInteraction_wi = R"doc(Incident direction in the local shading frame)doc";

static const char *__doc_mitsuba_SurfaceInteraction_zero =
R"doc(This callback method is invoked by dr::zeros<>, and takes care of
fields that deviate from the standard zero-initialization convention.)doc";

static const char *__doc_mitsuba_TShapeKDTree =
R"doc(Optimized KD-tree acceleration data structure for n-dimensional (n<=4)
shapes and various queries involving them.

Note that this class mainly concerns itself with primitives that cover
*a region* of space. For point data, other implementations will be
more suitable. The most important application in Mitsuba is the fast
construction of high-quality trees for ray tracing. See the class
ShapeKDTree for this specialization.

The code in this class is a fully generic kd-tree implementation,
which can theoretically support any kind of shape. However, subclasses
still need to provide the following signatures for a functional
implementation:

```
/// Return the total number of primitives
Size primitive_count() const;

/// Return the axis-aligned bounding box of a certain primitive
BoundingBox bbox(Index primIdx) const;

/// Return the bounding box of a primitive when clipped to another bounding box
BoundingBox bbox(Index primIdx, const BoundingBox &aabb) const;
```

This class follows the "Curiously recurring template" design pattern
so that the above functions can be inlined (in particular, no virtual
calls will be necessary!).

When the kd-tree is initially built, this class optimizes a cost
heuristic every time a split plane has to be chosen. For ray tracing,
the heuristic is usually the surface area heuristic (SAH), but other
choices are possible as well. The tree cost model must be passed as a
template argument, which can use a supplied bounding box and split
candidate to compute approximate probabilities of recursing into the
left and right subrees during a typical kd-tree query operation. See
SurfaceAreaHeuristic3 for an example of the interface that must be
implemented.

The kd-tree construction algorithm creates 'perfect split' trees as
outlined in the paper "On Building fast kd-Trees for Ray Tracing, and
on doing that in O(N log N)" by Ingo Wald and Vlastimil Havran. This
works even when the tree is not meant to be used for ray tracing. For
polygonal meshes, the involved Sutherland-Hodgman iterations can be
quite expensive in terms of the overall construction time. The
set_clip_primitives() method can be used to deactivate perfect splits
at the cost of a lower-quality tree.

Because the O(N log N) construction algorithm tends to cause many
incoherent memory accesses and does not parallelize particularly well,
a different method known as *Min-Max Binning* is used for the top
levels of the tree. Min-Max-binning is an approximation to the O(N log
N) approach, which works extremely well at the top of the tree (i.e.
when there are many elements). This algorithm realized as a series of
efficient parallel sweeps that harness the available cores at all
levels (even at the root node). Each iteration splits the list of
primitives into independent subtrees which can also be processed in
parallel. Eventually, the input data is reduced into sufficiently
small chunks, at which point the implementation switches over to the
more accurate O(N log N) builder. The various thresholds and
parameters for these different methods can be accessed and configured
via getters and setters of this class.)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext =
R"doc(Helper data structure used during tree construction (shared by all
threads))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_2 =
R"doc(Helper data structure used during tree construction (shared by all
threads))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_BuildContext = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_bad_refines = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_derived = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_exp_leaves_visited = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_exp_primitives_queried = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_exp_traversal_steps = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_index_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_max_depth = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_max_prims_in_leaf = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_node_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_nonempty_leaf_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_prim_buckets = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_pruned = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_retracted_splits = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_temp_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildContext_work_units = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask =
R"doc(Build task for building subtrees in parallel

This class is responsible for building a subtree of the final kd-tree.
It recursively spawns new tasks for its respective subtrees to enable
parallel construction.

At the top of the tree, it uses min-max-binning and parallel
reductions to create sufficient parallelism. When the number of
elements is sufficiently small, it switches to a more accurate O(N log
N) builder which uses normal recursion on the stack (i.e. it does not
spawn further parallel pieces of work).)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_BuildTask = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_FTZGuard = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_FTZGuard_FTZGuard = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_FTZGuard_csr = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_build_nlogn = R"doc(Recursively run the O(N log N builder))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_execute = R"doc(Run one iteration of min-max binning and spawn recursive tasks)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_bad_refines = R"doc(Number of "bad refines" so far)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_bbox = R"doc(Bounding box of the node)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_cost = R"doc(This scalar should be set to the final cost when done)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_ctx = R"doc(Context with build-specific variables (shared by all threads/tasks))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_depth = R"doc(Depth of the node within the tree)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_indices = R"doc(Index list of primitives to be organized)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_node = R"doc(Node to be initialized by this task)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_m_tight_bbox = R"doc(Tighter bounding box of the contained primitives)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_make_leaf =
R"doc(Create a leaf node using the given set of indices (called by min-max
binning))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_make_leaf_2 =
R"doc(Create a leaf node using the given edge event list (called by the O(N
log N) builder))doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_transition_to_nlogn =
R"doc(Create an initial sorted edge event list and start the O(N log N)
builder)doc";

static const char *__doc_mitsuba_TShapeKDTree_BuildTask_traverse =
R"doc(Traverse a subtree and collect all encountered primitive references in
a set)doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage =
R"doc(Compact storage for primitive classification

When classifying primitives with respect to a split plane, a data
structure is needed to hold the tertiary result of this operation.
This class implements a compact storage (2 bits per entry) in the
spirit of the std::vector<bool> specialization.)doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_get = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_m_buffer = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_m_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_resize = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_set = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_ClassificationStorage_size = R"doc(Return the size (in bytes))doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent =
R"doc(Describes the beginning or end of a primitive under orthogonal
projection onto different axes)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EdgeEvent = R"doc(Dummy constructor)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_EdgeEvent_2 = R"doc(Create a new edge event)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_Type = R"doc(Possible event types)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_Type_EdgeEnd = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_Type_EdgePlanar = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_Type_EdgeStart = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_axis = R"doc(Event axis)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_index = R"doc(Primitive index)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_operator_lt = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_pos = R"doc(Plane position)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_set_invalid = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_type = R"doc(Event type: end/planar/start)doc";

static const char *__doc_mitsuba_TShapeKDTree_EdgeEvent_valid = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode = R"doc(kd-tree node in 8 bytes.)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_axis = R"doc(Return the split axis (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_data = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_leaf = R"doc(Is this a leaf node?)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_left = R"doc(Return the left child (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_left_offset =
R"doc(Assuming that this is an inner node, return the relative offset to the
left child)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_primitive_count = R"doc(Assuming this is a leaf node, return the number of primitives)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_primitive_offset = R"doc(Assuming this is a leaf node, return the first primitive index)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_right = R"doc(Return the left child (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_set_inner_node =
R"doc(Initialize an interior kd-tree node.

Returns ``False`` if the offset or number of primitives is so large
that it can't be represented)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_set_leaf_node =
R"doc(Initialize a leaf kd-tree node.

Returns ``False`` if the offset or number of primitives is so large
that it can't be represented)doc";

static const char *__doc_mitsuba_TShapeKDTree_KDNode_split = R"doc(Return the split plane location (for interior nodes))doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext =
R"doc(Helper data structure used during tree construction (used by a single
thread))doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_classification_storage = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_ctx = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_left_alloc = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_LocalBuildContext_right_alloc = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins =
R"doc(Min-max binning data structure with parallel binning & partitioning
steps

See "Highly Parallel Fast KD-tree Construction for Interactive Ray
Tracing of Dynamic Scenes" by M. Shevtsov, A. Soupikov and A. Kapustin)doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_MinMaxBins = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_MinMaxBins_2 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_MinMaxBins_3 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_left_bounds = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_left_indices = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_right_bounds = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_Partition_right_indices = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_best_candidate = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_bin_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_bins = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_inv_bin_size = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_m_max_bin = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_operator_iadd = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_partition =
R"doc(Given a suitable split candidate, compute tight bounding boxes for the
left and right subtrees and return associated primitive lists.)doc";

static const char *__doc_mitsuba_TShapeKDTree_MinMaxBins_put = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_PrimClassification =
R"doc(Enumeration representing the state of a classified primitive in the
O(N log N) builder)doc";

static const char *__doc_mitsuba_TShapeKDTree_PrimClassification_Both = R"doc(Primitive is right of the split plane)doc";

static const char *__doc_mitsuba_TShapeKDTree_PrimClassification_Ignore = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_PrimClassification_Left = R"doc(Primitive was handled already, ignore from now on)doc";

static const char *__doc_mitsuba_TShapeKDTree_PrimClassification_Right = R"doc(Primitive is left of the split plane)doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate = R"doc(Data type for split candidates suggested by the tree cost model)doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_axis = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_cost = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_left_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_planar_left = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_right_bin = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_right_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_SplitCandidate_split = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_TShapeKDTree = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_bbox = R"doc(Return the bounding box of the entire kd-tree)doc";

static const char *__doc_mitsuba_TShapeKDTree_build = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_class_name = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_clip_primitives = R"doc(Return whether primitive clipping is used during tree construction)doc";

static const char *__doc_mitsuba_TShapeKDTree_compute_statistics = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_cost_model = R"doc(Return the cost model used by the tree construction algorithm)doc";

static const char *__doc_mitsuba_TShapeKDTree_derived = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_derived_2 = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_exact_primitive_threshold =
R"doc(Return the number of primitives, at which the builder will switch from
(approximate) Min-Max binning to the accurate O(n log n) optimization
method.)doc";

static const char *__doc_mitsuba_TShapeKDTree_log_level = R"doc(Return the log level of kd-tree status messages)doc";

static const char *__doc_mitsuba_TShapeKDTree_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_clip_primitives = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_cost_model = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_exact_prim_threshold = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_index_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_indices = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_log_level = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_max_bad_refines = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_max_depth = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_min_max_bins = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_node_count = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_nodes = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_retract_bad_splits = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_m_stop_primitives = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_max_bad_refines =
R"doc(Return the number of bad refines allowed to happen in succession
before a leaf node will be created.)doc";

static const char *__doc_mitsuba_TShapeKDTree_max_depth = R"doc(Return the maximum tree depth (0 == use heuristic))doc";

static const char *__doc_mitsuba_TShapeKDTree_min_max_bins = R"doc(Return the number of bins used for Min-Max binning)doc";

static const char *__doc_mitsuba_TShapeKDTree_ready = R"doc()doc";

static const char *__doc_mitsuba_TShapeKDTree_retract_bad_splits = R"doc(Return whether or not bad splits can be "retracted".)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_clip_primitives = R"doc(Set whether primitive clipping is used during tree construction)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_exact_primitive_threshold =
R"doc(Specify the number of primitives, at which the builder will switch
from (approximate) Min-Max binning to the accurate O(n log n)
optimization method.)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_log_level = R"doc(Return the log level of kd-tree status messages)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_max_bad_refines =
R"doc(Set the number of bad refines allowed to happen in succession before a
leaf node will be created.)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_max_depth = R"doc(Set the maximum tree depth (0 == use heuristic))doc";

static const char *__doc_mitsuba_TShapeKDTree_set_min_max_bins = R"doc(Set the number of bins used for Min-Max binning)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_retract_bad_splits = R"doc(Specify whether or not bad splits can be "retracted".)doc";

static const char *__doc_mitsuba_TShapeKDTree_set_stop_primitives =
R"doc(Set the number of primitives, at which recursion will stop when
building the tree.)doc";

static const char *__doc_mitsuba_TShapeKDTree_stop_primitives =
R"doc(Return the number of primitives, at which recursion will stop when
building the tree.)doc";

static const char *__doc_mitsuba_TensorFile =
R"doc(Simple exchange format for tensor data of arbitrary rank and size

This class provides convenient memory-mapped read-only access to
tensor data, usually exported from NumPy.

The Python functions :python:func:`mi.tensor_io.write(filename,
tensor_1=.., tensor_2=.., ...) <mitsuba.tensor_io.write>` and
:py:func:`tensor_file = mi.tensor_io.read(filename)
<mitsuba.tensor_io.read>` can be used to create and modify these files
within Python.

On the C++ end, use ``tensor_file.field("field_name").as<TensorXf>()``
to upload the data and obtain a device tensor handle.)doc";

static const char *__doc_mitsuba_TensorFile_Field = R"doc(Information about the specified field)doc";

static const char *__doc_mitsuba_TensorFile_Field_data = R"doc(Const pointer to the start of the tensor)doc";

static const char *__doc_mitsuba_TensorFile_Field_dtype = R"doc(Data type (uint32, float, ...) of the tensor)doc";

static const char *__doc_mitsuba_TensorFile_Field_offset = R"doc(Offset within the file)doc";

static const char *__doc_mitsuba_TensorFile_Field_shape = R"doc(Specifies both rank and size along each dimension)doc";

static const char *__doc_mitsuba_TensorFile_Field_to = R"doc()doc";

static const char *__doc_mitsuba_TensorFile_Field_to_string = R"doc(Return a human-readable summary)doc";

static const char *__doc_mitsuba_TensorFile_TensorFile = R"doc(Map the specified file into memory)doc";

static const char *__doc_mitsuba_TensorFile_TensorFileImpl = R"doc()doc";

static const char *__doc_mitsuba_TensorFile_class_name = R"doc()doc";

static const char *__doc_mitsuba_TensorFile_d = R"doc()doc";

static const char *__doc_mitsuba_TensorFile_field = R"doc(Return a data structure with information about the specified field)doc";

static const char *__doc_mitsuba_TensorFile_has_field = R"doc(Does the file contain a field of the specified name?)doc";

static const char *__doc_mitsuba_TensorFile_to_string = R"doc(Return a human-readable summary)doc";

static const char *__doc_mitsuba_Texture =
R"doc(Base class of all surface texture implementations

This class implements a generic texture map that supports evaluation
at arbitrary surface positions and wavelengths (if compiled in
spectral mode). It can be used to provide both intensities (e.g. for
light sources) and unitless reflectance parameters (e.g. an albedo of
a reflectance model).

The spectrum can be evaluated at arbitrary (continuous) wavelengths,
though the underlying function it is not required to be smooth or even
continuous.)doc";

static const char *__doc_mitsuba_Texture_2 = R"doc()doc";

static const char *__doc_mitsuba_Texture_3 = R"doc()doc";

static const char *__doc_mitsuba_Texture_4 = R"doc()doc";

static const char *__doc_mitsuba_Texture_5 = R"doc()doc";

static const char *__doc_mitsuba_Texture_6 = R"doc()doc";

static const char *__doc_mitsuba_Texture_D65 = R"doc(Convenience function returning the standard D65 illuminant)doc";

static const char *__doc_mitsuba_Texture_D65_2 =
R"doc(Convenience function to create a product texture of a texture and the
standard D65 illuminant)doc";

static const char *__doc_mitsuba_Texture_Texture = R"doc()doc";

static const char *__doc_mitsuba_Texture_class_name = R"doc()doc";

static const char *__doc_mitsuba_Texture_eval =
R"doc(Evaluate the texture at the given surface interaction

Parameter ``si``:
    An interaction record describing the associated surface position

Returns:
    An unpolarized spectral power distribution or reflectance value)doc";

static const char *__doc_mitsuba_Texture_eval_1 =
R"doc(Monochromatic evaluation of the texture at the given surface
interaction

This function differs from eval() in that it provided raw access to
scalar intensity/reflectance values without any color processing (e.g.
spectral upsampling). This is useful in parts of the renderer that
encode scalar quantities using textures, e.g. a height field.

Parameter ``si``:
    An interaction record describing the associated surface position

Returns:
    An scalar intensity or reflectance value)doc";

static const char *__doc_mitsuba_Texture_eval_1_grad =
R"doc(Monochromatic evaluation of the texture gradient at the given surface
interaction

Parameter ``si``:
    An interaction record describing the associated surface position

Returns:
    A (u,v) pair of intensity or reflectance value gradients)doc";

static const char *__doc_mitsuba_Texture_eval_3 =
R"doc(Trichromatic evaluation of the texture at the given surface
interaction

This function differs from eval() in that it provided raw access to
RGB intensity/reflectance values without any additional color
processing (e.g. RGB-to-spectral upsampling). This is useful in parts
of the renderer that encode 3D quantities using textures, e.g. a
normal map.

Parameter ``si``:
    An interaction record describing the associated surface position

Returns:
    An trichromatic intensity or reflectance value)doc";

static const char *__doc_mitsuba_Texture_is_spatially_varying = R"doc(Does this texture evaluation depend on the UV coordinates)doc";

static const char *__doc_mitsuba_Texture_max =
R"doc(Return the maximum value of the spectrum

Not every implementation necessarily provides this function. The
default implementation throws an exception.

Even if the operation is provided, it may only return an
approximation.)doc";

static const char *__doc_mitsuba_Texture_mean =
R"doc(Return the mean value of the spectrum over the support
(MI_WAVELENGTH_MIN..MI_WAVELENGTH_MAX)

Not every implementation necessarily provides this function. The
default implementation throws an exception.

Even if the operation is provided, it may only return an
approximation.)doc";

static const char *__doc_mitsuba_Texture_pdf_position = R"doc(Returns the probability per unit area of sample_position())doc";

static const char *__doc_mitsuba_Texture_pdf_spectrum =
R"doc(Evaluate the density function of the sample_spectrum() method as a
probability per unit wavelength (in units of 1/nm).

Not every implementation necessarily overrides this function. The
default implementation throws an exception.

Parameter ``si``:
    An interaction record describing the associated surface position

Returns:
    A density value for each wavelength in ``si.wavelengths`` (hence
    the Wavelength type).)doc";

static const char *__doc_mitsuba_Texture_resolution =
R"doc(Returns the resolution of the texture, assuming that it is based on a
discrete representation.

The default implementation returns ``(1, 1)``)doc";

static const char *__doc_mitsuba_Texture_sample_position =
R"doc(Importance sample a surface position proportional to the overall
spectral reflectance or intensity of the texture

This function assumes that the texture is implemented as a mapping
from 2D UV positions to texture values, which is not necessarily true
for all textures (e.g. 3D noise functions, mesh attributes, etc.). For
this reason, not every will plugin provide a specialized
implementation, and the default implementation simply return the input
sample (i.e. uniform sampling is used).

Parameter ``sample``:
    A 2D vector of uniform variates

Returns:
    1. A texture-space position in the range :math:`[0, 1]^2`

2. The associated probability per unit area in UV space)doc";

static const char *__doc_mitsuba_Texture_sample_spectrum =
R"doc(Importance sample a set of wavelengths proportional to the spectrum
defined at the given surface position

Not every implementation necessarily provides this function, and it is
a no-op when compiling non-spectral variants of Mitsuba. The default
implementation throws an exception.

Parameter ``si``:
    An interaction record describing the associated surface position

Parameter ``sample``:
    A uniform variate for each desired wavelength.

Returns:
    1. Set of sampled wavelengths specified in nanometers

2. The Monte Carlo importance weight (Spectral power distribution
value divided by the sampling density))doc";

static const char *__doc_mitsuba_Texture_spectral_resolution =
R"doc(Returns the resolution of the spectrum in nanometers (if discretized)

Not every implementation necessarily provides this function. The
default implementation throws an exception.)doc";

static const char *__doc_mitsuba_Texture_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Texture_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Texture_type = R"doc()doc";

static const char *__doc_mitsuba_Texture_variant_name = R"doc()doc";

static const char *__doc_mitsuba_Texture_wavelength_range =
R"doc(Returns the range of wavelengths covered by the spectrum

The default implementation returns ``(MI_CIE_MIN, MI_CIE_MAX)``)doc";

static const char *__doc_mitsuba_Thread =
R"doc(Dummy thread class for backward compatibility

This class has been largely stripped down and only maintains essential
methods for file resolver and logger access, plus static
initialization. Use std::thread or the nanothread-based thread pool
for actual threading needs.)doc";

static const char *__doc_mitsuba_Thread_Thread = R"doc(Create a dummy thread object (for compatibility))doc";

static const char *__doc_mitsuba_Thread_class_name = R"doc()doc";

static const char *__doc_mitsuba_Thread_file_resolver = R"doc(Return the global file resolver)doc";

static const char *__doc_mitsuba_Thread_logger = R"doc(Return the global logger instance)doc";

static const char *__doc_mitsuba_Thread_register_task = R"doc(Register nanothread Task to prevent internal resources leakage)doc";

static const char *__doc_mitsuba_Thread_set_logger =
R"doc(Set the global logger used by Mitsuba $.. deprecated::

Use mitsuba::set_logger() directly)doc";

static const char *__doc_mitsuba_Thread_static_initialization = R"doc(Initialize the threading system)doc";

static const char *__doc_mitsuba_Thread_static_shutdown = R"doc(Shut down the threading system)doc";

static const char *__doc_mitsuba_Thread_thread = R"doc(Return the current thread (dummy implementation))doc";

static const char *__doc_mitsuba_Thread_wait_for_tasks = R"doc(Wait for previously registered nanothread tasks to complete)doc";

static const char *__doc_mitsuba_Timer = R"doc()doc";

static const char *__doc_mitsuba_Timer_Timer = R"doc()doc";

static const char *__doc_mitsuba_Timer_begin_stage = R"doc()doc";

static const char *__doc_mitsuba_Timer_end_stage = R"doc()doc";

static const char *__doc_mitsuba_Timer_reset = R"doc()doc";

static const char *__doc_mitsuba_Timer_start = R"doc()doc";

static const char *__doc_mitsuba_Timer_value = R"doc()doc";

static const char *__doc_mitsuba_Transform =
R"doc(Unified homogeneous coordinate transformation

This class represents homogeneous coordinate transformations, i.e.,
composable mappings that include rotations, scaling, translations, and
perspective transformation. As a special case, the implementation can
also be specialized to *affine* (non-perspective) transformations,
which imposes a simpler structure that can be exploited to simplify
certain operations (e.g., transformation of points, compsition,
initialization from a matrix).

The class internally stores the matrix and its inverse transpose. The
latter is precomputed so that the class admits efficient
transformation of surface normals.)doc";

static const char *__doc_mitsuba_Transform_Transform = R"doc(Initialize the transformation from the given matrix)doc";

static const char *__doc_mitsuba_Transform_Transform_2 = R"doc(Initialize the transformation from the given matrix and its inverse)doc";

static const char *__doc_mitsuba_Transform_Transform_3 = R"doc(Copy constructor with type conversion)doc";

static const char *__doc_mitsuba_Transform_Transform_4 = R"doc()doc";

static const char *__doc_mitsuba_Transform_Transform_5 = R"doc()doc";

static const char *__doc_mitsuba_Transform_Transform_6 = R"doc()doc";

static const char *__doc_mitsuba_Transform_expand = R"doc(Expand to a higher-dimensional (inverse of extract))doc";

static const char *__doc_mitsuba_Transform_extract = R"doc(Extract a lower-dimensional submatrix (only for affine transforms))doc";

static const char *__doc_mitsuba_Transform_fields = R"doc()doc";

static const char *__doc_mitsuba_Transform_fields_2 = R"doc()doc";

static const char *__doc_mitsuba_Transform_from_frame =
R"doc(Creates a transformation that converts from 'frame' to the standard
basis)doc";

static const char *__doc_mitsuba_Transform_has_scale =
R"doc(Test for a scale component in each transform matrix by checking
whether ``M . M^T == I`` (where ``M`` is the matrix in question and
``I`` is the identity).)doc";

static const char *__doc_mitsuba_Transform_inverse = R"doc()doc";

static const char *__doc_mitsuba_Transform_inverse_transpose = R"doc()doc";

static const char *__doc_mitsuba_Transform_labels = R"doc()doc";

static const char *__doc_mitsuba_Transform_look_at =
R"doc(Create a look-at camera transformation

Parameter ``origin``:
    Camera position

Parameter ``target``:
    Target vector

Parameter ``up``:
    Up vector)doc";

static const char *__doc_mitsuba_Transform_matrix = R"doc(//! @{ \name Fields)doc";

static const char *__doc_mitsuba_Transform_name = R"doc()doc";

static const char *__doc_mitsuba_Transform_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Transform_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Transform_operator_assign_3 = R"doc()doc";

static const char *__doc_mitsuba_Transform_operator_eq = R"doc(Equality comparison operator)doc";

static const char *__doc_mitsuba_Transform_operator_mul =
R"doc(Transform a 3D vector

Remark:
    In the Python API, this maps to the \c @ operator)doc";

static const char *__doc_mitsuba_Transform_operator_mul_2 =
R"doc(Transform a 3D normal vector

Remark:
    In the Python API, one should use the \c @ operator)doc";

static const char *__doc_mitsuba_Transform_operator_mul_3 =
R"doc(Transform a 3D point with or without perspective division

Remark:
    In the Python API, this maps to the \c @ operator)doc";

static const char *__doc_mitsuba_Transform_operator_mul_4 = R"doc(Transform a ray (optimized for affine case))doc";

static const char *__doc_mitsuba_Transform_operator_mul_5 = R"doc(Concatenate transformations)doc";

static const char *__doc_mitsuba_Transform_operator_ne = R"doc(Inequality comparison operator)doc";

static const char *__doc_mitsuba_Transform_orthographic =
R"doc(Create an orthographic transformation, which maps Z to [0,1] and
leaves the X and Y coordinates untouched.

Parameter ``near``:
    Near clipping plane

Parameter ``far``:
    Far clipping plane)doc";

static const char *__doc_mitsuba_Transform_perspective =
R"doc(Create a perspective transformation. (Maps [near, far] to [0, 1])

Projects vectors in camera space onto a plane at z=1:

x_proj = x / z y_proj = y / z z_proj = (far * (z - near)) / (z * (far-
near))

Camera-space depths are not mapped linearly!

Parameter ``fov``:
    Field of view in degrees

Parameter ``near``:
    Near clipping plane

Parameter ``far``:
    Far clipping plane)doc";

static const char *__doc_mitsuba_Transform_rotate =
R"doc(Create a rotation transformation around an arbitrary axis in 3D. The
angle is specified in degrees)doc";

static const char *__doc_mitsuba_Transform_rotate_2 =
R"doc(Create a rotation transformation in 2D. The angle is specified in
degrees)doc";

static const char *__doc_mitsuba_Transform_scale = R"doc(Create a scale transformation)doc";

static const char *__doc_mitsuba_Transform_to_frame =
R"doc(Creates a transformation that converts from the standard basis to
'frame')doc";

static const char *__doc_mitsuba_Transform_transform_affine = R"doc()doc";

static const char *__doc_mitsuba_Transform_translate = R"doc(Create a translation transformation)doc";

static const char *__doc_mitsuba_Transform_translation = R"doc(Get the translation part of a matrix)doc";

static const char *__doc_mitsuba_Transform_transpose = R"doc()doc";

static const char *__doc_mitsuba_Transform_update = R"doc(Update the inverse transpose part following a modification to 'matrix')doc";

static const char *__doc_mitsuba_TransportMode =
R"doc(Specifies the transport mode when sampling or evaluating a scattering
function)doc";

static const char *__doc_mitsuba_TransportMode_Importance = R"doc(Importance transport)doc";

static const char *__doc_mitsuba_TransportMode_Radiance = R"doc(Radiance transport)doc";

static const char *__doc_mitsuba_TransportMode_TransportModes = R"doc(Specifies the number of supported transport modes)doc";

static const char *__doc_mitsuba_TraversalCallback =
R"doc(Abstract class providing an interface for traversing scene graphs

This interface can be implemented either in C++ or in Python, to be
used in conjunction with Object::traverse() to traverse a scene graph.
Mitsuba uses this mechanism for two primary purposes:

1. **Dynamic scene modification**: After a scene is loaded, the
traversal mechanism allows programmatic access to modify scene
parameters without rebuilding the entire scene. This enables workflows
where parameters are adjusted and the scene is re-rendered with
different settings.

2. **Differentiable parameter discovery**: The traversal callback can
discover all differentiable parameters in a scene (e.g., material
properties, transformation matrices, emission values). These
parameters can then be exposed to gradient-based optimizers for
inverse rendering tasks, which in practice involves the
``SceneParameters`` Python class.

The callback receives information about each traversed object's
parameters through the put() methods, which distinguish between
regular parameters and references to other scene objects that are
handled recursively.)doc";

static const char *__doc_mitsuba_TraversalCallback_put = R"doc()doc";

static const char *__doc_mitsuba_TraversalCallback_put_2 = R"doc()doc";

static const char *__doc_mitsuba_TraversalCallback_put_3 = R"doc()doc";

static const char *__doc_mitsuba_TraversalCallback_put_4 =
R"doc(Forward declaration for field<...> values that simultaneously store
host+device values)doc";

static const char *__doc_mitsuba_TraversalCallback_put_object =
R"doc(Actual implementation for Object references [To be provided by
subclass])doc";

static const char *__doc_mitsuba_TraversalCallback_put_value =
R"doc(Actual implementation for regular parameters [To be provided by
subclass])doc";

static const char *__doc_mitsuba_Vector = R"doc(//! @{ \name Elementary vector, point, and normal data types)doc";

static const char *__doc_mitsuba_Vector_Vector = R"doc()doc";

static const char *__doc_mitsuba_Vector_Vector_2 = R"doc()doc";

static const char *__doc_mitsuba_Vector_Vector_3 = R"doc()doc";

static const char *__doc_mitsuba_Vector_operator_assign = R"doc()doc";

static const char *__doc_mitsuba_Vector_operator_assign_2 = R"doc()doc";

static const char *__doc_mitsuba_Volume = R"doc(Abstract base class for 3D volumes.)doc";

static const char *__doc_mitsuba_Volume_2 = R"doc()doc";

static const char *__doc_mitsuba_Volume_3 = R"doc()doc";

static const char *__doc_mitsuba_Volume_4 = R"doc()doc";

static const char *__doc_mitsuba_Volume_5 = R"doc()doc";

static const char *__doc_mitsuba_Volume_6 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid =
R"doc(Class to read and write 3D volume grids

This class handles loading of volumes in the Mitsuba volume file
format Please see the documentation of gridvolume (grid3d.cpp) for the
file format specification.)doc";

static const char *__doc_mitsuba_VolumeGrid_2 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_3 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_4 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_5 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_6 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_VolumeGrid =
R"doc(Load a VolumeGrid from a given filename

Parameter ``path``:
    Name of the file to be loaded)doc";

static const char *__doc_mitsuba_VolumeGrid_VolumeGrid_2 =
R"doc(Load a VolumeGrid from an arbitrary stream data source

Parameter ``stream``:
    Pointer to an arbitrary stream data source)doc";

static const char *__doc_mitsuba_VolumeGrid_VolumeGrid_3 = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_bbox_transform =
R"doc(Estimates the transformation from a unit axis-aligned bounding box to
the given one.)doc";

static const char *__doc_mitsuba_VolumeGrid_buffer_size = R"doc(Return the volume grid size in bytes (excluding metadata))doc";

static const char *__doc_mitsuba_VolumeGrid_bytes_per_voxel = R"doc(Return the number bytes of storage used per voxel)doc";

static const char *__doc_mitsuba_VolumeGrid_channel_count = R"doc(Return the number of channels)doc";

static const char *__doc_mitsuba_VolumeGrid_class_name = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_data = R"doc(Return a pointer to the underlying volume storage)doc";

static const char *__doc_mitsuba_VolumeGrid_data_2 = R"doc(Return a pointer to the underlying volume storage)doc";

static const char *__doc_mitsuba_VolumeGrid_m_bbox = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_m_channel_count = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_m_data = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_m_max = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_m_max_per_channel = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_m_size = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_max = R"doc(Return the precomputed maximum over the volume grid)doc";

static const char *__doc_mitsuba_VolumeGrid_max_per_channel =
R"doc(Return the precomputed maximum over the volume grid per channel

Pointer allocation/deallocation must be performed by the caller.)doc";

static const char *__doc_mitsuba_VolumeGrid_read = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_set_max = R"doc(Set the precomputed maximum over the volume grid)doc";

static const char *__doc_mitsuba_VolumeGrid_set_max_per_channel =
R"doc(Set the precomputed maximum over the volume grid per channel

Pointer allocation/deallocation must be performed by the caller.)doc";

static const char *__doc_mitsuba_VolumeGrid_size = R"doc(Return the resolution of the voxel grid)doc";

static const char *__doc_mitsuba_VolumeGrid_to_string = R"doc(Return a human-readable summary of this volume grid)doc";

static const char *__doc_mitsuba_VolumeGrid_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_VolumeGrid_write =
R"doc(Write an encoded form of the bitmap to a binary volume file

Parameter ``path``:
    Target file name (expected to end in ".vol"))doc";

static const char *__doc_mitsuba_VolumeGrid_write_2 =
R"doc(Write an encoded form of the volume grid to a stream

Parameter ``stream``:
    Target stream that will receive the encoded output)doc";

static const char *__doc_mitsuba_Volume_Volume = R"doc()doc";

static const char *__doc_mitsuba_Volume_bbox = R"doc(Returns the bounding box of the volume)doc";

static const char *__doc_mitsuba_Volume_channel_count =
R"doc(Returns the number of channels stored in the volume

When the channel count is zero, it indicates that the volume does not
support per-channel queries.)doc";

static const char *__doc_mitsuba_Volume_class_name = R"doc()doc";

static const char *__doc_mitsuba_Volume_eval =
R"doc(Evaluate the volume at the given surface interaction, with color
processing.)doc";

static const char *__doc_mitsuba_Volume_eval_1 = R"doc(Evaluate this volume as a single-channel quantity.)doc";

static const char *__doc_mitsuba_Volume_eval_3 =
R"doc(Evaluate this volume as a three-channel quantity with no color
processing (e.g. velocity field).)doc";

static const char *__doc_mitsuba_Volume_eval_6 =
R"doc(Evaluate this volume as a six-channel quantity with no color
processing This interface is specifically intended to encode the
parameters of an SGGX phase function.)doc";

static const char *__doc_mitsuba_Volume_eval_gradient =
R"doc(Evaluate the volume at the given surface interaction, and compute the
gradients of the linear interpolant as well.)doc";

static const char *__doc_mitsuba_Volume_eval_n =
R"doc(Evaluate this volume as a n-channel float quantity

This interface is specifically intended to encode a variable number of
parameters. Pointer allocation/deallocation must be performed by the
caller.)doc";

static const char *__doc_mitsuba_Volume_m_bbox = R"doc(Bounding box)doc";

static const char *__doc_mitsuba_Volume_m_channel_count = R"doc(Number of channels stored in the volume)doc";

static const char *__doc_mitsuba_Volume_m_to_local = R"doc(Used to bring points in world coordinates to local coordinates.)doc";

static const char *__doc_mitsuba_Volume_max = R"doc(Returns the maximum value of the volume over all dimensions.)doc";

static const char *__doc_mitsuba_Volume_max_per_channel =
R"doc(In the case of a multi-channel volume, this function returns the
maximum value for each channel.

Pointer allocation/deallocation must be performed by the caller.)doc";

static const char *__doc_mitsuba_Volume_resolution =
R"doc(Returns the resolution of the volume, assuming that it is based on a
discrete representation.

The default implementation returns ``(1, 1, 1)``)doc";

static const char *__doc_mitsuba_Volume_to_string = R"doc(Returns a human-reable summary)doc";

static const char *__doc_mitsuba_Volume_traverse_1_cb_ro = R"doc()doc";

static const char *__doc_mitsuba_Volume_traverse_1_cb_rw = R"doc()doc";

static const char *__doc_mitsuba_Volume_type = R"doc()doc";

static const char *__doc_mitsuba_Volume_update_bbox = R"doc()doc";

static const char *__doc_mitsuba_Volume_variant_name = R"doc()doc";

static const char *__doc_mitsuba_ZStream =
R"doc(Transparent compression/decompression stream based on ``zlib``.

This class transparently decompresses and compresses reads and writes
to a nested stream, respectively.)doc";

static const char *__doc_mitsuba_ZStream_EStreamType = R"doc()doc";

static const char *__doc_mitsuba_ZStream_EStreamType_EDeflateStream = R"doc()doc";

static const char *__doc_mitsuba_ZStream_EStreamType_EGZipStream = R"doc(A raw deflate stream)doc";

static const char *__doc_mitsuba_ZStream_ZStream =
R"doc(Creates a new compression stream with the given underlying stream.
This new instance takes ownership of the child stream. The child
stream must outlive the ZStream.)doc";

static const char *__doc_mitsuba_ZStream_can_read = R"doc(Can we read from the stream?)doc";

static const char *__doc_mitsuba_ZStream_can_write = R"doc(Can we write to the stream?)doc";

static const char *__doc_mitsuba_ZStream_child_stream = R"doc(Returns the child stream of this compression stream)doc";

static const char *__doc_mitsuba_ZStream_child_stream_2 = R"doc(Returns the child stream of this compression stream)doc";

static const char *__doc_mitsuba_ZStream_class_name = R"doc(//! @})doc";

static const char *__doc_mitsuba_ZStream_close =
R"doc(Closes the stream, but not the underlying child stream. No further
read or write operations are permitted.

This function is idempotent. It is called automatically by the
destructor.)doc";

static const char *__doc_mitsuba_ZStream_flush = R"doc(Flushes any buffered data)doc";

static const char *__doc_mitsuba_ZStream_is_closed = R"doc(Whether the stream is closed (no read or write are then permitted).)doc";

static const char *__doc_mitsuba_ZStream_m_child_stream = R"doc(//! @})doc";

static const char *__doc_mitsuba_ZStream_m_deflate_buffer = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_deflate_stream = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_did_write = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_inflate_buffer = R"doc()doc";

static const char *__doc_mitsuba_ZStream_m_inflate_stream = R"doc()doc";

static const char *__doc_mitsuba_ZStream_read =
R"doc(Reads a specified amount of data from the stream, decompressing it
first using ZLib. Throws an exception when the stream ended
prematurely.)doc";

static const char *__doc_mitsuba_ZStream_seek = R"doc(Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_size = R"doc(Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_tell = R"doc(Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_to_string = R"doc(Returns a string representation)doc";

static const char *__doc_mitsuba_ZStream_truncate = R"doc(/ Unsupported. Always throws.)doc";

static const char *__doc_mitsuba_ZStream_write =
R"doc(Writes a specified amount of data into the stream, compressing it
first using ZLib. Throws an exception when not all data could be
written.)doc";

static const char *__doc_mitsuba_accumulate_2d =
R"doc(Accumulate the contents of a source bitmap into a target bitmap with
specified offsets for both.

Out-of-bounds regions are safely ignored. It is assumed that ``source
!= target``.

The function supports `T` being a raw pointer or an arbitrary Dr.Jit
array that can potentially live on the GPU and/or be differentiable.)doc";

static const char *__doc_mitsuba_any_cast = R"doc()doc";

static const char *__doc_mitsuba_any_cast_2 = R"doc()doc";

static const char *__doc_mitsuba_begin = R"doc()doc";

static const char *__doc_mitsuba_bsdf =
R"doc(Returns the BSDF of the intersected shape.

The parameter ``ray`` must match the one used to create the
interaction record. This function computes texture coordinate partials
if this is required by the BSDF (e.g. for texture filtering).

Implementation in 'bsdf.h')doc";

static const char *__doc_mitsuba_build_gas =
R"doc(Build OptiX geometry acceleration structures (GAS) for a given list of
shapes.

Two different GAS will be created for the meshes and the custom
shapes. Optix handles to those GAS will be stored in an
OptixAccelData.)doc";

static const char *__doc_mitsuba_cie1931_xyz =
R"doc(Evaluate the CIE 1931 XYZ color matching functions given a wavelength
in nanometers)doc";

static const char *__doc_mitsuba_cie1931_y =
R"doc(Evaluate the CIE 1931 Y color matching function given a wavelength in
nanometers)doc";

static const char *__doc_mitsuba_cie_d65 =
R"doc(Evaluate the CIE D65 illuminant spectrum given a wavelength in
nanometers, normalized to ensures that it integrates to a luminance of
1.0.)doc";

static const char *__doc_mitsuba_color_management_static_initialization = R"doc(Allocate arrays for the color space tables)doc";

static const char *__doc_mitsuba_color_management_static_shutdown = R"doc()doc";

static const char *__doc_mitsuba_comparator = R"doc()doc";

static const char *__doc_mitsuba_comparator_operator_call = R"doc()doc";

static const char *__doc_mitsuba_complex_ior_from_file = R"doc()doc";

static const char *__doc_mitsuba_compute_shading_frame =
R"doc(Given a smoothly varying shading normal and a tangent of a shape
parameterization, compute a smoothly varying orthonormal frame.

Parameter ``n``:
    A shading normal at a surface position

Parameter ``dp_du``:
    Position derivative of the underlying parameterization with
    respect to the 'u' coordinate

Parameter ``frame``:
    Used to return the computed frame)doc";

static const char *__doc_mitsuba_coordinate_system = R"doc(Complete the set {a} to an orthonormal basis {a, b, c})doc";

static const char *__doc_mitsuba_depolarizer =
R"doc(Turn a spectrum into a Mueller matrix representation that only has a
non-zero (1,1) entry. For all non-polarized modes, this is the
identity function.

Apart from the obvious usecase as a depolarizing Mueller matrix (e.g.
for a Lambertian diffuse material), this is also currently used in
many BSDFs and emitters where it is not clear how they should interact
with polarization.)doc";

static const char *__doc_mitsuba_detail_CIE1932Tables =
R"doc(Struct carrying color space tables with fits for cie1931_xyz and
cie1931_y as well as corresponding precomputed ITU-R Rec. BT.709
linear RGB tables.)doc";

static const char *__doc_mitsuba_detail_CIE1932Tables_d65 = R"doc(CIE D65 illuminant spectrum table)doc";

static const char *__doc_mitsuba_detail_CIE1932Tables_initialize = R"doc()doc";

static const char *__doc_mitsuba_detail_CIE1932Tables_initialized = R"doc()doc";

static const char *__doc_mitsuba_detail_CIE1932Tables_release = R"doc()doc";

static const char *__doc_mitsuba_detail_CIE1932Tables_srgb = R"doc(ITU-R Rec. BT.709 linear RGB tables)doc";

static const char *__doc_mitsuba_detail_CIE1932Tables_xyz = R"doc(CIE 1931 XYZ color tables)doc";

static const char *__doc_mitsuba_detail_ConcurrentVector = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_ConcurrentVector = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_ConcurrentVector_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_ConcurrentVector_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_grow_by = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_log2i = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_m_size_and_capacity = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_m_slices = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_operator_array = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_operator_array_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_release = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_reserve = R"doc()doc";

static const char *__doc_mitsuba_detail_ConcurrentVector_size = R"doc()doc";

static const char *__doc_mitsuba_detail_Log = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator =
R"doc(During kd-tree construction, large amounts of memory are required to
temporarily hold index and edge event lists. When not implemented
properly, these allocations can become a critical bottleneck. The
class OrderedChunkAllocator provides a specialized memory allocator,
which reserves memory in chunks of at least 512KiB (this number is
configurable). An important assumption made by the allocator is that
memory will be released in the exact same order in which it was
previously allocated. This makes it possible to create an
implementation with a very low memory overhead. Note that no locking
is done, hence each thread will need its own allocator.)doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_Chunk = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_contains = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_cur = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_remainder = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_size = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_start = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_Chunk_used = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_OrderedChunkAllocator = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_allocate =
R"doc(Request a block of memory from the allocator

Walks through the list of chunks to find one with enough free memory.
If no chunk could be found, a new one is created.)doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_chunk_count = R"doc(Return the currently allocated number of chunks)doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_m_chunks = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_m_min_allocation = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_release = R"doc()doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_shrink_allocation = R"doc(Shrink the size of the last allocated chunk)doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_size = R"doc(Return the total amount of chunk memory in bytes)doc";

static const char *__doc_mitsuba_detail_OrderedChunkAllocator_used = R"doc(Return the total amount of used memory in bytes)doc";

static const char *__doc_mitsuba_detail_Throw = R"doc()doc";

static const char *__doc_mitsuba_detail_base_type = R"doc()doc";

static const char *__doc_mitsuba_detail_base_type_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_compare_typeid = R"doc()doc";

static const char *__doc_mitsuba_detail_get_color_space_tables = R"doc()doc";

static const char *__doc_mitsuba_detail_is_transform_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_4 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_5 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_6 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_7 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_8 = R"doc()doc";

static const char *__doc_mitsuba_detail_prop_map_9 = R"doc()doc";

static const char *__doc_mitsuba_detail_serialization_helper =
R"doc(The serialization_helper<T> implementations for new types should in
general be implemented as a series of calls to the lower-level
serialization_helper::{read,write} functions. This way, endianness
swapping needs only be handled at the lowest level.)doc";

static const char *__doc_mitsuba_detail_serialization_helper_2 =
R"doc(The serialization_helper<T> implementations for new types should in
general be implemented as a series of calls to the lower-level
serialization_helper::{read,write} functions. This way, endianness
swapping needs only be handled at the lowest level.)doc";

static const char *__doc_mitsuba_detail_serialization_helper_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_serialization_helper_read =
R"doc(Reads ``count`` values of type T from stream ``s``, starting at its
current position. Note: ``count`` is the number of values, **not** a
size in bytes.

Support for additional types can be added in any header file by
declaring a template specialization for your type.)doc";

static const char *__doc_mitsuba_detail_serialization_helper_read_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_serialization_helper_type_id = R"doc()doc";

static const char *__doc_mitsuba_detail_serialization_helper_type_id_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_serialization_helper_write =
R"doc(Writes ``count`` values of type T into stream ``s`` starting at its
current position. Note: ``count`` is the number of values, **not** a
size in bytes.

Support for additional types can be added in any header file by
declaring a template specialization for your type.)doc";

static const char *__doc_mitsuba_detail_serialization_helper_write_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_spectrum_traits = R"doc(//! @{ \name Color mode traits)doc";

static const char *__doc_mitsuba_detail_spectrum_traits_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_4 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_5 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_6 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_7 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_8 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_9 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_10 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_11 = R"doc()doc";

static const char *__doc_mitsuba_detail_struct_type_12 = R"doc()doc";

static const char *__doc_mitsuba_detail_swap = R"doc()doc";

static const char *__doc_mitsuba_detail_swap_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_swap_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_swap_4 = R"doc()doc";

static const char *__doc_mitsuba_detail_underlying = R"doc(Type trait to strip away dynamic/masking-related type wrappers)doc";

static const char *__doc_mitsuba_detail_underlying_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant =
R"doc(Helper partial template overload to determine the variant string
associated with a given floating point and spectral type)doc";

static const char *__doc_mitsuba_detail_variant_2 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_3 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_4 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_5 = R"doc()doc";

static const char *__doc_mitsuba_detail_variant_6 = R"doc()doc";

static const char *__doc_mitsuba_dir_to_sph =
R"doc(Converts a unit vector to its spherical coordinates parameterization

Parameter ``v``:
    Vector to convert

Returns:
    The polar and azimuthal angles respectively.)doc";

static const char *__doc_mitsuba_emitter =
R"doc(Return the emitter associated with the intersection (if any) \note
Defined in scene.h)doc";

static const char *__doc_mitsuba_end = R"doc()doc";

static const char *__doc_mitsuba_eval_reflectance = R"doc()doc";

static const char *__doc_mitsuba_eval_transmittance = R"doc()doc";

static const char *__doc_mitsuba_field =
R"doc(Convenience wrapper to simultaneously instantiate a host and a device
version of a type

This class implements a simple wrapper that replicates instance
attributes on the host and device. This is only relevant when
``DeviceType`` is a JIT-compiled Dr.Jit array (when compiling the
renderer in CUDA/LLVM mode).

Why is this needed? Mitsuba plugins represent their internal state
using attributes like position, intensity, etc., which are typically
represented using Dr.Jit arrays. For technical reasons, it is helpful
if those fields are both accessible on the host (in which case the
lowest-level representation builds on standard C++ types like float or
int, for example Point<float, 3>) or the device, whose types invoke
the JIT compiler (e.g., Point<CUDAArray<float>, 3>). Copying this data
back and forth can be costly if both host and device require
simultaneous access. Even if all code effectively runs on the host
(e.g. in LLVM mode), accessing "LLVM device" arrays still requires
traversal of JIT compiler data structures, which was a severe
bottleneck e.g. when Embree calls shape-specific intersection
routines.)doc";

static const char *__doc_mitsuba_file_resolver =
R"doc(Return the global file resolver instance (this is a process-wide
setting))doc";

static const char *__doc_mitsuba_filesystem_absolute =
R"doc(Returns an absolute path to the same location pointed by ``p``,
relative to ``base``.

See also:
    http ://en.cppreference.com/w/cpp/experimental/fs/absolute))doc";

static const char *__doc_mitsuba_filesystem_copy_file =
R"doc(Copies a file from source to destination. Returns true if copying was
successful, false if there was an error (e.g. the source file did not
exist). Creates intermediate directories if necessary.)doc";

static const char *__doc_mitsuba_filesystem_create_directory =
R"doc(Creates a directory at ``p`` as if ``mkdir`` was used. Returns true if
directory creation was successful, false otherwise. If ``p`` already
exists and is already a directory, the function does nothing (this
condition is not treated as an error).)doc";

static const char *__doc_mitsuba_filesystem_current_path = R"doc(Returns the current working directory (equivalent to getcwd))doc";

static const char *__doc_mitsuba_filesystem_equivalent =
R"doc(Checks whether two paths refer to the same file system object. Both
must refer to an existing file or directory. Symlinks are followed to
determine equivalence.)doc";

static const char *__doc_mitsuba_filesystem_exists = R"doc(Checks if ``p`` points to an existing filesystem object.)doc";

static const char *__doc_mitsuba_filesystem_file_size =
R"doc(Returns the size (in bytes) of a regular file at ``p``. Attempting to
determine the size of a directory (as well as any other file that is
not a regular file or a symlink) is treated as an error.)doc";

static const char *__doc_mitsuba_filesystem_is_directory = R"doc(Checks if ``p`` points to a directory.)doc";

static const char *__doc_mitsuba_filesystem_is_regular_file =
R"doc(Checks if ``p`` points to a regular file, as opposed to a directory or
symlink.)doc";

static const char *__doc_mitsuba_filesystem_path =
R"doc(Represents a path to a filesystem resource. On construction, the path
is parsed and stored in a system-agnostic representation. The path can
be converted back to the system-specific string using ``native()`` or
``string()``.)doc";

static const char *__doc_mitsuba_filesystem_path_clear = R"doc(Makes the path an empty path. An empty path is considered relative.)doc";

static const char *__doc_mitsuba_filesystem_path_empty = R"doc(Checks if the path is empty)doc";

static const char *__doc_mitsuba_filesystem_path_extension =
R"doc(Returns the extension of the filename component of the path (the
substring starting at the rightmost period, including the period).
Special paths '.' and '..' have an empty extension.)doc";

static const char *__doc_mitsuba_filesystem_path_filename = R"doc(Returns the filename component of the path, including the extension.)doc";

static const char *__doc_mitsuba_filesystem_path_is_absolute = R"doc(Checks if the path is absolute.)doc";

static const char *__doc_mitsuba_filesystem_path_is_relative = R"doc(Checks if the path is relative.)doc";

static const char *__doc_mitsuba_filesystem_path_m_absolute = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_m_path = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_native =
R"doc(Returns the path in the form of a native string, so that it can be
passed directly to system APIs. The path is constructed using the
system's preferred separator and the native string type.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign = R"doc(Assignment operator.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign_2 = R"doc(Move assignment operator.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_assign_3 =
R"doc(Assignment from the system's native string type. Acts similarly to the
string constructor.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_basic_string =
R"doc(Implicit conversion operator to the basic_string corresponding to the
system's character type. Equivalent to calling ``native()``.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_div = R"doc(Concatenates two paths with a directory separator.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_eq =
R"doc(Equality operator. Warning: this only checks for lexicographic
equivalence. To check whether two paths point to the same filesystem
resource, use ``equivalent``.)doc";

static const char *__doc_mitsuba_filesystem_path_operator_ne = R"doc(Inequality operator.)doc";

static const char *__doc_mitsuba_filesystem_path_parent_path =
R"doc(Returns the path to the parent directory. Returns an empty path if it
is already empty or if it has only one element.)doc";

static const char *__doc_mitsuba_filesystem_path_path =
R"doc(Default constructor. Constructs an empty path. An empty path is
considered relative.)doc";

static const char *__doc_mitsuba_filesystem_path_path_2 = R"doc(Copy constructor.)doc";

static const char *__doc_mitsuba_filesystem_path_path_3 = R"doc(Move constructor.)doc";

static const char *__doc_mitsuba_filesystem_path_path_4 =
R"doc(Construct a path from a string view with native type. On Windows, the
path can use both '/' or '\\' as a delimiter.)doc";

static const char *__doc_mitsuba_filesystem_path_path_5 =
R"doc(Construct a path from a string with native type. On Windows, the path
can use both '/' or '\\' as a delimiter.)doc";

static const char *__doc_mitsuba_filesystem_path_path_6 =
R"doc(Construct a path from a native C-style string. On Windows, this
accepts wide strings (wchar_t*). On other platforms, this accepts
regular strings (char*).)doc";

static const char *__doc_mitsuba_filesystem_path_replace_extension =
R"doc(Replaces the substring starting at the rightmost '.' symbol by the
provided string.

A '.' symbol is automatically inserted if the replacement does not
start with a dot. Removes the extension altogether if the empty path
is passed. If there is no extension, appends a '.' followed by the
replacement. If the path is empty, '.' or '..', the method does
nothing.

Returns *this.)doc";

static const char *__doc_mitsuba_filesystem_path_set = R"doc(Builds a path from the passed string.)doc";

static const char *__doc_mitsuba_filesystem_path_str = R"doc()doc";

static const char *__doc_mitsuba_filesystem_path_string = R"doc(Equivalent to native(), converted to the std::string type)doc";

static const char *__doc_mitsuba_filesystem_path_tokenize =
R"doc(Splits a string into tokens delimited by any of the characters passed
in ``delim``.)doc";

static const char *__doc_mitsuba_filesystem_remove =
R"doc(Removes a file or empty directory. Returns true if removal was
successful, false if there was an error (e.g. the file did not exist).)doc";

static const char *__doc_mitsuba_filesystem_rename =
R"doc(Renames a file or directory. Returns true if renaming was successful,
false if there was an error (e.g. the file did not exist).)doc";

static const char *__doc_mitsuba_filesystem_resize_file =
R"doc(Changes the size of the regular file named by ``p`` as if ``truncate``
was called. If the file was larger than ``target_length``, the
remainder is discarded. The file must exist.)doc";

static const char *__doc_mitsuba_fill_hitgroup_records = R"doc(Creates and appends the HitGroupSbtRecord for a given list of shapes)doc";

static const char *__doc_mitsuba_fresnel =
R"doc(Calculates the unpolarized Fresnel reflection coefficient at a planar
interface between two dielectrics

Parameter ``cos_theta_i``:
    Cosine of the angle between the surface normal and the incident
    ray

Parameter ``eta``:
    Relative refractive index of the interface. A value greater than
    1.0 means that the surface normal is pointing into the region of
    lower density.

Returns:
    A tuple (F, cos_theta_t, eta_it, eta_ti) consisting of

F Fresnel reflection coefficient.

cos_theta_t Cosine of the angle between the surface normal and the
transmitted ray

eta_it Relative index of refraction in the direction of travel.

eta_ti Reciprocal of the relative index of refraction in the direction
of travel. This also happens to be equal to the scale factor that must
be applied to the X and Y component of the refracted direction.)doc";

static const char *__doc_mitsuba_fresnel_conductor =
R"doc(Calculates the unpolarized Fresnel reflection coefficient at a planar
interface of a conductor, i.e. a surface with a complex-valued
relative index of refraction

Remark:
    The implementation assumes that cos_theta_i > 0, i.e. light enters
    from *outside* of the conducting layer (generally a reasonable
    assumption unless very thin layers are being simulated)

Parameter ``cos_theta_i``:
    Cosine of the angle between the surface normal and the incident
    ray

Parameter ``eta``:
    Relative refractive index (complex-valued)

Returns:
    The unpolarized Fresnel reflection coefficient.)doc";

static const char *__doc_mitsuba_fresnel_diffuse_reflectance =
R"doc(Computes the diffuse unpolarized Fresnel reflectance of a dielectric
material (sometimes referred to as "Fdr").

This value quantifies what fraction of diffuse incident illumination
will, on average, be reflected at a dielectric material boundary

Parameter ``eta``:
    Relative refraction coefficient

Returns:
    F, the unpolarized Fresnel coefficient.)doc";

static const char *__doc_mitsuba_fresnel_polarized =
R"doc(Calculates the polarized Fresnel reflection coefficient at a planar
interface between two dielectrics. Returns complex values encoding the
amplitude and phase shift of the s- and p-polarized waves.

Parameter ``cos_theta_i``:
    Cosine of the angle between the surface normal and the incident
    ray

Parameter ``eta``:
    Real-valued relative refractive index of the interface. A value
    greater than 1.0 case means that the surface normal points into
    the region of lower density.

Returns:
    A tuple (a_s, a_p, cos_theta_t, eta_it, eta_ti) consisting of

a_s Perpendicularly polarized wave amplitude and phase shift.

a_p Parallel polarized wave amplitude and phase shift.

cos_theta_t Cosine of the angle between the surface normal and the
transmitted ray. Zero in the case of total internal reflection.

eta_it Relative index of refraction in the direction of travel

eta_ti Reciprocal of the relative index of refraction in the direction
of travel. This also happens to be equal to the scale factor that must
be applied to the X and Y component of the refracted direction.)doc";

static const char *__doc_mitsuba_fresnel_polarized_2 =
R"doc(Calculates the polarized Fresnel reflection coefficient at a planar
interface between two dielectrics or conductors. Returns complex
values encoding the amplitude and phase shift of the s- and
p-polarized waves.

This is the most general version, which subsumes all others (at the
cost of transcendental function evaluations in the complex-valued
arithmetic)

Parameter ``cos_theta_i``:
    Cosine of the angle between the surface normal and the incident
    ray

Parameter ``eta``:
    Complex-valued relative refractive index of the interface. In the
    real case, a value greater than 1.0 case means that the surface
    normal points into the region of lower density.

Returns:
    A tuple (a_s, a_p, cos_theta_t, eta_it, eta_ti) consisting of

a_s Perpendicularly polarized wave amplitude and phase shift.

a_p Parallel polarized wave amplitude and phase shift.

cos_theta_t Cosine of the angle between the surface normal and the
transmitted ray. Zero in the case of total internal reflection.

eta_it Relative index of refraction in the direction of travel

eta_ti Reciprocal of the relative index of refraction in the direction
of travel. In the real-valued case, this also happens to be equal to
the scale factor that must be applied to the X and Y component of the
refracted direction.)doc";

static const char *__doc_mitsuba_get =
R"doc(Retrieve a scalar parameter by index

This is the primary implementation. All type conversions and error
checking is done here. The name-based get() is a thin wrapper.)doc";

static const char *__doc_mitsuba_get_volume =
R"doc(Retrieve a volume parameter

This method retrieves a volume parameter, where ``T`` is a subclass of
``mitsuba::Volume<...>``.

Scalar and texture values are also accepted. In this case, the plugin
manager will automatically construct a ``constvolume`` instance.)doc";

static const char *__doc_mitsuba_get_volume_2 =
R"doc(Retrieve a volume parameter with float default

When the volume parameter doesn't exist, creates a constant volume
with the specified floating point value.)doc";

static const char *__doc_mitsuba_has_flag = R"doc()doc";

static const char *__doc_mitsuba_has_flag_2 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_3 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_4 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_5 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_6 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_7 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_8 = R"doc()doc";

static const char *__doc_mitsuba_has_flag_9 = R"doc()doc";

static const char *__doc_mitsuba_hash = R"doc()doc";

static const char *__doc_mitsuba_hash_2 = R"doc()doc";

static const char *__doc_mitsuba_hash_3 = R"doc()doc";

static const char *__doc_mitsuba_hash_4 = R"doc()doc";

static const char *__doc_mitsuba_hash_5 = R"doc()doc";

static const char *__doc_mitsuba_hash_6 = R"doc()doc";

static const char *__doc_mitsuba_hash_combine = R"doc()doc";

static const char *__doc_mitsuba_hasher = R"doc()doc";

static const char *__doc_mitsuba_hasher_operator_call = R"doc()doc";

static const char *__doc_mitsuba_ior_from_file = R"doc()doc";

static const char *__doc_mitsuba_key_iterator = R"doc(Forward declaration of iterator)doc";

static const char *__doc_mitsuba_key_iterator_get = R"doc(Retrieve the current property value using Properties::get<T>)doc";

static const char *__doc_mitsuba_key_iterator_index = R"doc(Return the current property index in the internal storage)doc";

static const char *__doc_mitsuba_key_iterator_key_iterator = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_m_filter_type = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_m_index = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_m_name = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_m_props = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_m_queried = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_m_type = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_name = R"doc(Return the current property name)doc";

static const char *__doc_mitsuba_key_iterator_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_operator_inc = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_operator_inc_2 = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_operator_mul = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_operator_sub = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_queried = R"doc(Check if the current property has been queried)doc";

static const char *__doc_mitsuba_key_iterator_skip_to_next_valid = R"doc()doc";

static const char *__doc_mitsuba_key_iterator_try_get =
R"doc(Attempt to retrieve and cast an object property to a specific type

This method retrieves the property value if it's an Object type and
attempts to dynamically cast it to the requested type T. The property
is only marked as queried if the cast succeeds.

Template parameter ``T``:
    The target type (must be derived from Object)

Returns:
    A pointer to the object of type T if successful, nullptr otherwise)doc";

static const char *__doc_mitsuba_key_iterator_type = R"doc(Return the current property type)doc";

static const char *__doc_mitsuba_librender_nop =
R"doc(Dummy function which can be called to ensure that the librender shared
library is loaded)doc";

static const char *__doc_mitsuba_linear_rgb_rec =
R"doc(Evaluate the ITU-R Rec. BT.709 linear RGB color matching functions
given a wavelength in nanometers)doc";

static const char *__doc_mitsuba_logger = R"doc(Return the logger instance (this is a process-wide setting))doc";

static const char *__doc_mitsuba_lookup_ior = R"doc()doc";

static const char *__doc_mitsuba_lookup_ior_2 = R"doc()doc";

static const char *__doc_mitsuba_lookup_ior_3 = R"doc()doc";

static const char *__doc_mitsuba_luminance = R"doc()doc";

static const char *__doc_mitsuba_luminance_2 = R"doc()doc";

static const char *__doc_mitsuba_math_bisect =
R"doc(Bisect a floating point interval given a predicate function

This function takes an interval [``left``, ``right``] and a predicate
``pred`` as inputs. It assumes that ``pred(left)==true`` and
``pred(right)==false``. It also assumes that there is a single
floating point number ``t`` such that ``pred`` is ``True`` for all
values in the range [``left``, ``t``] and ``False`` for all values in
the range (``t``, ``right``].

The bisection search then finds and returns ``t`` by repeatedly
splitting the input interval. The number of iterations is roughly
bounded by the number of bits of the underlying floating point
representation.)doc";

static const char *__doc_mitsuba_math_chi2 =
R"doc(Compute the Chi^2 statistic and degrees of freedom of the given arrays
while pooling low-valued entries together

Given a list of observations counts (``obs[i]``) and expected
observation counts (``exp[i]``), this function accumulates the Chi^2
statistic, that is, ``(obs-exp)^2 / exp`` for each element ``0, ...,
n-1``.

Minimum expected cell frequency. The Chi^2 test statistic is not
useful when when the expected frequency in a cell is low (e.g. less
than 5), because normality assumptions break down in this case.
Therefore, the implementation will merge such low-frequency cells when
they fall below the threshold specified here. Specifically, low-valued
cells with ``exp[i] < pool_threshold`` are pooled into larger groups
that are above the threshold before their contents are added to the
Chi^2 statistic.

The function returns the statistic value, degrees of freedom, below-
threshold entries and resulting number of pooled regions.)doc";

static const char *__doc_mitsuba_math_find_interval =
R"doc(Find an interval in an ordered set

This function performs a binary search to find an index ``i`` such
that ``pred(i)`` is ``True`` and ``pred(i+1)`` is ``False``, where
``pred`` is a user-specified predicate that monotonically decreases
over this range (i.e. max one ``True`` -> ``False`` transition).

The predicate will be evaluated exactly <tt>floor(log2(size)) + 1<tt>
times. Note that the template parameter ``Index`` is automatically
inferred from the supplied predicate, which takes an index or an index
vector of type ``Index`` as input argument and can (optionally) take a
mask argument as well. In the vectorized case, each vector lane can
use different predicate. When ``pred`` is ``False`` for all entries,
the function returns ``0``, and when it is ``True`` for all cases, it
returns <tt>size-2<tt>.

The main use case of this function is to locate an interval (i, i+1)
in an ordered list.

```
float my_list[] = { 1, 1.5f, 4.f, ... };

UInt32 index = find_interval(
    sizeof(my_list) / sizeof(float),
    [](UInt32 index, dr::mask_t<UInt32> active) {
        return dr::gather<Float>(my_list, index, active) < x;
    }
);
```)doc";

static const char *__doc_mitsuba_math_is_power_of_two = R"doc(Check whether the provided integer is a power of two)doc";

static const char *__doc_mitsuba_math_legendre_p = R"doc(Evaluate the l-th Legendre polynomial using recurrence)doc";

static const char *__doc_mitsuba_math_legendre_p_2 = R"doc(Evaluate an associated Legendre polynomial using recurrence)doc";

static const char *__doc_mitsuba_math_legendre_pd =
R"doc(Evaluate the l-th Legendre polynomial and its derivative using
recurrence)doc";

static const char *__doc_mitsuba_math_legendre_pd_diff = R"doc(Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x))doc";

static const char *__doc_mitsuba_math_log2i_ceil = R"doc(Ceiling of base-2 logarithm)doc";

static const char *__doc_mitsuba_math_middle =
R"doc(This function computes a suitable middle point for use in the bisect()
function

To mitigate the issue of varying density of floating point numbers on
the number line, the floats are reinterpreted as unsigned integers. As
long as sign of both numbers is the same, this maps the floats to the
evenly spaced set of integers. The middle of these integers ensures
that the space of numbers is halved on each iteration of the
bisection.

Note that this strategy does not work if the numbers have different
sign. In this case the function always returns 0.0 as the middle.)doc";

static const char *__doc_mitsuba_math_modulo = R"doc(Always-positive modulo function)doc";

static const char *__doc_mitsuba_math_round_to_power_of_two = R"doc(Round an unsigned integer to the next integer power of two)doc";

static const char *__doc_mitsuba_math_sample_shifted = R"doc(//! @})doc";

static const char *__doc_mitsuba_math_sample_shifted_2 =
R"doc(Map a uniformly distributed sample to an array of samples with shifts

Given a floating point value ``x`` on the interval ``[0, 1]`` return a
floating point array with values ``[x, x+offset, x+2*offset, ...]``,
where ``offset`` is the reciprocal of the array size. Entries that
become greater than 1.0 wrap around to the other side of the unit
interval.

This operation is useful to implement a type of correlated
stratification in the context of Monte Carlo integration.)doc";

static const char *__doc_mitsuba_math_solve_quadratic =
R"doc(Solve a quadratic equation of the form a*x^2 + b*x + c = 0.

Returns:
    ``True`` if a solution could be found)doc";

static const char *__doc_mitsuba_math_ulpdiff =
R"doc(Compare the difference in ULPs between a reference value and another
given floating point number)doc";

static const char *__doc_mitsuba_mueller_absorber =
R"doc(Constructs the Mueller matrix of an ideal absorber

Parameter ``value``:
    The amount of absorption.)doc";

static const char *__doc_mitsuba_mueller_depolarizer =
R"doc(Constructs the Mueller matrix of an ideal depolarizer

Parameter ``value``:
    The value of the (0, 0) element)doc";

static const char *__doc_mitsuba_mueller_diattenuator =
R"doc(Constructs the Mueller matrix of a linear diattenuator, which
attenuates the electric field components at 0 and 90 degrees by 'x'
and 'y', * respectively.)doc";

static const char *__doc_mitsuba_mueller_left_circular_polarizer =
R"doc(Constructs the Mueller matrix of a (left) circular polarizer.

"Polarized Light and Optical Systems" by Chipman et al. Table 6.2)doc";

static const char *__doc_mitsuba_mueller_linear_polarizer =
R"doc(Constructs the Mueller matrix of a linear polarizer which transmits
linear polarization at 0 degrees.

"Polarized Light" by Edward Collett, Ch. 5 eq. (13)

Parameter ``value``:
    The amount of attenuation of the transmitted component (1
    corresponds to an ideal polarizer).)doc";

static const char *__doc_mitsuba_mueller_linear_retarder =
R"doc(Constructs the Mueller matrix of a linear retarder which has its fast
axis aligned horizontally.

This implements the general case with arbitrary phase shift and can be
used to construct the common special cases of quarter-wave and half-
wave plates.

"Polarized Light, Third Edition" by Dennis H. Goldstein, Ch. 6 eq.
(6.43) (Note that the fast and slow axis were flipped in the first
edition by Edward Collett.)

Parameter ``phase``:
    The phase difference between the fast and slow axis)doc";

static const char *__doc_mitsuba_mueller_right_circular_polarizer =
R"doc(Constructs the Mueller matrix of a (right) circular polarizer.

"Polarized Light and Optical Systems" by Chipman et al. Table 6.2)doc";

static const char *__doc_mitsuba_mueller_rotate_mueller_basis =
R"doc(Return the Mueller matrix for some new reference frames. This version
rotates the input/output frames independently.

This operation is often used in polarized light transport when we have
a known Mueller matrix 'M' that operates from 'in_basis_current' to
'out_basis_current' but instead want to re-express it as a Mueller
matrix that operates from 'in_basis_target' to 'out_basis_target'.

Parameter ``M``:
    The current Mueller matrix that operates from ``in_basis_current``
    to ``out_basis_current``.

Parameter ``in_forward``:
    Direction of travel for input Stokes vector (normalized)

Parameter ``in_basis_current``:
    Current (normalized) input Stokes basis. Must be orthogonal to
    ``in_forward``.

Parameter ``in_basis_target``:
    Target (normalized) input Stokes basis. Must be orthogonal to
    ``in_forward``.

Parameter ``out_forward``:
    Direction of travel for input Stokes vector (normalized)

Parameter ``out_basis_current``:
    Current (normalized) output Stokes basis. Must be orthogonal to
    ``out_forward``.

Parameter ``out_basis_target``:
    Target (normalized) output Stokes basis. Must be orthogonal to
    ``out_forward``.

Returns:
    New Mueller matrix that operates from ``in_basis_target`` to
    ``out_basis_target``.)doc";

static const char *__doc_mitsuba_mueller_rotate_mueller_basis_collinear =
R"doc(Return the Mueller matrix for some new reference frames. This version
applies the same rotation to the input/output frames.

This operation is often used in polarized light transport when we have
a known Mueller matrix 'M' that operates from 'basis_current' to
'basis_current' but instead want to re-express it as a Mueller matrix
that operates from 'basis_target' to 'basis_target'.

Parameter ``M``:
    The current Mueller matrix that operates from ``basis_current`` to
    ``basis_current``.

Parameter ``forward``:
    Direction of travel for input Stokes vector (normalized)

Parameter ``basis_current``:
    Current (normalized) input Stokes basis. Must be orthogonal to
    ``forward``.

Parameter ``basis_target``:
    Target (normalized) input Stokes basis. Must be orthogonal to
    ``forward``.

Returns:
    New Mueller matrix that operates from ``basis_target`` to
    ``basis_target``.)doc";

static const char *__doc_mitsuba_mueller_rotate_stokes_basis =
R"doc(Gives the Mueller matrix that aligns the reference frames (defined by
their respective basis vectors) of two collinear stokes vectors.

If we have a stokes vector s_current expressed in 'basis_current', we
can re-interpret it as a stokes vector rotate_stokes_basis(..) * s1
that is expressed in 'basis_target' instead. For example: Horizontally
polarized light [1,1,0,0] in a basis [1,0,0] can be interpreted as
+45˚ linear polarized light [1,0,1,0] by switching to a target basis
[0.707, -0.707, 0].

Parameter ``forward``:
    Direction of travel for Stokes vector (normalized)

Parameter ``basis_current``:
    Current (normalized) Stokes basis. Must be orthogonal to
    ``forward``.

Parameter ``basis_target``:
    Target (normalized) Stokes basis. Must be orthogonal to
    ``forward``.

Returns:
    Mueller matrix that performs the desired change of reference
    frames.)doc";

static const char *__doc_mitsuba_mueller_rotated_element =
R"doc(Applies a counter-clockwise rotation to the mueller matrix of a given
element.)doc";

static const char *__doc_mitsuba_mueller_rotator =
R"doc(Constructs the Mueller matrix of an ideal rotator, which performs a
counter-clockwise rotation of the electric field by 'theta' radians
(when facing the light beam from the sensor side).

To be more precise, it rotates the reference frame of the current
Stokes vector. For example: horizontally linear polarized light s1 =
[1,1,0,0] will look like -45˚ linear polarized light s2 = R(45˚) * s1
= [1,0,-1,0] after applying a rotator of +45˚ to it.

"Polarized Light" by Edward Collett, Ch. 5 eq. (43))doc";

static const char *__doc_mitsuba_mueller_specular_reflection =
R"doc(Calculates the Mueller matrix of a specular reflection at an interface
between two dielectrics or conductors.

Parameter ``cos_theta_i``:
    Cosine of the angle between the surface normal and the incident
    ray

Parameter ``eta``:
    Complex-valued relative refractive index of the interface. In the
    real case, a value greater than 1.0 case means that the surface
    normal points into the region of lower density.)doc";

static const char *__doc_mitsuba_mueller_specular_transmission =
R"doc(Calculates the Mueller matrix of a specular transmission at an
interface between two dielectrics or conductors.

Parameter ``cos_theta_i``:
    Cosine of the angle between the surface normal and the incident
    ray

Parameter ``eta``:
    Complex-valued relative refractive index of the interface. A value
    greater than 1.0 in the real case means that the surface normal is
    pointing into the region of lower density.)doc";

static const char *__doc_mitsuba_mueller_stokes_basis =
R"doc(Gives the reference frame basis for a Stokes vector.

For light transport involving polarized quantities it is essential to
keep track of reference frames. A Stokes vector is only meaningful if
we also know w.r.t. which basis this state of light is observed. In
Mitsuba, these reference frames are never explicitly stored but
instead can be computed on the fly using this function.

Parameter ``forward``:
    Direction of travel for Stokes vector (normalized)

Returns:
    The (implicitly defined) reference coordinate system basis for the
    Stokes vector traveling along forward.)doc";

static const char *__doc_mitsuba_object_type_name = R"doc(Turn an ObjectType enumeration value into string form)doc";

static const char *__doc_mitsuba_operator_add = R"doc()doc";

static const char *__doc_mitsuba_operator_add_2 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_3 = R"doc(Adding a vector to a point should always yield a point)doc";

static const char *__doc_mitsuba_operator_add_4 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_5 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_6 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_7 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_8 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_9 = R"doc()doc";

static const char *__doc_mitsuba_operator_add_10 = R"doc()doc";

static const char *__doc_mitsuba_operator_band = R"doc()doc";

static const char *__doc_mitsuba_operator_band_2 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_3 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_4 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_5 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_6 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_7 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_8 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_9 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_10 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_11 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_12 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_13 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_14 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_15 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_16 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_17 = R"doc()doc";

static const char *__doc_mitsuba_operator_band_18 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_2 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_3 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_4 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_5 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_6 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_7 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_8 = R"doc()doc";

static const char *__doc_mitsuba_operator_bnot_9 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_2 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_3 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_4 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_5 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_6 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_7 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_8 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_9 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_10 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_11 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_12 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_13 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_14 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_15 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_16 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_17 = R"doc()doc";

static const char *__doc_mitsuba_operator_bor_18 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift = R"doc(Print a string representation of the bounding box)doc";

static const char *__doc_mitsuba_operator_lshift_2 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_3 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_4 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_5 = R"doc(Print a string representation of the bounding sphere)doc";

static const char *__doc_mitsuba_operator_lshift_6 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_7 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_8 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_9 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_10 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_11 = R"doc(Prints the canonical string representation of a field)doc";

static const char *__doc_mitsuba_operator_lshift_12 = R"doc(Return a string representation of a frame)doc";

static const char *__doc_mitsuba_operator_lshift_13 = R"doc(Prints the canonical string representation of an object instance)doc";

static const char *__doc_mitsuba_operator_lshift_14 = R"doc(Prints the canonical string representation of an object instance)doc";

static const char *__doc_mitsuba_operator_lshift_15 = R"doc(Return a string representation of the ray)doc";

static const char *__doc_mitsuba_operator_lshift_16 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_17 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_18 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_19 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_20 = R"doc(//! @{ \name Misc implementations)doc";

static const char *__doc_mitsuba_operator_lshift_21 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_22 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_23 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_24 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_25 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_26 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_27 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_28 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_29 = R"doc(Return a string representation of SGGXPhaseFunction parameters)doc";

static const char *__doc_mitsuba_operator_lshift_30 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_31 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_32 = R"doc()doc";

static const char *__doc_mitsuba_operator_lshift_33 = R"doc(//! @{ \name Misc implementations)doc";

static const char *__doc_mitsuba_operator_sub = R"doc(Subtracting two points should always yield a vector)doc";

static const char *__doc_mitsuba_operator_sub_2 = R"doc(Subtracting a vector from a point should always yield a point)doc";

static const char *__doc_mitsuba_optix_initialize = R"doc()doc";

static const char *__doc_mitsuba_orthographic_projection =
R"doc(Helper function to create a orthographic projection transformation
matrix)doc";

static const char *__doc_mitsuba_pair_eq = R"doc()doc";

static const char *__doc_mitsuba_pair_eq_operator_call = R"doc()doc";

static const char *__doc_mitsuba_pair_hasher = R"doc()doc";

static const char *__doc_mitsuba_pair_hasher_operator_call = R"doc()doc";

static const char *__doc_mitsuba_parse_fov = R"doc(Helper function to parse the field of view field of a camera)doc";

static const char *__doc_mitsuba_parser_ParserConfig =
R"doc(Configuration options for the parser

This structure contains various options that control parser behavior,
such as how to handle unused parameters and other validation settings.)doc";

static const char *__doc_mitsuba_parser_ParserConfig_ParserConfig = R"doc(Constructor that takes variant name)doc";

static const char *__doc_mitsuba_parser_ParserConfig_max_include_depth = R"doc(Maximum include depth to prevent infinite recursion)doc";

static const char *__doc_mitsuba_parser_ParserConfig_merge_equivalent = R"doc(Enable merging of identical plugin instances (e.g., materials))doc";

static const char *__doc_mitsuba_parser_ParserConfig_merge_meshes = R"doc(Merge compatible meshes (same material) into single larger mesh)doc";

static const char *__doc_mitsuba_parser_ParserConfig_parallel = R"doc(Enable parallel instantiation for better performance)doc";

static const char *__doc_mitsuba_parser_ParserConfig_unused_parameters =
R"doc(How to handle unused "$key" -> "value" substitutions during parsing:
Error (default), Warn, or Debug)doc";

static const char *__doc_mitsuba_parser_ParserConfig_unused_properties =
R"doc(How to handle unused properties during instantiation: Error (default),
Warn, or Debug)doc";

static const char *__doc_mitsuba_parser_ParserConfig_variant = R"doc(Target variant for instantiation (e.g., "scalar_rgb", "cuda_spectral"))doc";

static const char *__doc_mitsuba_parser_ParserState = R"doc(Keeps track of common state while parsing an XML file or dictionary)doc";

static const char *__doc_mitsuba_parser_ParserState_content =
R"doc(When parsing a file via parse_string(), this references the string
contents. Used to compute line information for error messages.)doc";

static const char *__doc_mitsuba_parser_ParserState_depth = R"doc(Current include depth (for preventing infinite recursion))doc";

static const char *__doc_mitsuba_parser_ParserState_empty = R"doc()doc";

static const char *__doc_mitsuba_parser_ParserState_files =
R"doc(List of files that were parsed while loading for error reporting.
Indexed by SceneNode.file_index, unused in the dictionary parser)doc";

static const char *__doc_mitsuba_parser_ParserState_id_to_index =
R"doc(Maps named nodes with an ``id`` attribute to their index in ``nodes``
Allows efficient lookup of objects for reference resolution)doc";

static const char *__doc_mitsuba_parser_ParserState_node_paths =
R"doc(Node paths (e.g., "scene.myshape.mybsdf") parallel to ``nodes``. Only
used in the dictionary parser, specifically for error reporting.)doc";

static const char *__doc_mitsuba_parser_ParserState_nodes =
R"doc(The list of all scene nodes. The root node is at position 0. Nodes are
added during parsing and never removed, only modified)doc";

static const char *__doc_mitsuba_parser_ParserState_operator_array = R"doc(Convenience operator for accessing nodes (const version))doc";

static const char *__doc_mitsuba_parser_ParserState_operator_array_2 = R"doc(Convenience operator for accessing nodes)doc";

static const char *__doc_mitsuba_parser_ParserState_operator_eq = R"doc(Equality comparison for ParserState)doc";

static const char *__doc_mitsuba_parser_ParserState_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_parser_ParserState_root = R"doc(Return the root node)doc";

static const char *__doc_mitsuba_parser_ParserState_root_2 = R"doc()doc";

static const char *__doc_mitsuba_parser_ParserState_size = R"doc()doc";

static const char *__doc_mitsuba_parser_ParserState_versions =
R"doc(Version number of each parsed file in files. Used by
transform_upgrade() to apply appropriate upgrades per file)doc";

static const char *__doc_mitsuba_parser_SceneNode =
R"doc(Intermediate scene object representation

This class stores information needed to instantiate a Mitsuba object
at some future point in time, including its plugin name and any
parameters to be supplied. The parse_string and parse_file functions
below turn an XML file or string into a sequence of SceneNode
instances that may undergo further transformation before finally being
instantiated.)doc";

static const char *__doc_mitsuba_parser_SceneNode_file_index =
R"doc(File index, identifies an entry of ParserState::files Used for error
reporting to show which file contains this node (unused in the dict
parser))doc";

static const char *__doc_mitsuba_parser_SceneNode_offset =
R"doc(Byte offset in the file where this node is found in the XML file Used
for precise error reporting with line/column information (unused in
the dict parser))doc";

static const char *__doc_mitsuba_parser_SceneNode_operator_eq = R"doc(Equality comparison for SceneNode)doc";

static const char *__doc_mitsuba_parser_SceneNode_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_parser_SceneNode_props =
R"doc(Stores the plugin attributes, sub-objects, ID, and plugin name.
References are also kept here. They are initially unresolved and later
resolved into concrete indices into ``ParserState.nodes``.)doc";

static const char *__doc_mitsuba_parser_SceneNode_type =
R"doc(Object type of this node (if known) Used for validation and type-
specific transformations (unused in the dict parser))doc";

static const char *__doc_mitsuba_parser_file_location =
R"doc(Generate a human-readable file location string for error reporting

Returns a string in the format "filename.xml:line:col" associated with
a given SceneNode. In the case of the dictionary parser, it returns a
period-separated string identifying the path to the object.

Parameter ``state``:
    Parser state containing file information

Parameter ``node``:
    Scene node to locate

Returns:
    Human-readable location string)doc";

static const char *__doc_mitsuba_parser_instantiate =
R"doc(Instantiate the parsed representation into concrete Mitsuba objects

This final stage creates the actual scene objects from the
intermediate representation. It handles:

- Plugin instantiation via the PluginManager - Dependency ordering for
correct instantiation order - Parallel instantiation of independent
objects (if enabled via ``config.parallel``) - Property validation and
type checking - Object expansion (Object::expand())

This function creates plugins with variant ParserConfig::variant,
using parallelism if requested (ParserConfig::parallel). It will
usually return a single object but may also produce multiple return
values if the top-level object expands into sub-objects.

Parameter ``config``:
    Parser configuration options including target variant and parallel
    flag

Parameter ``state``:
    Parser state containing the scene graph

Returns:
    Expanded top-level object)doc";

static const char *__doc_mitsuba_parser_parse_file =
R"doc(Parse a scene from an XML file and return the resulting parser state

This function loads an XML file and converts it to the intermediate
representation. It handles

- File includes via <include> tags - Parameter substitution using the
provided parameter list - Basic structural validation - Source
location tracking for error reporting

It does no further interpretation/instantiation.

This function is variant-independent.

Parameter ``config``:
    Parser configuration options

Parameter ``filename``:
    Path to the XML file to load

Parameter ``params``:
    List of parameter substitutions to apply

Returns:
    Parser state containing the scene graph)doc";

static const char *__doc_mitsuba_parser_parse_string =
R"doc(Parse a scene from an XML string and return the resulting parser state

Similar to parse_file() but takes the XML content as a string. This
function is variant-independent.

Parameter ``config``:
    Parser configuration options

Parameter ``string``:
    XML content to parse

Parameter ``params``:
    List of parameter substitutions to apply

Returns:
    Parser state containing the scene graph)doc";

static const char *__doc_mitsuba_parser_transform_all =
R"doc(Apply all transformations in sequence

This convenience function applies all parser transformations to the
scene graph in the following order: 1. transform_upgrade() 2.
transform_resolve() 3. transform_merge_equivalent() (if
config.merge_equivalent is enabled) 4. transform_merge_meshes() (if
config.merge_meshes is enabled)

Parameter ``config``:
    Parser configuration containing variant and other settings

Parameter ``state``:
    Parser state to transform (modified in-place))doc";

static const char *__doc_mitsuba_parser_transform_merge_equivalent =
R"doc(Merge equivalent nodes to reduce memory usage and instantiation time

This transformation identifies nodes with identical properties and
merges them. All references to duplicate nodes are updated to point to
a single canonical instance.

This optimization is particularly effective for scenes with many
repeated elements (e.g., identical materials or textures referenced by
multiple shapes).

Note: Nodes containing Object or Any properties are never deduplicated
as their equality cannot be reliably determined. Additionally, emitter
and shape nodes are excluded from merging to preserve their distinct
identities.

Parameter ``config``:
    Parser configuration

Parameter ``state``:
    Parser state to optimize (modified in-place))doc";

static const char *__doc_mitsuba_parser_transform_merge_meshes =
R"doc(Adapt the scene description to merge geometry whenever possible

This transformation moves all top-level geometry (i.e., occurring
directly within the <scene>) into a a shape plugin of type ``merge``.

When instantiated, this ``merge`` shape - Collects compatible groups
of mesh objects (i.e., with identical BSDF, media, emitter, etc.) -
Merges them into single mesh instances to reduce memory usage -
Preserves non-mesh shapes and meshes with unique attributes

Parameter ``config``:
    Parser configuration (currently unused)

Parameter ``state``:
    Parser state to modify in-place)doc";

static const char *__doc_mitsuba_parser_transform_relocate =
R"doc(Relocate scene files to subfolders

This transformation identifies file paths in the scene description and
relocates them to organized subfolders within the output directory,
creating subdirectories as needed.

File organization: - Textures and emitter files → textures/ subfolder
- Shape files (meshes) → meshes/ subfolder - Spectrum files → spectra/
subfolder - Other files → assets/ subfolder

Note: This transformation is not included in transform_all() and must
be called explicitly if desired, typically before XML export.

Parameter ``config``:
    Parser configuration (currently unused)

Parameter ``state``:
    Parser state containing the scene (modified in-place)

Parameter ``output_directory``:
    Base directory where files should be relocated)doc";

static const char *__doc_mitsuba_parser_transform_reorder =
R"doc(Reorder immediate children of scene nodes for better readability

This transformation reorders the immediate children of scene nodes to
follow a logical grouping that improves XML readability. The ordering
is:

1. Defaults 2. Integrators 3. Sensors 4. Materials (BSDFs, textures,
spectra) 5. Emitters (including shapes with area lights) 6. Shapes 7.
Media/volumes 8. Other elements

Shapes containing area lights are categorized as emitters rather than
shapes, keeping light sources grouped together.

This transformation only affects the ordering of immediate children of
the scene node. It does not recurse into nested structures.

Note: This transformation is not included in transform_all() and must
be called explicitly if desired.

Parameter ``config``:
    Parser configuration (currently unused)

Parameter ``state``:
    Parser state containing the scene to reorder (modified in-place))doc";

static const char *__doc_mitsuba_parser_transform_resolve =
R"doc(Resolve named references and raise an error when detecting broken
links

This transformation converts all (named) Properties::Reference objects
into index-based Properties::ResolvedReference references.

This transformation is variant-independent.

Parameter ``config``:
    Parser configuration

Parameter ``state``:
    Parser state to modify in-place

Throws:
    std::runtime_error if a reference cannot be resolved)doc";

static const char *__doc_mitsuba_parser_transform_upgrade =
R"doc(Upgrade scene data to the latest version

This transformation updates older scene formats for compatibility with
the current Mitsuba version. It performs the following steps:

- Converting property names from camelCase to underscore_case (version
< 2.0) - Upgrading deprecated plugin names/parameters to newer
equivalents

Parameter ``config``:
    Parser configuration

Parameter ``state``:
    Parser state to modify in-place)doc";

static const char *__doc_mitsuba_parser_write_file =
R"doc(Write scene data back to XML file

This function converts the intermediate representation into an XML
format and writes it to disk. Useful for: - Converting between scene
formats (dict to XML) - Saving programmatically generated scenes -
Debugging the parser's intermediate representation

Parameter ``state``:
    Parser state containing the scene graph

Parameter ``filename``:
    Path where the XML file should be written

Parameter ``add_section_headers``:
    Whether to add XML comment headers that group scene elements by
    category (e.g., "Materials", "Emitters", "Shapes"). These section
    headers improve readability and are particularly useful when the
    scene has been reorganized using the transform_reorder() function,
    which groups related elements together)doc";

static const char *__doc_mitsuba_parser_write_string =
R"doc(Convert scene data to an XML string

Similar to write_file() but returns the XML content as a string
instead of writing to disk.

Parameter ``state``:
    Parser state containing the scene graph

Parameter ``add_section_headers``:
    Whether to add section header comments for organization

Returns:
    XML representation of the scene)doc";

static const char *__doc_mitsuba_pdf_rgb_spectrum =
R"doc(PDF for the sample_rgb_spectrum strategy. It is valid to call this
function for a single wavelength (Float), a set of wavelengths
(Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all cases,
the PDF is returned per wavelength.)doc";

static const char *__doc_mitsuba_permute =
R"doc(Generate pseudorandom permutation vector using a shuffling network

This algorithm repeatedly invokes sample_tea_32() internally and has
O(log2(sample_count)) complexity. It only supports permutation
vectors, whose lengths are a power of 2.

Parameter ``index``:
    Input index to be permuted

Parameter ``size``:
    Length of the permutation vector

Parameter ``seed``:
    Seed value used as second input to the Tiny Encryption Algorithm.
    Can be used to generate different permutation vectors.

Parameter ``rounds``:
    How many rounds should be executed by the Tiny Encryption
    Algorithm? The default is 2.

Returns:
    The index corresponding to the input index in the pseudorandom
    permutation vector.)doc";

static const char *__doc_mitsuba_permute_kensler =
R"doc(Generate pseudorandom permutation vector using the algorithm described
in Pixar's technical memo "Correlated Multi-Jittered Sampling":

https://graphics.pixar.com/library/MultiJitteredSampling/

Unlike permute, this function supports permutation vectors of any
length.

Parameter ``index``:
    Input index to be mapped

Parameter ``sample_count``:
    Length of the permutation vector

Parameter ``seed``:
    Seed value used as second input to the Tiny Encryption Algorithm.
    Can be used to generate different permutation vectors.

Returns:
    The index corresponding to the input index in the pseudorandom
    permutation vector.)doc";

static const char *__doc_mitsuba_perspective_projection =
R"doc(Helper function to create a perspective projection transformation
matrix)doc";

static const char *__doc_mitsuba_plugin_type_name = R"doc(Get the XML tag name for an ObjectType (e.g. "scene", "bsdf"))doc";

static const char *__doc_mitsuba_prepare_ias =
R"doc(Prepares and fills the OptixInstance array associated with a given
list of shapes.)doc";

static const char *__doc_mitsuba_property_type_name = R"doc(Turn a Properties::Type enumeration value into string form)doc";

static const char *__doc_mitsuba_quad_chebyshev =
R"doc(Computes the Chebyshev nodes, i.e. the roots of the Chebyshev
polynomials of the first kind

The output array contains positions on the interval :math:`[-1, 1]`.

Parameter ``n``:
    Desired number of points)doc";

static const char *__doc_mitsuba_quad_composite_simpson =
R"doc(Computes the nodes and weights of a composite Simpson quadrature rule
with the given number of evaluations.

Integration is over the interval :math:`[-1, 1]`, which will be split
into :math:`(n-1) / 2` sub-intervals with overlapping endpoints. A
3-point Simpson rule is applied per interval, which is exact for
polynomials of degree three or less.

Parameter ``n``:
    Desired number of evaluation points. Must be an odd number bigger
    than 3.

Returns:
    A tuple (nodes, weights) storing the nodes and weights of the
    quadrature rule.)doc";

static const char *__doc_mitsuba_quad_composite_simpson_38 =
R"doc(Computes the nodes and weights of a composite Simpson 3/8 quadrature
rule with the given number of evaluations.

Integration is over the interval :math:`[-1, 1]`, which will be split
into :math:`(n-1) / 3` sub-intervals with overlapping endpoints. A
4-point Simpson rule is applied per interval, which is exact for
polynomials of degree four or less.

Parameter ``n``:
    Desired number of evaluation points. Must be an odd number bigger
    than 3.

Returns:
    A tuple (nodes, weights) storing the nodes and weights of the
    quadrature rule.)doc";

static const char *__doc_mitsuba_quad_gauss_legendre =
R"doc(Computes the nodes and weights of a Gauss-Legendre quadrature (aka
"Gaussian quadrature") rule with the given number of evaluations.

Integration is over the interval :math:`[-1, 1]`. Gauss-Legendre
quadrature maximizes the order of exactly integrable polynomials
achieves this up to degree :math:`2n-1` (where :math:`n` is the number
of function evaluations).

This method is numerically well-behaved until about :math:`n=200` and
then becomes progressively less accurate. It is generally not a good
idea to go much higher---in any case, a composite or adaptive
integration scheme will be superior for large :math:`n`.

Parameter ``n``:
    Desired number of evaluation points

Returns:
    A tuple (nodes, weights) storing the nodes and weights of the
    quadrature rule.)doc";

static const char *__doc_mitsuba_quad_gauss_lobatto =
R"doc(Computes the nodes and weights of a Gauss-Lobatto quadrature rule with
the given number of evaluations.

Integration is over the interval :math:`[-1, 1]`. Gauss-Lobatto
quadrature is preferable to Gauss-Legendre quadrature whenever the
endpoints of the integration domain should explicitly be included. It
maximizes the order of exactly integrable polynomials subject to this
constraint and achieves this up to degree :math:`2n-3` (where
:math:`n` is the number of function evaluations).

This method is numerically well-behaved until about :math:`n=200` and
then becomes progressively less accurate. It is generally not a good
idea to go much higher---in any case, a composite or adaptive
integration scheme will be superior for large :math:`n`.

Parameter ``n``:
    Desired number of evaluation points

Returns:
    A tuple (nodes, weights) storing the nodes and weights of the
    quadrature rule.)doc";

static const char *__doc_mitsuba_radical_inverse_2 = R"doc(Van der Corput radical inverse in base 2)doc";

static const char *__doc_mitsuba_reflect = R"doc(Reflection in local coordinates)doc";

static const char *__doc_mitsuba_reflect_2 = R"doc(Reflect ``wi`` with respect to a given surface normal)doc";

static const char *__doc_mitsuba_refract =
R"doc(Refraction in local coordinates

The 'cos_theta_t' and 'eta_ti' parameters are given by the last two
tuple entries returned by the fresnel and fresnel_polarized functions.)doc";

static const char *__doc_mitsuba_refract_2 =
R"doc(Refract ``wi`` with respect to a given surface normal

Parameter ``wi``:
    Direction to refract

Parameter ``m``:
    Surface normal

Parameter ``cos_theta_t``:
    Cosine of the angle between the normal the transmitted ray, as
    computed e.g. by fresnel.

Parameter ``eta_ti``:
    Relative index of refraction (transmitted / incident))doc";

static const char *__doc_mitsuba_sample_rgb_spectrum =
R"doc(Importance sample a "importance spectrum" that concentrates the
computation on wavelengths that are relevant for rendering of RGB data

Based on "An Improved Technique for Full Spectral Rendering" by
Radziszewski, Boryczko, and Alda

Returns a tuple with the sampled wavelength and inverse PDF)doc";

static const char *__doc_mitsuba_sample_tea_32 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

For details, refer to "GPU Random Numbers via the Tiny Encryption
Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

Parameter ``v0``:
    First input value to be encrypted (could be the sample index)

Parameter ``v1``:
    Second input value to be encrypted (e.g. the requested random
    number dimension)

Parameter ``rounds``:
    How many rounds should be executed? The default for random number
    generation is 4.

Returns:
    Two uniformly distributed 32-bit integers)doc";

static const char *__doc_mitsuba_sample_tea_64 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

For details, refer to "GPU Random Numbers via the Tiny Encryption
Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

Parameter ``v0``:
    First input value to be encrypted (could be the sample index)

Parameter ``v1``:
    Second input value to be encrypted (e.g. the requested random
    number dimension)

Parameter ``rounds``:
    How many rounds should be executed? The default for random number
    generation is 4.

Returns:
    A uniformly distributed 64-bit integer)doc";

static const char *__doc_mitsuba_sample_tea_float =
R"doc(Alias to sample_tea_float32 or sample_tea_float64 based given type
size)doc";

static const char *__doc_mitsuba_sample_tea_float32 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

This function uses sample_tea to return single precision floating
point numbers on the interval ``[0, 1)``

Parameter ``v0``:
    First input value to be encrypted (could be the sample index)

Parameter ``v1``:
    Second input value to be encrypted (e.g. the requested random
    number dimension)

Parameter ``rounds``:
    How many rounds should be executed? The default for random number
    generation is 4.

Returns:
    A uniformly distributed floating point number on the interval
    ``[0, 1)``)doc";

static const char *__doc_mitsuba_sample_tea_float64 =
R"doc(Generate fast and reasonably good pseudorandom numbers using the Tiny
Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

This function uses sample_tea to return double precision floating
point numbers on the interval ``[0, 1)``

Parameter ``v0``:
    First input value to be encrypted (could be the sample index)

Parameter ``v1``:
    Second input value to be encrypted (e.g. the requested random
    number dimension)

Parameter ``rounds``:
    How many rounds should be executed? The default for random number
    generation is 4.

Returns:
    A uniformly distributed floating point number on the interval
    ``[0, 1)``)doc";

static const char *__doc_mitsuba_sample_wavelength =
R"doc(Helper function to sample a wavelength (and a weight) given a random
number)doc";

static const char *__doc_mitsuba_scoped_optix_context =
R"doc(RAII wrapper which sets the CUDA context associated to the OptiX
context for the current scope.)doc";

static const char *__doc_mitsuba_scoped_optix_context_scoped_optix_context = R"doc()doc";

static const char *__doc_mitsuba_set_file_resolver = R"doc(Set the global file resolver instance (this is a process-wide setting))doc";

static const char *__doc_mitsuba_set_impl = R"doc()doc";

static const char *__doc_mitsuba_set_impl_2 = R"doc()doc";

static const char *__doc_mitsuba_set_logger = R"doc(Set the logger instance (this is a process-wide setting))doc";

static const char *__doc_mitsuba_sggx_pdf =
R"doc(Evaluates the probability of sampling a given normal using the SGGX
microflake distribution

Parameter ``wm``:
    The microflake normal

Parameter ``s``:
    The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy,
    S_xz, and S_yz that describe the entries of a symmetric positive
    definite 3x3 matrix. The user needs to ensure that the parameters
    indeed represent a positive definite matrix.

Returns:
    The probability of sampling a certain normal)doc";

static const char *__doc_mitsuba_sggx_projected_area =
R"doc(Evaluates the projected area of the SGGX microflake distribution

Parameter ``wi``:
    A 3D direction

Parameter ``s``:
    The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy,
    S_xz, and S_yz that describe the entries of a symmetric positive
    definite 3x3 matrix. The user needs to ensure that the parameters
    indeed represent a positive definite matrix.

Returns:
    The projected area of the SGGX microflake distribution)doc";

static const char *__doc_mitsuba_sggx_sample =
R"doc(Samples the visible normal distribution of the SGGX microflake
distribution

This function is based on the paper

"The SGGX microflake distribution", Siggraph 2015 by Eric Heitz,
Jonathan Dupuy, Cyril Crassin and Carsten Dachsbacher

Parameter ``sh_frame``:
    Shading frame aligned with the incident direction, e.g.
    constructed as Frame3f(wi)

Parameter ``sample``:
    A uniformly distributed 2D sample

Parameter ``s``:
    The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy,
    S_xz, and S_yz that describe the entries of a symmetric positive
    definite 3x3 matrix. The user needs to ensure that the parameters
    indeed represent a positive definite matrix.

Returns:
    A normal (in world space) sampled from the distribution of visible
    normals)doc";

static const char *__doc_mitsuba_sggx_sample_2 = R"doc()doc";

static const char *__doc_mitsuba_sobol_2 = R"doc(Sobol' radical inverse in base 2)doc";

static const char *__doc_mitsuba_spectrum_from_file =
R"doc(Read a spectral power distribution from an ASCII file.

The data should be arranged as follows: The file should contain a
single measurement per line, with the corresponding wavelength in
nanometers and the measured value separated by a space. Comments are
allowed.

Parameter ``path``:
    Path of the file to be read

Parameter ``wavelengths``:
    Array that will be loaded with the wavelengths stored in the file

Parameter ``values``:
    Array that will be loaded with the values stored in the file)doc";

static const char *__doc_mitsuba_spectrum_list_to_srgb =
R"doc(Transform a spectrum into a set of equivalent sRGB coefficients

When ``bounded`` is set, the resulting sRGB coefficients will be at
most 1.0. In any case, sRGB coefficients will be clamped to 0 if they
are negative.

Parameter ``wavelengths``:
    Array with the wavelengths of the spectrum

Parameter ``values``:
    Array with the values at the previously specified wavelengths

Parameter ``bounded``:
    Boolean that controls if clamping is required. (default: True)

Parameter ``d65``:
    Should the D65 illuminant be included in the integration.
    (default: False))doc";

static const char *__doc_mitsuba_spectrum_to_file =
R"doc(Write a spectral power distribution to an ASCII file.

The format is identical to that parsed by spectrum_from_file().

Parameter ``path``:
    Path to the file to be written to

Parameter ``wavelengths``:
    Array with the wavelengths to be stored in the file

Parameter ``values``:
    Array with the values to be stored in the file)doc";

static const char *__doc_mitsuba_spectrum_to_srgb =
R"doc(Spectral responses to sRGB normalized according to the CIE curves to
ensure that a unit-valued spectrum integrates to a luminance of 1.0.)doc";

static const char *__doc_mitsuba_spectrum_to_xyz =
R"doc(Spectral responses to XYZ normalized according to the CIE curves to
ensure that a unit-valued spectrum integrates to a luminance of 1.0.)doc";

static const char *__doc_mitsuba_sph_to_dir =
R"doc(Converts spherical coordinates to a cartesian vector

Parameter ``theta``:
    The polar angle

Parameter ``phi``:
    The azimuth angle

Returns:
    Unit vector corresponding to the input angles)doc";

static const char *__doc_mitsuba_spline_eval_1d =
R"doc(Evaluate a cubic spline interpolant of a *uniformly* sampled 1D
function

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment.

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``x``:
    Evaluation point

Remark:
    The Python API lacks the ``size`` parameter, which is inferred
    automatically from the size of the input array.

Remark:
    The Python API provides a vectorized version which evaluates the
    function for many arguments ``x``.

Returns:
    The interpolated value or zero when ``Extrapolate=false`` and
    ``x`` lies outside of [``min``, ``max``])doc";

static const char *__doc_mitsuba_spline_eval_1d_2 =
R"doc(Evaluate a cubic spline interpolant of a *non*-uniformly sampled 1D
function

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment.

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``size``:
    Denotes the size of the ``nodes`` and ``values`` array

Parameter ``x``:
    Evaluation point

Remark:
    The Python API lacks the ``size`` parameter, which is inferred
    automatically from the size of the input array

Remark:
    The Python API provides a vectorized version which evaluates the
    function for many arguments ``x``.

Returns:
    The interpolated value or zero when ``Extrapolate=false`` and
    ``x`` lies outside of \a [``min``, ``max``])doc";

static const char *__doc_mitsuba_spline_eval_2d =
R"doc(Evaluate a cubic spline interpolant of a uniformly sampled 2D function

This implementation relies on a tensor product of Catmull-Rom splines,
i.e. it uses finite differences to approximate the derivatives for
each dimension at the endpoints of spline patches.

Template parameter ``Extrapolate``:
    Extrapolate values when ``p`` is out of range? (default:
    ``False``)

Parameter ``nodes1``:
    Arrays containing ``size1`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated
    on the ``X`` axis (in increasing order)

Parameter ``size1``:
    Denotes the size of the ``nodes1`` array

Parameter ``nodes``:
    Arrays containing ``size2`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated
    on the ``Y`` axis (in increasing order)

Parameter ``size2``:
    Denotes the size of the ``nodes2`` array

Parameter ``values``:
    A 2D floating point array of ``size1*size2`` cells containing
    irregularly spaced evaluations of the function to be interpolated.
    Consecutive entries of this array correspond to increments in the
    ``X`` coordinate.

Parameter ``x``:
    ``X`` coordinate of the evaluation point

Parameter ``y``:
    ``Y`` coordinate of the evaluation point

Remark:
    The Python API lacks the ``size1`` and ``size2`` parameters, which
    are inferred automatically from the size of the input arrays.

Returns:
    The interpolated value or zero when ``Extrapolate=false``tt> and
    ``(x,y)`` lies outside of the node range)doc";

static const char *__doc_mitsuba_spline_eval_spline =
R"doc(Compute the definite integral and derivative of a cubic spline that is
parameterized by the function values and derivatives at the endpoints
of the interval ``[0, 1]``.

Parameter ``f0``:
    The function value at the left position

Parameter ``f1``:
    The function value at the right position

Parameter ``d0``:
    The function derivative at the left position

Parameter ``d1``:
    The function derivative at the right position

Parameter ``t``:
    The parameter variable

Returns:
    The interpolated function value at ``t``)doc";

static const char *__doc_mitsuba_spline_eval_spline_d =
R"doc(Compute the value and derivative of a cubic spline that is
parameterized by the function values and derivatives of the interval
``[0, 1]``.

Parameter ``f0``:
    The function value at the left position

Parameter ``f1``:
    The function value at the right position

Parameter ``d0``:
    The function derivative at the left position

Parameter ``d1``:
    The function derivative at the right position

Parameter ``t``:
    The parameter variable

Returns:
    The interpolated function value and its derivative at ``t``)doc";

static const char *__doc_mitsuba_spline_eval_spline_i =
R"doc(Compute the definite integral and value of a cubic spline that is
parameterized by the function values and derivatives of the interval
``[0, 1]``.

Parameter ``f0``:
    The function value at the left position

Parameter ``f1``:
    The function value at the right position

Parameter ``d0``:
    The function derivative at the left position

Parameter ``d1``:
    The function derivative at the right position

Returns:
    The definite integral and the interpolated function value at ``t``)doc";

static const char *__doc_mitsuba_spline_eval_spline_weights =
R"doc(Compute weights to perform a spline-interpolated lookup on a
*uniformly* sampled 1D function.

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment. The resulting weights are identical those internally
used by sample_1d().

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``size``:
    Denotes the number of function samples

Parameter ``x``:
    Evaluation point

Parameter ``weights``:
    Pointer to a weight array of size 4 that will be populated

Remark:
    In the Python API, the ``offset`` and ``weights`` parameters are
    returned as the second and third elements of a triple.

Returns:
    A boolean set to ``True`` on success and ``False`` when
    ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
    and an offset into the function samples associated with weights[0])doc";

static const char *__doc_mitsuba_spline_eval_spline_weights_2 =
R"doc(Compute weights to perform a spline-interpolated lookup on a
*non*-uniformly sampled 1D function.

The implementation relies on Catmull-Rom splines, i.e. it uses finite
differences to approximate the derivatives at the endpoints of each
spline segment. The resulting weights are identical those internally
used by sample_1d().

Template parameter ``Extrapolate``:
    Extrapolate values when ``x`` is out of range? (default:
    ``False``)

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``size``:
    Denotes the size of the ``nodes`` array

Parameter ``x``:
    Evaluation point

Parameter ``weights``:
    Pointer to a weight array of size 4 that will be populated

Remark:
    The Python API lacks the ``size`` parameter, which is inferred
    automatically from the size of the input array. The ``offset`` and
    ``weights`` parameters are returned as the second and third
    elements of a triple.

Returns:
    A boolean set to ``True`` on success and ``False`` when
    ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
    and an offset into the function samples associated with weights[0])doc";

static const char *__doc_mitsuba_spline_integrate_1d =
R"doc(Computes a prefix sum of integrals over segments of a *uniformly*
sampled 1D Catmull-Rom spline interpolant

This is useful for sampling spline segments as part of an importance
sampling scheme (in conjunction with sample_1d)

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``out``:
    An array with ``size`` entries, which will be used to store the
    prefix sum

Remark:
    The Python API lacks the ``size`` and ``out`` parameters. The
    former is inferred automatically from the size of the input array,
    and ``out`` is returned as a list.)doc";

static const char *__doc_mitsuba_spline_integrate_1d_2 =
R"doc(Computes a prefix sum of integrals over segments of a *non*-uniformly
sampled 1D Catmull-Rom spline interpolant

This is useful for sampling spline segments as part of an importance
sampling scheme (in conjunction with sample_1d)

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``out``:
    An array with ``size`` entries, which will be used to store the
    prefix sum

Remark:
    The Python API lacks the ``size`` and ``out`` parameters. The
    former is inferred automatically from the size of the input array,
    and ``out`` is returned as a list.)doc";

static const char *__doc_mitsuba_spline_invert_1d =
R"doc(Invert a cubic spline interpolant of a *uniformly* sampled 1D
function. The spline interpolant must be *monotonically increasing*.

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``y``:
    Input parameter for the inversion

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    The spline parameter ``t`` such that ``eval_1d(..., t)=y``)doc";

static const char *__doc_mitsuba_spline_invert_1d_2 =
R"doc(Invert a cubic spline interpolant of a *non*-uniformly sampled 1D
function. The spline interpolant must be *monotonically increasing*.

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``y``:
    Input parameter for the inversion

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    The spline parameter ``t`` such that ``eval_1d(..., t)=y``)doc";

static const char *__doc_mitsuba_spline_sample_1d =
R"doc(Importance sample a segment of a *uniformly* sampled 1D Catmull-Rom
spline interpolant

Parameter ``min``:
    Position of the first node

Parameter ``max``:
    Position of the last node

Parameter ``values``:
    Array containing ``size`` regularly spaced evaluations in the
    range [``min``, ``max``] of the approximated function.

Parameter ``cdf``:
    Array containing a cumulative distribution function computed by
    integrate_1d().

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``sample``:
    A uniformly distributed random sample in the interval ``[0,1]``

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    1. The sampled position 2. The value of the spline evaluated at
    the sampled position 3. The probability density at the sampled
    position (which only differs from item 2. when the function does
    not integrate to one))doc";

static const char *__doc_mitsuba_spline_sample_1d_2 =
R"doc(Importance sample a segment of a *non*-uniformly sampled 1D Catmull-
Rom spline interpolant

Parameter ``nodes``:
    Array containing ``size`` non-uniformly spaced values denoting
    positions the where the function to be interpolated was evaluated.
    They must be provided in *increasing* order.

Parameter ``values``:
    Array containing function evaluations matched to the entries of
    ``nodes``.

Parameter ``cdf``:
    Array containing a cumulative distribution function computed by
    integrate_1d().

Parameter ``size``:
    Denotes the size of the ``values`` array

Parameter ``sample``:
    A uniformly distributed random sample in the interval ``[0,1]``

Parameter ``eps``:
    Error tolerance (default: 1e-6f)

Returns:
    1. The sampled position 2. The value of the spline evaluated at
    the sampled position 3. The probability density at the sampled
    position (which only differs from item 2. when the function does
    not integrate to one))doc";

static const char *__doc_mitsuba_srgb_model_eval = R"doc()doc";

static const char *__doc_mitsuba_srgb_model_fetch =
R"doc(Look up the model coefficients for a sRGB color value

Parameter ``c``:
    An sRGB color value where all components are in [0, 1].

Returns:
    Coefficients for use with srgb_model_eval)doc";

static const char *__doc_mitsuba_srgb_model_mean = R"doc()doc";

static const char *__doc_mitsuba_srgb_to_xyz = R"doc(Convert ITU-R Rec. BT.709 linear RGB to XYZ tristimulus values)doc";

static const char *__doc_mitsuba_string_contains = R"doc(Check if a list of keys contains a specific key)doc";

static const char *__doc_mitsuba_string_ends_with = R"doc(Check if the given string ends with a specified suffix)doc";

static const char *__doc_mitsuba_string_iequals =
R"doc(Case-insensitive comparison of two string_views (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_indent = R"doc(Indent every line of a string by some number of spaces)doc";

static const char *__doc_mitsuba_string_indent_2 =
R"doc(Turn a type into a string representation and indent every line by some
number of spaces)doc";

static const char *__doc_mitsuba_string_indent_3 = R"doc()doc";

static const char *__doc_mitsuba_string_indent_4 = R"doc()doc";

static const char *__doc_mitsuba_string_parse_float =
R"doc(Locale-independent string to floating point conversion analogous to
std::stof. (implemented using Daniel Lemire's fast_float library.)

Parses a floating point number in a (potentially longer) string
start..end-1. The 'endptr' argument (if non-NULL) is used to return a
pointer to the character following the parsed floating point value.

Throws an exception if the conversion is unsuccessful.)doc";

static const char *__doc_mitsuba_string_replace_inplace = R"doc()doc";

static const char *__doc_mitsuba_string_starts_with = R"doc(Check if the given string starts with a specified prefix)doc";

static const char *__doc_mitsuba_string_stof =
R"doc(Locale-independent string to floating point conversion analogous to
std::stof. (implemented using Daniel Lemire's fast_float library.)

Throws an exception if the conversion is unsuccessful, or if the
portion of the string following the parsed number contains non-
whitespace characters.)doc";

static const char *__doc_mitsuba_string_strtof =
R"doc(Locale-independent string to floating point conversion analogous to
std::strtof. (implemented using Daniel Lemire's fast_float library.)

Throws an exception if the conversion is unsuccessful.)doc";

static const char *__doc_mitsuba_string_to_lower =
R"doc(Return a lower-case version of the given string (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_to_upper =
R"doc(Return a upper-case version of the given string (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_tokenize =
R"doc(Chop up the string given a set of delimiters (warning: not unicode
compliant))doc";

static const char *__doc_mitsuba_string_trim = R"doc(Remove leading and trailing characters)doc";

static const char *__doc_mitsuba_tuple_hasher = R"doc()doc";

static const char *__doc_mitsuba_tuple_hasher_operator_call = R"doc()doc";

static const char *__doc_mitsuba_type_suffix =
R"doc(Convenience function which computes an array size/type suffix (like
'2u' or '3fP'))doc";

static const char *__doc_mitsuba_unpolarized_spectrum =
R"doc(Return the (1,1) entry of a Mueller matrix. Identity function for all
other types.

This is useful for places in the renderer where we do not care about
the additional information tracked by the Mueller matrix. For instance
when performing Russian Roulette based on the path throughput or when
writing a final RGB pixel value to the image block.)doc";

static const char *__doc_mitsuba_util_Version = R"doc()doc";

static const char *__doc_mitsuba_util_Version_Version = R"doc()doc";

static const char *__doc_mitsuba_util_Version_Version_2 = R"doc()doc";

static const char *__doc_mitsuba_util_Version_Version_3 = R"doc()doc";

static const char *__doc_mitsuba_util_Version_major_version = R"doc()doc";

static const char *__doc_mitsuba_util_Version_minor_version = R"doc()doc";

static const char *__doc_mitsuba_util_Version_operator_eq = R"doc()doc";

static const char *__doc_mitsuba_util_Version_operator_ge = R"doc()doc";

static const char *__doc_mitsuba_util_Version_operator_gt = R"doc()doc";

static const char *__doc_mitsuba_util_Version_operator_le = R"doc()doc";

static const char *__doc_mitsuba_util_Version_operator_lt = R"doc()doc";

static const char *__doc_mitsuba_util_Version_operator_ne = R"doc()doc";

static const char *__doc_mitsuba_util_Version_patch_version = R"doc()doc";

static const char *__doc_mitsuba_util_Version_to_string = R"doc()doc";

static const char *__doc_mitsuba_util_core_count = R"doc(Determine the number of available CPU cores (including virtual cores))doc";

static const char *__doc_mitsuba_util_detect_debugger = R"doc(Returns 'true' if the application is running inside a debugger)doc";

static const char *__doc_mitsuba_util_info_build = R"doc(Return human-readable information about the Mitsuba build)doc";

static const char *__doc_mitsuba_util_info_copyright = R"doc(Return human-readable information about the version)doc";

static const char *__doc_mitsuba_util_info_features = R"doc(Return human-readable information about the enabled processor features)doc";

static const char *__doc_mitsuba_util_library_path = R"doc(Return the absolute path to <tt>libmitsuba-core.dylib/so/dll<tt>)doc";

static const char *__doc_mitsuba_util_mem_string = R"doc(Turn a memory size into a human-readable string)doc";

static const char *__doc_mitsuba_util_terminal_width = R"doc(Determine the width of the terminal window that is used to run Mitsuba)doc";

static const char *__doc_mitsuba_util_time_string =
R"doc(Convert a time difference (in seconds) to a string representation

Parameter ``time``:
    Time difference in (fractional) sections

Parameter ``precise``:
    When set to true, a higher-precision string representation is
    generated.)doc";

static const char *__doc_mitsuba_util_trap_debugger =
R"doc(Generate a trap instruction if running in a debugger; otherwise,
return.)doc";

static const char *__doc_mitsuba_warp_beckmann_to_square = R"doc(Inverse of the mapping square_to_uniform_cone)doc";

static const char *__doc_mitsuba_warp_bilinear_to_square = R"doc(Inverse of square_to_bilinear)doc";

static const char *__doc_mitsuba_warp_circ = R"doc(//! @{ \name Warping techniques that operate in the plane)doc";

static const char *__doc_mitsuba_warp_cosine_hemisphere_to_square = R"doc(Inverse of the mapping square_to_cosine_hemisphere)doc";

static const char *__doc_mitsuba_warp_detail_i0 = R"doc()doc";

static const char *__doc_mitsuba_warp_detail_log_i0 = R"doc()doc";

static const char *__doc_mitsuba_warp_interval_to_linear =
R"doc(Importance sample a linear interpolant

Given a linear interpolant on the unit interval with boundary values
``v0``, ``v1`` (where ``v1`` is the value at ``x=1``), warp a
uniformly distributed input sample ``sample`` so that the resulting
probability distribution matches the linear interpolant.)doc";

static const char *__doc_mitsuba_warp_interval_to_nonuniform_tent =
R"doc(Warp a uniformly distributed sample on [0, 1] to a nonuniform tent
distribution with nodes ``{a, b, c}``)doc";

static const char *__doc_mitsuba_warp_interval_to_tangent_direction =
R"doc(Warp a uniformly distributed sample on [0, 1] to a direction in the
tangent plane)doc";

static const char *__doc_mitsuba_warp_interval_to_tent = R"doc(Warp a uniformly distributed sample on [0, 1] to a tent distribution)doc";

static const char *__doc_mitsuba_warp_linear_to_interval = R"doc(Inverse of interval_to_linear)doc";

static const char *__doc_mitsuba_warp_square_to_beckmann = R"doc(Warp a uniformly distributed square sample to a Beckmann distribution)doc";

static const char *__doc_mitsuba_warp_square_to_beckmann_pdf = R"doc(Probability density of square_to_beckmann())doc";

static const char *__doc_mitsuba_warp_square_to_bilinear =
R"doc(Importance sample a bilinear interpolant

Given a bilinear interpolant on the unit square with corner values
``v00``, ``v10``, ``v01``, ``v11`` (where ``v10`` is the value at
(x,y) == (0, 0)), warp a uniformly distributed input sample ``sample``
so that the resulting probability distribution matches the linear
interpolant.

The implementation first samples the marginal distribution to obtain
``y``, followed by sampling the conditional distribution to obtain
``x``.

Returns the sampled point and PDF for convenience.)doc";

static const char *__doc_mitsuba_warp_square_to_bilinear_pdf = R"doc()doc";

static const char *__doc_mitsuba_warp_square_to_cosine_hemisphere =
R"doc(Sample a cosine-weighted vector on the unit hemisphere with respect to
solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_cosine_hemisphere_pdf = R"doc(Density of square_to_cosine_hemisphere() with respect to solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_rough_fiber =
R"doc(Warp a uniformly distributed square sample to a rough fiber
distribution)doc";

static const char *__doc_mitsuba_warp_square_to_rough_fiber_pdf = R"doc(Probability density of square_to_rough_fiber())doc";

static const char *__doc_mitsuba_warp_square_to_std_normal =
R"doc(Sample a point on a 2D standard normal distribution. Internally uses
the Box-Muller transformation)doc";

static const char *__doc_mitsuba_warp_square_to_std_normal_pdf = R"doc()doc";

static const char *__doc_mitsuba_warp_square_to_tent = R"doc(Warp a uniformly distributed square sample to a 2D tent distribution)doc";

static const char *__doc_mitsuba_warp_square_to_tent_pdf = R"doc(Density of square_to_tent per unit area.)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_cone =
R"doc(Uniformly sample a vector that lies within a given cone of angles
around the Z axis

Parameter ``cos_cutoff``:
    Cosine of the cutoff angle

Parameter ``sample``:
    A uniformly distributed sample on :math:`[0,1]^2`)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_cone_pdf =
R"doc(Density of square_to_uniform_cone per unit area.

Parameter ``cos_cutoff``:
    Cosine of the cutoff angle)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk = R"doc(Uniformly sample a vector on a 2D disk)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk_concentric = R"doc(Low-distortion concentric square to disk mapping by Peter Shirley)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk_concentric_pdf = R"doc(Density of square_to_uniform_disk per unit area)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_disk_pdf = R"doc(Density of square_to_uniform_disk per unit area)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_hemisphere =
R"doc(Uniformly sample a vector on the unit hemisphere with respect to solid
angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_hemisphere_pdf = R"doc(Density of square_to_uniform_hemisphere() with respect to solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_sphere =
R"doc(Uniformly sample a vector on the unit sphere with respect to solid
angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_sphere_pdf = R"doc(Density of square_to_uniform_sphere() with respect to solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_spherical_lune =
R"doc(Uniformly sample a direction in the two spherical lunes defined by the
valid boundary directions of two touching faces defined by their
normals ``n1`` and ``n2``.)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_spherical_lune_pdf = R"doc(Density of square_to_uniform_spherical_lune() w.r.t. solid angles)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_square_concentric =
R"doc(Low-distortion concentric square to square mapping (meant to be used
in conjunction with another warping method that maps to the sphere))doc";

static const char *__doc_mitsuba_warp_square_to_uniform_triangle =
R"doc(Convert an uniformly distributed square sample into barycentric
coordinates)doc";

static const char *__doc_mitsuba_warp_square_to_uniform_triangle_pdf = R"doc(Density of square_to_uniform_triangle per unit area.)doc";

static const char *__doc_mitsuba_warp_square_to_von_mises_fisher =
R"doc(Warp a uniformly distributed square sample to a von Mises Fisher
distribution)doc";

static const char *__doc_mitsuba_warp_square_to_von_mises_fisher_pdf = R"doc(Probability density of square_to_von_mises_fisher())doc";

static const char *__doc_mitsuba_warp_tangent_direction_to_interval = R"doc(Inverse of uniform_to_tangent_direction)doc";

static const char *__doc_mitsuba_warp_tent_to_interval = R"doc(Warp a tent distribution to a uniformly distributed sample on [0, 1])doc";

static const char *__doc_mitsuba_warp_tent_to_square = R"doc(Warp a uniformly distributed square sample to a 2D tent distribution)doc";

static const char *__doc_mitsuba_warp_uniform_cone_to_square = R"doc(Inverse of the mapping square_to_uniform_cone)doc";

static const char *__doc_mitsuba_warp_uniform_disk_to_square = R"doc(Inverse of the mapping square_to_uniform_disk)doc";

static const char *__doc_mitsuba_warp_uniform_disk_to_square_concentric = R"doc(Inverse of the mapping square_to_uniform_disk_concentric)doc";

static const char *__doc_mitsuba_warp_uniform_hemisphere_to_square = R"doc(Inverse of the mapping square_to_uniform_hemisphere)doc";

static const char *__doc_mitsuba_warp_uniform_sphere_to_square = R"doc(Inverse of the mapping square_to_uniform_sphere)doc";

static const char *__doc_mitsuba_warp_uniform_spherical_lune_to_square = R"doc(Inverse of the mapping square_to_uniform_spherical_lune)doc";

static const char *__doc_mitsuba_warp_uniform_triangle_to_square = R"doc(Inverse of the mapping square_to_uniform_triangle)doc";

static const char *__doc_mitsuba_warp_von_mises_fisher_to_square = R"doc(Inverse of the mapping von_mises_fisher_to_square)doc";

static const char *__doc_mitsuba_xyz_to_srgb = R"doc(Convert XYZ tristimulus values to ITU-R Rec. BT.709 linear RGB)doc";

static const char *__doc_operator_lshift = R"doc(Turns a vector of elements into a human-readable representation)doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

