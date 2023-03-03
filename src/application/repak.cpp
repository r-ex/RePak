#include "pch.h"
#include "assets/assets.h"
#include "rapidjson/error/en.h"
#include "logic/pakfile.h"

using namespace rapidjson;

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

    std::filesystem::path inputPath(argv[1]);

    if (!FILE_EXISTS(argv[1]))
        Error("couldn't find map file\n");

    std::ifstream ifs(argv[1]);

    if (!ifs.is_open())
        Error("couldn't open map file.\n");

    // begin json parsing
    IStreamWrapper isw{ ifs };

    Document doc{ };

    doc.ParseStream<rapidjson::ParseFlag::kParseCommentsFlag | rapidjson::ParseFlag::kParseTrailingCommasFlag>(isw);

    // handle parse errors
    if (doc.HasParseError()) {
        int lineNum = 1;
        int columnNum = 0;
        std::string lastLine = "";
        std::string curLine = "";

        int offset = doc.GetErrorOffset();
        ifs.clear();
        ifs.seekg(0, std::ios::beg);
        IStreamWrapper isw{ ifs };

        for (int i = 0; ; i++)
        {
            char c = isw.Take();
            curLine.push_back(c);
            if (c == '\n')
            {
                if (i >= offset)
                    break;
                lastLine = curLine;
                curLine = "";
                lineNum++;
                columnNum = 0;
            }
            else
            {
                if (i < offset)
                    columnNum++;
            }
        }

        // this could probably be formatted nicer
        Error("Failed to parse map file: \n\nLine %i, Column %i\n%s\n\n%s%s%s\n", 
            lineNum, columnNum, 
            GetParseError_En(doc.GetParseError()), 
            lastLine.c_str(), curLine.c_str(), (std::string(columnNum, ' ') += '^').c_str());
    }

    std::string sRpakName = DEFAULT_RPAK_NAME;

    if (doc.HasMember("name") && doc["name"].IsString())
        sRpakName = doc["name"].GetStdString();
    else
        Warning("Map file should have a 'name' field containing the string name for the new rpak, but none was provided. Using '%s.rpak'.\n", sRpakName.c_str());

    std::string sOutputDir = "build/";

    if (!doc.HasMember("assetsDir"))
    {
        Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
        if (inputPath.has_parent_path())
            Assets::g_sAssetsDir = inputPath.parent_path().u8string();
        else
            Assets::g_sAssetsDir = ".\\";
    }
    else
    {
        std::filesystem::path assetsDirPath(doc["assetsDir"].GetStdString());
        if (assetsDirPath.is_relative() && inputPath.has_parent_path())
            Assets::g_sAssetsDir = std::filesystem::canonical(inputPath.parent_path() / assetsDirPath).u8string();
        else
            Assets::g_sAssetsDir = assetsDirPath.u8string();

        // ensure that the path has a slash at the end
        Utils::AppendSlash(Assets::g_sAssetsDir);
    }

    if (doc.HasMember("outputDir"))
    {
        std::filesystem::path outputDirPath(doc["outputDir"].GetStdString());

        if (outputDirPath.is_relative() && inputPath.has_parent_path())
            sOutputDir = std::filesystem::canonical(inputPath.parent_path() / outputDirPath).u8string();
        else
            sOutputDir = outputDirPath.u8string();

        // ensure that the path has a slash at the end
        Utils::AppendSlash(sOutputDir);
    }

    if (!doc["files"].IsArray())
        Error("[JSON] 'files' field must be an array. Exiting...\n");

    if (!doc.HasMember("version") || !doc["version"].IsInt())
        Error("[JSON] Must specify an RPak file version with the \"version\" field. Valid options:\n7 - Titanfall 2\n8 - Apex Legends\nExiting...\n");

    int rpakVersion = doc["version"].GetInt();

    // print parsed settings
    Log("build settings:\n");
    Log("version: %i\n", rpakVersion);
    Log("filename: %s.rpak\n", sRpakName.c_str());
    Log("assetsDir: %s\n", Assets::g_sAssetsDir.c_str());
    Log("outputDir: %s\n", sOutputDir.c_str());
    Log("\n");

    CPakFile* pak = new CPakFile(rpakVersion);

    std::string outputPath = sOutputDir + sRpakName + ".rpak";
    pak->SetPath(outputPath);

    // if keepDevOnly exists, is boolean, and is set to true
    if (doc.HasMember("keepDevOnly") && doc["keepDevOnly"].IsBool() && doc["keepDevOnly"].GetBool())
        pak->AddFlags(PF_KEEP_DEV);

    if (doc.HasMember("starpakPath") && doc["starpakPath"].IsString())
        pak->SetPrimaryStarpakPath(doc["starpakPath"].GetStdString());

    // build asset data
    // loop through all assets defined in the map json
    for (auto& file : doc["files"].GetArray())
    {
        pak->AddAsset(file);
    }

    std::filesystem::create_directories(sOutputDir); // create output directory if it does not exist yet.

    BinaryIO out;
    out.open(pak->GetPath(), BinaryIOMode::Write);

    // write a placeholder header so we can come back and complete it
    // when we have all the info
    pak->WriteHeader(out);

    // write string vectors for starpak paths and get the total length of each vector
    size_t starpakPathsLength = pak->WriteStarpakPaths(out);
    size_t optStarpakPathsLength = pak->WriteStarpakPaths(out, true);

    pak->SetStarpakPathsSize(starpakPathsLength, optStarpakPathsLength);

    // generate file relation vector to be written
    pak->GenerateFileRelations();
    pak->GenerateGuidData();

    // write the non-paged data to the file first
    pak->WriteVirtualSegments(out);
    pak->WritePages(out);
    pak->WritePakDescriptors(out);
    pak->WriteAssets(out);
    pak->WriteGuidDescriptors(out);
    pak->WriteFileRelations(out);

    // now the actual paged data
    // this should probably be writing by page instead of just hoping that
    // the data blocks are in the right order
    pak->WriteRawDataBlocks(out);

    // get current time as FILETIME
    pak->SetFileTime(Utils::GetFileTimeBySystem());

    //pak->m_Header.fileTime = static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime; // write the current time into the file as FILETIME

    // !TODO: implement LZHAM and set these accordingly.
    pak->SetCompressedSize(out.tell());
    pak->SetDecompressedSize(out.tell());

    out.seek(0); // Go back to the beginning to finally write the rpakHeader now.

    pak->WriteHeader(out);

    out.close();

    Debug("written rpak file with size %lld\n", pak->GetCompressedSize());

    // free the memory
    pak->FreeRawDataBlocks();

    // write starpak data
    if (pak->GetNumStarpakPaths() == 1)
    {
        std::filesystem::path path(pak->GetStarpakPath(0));

        std::string filename = path.filename().u8string();

        Debug("writing starpak %s with %lld data entries\n", filename.c_str(), pak->GetStreamingAssetCount());
        BinaryIO srpkOut;

        srpkOut.open(sOutputDir + filename, BinaryIOMode::Write);

        int magic = 'kPRS';
        int version = 1;
        uint64_t entryCount = pak->GetStreamingAssetCount();

        srpkOut.write(magic);
        srpkOut.write(version);

        // data blocks in starpaks are all aligned to 4096 bytes, including the header which gets filled with 0xCB after the magic
        // and version
        char* why = new char[4088];
        memset(why, 0xCB, 4088);

        srpkOut.getWriter()->write(why, 4088);

        pak->WriteStarpakDataBlocks(srpkOut);
        pak->WriteStarpakSortsTable(srpkOut);

        srpkOut.write(entryCount);
        Debug("written starpak file with size %lld\n", srpkOut.tell());

        pak->FreeStarpakDataBlocks();
        srpkOut.close();
    }

    delete pak;
    return EXIT_SUCCESS;
}