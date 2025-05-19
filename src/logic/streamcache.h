#pragma once
#include <filesystem>
#include <utils/utils.h>

#define STREAM_CACHE_FILE_MAGIC ('S'+('R'<<8)+('M'<<16)+('p'<<24))
#define STREAM_CACHE_FILE_MAJOR_VERSION 2
#define STREAM_CACHE_FILE_MINOR_VERSION 3

struct StreamCacheFileHeader_s
{
	int magic;

	unsigned short majorVersion;
	unsigned short minorVersion;

	size_t streamingFileCount;

	size_t dataEntryCount;
	size_t dataEntriesOffset;
};

struct StreamCacheFileEntry_s
{
	bool isOptional;
	std::string streamFilePath;
};

struct StreamCacheDataEntry_s
{
	int64_t dataOffset : 52;
	int64_t pathIndex : 12;
	int64_t dataSize;
	__m128i hash;
};

struct StreamCacheFindParams_s
{
	__m128i hash;
	int64_t size;
	const char* streamFilePath;
};

struct StreamCacheFindResult_s
{
	const StreamCacheFileEntry_s* fileEntry;
	const StreamCacheDataEntry_s* dataEntry;
};

class CStreamCache
{
public:
	void BuildMapFromGamePaks(const char* const streamCacheFile);
	void ParseMap(const char* const streamCacheFile);

	int64_t AddStarpakPathToCache(const std::string& path, const bool optional);
	StreamCacheFileHeader_s ConstructHeader() const;

	static StreamCacheFindParams_s CreateParams(const uint8_t* const data, const int64_t size, const char* const streamFilePath);

	bool Find(const StreamCacheFindParams_s& params, StreamCacheFindResult_s& result, const bool optional);
	void Add(const StreamCacheFindParams_s& params, const int64_t offset, const bool optional);

	void Save(BinaryIO& io);

private:
	std::vector<StreamCacheFileEntry_s> m_streamFiles;
	std::vector<StreamCacheDataEntry_s> m_dataEntries;
};
