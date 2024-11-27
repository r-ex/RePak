#pragma once

// todo(amos): reorder headers so this can be moved into public.
typedef uint64_t PakGuid_t;

// contains the offset and size of a data entry within the starpak
// offset must be larger than 0x1000 (4096), as the beginning of 
// the file is filled with 0xCB until that point
// 
// there is an array of these structures at the end of the file that point to each data entry
// array size is equal to the value of the 64-bit unsigned integer in the last 8 bytes of the file
struct SRPkFileEntry
{
	uint64_t m_nOffset;
	uint64_t m_nSize;
};

namespace RTech
{
	PakGuid_t StringToGuid(const char* const string);
	std::uint32_t StringToUIMGHash(const char* const str);

	bool ParseGUIDFromString(const char* const str, PakGuid_t* const pGuid = nullptr);
	PakGuid_t GetAssetGUIDFromString(const char* const str, const bool forceRpakExtension = false);
}