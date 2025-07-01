//=============================================================================//
//
// Pak file builder and management class
//
//=============================================================================//

#include "pch.h"
#include "pakfile.h"
#include "assets/assets.h"
#include "utils/zstdutils.h"

CPakFileBuilder::CPakFileBuilder(const CBuildSettings* const buildSettings, CStreamFileBuilder* const streamBuilder)
{
	m_buildSettings = buildSettings;
	m_streamBuilder = streamBuilder;
}

static std::unordered_set<PakAssetHandler_s, PakAssetHasher_s> s_pakAssetHandlers
{
	{"anir", PakAssetScope_e::kServerOnly, Assets::AddAnimRecording_v1, Assets::AddAnimRecording_v1},
	{"txtr", PakAssetScope_e::kClientOnly, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8},
	{"txan", PakAssetScope_e::kClientOnly, nullptr, Assets::AddTextureAnimAsset_v1},
	{"uimg", PakAssetScope_e::kClientOnly, Assets::AddUIImageAsset_v10, Assets::AddUIImageAsset_v10},
	{"rlcd", PakAssetScope_e::kClientOnly, Assets::AddLcdScreenEffect_v0, Assets::AddLcdScreenEffect_v0},
	{"matl", PakAssetScope_e::kClientOnly, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15},
	{"mt4a", PakAssetScope_e::kClientOnly, nullptr, Assets::AddMaterialForAspectAsset_v3},
	{"shdr", PakAssetScope_e::kClientOnly, Assets::AddShaderAsset_v8, Assets::AddShaderAsset_v12},
	{"shds", PakAssetScope_e::kClientOnly, Assets::AddShaderSetAsset_v8, Assets::AddShaderSetAsset_v11},
	{"dtbl", PakAssetScope_e::kAll, Assets::AddDataTableAsset, Assets::AddDataTableAsset},
	{"stlt", PakAssetScope_e::kAll, nullptr, Assets::AddSettingsLayout_v0},
	{"stgs", PakAssetScope_e::kAll, nullptr, Assets::AddSettingsAsset_v1},
	{"mdl_", PakAssetScope_e::kAll, nullptr, Assets::AddModelAsset_v9},
	{"aseq", PakAssetScope_e::kAll, nullptr, Assets::AddAnimSeqAsset_v7},
	{"arig", PakAssetScope_e::kAll, nullptr, Assets::AddAnimRigAsset_v4},
	{"txls", PakAssetScope_e::kAll, nullptr, Assets::AddTextureListAsset_v1},
	{"Ptch", PakAssetScope_e::kAll, Assets::AddPatchAsset, Assets::AddPatchAsset}
};

void CPakFileBuilder::AddJSONAsset(const PakAssetHandler_s& assetHandler, const char* const assetPath, const rapidjson::Value& file)
{
	switch (assetHandler.assetScope)
	{
	case PakAssetScope_e::kServerOnly:
		if (!IsFlagSet(PF_KEEP_SERVER))
			return;
		break;
	case PakAssetScope_e::kClientOnly:
		if (!IsFlagSet(PF_KEEP_CLIENT))
			return;
		break;
	}

	PakAssetAddFunc_t targetFunc = nullptr;
	const uint16_t fileVersion = this->m_Header.fileVersion;

	switch (fileVersion)
	{
	case 7:
	{
		targetFunc = assetHandler.func_r2;
		break;
	}
	case 8:
	{
		targetFunc = assetHandler.func_r5;
		break;
	}
	}

	if (targetFunc)
	{
		Debug("Adding '%s' asset \"%s\".\n", assetHandler.assetType, assetPath);

		const steady_clock::time_point start = high_resolution_clock::now();
		const PakGuid_t assetGuid = Pak_GetGuidOverridable(file, assetPath);

		targetFunc(this, assetGuid, assetPath, file);
		const steady_clock::time_point stop = high_resolution_clock::now();

		const microseconds duration = duration_cast<microseconds>(stop - start);
		Debug("...done; took %lld ms.\n", duration.count());
	}
	else
		Error("Asset type '%.4s' is not supported on pak version %hu.\n", assetHandler.assetType, fileVersion);
}

//-----------------------------------------------------------------------------
// purpose: installs asset types and their callbacks
//-----------------------------------------------------------------------------
void CPakFileBuilder::AddAsset(const rapidjson::Value& file)
{
	const char* const assetType = JSON_GetValueOrDefault(file, "_type", static_cast<const char*>(nullptr));
	const char* const assetPath = JSON_GetValueOrDefault(file, "_path", static_cast<const char*>(nullptr));

	if (!assetType)
		Error("No type provided for asset \"%s\".\n", assetPath ? assetPath : "(unknown)");

	if (!assetPath)
		Error("No path provided for an asset of type '%.4s'.\n", assetType);

	g_currentAsset = assetPath;
	const auto it = s_pakAssetHandlers.find({ assetType });

	if (it == s_pakAssetHandlers.end())
	{
		Error("Unhandled asset type '%.4s' provided.\n", assetType);
		g_currentAsset = nullptr;

		return;
	}

	AddJSONAsset(*it, assetPath, file);
	g_currentAsset = nullptr;
}

//-----------------------------------------------------------------------------
// purpose: adds page pointer to the pak file
//-----------------------------------------------------------------------------
void CPakFileBuilder::AddPointer(PakPageLump_s& pointerLump, const size_t pointerOffset,
	const PakPageLump_s& dataLump, const size_t dataOffset)
{
	m_pagePointers.push_back(pointerLump.GetPointer(pointerOffset));

	// Set the pointer field in the struct to the page index and page offset.
	char* const pointerField = &pointerLump.data[pointerOffset];
	*reinterpret_cast<PagePtr_t*>(pointerField) = dataLump.GetPointer(dataOffset);
}

void CPakFileBuilder::AddPointer(PakPageLump_s& pointerLump, const size_t pointerOffset)
{
	m_pagePointers.push_back(pointerLump.GetPointer(pointerOffset));
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak file path to be used by the rpak
//-----------------------------------------------------------------------------
int64_t CPakFileBuilder::AddStreamingFileReference(const char* const path, const bool mandatory)
{
	auto& vec = mandatory ? m_mandatoryStreamFilePaths : m_optionalStreamFilePaths;
	const int64_t count = static_cast<int64_t>(vec.size());

	for (int64_t index = 0; index < count; index++)
	{
		const std::string& it = vec[index];

		if (it.compare(path) == 0)
			return index;
	}

	// Check if we don't overflow the maximum the runtime supports per set.
	// mandatory and optional are separate sets.
	const size_t newSize = vec.size() + 1;
	const size_t maxSize = GetMaxStreamingFileHandlesPerSet();

	if (newSize > maxSize)
	{
		const char* const streamSetName = Pak_StreamSetToName(mandatory ? PakStreamSet_e::STREAMING_SET_MANDATORY : PakStreamSet_e::STREAMING_SET_OPTIONAL);

		Error("Out of room while adding %s streaming file \"%s\"; runtime has a limit of %zu, got %zu.\n",
			streamSetName, path, maxSize, newSize);
	}

	vec.emplace_back(path);
	return count;
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak data entry
//-----------------------------------------------------------------------------
PakStreamSetEntry_s CPakFileBuilder::AddStreamingDataEntry(const int64_t size, const uint8_t* const data, const PakStreamSet_e set)
{
	const size_t pageAligned = IALIGN(size, STARPAK_DATABLOCK_ALIGNMENT);
	const size_t windowRemainder = pageAligned - size;

	if (windowRemainder > 0)
	{
		// Code bug, data must always be provided with size aligned to
		// STARPAK_DATABLOCK_ALIGNMENT as otherwise the de-duplication
		// code will not work. starpak data is always page aligned. We
		// will still continue to store the data as it will get padded
		// out in the stream file builder within the file itself.
		Warning("%s: didn't receive enough window for page alignment! [%zu < %zu].\n",
			__FUNCTION__, size, pageAligned);
		assert(0);
	}

	StreamAddEntryResults_s results;
	m_streamBuilder->AddStreamingDataEntry(size, data, set, results);

	PakStreamSetEntry_s block;

	block.streamOffset = results.dataOffset;
	block.streamIndex = AddStreamingFileReference(results.streamFile, set == STREAMING_SET_MANDATORY);

	return block;
}

void CPakFileBuilder::SetVersion(const uint16_t version)
{
	if (!Pak_IsVersionSupported(version))
		Error("Unsupported pak file version %hu.\n", version);

	m_Header.fileVersion = version;
}

//-----------------------------------------------------------------------------
// purpose: writes header to file stream
//-----------------------------------------------------------------------------
void CPakFileBuilder::WriteHeader(BinaryIO& io)
{
	m_Header.memSlabCount = m_pageBuilder.GetSlabCount();
	m_Header.memPageCount = m_pageBuilder.GetPageCount();

	assert(m_pagePointers.size() <= UINT32_MAX);
	m_Header.pointerCount = static_cast<uint32_t>(m_pagePointers.size());

	const uint16_t version = m_Header.fileVersion;

	io.Write(m_Header.magic);
	io.Write(m_Header.fileVersion);
	io.Write(m_Header.flags);
	io.Write(m_Header.fileTime);
	io.Write(m_Header.unk0);
	io.Write(m_Header.compressedSize);

	if (version == 8)
		io.Write(m_Header.embeddedStarpakOffset);

	io.Write(m_Header.unk1);
	io.Write(m_Header.decompressedSize);

	if (version == 8)
		io.Write(m_Header.embeddedStarpakSize);

	io.Write(m_Header.unk2);
	io.Write(m_Header.starpakPathsSize);

	if (version == 8)
		io.Write(m_Header.optStarpakPathsSize);

	io.Write(m_Header.memSlabCount);
	io.Write(m_Header.memPageCount);
	io.Write(m_Header.patchIndex);

	if (version == 8)
		io.Write(m_Header.alignment);

	io.Write(m_Header.pointerCount);
	io.Write(m_Header.assetCount);
	io.Write(m_Header.usesCount);
	io.Write(m_Header.dependentsCount);

	if (version == 7)
	{
		io.Write(m_Header.unk7count);
		io.Write(m_Header.unk8count);
	}
	else if (version == 8)
		io.Write(m_Header.unk3);
}

//-----------------------------------------------------------------------------
// purpose: writes assets to file stream
//-----------------------------------------------------------------------------
void CPakFileBuilder::WriteAssetDescriptors(BinaryIO& io)
{
	for (PakAsset_t& it : m_assets)
	{
		io.Write(it.guid);
		io.Write(it.unk0);
		io.Write(it.headPtr.index);
		io.Write(it.headPtr.offset);
		io.Write(it.cpuPtr.index);
		io.Write(it.cpuPtr.offset);
		io.Write(it.GetPackedStreamOffset());

		if (this->m_Header.fileVersion == 8)
			io.Write(it.GetPackedOptStreamOffset());

		assert(it.pageEnd <= UINT16_MAX);
		uint16_t pageEnd = static_cast<uint16_t>(it.pageEnd);
		io.Write(pageEnd);

		io.Write(it.internalDependencyCount);
		io.Write(it.dependentsIndex);
		io.Write(it.usesIndex);
		io.Write(it.dependentsCount);
		io.Write(it.usesCount);
		io.Write(it.headDataSize);
		io.Write(it.version);
		io.Write(it.id);

		it.SetPublicData<void*>(nullptr);
	}

	assert(m_assets.size() <= UINT32_MAX);
	// update header asset count with the assets we've just written
	this->m_Header.assetCount = static_cast<uint32_t>(m_assets.size());
}

//-----------------------------------------------------------------------------
// purpose: writes starpak paths to file stream
// returns: total length of written path vector
//-----------------------------------------------------------------------------
size_t CPakFileBuilder::WriteStarpakPaths(BinaryIO& out, const PakStreamSet_e set)
{
	const auto& vecPaths = set == STREAMING_SET_MANDATORY ? m_mandatoryStreamFilePaths : m_optionalStreamFilePaths;
	return Utils::WriteStringVector(out, vecPaths);
}

//-----------------------------------------------------------------------------
// purpose: writes pak descriptors to file stream
//-----------------------------------------------------------------------------
void CPakFileBuilder::WritePagePointers(BinaryIO& out)
{
	// pointers must be written in order otherwise the runtime crashes as the
	// decoding depends on their order.
	std::sort(m_pagePointers.begin(), m_pagePointers.end());

	for (const PagePtr_t& ptr : m_pagePointers)
		out.Write(ptr);
}

void CPakFileBuilder::WriteAssetUses(BinaryIO& out)
{
	for (const PakAsset_t& it : m_assets)
	{
		for (const PakGuidRef_s& ref : it._uses)
			out.Write(ref.ptr);
	}
}

void CPakFileBuilder::WriteAssetDependents(BinaryIO& out)
{
	for (const PakAsset_t& it : m_assets)
	{
		for (const unsigned int dependent : it._dependents)
			out.Write(dependent);
	}
}

//-----------------------------------------------------------------------------
// purpose: counts the number of internal dependencies for each asset and sets
// them dependent from another. internal dependencies reside in the same pak!
//-----------------------------------------------------------------------------
void CPakFileBuilder::GenerateInternalDependencies()
{
	for (size_t i = 0; i < m_assets.size(); i++)
	{
		PakAsset_t& it = m_assets[i];
		std::set<PakGuid_t> processed;

		for (const PakGuidRef_s& ref : it._uses)
		{
			// an asset can use a dependency more than once, but we should only
			// increment the dependency counter once per unique dependency!
			if (!processed.insert(ref.guid).second)
				continue;

			PakAsset_t* const dependency = GetAssetByGuid(ref.guid, nullptr, true);

			if (dependency)
			{
				dependency->AddDependent(i);
				it.internalDependencyCount++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// purpose: 
//-----------------------------------------------------------------------------
void CPakFileBuilder::GenerateAssetUses()
{
	size_t totalUsesCount = 0;

	for (PakAsset_t& it : m_assets)
	{
		const size_t numUses = it._uses.size();

		if (numUses > 0)
		{
			assert(numUses <= UINT32_MAX);

			it.usesIndex = static_cast<uint32_t>(totalUsesCount);
			it.usesCount = static_cast<uint32_t>(numUses);

			// pointers must be sorted, same principle as WritePagePointers.
			std::sort(it._uses.begin(), it._uses.end());
			totalUsesCount += numUses;
		}
	}

	m_Header.usesCount = static_cast<uint32_t>(totalUsesCount);
}

//-----------------------------------------------------------------------------
// purpose: populates file relations vector with combined asset relation data
//-----------------------------------------------------------------------------
void CPakFileBuilder::GenerateAssetDependents()
{
	size_t totalDependentsCount = 0;

	for (PakAsset_t& it : m_assets)
	{
		const size_t numDependents = it._dependents.size();

		if (numDependents > 0)
		{
			assert(numDependents <= UINT32_MAX);

			it.dependentsIndex = static_cast<uint32_t>(totalDependentsCount);
			it.dependentsCount = static_cast<uint32_t>(numDependents);

			totalDependentsCount += numDependents;
		}
	}

	m_Header.dependentsCount = static_cast<uint32_t>(totalDependentsCount);
}

PakPageLump_s CPakFileBuilder::CreatePageLump(const size_t size, const int flags, const int alignment, void* const buf)
{
	return m_pageBuilder.CreatePageLump(static_cast<int>(size), flags, alignment, buf);
}

//-----------------------------------------------------------------------------
// purpose: 
// returns: 
//-----------------------------------------------------------------------------
PakAsset_t* CPakFileBuilder::GetAssetByGuid(const PakGuid_t guid, size_t* const idx /*= nullptr*/, const bool silent /*= false*/)
{
	size_t i = 0;
	for (PakAsset_t& it : m_assets)
	{
		if (it.guid == guid)
		{
			if (idx)
				*idx = i;

			return &it;
		}
		i++;
	}
	if(!silent)
		Debug("Failed to find asset with guid %llX.\n", guid);
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: initialize pak encoder context
// 
// note(amos): unlike the pak file header, the zstd frame header needs to know
// the uncompressed size without the file header.
//-----------------------------------------------------------------------------
static bool Pak_InitEncoderContext(ZSTD_CCtx* const cctx, const size_t uncompressedBlockSize, const int compressLevel, const int workerCount)
{
	ZSTD_CCtx_reset(cctx, ZSTD_reset_session_only);
	size_t result = ZSTD_CCtx_setPledgedSrcSize(cctx, uncompressedBlockSize);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set pledged source size %zu: [%s].\n", uncompressedBlockSize, ZSTD_getErrorName(result));
		return false;
	}

	result = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressLevel);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set compression level %i: [%s].\n", compressLevel, ZSTD_getErrorName(result));
		return false;
	}

	result = ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, workerCount);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set worker count %i: [%s].\n", workerCount, ZSTD_getErrorName(result));
		return false;
	}

	return true;
}

static ZSTDEncoder_s s_zstdPakEncoder;

//-----------------------------------------------------------------------------
// Purpose: stream encode pak file with given level and worker count
// TODO: support Oodle stream to stream compress
//-----------------------------------------------------------------------------
static bool Pak_StreamToStreamEncode(BinaryIO& inStream, BinaryIO& outStream, const size_t headerSize, const int compressLevel, const int workerCount)
{
	// only the data past the main header gets compressed.
	const size_t decodedFrameSize = (static_cast<size_t>(inStream.GetSize()) - headerSize);

	if (!decodedFrameSize)
	{
		Warning("%s: pak file contains no data to be compressed.\n", __FUNCTION__);
		return false;
	}

	if (!Pak_InitEncoderContext(&s_zstdPakEncoder.cctx, decodedFrameSize, compressLevel, workerCount))
	{
		return false;
	}

	const size_t buffInSize = ZSTD_CStreamInSize();
	std::unique_ptr<uint8_t[]> buffInPtr(new uint8_t[buffInSize]);

	if (!buffInPtr)
	{
		Warning("%s: failed to allocate input stream buffer of size %zu.\n", __FUNCTION__, buffInSize);
		return false;
	}

	const size_t buffOutSize = ZSTD_CStreamOutSize();
	std::unique_ptr<uint8_t[]> buffOutPtr(new uint8_t[buffOutSize]);

	if (!buffOutPtr)
	{
		Warning("%s: failed to allocate output stream buffer of size %zu.\n", __FUNCTION__, buffOutSize);
		return false;
	}

	void* const buffIn = buffInPtr.get();
	void* const buffOut = buffOutPtr.get();

	inStream.SeekGet(headerSize);
	outStream.SeekPut(headerSize);

	size_t bytesLeft = decodedFrameSize;

	while (bytesLeft)
	{
		const bool lastChunk = (bytesLeft < buffInSize);
		const size_t numBytesToRead = lastChunk ? bytesLeft : buffInSize;

		inStream.Read(reinterpret_cast<uint8_t*>(buffIn), numBytesToRead);
		bytesLeft -= numBytesToRead;

		ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;
		ZSTD_inBuffer inputFrame = { buffIn, numBytesToRead, 0 };

		bool finished;
		do {
			ZSTD_outBuffer outputFrame = { buffOut, buffOutSize, 0 };
			size_t const remaining = ZSTD_compressStream2(&s_zstdPakEncoder.cctx, &outputFrame, &inputFrame, mode);

			if (ZSTD_isError(remaining))
			{
				Warning("Failed to compress stream at %zd to stream at %zd: [%s].\n",
					inStream.TellGet(), outStream.TellPut(), ZSTD_getErrorName(remaining));

				return false;
			}

			outStream.Write(reinterpret_cast<uint8_t*>(buffOut), outputFrame.pos);

			finished = lastChunk ? (remaining == 0) : (inputFrame.pos == inputFrame.size);
		} while (!finished);
	}

	return true;
}

static bool Pak_InitDecoderContext(ZSTD_DCtx* const dctx)
{
	ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
	return true;
}

static ZSTDDecoder_s s_zstdPakDecoder;

static bool Pak_StreamToStreamDecode(BinaryIO& inStream, BinaryIO& outStream, const size_t headerSize)
{
	// only the data past the main header gets compressed.
	const size_t encodedFrameSize = (static_cast<size_t>(inStream.GetSize()) - headerSize);

	if (!encodedFrameSize)
	{
		Warning("%s: pak file contains no data to be decompressed.\n", __FUNCTION__);
		return false;
	}

	if (!Pak_InitDecoderContext(&s_zstdPakDecoder.dctx))
	{
		return false;
	}

	const size_t buffInSize = ZSTD_DStreamInSize();
	std::unique_ptr<uint8_t[]> buffInPtr(new uint8_t[buffInSize]);

	if (!buffInPtr)
	{
		Warning("%s: failed to allocate input stream buffer of size %zu.\n", __FUNCTION__, buffInSize);
		return false;
	}

	const size_t buffOutSize = ZSTD_DStreamOutSize();
	std::unique_ptr<uint8_t[]> buffOutPtr(new uint8_t[buffOutSize]);

	if (!buffOutPtr)
	{
		Warning("%s: failed to allocate output stream buffer of size %zu.\n", __FUNCTION__, buffOutSize);
		return false;
	}

	void* const buffIn = buffInPtr.get();
	void* const buffOut = buffOutPtr.get();

	inStream.SeekGet(headerSize);
	outStream.SeekPut(headerSize);

	size_t bytesLeft = encodedFrameSize;
	size_t lastRet = 0;

	while (bytesLeft)
	{
		const bool lastChunk = (bytesLeft < buffInSize);
		const size_t numBytesToRead = lastChunk ? bytesLeft : buffInSize;

		inStream.Read(reinterpret_cast<uint8_t*>(buffIn), numBytesToRead);
		bytesLeft -= numBytesToRead;

		ZSTD_inBuffer inputFrame = { buffIn, numBytesToRead, 0 };

		while (inputFrame.pos < inputFrame.size) {
			ZSTD_outBuffer outputFrame = { buffOut, buffOutSize, 0 };
			size_t const ret = ZSTD_decompressStream(&s_zstdPakDecoder.dctx, &outputFrame, &inputFrame);

			if (ZSTD_isError(ret))
			{
				Error("Failed to decompress stream at %zd to stream at %zd: [%s].\n",
					inStream.TellGet(), outStream.TellPut(), ZSTD_getErrorName(ret));

				return false;
			}

			outStream.Write(reinterpret_cast<uint8_t*>(buffOut), outputFrame.pos);
			lastRet = ret;
		}
	}

	if (lastRet != 0) {

		Error("Failed to decompress; reached EOF before end of stream! (%zu).\n", lastRet);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: stream encode pak file to new stream and swap old stream with new
//-----------------------------------------------------------------------------
size_t Pak_EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount, const uint16_t pakVersion, const char* const pakPath)
{
	Log("*** encoding pak file \"%s\" with compress level %i and %i workers.\n", pakPath, compressLevel, workerCount);
	const steady_clock::time_point start = high_resolution_clock::now();

	BinaryIO outCompressed;
	std::string outCompressedPath = pakPath;

	outCompressedPath.append("_encoded");

	if (!outCompressed.Open(outCompressedPath, BinaryIO::Mode_e::Write))
	{
		Warning("Failed to open output pak file \"%s\" for compression.\n", outCompressedPath.c_str());
		return 0;
	}

	const size_t decompressedSize = (size_t)io.GetSize();

	if (!Pak_StreamToStreamEncode(io, outCompressed, Pak_GetHeaderSize(pakVersion), compressLevel, workerCount))
		return 0;

	const size_t compressedSize = outCompressed.TellPut();

	if (!Util_ReplaceStream(io, outCompressed, pakPath, outCompressedPath.c_str()))
		return 0;

	const size_t reopenedPakSize = io.GetSize();

	if (reopenedPakSize != compressedSize)
	{
		Error("Reopened pak file \"%s\" appears malformed; compressed size: %zu expected: %zu.\n",
			pakPath, reopenedPakSize, compressedSize);
	}

	const steady_clock::time_point stop = high_resolution_clock::now();
	const microseconds duration = duration_cast<microseconds>(stop - start);

	Log("*** finished pak file encoding; took %lld ms (%zu bytes -- %.1f%% ratio).\n",
		duration.count(), compressedSize, 100.0 * (decompressedSize - compressedSize) / decompressedSize);

	return compressedSize;
}

//-----------------------------------------------------------------------------
// Purpose: stream decode pak file to new stream and swap old stream with new
// TODO: support RTech and Oodle in stream wise manner as well
//-----------------------------------------------------------------------------
size_t Pak_DecodeStreamAndSwap(BinaryIO& io, const uint16_t pakVersion, const char* const pakPath)
{
	Log("*** decoding pak file \"%s\".\n", pakPath);
	const steady_clock::time_point start = high_resolution_clock::now();

	BinaryIO outDecompressed;
	std::string outDecompressedPath = pakPath;

	outDecompressedPath.append("_decoded");

	if (!outDecompressed.Open(outDecompressedPath, BinaryIO::Mode_e::Write))
	{
		Warning("Failed to open output pak file \"%s\" for decompression.\n", outDecompressedPath.c_str());
		return 0;
	}

	const size_t compressedSize = (size_t)io.GetSize();

	if (!Pak_StreamToStreamDecode(io, outDecompressed, Pak_GetHeaderSize(pakVersion)))
		return 0;

	const size_t decompressedSize = outDecompressed.TellPut();

	if (!Util_ReplaceStream(io, outDecompressed, pakPath, outDecompressedPath.c_str()))
		return 0;

	const size_t reopenedPakSize = io.GetSize();

	if (reopenedPakSize != decompressedSize)
	{
		Error("Reopened pak file \"%s\" appears malformed; decompressed size: %zu expected: %zu.\n",
			pakPath, reopenedPakSize, decompressedSize);
	}

	const steady_clock::time_point stop = high_resolution_clock::now();
	const microseconds duration = duration_cast<microseconds>(stop - start);

	Log("*** finished pak file decoding; took %lld ms (%zu bytes -- %.1f%% ratio).\n",
		duration.count(), decompressedSize, 100.0 * (decompressedSize - compressedSize) / decompressedSize);
	return decompressedSize;
}

//-----------------------------------------------------------------------------
// purpose: builds rpak and starpak from input map file
//-----------------------------------------------------------------------------
void CPakFileBuilder::BuildFromMap(const js::Document& doc)
{
	// determine source asset directory from map file
	m_assetPath = JSON_GetValueRequired<const char*>(doc, "assetsDir");

	Utils::AppendSlash(m_assetPath);
	Utils::ResolvePath(m_assetPath, m_buildSettings->GetBuildMapPath());

	this->SetVersion(static_cast<uint16_t>(m_buildSettings->GetPakVersion()));
	const char* const pakName = JSON_GetValueOrDefault(doc, "name", DEFAULT_RPAK_NAME);

	// print parsed settings
	Debug("build settings:\n");
	Debug("version: %i\n", GetVersion());
	Debug("fileName: %s.rpak\n", pakName);
	Debug("assetsDir: %s\n", m_assetPath.c_str());

	// set build path
	SetPath(std::string(m_buildSettings->GetOutputPath()) + pakName + ".rpak");

	// create file stream from path created above
	BinaryIO out;
	if (!out.Open(m_pakFilePath, BinaryIO::Mode_e::ReadWriteCreate))
		Error("Failed to open output pak file \"%s\".\n", m_pakFilePath.c_str());

	Log("*** building pak file \"%s\".\n", m_pakFilePath.c_str());

	// write a placeholder header so we can come back and complete it
	// when we have all the info
	out.Pad(GetVersion() >= 8 ? 0x80 : 0x58);

	// build asset data;
	// loop through all assets defined in the map file

	rapidjson::Value::ConstMemberIterator filesIt;

	if (JSON_GetIterator(doc, "files", JSONFieldType_e::kArray, filesIt))
	{
		for (const auto& file : filesIt->value.GetArray())
			AddAsset(file);
	}

	{
		// write string vectors for starpak paths and get the total length of each vector
		size_t starpakPathsLength = WriteStarpakPaths(out, STREAMING_SET_MANDATORY);
		size_t optStarpakPathsLength = WriteStarpakPaths(out, STREAMING_SET_OPTIONAL);
		const size_t combinedPathsLength = starpakPathsLength + optStarpakPathsLength;

		const size_t aligned = IALIGN8(combinedPathsLength);
		const int8_t padBytes = static_cast<int8_t>(aligned - combinedPathsLength);

		// align starpak paths to 
		if (optStarpakPathsLength != 0)
			optStarpakPathsLength += padBytes;
		else
			starpakPathsLength += padBytes;

		out.Seek(padBytes, std::ios::end);
		SetStarpakPathsSize(static_cast<uint16_t>(starpakPathsLength), static_cast<uint16_t>(optStarpakPathsLength));
	}

	GenerateInternalDependencies();

	// generate file relation vector to be written
	GenerateAssetUses();
	GenerateAssetDependents();

	m_pageBuilder.PadSlabsAndPages();

	// write the non-paged data to the file first
	m_pageBuilder.WriteSlabHeaders(out);
	m_pageBuilder.WritePageHeaders(out);

	WritePagePointers(out);
	WriteAssetDescriptors(out);

	WriteAssetUses(out);
	WriteAssetDependents(out);

	// now the actual paged data
	m_pageBuilder.WritePageData(out);

	// We are done building the data of the pack, this is the actual size.
	const size_t decompressedFileSize = out.GetSize();
	size_t compressedFileSize = 0;

	// If this is set and we have "example.rpak", the runtime will load the
	// library `example.dll` during the load of `example.rpak`, from the same
	// directory the pak is being loaded from. The loading of the library
	// happens before the individual assets are being loaded and parsed.
	if (JSON_GetValueOrDefault(doc, "hasDynamicLibrary", false))
		m_Header.flags |= PAK_HEADER_FLAGS_HAS_MODULE;

	const int compressLevel = JSON_GetValueOrDefault(doc, "compressLevel", 0);

	if (compressLevel > 0 && decompressedFileSize > Pak_GetHeaderSize(m_Header.fileVersion))
	{
		const int workerCount = JSON_GetValueOrDefault(doc, "compressWorkers", 0);
		compressedFileSize = Pak_EncodeStreamAndSwap(out, compressLevel, workerCount, GetVersion(), m_pakFilePath.c_str());

		// set the header flags indicating this pak is compressed using zstandard.
		m_Header.flags |= PAK_HEADER_FLAGS_ZSTD_ENCODED;
	}

	SetCompressedSize(compressedFileSize == 0 ? decompressedFileSize : compressedFileSize);
	SetDecompressedSize(decompressedFileSize);

	// set header descriptors
	SetFileTime(Utils::GetSystemFileTime());

	out.SeekPut(0); // go back to the beginning to finally write the rpakHeader now
	WriteHeader(out);

	const ssize_t totalPakSize = out.GetSize();

	Log("*** built pak file \"%s\" with %zu assets, totaling %zd bytes.\n",
		m_pakFilePath.c_str(), GetAssetCount(), totalPakSize);

	out.Close();
}
