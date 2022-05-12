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
#include <rapidcsv/rapidcsv.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "rmem.h"
#include "rpak.h"
#include "rtech.h"

#include "BinaryIO.h"
#include "RePak.h"
#include "Utils.h"
