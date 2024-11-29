//=============================================================================//
//
// purpose: pakfile system
//
//=============================================================================//

#include "pch.h"
#include "pakfile.h"
#include "assets/assets.h"

#include "thirdparty/zstd/zstd.h"
#include "thirdparty/zstd/decompress/zstd_decompress_internal.h"

bool CPakFile::AddJSONAsset(const char* type, const rapidjson::Value& file, AssetTypeFunc_t func_r2, AssetTypeFunc_t func_r5)
{
	if (file["$type"].GetStdString() == type)
	{
		AssetTypeFunc_t targetFunc = nullptr;
		const short fileVersion = this->m_Header.fileVersion;

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
			targetFunc(this, file["path"].GetString(), file);
		else
			Warning("Asset type '%s' is not supported on RPak version %i\n", type, fileVersion);

		// Asset type has been handled.
		return true;
	}
	else return false;
}

#define HANDLE_ASSET_TYPE(type, asset, func_r2, func_r5) if (AddJSONAsset(type, asset, func_r2, func_r5)) return;

//-----------------------------------------------------------------------------
// purpose: installs asset types and their callbacks
//-----------------------------------------------------------------------------
void CPakFile::AddAsset(const rapidjson::Value& file)
{
	HANDLE_ASSET_TYPE("txtr", file, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8);
	HANDLE_ASSET_TYPE("uimg", file, Assets::AddUIImageAsset_v10, Assets::AddUIImageAsset_v10);
	HANDLE_ASSET_TYPE("Ptch", file, Assets::AddPatchAsset, Assets::AddPatchAsset);
	HANDLE_ASSET_TYPE("dtbl", file, Assets::AddDataTableAsset, Assets::AddDataTableAsset);
	HANDLE_ASSET_TYPE("matl", file, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15);
	HANDLE_ASSET_TYPE("rmdl", file, nullptr, Assets::AddModelAsset_v9);
	HANDLE_ASSET_TYPE("aseq", file, nullptr, Assets::AddAnimSeqAsset_v7);
	HANDLE_ASSET_TYPE("arig", file, nullptr, Assets::AddAnimRigAsset_v4);

	HANDLE_ASSET_TYPE("shds", file, Assets::AddShaderSetAsset_v8, Assets::AddShaderSetAsset_v11);
	HANDLE_ASSET_TYPE("shdr", file, Assets::AddShaderAsset_v8, Assets::AddShaderAsset_v12);

	// If the function has not returned by this point, we have an invalid asset type name.
	Error("Invalid asset type '%s' provided for asset '%s'.\n", JSON_GetValueOrDefault(file, "$type", "(invalid)"), JSON_GetValueOrDefault(file, "path", "(unknown)"));
}

//-----------------------------------------------------------------------------
// purpose: adds page pointer to descriptor
//-----------------------------------------------------------------------------
void CPakFile::AddPointer(int pageIdx, int pageOffset)
{
	m_vPakDescriptors.push_back({ pageIdx, pageOffset });
}

void CPakFile::AddPointer(PagePtr_t ptr)
{
	m_vPakDescriptors.push_back(ptr);
}

//-----------------------------------------------------------------------------
// purpose: adds guid descriptor
//-----------------------------------------------------------------------------
void CPakFile::AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, int idx, int offset)
{
	guids->push_back({ idx, offset });
}

void CPakFile::AddGuidDescriptor(std::vector<PakGuidRefHdr_t>* guids, const PagePtr_t& ptr)
{
	guids->push_back(ptr);
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak file path to be used by the rpak
//-----------------------------------------------------------------------------
void CPakFile::AddStarpakReference(const std::string& path)
{
	for (auto& it : m_vStarpakPaths)
	{
		if (it == path)
			return;
	}
	m_vStarpakPaths.push_back(path);
}

//-----------------------------------------------------------------------------
// purpose: adds new optional starpak file path to be used by the rpak
//-----------------------------------------------------------------------------
void CPakFile::AddOptStarpakReference(const std::string& path)
{
	for (auto& it : m_vOptStarpakPaths)
	{
		if (it == path)
			return;
	}
	m_vOptStarpakPaths.push_back(path);
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak data entry
// returns: starpak data entry descriptor
//-----------------------------------------------------------------------------
StreamableDataEntry CPakFile::AddStarpakDataEntry(StreamableDataEntry block)
{
	const std::string starpakPath = this->GetPrimaryStarpakPath();

	if (starpakPath.length() == 0)
		Error("attempted to create a streaming asset without a starpak assigned. add 'starpakPath' as a global rpak variable to fix\n");

	// try to add starpak path. AddStarpakReference will handle duplicates so no need to do it here
	this->AddStarpakReference(starpakPath);

	// starpak data is aligned to 4096 bytes
	const size_t ns = Utils::PadBuffer((char**)&block.pData, block.dataSize, 4096);

	block.dataSize = ns;
	block.offset = m_NextStarpakOffset;

	m_vStarpakDataBlocks.push_back(block);

	m_NextStarpakOffset += block.dataSize;

	return block;
}

//-----------------------------------------------------------------------------
// purpose: writes header to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteHeader(BinaryIO& io)
{
	assert(m_vVirtualSegments.size() <= UINT16_MAX);
	m_Header.virtualSegmentCount = static_cast<uint16_t>(m_vVirtualSegments.size());

	assert(m_vPages.size() <= UINT16_MAX);
	m_Header.pageCount = static_cast<uint16_t>(m_vPages.size());

	assert(m_vPakDescriptors.size() <= UINT32_MAX);
	m_Header.descriptorCount = static_cast<uint32_t>(m_vPakDescriptors.size());

	assert(m_vGuidDescriptors.size() <= UINT32_MAX);
	m_Header.guidDescriptorCount = static_cast<uint32_t>(m_vGuidDescriptors.size());

	assert(m_vFileRelations.size() <= UINT32_MAX);
	m_Header.relationCount = static_cast<uint32_t>(m_vFileRelations.size());

	short version = m_Header.fileVersion;

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

	io.Write(m_Header.virtualSegmentCount);
	io.Write(m_Header.pageCount);
	io.Write(m_Header.patchIndex);

	if (version == 8)
		io.Write(m_Header.alignment);

	io.Write(m_Header.descriptorCount);
	io.Write(m_Header.assetCount);
	io.Write(m_Header.guidDescriptorCount);
	io.Write(m_Header.relationCount);

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
void CPakFile::WriteAssets(BinaryIO& io)
{
	for (auto& it : m_Assets)
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

		io.Write(it.remainingDependencyCount);
		io.Write(it.dependentsIndex);
		io.Write(it.dependenciesIndex);
		io.Write(it.dependentsCount);
		io.Write(it.dependenciesCount);
		io.Write(it.headDataSize);
		io.Write(it.version);
		io.Write(it.id);

		it.SetPublicData<void*>(nullptr);
	}

	assert(m_Assets.size() <= UINT32_MAX);
	// update header asset count with the assets we've just written
	this->m_Header.assetCount = static_cast<uint32_t>(m_Assets.size());
}

//-----------------------------------------------------------------------------
// purpose: writes raw data blocks to file stream
//-----------------------------------------------------------------------------
void CPakFile::WritePageData(BinaryIO& out)
{
	for (auto& page : m_vPages)
	{
		for (auto& chunk : page.chunks)
		{
			// should never happen
			if (chunk.IsReleased()) [[unlikely]]
			{
				assert(0);
				continue;
			}

			if(chunk.Data())
				out.Write(chunk.Data(), chunk.GetSize());
			else // if chunk is padding to realign the page
			{
				//printf("aligning by %i bytes at %zu\n", chunk.GetSize(), out.tell());

				out.SeekPut(chunk.GetSize(), std::ios::cur);
			}

			chunk.Release();
		}
	}
}

//-----------------------------------------------------------------------------
// purpose: writes starpak paths to file stream
// returns: total length of written path vector
//-----------------------------------------------------------------------------
size_t CPakFile::WriteStarpakPaths(BinaryIO& out, const bool optional)
{
	return Utils::WriteStringVector(out, optional ? m_vOptStarpakPaths : m_vStarpakPaths);
}

//-----------------------------------------------------------------------------
// purpose: writes virtual segments to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteSegmentHeaders(BinaryIO& out)
{
	for (auto& segment : m_vVirtualSegments)
	{
		PakSegmentHdr_t segmentHdr = segment.GetHeader();
		out.Write(segmentHdr);
	}
}

//-----------------------------------------------------------------------------
// purpose: writes page headers to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteMemPageHeaders(BinaryIO& out)
{
	for (auto& page : m_vPages)
	{
		PakPageHdr_t pageHdr = page.GetHeader();
		out.Write(pageHdr);
	}
}

//-----------------------------------------------------------------------------
// purpose: writes pak descriptors to file stream
//-----------------------------------------------------------------------------
void CPakFile::WritePakDescriptors(BinaryIO& out)
{
	// pointers must be written in order otherwise resolving them causes an access violation
	std::sort(m_vPakDescriptors.begin(), m_vPakDescriptors.end());

	WRITE_VECTOR(out, m_vPakDescriptors);
}

//-----------------------------------------------------------------------------
// purpose: writes starpak data blocks to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteStarpakDataBlocks(BinaryIO& out)
{
	for (auto& it : m_vStarpakDataBlocks)
	{
		out.Write((const char*)it.pData, it.dataSize);
	}
}

//-----------------------------------------------------------------------------
// purpose: writes starpak sorts table to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteStarpakSortsTable(BinaryIO& out)
{
	// starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
	for (auto& it : m_vStarpakDataBlocks)
	{
		SRPkFileEntry fe{};
		fe.m_nOffset = it.offset;
		fe.m_nSize = it.dataSize;

		out.Write(fe);
	}
}

//-----------------------------------------------------------------------------
// purpose: frees the starpak data blocks memory
//-----------------------------------------------------------------------------
void CPakFile::FreeStarpakDataBlocks()
{
	for (auto& it : m_vStarpakDataBlocks)
	{
		delete[] it.pData;
	}
}

//-----------------------------------------------------------------------------
// purpose: populates file relations vector with combined asset relation data
//-----------------------------------------------------------------------------
void CPakFile::GenerateFileRelations()
{
	for (auto& it : m_Assets)
	{
		assert(it._relations.size() <= UINT32_MAX);
		it.dependentsCount = static_cast<uint32_t>(it._relations.size());

		// todo: check why this is different to dependencies index
		it.dependentsIndex = static_cast<uint32_t>(m_vFileRelations.size());

		for (int i = 0; i < it._relations.size(); ++i)
			m_vFileRelations.push_back(it._relations[i]);
	}

	assert(m_vFileRelations.size() <= UINT32_MAX);

	m_Header.relationCount = static_cast<uint32_t>(m_vFileRelations.size());
}

//-----------------------------------------------------------------------------
// purpose: 
//-----------------------------------------------------------------------------
void CPakFile::GenerateGuidData()
{
	for (auto& it : m_Assets)
	{
		assert(it._guids.size() <= UINT32_MAX);
		it.dependenciesCount = static_cast<uint32_t>(it._guids.size());

		it.dependenciesIndex = it.dependenciesCount == 0 ? 0 : static_cast<uint32_t>(m_vGuidDescriptors.size());

		std::sort(it._guids.begin(), it._guids.end());

		for (int i = 0; i < it._guids.size(); ++i)
			m_vGuidDescriptors.push_back({ it._guids[i] });
	}

	assert(m_vGuidDescriptors.size() <= UINT32_MAX);

	m_Header.guidDescriptorCount = static_cast<uint32_t>(m_vGuidDescriptors.size());
}

//-----------------------------------------------------------------------------
// purpose: creates page and segment with the specified parameters
// returns: 
//-----------------------------------------------------------------------------
CPakVSegment& CPakFile::FindOrCreateSegment(int flags, int alignment)
{
	int i = 0;
	for (auto& it : m_vVirtualSegments)
	{
		if (it.GetFlags() == flags)
		{
			// if the segment's alignment is less than our requested alignment, we can increase it
			// as increasing the alignment will still allow the previous data to be aligned to the same boundary
			// (all alignments are powers of two)
			if (it.GetAlignment() < alignment)
				it.alignment = alignment;

			return it;
		}
		i++;
	}

	CPakVSegment newSegment{ i, flags, alignment, 0 };

	return m_vVirtualSegments.emplace_back(newSegment);
}

// find the last page that matches the required flags and check if there is room for new data to be added
CPakPage& CPakFile::FindOrCreatePage(int flags, int alignment, size_t newDataSize)
{
	for (size_t i = m_vPages.size(); i > 0; --i)
	{
		CPakPage& page = m_vPages[i-1];
		if (page.GetFlags() == flags)
		{
			if (page.GetSize() + newDataSize <= MAX_PAK_PAGE_SIZE)
			{
				if (page.GetAlignment() < alignment)
				{
					page.alignment = alignment;
					
					// we have to check this because otherwise we can end up with vsegs with lower alignment than their pages
					// and that's probably not supposed to happen
					CPakVSegment& seg = m_vVirtualSegments[page.segmentIndex];
					if (seg.alignment < alignment)
					{
						int j = 0;
						bool updated = false;
						for (auto& it : m_vVirtualSegments)
						{
							if (it.GetFlags() == seg.flags && it.GetAlignment() == alignment)
							{
								//it.dataSize += seg.dataSize;

								//int oldSegIdx = page.segmentIndex;

								//for (auto& pg : m_vPages)
								//{
								//	if (pg.segmentIndex == oldSegIdx)
								//		pg.segmentIndex = j;

								//	// we are about to remove the old segment so anything referencing a higher segment
								//	// needs to be adjusted
								//	if (pg.segmentIndex > oldSegIdx)
								//		pg.segmentIndex--;
								//}

								//m_vVirtualSegments.erase(m_vVirtualSegments.begin() + oldSegIdx);

								seg.dataSize -= page.dataSize;
								it.dataSize += page.dataSize;

								page.segmentIndex = j;

								updated = true;
								break;
							}

							j++;
						}

						if (!updated) // if a segment has not been found matching the new alignment, update the page's existing segment
							seg.alignment = alignment;
					}
				}

				return page;
			}
		}
	}

	CPakVSegment& segment = FindOrCreateSegment(flags, alignment);

	CPakPage p{ this, segment.GetIndex(), static_cast<int>(m_vPages.size()), flags, alignment };

	return m_vPages.emplace_back(p);
}

void CPakPage::AddDataChunk(CPakDataChunk& chunk)
{
	assert(this->alignment > 0 && this->alignment < UINT8_MAX);
	this->PadPageToChunkAlignment(static_cast<uint8_t>(this->alignment));

	chunk.pageIndex = this->GetIndex();
	chunk.pageOffset = this->GetSize();

	this->dataSize += chunk.size;

	this->pak->m_vVirtualSegments[this->segmentIndex].AddToDataSize(chunk.size);

	this->chunks.emplace_back(chunk);
}


void CPakPage::PadPageToChunkAlignment(uint8_t chunkAlignment)
{
	uint32_t alignAmount = IALIGN(this->dataSize, static_cast<uint32_t>(chunkAlignment)) - this->dataSize;

	if (alignAmount > 0)
	{
		//printf("Aligning by %i bytes...\n", alignAmount);
		this->dataSize += alignAmount;
		this->pak->m_vVirtualSegments[this->segmentIndex].AddToDataSize(alignAmount);

		// create null chunk with size of the alignment amount
		// these chunks are handled specially when writing to file,
		// writing only null bytes for the size of the chunk when no data ptr is present
		CPakDataChunk chunk{ 0, 0, alignAmount, 0, nullptr };

		this->chunks.emplace_back(chunk);
	}	
}

CPakDataChunk CPakFile::CreateDataChunk(size_t size, int flags, int alignment)
{
	// this assert is replicated in r5sdk
	assert(alignment != 0 && alignment < UINT8_MAX);

	CPakPage& page = FindOrCreatePage(flags, alignment, size);

	char* buf = new char[size];

	memset(buf, 0, size);

	CPakDataChunk chunk{ size, static_cast<uint8_t>(alignment), buf };
	page.AddDataChunk(chunk);

	return chunk;
}

//-----------------------------------------------------------------------------
// purpose: 
// returns: 
//-----------------------------------------------------------------------------
PakAsset_t* CPakFile::GetAssetByGuid(const PakGuid_t guid, uint32_t* const idx /*= nullptr*/, const bool silent /*= false*/)
{
	uint32_t i = 0;
	for (auto& it : m_Assets)
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
		Debug("failed to find asset with guid %llX\n", guid);
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: initialize pak encoder context
// 
// note(amos): unlike the pak file header, the zstd frame header needs to know
// the uncompressed size without the file header.
//-----------------------------------------------------------------------------
static ZSTD_CCtx* InitEncoderContext(const size_t uncompressedBlockSize, const int compressLevel, const int workerCount)
{
	ZSTD_CCtx* const cctx = ZSTD_createCCtx();

	if (!cctx)
	{
		Warning("Failed to create encoder context\n", workerCount);
		return nullptr;
	}

	size_t result = ZSTD_CCtx_setPledgedSrcSize(cctx, uncompressedBlockSize);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set pledged source size %zu: [%s]\n", uncompressedBlockSize, ZSTD_getErrorName(result));
		ZSTD_freeCCtx(cctx);

		return nullptr;
	}

	result = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compressLevel);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set compression level %i: [%s]\n", compressLevel, ZSTD_getErrorName(result));
		ZSTD_freeCCtx(cctx);

		return nullptr;
	}

	result = ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, workerCount);

	if (ZSTD_isError(result))
	{
		Warning("Failed to set worker count %i: [%s]\n", workerCount, ZSTD_getErrorName(result));
		ZSTD_freeCCtx(cctx);

		return nullptr;
	}

	return cctx;
}

//-----------------------------------------------------------------------------
// Purpose: stream encode pak file with given level and worker count
//-----------------------------------------------------------------------------
bool CPakFile::StreamToStreamEncode(BinaryIO& inStream, BinaryIO& outStream, const int compressLevel, const int workerCount)
{
	// only the data past the main header gets compressed.
	const size_t headerSize = GetHeaderSize();
	const size_t decodedFrameSize = (static_cast<size_t>(inStream.GetSize()) - headerSize);

	ZSTD_CCtx* const cctx = InitEncoderContext(decodedFrameSize, compressLevel, workerCount);

	if (!cctx)
	{
		return false;
	}

	const size_t buffInSize = ZSTD_CStreamInSize();
	std::unique_ptr<uint8_t[]> buffInPtr(new uint8_t[buffInSize]);

	if (!buffInPtr)
	{
		Warning("Failed to allocate input stream buffer of size %zu\n", buffInSize);
		ZSTD_freeCCtx(cctx);

		return false;
	}

	const size_t buffOutSize = ZSTD_CStreamOutSize();
	std::unique_ptr<uint8_t[]> buffOutPtr(new uint8_t[buffOutSize]);

	if (!buffOutPtr)
	{
		Warning("Failed to allocate output stream buffer of size %zu\n", buffOutSize);
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
				Warning("Failed to compress frame at %zu to frame at %zu: [%s]\n",
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
size_t CPakFile::EncodeStreamAndSwap(BinaryIO& io, const int compressLevel, const int workerCount)
{
	BinaryIO outCompressed;
	const std::string outCompressedPath = GetPath() + "_encoded";

	if (!outCompressed.Open(outCompressedPath, BinaryIO::Mode_e::Write))
	{
		Warning("Failed to open output pak file '%s' for compression\n", outCompressedPath.c_str());
		return 0;
	}

	if (!StreamToStreamEncode(io, outCompressed, compressLevel, workerCount))
		return 0;

	const size_t compressedSize = outCompressed.TellPut();

	outCompressed.Close();
	io.Close();

	// note(amos): we must reopen the file in ReadWrite mode as otherwise
	// the file gets truncated.

	if (!std::filesystem::remove(m_Path))
	{
		Warning("Failed to remove uncompressed pak file '%s' for swap\n", outCompressedPath.c_str());
		
		// reopen and continue uncompressed.
		if (io.Open(m_Path, BinaryIO::Mode_e::ReadWrite))
			Error("Failed to reopen pak file '%s'\n", m_Path.c_str());

		return 0;
	}

	std::filesystem::rename(outCompressedPath, m_Path);

	// either the rename failed or something holds an open handle to the
	// newly renamed compressed file, irrecoverable.
	if (!io.Open(m_Path, BinaryIO::Mode_e::ReadWrite))
		Error("Failed to reopen pak file '%s'\n", m_Path.c_str());

	const size_t reopenedPakSize = io.GetSize();

	if (reopenedPakSize != compressedSize)
		Error("Reopened pak file '%s' appears truncated or corrupt; compressed size: %zu expected: %zu\n",
			m_Path.c_str(), reopenedPakSize, compressedSize);

	// set the header flags indicating this pak is compressed.
	m_Header.flags |= (PAK_HEADER_FLAGS_COMPRESSED | PAK_HEADER_FLAGS_ZSTREAM_ENCODED);

	return compressedSize;
}

//-----------------------------------------------------------------------------
// purpose: builds rpak and starpak from input map file
//-----------------------------------------------------------------------------
void CPakFile::BuildFromMap(const string& mapPath)
{
	// load and parse map file, this file is essentially the
	// control file; deciding what is getting packed, etc..
	js::Document doc{ };
	fs::path inputPath(mapPath);
	Utils::ParseMapDocument(doc, inputPath);

	const string pakName = JSON_GetValueOrDefault(doc, "name", DEFAULT_RPAK_NAME);

	// determine source asset directory from map file
	const char* assetDir;

	if (!JSON_GetValue(doc, "assetsDir", assetDir))
	{
		Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
		if (inputPath.has_parent_path())
			m_AssetPath = inputPath.parent_path().string();
		else
			m_AssetPath = ".\\";
	}
	else
	{
		const fs::path assetsDirPath(assetDir);
		if (assetsDirPath.is_relative() && inputPath.has_parent_path())
			m_AssetPath = std::filesystem::canonical(inputPath.parent_path() / assetsDirPath).string();
		else
			m_AssetPath = assetsDirPath.string();

		// ensure that the path has a slash at the end
		Utils::AppendSlash(m_AssetPath);
	}


	// determine final build path from map file
	std::string outputPath;

	if (JSON_GetValue(doc, "outputDir", outputPath))
	{
		fs::path outputDirPath(doc["outputDir"].GetString());

		if (outputDirPath.is_relative() && inputPath.has_parent_path())
			outputPath = fs::canonical(inputPath.parent_path() / outputDirPath).string();
		else
			outputPath = outputDirPath.string();

		// ensure that the path has a slash at the end
		Utils::AppendSlash(outputPath);
	}
	else
		outputPath = DEFAULT_RPAK_PATH;

	const int pakVersion = JSON_GetValueOrDefault(doc, "version", -1);

	if (pakVersion < 0)
		Warning("No version field provided; assuming version 8 (r5)\n");

	this->SetVersion(JSON_GetValueOrDefault(doc, "version", 8));

	// print parsed settings
	Log("build settings:\n");
	Log("version: %i\n", GetVersion());
	Log("fileName: %s.rpak\n", pakName.c_str());
	Log("assetsDir: %s\n", m_AssetPath.c_str());
	Log("outputDir: %s\n\n", outputPath.c_str());

	// create output directory if it does not exist yet.
	fs::create_directories(outputPath);

	// set build path
	SetPath(outputPath + pakName + ".rpak");

	// should dev-only data be kept - e.g. texture asset names, uimg texture names
	if (JSON_GetValueOrDefault(doc, "keepDevOnly", false))
		AddFlags(PF_KEEP_DEV);

	const char* starpakPath;

	if (JSON_GetValue(doc, "starpakPath", starpakPath))
		SetPrimaryStarpakPath(starpakPath);

	// build asset data;
	// loop through all assets defined in the map file

	rapidjson::Value::ConstMemberIterator filesIt;

	if (JSON_GetIterator(doc, "files", JSONFieldType_e::kArray, filesIt))
	{
		for (const auto& file : filesIt->value.GetArray())
		{
			// todo: print asset name here?
			AddAsset(file);
		}
	}

	// create file stream from path created above
	BinaryIO out;
	const std::string outPath = GetPath();
	
	if (!out.Open(outPath, BinaryIO::Mode_e::ReadWriteCreate))
	{
		Error("Failed to open output pak file '%s'\n", outPath.c_str());
	}

	// write a placeholder header so we can come back and complete it
	// when we have all the info
	WriteHeader(out);

	{
		// write string vectors for starpak paths and get the total length of each vector
		size_t starpakPathsLength = WriteStarpakPaths(out, false);
		size_t optStarpakPathsLength = WriteStarpakPaths(out, true);
		size_t combinedPathsLength = starpakPathsLength + optStarpakPathsLength;

		size_t aligned = IALIGN8(combinedPathsLength);
		__int8 padBytes = static_cast<__int8>(aligned - combinedPathsLength);

		// align starpak paths to 
		if (optStarpakPathsLength != 0)
			optStarpakPathsLength += padBytes;
		else
			starpakPathsLength += padBytes;

		out.Seek(padBytes, std::ios::end);

		SetStarpakPathsSize(static_cast<uint16_t>(starpakPathsLength), static_cast<uint16_t>(optStarpakPathsLength));
	}

	// generate file relation vector to be written
	GenerateFileRelations();
	GenerateGuidData();

	// write the non-paged data to the file first
	WriteSegmentHeaders(out);
	WriteMemPageHeaders(out);
	WritePakDescriptors(out);
	WriteAssets(out);

	WRITE_VECTOR(out, m_vGuidDescriptors);
	WRITE_VECTOR(out, m_vFileRelations);

	// now the actual paged data
	WritePageData(out);

	// set header descriptors
	SetFileTime(Utils::GetSystemFileTime());

	// We are done building the data of the pack, this is the actual size.
	const size_t decompressedFileSize = out.GetSize();
	size_t compressedFileSize = 0;

	const int compressLevel = JSON_GetValueOrDefault(doc, "compressLevel", 0);

	if (compressLevel > 0 && decompressedFileSize > GetHeaderSize())
	{
		// todo: add compress print
		const int workerCount = JSON_GetValueOrDefault(doc, "compressWorkers", 0);
		compressedFileSize = EncodeStreamAndSwap(out, compressLevel, workerCount);
	}

	SetCompressedSize(compressedFileSize == 0 ? decompressedFileSize : compressedFileSize);
	SetDecompressedSize(decompressedFileSize);

	out.SeekPut(0); // go back to the beginning to finally write the rpakHeader now
	WriteHeader(out); out.Close();

	Debug("written rpak file with size %zu\n", GetCompressedSize());

	// !TODO: we really should add support for multiple starpak files and share existing
	// assets across rpaks. e.g. if the base 'pc_all.opt.starpak' already contains the
	// highest mip level for 'ola_sewer_grate', don't copy it into 'sdk_all.opt.starpak'.
	// the sort table at the end of the file (see WriteStarpakSortsTable) contains offsets
	// to the asset with their respective sizes. this could be used in combination of a
	// static database (who's name is to be selected from a hint provided by the map file)
	// to map assets among various rpaks avoiding extraneous copies of the same streamed data.
	if (GetNumStarpakPaths() == 1)
	{
		fs::path path(GetStarpakPath(0));
		std::string filename = path.filename().string();

		Debug("writing starpak %s with %zu data entries\n", filename.c_str(), GetStreamingAssetCount());
		BinaryIO srpkOut;

		const std::string fullFilePath = outputPath + filename;

		if (!srpkOut.Open(fullFilePath, BinaryIO::Mode_e::Write))
		{
			Error("Failed to open output streaming file '%s'\n", outPath.c_str());
		}

		StarpakFileHeader_t srpkHeader{ STARPAK_MAGIC , STARPAK_VERSION };
		srpkOut.Write(srpkHeader);

		int padSize = (STARPAK_DATABLOCK_ALIGNMENT - sizeof(StarpakFileHeader_t));

		char* initialPad = new char[padSize];
		memset(initialPad, STARPAK_DATABLOCK_ALIGNMENT_PADDING, padSize);

		srpkOut.Write(initialPad, padSize);
		delete[] initialPad;

		WriteStarpakDataBlocks(srpkOut);
		WriteStarpakSortsTable(srpkOut);

		uint64_t entryCount = GetStreamingAssetCount();
		srpkOut.Write(entryCount);

		Debug("written starpak file with size %zu\n", srpkOut.TellPut());

		FreeStarpakDataBlocks();
		srpkOut.Close();
	}
}