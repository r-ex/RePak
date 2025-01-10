#pragma once
#include <filesystem>
#include <utils/utils.h>

#define STREAM_CACHE_FILE_MAGIC ('S'+('R'<<8)+('M'<<16)+('p'<<24))
#define STREAM_CACHE_FILE_MAJOR_VERSION 1
#define STREAM_CACHE_FILE_MINOR_VERSION 0

struct StreamCacheFileHeader_s
{
	int magic;

	unsigned short majorVersion;
	unsigned short minorVersion;

	size_t streamingFileCount;

	size_t dataEntryCount;
	size_t dataEntriesOffset;
};

struct StreamCacheDataEntry_s
{
	uint64_t pathIndex : 12;
	uint64_t dataOffset : 52;
	size_t dataSize;
	__m128i hash;
};

class CStreamCache
{
public:
	void BuildMapFromGamePaks(const char* const streamCacheFile);
	void ParseMap(const char* const streamCacheFile);

	uint64_t AddStarpakPathToCache(const std::string& path);
	StreamCacheFileHeader_s ConstructHeader() const;

private:
	std::vector<std::string> m_streamPaths;
	std::vector<StreamCacheDataEntry_s> m_dataEntries;

	size_t m_starpakBufSize;
};
