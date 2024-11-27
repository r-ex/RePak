//=============================================================================//
//
// purpose: various common utilities
//
//=============================================================================//
#include "pch.h"
#include "utils.h"
#include "rapidjson/error/en.h"

//-----------------------------------------------------------------------------
// purpose: gets size of the specified file
// returns: file size
//-----------------------------------------------------------------------------
uintmax_t Utils::GetFileSize(const std::string& filename) // !TODO: change to 'fs::path' instead?
{
	try {
		return std::filesystem::file_size(filename);
	}
	catch (std::filesystem::filesystem_error& e) {
		std::cout << e.what() << '\n';
		exit(0);
	}
}

//-----------------------------------------------------------------------------
// purpose: pad buffer to the specified alignment
// returns: new buffer size
//-----------------------------------------------------------------------------
size_t Utils::PadBuffer(char** buf, size_t size, size_t alignment)
{
	size_t newSize = IALIGN(size, alignment);

	char* newbuf = new char[newSize]{};
	memcpy_s(newbuf, size, *buf, size);

	delete[] *buf;

	*buf = newbuf;
	return newSize;
}

//-----------------------------------------------------------------------------
// purpose: write vector of strings to the specified BinaryIO instance
// returns: length of data written
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
// purpose: get current system time as FILETIME
//-----------------------------------------------------------------------------
FILETIME Utils::GetSystemFileTime()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ft;
}

//-----------------------------------------------------------------------------
// purpose: add backslash to the end of the string if not already present
//-----------------------------------------------------------------------------
void Utils::AppendSlash(std::string& in)
{
	char lchar = in[in.size() - 1];
	if (lchar != '\\' && lchar != '/')
		in.append("\\");
}

//-----------------------------------------------------------------------------
// purpose: replace extension with that of a new one in string
//-----------------------------------------------------------------------------
std::string Utils::ChangeExtension(const std::string& in, const std::string& ext)
{
	return std::filesystem::path(in).replace_extension(ext).string();
}

//-----------------------------------------------------------------------------
// purpose: parse json document and handle parsing errors
//-----------------------------------------------------------------------------
void Utils::ParseMapDocument(js::Document& doc, const fs::path& path)
{
    std::ifstream ifs(path);

    if (!ifs.is_open())
        Error("couldn't open map file.\n");

    // begin json parsing
    js::IStreamWrapper jsonStreamWrapper{ ifs };
    doc.ParseStream<js::ParseFlag::kParseCommentsFlag | js::ParseFlag::kParseTrailingCommasFlag>(jsonStreamWrapper);

    // handle parse errors
    if (doc.HasParseError()) {
        int lineNum = 1;
        int columnNum = 0;
        std::string lastLine = "";
        std::string curLine = "";

        size_t offset = doc.GetErrorOffset();
        ifs.clear();
        ifs.seekg(0, std::ios::beg);
        js::IStreamWrapper isw{ ifs };

        for (int i = 0; ; i++)
        {
            const char c = isw.Take();
            curLine.push_back(c);
            if (c == '\n')
            {
                if (i >= offset)
                    break;
                lastLine = curLine;
                curLine = "";
                lineNum++;
                columnNum = 0;
            }
            else
            {
                if (i < offset)
                    columnNum++;
            }
        }

        // this could probably be formatted nicer
        Error("Failed to parse map file: \n\nLine %i, Column %i\n%s\n\n%s%s%s\n",
            lineNum, columnNum,
            GetParseError_En(doc.GetParseError()),
            lastLine.c_str(), curLine.c_str(), (std::string(columnNum, ' ') += '^').c_str());
    }
}
