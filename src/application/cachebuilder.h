#pragma once
#include <filesystem>
#include <utils/utils.h>

REPAK_BEGIN_NAMESPACE(CacheBuilder)

bool BuildCacheFileFromGamePaksDirectory(const std::filesystem::path& directoryPath);

REPAK_END_NAMESPACE()