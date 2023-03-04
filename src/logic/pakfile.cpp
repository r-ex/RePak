//=============================================================================//
//
// purpose: pakfile system
//
//=============================================================================//

#include "pch.h"
#include "pakfile.h"
#include "application/repak.h"

//-----------------------------------------------------------------------------
// purpose: constructor
//-----------------------------------------------------------------------------
CPakFile::CPakFile(int version)
{
	SetVersion(version);
}

//-----------------------------------------------------------------------------
// purpose: installs asset types and their callbacks
//-----------------------------------------------------------------------------
void CPakFile::AddAsset(rapidjson::Value& file)
{
	ASSET_HANDLER("txtr", file, m_Assets, Assets::AddTextureAsset_v8, Assets::AddTextureAsset_v8);
	ASSET_HANDLER("uimg", file, m_Assets, Assets::AddUIImageAsset_v10, Assets::AddUIImageAsset_v10);
	ASSET_HANDLER("Ptch", file, m_Assets, Assets::AddPatchAsset, Assets::AddPatchAsset);
	ASSET_HANDLER("dtbl", file, m_Assets, Assets::AddDataTableAsset_v0, Assets::AddDataTableAsset_v1);
	ASSET_HANDLER("rmdl", file, m_Assets, Assets::AddModelAsset_stub, Assets::AddModelAsset_v9);
	ASSET_HANDLER("matl", file, m_Assets, Assets::AddMaterialAsset_v12, Assets::AddMaterialAsset_v15);
	ASSET_HANDLER("rseq", file, m_Assets, Assets::AddAnimSeqAsset_stub, Assets::AddAnimSeqAsset_v7);
}

//-----------------------------------------------------------------------------
// purpose: adds page pointer to descriptor
//-----------------------------------------------------------------------------
void CPakFile::AddPointer(unsigned int pageIdx, unsigned int pageOffset)
{
	m_vPakDescriptors.push_back({ pageIdx, pageOffset });
}

//-----------------------------------------------------------------------------
// purpose: adds guid descriptor
//-----------------------------------------------------------------------------
void CPakFile::AddGuidDescriptor(std::vector<RPakGuidDescriptor>* guids, unsigned int idx, unsigned int offset)
{
	guids->push_back({ idx, offset });
}

//-----------------------------------------------------------------------------
// purpose: adds raw data block
//-----------------------------------------------------------------------------
void CPakFile::AddRawDataBlock(RPakRawDataBlock block)
{
	m_vRawDataBlocks.push_back(block);
};

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
	// starpak data is aligned to 4096 bytes
	size_t ns = Utils::PadBuffer((char**)&block.m_nDataPtr, block.m_nDataSize, 4096);

	block.m_nDataSize = ns;
	block.m_nOffset = m_NextStarpakOffset;

	m_vStarpakDataBlocks.push_back(block);

	m_NextStarpakOffset += block.m_nDataSize;

	return block;
}

//-----------------------------------------------------------------------------
// purpose: writes header to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteHeader(BinaryIO& io)
{
	m_Header.virtualSegmentCount = m_vVirtualSegments.size();
	m_Header.pageCount = m_vPages.size();
	m_Header.descriptorCount = m_vPakDescriptors.size();
	m_Header.guidDescriptorCount = m_vGuidDescriptors.size();
	m_Header.relationCount = m_vFileRelations.size();

	int version = m_Header.fileVersion;

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
		io.write(it.headIdx);
		io.write(it.headOffset);
		io.write(it.cpuIdx);
		io.write(it.cpuOffset);
		io.write(it.starpakOffset);

		if (this->m_Header.fileVersion == 8)
			io.write(it.optStarpakOffset);

		io.write(it.pageEnd);
		io.write(it.remainingDependencyCount);
		io.write(it.dependentsIndex);
		io.write(it.dependenciesIndex);
		io.write(it.dependentsCount);
		io.write(it.dependenciesCount);
		io.write(it.headDataSize);
		io.write(it.version);
		io.write(it.id);
	}

	// update header asset count with the assets we've just written
	this->m_Header.assetCount = m_Assets.size();
}

//-----------------------------------------------------------------------------
// purpose: writes raw data blocks to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteRawDataBlocks(BinaryIO& out)
{
	for (auto it = m_vRawDataBlocks.begin(); it != m_vRawDataBlocks.end(); ++it)
	{
		out.getWriter()->write((char*)it->m_nDataPtr, it->m_nDataSize);
	}
}

//-----------------------------------------------------------------------------
// purpose: writes starpak paths to file stream
// returns: total length of written path vector
//-----------------------------------------------------------------------------
size_t CPakFile::WriteStarpakPaths(BinaryIO& out, bool optional)
{
	if (optional)
		return Utils::WriteStringVector(out, m_vOptStarpakPaths);
	else
		return Utils::WriteStringVector(out, m_vStarpakPaths);
}

//-----------------------------------------------------------------------------
// purpose: writes virtual segments to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteVirtualSegments(BinaryIO& out)
{
	WRITE_VECTOR(out, m_vVirtualSegments);
}

//-----------------------------------------------------------------------------
// purpose: writes pages to file stream
//-----------------------------------------------------------------------------
void CPakFile::WritePages(BinaryIO& out)
{
	WRITE_VECTOR(out, m_vPages);
}

//-----------------------------------------------------------------------------
// purpose: writes pak descriptors to file stream
//-----------------------------------------------------------------------------
void CPakFile::WritePakDescriptors(BinaryIO& out)
{
	WRITE_VECTOR(out, m_vPakDescriptors);
}

//-----------------------------------------------------------------------------
// purpose: writes guid descriptors to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteGuidDescriptors(BinaryIO& out)
{
	WRITE_VECTOR(out, m_vGuidDescriptors);
}

//-----------------------------------------------------------------------------
// purpose: writes file relations to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteFileRelations(BinaryIO& out)
{
	WRITE_VECTOR(out, m_vFileRelations);
}

//-----------------------------------------------------------------------------
// purpose: writes starpak data blocks to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteStarpakDataBlocks(BinaryIO& out)
{
	for (auto& it : m_vStarpakDataBlocks)
	{
		out.getWriter()->write((const char*)it.m_nDataPtr, it.m_nDataSize);
	}
}

//-----------------------------------------------------------------------------
// purpose: writes starpak sorts table to file stream
//-----------------------------------------------------------------------------
void CPakFile::WriteStarpakSortsTable(BinaryIO& out)
{
	// starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
	// as far as i'm aware, this isn't even used by the game, so i'm not entirely sure why it exists?
	for (auto& it : m_vStarpakDataBlocks)
	{
		SRPkFileEntry fe{};
		fe.m_nOffset = it.m_nOffset;
		fe.m_nSize = it.m_nDataSize;

		out.write(fe);
	}
}

//-----------------------------------------------------------------------------
// purpose: frees the raw data blocks memory
//-----------------------------------------------------------------------------
void CPakFile::FreeRawDataBlocks()
{
	for (auto& it : m_vRawDataBlocks)
	{
		delete[] it.m_nDataPtr;
	}
}

//-----------------------------------------------------------------------------
// purpose: frees the starpak data blocks memory
//-----------------------------------------------------------------------------
void CPakFile::FreeStarpakDataBlocks()
{
	for (auto& it : m_vStarpakDataBlocks)
	{
		delete[] it.m_nDataPtr;
	}
}

//-----------------------------------------------------------------------------
// purpose: populates file relations vector with combined asset relation data
//-----------------------------------------------------------------------------
void CPakFile::GenerateFileRelations()
{
	for (auto& it : m_Assets)
	{
		it.dependentsCount = it._relations.size();
		it.dependentsIndex = m_vFileRelations.size();

		for (int i = 0; i < it._relations.size(); ++i)
			m_vFileRelations.push_back(it._relations[i]);
	}
	m_Header.relationCount = m_vFileRelations.size();
}

//-----------------------------------------------------------------------------
// purpose: 
//-----------------------------------------------------------------------------
void CPakFile::GenerateGuidData()
{
	for (auto& it : m_Assets)
	{
		it.dependenciesCount = it._guids.size();
		it.dependenciesIndex = it.dependenciesCount == 0 ? 0 : m_vGuidDescriptors.size();

		for (int i = 0; i < it._guids.size(); ++i)
			m_vGuidDescriptors.push_back({ it._guids[i] });
	}
	m_Header.guidDescriptorCount = m_vGuidDescriptors.size();
}

//-----------------------------------------------------------------------------
// purpose: 
// returns: 
//-----------------------------------------------------------------------------
_vseginfo_t CPakFile::CreateNewSegment(uint32_t size, uint32_t flags, uint32_t alignment, uint32_t vsegAlignment /*= -1*/)
{
	uint32_t vsegidx = (uint32_t)m_vVirtualSegments.size();

	// find existing "segment" with the same values or create a new one, this is to overcome the engine's limit of having max 20 of these
	// since otherwise we write into unintended parts of the stack, and that's bad
	RPakVirtualSegment seg = GetMatchingSegment(flags, vsegAlignment == -1 ? alignment : vsegAlignment, &vsegidx);

	bool bShouldAddVSeg = seg.dataSize == 0;
	seg.dataSize += size;

	if (bShouldAddVSeg)
		m_vVirtualSegments.emplace_back(seg);
	else
		m_vVirtualSegments[vsegidx] = seg;

	RPakPageInfo vsegblock{ vsegidx, alignment, size };

	m_vPages.emplace_back(vsegblock);
	unsigned int pageidx = m_vPages.size() - 1;

	return { pageidx, size };
}

//-----------------------------------------------------------------------------
// purpose: 
// returns: 
//-----------------------------------------------------------------------------
RPakAssetEntry* CPakFile::GetAssetByGuid(uint64_t guid, uint32_t* idx /*= nullptr*/)
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
	Debug("failed to find asset with guid %llX\n", guid);
	return nullptr;
}

//-----------------------------------------------------------------------------
// purpose: creates page and segment with the specified parameters
// returns: 
//-----------------------------------------------------------------------------
RPakVirtualSegment CPakFile::GetMatchingSegment(uint32_t flags, uint32_t alignment, uint32_t* segidx)
{
	uint32_t i = 0;
	for (auto& it : m_vVirtualSegments)
	{
		if (it.flags == flags && it.alignment == alignment)
		{
			*segidx = i;
			return it;
		}
		i++;
	}

	return { flags, alignment, 0 };
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


	// determine pak name from control file
	string pakName(DEFAULT_RPAK_NAME);
	if (doc.HasMember("name") && doc["name"].IsString())
		pakName = doc["name"].GetStdString();
	else
		Warning("Map file should have a 'name' field containing the string name for the new rpak, but none was provided. Using '%s.rpak'.\n", pakName.c_str());


	// determine source asset directory from map file
	if (!doc.HasMember("assetsDir"))
	{
		Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
		if (inputPath.has_parent_path())
			m_AssetPath = inputPath.parent_path().u8string();
		else
			m_AssetPath = ".\\";
	}
	else
	{
		fs::path assetsDirPath(doc["assetsDir"].GetStdString());
		if (assetsDirPath.is_relative() && inputPath.has_parent_path())
			m_AssetPath = std::filesystem::canonical(inputPath.parent_path() / assetsDirPath).u8string();
		else
			m_AssetPath = assetsDirPath.u8string();

		// ensure that the path has a slash at the end
		Utils::AppendSlash(m_AssetPath);
	}


	// determine final build path from map file
	std::string outputPath(DEFAULT_RPAK_PATH);
	if (doc.HasMember("outputDir"))
	{
		fs::path outputDirPath(doc["outputDir"].GetStdString());

		if (outputDirPath.is_relative() && inputPath.has_parent_path())
			outputPath = fs::canonical(inputPath.parent_path() / outputDirPath).u8string();
		else
			outputPath = outputDirPath.u8string();

		// ensure that the path has a slash at the end
		Utils::AppendSlash(outputPath);
	}


	// determine pakfile version from map file
	if (!doc.HasMember("version") || !doc["version"].IsInt())
		Warning("[JSON] No version field provided; using '%d'.\n", GetVersion());
	else
		SetVersion(doc["version"].GetInt());


	// print parsed settings
	Log("build settings:\n");
	Log("version: %i\n", GetVersion());
	Log("fileName: %s.rpak\n", pakName.c_str());
	Log("assetsDir: %s\n", m_AssetPath.c_str());
	Log("outputDir: %s\n", outputPath.c_str());
	Log("\n");


	// create output directory if it does not exist yet.
	fs::create_directories(outputPath);

	// set build path
	SetPath(outputPath + pakName + ".rpak");


	// if keepDevOnly exists, is boolean, and is set to true
	if (doc.HasMember("keepDevOnly") && doc["keepDevOnly"].IsBool() && doc["keepDevOnly"].GetBool())
		AddFlags(PF_KEEP_DEV);

	if (doc.HasMember("starpakPath") && doc["starpakPath"].IsString())
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


	// write string vectors for starpak paths and get the total length of each vector
	size_t starpakPathsLength = WriteStarpakPaths(out);
	size_t optStarpakPathsLength = WriteStarpakPaths(out, true);

	SetStarpakPathsSize(starpakPathsLength, optStarpakPathsLength);


	// generate file relation vector to be written
	GenerateFileRelations();
	GenerateGuidData();


	// write the non-paged data to the file first
	WriteVirtualSegments(out);
	WritePages(out);
	WritePakDescriptors(out);
	WriteAssets(out);
	WriteGuidDescriptors(out);
	WriteFileRelations(out);


	// now the actual paged data
	// this should probably be writing by page instead of just hoping that
	// the data blocks are in the right order
	WriteRawDataBlocks(out);


	// set header descriptors
	SetFileTime(Utils::GetFileTimeBySystem());

	// !TODO: implement LZHAM and set these accordingly.
	SetCompressedSize(out.tell());
	SetDecompressedSize(out.tell());


	out.seek(0); // go back to the beginning to finally write the rpakHeader now
	WriteHeader(out); out.close();

	Debug("written rpak file with size %lld\n", GetCompressedSize());


	// free the memory
	FreeRawDataBlocks();


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
		std::string filename = path.filename().u8string();

		Debug("writing starpak %s with %lld data entries\n", filename.c_str(), GetStreamingAssetCount());
		BinaryIO srpkOut;

		srpkOut.open(outputPath + filename, BinaryIOMode::Write);

		StreamableSetHeader srpkHeader{ STARPAK_MAGIC , STARPAK_VERSION };
		srpkOut.write(srpkHeader);

		int padSize = (STARPAK_DATABLOCK_ALIGNMENT - sizeof(StreamableSetHeader));

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