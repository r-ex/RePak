#pragma once
#include "math/vector.h"

#pragma pack(push, 1)
// used for referencing a material from within a model
// pathoffset is the offset to the material's path (duh)
// guid is the material's asset guid (or 0 if it's a vmt, i think)
struct mstudiotexture_t
{
	uint32_t pathoffset;
	uint64_t guid;
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

	inline bool IsStaticProp() { return flags & 0x10; };

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

	inline mstudiotexture_t* pTexture(int i)
	{
		return reinterpret_cast<mstudiotexture_t*>((char*)this + textureindex) + i;
	}

	inline uint8_t materialType(int i)
	{
		return reinterpret_cast<uint8_t*>((char*)this + materialtypesindex)[i];
	}

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

struct mstudioseqdesc_t
{
	int baseptr;

	int	szlabelindex;

	int szactivitynameindex;

	int flags; // looping/non-looping flags

	int activity; // initialized at loadtime to game DLL values
	int actweight;

	int numevents;
	int eventindex;

	Vector3 bbmin; // per sequence bounding box
	Vector3 bbmax;

	int numblends;

	// Index into array of shorts which is groupsize[0] x groupsize[1] in length
	int animindexindex;

	int movementindex; // [blend] float array for blended movement
	int groupsize[2];
	int paramindex[2]; // X, Y, Z, XR, YR, ZR
	float paramstart[2]; // local (0..1) starting value
	float paramend[2]; // local (0..1) ending value
	int paramparent;

	float fadeintime; // ideal cross fate in time (0.2 default)
	float fadeouttime; // ideal cross fade out time (0.2 default)

	int localentrynode; // transition node at entry
	int localexitnode; // transition node at exit
	int nodeflags; // transition rules

	float entryphase; // used to match entry gait
	float exitphase; // used to match exit gait

	float lastframe; // frame that should generation EndOfSequence

	int nextseq; // auto advancing sequences
	int pose; // index of delta animation between end and nextseq

	int numikrules;

	int numautolayers;
	int autolayerindex;

	int weightlistindex;

	int posekeyindex;

	int numiklocks;
	int iklockindex;

	// Key values
	int keyvalueindex;
	int keyvaluesize;

	int cycleposeindex; // index of pose parameter to use as cycle index

	int activitymodifierindex;
	int numactivitymodifiers;

	int unk;
	int unk1;

	int unkindex;

	int unk2;
};

struct mstudioautolayer_t
{
	uint64_t guid; // hashed aseq guid asset

	short iSequence;
	short iPose;

	int flags;
	float start; // beginning of influence
	float peak;	 // start of full influence
	float tail;	 // end of full influence
	float end;	 // end of all influence
};

struct AnimSeqAssetHeader_t
{
	PagePtr_t data; // pointer to raw rseq.
	PagePtr_t szname; // pointer to debug name, placed before raw rseq normally.

	// this can point to a group of guids and not one singular one.
	PagePtr_t pModels;
	uint32_t modelCount;

	uint32_t padding_0; // aligns next member to 8 bytes

	PagePtr_t pSettings; // points to an array of settings asset guids
	uint32_t settingsCount;

	uint32_t padding_1; // aligns full struct to 8 bytes
};

// size: 0x78 (120 bytes)
struct ModelAssetHeader_t
{
	// IDST data
	// .rmdl
	PagePtr_t pData;
	uint64_t Padding = 0;

	// model path
	// e.g. mdl/vehicle/goblin_dropship/goblin_dropship.rmdl
	PagePtr_t pName;
	uint64_t Padding2 = 0;

	// .phy
	PagePtr_t pPhyData;
	uint64_t Padding3 = 0;

	// preload cache data for static props
	PagePtr_t pStaticPropVtxCache;

	// pointer to data for the model's arig guid(s?)
	PagePtr_t pAnimRigs;

	// this is a guess based on the above ptr's data. i think this is == to the number of guids at where the ptr points to
	uint32_t animRigCount = 0;

	// size of the data kept in starpak
	uint32_t unkDataSize = 0;
	uint32_t alignedStreamingSize = 0; // full size of the starpak entry, aligned to 4096.

	uint64_t Padding6 = 0;

	// number of anim sequences directly associated with this model
	uint32_t sequenceCount = 0;
	PagePtr_t pSequences;

	uint64_t Padding7 = 0;
	uint64_t Padding8 = 0;
	uint64_t Padding9 = 0;
};
static_assert(sizeof(ModelAssetHeader_t) == 120);

struct VertexGroupHeader_t
{
	int id;		        // 0x47567430	'0tVG'
	int version;	    // 0x1
	int unk;	        // Usually 0
	int dataSize;	    // Total size of data + header in starpak

	__int64 boneStateChangeOffset; // offset to bone remap buffer
	__int64 numBoneStateChanges;   // number of "bone remaps" (size: 1)

	__int64 meshOffset;            // offset to mesh buffer
	__int64 numMeshes;             // number of meshes (size: 0x48)

	__int64 indexOffset;           // offset to index buffer
	__int64 numIndices;            // number of indices (size: 2 (uint16_t))

	__int64 vertOffset;            // offset to vertex buffer
	__int64 vertDataSize;          // number of bytes in vertex buffer

	__int64 externalWeightOffset;  // offset to extended weights buffer
	__int64 externalWeightsSize;   // number of bytes in extended weights buffer

	// there is one for every LOD mesh
	// i.e, unknownCount == lod.meshCount for all LODs
	__int64 unknownOffset;         // offset to buffer
	__int64 numUnknown;            // count (size: 0x30)

	__int64 lodOffset;             // offset to LOD buffer
	__int64 numLODs;               // number of LODs (size: 0x8)

	__int64 legacyWeightOffset;	   // seems to be an offset into the "external weights" buffer for this mesh
	__int64 numLegacyWeights;      // seems to be the number of "external weights" that this mesh uses

	__int64 stripOffset;           // offset to strips buffer
	__int64 numStrips;             // number of strips (size: 0x23)

	int unused[16];
};
#pragma pack(pop)