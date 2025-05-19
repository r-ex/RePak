//=============================================================================//
//
// Pak streaming file build manager
//
//=============================================================================//
#include <pch.h>
#include "streamfile.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CStreamFileBuilder::CStreamFileBuilder(const CBuildSettings* const buildSettings)
{
	m_buildSettings = buildSettings;
	m_streamCacheFileName = nullptr;
	m_mandatoryStreamFileName = nullptr;
	m_optionalStreamFileName = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: parse and initialize
//-----------------------------------------------------------------------------
void CStreamFileBuilder::Init(const js::Document& doc, const bool useOptional)
{
	if (JSON_GetValue(doc, "streamFileMandatory", m_mandatoryStreamFileName))
		CreateStreamFileStream(m_mandatoryStreamFileName, STREAMING_SET_MANDATORY);

	if (useOptional && JSON_GetValue(doc, "streamFileOptional", m_optionalStreamFileName))
		CreateStreamFileStream(m_optionalStreamFileName, STREAMING_SET_OPTIONAL);

	if (JSON_GetValue(doc, "streamCache", m_streamCacheFileName))
	{
		fs::path streamCacheDirFs(m_streamCacheFileName);
		std::string streamCacheDirStr = streamCacheDirFs.parent_path().string();

		Utils::ResolvePath(streamCacheDirStr, m_buildSettings->GetBuildMapPath());
		streamCacheDirStr.append(streamCacheDirFs.filename().string());

		Log("Loading cache from streaming map file \"%s\".\n", streamCacheDirStr.c_str());
		m_streamCache.ParseMap(streamCacheDirStr.c_str());
	}
}

//-----------------------------------------------------------------------------
// Purpose: shutdown
//-----------------------------------------------------------------------------
void CStreamFileBuilder::Shutdown()
{
	FinishStreamFileStream(STREAMING_SET_MANDATORY);
	FinishStreamFileStream(STREAMING_SET_OPTIONAL);

	const char* const streamFile = m_mandatoryStreamFileName 
		? m_mandatoryStreamFileName 
		: m_optionalStreamFileName;

	if (streamFile)
	{
		const char* streamFileName = Utils::ExtractFileName(streamFile);
		std::string fullFilePath = m_buildSettings->GetOutputPath();

		fullFilePath.append(streamFileName);
		fullFilePath = Utils::ChangeExtension(fullFilePath, ".starmap");

		BinaryIO newCache;

		if (newCache.Open(fullFilePath, BinaryIO::Mode_e::Write))
		{
			m_streamCache.Save(newCache);
			Log("Saved cache to streaming map file \"%s\".\n", fullFilePath.c_str());
		}
		else
			Warning("Failed to save cache to streaming map file \"%s\".\n", fullFilePath.c_str());
	}
}

//-----------------------------------------------------------------------------
// Purpose: creates the stream file stream and sets the header up
//-----------------------------------------------------------------------------
void CStreamFileBuilder::CreateStreamFileStream(const char* const streamFilePath, const PakStreamSet_e set)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	BinaryIO& out = isMandatory ? m_mandatoryStreamFile : m_optionalStreamFile;

	if (out.IsWritable())
		return; // Already opened.

	const char* streamFileName = Utils::ExtractFileName(streamFilePath);

	std::string fullFilePath = m_buildSettings->GetOutputPath();
	fullFilePath.append(streamFileName);

	if (!out.Open(fullFilePath, BinaryIO::Mode_e::Write))
		Error("Failed to open %s streaming file \"%s\".\n", Pak_StreamSetToName(set), fullFilePath.c_str());

	Log("Opened %s streaming file stream \"%s\".\n", Pak_StreamSetToName(set), fullFilePath.c_str());

	// write out the header and pad it out for the first asset entry.
	const PakStreamSetFileHeader_s srpkHeader{ STARPAK_MAGIC, STARPAK_VERSION };
	out.Write(srpkHeader);

	char initialPadding[STARPAK_DATABLOCK_ALIGNMENT - sizeof(PakStreamSetFileHeader_s)];
	memset(initialPadding, STARPAK_DATABLOCK_ALIGNMENT_PADDING, sizeof(initialPadding));

	out.Write(initialPadding, sizeof(initialPadding));
}

//-----------------------------------------------------------------------------
// Purpose: writes the sorts table and finishes the stream file stream
//-----------------------------------------------------------------------------
void CStreamFileBuilder::FinishStreamFileStream(const PakStreamSet_e set)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	BinaryIO& out = isMandatory ? m_mandatoryStreamFile : m_optionalStreamFile;

	if (!out.IsWritable())
		return; // Never opened.

	// starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
	const auto& vecData = isMandatory ? m_mandatoryStreamingDataBlocks : m_optionalStreamingDataBlocks;

	for (const PakStreamSetAssetEntry_s& it : vecData)
		out.Write(it);

	const size_t entryCount = isMandatory ? GetMandatoryStreamingAssetCount() : GetOptionalStreamingAssetCount();
	out.Write(entryCount);

	const char* const streamFileName = isMandatory ? m_mandatoryStreamFileName : m_optionalStreamFileName;

	Log("Built %s streaming file \"%s\" with %zu assets, totaling %zd bytes.\n",
		Pak_StreamSetToName(set), streamFileName, entryCount, (ssize_t)out.GetSize());

	out.Close();
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak data entry
//-----------------------------------------------------------------------------
bool CStreamFileBuilder::AddStreamingDataEntry(const int64_t size, const uint8_t* const data,
	const PakStreamSet_e set, StreamAddEntryResults_s& outResults)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	const char* const newStarPak = isMandatory ? m_mandatoryStreamFileName : m_optionalStreamFileName;

	StreamCacheFindParams_s params = m_streamCache.CreateParams(data, size, newStarPak);
	StreamCacheFindResult_s result;

	if (m_streamCache.Find(params, result, !isMandatory))
	{
		outResults.streamFile = result.fileEntry->streamFilePath.c_str();
		outResults.pathIndex = result.dataEntry->pathIndex;
		outResults.dataOffset = result.dataEntry->dataOffset;

		return false; // Data wasn't added, but mapped to existing data.
	}

	BinaryIO& out = isMandatory ? m_mandatoryStreamFile : m_optionalStreamFile;

	if (!out.IsWritable())
		Error("Attempted to write %s streaming asset without a stream file handle.\n", Pak_StreamSetToName(set));

	const int64_t dataOffset = out.GetSize();
	assert(dataOffset >= STARPAK_DATABLOCK_ALIGNMENT);

	out.Write(data, size);
	const int64_t paddedSize = IALIGN(size, STARPAK_DATABLOCK_ALIGNMENT);

	// starpak data is aligned to 4096 bytes, pad the remainder out for the next asset.
	if (paddedSize > size)
	{
		const size_t paddingRemainder = paddedSize - size;
		out.Pad(paddingRemainder);
	}

	std::vector<PakStreamSetAssetEntry_s>& dataBlockDescs = isMandatory ? m_mandatoryStreamingDataBlocks : m_optionalStreamingDataBlocks;
	PakStreamSetAssetEntry_s& desc = dataBlockDescs.emplace_back();

	desc.offset = dataOffset;
	desc.size = paddedSize;

	outResults.streamFile = newStarPak;
	outResults.dataOffset = dataOffset;

	m_streamCache.Add(params, dataOffset, !isMandatory);
	return true;
}
