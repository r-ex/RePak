#pragma once

// non-fatal errors/issues
void Warning(_Printf_format_string_ const char* fmt, ...);
// fatal errors
void Error(_Printf_format_string_ const char* fmt, ...);
// general prints for Release
void Log(_Printf_format_string_ const char* fmt, ...);
// any prints that shouldn't be used in Release
void Debug(_Printf_format_string_ const char* fmt, ...);
