#pragma once
#include "materialflags.h"

#define MAT_DX_STATE_COUNT 2 // the same for r2 and r5
#define MAT_BLEND_STATE_COUNT 8 // r2 is 4

enum MaterialShaderType_t : uint8_t
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

	// not real types
	_TYPE_COUNT = 0xA,
	_TYPE_LEGACY = 0xFE, // r2 types
	_TYPE_INVALID = 0xFF,
};

static const char* s_materialShaderTypeNames[] = {
	"rgdu",
	"rgdp",
	"rgdc",
	"sknu",
	"sknp",
	"sknc",
	"wldu",
	"wldc",
	"ptcu",
	"ptcs",
};

static const std::map<int, MaterialShaderType_t> s_materialShaderTypeMap
{
	// static props
	{'udgr', RGDU}, // rgdu
	{'pdgr', RGDP}, // rgdp
	{'cdgr', RGDC}, // rgdc

	// non-static models
	{'unks', SKNU}, // sknu
	{'pnks', SKNP}, // sknp
	{'cnks', SKNC}, // sknc

	// world/geo models
	{'udlw', WLDU}, // wldu
	{'cdlw', WLDC}, // wldc

	// particles
	{'uctp', PTCU}, // ptcu
	{'sctp', PTCS}, // ptcs

	// r2 materials
	{'neg', _TYPE_LEGACY}, // gen
	{'dlw', _TYPE_LEGACY}, // wld
	{'xif', _TYPE_LEGACY}, // fix
	{'nks', _TYPE_LEGACY}, // skn
};


inline MaterialShaderType_t Material_ShaderTypeFromString(std::string& str)
{
	int type = *reinterpret_cast<const int*>(str.c_str());

	if (s_materialShaderTypeMap.count(type) != 0)
		return s_materialShaderTypeMap.at(type);

	return _TYPE_INVALID;
}

struct MaterialBlendState_t
{
	__int32 unknown : 1;
	__int32 blendEnable : 1;
	__int32 srcBlend : 5;
	__int32 destBlend : 5;
	__int32 blendOp : 3;
	__int32 srcBlendAlpha : 5;
	__int32 destBlendAlpha : 5;
	__int32 blendOpAlpha : 3;
	__int32 renderTargetWriteMask : 4;

	MaterialBlendState_t() = default;

	MaterialBlendState_t(const bool bUnknown, const bool bBlendEnable,
		const D3D11_BLEND _srcBlend, const D3D11_BLEND _destBlend,
		const D3D11_BLEND_OP _blendOp, const D3D11_BLEND _srcBlendAlpha,
		const D3D11_BLEND _destBlendAlpha, const D3D11_BLEND_OP _blendOpAlpha,
		const __int8 _renderTargetWriteMask)
	{
		this->unknown = bUnknown ? 1 : 0;
		this->blendEnable = bBlendEnable ? 1 : 0;

		this->srcBlend = _srcBlend;
		this->destBlend = _destBlend;
		this->blendOp = _blendOp;
		this->srcBlendAlpha = _srcBlendAlpha;
		this->destBlendAlpha = _destBlendAlpha;
		this->blendOpAlpha = _blendOpAlpha;

		this->renderTargetWriteMask = _renderTargetWriteMask & 0xF;
	}

	MaterialBlendState_t(const unsigned int nFlags)
	{
		this->unknown = (nFlags & 1);
		this->blendEnable = ((nFlags >> 1) & 1);

		this->srcBlend = ((nFlags >> 2) & 0x1F);
		this->destBlend = ((nFlags >> 7) & 0x1F);
		this->blendOp = ((nFlags >> 12) & 7);
		this->srcBlendAlpha = ((nFlags >> 15) & 0x1F);
		this->destBlendAlpha = ((nFlags >> 20) & 0x1F);
		this->blendOpAlpha = ((nFlags >> 25) & 7);

		this->renderTargetWriteMask = (nFlags >> 28) & 0xF;
	}
};

// Suppress structure alignment warnings, we need to align them to 16 bytes
// for SIMD alignment.
#pragma warning(push)
#pragma warning(disable : 4324)

// aligned to 16 bytes so it can be loaded as 3 m128i structs in engine
struct __declspec(align(16)) MaterialDXState_v15_t
{
	// bitfield defining a D3D11_RENDER_TARGET_BLEND_DESC for each of the 8 possible DX render targets
	MaterialBlendState_t blendStates[MAT_BLEND_STATE_COUNT];

	uint32_t unk;

	// flags to determine how the D3D11_DEPTH_STENCIL_DESC is defined for this material
	uint16_t depthStencilFlags;

	// flags to determine how the D3D11_RASTERIZER_DESC is defined
	uint16_t rasterizerFlags;
};

static_assert(sizeof(MaterialDXState_v15_t) == 0x30);

struct __declspec(align(16)) MaterialDXState_v12_t
{
	// r2 only supports 4 render targets?
	MaterialBlendState_t blendStates[4];

	uint32_t unk; // 0x5
	uint16_t depthStencilFlags; // different render settings, such as opacity and transparency.
	uint16_t rasterizerFlags; // how the face is drawn, culling, wireframe, etc.

	uint64_t padding; // alignment to 16 bytes (probably)
};
static_assert(sizeof(MaterialDXState_v12_t) == 0x20);
#pragma warning(pop)

#pragma pack(push, 1)

// bunch this up into a struct for ease of access and readability
struct uvTransform_t
{
	// this section is actually broken up into three parts.
	// c_uvRotScaleX
	float uvScaleX = 1.f;
	float uvRotationX = 0.f; // rotation, but w e i r d.
	// c_uvRotScaleY
	float uvRotationY = -0.f; //counter clockwise, 0-1, exceeding one causes Weird Stuff to happen.
	float uvScaleY = 1.f;
	// c_uvTranslate
	float uvTranslateX = 0.f;
	float uvTranslateY = 0.f;

	inline float* pFloat(int i) { return reinterpret_cast<float*>(this) + i; } // const ptr here mayhaps
};

// Titanfall 2 uses generally the same shader buffer so we can store the full struct (for now)
struct MaterialShaderBufferV12
{
	// the assignment of these depends on the shader set, they work similarly to texturetransforms in normal source.
	uvTransform_t c_uv1; // this is frequently used for detail textures.
	uvTransform_t c_uv2;
	uvTransform_t c_uv3;

	// u v
	float c_uvDistortionIntensity[2] = {0.f, 0.f}; // distortion on the { x, y } axis.
	float c_uvDistortion2Intensity[2] = {0.f, 0.f}; // see above, but for a second distortion texture.

	float c_fogColorFactor = 1.f;

	float c_layerBlendRamp = 0.f; // blend intensity (assumed), likely the hardness/softness of the two textures meshing.

	// r g b
	float c_albedoTint[3] = {1.f, 1.f, 1.f}; // color of the albedo texture.
	float c_opacity = 1.f; // untested.

	float c_useAlphaModulateSpecular = 0.f;
	float c_alphaEdgeFadeExponent = 0.f;
	float c_alphaEdgeFadeInner = 0.f;
	float c_alphaEdgeFadeOuter = 0.f;

	float c_useAlphaModulateEmissive = 1.f; // almost always set to 1.
	float c_emissiveEdgeFadeExponent = 0.f;
	float c_emissiveEdgeFadeInner = 0.f;
	float c_emissiveEdgeFadeOuter = 0.f;

	float c_alphaDistanceFadeScale = 10000.f;
	float c_alphaDistanceFadeBias = -0.f;
	float c_alphaTestReference = 0.f;

	float c_aspectRatioMulV = 1.778f; // this is equal to width divided by height see: 16/9 = 1.778~, not clear what it actually does.

	// r g b
	float c_emissiveTint[3] = {0.f, 0.f, 0.f}; // color of the emission, this is normally set to { 0.f, 0.f, 0.f } if you don't have an emission mask.

	float c_shadowBias = 0.f;

	float c_tsaaDepthAlphaThreshold = 0.f;
	float c_tsaaMotionAlphaThreshold = 0.9f;
	float c_tsaaMotionAlphaRamp = 10.f;
	int c_tsaaResponsiveFlag = 0x0; // this is 0 or 1 I think.

	float c_dofOpacityLuminanceScale = 1.f;

	union {
		int pad_CBufUberStatic[3] = { -1, -1, -1 }; // this is reserved space for special values, three sections by default.
		float c_glitchStrength; // only used  sometimes. on 'Glitch' shadersets, if used 'pad_CBufUberStatic' is only two sections.
	};

	float c_perfGloss = 1.f;

	// r g b
	float c_perfSpecColor[3] = {0.03f, 0.03f, 0.03f}; // specular color, consistent across most materials.

	inline char* AsCharPtr() { return reinterpret_cast<char*>(this); }
};

// from tripletake
struct MaterialShaderBufferV15
{
	// the assignment of these depends on the shader set, they work similarly to texturetransforms in normal source.
	uvTransform_t c_uv1; // this is frequently used for detail textures.
	uvTransform_t c_uv2;
	uvTransform_t c_uv3;
	uvTransform_t c_uv4;
	uvTransform_t c_uv5;

	float c_uvDistortionIntensity[2] = {1.f, 1.f};
	float c_uvDistortion2Intensity[2] = {1.f, 1.f};

	float c_L0_scatterDistanceScale = 0.166667f;

	float c_layerBlendRamp = 0.f;

	float c_opacity = 1.f;

	float c_useAlphaModulateSpecular = 0.f;
	float c_alphaEdgeFadeExponent = 0.f;
	float c_alphaEdgeFadeInner = 0.f;
	float c_alphaEdgeFadeOuter = 0.f;
	float c_useAlphaModulateEmissive = 1.f;
	float c_emissiveEdgeFadeExponent = 0.f;
	float c_emissiveEdgeFadeInner = 0.f;
	float c_emissiveEdgeFadeOuter = 0.f;
	float c_alphaDistanceFadeScale = 10000.f;
	float c_alphaDistanceFadeBias = -0.f;
	float c_alphaTestReference = 0.f;
	float c_aspectRatioMulV = 1.778000f;
	float c_shadowBias = 0.f;
	float c_shadowBiasStatic = 0.f;
	float c_dofOpacityLuminanceScale = 1.f;
	float c_tsaaDepthAlphaThreshold = 0.f;
	float c_tsaaMotionAlphaThreshold = 0.9f;
	float c_tsaaMotionAlphaRamp = 10.f;
	char UNIMPLEMENTED_c_tsaaResponsiveFlag[4];
	float c_outlineColorSDF[3] = {0.f, 0.f, 0.f};
	float c_outlineWidthSDF = 0.f;
	float c_shadowColorSDF[3] = { 0.f, 0.f, 0.f };
	float c_shadowWidthSDF = 0.f;
	float c_insideColorSDF[3] = {0.f, 0.f, 0.f};
	float c_outsideAlphaScalarSDF = 0.f;
	float c_glitchStrength = 0.f;
	float c_vertexDisplacementScale = 0.f;
	float c_innerFalloffWidthSDF = 0.f;
	float c_innerEdgeOffsetSDF = 0.f;
	float c_dropShadowOffsetSDF[2] = {0.f, 0.f};
	float c_normalMapEdgeWidthSDF = 0.f;
	float c_shadowFalloffSDF = 0.f;
	float c_L0_scatterAmount[3] = {0.f, 0.f, 0.f};
	float c_L0_scatterRatio = 0.f;
	float c_L0_transmittanceIntensityScale = 1.f;
	float c_vertexDisplacementDirection[3] = {0.f, 0.f, 0.f};
	float c_L0_transmittanceAmount = 0.f;
	float c_L0_transmittanceDistortionAmount = 0.5f;
	float c_zUpBlendingMinAngleCos = 1.f;
	float c_zUpBlendingMaxAngleCos = 1.f;
	float c_zUpBlendingVertexAlpha = 0.f;
	float c_L0_albedoTint[3] = {1.f, 1.f, 1.f};
	float c_depthBlendScalar = 1.f;
	float c_L0_emissiveTint[3] = {1.f, 1.f, 1.f};
	char UNIMPLEMENTED_c_subsurfaceMaterialID[4];
	float c_L0_perfSpecColor[3] = {0.037972f, 0.037972f, 0.037972f};
	float c_L0_perfGloss = 1.f;
	float c_L1_albedoTint[3] = {0.f, 0.f, 0.f};
	float c_L1_perfGloss = 0.f;
	float c_L1_emissiveTint[3] = {0.f, 0.f, 0.f};
	float c_L1_perfSpecColor[3] = {0.f, 0.f, 0.f};
	float c_splineMinPixelPercent = 0.f;
	float c_L0_anisoSpecCosSinTheta[2] = {1.f, 0.f};
	float c_L1_anisoSpecCosSinTheta[2] = {1.f, 0.f};
	float c_L0_anisoSpecStretchAmount = 0.f;
	float c_L1_anisoSpecStretchAmount = 0.f;
	float c_L0_emissiveHeightFalloff = 0.f;
	float c_L1_emissiveHeightFalloff = 0.f;
	float c_L1_transmittanceIntensityScale = 0.f;
	float c_L1_transmittanceAmount = 0.f;
	float c_L1_transmittanceDistortionAmount = 0.f;
	float c_L1_scatterDistanceScale = 0.f;
	float c_L1_scatterAmount[3] = {0.f, 0.f, 0.f};
	float c_L1_scatterRatio = 0.f;

	inline char* AsCharPtr() { return reinterpret_cast<char*>(this); }
};
static_assert(sizeof(MaterialShaderBufferV15) == 512);

// agony
struct GenericShaderBuffer
{
	// the assignment of these depends on the shader set, they work similarly to texturetransforms in normal source.
	uvTransform_t c_uv1; // this is frequently used for detail textures.
	uvTransform_t c_uv2;
	uvTransform_t c_uv3;
	uvTransform_t c_uv4;
	uvTransform_t c_uv5;

	float c_uvDistortionIntensity[2] = { 0.f, 0.f }; // distortion on the { x, y } axis.
	float c_uvDistortion2Intensity[2] = { 0.f, 0.f }; // see above, but for a second distortion texture.	

	float c_layerBlendRamp;

	float c_L0_albedoTint[3];
	float c_L1_albedoTint[3];

	float c_opacity;

	float c_L0_emissiveTint[3];
	float c_L1_emissiveTint[3];

	float c_L0_perfSpecColor[3];
	float c_L1_perfSpecColor[3];

	MaterialShaderBufferV12 GenericV12()
	{
		MaterialShaderBufferV12 out{};

		out.c_uv1 = this->c_uv1;
		out.c_uv2 = this->c_uv2;
		out.c_uv3 = this->c_uv3;

		out.c_uvDistortionIntensity[0] = this->c_uvDistortionIntensity[0];
		out.c_uvDistortionIntensity[1] = this->c_uvDistortionIntensity[1];
		out.c_uvDistortion2Intensity[0] = this->c_uvDistortion2Intensity[0];
		out.c_uvDistortion2Intensity[1] = this->c_uvDistortion2Intensity[1];

		out.c_layerBlendRamp = this->c_layerBlendRamp;

		for (int i = 0; i < 3; i++)
		{
			out.c_albedoTint[i] = this->c_L0_albedoTint[i];
		}

		out.c_opacity = this->c_opacity;

		for (int i = 0; i < 3; i++)
		{
			out.c_emissiveTint[i] = this->c_L0_emissiveTint[i];
		}

		return out;
	}

	MaterialShaderBufferV15 GenericV15()
	{
		MaterialShaderBufferV15 out{};

		out.c_uv1 = this->c_uv1;
		out.c_uv2 = this->c_uv2;
		out.c_uv3 = this->c_uv3;
		out.c_uv3 = this->c_uv4;
		out.c_uv3 = this->c_uv5;

		out.c_uvDistortionIntensity[0] = this->c_uvDistortionIntensity[0];
		out.c_uvDistortionIntensity[1] = this->c_uvDistortionIntensity[1];
		out.c_uvDistortion2Intensity[0] = this->c_uvDistortion2Intensity[0];
		out.c_uvDistortion2Intensity[1] = this->c_uvDistortion2Intensity[1];

		out.c_layerBlendRamp = this->c_layerBlendRamp;

		for (int i = 0; i < 3; i++)
		{
			out.c_L0_albedoTint[i] = this->c_L0_albedoTint[i];
		}

		out.c_opacity = this->c_opacity;

		for (int i = 0; i < 3; i++)
		{
			out.c_L0_emissiveTint[i] = this->c_L0_emissiveTint[i];
		}

		return out;
	}
};

// Suppress structure alignment warnings, we need to align them to 16 bytes
// for SIMD alignment.
#pragma warning(push)
#pragma warning(disable : 4324)

struct __declspec(align(16)) MaterialAssetHeader_v12_t
{
	uint64_t vftableReserved; // Gets set to CMaterialGlue vtbl ptr
	char gap_8[0x8]; // unused?
	uint64_t guid; // guid of this material asset

	PagePtr_t materialName; // pointer to partial asset path
	PagePtr_t surfaceProp; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	PagePtr_t surfaceProp2; // pointer to surfaceprop2 

	uint64_t depthShadowMaterial;
	uint64_t depthPrepassMaterial;
	uint64_t depthVSMMaterial;
	uint64_t colpassMaterial;

	// these blocks dont seem to change often but are the same?
	// these blocks relate to different render filters and flags. still not well understood.
	MaterialDXState_v12_t dxStates[2];

	uint64_t shaderSet; // guid of the shaderset asset that this material uses

	PagePtr_t textureHandles; // TextureGUID Map
	PagePtr_t streamingTextureHandles; // Streamable TextureGUID Map
	short numStreamingTextureHandles; // Number of textures with streamed mip levels.

	char samplers[4]; // 0x503000

	short unk_AE;
	uint64_t unk_B0; // haven't observed anything here.

	// seems to be 0xFBA63181 for loadscreens
	uint32_t unk_B8; // no clue tbh, 0xFBA63181

	uint32_t unk_BC; // this might actually be "Alignment"

	uint64_t flags2;

	short width;
	short height;

	uint32_t unk_CC; // likely alignment

	/* flags
	0x050300 for loadscreens, 0x1D0300 for normal materials.
	0x1D has been observed, seems to invert lighting? used on some exceptionally weird materials.*/
};
static_assert(sizeof(MaterialAssetHeader_v12_t) == 208);

// start of CMaterialGlue class
struct __declspec(align(16)) MaterialAssetHeader_v15_t
{
	uint64_t vftableReserved; // reserved for virtual function table pointer (when copied into native CMaterialGlue)

	char gap_8[0x8]; // unused?
	uint64_t guid; // guid of this material asset

	PagePtr_t materialName; // pointer to partial asset path
	PagePtr_t surfaceProp; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	PagePtr_t surfaceProp2; // pointer to surfaceprop2 

	uint64_t depthShadowMaterial;
	uint64_t depthPrepassMaterial;
	uint64_t depthVSMMaterial;
	uint64_t depthShadowTightMaterial;
	uint64_t colpassMaterial;

	uint64_t shaderSet; // guid of the shaderset asset that this material uses

	PagePtr_t textureHandles; // ptr to array of texture guids
	PagePtr_t streamingTextureHandles; // ptr to array of streamable texture guids (empty at build time)

	short numStreamingTextureHandles; // number of textures with streamed mip levels.
	short width;
	short height;
	short depth;

	// array of indices into sampler states array. must be set properly to have accurate texture tiling
	// used in CShaderGlue::SetupShader (1403B3C60)
	char samplers[4];// = 0x1D0300;

	uint32_t unk_7C;

	uint32_t unk_80;// = 0x1F5A92BD; // REQUIRED but why?

	uint32_t unk_84;

	uint64_t flags2;

	MaterialDXState_v15_t dxStates[2]; // seems to be used for setting up some D3D states?

	uint16_t numAnimationFrames; // used in CMaterialGlue::GetNumAnimationFrames (0x1403B4250), which is called from GetSpriteInfo @ 0x1402561FC
	MaterialShaderType_t materialType;
	uint8_t bytef3; // used for unksections loading in UpdateMaterialAsset

	//char pad_00F4[0x4];

	uint64_t textureAnimation;
};
static_assert(sizeof(MaterialAssetHeader_v15_t) == 256);

struct MaterialAsset_t
{
	int assetVersion;

	uint64_t guid; // guid of this material asset

	const char* materialAssetPath;
	PagePtr_t materialName; // pointer to partial asset path
	PagePtr_t surfaceProp; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	PagePtr_t surfaceProp2; // pointer to surfaceprop2 

	uint64_t depthShadowMaterial;
	uint64_t depthPrepassMaterial;
	uint64_t depthVSMMaterial;
	uint64_t depthShadowTightMaterial;
	uint64_t colpassMaterial;

	uint64_t shaderSet = 0; // guid of the shaderset asset that this material uses

	uint16_t numAnimationFrames;
	uint64_t textureAnimation;

	PagePtr_t textureHandles; // ptr to array of texture guids
	PagePtr_t streamingTextureHandles; // ptr to array of streamable texture guids (empty at build time)

	short width;
	short height;
	short depth;

	uint32_t unk; // 0x1F5A92BD, REQUIRED but why?

	char samplers[4];
	uint64_t flags2;

	MaterialDXState_v15_t dxStates[MAT_DX_STATE_COUNT]; // seems to be used for setting up some D3D states?

	std::string materialTypeStr;
	MaterialShaderType_t materialType;

	//std::string name;
	std::string surface;
	std::string surface2;

	void SetupDepthMaterialOverrides(const rapidjson::Value& mapEntry);
	void FromJSON(rapidjson::Value& mapEntry);

	void WriteToBuffer(char* buf)
	{
		if (assetVersion <= 12) // r2 and older
		{
			MaterialAssetHeader_v12_t* matl = reinterpret_cast<MaterialAssetHeader_v12_t*>(buf);

			matl->guid = this->guid;

			matl->materialName = this->materialName;
			matl->surfaceProp = this->surfaceProp;
			matl->surfaceProp2 = this->surfaceProp2;

			matl->depthShadowMaterial = this->depthShadowMaterial;
			matl->depthPrepassMaterial = this->depthPrepassMaterial;
			matl->depthVSMMaterial = this->depthVSMMaterial;
			matl->colpassMaterial = this->colpassMaterial;
			matl->shaderSet = this->shaderSet;

			matl->textureHandles = this->textureHandles;
			matl->streamingTextureHandles = this->streamingTextureHandles;

			matl->width = this->width;
			matl->height = this->height;
			// matl->depth = this->depth;

			matl->unk_B8 = this->unk;

			memcpy(matl->samplers, this->samplers, sizeof(matl->samplers));
			//matl->samplers = this->samplers;
			matl->flags2 = this->flags2;

			for (int i = 0; i < 2; i++)
			{
				matl->dxStates[i].blendStates[0] = this->dxStates[i].blendStates[0];
				matl->dxStates[i].blendStates[1] = this->dxStates[i].blendStates[1];
				matl->dxStates[i].blendStates[2] = this->dxStates[i].blendStates[2];
				matl->dxStates[i].blendStates[3] = this->dxStates[i].blendStates[3];

				matl->dxStates[i].unk = this->dxStates[i].unk;
				matl->dxStates[i].depthStencilFlags = this->dxStates[i].depthStencilFlags;
				matl->dxStates[i].rasterizerFlags = this->dxStates[i].rasterizerFlags;
			}
		}
		else if (assetVersion == 15) // version 15 - season 3 apex
		{
			MaterialAssetHeader_v15_t* matl = reinterpret_cast<MaterialAssetHeader_v15_t*>(buf);

			matl->guid = this->guid;

			matl->materialName = this->materialName;
			matl->surfaceProp = this->surfaceProp;
			matl->surfaceProp2 = this->surfaceProp2;

			matl->depthShadowMaterial = this->depthShadowMaterial;
			matl->depthPrepassMaterial = this->depthPrepassMaterial;
			matl->depthVSMMaterial = this->depthVSMMaterial;
			matl->depthShadowTightMaterial = this->depthShadowTightMaterial;
			matl->colpassMaterial = this->colpassMaterial;
			matl->shaderSet = this->shaderSet;

			matl->textureHandles = this->textureHandles;
			matl->streamingTextureHandles = this->streamingTextureHandles;

			matl->width = this->width;
			matl->height = this->height;
			// matl->depth = this->depth;

			matl->unk_80 = this->unk;

			memcpy(matl->samplers, this->samplers, sizeof(matl->samplers));
			matl->flags2 = this->flags2;

			matl->materialType = this->materialType;

			matl->textureAnimation = this->textureAnimation;
			matl->numAnimationFrames = this->numAnimationFrames;

			for (int i = 0; i < 2; i++)
			{
				for (int targetIdx = 0; targetIdx < 8; targetIdx++)
				{
					matl->dxStates[i].blendStates[targetIdx] = this->dxStates[i].blendStates[targetIdx];
				}

				matl->dxStates[i].unk = this->dxStates[i].unk;
				matl->dxStates[i].depthStencilFlags = this->dxStates[i].depthStencilFlags;
				matl->dxStates[i].rasterizerFlags = this->dxStates[i].rasterizerFlags;
			}
		}
	}
};

#pragma warning(pop)

// header struct for the material asset cpu data
struct MaterialCPUHeader
{
	PagePtr_t  dataPtr; // points to the rest of the cpu data. shader buffer.
	uint32_t dataSize;
	uint32_t unk_C; // every unknown is now either datasize, version, or flags. enum?
};
#pragma pack(pop)
