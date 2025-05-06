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

int main(int argc, char** argv)
{
    printf(startupVersion);
    g_jsonErrorCallback = Error;

    if (argc < 2)
        Error("invalid usage\n");

    RePak_HandleBuild(argv[1]);
    return EXIT_SUCCESS;
}