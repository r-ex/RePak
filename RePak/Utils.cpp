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

void Warning(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg = "WARNING: " + std::string(fmt);

	vprintf(msg.c_str(), args);

	va_end(args);
}

void Log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

void Debug(const char* fmt, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, fmt);

	std::string msg = "[D] " + std::string(fmt);

	vprintf(msg.c_str(), args);

	va_end(args);
#endif
}