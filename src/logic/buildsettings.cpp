//=============================================================================//
//
// Pak build settings manager
//
//=============================================================================//
#include "pch.h"
#include "utils/utils.h"

#include "buildsettings.h"

CBuildSettings::CBuildSettings()
{
	m_pakVersion = 0;
	m_buildFlags = 0;
	m_compressLevel = 0;
	m_compressWorkers = 0;
}

void CBuildSettings::Init(const js::Document& doc, const char* const buildMapFile)
{
	m_pakVersion = JSON_GetValueOrDefault(doc, "version", -1);

	if (m_pakVersion < 0)
		Error("No \"version\" field provided.\n");

	Utils::ResolvePath(m_workingDirectory, buildMapFile);

	// Determine final build path from map file.
	const char* const outputDir = JSON_GetValueRequired<const char*>(doc, "outputDir");
	m_outputPath = m_workingDirectory + outputDir;

	// Create output directory if it does not exist yet.
	fs::create_directories(m_outputPath);

	// Should dev-only data be kept - e.g. texture asset names, shader names, etc.
	if (JSON_GetValueOrDefault(doc, "keepDevOnly", false))
		AddFlags(PF_KEEP_DEV);

	if (JSON_GetValueOrDefault(doc, "keepServerOnly", true))
		AddFlags(PF_KEEP_SERVER);

	if (JSON_GetValueOrDefault(doc, "keepClientOnly", true))
		AddFlags(PF_KEEP_CLIENT);

	m_compressLevel = JSON_GetValueOrDefault(doc, "compressLevel", 0);

	if (m_compressLevel > 0)
		m_compressWorkers = JSON_GetValueOrDefault(doc, "compressWorkers", 0);
}
