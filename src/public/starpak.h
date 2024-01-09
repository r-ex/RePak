#pragma once

// starpak header
struct StreamableSetHeader
{
	int magic;
	int version;
};

// internal data structure for referencing streaming data to be written
struct StreamableDataEntry
{
	uint64_t offset;
	uint64_t dataSize;
	uint8_t* pData;
};