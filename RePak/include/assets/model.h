#pragma once

#include <pch.h>

#pragma pack(push, 2)
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

struct RMDLVTXHeader
{
	// file version as defined by OPTIMIZED_MODEL_FILE_VERSION (currently 7)
	int version;

	// hardware params that affect how the model is to be optimized.
	int vertCacheSize;
	uint16_t maxBonesPerStrip;
	uint16_t maxBonesPerTri;
	int maxBonesPerVert;

	// must match checkSum in the .mdl
	int checksum;

	int numLODs; // Also specified in ModelHeader_t's and should match

	// Offset to materialReplacementList Array. one of these for each LOD, 8 in total
	int materialReplacementListOffset;

	// Defines the size and location of the body part array
	int numBodyParts;
	int bodyPartOffset;
};
#pragma pack(pop)