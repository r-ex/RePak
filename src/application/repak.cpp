#include "pch.h"
#include "assets/assets.h"
#include "logic/buildsettings.h"
#include "logic/pakfile.h"
#include "logic/streamfile.h"
#include "logic/streamcache.h"

const char startupVersion[] = {
    "RePak - Built "
    __DATE__
    " "
    __TIME__
    "\n\n"
};

static void RePak_Init(const js::Document& doc, const char* const mapPath, CBuildSettings& settings, CStreamFileBuilder& streamBuilder)
{
    settings.Init(doc, mapPath);

    const bool keepClient = settings.IsFlagSet(PF_KEEP_CLIENT);

    // Server-only paks never uses streaming assets.
    if (keepClient)
        streamBuilder.Init(doc, settings.GetPakVersion() >= 8);
}

static void RePak_Shutdown(CBuildSettings& settings, CStreamFileBuilder& streamBuilder)
{
    const bool keepClient = settings.IsFlagSet(PF_KEEP_CLIENT);

    if (keepClient)
        streamBuilder.Shutdown();
}

static void RePak_ParseListedDocument(js::Document& doc, const char* const docPath, const char* const docName)
{
    Log("Parsing listed build map \"%s\".\n", docName);
    std::string finalName = docName;

    Utils::ResolvePath(finalName, docPath);
    JSON_ParseFromFile(finalName.c_str(), "listed build map", doc, true);
}

static void RePak_BuildSingle(const js::Document& doc, const char* const mapPath)
{
    CBuildSettings settings;
    CStreamFileBuilder streamBuilder(&settings);

    RePak_Init(doc, mapPath, settings, streamBuilder);

    CPakFileBuilder pakFile(&settings, &streamBuilder);
    pakFile.BuildFromMap(doc);

    RePak_Shutdown(settings, streamBuilder);
}

static void RePak_BuildFromList(const js::Document& doc, const js::Value& list, const char* const mapPath)
{
    if (!list.IsArray())
    {
        Error("Pak build list is of type %s, but code expects %s.\n",
            JSON_TypeToString(JSON_ExtractType(list)), JSON_TypeToString(JSONFieldType_e::kArray));
    }

    CBuildSettings settings;
    CStreamFileBuilder streamBuilder(&settings);

    RePak_Init(doc, mapPath, settings, streamBuilder);

    ssize_t i = -1;

    for (const js::Value& pak : list.GetArray())
    {
        i++;

        if (!pak.IsString())
        {
            Error("Pak #%zd in build list is of type %s, but code expects %s.\n",
                i, JSON_TypeToString(JSON_ExtractType(pak)), JSON_TypeToString(JSONFieldType_e::kString));
        }

        js::Document pakDoc;
        RePak_ParseListedDocument(pakDoc, settings.GetBuildMapPath(), pak.GetString());

        CPakFileBuilder pakFile(&settings, &streamBuilder);
        pakFile.BuildFromMap(pakDoc);
    }

    RePak_Shutdown(settings, streamBuilder);
}

static void RePak_HandleBuild(const char* const arg)
{
    fs::path starmapPath(arg);

    // this should be changed to proper CLI handling and mode selection 
    if (std::filesystem::is_directory(starmapPath))
    {
        starmapPath.append("pc_roots.starmap");
        const std::string starmapStreamStr = starmapPath.string();

        CStreamCache writeCache;
        writeCache.BuildMapFromGamePaks(starmapStreamStr.c_str());
    }
    else
    {
        // load and parse map file, this file is essentially the
        // control file; deciding what is getting packed, etc..
        js::Document doc;
        JSON_ParseFromFile(arg, "main build map", doc, true);

        js::Value::ConstMemberIterator paksIt;

        if (JSON_GetIterator(doc, "paks", paksIt))
            RePak_BuildFromList(doc, paksIt->value, arg);
        else
            RePak_BuildSingle(doc, arg);
    }
}

static void RePak_ExplainUsage()
{
    printf(startupVersion);
    Error("invalid usage\n");
}

static inline void RePak_ValidateArguments(const char* const argName, const int argc, const int required)
{
    const int delta = required - argc;

    if (delta > 0)
    {
        if (delta == 1)
            Error("Invalid usage; \"%s\" requires an additional argument.\n", __FUNCTION__, argName);

        Error("Invalid usage; \"%s\" requires %i additional arguments.\n", __FUNCTION__, argName, delta);
    }
}

static bool RePak_CheckCommandLine(const char* arg, const char* const target, const int argc, const int required)
{
    if (strcmp(arg, target) == 0)
    {
        RePak_ValidateArguments(target, argc, required);
        return true;
    }

    return false;
}

static void RePak_HandleCompressPak(const char* const pakPath, const int compressLevel, const int workerCount)
{
    BinaryIO bio;

    if (!bio.Open(pakPath, BinaryIO::Mode_e::ReadWrite))
        Error("Failed to open pak file \"%s\" for encode job.\n", pakPath);

    const std::streamoff size = bio.GetSize();
    const std::streamoff toConsume = 6; // size of magic( 4 ) + version( 2 ).

    if (size < toConsume)
        Error("Short read on pak file \"%s\"; header criteria unavailable.\n", pakPath);

    const uint32_t magic = bio.Read<uint32_t>();

    if (magic != RPAK_MAGIC)
        Error("Pak file \"%s\" has invalid magic! ( %x != %x ).\n", pakPath, magic, RPAK_MAGIC);

    const uint16_t version = bio.Read<uint16_t>();

    if (!Pak_IsVersionSupported(version))
        Error("Pak file \"%s\" has version %hu which is unsupported!\n", pakPath, version);

    const std::streamoff headerSize = (std::streamoff)Pak_GetHeaderSize(version);

    if (size < headerSize)
        Error("Pak file \"%s\" appears truncated! ( %zd < %zd ).\n", pakPath, size, headerSize);

    char tempHdrBuf[256];

    bio.Seek(0);
    bio.Read(tempHdrBuf, headerSize);

    PakHdr_t* const hdr = (PakHdr_t*)tempHdrBuf;

    if (hdr->flags & (PAK_HEADER_FLAGS_RTECH_ENCODED | PAK_HEADER_FLAGS_ZSTD_ENCODED))
        Error("Pak file \"%s\" is already encoded using %s!.\n", pakPath, Pak_EncodeAlgorithmToString(hdr->flags));

    const size_t newSize = Pak_EncodeStreamAndSwap(bio, compressLevel, workerCount, version, pakPath);

    // Update the header to accommodate for the compression method
    // and the new size so the runtime is aware of it.
    hdr->flags |= PAK_HEADER_FLAGS_ZSTD_ENCODED;
    hdr->compressedSize = newSize;

    bio.Seek(0); // Write the new header out.
    bio.Write(tempHdrBuf, headerSize);
}

static void RePak_HandleCommandLine(const int argc, char** argv)
{
    if (argc < 2)
    {
        RePak_ExplainUsage();
        return;
    }

    if (RePak_CheckCommandLine(argv[1], "-pakguid", argc, 3))
    {
        const PakGuid_t guid = RTech::StringToGuid(argv[2]);
        Log("0x%llX\n", guid);

        return;
    }

    if (RePak_CheckCommandLine(argv[1], "-uimghash", argc, 3))
    {
        const uint32_t hash = RTech::StringToUIMGHash(argv[2]);
        Log("0x%lX\n", hash);

        return;
    }

    if (RePak_CheckCommandLine(argv[1], "-compress", argc, 3))
    {
        g_showDebugLogs = true;
        int compressLevel = 6;

        if ((argc > 3) && (!JSON_StringToNumber(argv[3], strlen(argv[3]), compressLevel)))
            Error("%s: failed to parse compressLevel for argument \"%s\".\n", __FUNCTION__, argv[1]);

        int workerCount = 16;

        if ((argc > 4) && (!JSON_StringToNumber(argv[4], strlen(argv[4]), workerCount)))
            Error("%s: failed to parse workerCount for argument \"%s\".\n", __FUNCTION__, argv[1]);

        RePak_HandleCompressPak(argv[2], compressLevel, workerCount);
        return;
    }

    RePak_HandleBuild(argv[1]);
}

int main(int argc, char** argv)
{
    g_jsonErrorCallback = Error;

    RePak_HandleCommandLine(argc, argv);
    return EXIT_SUCCESS;
}
