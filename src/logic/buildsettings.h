#pragma once

class CBuildSettings
{
public:
	CBuildSettings();

	void Init(const js::Document& doc, const char* const buildMapFile);

	inline void AddFlags(const int flags) { m_buildFlags |= flags; }
	inline bool IsFlagSet(const int flag) const { return m_buildFlags & flag; };

	inline int GetPakVersion() const { return m_pakVersion; }

	inline const char* GetBuildMapPath() const { return m_buildMapPath.c_str(); }
	inline const char* GetOutputPath() const { return m_outputPath.c_str(); }

private:
	int m_pakVersion;
	int m_buildFlags;

	std::string m_buildMapPath;
	std::string m_outputPath;
};
