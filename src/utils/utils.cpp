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
size_t Utils::WriteStringVector(BinaryIO& out, const std::vector<std::string>& dataVector)
{
	size_t lenTotal = 0;
	for (auto it = dataVector.begin(); it != dataVector.end(); ++it)
	{
		// NOTE: +1 because we need to take the null char into account too.
		const size_t lenPath = (*it).length() + 1;
		lenTotal += lenPath;

		out.Write((*it).c_str(), lenPath);
	}
	return lenTotal;
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
        Error("Couldn't open map file \"%s\".\n", path.string().c_str());

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
        Error("Failed to parse map file %s: \n\nLine %i, Column %i\n%s\n\n%s%s%s\n",
            path.string().c_str(),
            lineNum, columnNum,
            GetParseError_En(doc.GetParseError()),
            lastLine.c_str(), curLine.c_str(), (std::string(columnNum, ' ') += '^').c_str());
    }
}

PakGuid_t Pak_ParseGuid(const rapidjson::Value& val, bool* const success)
{
    PakGuid_t guid;

    // Try parsing it out from number
    if (JSON_ParseNumber(val, guid))
    {
        if (success) *success = true;
        return guid;
    }

    // Parse it from string
    if (val.IsString())
    {
        if (success) *success = true;
        return RTech::StringToGuid(val.GetString());
    }

    if (success) *success = false;
    return 0;
}

PakGuid_t Pak_ParseGuid(const rapidjson::Value& val, const char* const member, bool* const success)
{
    rapidjson::Value::ConstMemberIterator it;

    if (JSON_GetIterator(val, member, it))
        return Pak_ParseGuid(it->value, success);

    if (success) *success = false;
    return 0;
}

PakGuid_t Pak_ParseGuidDefault(const rapidjson::Value& val, const char* const member, const PakGuid_t fallback)
{
    bool success;
    const PakGuid_t guid = Pak_ParseGuid(val, member, &success);

    if (success)
        return guid;

    return fallback;
}

PakGuid_t Pak_ParseGuidDefault(const rapidjson::Value& val, const char* const member, const char* const fallback)
{
    bool success;
    const PakGuid_t guid = Pak_ParseGuid(val, member, &success);

    if (success)
        return guid;

    return RTech::StringToGuid(fallback);
}

PakGuid_t Pak_ParseGuidRequired(const rapidjson::Value& val, const char* const member)
{
    bool success;
    const PakGuid_t guid = Pak_ParseGuid(val, member, &success);

    if (!success)
        Error("%s: failed to parse field \"%s\".\n", __FUNCTION__, member);

    return guid;
}

//-----------------------------------------------------------------------------
// purpose: check if we have an override guid, and return that, else compute it
//          from the given asset path.
// NOTE   : this should be the only function used to get guids for asset entries
//-----------------------------------------------------------------------------
PakGuid_t Pak_GetGuidOverridable(const rapidjson::Value& mapEntry, const char* const assetPath)
{
    PakGuid_t assetGuid;

    if (JSON_ParseNumber(mapEntry, "$guid", assetGuid))
    {
        if (assetGuid == 0)
            Error("%s: invalid GUID override provided for asset \"%s\".\n", __FUNCTION__, assetPath);

        return assetGuid;
    }

    return RTech::StringToGuid(assetPath);
}

size_t Pak_ExtractAssetStem(const char* const assetPath, char* const outBuf, const size_t outBufLen)
{
    // skip 'texture/'
    const char* bufPos = strchr(assetPath, '/');

    if (!bufPos)
        bufPos = assetPath;
    else
        bufPos += 1; // skip the '/'.

    // copy until '.rpak' or '\0'
    size_t i = 0;
    while (*bufPos != '\0' && *bufPos != '.')
    {
        if (i == outBufLen)
            Error("%s: ran out of space on %s.\n", __FUNCTION__, assetPath);

        outBuf[i++] = *bufPos;
        bufPos++;
    }

    outBuf[i] = '\0';
    return i;
}
