#pragma once

namespace Utils
{
	uintmax_t GetFileSize(std::string filename);
	
	template <typename T> void WriteVector(BinaryIO& out, std::vector<T>& dataVector)
	{
		for (auto it = dataVector.begin(); it != dataVector.end(); ++it)
		{
			out.write(*it);
		}
	}

	FILETIME GetFileTimeBySystem();
};

// non-fatal errors/issues
void Warning(const char* fmt, ...);
// general prints for Release
void Log(const char* fmt, ...);
// any prints that shouldnt be used in Release
void Debug(const char* fmt, ...);