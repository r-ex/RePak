#pragma once

namespace Utils
{
	uintmax_t GetFileSize(std::string filename);
	
	size_t PadBuffer(char** buf, size_t size, size_t alignment);

	size_t WriteStringVector(BinaryIO& out, std::vector<std::string>& dataVector);

	FILETIME GetFileTimeBySystem();

	void AppendSlash(std::string& in);

	std::string ChangeExtension(const std::string& in, const std::string& ext);
};

// non-fatal errors/issues
void Warning(const char* fmt, ...);
// fatal errors
void Error(const char* fmt, ...);
// general prints for Release
void Log(const char* fmt, ...);
// any prints that shouldnt be used in Release
void Debug(const char* fmt, ...);

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
