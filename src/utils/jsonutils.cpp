//===========================================================================//
//
// Purpose: A set of utilities for RapidJSON
//
//===========================================================================//
#include "pch.h"
#include "jsonutils.h"

//-----------------------------------------------------------------------------
// Purpose: parsing a json file from stream.
//-----------------------------------------------------------------------------
bool JSON_ParseFromFile(const char* const assetPath, const char* const debugName, rapidjson::Document& document)
{
    BinaryIO jsonStream;

    if (!jsonStream.Open(assetPath, BinaryIO::Mode_e::Read))
        return false;

    const ssize_t fileSize = jsonStream.GetSize();

    if (!fileSize)
    {
        g_jsonErrorCallback("%s: %s was empty.\n", __FUNCTION__, debugName);
        return false;
    }

    std::unique_ptr<char[]> uniquebuf(new char[fileSize + 1]);
    char* const bufptr = uniquebuf.get();

    jsonStream.Read(bufptr, fileSize);
    bufptr[fileSize] = '\0';

    if (document.Parse(bufptr, fileSize).HasParseError())
    {
        g_jsonErrorCallback("%s: %s parse error at position %zu: [%s].\n", __FUNCTION__, debugName,
            document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError()));

        return false;
    }

    if (!document.IsObject())
    {
        g_jsonErrorCallback("%s: %s root was not an object.\n", __FUNCTION__, debugName);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// Purpose: dumping a json document to a string buffer.
//-----------------------------------------------------------------------------
void JSON_DocumentToBufferDeserialize(const rapidjson::Document& document, rapidjson::StringBuffer& buffer, unsigned int indent)
{
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writer.SetIndent(' ', indent);
    document.Accept(writer);
}
