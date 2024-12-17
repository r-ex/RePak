#include <pch.h>
#include <public/starpak.h>
#include <utils/utils.h>
#include <fstream>

#include <utils/MurmurHash3.h>

REPAK_BEGIN_NAMESPACE(CacheBuilder)

#pragma pack(push, 1)
struct CacheFileHeader_t
{
	size_t starpakPathBufferSize;

	size_t dataEntryCount;
	size_t dataEntriesOffset;
};
#pragma pack(pop)

struct CacheDataEntry_t
{
	size_t dataOffset;
	size_t dataSize;

	__m128i hash;

	size_t starpakPathOffset;
};
static_assert(sizeof(CacheDataEntry_t) == 0x30);

class CCacheFile
{
public:
	CCacheFile() : starpakBufSize(0) {};

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

	CacheFileHeader_t ConstructHeader() const 
	{
		CacheFileHeader_t fileHeader = {};
		fileHeader.starpakPathBufferSize = starpakBufSize;
		fileHeader.dataEntryCount = cachedDataEntries.size();
		fileHeader.dataEntriesOffset = IALIGN4(sizeof(CacheFileHeader_t) + starpakBufSize);

		return fileHeader;
	}

	size_t starpakBufSize;

	// vector of starpak paths relative to game root
	// (i.e., paks/Win64/(name).starpak)
	std::vector<std::string> cachedStarpaks;

	std::vector<CacheDataEntry_t> cachedDataEntries;
};

std::vector<fs::path> GetStarpakFilesFromDirectory(const fs::path& directoryPath)
{
	std::vector<fs::path> paths;
	for (auto& it : std::filesystem::directory_iterator(directoryPath))
	{
		const fs::path& entryPath = it.path();

		if (!entryPath.has_extension())
			continue;

		if (entryPath.extension() == ".starpak")
			paths.push_back(entryPath);
	}

	return paths;
}

bool BuildCacheFileFromGamePaksDirectory(const fs::path& directoryPath)
{
	// ensure that our directory path is both a path and a directory
	if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
		return false;

	// open cache file at the start so we don't get thru the whole process and fail to write at the end
	BinaryIO cacheFileStream;
	cacheFileStream.Open((directoryPath / "repak_starpak_cache.bin").string(), BinaryIO::Mode_e::Write);

	if (!cacheFileStream.IsWritable())
	{
		Warning("CacheBuilder: Failed to open cache file '%s' for writing.\n", (directoryPath / "repak_starpak_cache.bin").u8string().c_str());
		return false;
	}

	const std::unique_ptr<CCacheFile> cacheFile = std::make_unique<CCacheFile>();
	const std::vector<fs::path> foundStarpakPaths = GetStarpakFilesFromDirectory(directoryPath);

	Log("CacheBuilder: Found %lld streaming files to cache in directory \"%s\".\n", foundStarpakPaths.size(), directoryPath.string().c_str());

	// Start a timer for the cache builder process so that the user is notified when the tool finishes.
	TIME_SCOPE("CacheBuilder");

	size_t starpakIndex = 0;
	for (const fs::path& starpakPath : foundStarpakPaths)
	{
#if _DEBUG
		//printf("\n");
		//TIME_SCOPE(starpakPath.u8string().c_str());
		//Debug("CacheBuilder: Opening StarPak file '%s' (%lld/%lld) for reading.\n", starpakPath.u8string().c_str(), starpakIndex+1, foundStarpakPaths.size());
#endif

		Log("CacheBuilder: (%03lld/%lld) Adding streaming file \"%s\" to the cache.\n", starpakIndex+1, foundStarpakPaths.size(), starpakPath.string().c_str());

		const std::string relativeStarpakPath = ("paks/Win64/" / starpakPath.stem()).string();
		const size_t starpakPathOffset = cacheFile->AddStarpakPathToCache(relativeStarpakPath);

		BinaryIO starpakStream;
		starpakStream.Open(starpakPath.string(), BinaryIO::Mode_e::Read);

		if (!starpakStream.IsReadable())
		{
			Warning("CacheBuilder: Failed to open StarPak file '%s' for reading.\n", starpakPath.u8string().c_str());
			continue;
		}

		PakStreamSetFileHeader_s starpakFileHeader = starpakStream.Read<PakStreamSetFileHeader_s>();

		if (starpakFileHeader.magic != STARPAK_MAGIC)
		{
			Warning("CacheBuilder: StarPak file '%s' had invalid file magic; found %X, expected %X.\n", starpakPath.u8string().c_str(), starpakFileHeader.magic, STARPAK_MAGIC);
			continue;
		}

		const size_t starpakFileSize = fs::file_size(starpakPath);

		starpakStream.Seek(starpakFileSize - 8, std::ios::beg);

		// get the number of data entries in this starpak file
		const size_t starpakEntryCount = starpakStream.Read<size_t>();
		const size_t starpakEntryHeadersSize = sizeof(PakStreamSetEntry_s) * starpakEntryCount;

		std::unique_ptr<PakStreamSetEntry_s> starpakEntryHeaders = std::unique_ptr<PakStreamSetEntry_s>(new PakStreamSetEntry_s[starpakEntryCount]);

		// go to the start of the entry structs
		starpakStream.Seek(starpakFileSize - (8 + starpakEntryHeadersSize), std::ios::beg);
		starpakStream.Read(reinterpret_cast<char*>(starpakEntryHeaders.get()), starpakEntryHeadersSize);

		for (size_t i = 0; i < starpakEntryCount; ++i)
		{
			const PakStreamSetEntry_s* entryHeader = &starpakEntryHeaders.get()[i];

			if (entryHeader->dataSize <= 0) [[unlikely]] // not possible
				continue;
			
			if (entryHeader->offset < 0x1000) [[unlikely]] // also not possible
				continue;

			char* entryData = reinterpret_cast<char*>(_aligned_malloc(entryHeader->dataSize, 8));
			//std::unique_ptr<char> entryData = std::make_unique<char>(new char[entryHeader->dataSize]);

			starpakStream.Seek(entryHeader->offset, std::ios::beg);
			starpakStream.Read(entryData, entryHeader->dataSize);

			CacheDataEntry_t cacheEntry = {};
			cacheEntry.starpakPathOffset = starpakPathOffset;
			cacheEntry.dataOffset = entryHeader->offset;
			cacheEntry.dataSize = entryHeader->dataSize;

			// ideally we don't have entries over 2gb.
			assert(entryHeader->dataSize < INT32_MAX);

			MurmurHash3_x64_128(entryData, static_cast<int>(entryHeader->dataSize), 0x165DCA75, &cacheEntry.hash);

			cacheFile->cachedDataEntries.push_back(cacheEntry);

			_aligned_free(entryData);
		}

		starpakIndex++;

		starpakStream.Close();
	}

	CacheFileHeader_t cacheHeader = cacheFile->ConstructHeader();

	cacheFileStream.Write(cacheHeader);

	for (const std::string& it : cacheFile->cachedStarpaks)
	{
		cacheFileStream.WriteString(it);
	}
	
	cacheFileStream.Seek(cacheHeader.dataEntriesOffset);

	for (const CacheDataEntry_t& dataEntry : cacheFile->cachedDataEntries)
	{
		cacheFileStream.Write(dataEntry);
	}

	cacheFileStream.Close();

	return true;
}


REPAK_END_NAMESPACE()