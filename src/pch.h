#pragma once

//#include <windows.h>
#include <d3d11.h>

#include <filesystem>
#include <iostream>
#include <unordered_map>
//#include <sysinfoapi.h>
#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <regex>

#include <rapidcsv/rapidcsv.h>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include "rapidjson/prettywriter.h"

#include "common/decls.h"
#include "common/const.h"

#include "logic/rmem.h"
#include "logic/rtech.h"

#include "utils/binaryio.h"
#include "utils/utils.h"
#include "utils/strutils.h"
#include "utils/jsonutils.h"
#include "utils/logger.h"

#define UNUSED(x)	(void)(x)
