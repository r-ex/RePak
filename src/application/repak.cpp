#include "pch.h"
#include "assets/assets.h"
#include "logic/pakfile.h"

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

    if (argc < 2)
        Error("invalid usage\n");

    CPakFile pakFile(8);
    pakFile.BuildFromMap(argv[1]);

    return EXIT_SUCCESS;
}