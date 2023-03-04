#pragma once

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
	std::uint64_t __fastcall StringToGuid(const char* pData);
	std::uint32_t __fastcall StringToUIMGHash(const char* str);
}