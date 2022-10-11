#pragma once

#include <pch.h>
#include <assert.h>

#pragma pack(push, 2)


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
	int baseptr = 0;
	int szlabelindex;
	int szactivitynameindex;
	int flags;

	int activity;
	int actweight;

	int numevents;
	int eventindex;

	Vector3 bbmin;
	Vector3 bbmax;

	int numblends;
	int animindexindex;
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
