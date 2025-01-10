#include <pch.h>
#include <public/starpak.h>
#include <utils/utils.h>
#include <fstream>

#include <utils/MurmurHash3.h>
#include "streamcache.h"

static std::vector<std::string> StreamCache_GetStarpakFilesFromDirectory(const char* const directoryPath)
{
	std::vector<std::string> paths;
	for (auto& it : std::filesystem::directory_iterator(directoryPath))
	{
		const fs::path& entryPath = it.path();

		if (!entryPath.has_extension())
			continue;

		if (entryPath.extension() == ".starpak")
		{
			std::string newEntry = entryPath.string();

			// Normalize the slashes.
			for (char& c : newEntry)
			{
				if (c == '\\')
					c = '/';
			}

			paths.push_back(newEntry);
		}
	}

	return paths;
}

bool StreamCache_BuildMapFromGamePaks(const char* const directoryPath)
{
	fs::path starmapStream(directoryPath);
	starmapStream.append("pc_roots.starmap");

	const std::string starmapStreamStr = starmapStream.string();

	// open cache file at the start so we don't get thru the whole process and fail to write at the end
	BinaryIO cacheFileStream;

	if (!cacheFileStream.Open(starmapStreamStr, BinaryIO::Mode_e::Write))
		Error("Failed to create streaming map file \"%s\".\n", starmapStreamStr.c_str());

	const std::vector<std::string> foundStarpakPaths = StreamCache_GetStarpakFilesFromDirectory(directoryPath);

	Log("Found %zu streaming files to cache in directory \"%s\".\n", foundStarpakPaths.size(), starmapStreamStr.c_str());

	// Start a timer for the cache builder process so that the user is notified when the tool finishes.
	TIME_SCOPE("StreamCacheBuilder");

	CStreamCacheBuilder cacheFile;
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

		const size_t starpakPathOffset = cacheFile.AddStarpakPathToCache(relativeStarpakPath);

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

			StreamCacheDataEntry_s cacheEntry = {};
			cacheEntry.starpakPathOffset = starpakPathOffset;
			cacheEntry.dataOffset = entryHeader->offset;
			cacheEntry.dataSize = entryHeader->dataSize;

			// ideally we don't have entries over 2gb.
			assert(entryHeader->dataSize < INT32_MAX);

			MurmurHash3_x64_128(entryData, static_cast<int>(entryHeader->dataSize), 0x165DCA75, &cacheEntry.hash);

			cacheFile.m_cachedDataEntries.push_back(cacheEntry);

			_aligned_free(entryData);
		}

		starpakIndex++;
	}

	StreamCacheFileHeader_s cacheHeader = cacheFile.ConstructHeader();

	cacheFileStream.Write(cacheHeader);

	for (const std::string& it : cacheFile.m_cachedStarpaks)
	{
		cacheFileStream.WriteString(it, true);
	}
	
	cacheFileStream.Seek(cacheHeader.dataEntriesOffset);

	for (const StreamCacheDataEntry_s& dataEntry : cacheFile.m_cachedDataEntries)
	{
		cacheFileStream.Write(dataEntry);
	}

	cacheFileStream.Close();

	return true;
}
