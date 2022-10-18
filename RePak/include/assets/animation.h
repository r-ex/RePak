#pragma once

#include <pch.h>
#include <assert.h>

void AddRseqListAsset_v7(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, std::string sAssetsDir, std::vector<std::string> AseqList);

enum mstudioseqflags : uint32_t
{
	STUDIO_LOOPING = 0x0001,	// ending frame should be the same as the starting frame
	STUDIO_SNAP = 0x0002,	// do not interpolate between previous animation and this one
	STUDIO_DELTA = 0x0004,	// this sequence "adds" to the base sequences, not slerp blends
	STUDIO_AUTOPLAY = 0x0008,	// temporary flag that forces the sequence to always play
	STUDIO_POST = 0x0010,	//
	STUDIO_ALLZEROS = 0x0020,	// this animation/sequence has no real animation data
	STUDIO_FRAMEANIM = 0x0040,	// animation is encoded as by frame x bone instead of RLE bone x frame
	STUDIO_CYCLEPOSE = 0x0080,	// cycle index is taken from a pose parameter index
	STUDIO_REALTIME = 0x0100,	// cycle index is taken from a real-time clock, not the animations cycle index
	STUDIO_LOCAL = 0x0200,	// sequence has a local context sequence
	STUDIO_HIDDEN = 0x0400,	// don't show in default selection views
	STUDIO_OVERRIDE = 0x0800,	// a forward declared sequence (empty)
	STUDIO_ACTIVITY = 0x1000,	// Has been updated at runtime to activity index
	STUDIO_EVENT = 0x2000,	// Has been updated at runtime to event index on server
	STUDIO_WORLD = 0x4000,	// sequence blends in worldspace
	STUDIO_NOFORCELOOP = 0x8000,	// do not force the animation loop
	STUDIO_EVENT_CLIENT = 0x10000	// Has been updated at runtime to event index on client
};

#pragma pack(push, 1)
// --- arig ---
struct AnimRigHeader
{
	RPakPtr pSkeleton{};

	RPakPtr pName{};

	uint32_t Unk1;
	uint32_t AseqRefCount;

	RPakPtr pAseqRefs{};

	uint64_t Padding;
};

// ANIMATIONS
// --- aseq ---
struct AnimHeader
{
	RPakPtr pAnimation{};

	RPakPtr pName{};

	RPakPtr pModelGuid{};

	uint32_t ModelCount = 0;
	uint32_t Reserved = 0;

	RPakPtr pSettings{};

	uint32_t SettingCount = 0;
	uint32_t Reserved1 = 0;
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

struct mstudioanimdescv54_t
{
	int baseptr;
	int sznameindex;

	float fps;

	mstudioseqflags flags;
	int numframes;

	int nummovements;
	int movementindex;

	int compressedikerrorindex;
	int animindex;

	int Flags2;
	int UnknownTableOffset;

	int sectionindex; // was chunk offset
	int FrameSplitCount;
	int FrameMedianCount;
	uint64_t Padding;
	uint64_t SomeDataOffset;
};

struct mstudioeventv54_t
{
	float cycle = 0.0;
	int	event = 0;
	int type = 0x400; // this will be 0 if old style I'd imagine
	char options[256];

	int szeventindex;
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

struct RAnimBoneFlag
{
	uint16_t Size : 12;
	uint16_t bAdditiveCustom : 1;
	uint16_t bDynamicScale : 1;			// If zero, one per data set
	uint16_t bDynamicRotation : 1;		// If zero, one per data set
	uint16_t bDynamicTranslation : 1;	// If zero, one per data set
};

struct RAnimTitanfallBoneFlag
{
	uint8_t Unused : 1;
	uint8_t bStaticTranslation : 1;		// If zero, one per data set
	uint8_t bStaticRotation : 1;		// If zero, one per data set
	uint8_t bStaticScale : 1;			// If zero, one per data set
	uint8_t Unused2 : 1;
	uint8_t Unused3 : 1;
	uint8_t Unused4 : 1;
};

struct RAnimBoneHeader
{
	float TranslationScale;

	uint8_t BoneIndex;
	RAnimBoneFlag BoneFlags;
	uint8_t Flags2;
	uint8_t Flags3;

	Quaternion Rotation;
	Vector3 Translation;
	Vector3 Scale;

	uint32_t DataSize;
};
#pragma pack(pop)

uint64_t CalculateAllocSizeForEvents(rapidjson::Value& mapEntry);

void AddCustomEventsToAseq(mstudioseqdesc_t& seqdesc, rmem& writer, uint64_t filenamedatasize, uint64_t eventoffset, rapidjson::Value& mapEntry);