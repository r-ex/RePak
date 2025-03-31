#pragma once

#define STARPAK_MAGIC (('k'<<24)+('P'<<16)+('R'<<8)+'S')
#define STARPAK_VERSION 1
#define STARPAK_EXTENSION ".starpak"

// data blocks in starpaks are all aligned to 4096 bytes, including
// the header which gets filled with 0xCB after the magic and version
#define STARPAK_DATABLOCK_ALIGNMENT 4096
#define STARPAK_DATABLOCK_ALIGNMENT_PADDING 0xCB

// starpak header
struct PakStreamSetFileHeader_s
{
	int magic;
	int version;
};

// contains the offset and size of a data entry within the starpak
// offset must be larger than 0x1000 (4096), as the beginning of 
// the file is filled with 0xCB until that point
// 
// there is an array of these structures at the end of the file that point to each data entry
// array size is equal to the value of the 64-bit unsigned integer in the last 8 bytes of the file
struct PakStreamSetEntry_s
{
	uint64_t offset;
	uint64_t dataSize;
};

enum PakStreamSet_e
{
	STREAMING_SET_MANDATORY = 0,
	STREAMING_SET_OPTIONAL,

	// number of streaming sets
	STREAMING_SET_COUNT
};

static inline const char* Pak_StreamSetToName(const PakStreamSet_e set)
{
	switch (set)
	{
	case STREAMING_SET_MANDATORY: return "mandatory";
	case STREAMING_SET_OPTIONAL: return "optional";
	}

	return "invalid";
}
