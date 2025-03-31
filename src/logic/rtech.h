#pragma once

// todo(amos): reorder headers so this can be moved into public.
typedef uint64_t PakGuid_t;

namespace RTech
{
	PakGuid_t StringToGuid(const char* const string);
	std::uint32_t StringToUIMGHash(const char* const str);

	bool ParseGUIDFromString(const char* const str, PakGuid_t* const pGuid = nullptr);
	PakGuid_t GetAssetGUIDFromString(const char* const str, const bool forceRpakExtension = false);
}