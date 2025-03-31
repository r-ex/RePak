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

void Utils::ResolvePath(std::string& outPath, const std::filesystem::path& mapPath)
{
    fs::path outputDirPath(outPath);

    if (outputDirPath.is_relative() && mapPath.has_parent_path())
    {
        try {
            outPath = fs::canonical(mapPath.parent_path() / outputDirPath).string();
        }
        catch (const fs::filesystem_error& e) {
            Error("Failed to resolve path \"%s\": %s.\n", mapPath.string().c_str(), e.what());
        }
    }
    // else we just use whatever is in outPath.

    if (!strrchr(outPath.c_str(), '.'))
    {
        // ensure that the path has a slash at the end
        Utils::AppendSlash(outPath);
    }
}

const char* Utils::ExtractFileName(const char* const string)
{
    const size_t len = strlen(string);
    const char* result = nullptr;

    for (size_t i = (len - 1); i-- > 0;)
    {
        const char c = string[i];

        if (c == '/' || c == '\\')
        {
            result = &string[i] + 1; // +1 to advance from slash.
            break;
        }
    }

    // No path, this is already the file name.
    if (!result)
        result = string;

    return result;
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

PakGuid_t Pak_ParseGuid(const rapidjson::Value& val, rapidjson::Value::StringRefType member, bool* const success)
{
    rapidjson::Value::ConstMemberIterator it;

    if (JSON_GetIterator(val, member, it))
        return Pak_ParseGuid(it->value, success);

    if (success) *success = false;
    return 0;
}

PakGuid_t Pak_ParseGuidDefault(const rapidjson::Value& val, rapidjson::Value::StringRefType member, const PakGuid_t fallback)
{
    bool success;
    const PakGuid_t guid = Pak_ParseGuid(val, member, &success);

    if (success)
        return guid;

    return fallback;
}

PakGuid_t Pak_ParseGuidDefault(const rapidjson::Value& val, rapidjson::Value::StringRefType member, const char* const fallback)
{
    bool success;
    const PakGuid_t guid = Pak_ParseGuid(val, member, &success);

    if (success)
        return guid;

    return RTech::StringToGuid(fallback);
}

PakGuid_t Pak_ParseGuidRequired(const rapidjson::Value& val, rapidjson::Value::StringRefType member)
{
    bool success;
    const PakGuid_t guid = Pak_ParseGuid(val, member, &success);

    if (!success)
        Error("%s: failed to parse field \"%s\".\n", __FUNCTION__, member.s);

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

// If the field was defined as a string, outAssetName will point to the asset's name
PakGuid_t Pak_ParseGuidFromObject(const rapidjson::Value& val, const char* const debugName,
    const char*& outAssetName)
{
    PakGuid_t resultGuid;

    if (JSON_ParseNumber(val, resultGuid))
        return resultGuid;

    if (!val.IsString())
        Error("%s: %s is of unsupported type; expected %s or %s, found %s.\n", __FUNCTION__, debugName,
            JSON_TypeToString(JSONFieldType_e::kUint64), JSON_TypeToString(JSONFieldType_e::kString),
            JSON_TypeToString(JSON_ExtractType(val)));

    if (val.GetStringLength() == 0)
        Error("%s: %s was defined as an invalid empty string.\n", __FUNCTION__, debugName);

    outAssetName = val.GetString();
    return RTech::StringToGuid(outAssetName);
}

PakGuid_t Pak_ParseGuidFromMap(const rapidjson::Value& mapEntry, rapidjson::Value::StringRefType fieldName,
    const char* const debugName, const char*& outAssetName, const bool requiredField)
{
    rapidjson::Value::ConstMemberIterator it;

    if (requiredField)
        JSON_GetRequired(mapEntry, fieldName, it);
    else
    {
        if (!JSON_GetIterator(mapEntry, fieldName, it))
            return 0;
    }

    return Pak_ParseGuidFromObject(it->value, debugName, outAssetName);
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
