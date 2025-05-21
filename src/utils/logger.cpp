#include "pch.h"
#include "logger.h"

const char* g_currentAsset = nullptr;
bool g_showDebugLogs = false;

static std::string s_debugColorCode;
static std::string s_warningColorCode;
static std::string s_errorColorCode;
static std::string s_resetColorCode;

void Logger_colorInit()
{
	s_debugColorCode = "\x1B[94m";
	s_warningColorCode = "\x1B[93m";
	s_errorColorCode = "\x1B[91m";
	s_resetColorCode = "\033[0m";
}

void Warning(_Printf_format_string_ const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg;

	if (g_currentAsset)
		msg = Utils::VFormat("WARNING( %s ): %s%s%s", g_currentAsset, s_warningColorCode.c_str(), fmt, s_resetColorCode.c_str());
	else
		msg = "WARNING: " + s_warningColorCode + fmt + s_resetColorCode;

	vprintf(msg.c_str(), args);
	va_end(args);
}

void Error(_Printf_format_string_ const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	std::string msg;

	if (g_currentAsset)
		msg = Utils::VFormat("ERROR( %s ): %s%s%s", g_currentAsset, s_errorColorCode.c_str(), fmt, s_resetColorCode.c_str());
	else
		msg = "ERROR: " + s_errorColorCode + fmt + s_resetColorCode;

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

	std::string msg = "[D] " + s_debugColorCode + fmt + s_resetColorCode;

	vprintf(msg.c_str(), args);
	va_end(args);
}