//=============================================================================//
//
// Pak streaming file data manager
//
//=============================================================================//
#include <pch.h>
#include <public/starpak.h>
#include <utils/utils.h>
#include <fstream>

#include <utils/MurmurHash3.h>
#include "streamcache.h"
#include <public/rpak.h>

#define MURMUR_SEED 0x165DCA75
//#define CHECK_FOR_DUPLICATES

static std::vector<StreamCacheFileEntry_s> StreamCache_GetStarpakFilesFromDirectory(const char* const directoryPath)
{
	std::vector<StreamCacheFileEntry_s> paths;
	for (auto& it : std::filesystem::directory_iterator(directoryPath))
	{
		const fs::path& entryPath = it.path();

		if (!entryPath.has_extension())
			continue;

		const fs::path fileNameFs = entryPath.filename();
		const std::string fileName = fileNameFs.string();

		const char* const fullExtension = strchr(fileName.c_str(), '.');

		if (!fullExtension)
			continue;

		bool optional = false;

		if (strcmp(fullExtension, ".starpak") != 0)
		{
			// This is the only way to check if a starpak is optional without
			// parsing an rpak that uses it. All optional starpaks are marked
			// with '.opt' so this is very robust and reliable.
			if (strcmp(fullExtension, ".opt.starpak") != 0)
				continue;

			optional = true;
		}

		StreamCacheFileEntry_s& entry = paths.emplace_back();

		entry.isOptional = optional;
		entry.streamFilePath = entryPath.string();

		// Normalize the slashes.
		for (char& c : entry.streamFilePath)
		{
			if (c == '\\')
				c = '/';
		}
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

	const std::vector<StreamCacheFileEntry_s> foundStarpakPaths = StreamCache_GetStarpakFilesFromDirectory(directoryPath.c_str());

	Log("Found %zu streaming files to cache in directory \"%s\".\n", foundStarpakPaths.size(), directoryPath.c_str());

	// Start a timer for the cache builder process so that the user is notified when the tool finishes.
	TIME_SCOPE("StreamCacheBuilder");
	size_t starpakIndex = 0;

	for (const StreamCacheFileEntry_s& foundEntry : foundStarpakPaths)
	{
		const std::string& starpakPath = foundEntry.streamFilePath;

		BinaryIO starpakStream;

		if (!starpakStream.Open(starpakPath, BinaryIO::Mode_e::Read))
		{
			Error("Failed to open streaming file \"%s\" for reading.\n", starpakPath.c_str());
			continue;
		}

		PakStreamSetFileHeader_s starpakFileHeader = starpakStream.Read<PakStreamSetFileHeader_s>();

		if (starpakFileHeader.magic != STARPAK_MAGIC)
		{
			Error("Streaming file \"%s\" has an invalid file magic; expected %x, got %x.\n", starpakPath.c_str(), STARPAK_MAGIC, starpakFileHeader.magic);
			continue;
		}

		Log("Adding streaming file \"%s\" (%zu/%zu) to the cache.\n", starpakPath.c_str(), starpakIndex + 1, foundStarpakPaths.size());

		const size_t starpakFileSize = fs::file_size(starpakPath);

		starpakStream.Seek(starpakFileSize - sizeof(int64_t), std::ios::beg);

		// get the number of data entries in this starpak file
		const int64_t starpakEntryCount = starpakStream.Read<int64_t>();
		const size_t starpakEntryHeadersSize = sizeof(PakStreamSetAssetEntry_s) * starpakEntryCount;

		std::unique_ptr<PakStreamSetAssetEntry_s> starpakEntryHeaders = std::unique_ptr<PakStreamSetAssetEntry_s>(new PakStreamSetAssetEntry_s[starpakEntryCount]);

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

		const int64_t pathIndex = AddStarpakPathToCache(relativeStarpakPath, foundEntry.isOptional);

		for (int64_t i = 0; i < starpakEntryCount; ++i)
		{
			const PakStreamSetAssetEntry_s* entryHeader = &starpakEntryHeaders.get()[i];

			if (entryHeader->size == 0) [[unlikely]] // not possible
				Error("Stream entry #%lld has a size of 0; streaming file appears corrupt.\n", i);
			
			if (entryHeader->offset < STARPAK_DATABLOCK_ALIGNMENT) [[unlikely]] // also not possible
				Error("Stream entry #%lld has an offset lower than %d; streaming file appears corrupt.\n", i, STARPAK_DATABLOCK_ALIGNMENT);

			char* const entryData = reinterpret_cast<char*>(_aligned_malloc(entryHeader->size, 8));

			starpakStream.Seek(entryHeader->offset, std::ios::beg);
			starpakStream.Read(entryData, entryHeader->size);

			StreamCacheDataEntry_s& cacheEntry = m_dataEntries.emplace_back();

			cacheEntry.dataOffset = entryHeader->offset;
			cacheEntry.pathIndex = pathIndex;
			cacheEntry.dataSize = entryHeader->size;

			// ideally we don't have entries over 2gb.
			assert(entryHeader->size < INT32_MAX);

			MurmurHash3_x64_128(entryData, static_cast<size_t>(entryHeader->size), MURMUR_SEED, &cacheEntry.hash);
			_aligned_free(entryData);
		}

		starpakIndex++;
	}

	Save(cacheFileStream);
}

static bool SIMD_CompareM128i(const __m128i a, const __m128i b)
{
	const __m128i result = _mm_cmpeq_epi8(a, b); // Compare element-wise for equality (32-bit integers).
	return _mm_movemask_epi8(result) == 0xFFFF; // Check if all elements are equal.
}

#ifdef CHECK_FOR_DUPLICATES
struct DuplicateChecker
{
	DuplicateChecker(__m128i o)
		: i(o) {}

	inline bool operator<(const DuplicateChecker& o) const
	{
		const int64_t lowA = _mm_extract_epi64(i, 0);
		const int64_t highA = _mm_extract_epi64(i, 1);
		const int64_t lowB = _mm_extract_epi64(o.i, 0);
		const int64_t highB = _mm_extract_epi64(o.i, 1);

		if (highA == highB)
			return lowA < lowB;

		return highA < highB;
	}

	__m128i i;
};

static void PrintM128i64(__m128i in)
{
	alignas(16) uint64_t v[2];  // uint64_t might give format-string warnings with %llx; it's just long in some ABIs
	_mm_store_si128(reinterpret_cast<__m128i*>(v), in);
	printf("v2_u64: %llx %llx\n", v[0], v[1]);
}
#endif // CHECK_FOR_DUPLICATES

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
	m_streamFiles.resize(streamCacheHeader.streamingFileCount);

	for (size_t i = 0; i < streamCacheHeader.streamingFileCount; i++)
	{
		StreamCacheFileEntry_s& entry = m_streamFiles[i];

		cacheFileStream.Read(entry.isOptional);
		cacheFileStream.ReadString(entry.streamFilePath);
	}

	m_dataEntries.resize(streamCacheHeader.dataEntryCount);

	cacheFileStream.SeekGet(streamCacheHeader.dataEntriesOffset);
	cacheFileStream.Read(m_dataEntries.data(), actualBlockSize);

#ifdef CHECK_FOR_DUPLICATES
	std::set<DuplicateChecker> testSet;

	for (auto& e : m_dataEntries)
	{
		auto p = testSet.insert(e.hash);

		if (!p.second)
		{
			Warning("Detected duplicate hash entries!\n");

			PrintM128i64(p.first->i);
			PrintM128i64(e.hash);
		}
	}
#endif // CHECK_FOR_DUPLICATES
}

int64_t CStreamCache::AddStarpakPathToCache(const std::string& path, const bool optional)
{
	const int64_t index = static_cast<int64_t>(m_streamFiles.size());
	m_streamFiles.push_back({ optional, path });

	// We store the count into 12 bits, so we cannot have more than 4096.
	// Realistically, we shouldn't ever exceed this as we should only
	// deduplicate data across the 'all' and 'roots' starpaks.
	assert(m_streamFiles.size() <= 4096);
	return index;
}

StreamCacheFileHeader_s CStreamCache::ConstructHeader() const
{
	StreamCacheFileHeader_s fileHeader;

	fileHeader.magic = STREAM_CACHE_FILE_MAGIC;
	fileHeader.majorVersion = STREAM_CACHE_FILE_MAJOR_VERSION;
	fileHeader.minorVersion = STREAM_CACHE_FILE_MINOR_VERSION;
	fileHeader.streamingFileCount = m_streamFiles.size();
	fileHeader.dataEntryCount = m_dataEntries.size();

	size_t totStreamFileNameBufSize = 0;

	for (const StreamCacheFileEntry_s& fileEntry : m_streamFiles)
		totStreamFileNameBufSize += fileEntry.streamFilePath.length() + 2; // 1 for the 'optional' bool, 1 for the null terminator.

	fileHeader.dataEntriesOffset = IALIGN16(sizeof(StreamCacheFileHeader_s) + totStreamFileNameBufSize);
	return fileHeader;
}

StreamCacheFindParams_s CStreamCache::CreateParams(const uint8_t* const data, const int64_t size, const char* const streamFilePath)
{
	__m128i hash;
	MurmurHash3_x64_128(data, static_cast<size_t>(size), MURMUR_SEED, &hash);

	StreamCacheFindParams_s ret;

	ret.hash = hash;
	ret.size = size;
	ret.streamFilePath = streamFilePath;

	return ret;
}

bool CStreamCache::Find(const StreamCacheFindParams_s& params, StreamCacheFindResult_s& result, const bool optional)
{
	for (const StreamCacheDataEntry_s& entry : m_dataEntries)
	{
		if (entry.dataSize != params.size)
			continue;

		const StreamCacheFileEntry_s& file = m_streamFiles[entry.pathIndex];

		if (file.isOptional != optional)
			continue;

		if (!SIMD_CompareM128i(entry.hash, params.hash))
			continue;

		result.fileEntry = &file;
		result.dataEntry = &entry;

		return true;
	}

	return false;
}

void CStreamCache::Add(const StreamCacheFindParams_s& params, const int64_t offset, const bool optional)
{
	const StreamCacheFileEntry_s* pNewFileEntry = nullptr;
	int64_t newIndex = -1;

	for (const StreamCacheFileEntry_s& fileEntry : m_streamFiles)
	{
		newIndex++;

		if (fileEntry.streamFilePath.compare(params.streamFilePath) != 0)
			continue;

		pNewFileEntry = &fileEntry;
		break;
	}

	if (!pNewFileEntry)
	{
		newIndex = static_cast<int64_t>(m_streamFiles.size());
		StreamCacheFileEntry_s& newFileEntry = m_streamFiles.emplace_back();

		newFileEntry.isOptional = optional;
		newFileEntry.streamFilePath = params.streamFilePath;

		pNewFileEntry = &newFileEntry;
	}

	StreamCacheDataEntry_s& newDataEntry = m_dataEntries.emplace_back();

	newDataEntry.dataOffset = offset;
	newDataEntry.pathIndex = newIndex;
	newDataEntry.dataSize = params.size;
	newDataEntry.hash = params.hash;
}

void CStreamCache::Save(BinaryIO& io)
{
	assert(io.IsWritable());

	StreamCacheFileHeader_s cacheHeader = ConstructHeader();
	io.Write(cacheHeader);

	for (const StreamCacheFileEntry_s& fileEntry : m_streamFiles)
	{
		io.Write(fileEntry.isOptional);
		io.WriteString(fileEntry.streamFilePath, true);
	}

	const size_t padDelta = cacheHeader.dataEntriesOffset - io.TellPut();

	if (padDelta > 0)
		io.Pad(padDelta);

	for (const StreamCacheDataEntry_s& dataEntry : m_dataEntries)
	{
		io.Write(dataEntry);
	}
}
