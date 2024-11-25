//===========================================================================//
//
// Purpose: A set of utilities for RapidJSON
//
//===========================================================================//
#include "pch.h"
#include "jsonutils.h"

//-----------------------------------------------------------------------------
// Purpose: dumping a json document to a string buffer.
//-----------------------------------------------------------------------------
void JSON_DocumentToBufferDeserialize(const rapidjson::Document& document, rapidjson::StringBuffer& buffer, unsigned int indent)
{
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writer.SetIndent(' ', indent);
    document.Accept(writer);
}

