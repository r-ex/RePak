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
bool JSON_ParseFromFile(const char* const assetPath, const char* const debugName, rapidjson::Document& document, const bool mandatory)
{
    std::ifstream ifs(assetPath);

    if (!ifs)
    {
        // Note: mandatory only prevents the error if the file doesn't exist,
        // if there are parsing or validation problems, we will still error as
        // these are considered unintentional problems.
        if (mandatory)
        {
            if (g_jsonErrorCallback)
                g_jsonErrorCallback("%s: couldn't open %s file.\n", __FUNCTION__, debugName);
        }

        return false;
    }

    rapidjson::IStreamWrapper jsonStreamWrapper(ifs);

    if (document.ParseStream(jsonStreamWrapper).HasParseError())
    {
        if (g_jsonErrorCallback)
        {
            g_jsonErrorCallback("%s: %s parse error at position %zu: [%s].\n", __FUNCTION__, debugName,
                document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError()));
        }

        return false;
    }

    if (!document.IsObject())
    {
        if (g_jsonErrorCallback)
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
