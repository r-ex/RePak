#pragma once

#include <d3d11.h>
#include <pch.h>

struct RGBA
{
	float r, g, b, a;

	RGBA(float r, float g, float b, float a) {
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
	}

	RGBA() {};
};

enum MatVisFlags : uint16_t
{
	None = 0x0,
	Colpass = 0x5,
	Transparent = 0x7,
	Opaque = 0x17
};

enum MatRenderFlags : uint16_t
{
	Default = 0x0,
	Wireframe = 0x1,
	Culling = 0x2,
	Inverted = 0x4,
	NoCulling = 0x6,
	Unknown = 0x8,
};

enum MaterialShaderType_t : unsigned __int8
{
	RGDU = 0x0,
	RGDP = 0x1,
	RGDC = 0x2,
	SKNU = 0x3,
	SKNP = 0x4,
	SKNC = 0x5,
	WLDU = 0x6,
	WLDC = 0x7,
	PTCU = 0x8,
	PTCS = 0x9,
};

#pragma pack(push, 2)
// header struct for the material asset cpu data
struct MaterialCPUHeader
{
	RPakPtr  m_nUnknownRPtr{}; // points to the rest of the cpu data. maybe for colour?
	uint32_t m_nDataSize = 0;
	uint32_t m_nVersionMaybe = 3; // every unknown is now either datasize, version, or flags
};

// the following two structs are found in the ""cpu data"", they are very much alike to what you would use in normal source materials.
// apex probably has these and more stuff.
struct MaterialTextureTransformMatrix
{
	// very similar to how it's done in normal source
	float TextureScaleX = 1.0;
	float TextureUnk = 0.0; // unsure what this does, appears to skew/rotate and scale the texture at the same time? weird.
	float TextureRotation = -0.0; // counter clockwise, 0-1, exceeding one causes Weird Stuff to happen.
	float TextureScaleY = 1.0;
	float TextureTranslateX = 0.0;
	float TextureTranslateY = 0.0;
};

// the following two structs are found in the ""cpu data"", they are very much alike to what you would use in normal source materials.
// apex probably has these and more stuff.
struct UVTransformMatrix
{
	Vector2 c_RotScaleX = { 1.000000, 0.000000 };
	Vector2 c_RotScaleY = { -0.000000, 1.000000 };
	Vector2 c_Translate = { 0.000000, 0.000000 };
};

struct UnknownMaterialSectionV12
{
	// not sure how these work but 0xF0 -> 0x00 toggles them off and vice versa.
	// they seem to affect various rendering filters, said filters might actually be the used shaders.
	// the duplicate one is likely for the second set of textures which (probably) never gets used.
	uint32_t UnkRenderLighting = 0xF0138286;
	uint32_t UnkRenderAliasing = 0xF0138286;
	uint32_t UnkRenderDoF = 0xF0008286;
	uint32_t UnkRenderUnknown = 0x00138286;

	uint32_t UnkRenderFlags = 0x00000005; // this changes sometimes.
	uint16_t VisibilityFlags = 0x0000; // different render settings, such as opacity and transparency.
	uint16_t FaceDrawingFlags = 0x0006; // how the face is drawn, culling, wireframe, etc.

	uint64_t Padding;

	/*VisibilityFlags
	0x0000 unknown
	0x0001 inverted ignorez
	0x0002 required when ignorez is enabled, why?
	0x0004 unknown but used in most opaque materials, not required.
	0x0008
	0x0010 seems to toggle transparency, will draw opaque if inverted ignorez is enabled
	0x0017 used for most normal materials.
	0x0007 used for glass which makes me think 0x0010 is for translucency.
	0x0013 is vaild and looks like a normal opaque material.  */

	/*FlagDrawingFlags Flags
	0x0000 culling this is the same as 0x0002??? maybe the default?
	0x0001 wireframe
	0x0002 normal texture drawing aka culling (front side and backside drawn).
	0x0004 inverted faces
	0x0008 if this exists I can't tell what it is.
	to get the equalivilent to 'nocull' both 'culling' and 'inverted faces' need to be enabled, see: why most matls have '0x06'.  */
};

// should be size of 208
struct MaterialHeaderV12
{
	uint64_t VtblPtrPad = 0; // Gets set to CMaterialGlue vtbl ptr
	uint64_t padding = 0; // Un-used.
	uint64_t AssetGUID = 0; // guid of this material asset

	RPakPtr m_pszName{}; // pointer to partial asset path
	RPakPtr m_pszSurfaceProp{}; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	RPakPtr m_pszSurfaceProp2{}; // pointer to surfaceprop2

	// IDX 1: DepthShadow
	// IDX 2: DepthPrepass
	// IDX 3: DepthVSM
	// IDX 4: ColPass
	// Titanfall is does not have 'DepthShadowTight'

	uint64_t GUIDRefs[4]{}; // Required to have proper textures.

	// these blocks dont seem to change often but are the same?
	// these blocks relate to different render filters and flags. still not well understood.
	UnknownMaterialSectionV12 UnkSections[2];

	uint64_t ShaderSetGUID = 0; // guid of the shaderset asset that this material uses

	RPakPtr TextureGUIDs{}; // TextureGUID Map
	RPakPtr TextureGUIDs2{}; // Streamable TextureGUID Map

	int16_t StreamableTextureCount = 0; // Number of textures with streamed mip levels.
	uint32_t ImageFlags = 0x503000; // see ImageFlags in the apex struct.
	int16_t Unk1 = 0; // might be "Unknown"

	uint64_t padding2 = 0; // haven't observed anything here.

	// seems to be 0xFBA63181 for loadscreens
	uint32_t Unknown2 = 0xFBA63181; // no clue tbh

	uint32_t Unk2 = 0; // this might actually be "Alignment"

	uint32_t Flags2 = 0;
	uint32_t something2 = 0x0; // seems mostly unchanged between all materials, including apex, however there are some edge cases where this is 0x00.

	int16_t Width = 2048;
	int16_t Height = 2048;

	uint32_t Unk3 = 0; // might be padding but could also be m_Flags2 else.

	/* ImageFlags
	0x050300 for loadscreens, 0x1D0300 for normal materials.
	0x1D has been observed, seems to invert lighting? used on some exceptionally weird materials.*/
};

// some repeated section at the end of the material header (CMaterialGlue) struct
struct UnknownMaterialSectionV15
{
	// Spoon - I have kept the names that Rika made for the most part here, except for adding m_

	// required but seems to follow a pattern. maybe related to "Unknown2" above?
	// nulling these bytes makes the material stop drawing entirely
	uint32_t m_Unknown1[8]{};

	// for more details see the 'UnknownMaterialSectionV12' struct.
	uint32_t m_UnkRenderFlags = 0x0;
	uint16_t m_VisibilityFlags = 0x0000; // different render settings, such as opacity and transparency.
	uint16_t m_FaceDrawingFlags = 0x0006; // how the face is drawn, culling, wireframe, etc.

	uint64_t m_Padding = 0;
};

// start of CMaterialGlue class
struct MaterialHeaderV15
{
	uint64_t m_VtblReserved = 0; // Gets set to CMaterialGlue vtbl ptr
	uint8_t m_Padding[0x8]{}; // Un-used.
	uint64_t m_nGUID = 0; // guid of this material asset

	RPakPtr m_pszName{}; // pointer to partial asset path
	RPakPtr m_pszSurfaceProp{}; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	RPakPtr m_pszSurfaceProp2{}; // pointer to surfaceprop2

	// IDX 1: DepthShadow
	// IDX 2: DepthPrepass
	// IDX 3: DepthVSM
	// IDX 4: DepthShadowTight
	// IDX 5: ColPass
	// They seem to be the exact same for all materials throughout the game.
	uint64_t m_GUIDRefs[5]{}; // Required to have proper textures.
	uint64_t m_pShaderSet = 0; // guid of the shaderset asset that this material uses

	/* 0x60 */ RPakPtr m_pTextureHandles{}; // TextureGUID Map
	/* 0x68 */ RPakPtr m_pStreamingTextureHandles{}; // Streamable TextureGUID Map

	/* 0x70 */ int16_t m_nStreamingTextureHandleCount = 0x4; // Number of textures with streamed mip levels.
	/* 0x72 */ int16_t m_nWidth = 2048;
	/* 0x74 */ int16_t m_nHeight = 2048;
	/* 0x76 */ int16_t m_Unknown1 = 0;

	/* 0x78 */ uint32_t m_ImageFlags = 0x1D0300;
	/* 0x7C */ uint32_t m_Unknown2 = 0;

	/* 0x80 */ uint32_t m_Unknown3 = 0x1F5A92BD; // REQUIRED but why?

	/* 0x84 */ uint32_t m_Unknown4 = 0;

	// neither of these 2 seem to be required
	/* 0x88 */ uint32_t m_Flags2 = 0;
	/* 0x8C */ uint32_t something = 0;

	/* 0x90 */ UnknownMaterialSectionV15 m_UnknownSections[2]{};
	/* 0xF0 */ uint8_t bytef0;
	/* 0xF1 */ uint8_t bytef1;
	/* 0xF2 */ MaterialShaderType_t materialType;
	/* 0xF3 */ uint8_t bytef3; // used for unksections loading in UpdateMaterialAsset
	/* 0xF4 */ char pad_00F4[4];
	uint64_t textureAnimationGuid = 0;
};

struct MaterialCPUDataV12
{
	MaterialTextureTransformMatrix DetailTransform[1]; // detail texture transform matrix
	MaterialTextureTransformMatrix TextureTransform[2]; // 1st is presumably texture (unconfirmed), 2nd assumed to be texture.

	// this might be another texture transform matrix.
	float UnkFloat2[6] = {
		0.0, 0.0, 0.0, 0.0, 1.0, 0.0
	};

	RGBA MainTint[1];

	// these are vector4s for rgba I would think.
	float UnkData1[12] = {
		0.0, 0.0, 0.0, 0.0,
		1.0, 0.0, 0.0, 0.0,
		10000, -0.0, 0.0, 1.778
	};

	RGBA SelfillumTint[1];

	// these are (more) vector4s for rgba I would think.
	uint8_t UnkData2[12 * 4] = {
	0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x3F,
	0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x80, 0x3F, 0x8F, 0xC2, 0xF5, 0x3C,
	0x8F, 0xC2, 0xF5, 0x3C, 0x8F, 0xC2, 0xF5, 0x3C
	};
	// this is actually floats but i cba to type all this default data in
	// the last few are stated as 0xFFFFFFFF which converts to NaN in float, converting NaN to float does not give the same results, why?
};

struct MaterialCPUDataV15
{
	UVTransformMatrix uv1;
	UVTransformMatrix uv2;
	UVTransformMatrix uv3;
	UVTransformMatrix uv4;
	UVTransformMatrix uv5;

	Vector2 c_uvDistortionIntensity = { 1.000000, 1.000000 };
	Vector2 c_uvDistortion2Intensity = { 1.000000, 1.000000 };

	float c_L0_scatterDistanceScale = 0.166667;
	float c_layerBlendRamp = 0.000000;
	float c_opacity = 1.000000;
	float c_useAlphaModulateSpecular = 0.000000;
	float c_alphaEdgeFadeExponent = 0.000000;
	float c_alphaEdgeFadeInner = 0.000000;
	float c_alphaEdgeFadeOuter = 0.000000;
	float c_useAlphaModulateEmissive = 1.000000;
	float c_emissiveEdgeFadeExponent = 0.000000;
	float c_emissiveEdgeFadeInner = 0.000000;
	float c_emissiveEdgeFadeOuter = 0.000000;
	float c_alphaDistanceFadeScale = 10000.000000;
	float c_alphaDistanceFadeBias = -0.000000;
	float c_alphaTestReference = 0.000000;
	float c_aspectRatioMulV = 1.778000;
	float c_shadowBias = 0.000000;
	float c_shadowBiasStatic = 0.000000;
	float c_dofOpacityLuminanceScale = 1.000000;
	float c_tsaaDepthAlphaThreshold = 0.000000;
	float c_tsaaMotionAlphaThreshold = 0.900000;
	float c_tsaaMotionAlphaRamp = 10.000000;
	uint32_t c_tsaaResponsiveFlag = 0x0;
	Vector3 c_outlineColorSDF = { 0.000000, 0.000000, 0.000000 };
	float c_outlineWidthSDF = 0.000000;
	Vector3 c_shadowColorSDF = { 0.000000, 0.000000, 0.000000 };
	float c_shadowWidthSDF = 0.000000;
	Vector3 c_insideColorSDF = { 0.000000, 0.000000, 0.000000 };
	float c_outsideAlphaScalarSDF = 0.000000;
	float c_glitchStrength = 0.000000;
	float c_vertexDisplacementScale = 0.000000;
	float c_innerFalloffWidthSDF = 0.000000;
	float c_innerEdgeOffsetSDF = 0.000000;
	Vector2 c_dropShadowOffsetSDF = { 0.000000, 0.000000 };
	float c_normalMapEdgeWidthSDF = 0.000000;
	float c_shadowFalloffSDF = 0.000000;
	Vector3 c_L0_scatterAmount = { 0.000000, 0.000000, 0.000000 };
	float c_L0_scatterRatio = 0.000000;
	float c_L0_transmittanceIntensityScale = 1.000000;
	Vector3 c_vertexDisplacementDirection = { 0.000000, 0.000000, 0.000000 };
	float c_L0_transmittanceAmount = 0.000000;
	float c_L0_transmittanceDistortionAmount = 0.500000;
	float c_zUpBlendingMinAngleCos = 1.000000;
	float c_zUpBlendingMaxAngleCos = 1.000000;
	float c_zUpBlendingVertexAlpha = 0.000000;
	Vector3 c_L0_albedoTint = { 1.000000, 1.000000, 1.000000 };
	float c_depthBlendScalar = 1.000000;
	Vector3 c_L0_emissiveTint = { 0.000000, 0.000000, 0.000000 };
	uint32_t c_subsurfaceMaterialID;
	Vector3 c_L0_perfSpecColor = { 0.037972, 0.037972, 0.037972 };
	float c_L0_perfGloss = 1.000000;
	Vector3 c_L1_albedoTint = { 0.000000, 0.000000, 0.000000 };
	float c_L1_perfGloss = 0.000000;
	Vector3 c_L1_emissiveTint = { 0.000000, 0.000000, 0.000000 };
	Vector3 c_L1_perfSpecColor = { 0.000000, 0.000000, 0.000000 };
	float c_splineMinPixelPercent = 0.000000;
	Vector2 c_L0_anisoSpecCosSinTheta = { 1.000000, 0.000000 };
	Vector2 c_L1_anisoSpecCosSinTheta = { 1.000000, 0.000000 };
	float c_L0_anisoSpecStretchAmount = 0.000000;
	float c_L1_anisoSpecStretchAmount = 0.000000;
	float c_L0_emissiveHeightFalloff = 0.000000;
	float c_L1_emissiveHeightFalloff = 0.000000;
	float c_L1_transmittanceIntensityScale = 0.000000;
	float c_L1_transmittanceAmount = 0.000000;
	float c_L1_transmittanceDistortionAmount = 0.000000;
	float c_L1_scatterDistanceScale = 0.000000;
	Vector3 c_L1_scatterAmount = { 0.000000, 0.000000, 0.000000 };
	float c_L1_scatterRatio = 0.000000;
};

#pragma pack(pop)
