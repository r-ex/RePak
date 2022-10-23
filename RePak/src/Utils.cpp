#include "pch.h"
#include "Utils.h"

// purpose: gets size of the specified file
// returns: file size
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

// purpose: pad buffer to the specified alignment
// returns: new buffer size
size_t Utils::PadBuffer(char** buf, size_t size, size_t alignment)
{
	size_t extra = alignment - (size % alignment);
	size_t newSize = size + extra;

	char* newbuf = new char[newSize]{};
	memcpy_s(newbuf, size, *buf, size);

	delete[] *buf;

	*buf = newbuf;
	return newSize;
}

// purpose: write vector of strings to the specified BinaryIO instance
// returns: length of data written
size_t Utils::WriteStringVector(BinaryIO& out, std::vector<std::string>& dataVector)
{
	size_t length = 0;
	for (auto it = dataVector.begin(); it != dataVector.end(); ++it)
	{
		length += (*it).length() + 1;
		out.writeString(*it);
	}
	return length;
}

// purpose: get current system time as FILETIME
FILETIME Utils::GetFileTimeBySystem()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ft;
}

// purpose: add backslash to the end of the string if not already present
void Utils::AppendSlash(std::string& in)
{
	char lchar = in[in.size() - 1];
	if (lchar != '\\' && lchar != '/')
		in.append("\\");
}

std::string Utils::ChangeExtension(const std::string& in, const std::string& ext)
{
	return std::filesystem::path(in).replace_extension(ext).u8string();
}

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
#endif
}