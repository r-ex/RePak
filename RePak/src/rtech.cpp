#include "pch.h"
#include "rtech.h"

std::uint64_t __fastcall RTech::StringToGuid(const char* pData)
{
	std::uint32_t* v1; // r8
	std::uint64_t         v2; // r10
	int                   v3; // er11
	std::uint32_t         v4; // er9
	std::uint32_t          i; // edx
	std::uint64_t         v6; // rcx
	int                   v7; // er9
	int                   v8; // edx
	int                   v9; // eax
	std::uint32_t        v10; // er8
	int                  v12; // ecx
	std::uint32_t* a1 = (std::uint32_t*)pData;

	v1 = a1;
	v2 = 0i64;
	v3 = 0;
	v4 = (*a1 - 45 * ((~(*a1 ^ 0x5C5C5C5Cu) >> 7) & (((*a1 ^ 0x5C5C5C5Cu) - 0x1010101) >> 7) & 0x1010101)) & 0xDFDFDFDF;
	for (i = ~*a1 & (*a1 - 0x1010101) & 0x80808080; !i; i = v8 & 0x80808080)
	{
		v6 = v4;
		v7 = v1[1];
		++v1;
		v3 += 4;
		v2 = ((((std::uint64_t)(0xFB8C4D96501i64 * v6) >> 24) + 0x633D5F1 * v2) >> 61) ^ (((std::uint64_t)(0xFB8C4D96501i64 * v6) >> 24)
			+ 0x633D5F1 * v2);
		v8 = ~v7 & (v7 - 0x1010101);
		v4 = (v7 - 45 * ((~(v7 ^ 0x5C5C5C5Cu) >> 7) & (((v7 ^ 0x5C5C5C5Cu) - 0x1010101) >> 7) & 0x1010101)) & 0xDFDFDFDF;
	}
	v9 = -1;
	v10 = (i & -(signed)i) - 1;
	if (_BitScanReverse((unsigned long*)&v12, v10))
	{
		v9 = v12;
	}
	return 0x633D5F1 * v2 + ((0xFB8C4D96501i64 * (std::uint64_t)(v4 & v10)) >> 24) - 0xAE502812AA7333i64 * (std::uint32_t)(v3 + v9 / 8);
}

std::uint32_t __fastcall RTech::StringToUIMGHash(const char* str)
{
	std::uint64_t r = StringToGuid(str);
	unsigned int l = (r & 0xFFFFFFFF);
	unsigned int h = (r >> 32);

	unsigned int x = l ^ h;

	return x;
}