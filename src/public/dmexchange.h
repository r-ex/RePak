#pragma once
#include "dmelement.h"
#include "symboltable.h"

#define DMX_VERSION_STARTING_TOKEN "<!-- dmx"
#define DMX_VERSION_ENDING_TOKEN "-->"
#define DMX_HEADER_COMPONENT_COUNT 4 // <encoding> <version> <format> <version>

struct DmxHeader_s
{
	enum
	{
		MAX_FORMAT_NAME_LENGTH = 64,
		MAX_HEADER_LENGTH = 40 + 2 * MAX_FORMAT_NAME_LENGTH,
	};

	DmxHeader_s()
		: encodingVersion(-1)
		, formatVersion(-1)
	{
		encodingName[0] = '\0';
		formatName[0] = '\0';
	}

	char encodingName[MAX_FORMAT_NAME_LENGTH];
	int encodingVersion;
	char formatName[MAX_FORMAT_NAME_LENGTH];
	int formatVersion;
};

enum DmFileId_e
{
	DMFILEID_INVALID = 0xFFFFFFFF
};

typedef CSymbolTable<false> DmSymbolTable;
typedef std::vector<DmElement_s> DmElementList;

struct DmContext_s
{
	DmSymbolTable symbolTable;
	DmElementList elementList;
};
