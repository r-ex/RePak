#pragma once

#ifndef MAKE_FOURCC
#define MAKE_FOURCC(a,b,c,d) ((d<<24)+(c<<16)+(b<<8)+a)
#endif

#define STARPAK_FILE_MAGIC MAKE_FOURCC('S', 'R', 'P', 'k')

// starpak header
struct StarpakFileHeader_t
{
	int magic;
	int version;
};

// entry struct for the table at the end of starpak files
struct StarpakEntry_t
{
	size_t dataOffset;
	size_t dataSize;
};

// internal data structure for referencing streaming data to be written
struct StreamableDataEntry
{
	uint64_t offset;
	uint64_t dataSize;
	uint8_t* pData;
};