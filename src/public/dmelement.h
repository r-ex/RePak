#pragma once
#include "dmattribute.h"

#define DM_ELEMENT_NULL -1
#define DM_ELEMENT_EXTERNAL -2

struct DmObjectId_s
{
	unsigned char value[16];
};

using DmObjectIdString_s = char[37]; // 37 is max a string representation of the guid will utilize.

struct DmElement_s
{
	DmObjectId_s id;
	DmeSymbol_t name;
	DmeSymbol_t type;
	std::vector<DmAttribute_s> attr;
};

inline const DmAttribute_s* Dme_FindAttribute(const DmElement_s& elem, const DmeSymbol_t name)
{
	for (const DmAttribute_s& at : elem.attr)
	{
		if (at.name.s == name.s)
			return &at;
	}

	return nullptr;
}
