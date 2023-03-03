#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <sysinfoapi.h>
#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <regex>
#include <rapidcsv/rapidcsv.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "logic/rmem.h"
#include "logic/rtech.h"

#include "utils/binaryio.h"
#include "utils/utils.h"
#include "utils/logger.h"

#include "logic/pakfile.h"
#include "public/const.h"