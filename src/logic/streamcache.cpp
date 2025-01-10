#include <pch.h>
#include <public/starpak.h>
#include <utils/utils.h>
#include <fstream>

#include <utils/MurmurHash3.h>
#include "streamcache.h"
#include <public/rpak.h>

static std::vector<std::string> StreamCache_GetStarpakFilesFromDirectory(const char* const directoryPath)
{
	std::vector<std::string> paths;
	for (auto& it : std::filesystem::directory_iterator(directoryPath))
	{
		const fs::path& entryPath = it.path();

		if (!entryPath.has_extension())
			continue;

		if (entryPath.extension() != ".starpak")
			continue;

		std::string newEntry = entryPath.string();

		// Normalize the slashes.
		for (char& c : newEntry)
		{
			if (c == '\\')
				c = '/';
		}

		paths.push_back(newEntry);
	}

	return paths;
}

void CStreamCache::BuildMapFromGamePaks(const char* const streamCacheFile)
{
	std::string directoryPath(streamCacheFile);
	directoryPath = directoryPath.substr(0, directoryPath.find_last_of("\\/"));

	// open cache file at the start so we don't get thru the whole process and fail to write at the end
	BinaryIO cacheFileStream;

	if (!cacheFileStream.Open(streamCacheFile, BinaryIO::Mode_e::Write))
		Error("Failed to create streaming map file \"%s\".\n", streamCacheFile);

	const std::vector<std::string> foundStarpakPaths = StreamCache_GetStarpakFilesFromDirectory(directoryPath.c_str());

	Log("Found %zu streaming files to cache in directory \"%s\".\n", foundStarpakPaths.size(), streamCacheFile);

	// Start a timer for the cache builder process so that the user is notified when the tool finishes.
	TIME_SCOPE("StreamCacheBuilder");
	size_t starpakIndex = 0;

	for (const std::string& starpakPath : foundStarpakPaths)
	{
		BinaryIO starpakStream;

		if (!starpakStream.Open(starpakPath, BinaryIO::Mode_e::Read))
		{
			Warning("Failed to open streaming file \"%s\" for reading.\n", starpakPath.c_str());
			continue;
		}

		PakStreamSetFileHeader_s starpakFileHeader = starpakStream.Read<PakStreamSetFileHeader_s>();

		if (starpakFileHeader.magic != STARPAK_MAGIC)
		{
			Warning("Streaming file \"%s\" has an invalid file magic; expected %X, found %X.\n", starpakPath.c_str(), starpakFileHeader.magic, STARPAK_MAGIC);
			continue;
		}

		Log("Adding streaming file \"%s\" (%zu/%zu) to the cache.\n", starpakPath.c_str(), starpakIndex + 1, foundStarpakPaths.size());

		const size_t starpakFileSize = fs::file_size(starpakPath);

		starpakStream.Seek(starpakFileSize - sizeof(uint64_t), std::ios::beg);

		// get the number of data entries in this starpak file
		const size_t starpakEntryCount = starpakStream.Read<size_t>();
		const size_t starpakEntryHeadersSize = sizeof(PakStreamSetEntry_s) * starpakEntryCount;

		std::unique_ptr<PakStreamSetEntry_s> starpakEntryHeaders = std::unique_ptr<PakStreamSetEntry_s>(new PakStreamSetEntry_s[starpakEntryCount]);

		// go to the start of the entry structs
		starpakStream.Seek(starpakFileSize - (8 + starpakEntryHeadersSize), std::ios::beg);
		starpakStream.Read(reinterpret_cast<char*>(starpakEntryHeaders.get()), starpakEntryHeadersSize);

		const char* starpakFileName = strrchr(starpakPath.c_str(), '/');

		if (starpakFileName)
			starpakFileName += 1; // Skip the '/'.
		else
			starpakFileName = starpakPath.c_str();

		std::string relativeStarpakPath("paks/Win64/");
		relativeStarpakPath.append(starpakFileName);

		const size_t pathIndex = AddStarpakPathToCache(relativeStarpakPath);

		for (size_t i = 0; i < starpakEntryCount; ++i)
		{
			const PakStreamSetEntry_s* entryHeader = &starpakEntryHeaders.get()[i];

			if (entryHeader->dataSize == 0) [[unlikely]] // not possible
				continue;
			
			if (entryHeader->offset < STARPAK_DATABLOCK_ALIGNMENT) [[unlikely]] // also not possible
				continue;

			char* const entryData = reinterpret_cast<char*>(_aligned_malloc(entryHeader->dataSize, 8));

			starpakStream.Seek(entryHeader->offset, std::ios::beg);
			starpakStream.Read(entryData, entryHeader->dataSize);

			StreamCacheDataEntry_s cacheEntry;
			cacheEntry.pathIndex = pathIndex;
			cacheEntry.dataOffset = entryHeader->offset;
			cacheEntry.dataSize = entryHeader->dataSize;

			// ideally we don't have entries over 2gb.
			assert(entryHeader->dataSize < INT32_MAX);

			MurmurHash3_x64_128(entryData, static_cast<int>(entryHeader->dataSize), 0x165DCA75, &cacheEntry.hash);

			m_dataEntries.push_back(cacheEntry);

			_aligned_free(entryData);
		}

		starpakIndex++;
	}

	StreamCacheFileHeader_s cacheHeader = ConstructHeader();

	cacheFileStream.Write(cacheHeader);

	for (const std::string& it : m_streamPaths)
	{
		cacheFileStream.WriteString(it, true);
	}
	
	cacheFileStream.Seek(cacheHeader.dataEntriesOffset);

	for (const StreamCacheDataEntry_s& dataEntry : m_dataEntries)
	{
		cacheFileStream.Write(dataEntry);
	}

	cacheFileStream.Close();

	return;
}

void CStreamCache::ParseMap(const char* const streamCacheFile)
{
	BinaryIO cacheFileStream;

	if (!cacheFileStream.Open(streamCacheFile, BinaryIO::Mode_e::Read))
		Error("Failed to open streaming map file \"%s\".\n", streamCacheFile);

	const size_t streamMapSize = cacheFileStream.GetSize();

	if (streamMapSize < sizeof(StreamCacheFileHeader_s))
		Error("Streaming map file \"%s\" appears truncated (%zu < %zu).\n", streamCacheFile, streamMapSize, sizeof(StreamCacheFileHeader_s));

	StreamCacheFileHeader_s streamCacheHeader;
	cacheFileStream.Read(streamCacheHeader);

	if (streamCacheHeader.magic != STREAM_CACHE_FILE_MAGIC)
		Error("Streaming map file \"%s\" has bad magic (expected magic %x, got %x).\n", streamCacheFile, STREAM_CACHE_FILE_MAGIC, streamCacheHeader.magic);

	if (streamCacheHeader.majorVersion != STREAM_CACHE_FILE_MAJOR_VERSION ||
		streamCacheHeader.minorVersion != STREAM_CACHE_FILE_MINOR_VERSION)
	{
		Error("Streaming map file \"%s\" is unsupported (expected version %hu.%hu, got %hu.%hu).\n", 
			streamCacheFile, STREAM_CACHE_FILE_MAJOR_VERSION, STREAM_CACHE_FILE_MINOR_VERSION,
			streamCacheHeader.majorVersion, streamCacheHeader.minorVersion);
	}

	// Make sure the file contains as much as what the header says.
	const size_t actualBlockSize = streamMapSize - streamCacheHeader.dataEntriesOffset;
	const size_t expectedSize = streamCacheHeader.dataEntryCount * sizeof(StreamCacheDataEntry_s);

	if (actualBlockSize < expectedSize)
	{
		Error("Streaming map file \"%s\" appears malformed (actualBlockSize(%zu) < expectedSize(%zu)).\n", streamCacheFile,
			streamMapSize, expectedSize);
	}

	// Page the data in.
	m_streamPaths.resize(streamCacheHeader.streamingFileCount);

	for (size_t i = 0; i < streamCacheHeader.streamingFileCount; i++)
		cacheFileStream.ReadString(m_streamPaths[i]);

	m_dataEntries.resize(streamCacheHeader.dataEntryCount);

	cacheFileStream.SeekGet(streamCacheHeader.dataEntriesOffset);
	cacheFileStream.Read(m_dataEntries.data(), actualBlockSize);
}

uint64_t CStreamCache::AddStarpakPathToCache(const std::string& path)
{
	const uint64_t index = m_streamPaths.size();
	m_streamPaths.push_back(path);

	// We store the count into 12 bits, so we cannot have more than 4096.
	// Realistically, we shouldn't ever exceed this as we should only
	// deduplicate data across the 'all' and 'roots' starpaks.
	assert(m_streamPaths.size() <= 4096);

	// add this path to the starpak buffer size
	m_starpakBufSize += path.length() + 1;
	return index;
}

StreamCacheFileHeader_s CStreamCache::ConstructHeader() const
{
	StreamCacheFileHeader_s fileHeader;

	fileHeader.magic = STREAM_CACHE_FILE_MAGIC;
	fileHeader.majorVersion = STREAM_CACHE_FILE_MAJOR_VERSION;
	fileHeader.minorVersion = STREAM_CACHE_FILE_MINOR_VERSION;
	fileHeader.streamingFileCount = m_streamPaths.size();
	fileHeader.dataEntryCount = m_dataEntries.size();
	fileHeader.dataEntriesOffset = IALIGN4(sizeof(StreamCacheFileHeader_s) + m_starpakBufSize);

	return fileHeader;
}
