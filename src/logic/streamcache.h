#pragma once
#include <filesystem>
#include <utils/utils.h>

#define STREAM_CACHE_FILE_MAGIC ('S'+('R'<<8)+('M'<<16)+('p'<<24))
#define STREAM_CACHE_FILE_MAJOR_VERSION 0
#define STREAM_CACHE_FILE_MINOR_VERSION 0

#pragma pack(push, 1)
struct StreamCacheFileHeader_s
{
	int magic;

	unsigned short majorVersion;
	unsigned short minorVersion;

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
	CStreamCacheBuilder() : m_starpakBufSize(0) {};

	// returns offset in starpak paths buffer to this path
	size_t AddStarpakPathToCache(const std::string& path)
	{
		this->m_cachedStarpaks.push_back(path);

		assert(this->m_cachedStarpaks.size() > 0 && this->m_cachedStarpaks.size() <= UINT32_MAX);

		size_t pathOffset = this->m_starpakBufSize;

		// add this path to the starpak buffer size
		this->m_starpakBufSize += path.length() + 1;

		return pathOffset;
	}

	StreamCacheFileHeader_s ConstructHeader() const
	{
		StreamCacheFileHeader_s fileHeader;

		fileHeader.magic = STREAM_CACHE_FILE_MAGIC;
		fileHeader.majorVersion = STREAM_CACHE_FILE_MAJOR_VERSION;
		fileHeader.minorVersion = STREAM_CACHE_FILE_MINOR_VERSION;
		fileHeader.starpakPathBufferSize = m_starpakBufSize;
		fileHeader.dataEntryCount = m_cachedDataEntries.size();
		fileHeader.dataEntriesOffset = IALIGN4(sizeof(StreamCacheFileHeader_s) + m_starpakBufSize);

		return fileHeader;
	}

	size_t m_starpakBufSize;

	// vector of starpak paths relative to game root
	// (i.e., paks/Win64/(name).starpak)
	std::vector<std::string> m_cachedStarpaks;

	std::vector<StreamCacheDataEntry_s> m_cachedDataEntries;
};

bool StreamCache_BuildMapFromGamePaks(const char* const directoryPath);
