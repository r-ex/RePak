#pragma once
#include <filesystem>
#include <utils/utils.h>

#pragma pack(push, 1)
struct StreamCacheFileHeader_s
{
	size_t starpakPathBufferSize;

	size_t dataEntryCount;
	size_t dataEntriesOffset;
};
#pragma pack(pop)

struct StreamCacheDataEntry_s
{
	size_t dataOffset;
	size_t dataSize;

	__m128i hash;

	size_t starpakPathOffset;
};
static_assert(sizeof(StreamCacheDataEntry_s) == 0x30);

class CStreamCacheBuilder
{
public:
	CStreamCacheBuilder() : starpakBufSize(0) {};

	// returns offset in starpak paths buffer to this path
	size_t AddStarpakPathToCache(const std::string& path)
	{
		this->cachedStarpaks.push_back(path);

		assert(this->cachedStarpaks.size() > 0 && this->cachedStarpaks.size() <= UINT32_MAX);

		size_t pathOffset = this->starpakBufSize;

		// add this path to the starpak buffer size
		this->starpakBufSize += path.length() + 1;

		return pathOffset;
	}

	StreamCacheFileHeader_s ConstructHeader() const
	{
		StreamCacheFileHeader_s fileHeader = {};
		fileHeader.starpakPathBufferSize = starpakBufSize;
		fileHeader.dataEntryCount = cachedDataEntries.size();
		fileHeader.dataEntriesOffset = IALIGN4(sizeof(StreamCacheFileHeader_s) + starpakBufSize);

		return fileHeader;
	}

	size_t starpakBufSize;

	// vector of starpak paths relative to game root
	// (i.e., paks/Win64/(name).starpak)
	std::vector<std::string> cachedStarpaks;

	std::vector<StreamCacheDataEntry_s> cachedDataEntries;
};

bool StreamCache_BuildFromGamePaks(const std::filesystem::path& directoryPath);
