//=============================================================================//
//
// Pak file builder and management class
//
//=============================================================================//

#include "pch.h"
#include "pakfile.h"
#include "assets/assets.h"

#include "thirdparty/zstd/zstd.h"
#include "thirdparty/zstd/decompress/zstd_decompress_internal.h"

bool CPakFileBuilder::AddJSONAsset(const char* const targetType, const char* const assetType, const char* const assetPath,
							const rapidjson::Value& file, AssetTypeFunc_t func_r2, AssetTypeFunc_t func_r5)
{
	if (strcmp(targetType, assetType) != 0)
		return false;

	AssetTypeFunc_t targetFunc = nullptr;
	const uint16_t fileVersion = this->m_Header.fileVersion;

	switch (fileVersion)
	{
	case 7:
	{
		targetFunc = func_r2;
		break;
	}
	case 8:
	{
		targetFunc = func_r5;
		break;
	}
	}

	if (targetFunc)
	{
		Log("Adding '%s' asset \"%s\".\n", assetType, assetPath);

		const steady_clock::time_point start = high_resolution_clock::now();
		const PakGuid_t assetGuid = Pak_GetGuidOverridable(file, assetPath);

		uint32_t slotIndex;
		const PakAsset_t* const existingAsset = GetAssetByGuid(assetGuid, &slotIndex, true);

		if (existingAsset)
			Error("'%s' asset \"%s\" with GUID 0x%llX was already added in slot #%u.\n", assetType, assetPath, assetGuid, slotIndex);

		targetFunc(this, assetGuid, assetPath, file);
		const steady_clock::time_point stop = high_resolution_clock::now();

		const microseconds duration = duration_cast<microseconds>(stop - start);
		Log("...done; took %lld ms.\n", duration.count());
	}
	else
		Error("Asset type '%.4s' is not supported on pak version %hu.\n", assetType, fileVersion);

	// Asset type has been handled.
	return true;
}

#define HANDLE_ASSET_TYPE(targetType, assetType, assetPath, asset, func_r2, func_r5) if (AddJSONAsset(targetType, assetType, assetPath, asset, func_r2, func_r5)) return;

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

	//if (IsFlagSet(PF_KEEP_CLIENT))
	{
		HANDLE_ASSET_TYPE("txtr", assetType, assetPath, file, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8);
		HANDLE_ASSET_TYPE("txan", assetType, assetPath, file, nullptr, Assets::AddTextureAnimAsset_v1);
		HANDLE_ASSET_TYPE("uimg", assetType, assetPath, file, Assets::AddUIImageAsset_v10, Assets::AddUIImageAsset_v10);
		HANDLE_ASSET_TYPE("matl", assetType, assetPath, file, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15);

		HANDLE_ASSET_TYPE("shds", assetType, assetPath, file, Assets::AddShaderSetAsset_v8, Assets::AddShaderSetAsset_v11);
		HANDLE_ASSET_TYPE("shdr", assetType, assetPath, file, Assets::AddShaderAsset_v8, Assets::AddShaderAsset_v12);
	}

	HANDLE_ASSET_TYPE("dtbl", assetType, assetPath, file, Assets::AddDataTableAsset, Assets::AddDataTableAsset);

	HANDLE_ASSET_TYPE("mdl_", assetType, assetPath, file, nullptr, Assets::AddModelAsset_v9);
	HANDLE_ASSET_TYPE("aseq", assetType, assetPath, file, nullptr, Assets::AddAnimSeqAsset_v7);
	HANDLE_ASSET_TYPE("arig", assetType, assetPath, file, nullptr, Assets::AddAnimRigAsset_v4);

	HANDLE_ASSET_TYPE("Ptch", assetType, assetPath, file, Assets::AddPatchAsset, Assets::AddPatchAsset);

	// If the function has not returned by this point, we have an unhandled asset type.
	Error("Unhandled asset type '%.4s' provided for asset \"%s\".\n", assetType, assetPath);
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
void CPakFileBuilder::AddStarpakReference(const std::string& path)
{
	for (auto& it : m_mandatoryStreamFilePaths)
	{
		if (it == path)
			return;
	}
	m_mandatoryStreamFilePaths.push_back(path);
}

//-----------------------------------------------------------------------------
// purpose: adds new optional starpak file path to be used by the rpak
//-----------------------------------------------------------------------------
void CPakFileBuilder::AddOptStarpakReference(const std::string& path)
{
	for (auto& it : m_optionalStreamFilePaths)
	{
		if (it == path)
			return;
	}
	m_optionalStreamFilePaths.push_back(path);
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak data entry
//-----------------------------------------------------------------------------
void CPakFileBuilder::AddStreamingDataEntry(PakStreamSetEntry_s& block, const uint8_t* const data, const PakStreamSet_e set)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	BinaryIO& out = isMandatory ? m_mandatoryStreamFile : m_optionalStreamFile;

	if (!out.IsWritable())
		Error("Attempted to write a %s streaming asset without a stream file handle.\n", Pak_StreamSetToName(set));

	out.Write(data, block.dataSize);
	const size_t paddedSize = IALIGN(block.dataSize, STARPAK_DATABLOCK_ALIGNMENT);

	// starpak data is aligned to 4096 bytes, pad the remainder out for the next asset.
	if (paddedSize > block.dataSize)
	{
		const size_t paddingRemainder = paddedSize - block.dataSize;
		out.Pad(paddingRemainder);
	}

	size_t& nextOffsetCounter = isMandatory ? m_nextMandatoryStarpakOffset : m_nextOptionalStarpakOffset;

	block.offset = nextOffsetCounter;
	block.dataSize = paddedSize;

	nextOffsetCounter += paddedSize;

	auto& vecBlocks = isMandatory ? m_mandatoryStreamingDataBlocks : m_optionalStreamingDataBlocks;
	vecBlocks.push_back(block);
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
		io.Write(it.starpakOffset);

		if (this->m_Header.fileVersion == 8)
			io.Write(it.optStarpakOffset);

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
PakAsset_t* CPakFileBuilder::GetAssetByGuid(const PakGuid_t guid, uint32_t* const idx /*= nullptr*/, const bool silent /*= false*/)
{
	uint32_t i = 0;
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
// purpose: gets the pak file header size based on pak version
//-----------------------------------------------------------------------------
static inline size_t Pak_GetHeaderSize(const uint16_t version)
{
	switch (version)
	{
		// todo(amos): we probably should import headers for both
		// versions and do a sizeof here.
	case 7: return 0x58;
	case 8: return 0x80;
	default: assert(0); return 0;
	};
}

//-----------------------------------------------------------------------------
// Purpose: initialize pak encoder context
// 
// note(amos): unlike the pak file header, the zstd frame header needs to know
// the uncompressed size without the file header.
//-----------------------------------------------------------------------------
static ZSTD_CCtx* Pak_InitEncoderContext(const size_t uncompressedBlockSize, const int compressLevel, const int workerCount)
{
	ZSTD_CCtx* const cctx = ZSTD_createCCtx();

	if (!cctx)
	{
		Warning("Failed to create encoder context.\n");
		return nullptr;
	}

	size_t result = ZSTD_CCtx_setPledgedSrcSize(cctx, uncompressedBlockSize);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set pledged source size %zu: [%s].\n", uncompressedBlockSize, ZSTD_getErrorName(result));
		ZSTD_freeCCtx(cctx);

		return nullptr;
	}

	result = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressLevel);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set compression level %i: [%s].\n", compressLevel, ZSTD_getErrorName(result));
		ZSTD_freeCCtx(cctx);

		return nullptr;
	}

	result = ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, workerCount);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set worker count %i: [%s].\n", workerCount, ZSTD_getErrorName(result));
		ZSTD_freeCCtx(cctx);

		return nullptr;
	}

	return cctx;
}

//-----------------------------------------------------------------------------
// Purpose: stream encode pak file with given level and worker count
//-----------------------------------------------------------------------------
static bool Pak_StreamToStreamEncode(BinaryIO& inStream, BinaryIO& outStream, const size_t headerSize, const int compressLevel, const int workerCount)
{
	// only the data past the main header gets compressed.
	const size_t decodedFrameSize = (static_cast<size_t>(inStream.GetSize()) - headerSize);

	ZSTD_CCtx* const cctx = Pak_InitEncoderContext(decodedFrameSize, compressLevel, workerCount);

	if (!cctx)
	{
		return false;
	}

	const size_t buffInSize = ZSTD_CStreamInSize();
	std::unique_ptr<uint8_t[]> buffInPtr(new uint8_t[buffInSize]);

	if (!buffInPtr)
	{
		Warning("Failed to allocate input stream buffer of size %zu.\n", buffInSize);
		ZSTD_freeCCtx(cctx);

		return false;
	}

	const size_t buffOutSize = ZSTD_CStreamOutSize();
	std::unique_ptr<uint8_t[]> buffOutPtr(new uint8_t[buffOutSize]);

	if (!buffOutPtr)
	{
		Warning("Failed to allocate output stream buffer of size %zu.\n", buffOutSize);
		ZSTD_freeCCtx(cctx);

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
			size_t const remaining = ZSTD_compressStream2(cctx, &outputFrame, &inputFrame, mode);

			if (ZSTD_isError(remaining))
			{
				Warning("Failed to compress frame at %zd to frame at %zd: [%s].\n",
					inStream.TellGet(), outStream.TellPut(), ZSTD_getErrorName(remaining));

				ZSTD_freeCCtx(cctx);
				return false;
			}

			outStream.Write(reinterpret_cast<uint8_t*>(buffOut), outputFrame.pos);

			finished = lastChunk ? (remaining == 0) : (inputFrame.pos == inputFrame.size);
		} while (!finished);
	}

	ZSTD_freeCCtx(cctx);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: stream encode pak file to new stream and swap old stream with new
//-----------------------------------------------------------------------------
size_t CPakFileBuilder::EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount)
{
	BinaryIO outCompressed;
	const std::string outCompressedPath = m_pakFilePath + "_encoded";

	if (!outCompressed.Open(outCompressedPath, BinaryIO::Mode_e::Write))
	{
		Warning("Failed to open output pak file \"%s\" for compression.\n", outCompressedPath.c_str());
		return 0;
	}

	if (!Pak_StreamToStreamEncode(io, outCompressed, Pak_GetHeaderSize(m_Header.fileVersion), compressLevel, workerCount))
		return 0;

	const size_t compressedSize = outCompressed.TellPut();

	outCompressed.Close();
	io.Close();

	// note(amos): we must reopen the file in ReadWrite mode as otherwise
	// the file gets truncated.

	if (!std::filesystem::remove(m_pakFilePath))
	{
		Warning("Failed to remove uncompressed pak file \"%s\" for swap.\n", outCompressedPath.c_str());
		
		// reopen and continue uncompressed.
		if (io.Open(m_pakFilePath, BinaryIO::Mode_e::ReadWrite))
			Error("Failed to reopen pak file \"%s\".\n", m_pakFilePath.c_str());

		return 0;
	}

	std::filesystem::rename(outCompressedPath, m_pakFilePath);

	// either the rename failed or something holds an open handle to the
	// newly renamed compressed file, irrecoverable.
	if (!io.Open(m_pakFilePath, BinaryIO::Mode_e::ReadWrite))
		Error("Failed to reopen pak file \"%s\".\n", m_pakFilePath.c_str());

	const size_t reopenedPakSize = io.GetSize();

	if (reopenedPakSize != compressedSize)
		Error("Reopened pak file \"%s\" appears truncated or corrupt; compressed size: %zu expected: %zu.\n",
			m_pakFilePath.c_str(), reopenedPakSize, compressedSize);

	// set the header flags indicating this pak is compressed.
	m_Header.flags |= (PAK_HEADER_FLAGS_COMPRESSED | PAK_HEADER_FLAGS_ZSTREAM_ENCODED);

	return compressedSize;
}

//-----------------------------------------------------------------------------
// Purpose: creates the stream file stream and sets the header up
//-----------------------------------------------------------------------------
void CPakFileBuilder::CreateStreamFileStream(const char* const streamFilePath, const PakStreamSet_e set)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	BinaryIO& out = isMandatory ? m_mandatoryStreamFile : m_optionalStreamFile;

	const char* streamFileName = strrchr(streamFilePath, '/');

	if (!streamFileName)
		streamFileName = streamFilePath;
	else
		streamFileName += 1; // advance from '/' to start of filename.

	const std::string fullFilePath = m_outputPath + streamFileName;

	if (!out.Open(fullFilePath, BinaryIO::Mode_e::Write))
		Error("Failed to open %s streaming file \"%s\".\n", Pak_StreamSetToName(set), fullFilePath.c_str());

	// write out the header and pad it out for the first asset entry.
	const PakStreamSetFileHeader_s srpkHeader{ STARPAK_MAGIC, STARPAK_VERSION };
	out.Write(srpkHeader);

	char initialPadding[STARPAK_DATABLOCK_ALIGNMENT - sizeof(PakStreamSetFileHeader_s)];
	memset(initialPadding, STARPAK_DATABLOCK_ALIGNMENT_PADDING, sizeof(initialPadding));

	out.Write(initialPadding, sizeof(initialPadding));

	auto& vecName = isMandatory ? m_mandatoryStreamFilePaths : m_optionalStreamFilePaths;
	vecName.push_back(streamFilePath);
}

//-----------------------------------------------------------------------------
// Purpose: writes the sorts table and finishes the stream file stream
//-----------------------------------------------------------------------------
void CPakFileBuilder::FinishStreamFileStream(const PakStreamSet_e set)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	BinaryIO& out = isMandatory ? m_mandatoryStreamFile : m_optionalStreamFile;

	if (!out.IsWritable())
		Error("Unable to finish %s streaming file without a stream file handle.\n", Pak_StreamSetToName(set));

	// starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
	const auto& vecData = isMandatory ? m_mandatoryStreamingDataBlocks : m_optionalStreamingDataBlocks;

	for (const PakStreamSetEntry_s& it : vecData)
		out.Write(it);

	const size_t entryCount = isMandatory ? GetMandatoryStreamingAssetCount() : GetOptionalStreamingAssetCount();
	out.Write(entryCount);

	const auto& vecName = isMandatory ? m_mandatoryStreamFilePaths : m_optionalStreamFilePaths;

	Log("Written %s streaming file \"%s\" with %zu assets, totaling %zd bytes.\n",
		Pak_StreamSetToName(set), vecName[0].c_str(), entryCount, (ssize_t)out.GetSize());

	out.Close();
}

static void ResolvePath(std::string& outPath, const std::filesystem::path& mapPath, const bool input)
{
	fs::path outputDirPath(outPath);

	if (outputDirPath.is_relative() && mapPath.has_parent_path())
	{
		try {
			outPath = fs::canonical(mapPath.parent_path() / outputDirPath).string();
		}
		catch (const fs::filesystem_error& e) {
			Error("Failed to resolve %s path: %s.\n", input ? "input" : "output", e.what());
		}
	}
	else
		outPath = outputDirPath.string();

	// ensure that the path has a slash at the end
	Utils::AppendSlash(outPath);
}

//-----------------------------------------------------------------------------
// purpose: builds rpak and starpak from input map file
//-----------------------------------------------------------------------------
void CPakFileBuilder::BuildFromMap(const string& mapPath)
{
	// load and parse map file, this file is essentially the
	// control file; deciding what is getting packed, etc..
	js::Document doc{ };
	fs::path inputPath(mapPath);
	Utils::ParseMapDocument(doc, inputPath);

	// determine source asset directory from map file
	if (!JSON_GetValue(doc, "assetsDir", m_assetPath))
	{
		Warning("No \"assetsDir\" field provided; assuming that everything is relative to the working directory.\n");
		m_assetPath = ".\\";
	}
	else
		ResolvePath(m_assetPath, inputPath, true);

	// determine final build path from map file
	if (JSON_GetValue(doc, "outputDir", m_outputPath))
		ResolvePath(m_outputPath, inputPath, false);
	else
		m_outputPath = DEFAULT_RPAK_PATH;

	// create output directory if it does not exist yet.
	fs::create_directories(m_outputPath);

	const int pakVersion = JSON_GetValueOrDefault(doc, "version", -1);

	if (pakVersion < 0)
		Error("No \"version\" field provided.\n");

	this->SetVersion(static_cast<uint16_t>(pakVersion));
	const char* const pakName = JSON_GetValueOrDefault(doc, "name", DEFAULT_RPAK_NAME);

	// print parsed settings
	Log("build settings:\n");
	Log("version: %i\n", GetVersion());
	Log("fileName: %s.rpak\n", pakName);
	Log("assetsDir: %s\n", m_assetPath.c_str());
	Log("outputDir: %s\n\n", m_outputPath.c_str());

	// set build path
	SetPath(m_outputPath + pakName + ".rpak");

	// should dev-only data be kept - e.g. texture asset names, uimg texture names
	if (JSON_GetValueOrDefault(doc, "keepDevOnly", false))
		AddFlags(PF_KEEP_DEV);

	if (JSON_GetValueOrDefault(doc, "keepServerOnly", true))
		AddFlags(PF_KEEP_SERVER);

	if (JSON_GetValueOrDefault(doc, "keepClientOnly", true))
		AddFlags(PF_KEEP_CLIENT);

	// create file stream from path created above
	BinaryIO out;
	if (!out.Open(m_pakFilePath, BinaryIO::Mode_e::ReadWriteCreate))
		Error("Failed to open output pak file \"%s\".\n", m_pakFilePath.c_str());

	// write a placeholder header so we can come back and complete it
	// when we have all the info
	out.Pad(pakVersion >= 8 ? 0x80 : 0x58);

	const char* streamFileMandatory = nullptr;

	if (JSON_GetValue(doc, "streamFileMandatory", streamFileMandatory))
		CreateStreamFileStream(streamFileMandatory, STREAMING_SET_MANDATORY);

	const char* streamFileOptional = nullptr;

	if (pakVersion >= 8 && JSON_GetValue(doc, "streamFileOptional", streamFileOptional))
		CreateStreamFileStream(streamFileOptional, STREAMING_SET_OPTIONAL);

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

	const int compressLevel = JSON_GetValueOrDefault(doc, "compressLevel", 0);

	if (compressLevel > 0 && decompressedFileSize > Pak_GetHeaderSize(m_Header.fileVersion))
	{
		const int workerCount = JSON_GetValueOrDefault(doc, "compressWorkers", 0);

		Log("Encoding pak file with compress level %i and %i workers.\n", compressLevel, workerCount);
		compressedFileSize = EncodeStreamAndSwap(out, compressLevel, workerCount);
	}

	SetCompressedSize(compressedFileSize == 0 ? decompressedFileSize : compressedFileSize);
	SetDecompressedSize(decompressedFileSize);

	// !TODO: we really should add support for multiple starpak files and share existing
	// assets across rpaks. e.g. if the base 'pc_all.opt.starpak' already contains the
	// highest mip level for 'ola_sewer_grate', don't copy it into 'pc_sdk.opt.starpak'.
	// the sort table at the end of the file (see WriteStarpakSortsTable) contains offsets
	// to the asset with their respective sizes. this could be used in combination of a
	// static database (who's name is to be selected from a hint provided by the map file)
	// to map assets among various rpaks avoiding extraneous copies of the same streamed data.

	if (streamFileMandatory)
		FinishStreamFileStream(STREAMING_SET_MANDATORY);

	if (streamFileOptional)
		FinishStreamFileStream(STREAMING_SET_OPTIONAL);

	// set header descriptors
	SetFileTime(Utils::GetSystemFileTime());

	out.SeekPut(0); // go back to the beginning to finally write the rpakHeader now
	WriteHeader(out);

	const ssize_t totalPakSize = out.GetSize();

	Log("Written pak file \"%s\" with %zu assets, totaling %zd bytes.\n",
		m_pakFilePath.c_str(), GetAssetCount(), totalPakSize);

	out.Close();
}
