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

