#pragma once

#include <d3d11.h>

struct Vector3
{
	float x, y, z;

	Vector3(float x, float y, float z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vector3() {};
};

//I think this implementation is better than what 'TintVec4' was before..
struct Vector4
{
	float r, g, b, a;

	Vector4(float r, float g, float b, float a) {
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
	}

	Vector4() {};
};

#pragma pack(push, 1)

// represents a "pointer" into a mempage by page index and offset
// when loaded, these usually get converted to a real pointer
struct RPakPtr
{
	uint32_t m_nIndex = 0;
	uint32_t m_nOffset = 0;
};

// generic header struct for both apex and titanfall 2
// contains all the necessary members for both, RPakFileBase::WriteHeader decides
// which should be written depending on the version
struct RPakFileHeader
{
	uint32_t magic = 0x6b615052;
	uint16_t fileVersion = 0x8;
	uint8_t  flags[0x2];
	uint64_t fileTime; // this is actually FILETIME, but if we don't make it uint64_t here, it'll break the struct when writing
	uint8_t  unk0[0x8];
	uint64_t compressedSize; // size of the rpak file on disk before decompression
	uint64_t embeddedStarpakOffset = 0;
	uint8_t  unk1[0x8];
	uint64_t decompressedSize; // actual data size of the rpak file after decompression
	uint64_t embeddedStarpakSize = 0;
	uint8_t  unk2[0x8];
	uint16_t starpakPathsSize = 0; // size in bytes of the section containing mandatory starpak paths
	uint16_t optStarpakPathsSize = 0; // size in bytes of the section containing optional starpak paths
	uint16_t virtualSegmentCount = 0;
	uint16_t pageCount = 0; // number of "mempages" in the rpak
	uint16_t patchIndex = 0;
	uint16_t alignment = 0;
	uint32_t descriptorCount = 0;
	uint32_t assetCount = 0;
	uint32_t guidDescriptorCount = 0;
	uint32_t relationCount = 0;

	// only in tf2
	uint32_t unk7count = 0;
	uint32_t unk8count = 0;

	// only in apex
	uint8_t  unk3[0x1c];
};

struct RPakPatchCompressedHeader // Comes after file header if its an patch rpak.
{
	uint64_t compressedSize;
	uint64_t decompressedSize;
};

// segment
// these probably aren't actually called virtual segments
// this struct doesn't really describe any real data segment, but collects info
// about the size of pages that are using specific flags/types/whatever
struct RPakVirtualSegment
{
	uint32_t flags = 0; // not sure what this actually is, doesn't seem to be used in that many places
	uint32_t alignment = 0;
	uint64_t dataSize = 0;
};

// mem page
// describes an actual section in the file data. all pages are sequential
// with page at idx 0 being just after the asset relation data
// in patched rpaks (e.g. common(01).rpak), these sections don't fully line up with the data,
// because of both the patch edit stream and also missing pages that are only present in the base rpak
struct RPakPageInfo
{
	uint32_t segIdx; // index into vseg array
	uint32_t pageAlignment; // no idea
	uint32_t dataSize; // actual size of page in bytes
};

// defines the location of a data "pointer" within the pak's mem pages
// allows the engine to read the index/offset pair and replace it with an actual memory pointer at runtime
struct RPakDescriptor
{
	uint32_t index;	 // page index
	uint32_t offset; // offset within page
};

// same kinda thing as RPakDescriptor, but this one tells the engine where
// guid references to other assets are within mem pages
typedef RPakDescriptor RPakGuidDescriptor;

// this definitely doesn't need to be in a struct but whatever
struct RPakRelationBlock
{
	uint32_t FileID;
};

// defines a bunch of values for registering/using an asset from the rpak
struct RPakAssetEntry
{
	RPakAssetEntry() = default;

	void InitAsset(uint64_t nGUID,
		uint32_t nSubHeaderBlockIdx,
		uint32_t nSubHeaderBlockOffset,
		uint32_t nSubHeaderSize,
		uint32_t nRawDataBlockIdx,
		uint32_t nRawDataBlockOffset,
		uint64_t nStarpakOffset,
		uint64_t nOptStarpakOffset,
		uint32_t Type)
	{
		this->guid = nGUID;
		this->headIdx = nSubHeaderBlockIdx;
		this->headOffset = nSubHeaderBlockOffset;
		this->cpuIdx = nRawDataBlockIdx;
		this->cpuOffset = nRawDataBlockOffset;
		this->starpakOffset = nStarpakOffset;
		this->optStarpakOffset = nOptStarpakOffset;
		this->headDataSize = nSubHeaderSize;
		this->id = Type;
	}

	// hashed version of the asset path
	// used for referencing the asset from elsewhere
	//
	// - when referenced from other assets, the GUID is used directly
	// - when referenced from scripts, the GUID is calculated from the original asset path
	//   by a function such as RTech::StringToGuid
	uint64_t guid = 0;
	uint8_t  unk0[0x8]{};

	// page index and offset for where this asset's header is located
	uint32_t headIdx = 0;
	uint32_t headOffset = 0;

	// page index and offset for where this asset's data is located
	// note: this may not always be used for finding the data:
	//		 some assets use their own idx/offset pair from within the subheader
	//		 when adding pairs like this, you MUST register it as a descriptor
	//		 otherwise the pointer won't be converted
	uint32_t cpuIdx = 0;
	uint32_t cpuOffset = 0;

	// offset to any available streamed data
	// m_nStarpakOffset    = "mandatory" starpak file offset
	// m_nOptStarpakOffset = "optional" starpak file offset
	// 
	// in reality both are mandatory but respawn likes to do a little trolling
	// so "opt" starpaks are a thing
	uint64_t starpakOffset = -1;
	uint64_t optStarpakOffset = -1;

	uint16_t pageEnd = 0; // highest mem page used by this asset
	uint16_t unk1 = 0; // might be local "uses" + 1

	uint32_t relStartIdx = 0;

	uint32_t usesStartIdx = 0;
	uint32_t relationCount = 0;
	uint32_t usesCount = 0; // number of other assets that this asset uses

	// size of the asset header
	uint32_t headDataSize = 0;

	// this isn't always changed when the asset gets changed
	// but respawn calls it a version so i will as well
	uint32_t version = 0;

	// see AssetType enum below
	uint32_t id = 0;

	// internal

	// vector of indexes for local assets that use this asset
	std::vector<unsigned int> _relations{};

	inline void AddRelation(unsigned int idx) { _relations.push_back({ idx }); };

	std::vector<RPakGuidDescriptor> _guids{};

	inline void AddGuid(RPakGuidDescriptor desc) { _guids.push_back(desc); };

	inline void AddGuids(std::vector<RPakGuidDescriptor>* descs)
	{
		for (auto& it : *descs)
			_guids.push_back(it);
	};
};
#pragma pack(pop)

// internal data structure for referencing file data to be written
struct RPakRawDataBlock
{
	uint32_t m_nPageIdx;
	uint64_t m_nDataSize;
	uint8_t* m_nDataPtr;
};

// internal data structure for referencing streaming data to be written
struct SRPkDataEntry
{
	uint64_t m_nOffset = -1; // set when added
	uint64_t m_nDataSize = 0;
	uint8_t* m_nDataPtr = nullptr;
};

enum class AssetType : uint32_t
{
	TEXTURE = 0x72747874, // b'txtr' - texture
	RMDL = 0x5f6c646d,    // b'mdl_' - model
	UIMG = 0x676d6975,    // b'uimg' - ui image atlas
	PTCH = 0x68637450,	  // b'Ptch' - patch
	DTBL = 0x6c627464,    // b'dtbl' - datatable
	MATL = 0x6c74616d,    // b'matl' - material
};

// identifies the data type for each column in a datatable asset
enum class dtblcoltype_t : uint32_t
{
	Bool,
	Int,
	Float,
	Vector,
	StringT,
	Asset,
	AssetNoPrecache
};

//
//	Assets
//
#pragma pack(push, 1)
struct TextureHeader
{
	uint64_t guid = 0;
	RPakPtr  pName;

	uint16_t width = 0;
	uint16_t height = 0;

	uint16_t unk0 = 0;
	uint16_t imgFormat = 0;

	uint32_t dataSize; // total data size across all sources
	uint8_t  unk1;
	uint8_t  optStreamedMipLevels; // why is this here and not below? respawn moment

	// d3d11 texture desc params
	uint8_t  arraySize;
	uint8_t  layerCount;

	uint8_t  unk2;
	uint8_t  mipLevels;
	uint8_t  streamedMipLevels;
	uint8_t  unk3[0x15];
};

struct UIImageHeader
{
	float widthRatio; // 1 / m_nWidth
	float heightRatio; // 1 / m_nHeight
	uint16_t width = 1;
	uint16_t height = 1;
	uint16_t textureCount = 0;
	uint16_t unkCount = 0;
	RPakPtr pTextureOffsets{};
	RPakPtr pTextureDimensions{};
	uint32_t unk1 = 0;
	uint32_t unk2 = 0;
	RPakPtr pTextureHashes{};
	RPakPtr pTextureNames{};
	uint64_t atlasGUID = 0;
};

struct UIImageUV
{
	void InitUIImageUV(float startX, float startY, float width, float height)
	{
		this->uv0x = startX;
		this->uv1x = width;
		this->uv0y = startY;
		this->uv1y = height;
	}
	// maybe the uv coords for top left?
	// just leave these as 0 and it should be fine
	float uv0x = 0;
	float uv0y = 0;

	// these two seem to be the uv coords for the bottom right corner
	// examples:
	// uv1x = 10;
	// | | | | | | | | | |
	// uv1x = 5;
	// | | | | |
	float uv1x = 1.f;
	float uv1y = 1.f;
};

// examples of changes from these values: https://imgur.com/a/l1YDXaz
struct UIImageOffset
{

	void InitUIImageOffset(float startX, float startY, float endX, float endY)
	{
		this->startX = startX;
		this->startY = startY;
		this->endX = endX;
		this->endY = endY;
		//this->unkX = 1 - 2 * startX; // doesnt seem to always 100% of the time match up but its very close
		//this->unkY = 1 - 2 * startY;
	}
	// these don't seem to matter all that much as long as they are a valid float number
	float f0 = 0.f;
	float f1 = 0.f;

	// endX and endY define where the edge of the image is, with 1.f being the full length of the image and 0.5f being half of the image
	float endX = 1.f;
	float endY = 1.f;

	// startX and startY define where the top left corner is in proportion to the full image dimensions
	float startX = 0.f;
	float startY = 0.f;

	// changing these 2 values causes the image to be distorted on each axis
	float unkX = 1.f;
	float unkY = 1.f;
};

struct DataTableColumn
{
	RPakPtr NamePtr;
	dtblcoltype_t Type;
	uint32_t RowOffset;
};

struct DataTableHeader
{
	uint32_t ColumnCount;
	uint32_t RowCount;

	RPakPtr ColumnHeaderPtr;
	RPakPtr RowHeaderPtr;
	uint32_t UnkHash;

	uint16_t Un1 = 0;
	uint16_t Un2 = 0;

	uint32_t RowStride;	// Number of bytes per row
	uint32_t Padding;
};

struct DataTableColumnData
{
	dtblcoltype_t Type;
	bool bValue = 0;
	int iValue = -1;
	float fValue = -1;
	Vector3 vValue;
	std::string stringValue;
	std::string assetValue;
	std::string assetNPValue;
};

struct PtchHeader
{
	uint32_t unknown_1 = 255; // always FF 00 00 00?
	uint32_t patchedPakCount = 0;

	RPakPtr pPakNames;

	RPakPtr pPakPatchNums;
};

// size: 0x78 (120 bytes)
struct ModelHeader
{
	// IDST data
	// .mdl
	RPakPtr pRMDL;
	uint64_t Padding = 0;

	// model path
	// e.g. mdl/vehicle/goblin_dropship/goblin_dropship.rmdl
	RPakPtr pName;
	uint64_t Padding2 = 0;

	// .phy
	RPakPtr pPhyData;
	uint64_t Padding3 = 0;

	// preload cache data for static props
	RPakPtr pStaticPropVtxCache;

	// pointer to data for the model's arig guid(s?)
	RPakPtr pAnimRigs;

	// this is a guess based on the above ptr's data. i think this is == to the number of guids at where the ptr points to
	uint32_t animRigCount = 0;

	// size of the data kept in starpak
	uint32_t unkDataSize = 0;
	uint32_t alignedStreamingSize = 0; // full size of the starpak entry, aligned to 4096.

	uint64_t Padding6 = 0;

	// number of anim sequences directly associated with this model
	uint32_t animSeqCount = 0;
	RPakPtr pAnimSeqs;

	uint64_t Padding7 = 0;
	uint64_t Padding8 = 0;
	uint64_t Padding9 = 0;
};

// modified source engine studio mdl header struct
struct studiohdr_t
{
	int id; // Model format ID, such as "IDST" (0x49 0x44 0x53 0x54)
	int version; // Format version number, such as 48 (0x30,0x00,0x00,0x00)
	int checksum; // This has to be the same in the phy and vtx files to load!
	int sznameindex; // This has been moved from studiohdr2 to the front of the main header.
	char name[64]; // The internal name of the model, padding with null bytes.
	// Typically "my_model.mdl" will have an internal name of "my_model"
	int length; // Data size of MDL file in bytes.

	Vector3 eyeposition;	// ideal eye position

	Vector3 illumposition;	// illumination center

	Vector3 hull_min;		// ideal movement hull size
	Vector3 hull_max;

	Vector3 view_bbmin;		// clipping bounding box
	Vector3 view_bbmax;

	int flags;

	int numbones; // bones
	int boneindex;

	int numbonecontrollers; // bone controllers
	int bonecontrollerindex;

	int numhitboxsets;
	int hitboxsetindex;

	int numlocalanim; // animations/poses
	int localanimindex; // animation descriptions

	int numlocalseq; // sequences
	int	localseqindex;

	int activitylistversion; // initialization flag - have the sequences been indexed?

	// mstudiotexture_t
	// short rpak path
	// raw textures
	int materialtypesindex;
	int numtextures; // the material limit exceeds 128, probably 256.
	int textureindex;

	// this should always only be one, unless using vmts.
	// raw textures search paths
	int numcdtextures;
	int cdtextureindex;

	// replaceable textures tables
	int numskinref;
	int numskinfamilies;
	int skinindex;

	int numbodyparts;
	int bodypartindex;

	int numlocalattachments;
	int localattachmentindex;

	int numlocalnodes;
	int localnodeindex;
	int localnodenameindex;

	int numflexdesc;
	int flexdescindex;

	int meshindex; // SubmeshLodsOffset, might just be a mess offset

	int numflexcontrollers;
	int flexcontrollerindex;

	int numflexrules;
	int flexruleindex;

	int numikchains;
	int ikchainindex;

	// this is rui meshes
	int numruimeshes;
	int ruimeshindex;

	int numlocalposeparameters;
	int localposeparamindex;

	int surfacepropindex;

	int keyvalueindex;
	int keyvaluesize;

	int numlocalikautoplaylocks;
	int localikautoplaylockindex;

	float mass;
	int contents;

	// unused for packed models
	int numincludemodels;
	int includemodelindex;

	uint32_t virtualModel;

	int bonetablebynameindex;

	// if STUDIOHDR_FLAGS_CONSTANT_DIRECTIONAL_LIGHT_DOT is set,
	// this value is used to calculate directional components of lighting 
	// on static props
	byte constdirectionallightdot;

	// set during load of mdl data to track *desired* lod configuration (not actual)
	// the *actual* clamped root lod is found in studiohwdata
	// this is stored here as a global store to ensure the staged loading matches the rendering
	byte rootLOD;

	// set in the mdl data to specify that lod configuration should only allow first numAllowRootLODs
	// to be set as root LOD:
	//	numAllowedRootLODs = 0	means no restriction, any lod can be set as root lod.
	//	numAllowedRootLODs = N	means that lod0 - lod(N-1) can be set as root lod, but not lodN or lower.
	byte numAllowedRootLODs;

	byte unused;

	float fadedistance;

	float gathersize; // what. from r5r struct

	int numunk_v54_early;
	int unkindex_v54_early;

	int unk_v54[2];

	// this is in all shipped models, probably part of their asset bakery. it should be 0x2CC.
	int mayaindex; // doesn't actually need to be written pretty sure, only four bytes when not present.

	int numsrcbonetransform;
	int srcbonetransformindex;

	int	illumpositionattachmentindex;

	int linearboneindex;

	int m_nBoneFlexDriverCount; // unsure if that's what it is in apex
	int m_nBoneFlexDriverIndex;

	int unk1_v54[7];

	// always "" or "Titan"
	int unkstringindex;

	// this is now used for combined files in rpak, vtx, vvd, and vvc are all combined while vphy is separate.
	// the indexes are added to the offset in the rpak mdl_ header.
	// vphy isn't vphy, looks like a heavily modified vphy.
	int vtxindex; // VTX
	int vvdindex; // VVD / IDSV
	int vvcindex; // VVC / IDCV 
	int vphyindex; // VPHY / IVPS

	int vtxsize;
	int vvdsize;
	int vvcsize;
	int vphysize; // still used in models using vg

	// unk2_v54[3] is the chunk after following unkindex2's chunk
	int unk2_v54[3]; // the same four unks in v53 I think, the first index being unused now probably

	int unkindex3; // index to chunk after string block

	// likely related to AABB
	Vector3 mins; // min/max for Something
	Vector3 maxs; // seem to be the same as hull size

	int unk3_v54[3];

	int unkindex4; // chunk before unkindex3 sometimes

	int unk4_v54[3]; // same as unk3_v54_v121

	//int vgindex; // 0tVG
	//int unksize; // might be offset
	//int unksize1; // might be offset

};

// used for referencing a material from within a model
// pathoffset is the offset to the material's path (duh)
// guid is the material's asset guid (or 0 if it's a vmt, i think)
struct materialref_t
{
	uint32_t pathoffset;
	uint64_t guid;
};

// small struct to allow verification of the 0tVG section of starpak
// model data without needing to load the entire thing into memory for a simple
// validation check
struct BasicRMDLVGHeader
{
	uint32_t magic;
	uint32_t version;
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

	/* 0x78 */ uint32_t m_SomeFlags = 0x1D0300;
	/* 0x7C */ uint32_t m_Unknown2 = 0;

	/* 0x80 */ uint32_t m_Unknown3 = 0x1F5A92BD; // REQUIRED but why?

	/* 0x84 */ uint32_t m_Unknown4 = 0;

	// neither of these 2 seem to be required
	/* 0x88 */ uint32_t something = 0;
	/* 0x8C */ uint32_t something2 = 0;

	/* 0x90 */ UnknownMaterialSectionV15 m_UnknownSections[2]{};
	/* 0xF0 */ uint8_t bytef0;
	/* 0xF1 */ uint8_t bytef1;
	/* 0xF2 */ uint8_t bytef2;
	/* 0xF3 */ uint8_t bytef3; // used for unksections loading in UpdateMaterialAsset
	/* 0xF4 */ char pad_00F4[12];
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

struct MaterialCPUDataV12
{

	MaterialTextureTransformMatrix DetailTransform[1]; // detail texture transform matrix
	MaterialTextureTransformMatrix TextureTransform[2]; // 1st is presumably texture (unconfirmed), 2nd assumed to be texture.

	// this might be another texture transform matrix.
	float UnkFloat2[6] = {
		0.0, 0.0, 0.0, 0.0, 1.0, 0.0
	};

	Vector4 MainTint[1];

	// these are vector4s for rgba I would think.
	float UnkData1[12] = {

		0.0, 0.0, 0.0, 0.0,
		1.0, 0.0, 0.0, 0.0,
		10000, -0.0, 0.0, 1.778

	};

	Vector4 SelfillumTint[1];

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

	uint32_t Unk3 = 0; // might be padding but could also be something else.

	/* ImageFlags
	0x050300 for loadscreens, 0x1D0300 for normal materials.
	0x1D has been observed, seems to invert lighting? used on some exceptionally weird materials.*/
};

// header struct for the material asset cpu data
struct MaterialCPUHeader
{
	RPakPtr  m_nUnknownRPtr{}; // points to the rest of the cpu data. maybe for colour?
	uint32_t m_nDataSize = 0;
	uint32_t m_nVersionMaybe = 3; // every unknown is now either datasize, version, or flags
};

#pragma pack(pop)

// internal data structure for storing patch_master entries before being written
struct PtchEntry
{
	std::string FileName = "";
	uint8_t PatchNum = 0;
	uint32_t FileNamePageOffset = 0;
};

// map of dxgi format to the corresponding txtr asset format value
static std::map<DXGI_FORMAT, uint16_t> s_txtrFormatMap{
	{ DXGI_FORMAT_BC1_UNORM, 0 },
	{ DXGI_FORMAT_BC1_UNORM_SRGB, 1 },
	{ DXGI_FORMAT_BC2_UNORM, 2 },
	{ DXGI_FORMAT_BC2_UNORM_SRGB, 3 },
	{ DXGI_FORMAT_BC3_UNORM, 4 },
	{ DXGI_FORMAT_BC3_UNORM_SRGB, 5 },
	{ DXGI_FORMAT_BC4_UNORM, 6 },
	{ DXGI_FORMAT_BC4_SNORM, 7 },
	{ DXGI_FORMAT_BC5_UNORM, 8 },
	{ DXGI_FORMAT_BC5_SNORM, 9 },
	{ DXGI_FORMAT_BC6H_UF16, 10 },
	{ DXGI_FORMAT_BC6H_SF16, 11 },
	{ DXGI_FORMAT_BC7_UNORM, 12 },
	{ DXGI_FORMAT_BC7_UNORM_SRGB, 13 },
	{ DXGI_FORMAT_R32G32B32A32_FLOAT, 14 },
	{ DXGI_FORMAT_R32G32B32A32_UINT, 15 },
	{ DXGI_FORMAT_R32G32B32A32_SINT, 16 },
	{ DXGI_FORMAT_R32G32B32_FLOAT, 17 },
	{ DXGI_FORMAT_R32G32B32_UINT, 18 },
	{ DXGI_FORMAT_R32G32B32_SINT, 19 },
	{ DXGI_FORMAT_R16G16B16A16_FLOAT, 20 },
	{ DXGI_FORMAT_R16G16B16A16_UNORM, 21 },
	{ DXGI_FORMAT_R16G16B16A16_UINT, 22 },
	{ DXGI_FORMAT_R16G16B16A16_SNORM, 23 },
	{ DXGI_FORMAT_R16G16B16A16_SINT, 24 },
	{ DXGI_FORMAT_R32G32_FLOAT, 25 },
	{ DXGI_FORMAT_R32G32_UINT, 26 },
	{ DXGI_FORMAT_R32G32_SINT, 27 },
	{ DXGI_FORMAT_R10G10B10A2_UNORM, 28 },
	{ DXGI_FORMAT_R10G10B10A2_UINT, 29 },
	{ DXGI_FORMAT_R11G11B10_FLOAT, 30 },
	{ DXGI_FORMAT_R8G8B8A8_UNORM, 31 },
	{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 32 },
	{ DXGI_FORMAT_R8G8B8A8_UINT, 33 },
	{ DXGI_FORMAT_R8G8B8A8_SNORM, 34 },
	{ DXGI_FORMAT_R8G8B8A8_SINT, 35 },
	{ DXGI_FORMAT_R16G16_FLOAT, 36 },
	{ DXGI_FORMAT_R16G16_UNORM, 37 },
	{ DXGI_FORMAT_R16G16_UINT, 38 },
	{ DXGI_FORMAT_R16G16_SNORM, 39 },
	{ DXGI_FORMAT_R16G16_SINT, 40 },
	{ DXGI_FORMAT_R32_FLOAT, 41 },
	{ DXGI_FORMAT_R32_UINT, 42 },
	{ DXGI_FORMAT_R32_SINT, 43 },
	{ DXGI_FORMAT_R8G8_UNORM, 44 },
	{ DXGI_FORMAT_R8G8_UINT, 45 },
	{ DXGI_FORMAT_R8G8_SNORM, 46 },
	{ DXGI_FORMAT_R8G8_SINT, 47 },
	{ DXGI_FORMAT_R16_FLOAT, 48 },
	{ DXGI_FORMAT_R16_UNORM, 49 },
	{ DXGI_FORMAT_R16_UINT, 50 },
	{ DXGI_FORMAT_R16_SNORM, 51 },
	{ DXGI_FORMAT_R16_SINT, 52 },
	{ DXGI_FORMAT_R8_UNORM, 53 },
	{ DXGI_FORMAT_R8_UINT, 54 },
	{ DXGI_FORMAT_R8_SNORM, 55 },
	{ DXGI_FORMAT_R8_SINT, 56 },
	{ DXGI_FORMAT_A8_UNORM, 57 },
	{ DXGI_FORMAT_R9G9B9E5_SHAREDEXP, 58 },
	{ DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, 59 },
	{ DXGI_FORMAT_D32_FLOAT, 60 },
	{ DXGI_FORMAT_D16_UNORM, 61 },
};

#define SF_HEAD   0 // :skull:
#define SF_CPU    (1 << 0)
#define SF_TEMP   (1 << 1)
#define SF_SERVER (1 << 5)
#define SF_CLIENT (1 << 6)
#define SF_DEV    (1 << 7)
