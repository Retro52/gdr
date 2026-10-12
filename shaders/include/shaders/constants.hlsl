#pragma once

#define SHADERS_DEBUG 1

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

static const uint kMaxShadowCascades      = 4;
static const uint kMaxVerticesPerMeshlet  = 64;
static const uint kMaxTrianglesPerMeshlet = 94;
static const uint kMaxIndicesPerMeshlet   = kMaxTrianglesPerMeshlet * 3;

static const uint kLODCount = 8;

static const uint kTaskWorkGroups = 64;
static const uint kMeshWorkGroups = 64;

static const uint kInstanceIdMask = ~(1u << 31);

#define GET_BIT(flags, bit) ((flags >> bit) & 1)

#if SHADERS_DEBUG
#define DBG_GET_BIT1(flags, bit) GET_BIT(flags, bit)
#define DBG_GET_BIT0(flags, bit) GET_BIT(flags, bit)
#else
#define DBG_GET_BIT1(flags, bit) 1
#define DBG_GET_BIT0(flags, bit) 0
#endif
