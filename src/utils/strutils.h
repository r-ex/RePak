#pragma once

namespace Utils
{
	typedef char FourCCString_t[5];
	void FourCCToString(FourCCString_t& buf, const unsigned int n);

	std::vector<std::string> StringSplit(std::string input, const char delim, const size_t max = SIZE_MAX);
	const std::string VFormat(const char* const zcFormat, ...);
};
