#include "pch.h"
#include "logger.h"

void Warning(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg = "WARNING: " + std::string(fmt);

	vprintf(msg.c_str(), args);

	va_end(args);
}

void Error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg = "ERROR: " + std::string(fmt);

	vprintf(msg.c_str(), args);

	va_end(args);

	exit(EXIT_FAILURE);
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
#else
	UNUSED(fmt);
#endif
}