#pragma once

namespace Utils
{
	uintmax_t GetFileSize(std::string filename);

	template <typename T>
	void WriteVector(BinaryIO& out, std::vector<T>& dataVector);

	FILETIME GetFileTimeBySystem();
};

