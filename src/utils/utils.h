#pragma once

namespace Utils
{
	uintmax_t GetFileSize(const std::string& filename);
	FILETIME GetSystemFileTime();
	
	size_t PadBuffer(char** buf, size_t size, size_t alignment);
	size_t WriteStringVector(BinaryIO& out, const std::vector<std::string>& dataVector);

	void AppendSlash(std::string& in);
	std::string ChangeExtension(const std::string& in, const std::string& ext);

	void ParseMapDocument(js::Document& doc, const fs::path& path);
};

extern PakGuid_t Pak_ParseGuid(const rapidjson::Value& val, bool* const success = nullptr);
extern PakGuid_t Pak_ParseGuid(const rapidjson::Value& val, const char* const member, bool* const success = nullptr);
extern PakGuid_t Pak_ParseGuidDefault(const rapidjson::Value& val, const char* const member, const PakGuid_t fallback);
extern PakGuid_t Pak_ParseGuidDefault(const rapidjson::Value& val, const char* const member, const char* const fallback);
extern PakGuid_t Pak_ParseGuidRequired(const rapidjson::Value& val, const char* const member);

extern PakGuid_t Pak_GetGuidOverridable(const rapidjson::Value& mapEntry, const char* const assetPath);

extern size_t Pak_ExtractAssetStem(const char* const assetPath, char* const outBuf, const size_t outBufLen);

using namespace std::chrono;

class CScopeTimer
{
public:
	CScopeTimer(const char* const name)
	{
		m_name = name;
		m_startTime = system_clock::now().time_since_epoch();
	}

	~CScopeTimer()
	{
		system_clock::duration now = system_clock::now().time_since_epoch();
		printf("%s: finished in %.3fms.\n", m_name, duration_cast<microseconds>(now - m_startTime).count() / 1000.f);
	}

private:
	const char* m_name;
	system_clock::duration m_startTime;
};

#define XTIME_SCOPE2(x, y) CScopeTimer __timer_##y(x)
#define XTIME_SCOPE(x, y) XTIME_SCOPE2(x, y)
#define TIME_SCOPE(x) XTIME_SCOPE(x, __COUNTER__)

#define IALIGN(a,b)  (((a) + ((b)-1)) & ~((b)-1))

#define IALIGN2(a)   IALIGN(a,2)
#define IALIGN4(a)   IALIGN(a,4)
#define IALIGN8(a)   IALIGN(a,8)
#define IALIGN16(a)  IALIGN(a,16)
#define IALIGN32(a)  IALIGN(a,32)
#define IALIGN64(a)  IALIGN(a,64)

#define MAKE_FOURCC(a,b,c,d) ((d<<24)+(c<<16)+(b<<8)+a)

#define REPAK_BEGIN_NAMESPACE(n) namespace n {
#define REPAK_END_NAMESPACE() }
