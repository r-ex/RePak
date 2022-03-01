#pragma once

struct DDS_PIXELFORMAT {
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC; // int so it can be compared properly
	uint32_t RGBBitCount;
	uint32_t RBitMask;
	uint32_t GBitMask;
	uint32_t BBitMask;
	uint32_t ABitMask;
};

struct DDS_HEADER {
	uint32_t size;
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitchOrLinearSize;
	uint32_t depth;
	uint32_t mipMapCount;
	uint32_t reserved1[11];
	DDS_PIXELFORMAT pixelfmt;
	uint32_t caps;
	uint32_t caps2;
	uint32_t caps3;
	uint32_t caps4;
	uint32_t reserved2;
} dds_header;

struct SRPkFileEntry
{
	uint64_t offset;
	uint64_t size;
};


namespace RTech
{
	std::uint64_t __fastcall StringToGuid(const char* pData);
	std::uint32_t __fastcall StringToUIMGHash(const char* str);
}

