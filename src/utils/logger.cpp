#include "pch.h"
#include "logger.h"

const char* g_currentAsset = nullptr;
bool g_showDebugLogs = false;

void Warning(_Printf_format_string_ const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg;

	if (g_currentAsset)
		msg = Utils::VFormat("WARNING( %s ): %s", g_currentAsset, fmt);
	else
		msg = "WARNING: " + std::string(fmt);

	vprintf(msg.c_str(), args);
	va_end(args);
}

void Error(_Printf_format_string_ const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg;

	if (g_currentAsset)
		msg = Utils::VFormat("ERROR( %s ): %s", g_currentAsset, fmt);
	else
		msg = "ERROR: " + std::string(fmt);

	vprintf(msg.c_str(), args);
	va_end(args);

	exit(EXIT_FAILURE);
}

void Log(_Printf_format_string_ const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

void Debug(_Printf_format_string_ const char* fmt, ...)
{
	if (!g_showDebugLogs)
		return;

	va_list args;
	va_start(args, fmt);

	std::string msg = "[D] " + std::string(fmt);

	vprintf(msg.c_str(), args);
	va_end(args);
}