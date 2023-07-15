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
	uint64_t offset = -1; // set when added
	uint64_t dataSize = 0;
	uint8_t* pData = nullptr;
};