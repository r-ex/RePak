#pragma once

#ifndef MAKE_FOURCC
#define MAKE_FOURCC(a,b,c,d) ((d<<24)+(c<<16)+(b<<8)+a)
#endif

#define STARPAK_FILE_MAGIC MAKE_FOURCC('S', 'R', 'P', 'k')

// starpak header
struct StreamableSetHeader
{
	int magic;
	int version;
};

// internal data structure for referencing streaming data to be written
struct StreamableDataEntry
{
	uint64_t offset = -1; // set when added
	uint64_t dataSize = 0;
	uint8_t* pData = nullptr;
};