#pragma once

namespace Utils
{
	uintmax_t GetFileSize(const std::string& filename);
	
	size_t PadBuffer(char** buf, size_t size, size_t alignment);

	size_t WriteStringVector(BinaryIO& out, std::vector<std::string>& dataVector);

	FILETIME GetFileTimeBySystem();

	void AppendSlash(std::string& in);

	std::string ChangeExtension(const std::string& in, const std::string& ext);
};

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

#define IALIGN2( a ) ((a + 1)  & ~1)
#define IALIGN4( a ) ((a + 3)  & ~3)
#define IALIGN8( a ) ((a + 7)  & ~7)
#define IALIGN16( a ) ((a + 15)  & ~15)
#define IALIGN32( a ) ((a + 31)  & ~31)
#define IALIGN64( a ) ((a + 63) & ~63)
