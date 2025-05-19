#pragma once
#include <public/starpak.h>
#include "buildsettings.h"
#include "streamcache.h"
#include <utils/binaryio.h>

struct StreamAddEntryResults_s
{
	const char* streamFile;
	int64_t dataOffset : 52;
	int64_t pathIndex : 12;
};

class CStreamFileBuilder
{
public:
	CStreamFileBuilder(const CBuildSettings* const buildSettings);

	void Init(const js::Document& doc, const bool useOptional);
	void Shutdown();

	void CreateStreamFileStream(const char* const path, const PakStreamSet_e set);
	void FinishStreamFileStream(const PakStreamSet_e set);

	bool AddStreamingDataEntry(const int64_t size, const uint8_t* const data, const PakStreamSet_e set, StreamAddEntryResults_s& results);

	inline size_t GetMandatoryStreamingAssetCount() const { return m_mandatoryStreamingDataBlocks.size(); };
	inline size_t GetOptionalStreamingAssetCount() const { return m_optionalStreamingDataBlocks.size(); };

private:

	const CBuildSettings* m_buildSettings;

	const char* m_streamCacheFileName;
	const char* m_mandatoryStreamFileName;
	const char* m_optionalStreamFileName;

	CStreamCache m_streamCache;

	BinaryIO m_mandatoryStreamFile;
	BinaryIO m_optionalStreamFile;

	std::vector<PakStreamSetAssetEntry_s> m_mandatoryStreamingDataBlocks;
	std::vector<PakStreamSetAssetEntry_s> m_optionalStreamingDataBlocks;
};
