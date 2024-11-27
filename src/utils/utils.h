#pragma once

namespace Utils
{
	uintmax_t GetFileSize(const std::string& filename);
	FILETIME GetSystemFileTime();
	
	size_t PadBuffer(char** buf, size_t size, size_t alignment);
	size_t WriteStringVector(BinaryIO& out, std::vector<std::string>& dataVector);

	void AppendSlash(std::string& in);
	std::string ChangeExtension(const std::string& in, const std::string& ext);

	void ParseMapDocument(js::Document& doc, const fs::path& path);
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

#define IALIGN(a,b)  (((a) + ((b)-1)) & ~((b)-1))

#define IALIGN2(a)   IALIGN(a,2)
#define IALIGN4(a)   IALIGN(a,4)
#define IALIGN8(a)   IALIGN(a,8)
#define IALIGN16(a)  IALIGN(a,16)
#define IALIGN32(a)  IALIGN(a,32)
#define IALIGN64(a)  IALIGN(a,64)

#define MAKE_FOURCC(a,b,c,d) ((d<<24)+(c<<16)+(b<<8)+a)

// json helper macros
// todo: move to jsonutils.h and reuse cache iterator
#define JSON_IS_BOOL(doc, name) (doc.HasMember(name) && doc[name].IsBool())
#define JSON_IS_INT64(doc, name) (doc.HasMember(name) && doc[name].IsInt64())
#define JSON_IS_UINT64(doc, name) (doc.HasMember(name) && doc[name].IsUint64())
#define JSON_IS_INT(doc, name) (doc.HasMember(name) && doc[name].IsInt())
#define JSON_IS_UINT(doc, name) (doc.HasMember(name) && doc[name].IsUint())
#define JSON_IS_STR(doc, name) (doc.HasMember(name) && doc[name].IsString())
#define JSON_IS_ARRAY(doc, name) (doc.HasMember(name) && doc[name].IsArray())
#define JSON_IS_OBJECT(doc, name) (doc.HasMember(name) && doc[name].IsObject())
#define JSON_IS_NULL(doc, name) (doc.HasMember(name) && doc[name].IsNull())

#define JSON_GET_BOOL(doc, name) (JSON_IS_BOOL(doc, name) && doc[name].GetBool())
#define JSON_GET_INT64(doc, name, defaultValue) ((JSON_IS_INT64(doc, name)) ? doc[name].GetInt64() : defaultValue)
#define JSON_GET_UINT64(doc, name, defaultValue) ((JSON_IS_UINT64(doc, name)) ? doc[name].GetUint64() : defaultValue)
#define JSON_GET_INT(doc, name, defaultValue) ((JSON_IS_INT(doc, name)) ? doc[name].GetInt() : defaultValue)
#define JSON_GET_UINT(doc, name, defaultValue) ((JSON_IS_UINT(doc, name)) ? doc[name].GetUint() : defaultValue)
#define JSON_GET_STR(doc, name, defaultValue) ((JSON_IS_STR(doc, name)) ? doc[name].GetString() : defaultValue)

#define REPAK_BEGIN_NAMESPACE(n) namespace n {

#define REPAK_END_NAMESPACE() }
