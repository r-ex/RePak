#include "pch.h"
#include "Utils.h"

uintmax_t Utils::GetFileSize(std::string filename)
{
	try {
		return std::filesystem::file_size(filename);
	}
	catch (std::filesystem::filesystem_error& e) {
		std::cout << e.what() << '\n';
		exit(0);
	}
}

FILETIME Utils::GetFileTimeBySystem()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ft;
}