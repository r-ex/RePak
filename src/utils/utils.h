#pragma once

namespace Utils
{
	uintmax_t GetFileSize(const std::string& filename);
	FILETIME GetFileTimeBySystem();
	
	size_t PadBuffer(char** buf, size_t size, size_t alignment);
	size_t WriteStringVector(BinaryIO& out, std::vector<std::string>& dataVector);

	void AppendSlash(std::string& in);
	std::string ChangeExtension(const std::string& in, const std::string& ext);

	void ParseMapDocument(js::Document& doc, const fs::path& path);

	const std::string VFormat(const char* const zcFormat, ...);
};

using namespace std::chrono;

class CScopeTimer
{
public:
	CScopeTimer(const std::string& name)
	{
		m_name = name;
		m_startTime = system_clock::now().time_since_epoch();
	}

	~CScopeTimer()
	{
		system_clock::duration now = system_clock::now().time_since_epoch();
		printf("%s: finished in %.3fms\n", m_name.c_str(), duration_cast<microseconds>(now - m_startTime).count() / 1000.f);
	}

private:
	std::string m_name;
	system_clock::duration m_startTime;
};

#define XTIME_SCOPE2(x, y) CScopeTimer __timer_##y(x)
#define XTIME_SCOPE(x, y) XTIME_SCOPE2(x, y)
#define TIME_SCOPE(x) XTIME_SCOPE(x, __COUNTER__)

#define WRITE_VECTOR(out, dataVector) for (auto it = dataVector.begin(); it != dataVector.end(); ++it) \
{ \
	out.write(*it); \
}

#define WRITE_VECTOR_PTRIO(out, dataVector) for (auto it = dataVector.begin(); it != dataVector.end(); ++it) \
{ \
	out->write(*it); \
}

#define FILE_EXISTS(path) std::filesystem::exists(path)

#define REQUIRE_FILE(path) \
	if(!FILE_EXISTS(path)) \
		Error("Unable to find required file '%s'\n", std::string(path).c_str())

#define IALIGN(a,b)  ((a + (b-1)) & ~(b-1))

#define IALIGN2(a)   IALIGN(a,2)
#define IALIGN4(a)   IALIGN(a,4)
#define IALIGN8(a)   IALIGN(a,8)
#define IALIGN16(a)  IALIGN(a,16)
#define IALIGN32(a)  IALIGN(a,32)
#define IALIGN64(a)  IALIGN(a,64)

// #define IALIGN2( a ) ((a + 1)  & ~1)
// #define IALIGN4( a ) ((a + 3)  & ~3)
// #define IALIGN8( a ) ((a + 7)  & ~7)
// #define IALIGN16( a ) ((a + 15)  & ~15)
// #define IALIGN32( a ) ((a + 31)  & ~31)
// #define IALIGN64( a ) ((a + 63) & ~63)

