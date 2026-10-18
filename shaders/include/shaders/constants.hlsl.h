#pragma once

#define SHADERS_DEBUG 1

#ifdef __cplusplus
namespace shader_constants
{
    using uint = unsigned int;
#endif
    static const uint kLodFlagBit              = 0;
    static const uint kFrustumCullBit          = 1;
    static const uint kOcclusionCullBit        = 2;
    static const uint kMeshletConeCullBit      = 3;
    static const uint kMeshletFrustumCullBit   = 4;
    static const uint kMeshletOcclusionCullBit = 5;
    static const uint kSmallMeshletsCullBit    = 6;

    static const uint kMatGlossBit        = 0;
    static const uint kMatTransmitanceBit = 1;

    static const uint kMatClassOpaque      = 0;
    static const uint kMatClassMasked      = 1;
    static const uint kMatClassTranslucent = 2;
    static const uint kMatClassCount       = 3;

    static const uint kEnvPrefilterMips = 5;

    static const uint kMaxVerticesPerMeshlet  = 64;
    static const uint kMaxTrianglesPerMeshlet = 94;
    static const uint kMaxIndicesPerMeshlet   = kMaxTrianglesPerMeshlet * 3;

    static const uint kLODCount = 8;

    static const uint kTaskWorkGroups = 64;
    static const uint kMeshWorkGroups = 64;

    static const uint kInstanceIdMask = ~(1u << 31);

    static const uint kDebugModeShaded         = 0;
    static const uint kDebugModeLit            = 1;
    static const uint kDebugModeLitDiffuse     = 2;
    static const uint kDebugModeLitAmbient     = 3;
    static const uint kDebugModeLitSpecular    = 4;
    static const uint kDebugModeUV             = 5;
    static const uint kDebugModeNormal         = 6;
    static const uint kDebugModeTangent        = 7;
    static const uint kDebugModeDepth          = 8;
    static const uint kDebugModeVelocity       = 9;
    static const uint kDebugModeWorldPos       = 10;
    static const uint kDebugModeColor          = 11;
    static const uint kDebugModeMetallic       = 12;
    static const uint kDebugModeRoughness      = 13;
    static const uint kDebugModeAlbedoTexture  = 14;
    static const uint kDebugModeNormalTexture  = 15;
    static const uint kDebugModeOrmTexture     = 16;
    static const uint kDebugModeTriangleId     = 17;
    static const uint kDebugModeInstanceId     = 18;
    static const uint kDebugModeTriangleFace   = 19;
    static const uint kDebugModeShadow         = 20;
    static const uint kDebugModeShadowCascades = 21;
#ifdef __cplusplus
}
#endif

#define GET_BIT(flags, bit) ((flags >> bit) & 1)

#if SHADERS_DEBUG
#define DBG_GET_BIT1(flags, bit) GET_BIT(flags, bit)
#define DBG_GET_BIT0(flags, bit) GET_BIT(flags, bit)
#else
#define DBG_GET_BIT1(flags, bit) 1
#define DBG_GET_BIT0(flags, bit) 0
#endif
