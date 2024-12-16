#include "pch.h"
#include "assets/assets.h"
#include "logic/pakfile.h"

#include <application/cachebuilder.h>

const char startupVersion[] = {
    "RePak - Built "
    __DATE__
    " "
    __TIME__
    "\n\n"
};

int main(int argc, char** argv)
{
    printf(startupVersion);
    g_jsonErrorCallback = Error;

    if (argc < 2)
        Error("invalid usage\n");

    const std::string targetPath = argv[1];

    // this should be changed to proper CLI handling and mode selection 
    if (std::filesystem::is_directory(targetPath))
    {
        CacheBuilder::BuildCacheFileFromGamePaksDirectory(targetPath);
    }
    else
    {
        CPakFile pakFile(8);
        pakFile.BuildFromMap(targetPath);
    }

    return EXIT_SUCCESS;
}