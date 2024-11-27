#include "pch.h"
#include "rtech.h"

// compute a guid from input string data that resides in a memory address that
// is aligned to a 4 byte boundary
static PakGuid_t StringToGuidAligned(const char* string)
{
	uint64_t         v1; // r9
	int               i; // r11d
	uint32_t         v4; // edi
	int              v5; // ebp
	int              v6; // r10d
	uint32_t         v7; // ecx
	uint32_t         v8; // edx
	uint32_t         v9; // eax
	uint32_t        v10; // r8d
	int64_t         v11; // r10
	uint64_t        v12; // r8
	int             v13; // eax
	int             v15; // ecx

	v1 = 0ull;
	for (i = 0; ; i += 4)
	{
		v4 = ~*(uint32_t*)string & (*(uint32_t*)string - 0x1010101) & 0x80808080;
		v5 = v4 ^ (v4 - 1);
		v6 = v5 & *(uint32_t*)string ^ 0x5C5C5C5C;
		v7 = ~v6 & (v6 - 0x1010101) & 0x80808080;
		v8 = v7 & -(int32_t)v7;
		if (v7 != v8)
		{
			v9 = 0xFF000000;
			do
			{
				v10 = v9;
				if ((v9 & v6) == 0)
					v8 |= v9 & 0x80808080;
				v9 >>= 8;
			} while (v10 >= 0x100);
		}
		v11 = 0x633D5F1 * v1;
		v12 = (0xFB8C4D96501i64 * (uint64_t)(((v5 & *(uint32_t*)string) - 45 * (v8 >> 7)) & 0xDFDFDFDF)) >> 24;
		if (v4)
			break;
		string += 4;
		v1 = ((v11 + v12) >> 61) ^ (v11 + v12);
	}
	v13 = -1;
	if (_BitScanReverse((unsigned long*)&v15, v5))
		v13 = v15;
	return v12 + v11 - 0xAE502812AA7333i64 * (uint32_t)(i + v13 / 8);
}

// compute a guid from input string data that resides in a memory address that
// isn't aligned to a 4 byte boundary
static PakGuid_t StringToGuidUnaligned(const char* string)
{
	uint64_t        v1; // rbx
	uint64_t        v2; // r10
	int              i; // esi
	int             v4; // edx
	uint32_t        v5; // edi
	int             v6; // ebp
	int             v7; // edx
	uint32_t        v8; // ecx
	uint32_t        v9; // r8d
	uint32_t       v10; // eax
	uint32_t       v11; // r9d
	int64_t        v12; // r9
	uint64_t       v13; // r8
	int            v14; // eax
	int            v16; // ecx

	v1 = 0ull;
	v2 = (uint64_t)(string + 3);
	for (i = 0; ; i += 4)
	{
		if ((v2 ^ (v2 - 3)) >= 0x1000)
		{
			v4 = *(uint8_t*)(v2 - 3);
			if ((uint8_t)v4)
			{
				v4 = *(uint16_t*)(v2 - 3);
				if (*(uint8_t*)(v2 - 2))
				{
					v4 |= *(uint8_t*)(v2 - 1) << 16;
					if (*(uint8_t*)(v2 - 1))
						v4 |= *(uint8_t*)v2 << 24;
				}
			}
		}
		else
		{
			v4 = *(uint32_t*)(v2 - 3);
		}
		v5 = ~v4 & (v4 - 0x1010101) & 0x80808080;
		v6 = v5 ^ (v5 - 1);
		v7 = v6 & v4;
		v8 = ~(v7 ^ 0x5C5C5C5C) & ((v7 ^ 0x5C5C5C5C) - 0x1010101) & 0x80808080;
		v9 = v8 & -(int32_t)v8;
		if (v8 != v9)
		{
			v10 = 0xFF000000;
			do
			{
				v11 = v10;
				if ((v10 & (v7 ^ 0x5C5C5C5C)) == 0)
					v9 |= v10 & 0x80808080;
				v10 >>= 8;
			} while (v11 >= 0x100);
		}
		v12 = 0x633D5F1 * v1;
		v13 = (0xFB8C4D96501i64 * (uint64_t)((v7 - 45 * (v9 >> 7)) & 0xDFDFDFDF)) >> 24;
		if (v5)
			break;
		v2 += 4i64;
		v1 = ((v12 + v13) >> 61) ^ (v12 + v13);
	}
	v14 = -1;
	if (_BitScanReverse((unsigned long*)&v16, v6))
		v14 = v16;
	return v13 + v12 - 0xAE502812AA7333i64 * (uint32_t)(i + v14 / 8);
}

PakGuid_t RTech::StringToGuid(const char* const string)
{
	return ((uintptr_t)string & 3)
		? StringToGuidUnaligned(string)
		: StringToGuidAligned(string);
}

std::uint32_t RTech::StringToUIMGHash(const char* const str)
{
	const PakGuid_t guid = RTech::StringToGuid(str);

	const unsigned int l = (guid & 0xFFFFFFFF);
	const unsigned int h = (guid >> 32);

	return l ^ h;
}

PakGuid_t RTech::GetAssetGUIDFromString(const char* const str, const bool forceRpakExtension)
{
	if (strlen(str) == 0)
		return 0;

	PakGuid_t guid = 0;

	// check for upper and lower case hex guids (e.g. 0x5DCAT, 0x5dcat)
	if (!sscanf_s(str, "0x%llX", &guid) && !sscanf_s(str, "0x%llx", &guid))
	{
		if (forceRpakExtension)
			guid = RTech::StringToGuid(Utils::ChangeExtension(str, ".rpak").c_str());
		else
			guid = RTech::StringToGuid(str);
	}

	return guid;
}

bool RTech::ParseGUIDFromString(const char* const str, PakGuid_t* const pGuid)
{
	PakGuid_t guid = 0;

	const bool found = sscanf_s(str, "0x%llX", &guid) || sscanf_s(str, "0x%llx", &guid);

	if (found && pGuid != nullptr)
		*pGuid = guid;

	return found;
}