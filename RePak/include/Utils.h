#pragma once

namespace Utils
{
	uintmax_t GetFileSize(std::string filename);
	
	size_t PadBuffer(char** buf, size_t size, size_t alignment);

	template <typename T> void WriteVector(BinaryIO& out, std::vector<T>& dataVector)
	{
		for (auto it = dataVector.begin(); it != dataVector.end(); ++it)
		{
			out.write(*it);
		}
	}

	size_t WriteStringVector(BinaryIO& out, std::vector<std::string>& dataVector);

	FILETIME GetFileTimeBySystem();

	void AppendSlash(std::string& in);
};

// non-fatal errors/issues
void Warning(const char* fmt, ...);
// general prints for Release
void Log(const char* fmt, ...);
// any prints that shouldnt be used in Release
void Debug(const char* fmt, ...);