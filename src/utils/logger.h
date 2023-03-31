#pragma once

// non-fatal errors/issues
void Warning(const char* fmt, ...);
// fatal errors
void Error(const char* fmt, ...);
// general prints for Release
void Log(const char* fmt, ...);
// any prints that shouldn't be used in Release
void Debug(const char* fmt, ...);
