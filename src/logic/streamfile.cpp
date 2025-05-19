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
}

//-----------------------------------------------------------------------------
// Purpose: parse and initialize
//-----------------------------------------------------------------------------
void CStreamFileBuilder::Init(const js::Document& doc, const bool useOptional)
{
	rapidjson::Value::ConstMemberIterator mandatoryIt;

	if (JSON_GetIterator(doc, "streamFileMandatory", JSONFieldType_e::kString, mandatoryIt))
	{
		m_mandatoryStreamFileName.assign(mandatoryIt->value.GetString(), mandatoryIt->value.GetStringLength());
		Utils::FixSlashes(m_mandatoryStreamFileName);

		CreateStreamFileStream(m_mandatoryStreamFileName, STREAMING_SET_MANDATORY);
	}

	rapidjson::Value::ConstMemberIterator optionalIt;

	if (useOptional && JSON_GetIterator(doc, "streamFileOptional", JSONFieldType_e::kString, optionalIt))
	{
		m_optionalStreamFileName.assign(optionalIt->value.GetString(), optionalIt->value.GetStringLength());
		Utils::FixSlashes(m_optionalStreamFileName);

		CreateStreamFileStream(m_optionalStreamFileName, STREAMING_SET_OPTIONAL);
	}

	rapidjson::Value::ConstMemberIterator streamCacheIt;

	if (JSON_GetIterator(doc, "streamCache", JSONFieldType_e::kString, streamCacheIt))
	{
		fs::path streamCacheDirFs(std::move(std::string(streamCacheIt->value.GetString(), streamCacheIt->value.GetStringLength())));
		std::string streamCacheDirStr = streamCacheDirFs.parent_path().string();

		Utils::ResolvePath(streamCacheDirStr, m_buildSettings->GetBuildMapPath());
		streamCacheDirStr.append(streamCacheDirFs.filename().string());

		Log("Loading cache from streaming map file \"%s\".\n", streamCacheDirStr.c_str());
		m_streamCache.ParseMap(streamCacheDirStr.c_str());

		rapidjson::Value::ConstMemberIterator filterIt;

		if (JSON_GetIterator(doc, "streamCacheFilter", JSONFieldType_e::kArray, filterIt))
		{
			const rapidjson::Value::ConstArray filterArray = filterIt->value.GetArray();
			int filterIdx = -1;

			for (const rapidjson::Value& filtered : filterArray)
			{
				filterIdx++;

				if (!JSON_IsOfType(filtered, JSONFieldType_e::kString))
				{
					Error("Element #%i in array \"%s\" must be a %s.\n",
						filterIdx, "streamCacheFilter", JSON_TypeToString(JSONFieldType_e::kString));
				}

				m_streamCache.AddStreamFileToFilter(filtered.GetString(), filtered.GetStringLength());
			}

			if (!filterArray.Empty())
			{
				if (!m_mandatoryStreamFileName.empty())
					m_streamCache.AddStreamFileToFilter(m_mandatoryStreamFileName);

				if (!m_optionalStreamFileName.empty())
					m_streamCache.AddStreamFileToFilter(m_optionalStreamFileName);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: shutdown
//-----------------------------------------------------------------------------
void CStreamFileBuilder::Shutdown()
{
	FinishStreamFileStream(STREAMING_SET_MANDATORY);
	FinishStreamFileStream(STREAMING_SET_OPTIONAL);

	const std::string& streamFile = !m_mandatoryStreamFileName.empty()
		? m_mandatoryStreamFileName 
		: m_optionalStreamFileName;

	if (!streamFile.empty())
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
void CStreamFileBuilder::CreateStreamFileStream(const std::string& streamFilePath, const PakStreamSet_e set)
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

	const std::string& streamFileName = isMandatory ? m_mandatoryStreamFileName : m_optionalStreamFileName;

	Log("Built %s streaming file \"%s\" with %zu assets, totaling %zd bytes.\n",
		Pak_StreamSetToName(set), streamFileName.c_str(), entryCount, (ssize_t)out.GetSize());

	out.Close();
}

//-----------------------------------------------------------------------------
// purpose: adds new starpak data entry
//-----------------------------------------------------------------------------
bool CStreamFileBuilder::AddStreamingDataEntry(const int64_t size, const uint8_t* const data,
	const PakStreamSet_e set, StreamAddEntryResults_s& outResults)
{
	const bool isMandatory = set == STREAMING_SET_MANDATORY;
	const std::string& newStarPak = isMandatory ? m_mandatoryStreamFileName : m_optionalStreamFileName;

	StreamCacheFindParams_s params = m_streamCache.CreateParams(data, size, newStarPak.c_str());
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

	outResults.streamFile = newStarPak.c_str();
	outResults.dataOffset = dataOffset;

	m_streamCache.Add(params, dataOffset, !isMandatory);
	return true;
}
