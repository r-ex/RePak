#include "pch.h"

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

template <typename T>
void Utils::WriteVector(BinaryIO& out, std::vector<T>& dataVector)
{
	for (auto it = dataVector.begin(); it != dataVector.end(); ++it)
	{
		out.write(*it);
	}
}

FILETIME Utils::GetFileTimeBySystem()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ft;
}