#pragma once

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
};

#pragma pack(push, 1)

// the following two structs are found in the ""cpu data"", they are very much alike to what you would use in normal source materials.
// apex probably has these and more stuff.
struct MaterialTextureTransformMatrix
{
	// very similar to how it's done in normal source
	float TextureScaleX = 1.f;
	float TextureUnk = 0.f; // unsure what this does, appears to skew/rotate and scale the texture at the same time? weird.
	float TextureRotation = -0.f; // counter clockwise, 0-1, exceeding one causes Weird Stuff to happen.
	float TextureScaleY = 1.f;
	float TextureTranslateX = 0.f;
	float TextureTranslateY = 0.f;
};

// some repeated section at the end of the material header (CMaterialGlue) struct
struct UnknownMaterialSectionV15
{
	// Spoon - I have kept the names that Rika made for the most part here, except for adding m_ 

	// required but seems to follow a pattern. maybe related to "Unknown2" above?
	// nulling these bytes makes the material stop drawing entirely
	uint32_t unk_0[8]{};

	// for more details see the 'UnknownMaterialSectionV12' struct.
	uint32_t unk = 0x0;
	uint16_t depthStencilFlags = 0x0000; // different render settings, such as opacity and transparency.
	uint16_t rasterizerFlags = 0x0006; // how the face is drawn, culling, wireframe, etc.

	uint64_t m_Padding = 0;
};

// this is currently unused, but could probably be used just fine if you copy stuff over from the RPak V7 material function in material.cpp
struct MaterialCPUDataV15
{
	// hard to test this but I'm pretty sure that's where it is.
	MaterialTextureTransformMatrix DetailTransform[1]; // detail texture transform matrix

	// SelfIllumTint NEEDS to be found.
	// this has lots of similar bits to the V12 version but I cba to actually dig into it.
	// the top section has as few MaterialTextureTransformMatrix for sure, the section is probably comprised of floats as well.
	uint8_t testData[520] = {
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0xAB, 0xAA, 0x2A, 0x3E, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x1C, 0x46, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
		0x81, 0x95, 0xE3, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xDE, 0x88, 0x1B, 0x3D, 0xDE, 0x88, 0x1B, 0x3D, 0xDE, 0x88, 0x1B, 0x3D,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

};

// start of CMaterialGlue class
struct MaterialHeaderV15
{
	uint64_t vftableReserved = 0; // reserved for virtual function table pointer (when copied into native CMaterialGlue)
	char gap_8[0x8]{}; // unused?
	uint64_t guid = 0; // guid of this material asset

	PagePtr_t materialName{}; // pointer to partial asset path
	PagePtr_t surfaceProp{}; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	PagePtr_t surfaceProp2{}; // pointer to surfaceprop2 

	uint64_t depthShadowMaterial = 0;
	uint64_t depthPrepassMaterial = 0;
	uint64_t depthVSMMaterial = 0;
	uint64_t depthShadowTightMaterial = 0;
	uint64_t colpassMaterial = 0;

	uint64_t shaderSet = 0; // guid of the shaderset asset that this material uses

	/* 0x60 */ PagePtr_t textureHandles{}; // ptr to array of texture guids
	/* 0x68 */ PagePtr_t streamingTextureHandles{}; // ptr to array of streamable texture guids (empty at build time)

	/* 0x70 */ short numStreamingTextureHandles = 0x4; // Number of textures with streamed mip levels.
	/* 0x72 */ short width = 2048;
	/* 0x74 */ short height = 2048;
	/* 0x76 */ short unk_76 = 0;

	/* 0x78 */ uint32_t flags_78 = 0x1D0300;
	/* 0x7C */ uint32_t unk_7C = 0;

	/* 0x80 */ uint32_t unk_80 = 0x1F5A92BD; // REQUIRED but why?

	/* 0x84 */ uint32_t unk_84 = 0;

	// neither of these 2 seem to be required
	/* 0x88 */ uint32_t unk_88 = 0;
	/* 0x8C */ uint32_t unk_8C = 0;

	/* 0x90 */ UnknownMaterialSectionV15 unkSections[2]{}; // seems to be used for setting up some D3D states?
	/* 0xF0 */ uint8_t bytef0;
	/* 0xF1 */ uint8_t bytef1;
	/* 0xF2 */ MaterialShaderType_t materialType;
	/* 0xF3 */ uint8_t bytef3; // used for unksections loading in UpdateMaterialAsset
	/* 0xF4 */ char pad_00F4[12];
};

// bunch this up into a struct for ease of access and readability
struct uvTransformMatrix
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

	inline float* pFloat(int i) { return reinterpret_cast<float*>((float*)this + i); } // const ptr here mayhaps
};

// Titanfall 2 uses generally the same shader buffer so we can store the full struct
struct MaterialShaderBufferV12
{
	// the assignment of these depends on the shader set, they work similarly to texturetransforms in normal source.
	uvTransformMatrix c_uv1; // this is frequently used for detail textures.
	uvTransformMatrix c_uv2;
	uvTransformMatrix c_uv3;

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

struct UnknownMaterialSectionV12
{
	// not sure how these work but 0xF0 -> 0x00 toggles them off and vice versa.
	// they seem to affect various rendering filters, said filters might actually be the used shaders.
	// the duplicate one is likely for the second set of textures which (probably) never gets used.
	uint32_t UnkRenderLighting = 0xF0138286;
	uint32_t UnkRenderAliasing = 0xF0138286;
	uint32_t UnkRenderDoF = 0xF0008286;
	uint32_t UnkRenderUnknown = 0x00138286;

	// for more details see the 'UnknownMaterialSectionV12' struct.
	uint32_t unk = 0x5;
	uint16_t depthStencilFlags = 0x0; // different render settings, such as opacity and transparency.
	uint16_t rasterizerFlags = 0x6; // how the face is drawn, culling, wireframe, etc.

	uint64_t padding; // aligment to 16 bytes (probably)
};

struct MaterialHeaderV12
{
	uint64_t vftableReserved = 0; // Gets set to CMaterialGlue vtbl ptr
	char gap_8[0x8]{}; // unused?
	uint64_t guid = 0; // guid of this material asset

	PagePtr_t materialName{}; // pointer to partial asset path
	PagePtr_t surfaceProp{}; // pointer to surfaceprop (as defined in surfaceproperties.rson)
	PagePtr_t surfaceProp2{}; // pointer to surfaceprop2 

	// IDX 1: DepthShadow
	// IDX 2: DepthPrepass
	// IDX 3: DepthVSM
	// IDX 4: ColPass
	// Titanfall is does not have 'DepthShadowTight'

	uint64_t depthShadowMaterial = 0;
	uint64_t depthPrepassMaterial = 0;
	uint64_t depthVSMMaterial = 0;
	uint64_t colpassMaterial = 0;

	// these blocks dont seem to change often but are the same?
	// these blocks relate to different render filters and flags. still not well understood.
	UnknownMaterialSectionV12 unkSections[2];

	uint64_t shaderSet = 0; // guid of the shaderset asset that this material uses

	PagePtr_t textureHandles{}; // TextureGUID Map
	PagePtr_t streamingTextureHandles{}; // Streamable TextureGUID Map

	short numStreamingTextureHandles = 0; // Number of textures with streamed mip levels.
	uint32_t flags = 0x503000; // see ImageFlags in the apex struct.
	short unk1 = 0;

	uint64_t unk2 = 0; // haven't observed anything here.

	// seems to be 0xFBA63181 for loadscreens
	uint32_t unk3 = 0xFBA63181; // no clue tbh

	uint32_t unk4 = 0; // this might actually be "Alignment"

	uint64_t flags2;

	short width;
	short height;

	uint32_t unk6 = 0; // likely alignment

	/* ImageFlags
	0x050300 for loadscreens, 0x1D0300 for normal materials.
	0x1D has been observed, seems to invert lighting? used on some exceptionally weird materials.*/
};
static_assert(sizeof(MaterialHeaderV12) == 208); // should be size of 208

// header struct for the material asset cpu data
struct MaterialCPUHeader
{
	PagePtr_t dataPtr{}; // points to the rest of the cpu data
	uint32_t dataSize = 0;
	uint32_t maybeVersion = 3; // every unknown is now either datasize, version, or flags
};
#pragma pack(pop)
