#pragma once

#include <pch.h>

struct PtchHeader
{
	uint32_t unknown_1 = 255; // always FF 00 00 00?
	uint32_t patchedPakCount = 0;

	RPakPtr pPakNames;

	RPakPtr pPakPatchNums;
};

// internal data structure for storing patch_master entries before being written
struct PtchEntry
{
	std::string FileName = "";
	uint8_t PatchNum = 0;
	uint32_t FileNamePageOffset = 0;
};
