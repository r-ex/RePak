#include "pch.h"
#include "assets/assets.h"
#include "logic/buildsettings.h"
#include "logic/pakfile.h"
#include "logic/streamfile.h"
#include "logic/streamcache.h"
#include "utils/zstdutils.h"

#define REPAK_DEFAULT_COMPRESS_LEVEL 6
#define REPAK_DEFAULT_COMPRESS_WORKERS 16

#define REPAK_STR_TO_GUID_COMMAND "-pakguid"
#define REPAK_STR_TO_UIMG_HASH_COMMAND "-uimghash"
#define REPAK_COMPRESS_PAK_COMMAND "-compress"
#define REPAK_DECOMPRESS_PAK_COMMAND "-decompress"

static void RePak_InitBuilder(const js::Document& doc, const char* const mapPath, CBuildSettings& settings, CStreamFileBuilder& streamBuilder)
{
    settings.Init(doc, mapPath);

    const bool keepClient = settings.IsFlagSet(PF_KEEP_CLIENT);

    // Server-only paks never uses streaming assets.
    if (keepClient)
        streamBuilder.Init(doc, settings.GetPakVersion() >= 8);
}

static void RePak_ShutdownBuilder(CBuildSettings& settings, CStreamFileBuilder& streamBuilder)
{
    const bool keepClient = settings.IsFlagSet(PF_KEEP_CLIENT);

    if (keepClient)
        streamBuilder.Shutdown();
}

static void RePak_ParseListedDocument(js::Document& doc, const char* const docPath, const char* const docName)
{
    Log("*** parsing listed build map \"%s\".\n", docName);
    std::string finalName = docName;

    Utils::ResolvePath(finalName, docPath);
    JSON_ParseFromFile(finalName.c_str(), "listed build map", doc, true);
}

static void RePak_BuildSingle(const js::Document& doc, const char* const mapPath)
{
    CBuildSettings settings;
    CStreamFileBuilder streamBuilder(&settings);

    RePak_InitBuilder(doc, mapPath, settings, streamBuilder);

    CPakFileBuilder pakFile(&settings, &streamBuilder);
    pakFile.BuildFromMap(doc);

    RePak_ShutdownBuilder(settings, streamBuilder);
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

    RePak_InitBuilder(doc, mapPath, settings, streamBuilder);

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

    RePak_ShutdownBuilder(settings, streamBuilder);
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
    Log(
        "*** RePak ( built on " __DATE__ " at " __TIME__" ) usage guide ***\n"
        "For building pak files, run 'repak' with the following parameter:\n"
        "\t<%s>\t- path to a map file containing the build parameters for the pak to build\n"

        "For creating stream caches, run 'repak' with the following parameter:\n"
        "\t<%s>\t- path to a directory containing streaming files to be cached\n"

        "For calculating Pak Asset guids, run 'repak %s' with the following parameter:\n"
        "\t<%s>\t- the string to compute the asset guid from\n"

        "For calculating UI Image hashes, run 'repak %s' with the following parameter:\n"
        "\t<%s>\t- the string to compute the uimg hash from\n"

        "For compressing standalone paks, run 'repak %s' with the following parameters:\n"
        "\t<%s>\t- the target pak file to compress\n"
        "\t<%s>\t- ( optional ) the level of compression [ %d, %d ]; default = %d\n"
        "\t<%s>\t- ( optional ) the number of compression workers [ %d, %d ]; default = %d\n"

        "For decompressing standalone paks, run 'repak %s' with the following parameter:\n"
        "\t<%s>\t- the target pak file to decompress\n",

        "buildMapPath",
        "streamingPath",

        REPAK_STR_TO_GUID_COMMAND, "strToGuid",
        REPAK_STR_TO_UIMG_HASH_COMMAND, "strToHash",

        REPAK_COMPRESS_PAK_COMMAND, "pakFilePath", "compressLevel",
        -5, // See https://github.com/facebook/zstd/issues/3032
        ZSTD_maxCLevel(), REPAK_DEFAULT_COMPRESS_LEVEL,

        "workerCount",
        1, ZSTDMT_NBWORKERS_MAX, REPAK_DEFAULT_COMPRESS_WORKERS,

        REPAK_DECOMPRESS_PAK_COMMAND,
        "pakFilePath"
    );
}

static inline void RePak_ValidateArguments(const char* const argName, const int argc, const int required)
{
    const int delta = required - argc;

    if (delta > 0)
    {
        if (delta == 1)
            Error("Invalid usage; \"%s\" requires an additional argument.\n", argName);

        Error("Invalid usage; \"%s\" requires %i additional arguments.\n", argName, delta);
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

static uint16_t RePak_OpenPakAndValidateHeader(BinaryIO& bio, const char* const pakPath)
{
    if (!bio.Open(pakPath, BinaryIO::Mode_e::ReadWrite))
        Error("Failed to open pak file \"%s\"; validation not possible!\n", pakPath);

    const std::streamoff size = bio.GetSize();
    const std::streamoff toConsume = 6; // size of magic( 4 ) + version( 2 ).

    if (size < toConsume)
        Error("Short read on pak file \"%s\"; header criteria unavailable!\n", pakPath);

    const uint32_t magic = bio.Read<uint32_t>();

    if (magic != RPAK_MAGIC)
        Error("Pak file \"%s\" has invalid magic! ( %x != %x ).\n", pakPath, magic, RPAK_MAGIC);

    const uint16_t version = bio.Read<uint16_t>();

    if (!Pak_IsVersionSupported(version))
        Error("Pak file \"%s\" has version %hu which is unsupported!\n", pakPath, version);

    const std::streamoff headerSize = (std::streamoff)Pak_GetHeaderSize(version);

    if (size < headerSize)
        Error("Pak file \"%s\" appears truncated! ( %zd < %zd ).\n", pakPath, size, headerSize);

    return version;
}

static void RePak_HandleCompressPak(const char* const pakPath, const int compressLevel, const int workerCount)
{
    BinaryIO bio;
    const uint16_t version = RePak_OpenPakAndValidateHeader(bio, pakPath);

    // Largest header is 128 bytes (v8).
    char tempHdrBuf[128];
    bio.Seek(0);

    const size_t headerSize = Pak_GetHeaderSize(version);
    bio.Read(tempHdrBuf, headerSize);

    PakHdr_t* const hdr = (PakHdr_t*)tempHdrBuf;

    if (hdr->flags & (PAK_HEADER_FLAGS_RTECH_ENCODED | PAK_HEADER_FLAGS_OODLE_ENCODED | PAK_HEADER_FLAGS_ZSTD_ENCODED))
        Error("Pak file \"%s\" is already encoded using %s!\n", pakPath, Pak_EncodeAlgorithmToString(hdr->flags));

    const size_t newSize = Pak_EncodeStreamAndSwap(bio, compressLevel, workerCount, version, pakPath);

    if (!newSize)
        return; // Failure, don't mutate the file.

    // Update the header to accommodate for the compression method
    // and the new size so the runtime is aware of it.
    hdr->flags |= PAK_HEADER_FLAGS_ZSTD_ENCODED;
    hdr->compressedSize = newSize;

    bio.Seek(0); // Write the new header out.
    bio.Write(tempHdrBuf, headerSize);
}

static void RePak_HandleDecompressPak(const char* const pakPath)
{
    BinaryIO bio;
    const uint16_t version = RePak_OpenPakAndValidateHeader(bio, pakPath);

    // Largest header is 128 bytes (v8).
    char tempHdrBuf[128];
    bio.Seek(0);

    const size_t headerSize = Pak_GetHeaderSize(version);
    bio.Read(tempHdrBuf, headerSize);

    PakHdr_t* const hdr = (PakHdr_t*)tempHdrBuf;

    // TODO: support these are well.
    if (hdr->flags & (PAK_HEADER_FLAGS_RTECH_ENCODED | PAK_HEADER_FLAGS_OODLE_ENCODED))
        Error("Pak file \"%s\" is encoded using %s which is unsupported!\n", pakPath, Pak_EncodeAlgorithmToString(hdr->flags));

    if (!(hdr->flags & PAK_HEADER_FLAGS_ZSTD_ENCODED))
        Error("Pak file \"%s\" is already decoded!\n", pakPath);

    const size_t newSize = Pak_DecodeStreamAndSwap(bio, version, pakPath);

    if (!newSize)
        return; // Failure, don't mutate the file.

    // Update the header to accommodate for the compression method
    // and the new size so the runtime is aware of it.
    hdr->flags &= ~PAK_HEADER_FLAGS_ZSTD_ENCODED;
    hdr->compressedSize = newSize;

    // Should never happen, but in case it does inform the user and
    // equal decompressedSize to compressedSize as otherwise the
    // runtime will crash. There is no guarantee this will fix the
    // file and avoid undesired behavior in the runtime because the
    // file might just be corrupt as it is!
    if (hdr->compressedSize != hdr->decompressedSize)
    {
        Warning("Size mismatch after decoding \"%s\" ( pakHdr->compressedSize( %zu ) != pakHdr->decompressedSize( %zu ) ) -- correcting header... pak file may be corrupt!\n",
            pakPath, hdr->compressedSize, hdr->decompressedSize);

        hdr->decompressedSize = hdr->compressedSize;
    }

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

    if (RePak_CheckCommandLine(argv[1], REPAK_STR_TO_GUID_COMMAND, argc, 3))
    {
        const PakGuid_t guid = RTech::StringToGuid(argv[2]);
        Log("0x%llX\n", guid);

        return;
    }

    if (RePak_CheckCommandLine(argv[1], REPAK_STR_TO_UIMG_HASH_COMMAND, argc, 3))
    {
        const uint32_t hash = RTech::StringToUIMGHash(argv[2]);
        Log("0x%lX\n", hash);

        return;
    }

    if (RePak_CheckCommandLine(argv[1], REPAK_COMPRESS_PAK_COMMAND, argc, 3))
    {
        int compressLevel = REPAK_DEFAULT_COMPRESS_LEVEL;

        if ((argc > 3) && (!JSON_StringToNumber(argv[3], strlen(argv[3]), compressLevel)))
            Error("%s: failed to parse compressLevel for argument \"%s\".\n", __FUNCTION__, argv[1]);

        int workerCount = REPAK_DEFAULT_COMPRESS_WORKERS;

        if ((argc > 4) && (!JSON_StringToNumber(argv[4], strlen(argv[4]), workerCount)))
            Error("%s: failed to parse workerCount for argument \"%s\".\n", __FUNCTION__, argv[1]);

        RePak_HandleCompressPak(argv[2], compressLevel, workerCount);
        return;
    }

    if (RePak_CheckCommandLine(argv[1], REPAK_DECOMPRESS_PAK_COMMAND, argc, 3))
    {
        RePak_HandleDecompressPak(argv[2]);
        return;
    }

    RePak_HandleBuild(argv[1]);
}

int main(int argc, char** argv)
{
    extern bool Console_ColorInit();
    Console_ColorInit();

    g_jsonErrorCallback = Error;

    RePak_HandleCommandLine(argc, argv);
    return EXIT_SUCCESS;
}
