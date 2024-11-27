//=============================================================================//
//
// purpose: pakfile system
//
//=============================================================================//

#include "pch.h"
#include "pakfile.h"
#include "assets/assets.h"

bool CPakFile::AddJSONAsset(const char* type, rapidjson::Value& file, AssetTypeFunc_t func_r2, AssetTypeFunc_t func_r5)
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
void CPakFile::AddAsset(rapidjson::Value& file)
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
	Error("Invalid asset type '%s' provided for asset '%s'.\n", JSON_GET_STR(file, "$type", "(invalid)"), JSON_GET_STR(file, "path", "(unknown)"));
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

	io.write(m_Header.magic);
	io.write(m_Header.fileVersion);
	io.write(m_Header.flags);
	io.write(m_Header.fileTime);
	io.write(m_Header.unk0);
	io.write(m_Header.compressedSize);

	if (version == 8)
		io.write(m_Header.embeddedStarpakOffset);

	io.write(m_Header.unk1);
	io.write(m_Header.decompressedSize);

	if (version == 8)
		io.write(m_Header.embeddedStarpakSize);

	io.write(m_Header.unk2);
	io.write(m_Header.starpakPathsSize);

	if (version == 8)
		io.write(m_Header.optStarpakPathsSize);

	io.write(m_Header.virtualSegmentCount);
	io.write(m_Header.pageCount);
	io.write(m_Header.patchIndex);

	if (version == 8)
		io.write(m_Header.alignment);

	io.write(m_Header.descriptorCount);
	io.write(m_Header.assetCount);
	io.write(m_Header.guidDescriptorCount);
	io.write(m_Header.relationCount);

	if (version == 7)
	{
		io.write(m_Header.unk7count);
		io.write(m_Header.unk8count);
	}
	else if (version == 8)
		io.write(m_Header.unk3);
}

//-----------------------------------------------------------------------------
// purpose: writes assets to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteAssets(BinaryIO& io)
{
	for (auto& it : m_Assets)
	{
		io.write(it.guid);
		io.write(it.unk0);
		io.write(it.headPtr.index);
		io.write(it.headPtr.offset);
		io.write(it.cpuPtr.index);
		io.write(it.cpuPtr.offset);
		io.write(it.starpakOffset);

		if (this->m_Header.fileVersion == 8)
			io.write(it.optStarpakOffset);

		assert(it.pageEnd <= UINT16_MAX);
		uint16_t pageEnd = static_cast<uint16_t>(it.pageEnd);
		io.write(pageEnd);

		io.write(it.remainingDependencyCount);
		io.write(it.dependentsIndex);
		io.write(it.dependenciesIndex);
		io.write(it.dependentsCount);
		io.write(it.dependenciesCount);
		io.write(it.headDataSize);
		io.write(it.version);
		io.write(it.id);

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
				out.getWriter()->write(chunk.Data(), chunk.GetSize());
			else // if chunk is padding to realign the page
			{
				//printf("aligning by %i bytes at %lld\n", chunk.GetSize(), out.tell());

				out.getWriter()->seekp(chunk.GetSize(), std::ios::cur);
			}

			chunk.Release();
		}
	}
}

//-----------------------------------------------------------------------------
// purpose: writes starpak paths to file stream
// returns: total length of written path vector
//-----------------------------------------------------------------------------
size_t CPakFile::WriteStarpakPaths(BinaryIO& out, bool optional)
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
		out.write(segmentHdr);
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
		out.write(pageHdr);
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
		out.getWriter()->write((const char*)it.pData, it.dataSize);
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

		out.write(fe);
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
PakAsset_t* CPakFile::GetAssetByGuid(uint64_t guid, uint32_t* idx /*= nullptr*/, bool silent /*= false*/)
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
// purpose: builds rpak and starpak from input map file
//-----------------------------------------------------------------------------
void CPakFile::BuildFromMap(const string& mapPath)
{
	// load and parse map file, this file is essentially the
	// control file; deciding what is getting packed, etc..
	js::Document doc{ };
	fs::path inputPath(mapPath);
	Utils::ParseMapDocument(doc, inputPath);

	string pakName = JSON_GET_STR(doc, "name", DEFAULT_RPAK_NAME);

	// determine source asset directory from map file
	if (!JSON_IS_STR(doc, "assetsDir"))
	{
		Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
		if (inputPath.has_parent_path())
			m_AssetPath = inputPath.parent_path().string();
		else
			m_AssetPath = ".\\";
	}
	else
	{
		fs::path assetsDirPath(doc["assetsDir"].GetStdString());
		if (assetsDirPath.is_relative() && inputPath.has_parent_path())
			m_AssetPath = std::filesystem::canonical(inputPath.parent_path() / assetsDirPath).string();
		else
			m_AssetPath = assetsDirPath.string();

		// ensure that the path has a slash at the end
		Utils::AppendSlash(m_AssetPath);
	}


	// determine final build path from map file
	std::string outputPath(DEFAULT_RPAK_PATH);
	if (JSON_IS_STR(doc, "outputDir"))
	{
		fs::path outputDirPath(doc["outputDir"].GetString());

		if (outputDirPath.is_relative() && inputPath.has_parent_path())
			outputPath = fs::canonical(inputPath.parent_path() / outputDirPath).string();
		else
			outputPath = outputDirPath.string();

		// ensure that the path has a slash at the end
		Utils::AppendSlash(outputPath);
	}

	if (!JSON_IS_INT(doc, "version"))
		Warning("No version field provided; assuming version 8 (r5)\n");

	this->SetVersion(JSON_GET_INT(doc, "version", 8));

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
	if (JSON_GET_BOOL(doc, "keepDevOnly"))
		AddFlags(PF_KEEP_DEV);

	if (JSON_IS_STR(doc, "starpakPath"))
		SetPrimaryStarpakPath(doc["starpakPath"].GetStdString());

	// build asset data;
	// loop through all assets defined in the map file
	for (auto& file : doc["files"].GetArray())
	{
		AddAsset(file);
	}

	// create file stream from path created above
	BinaryIO out;
	out.open(GetPath(), BinaryIOMode::Write);

	// write a placeholder header so we can come back and complete it
	// when we have all the info
	WriteHeader(out);

	{
		// write string vectors for starpak paths and get the total length of each vector
		size_t starpakPathsLength = WriteStarpakPaths(out);
		size_t optStarpakPathsLength = WriteStarpakPaths(out, true);
		size_t combinedPathsLength = starpakPathsLength + optStarpakPathsLength;

		size_t aligned = IALIGN8(combinedPathsLength);
		__int8 padBytes = static_cast<__int8>(aligned - combinedPathsLength);

		// align starpak paths to 
		if (optStarpakPathsLength != 0)
			optStarpakPathsLength += padBytes;
		else
			starpakPathsLength += padBytes;

		out.seek(padBytes, std::ios::cur);

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

	// !TODO: implement LZHAM and set these accordingly.
	SetCompressedSize(out.tell());
	SetDecompressedSize(out.tell());


	out.seek(0); // go back to the beginning to finally write the rpakHeader now
	WriteHeader(out); out.close();

	Debug("written rpak file with size %lld\n", GetCompressedSize());

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

		Debug("writing starpak %s with %lld data entries\n", filename.c_str(), GetStreamingAssetCount());
		BinaryIO srpkOut;

		srpkOut.open(outputPath + filename, BinaryIOMode::Write);

		StarpakFileHeader_t srpkHeader{ STARPAK_MAGIC , STARPAK_VERSION };
		srpkOut.write(srpkHeader);

		int padSize = (STARPAK_DATABLOCK_ALIGNMENT - sizeof(StarpakFileHeader_t));

		char* initialPad = new char[padSize];
		memset(initialPad, STARPAK_DATABLOCK_ALIGNMENT_PADDING, padSize);

		srpkOut.getWriter()->write(initialPad, padSize);
		delete[] initialPad;

		WriteStarpakDataBlocks(srpkOut);
		WriteStarpakSortsTable(srpkOut);

		uint64_t entryCount = GetStreamingAssetCount();
		srpkOut.write(entryCount);

		Debug("written starpak file with size %lld\n", srpkOut.tell());

		FreeStarpakDataBlocks();
		srpkOut.close();
	}
}