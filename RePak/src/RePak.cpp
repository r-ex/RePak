#include "pch.h"
#include "Assets.h"
#include "rapidjson/error/en.h"

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

    std::filesystem::path mapPath(argv[1]);

    if (!FILE_EXISTS(argv[1]))
        Error("couldn't find map file\n");

    std::ifstream ifs(argv[1]);

    if (!ifs.is_open())
        Error("couldn't open map file.\n");

    // begin json parsing
    IStreamWrapper isw{ ifs };

    Document doc{ };

    doc.ParseStream(isw);

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
            lineNum, 
            columnNum, 
            GetParseError_En(doc.GetParseError()), 
            lastLine.c_str(), 
            curLine.c_str(), 
            (std::string(columnNum, ' ') += '^').c_str());
    }

    std::string sRpakName = DEFAULT_RPAK_NAME;

    if (doc.HasMember("name") && doc["name"].IsString())
        sRpakName = doc["name"].GetStdString();
    else
        Warning("Map file should have a 'name' field containing the string name for the new rpak, but none was provided. Defaulting to '%s.rpak' and continuing...\n", DEFAULT_RPAK_NAME);

    Log("build settings:\n");
    Log("filename: %s\n", sRpakName.c_str());

    if (!doc.HasMember("assetsDir"))
    {
        Warning("No assetsDir field provided. Assuming that everything is relative to the working directory.\n");
        if (mapPath.has_parent_path())
            Assets::g_sAssetsDir = mapPath.parent_path().u8string();
        else
            Assets::g_sAssetsDir = ".\\";
    }
    else
    {
        std::filesystem::path assetsDirPath(doc["assetsDir"].GetStdString());
        if (assetsDirPath.is_relative() && mapPath.has_parent_path())
            Assets::g_sAssetsDir = std::filesystem::canonical(mapPath.parent_path() / assetsDirPath).u8string();
        else
            Assets::g_sAssetsDir = assetsDirPath.u8string();

        // ensure that the path has a slash at the end
        Utils::AppendSlash(Assets::g_sAssetsDir);
        Log("assetsDir: %s\n", Assets::g_sAssetsDir.c_str());
    }

    std::string sOutputDir = "build/";

    if (doc.HasMember("outputDir"))
    {
        std::filesystem::path outputDirPath(doc["outputDir"].GetStdString());

        if (outputDirPath.is_relative() && mapPath.has_parent_path())
            sOutputDir = std::filesystem::canonical(mapPath.parent_path() / outputDirPath).u8string();
        else
            sOutputDir = outputDirPath.u8string();

        // ensure that the path has a slash at the end
        Utils::AppendSlash(sOutputDir);
        Log("outputDir: %s\n", sOutputDir.c_str());
    }

    if (!doc.HasMember("files"))
        Warning("No 'files' field specified, the RPak will contain no assets...\n");
    else if (!doc["files"].IsArray())
        Error("'files' field is not of required type 'array'. Exiting...\n");


    if (!doc.HasMember("version"))
        Error("No RPak file version specified. Valid options:\n7 - Titanfall 2\n8 - Apex Legends\nExiting...\n");
    else if ( !doc["version"].IsInt() )
        Error("Invalid RPak file version specified. Valid options:\n7 - Titanfall 2\n8 - Apex Legends\nExiting...\n");

    int rpakVersion = doc["version"].GetInt();

    CPakFile* pak = new CPakFile(rpakVersion);

    // if keepDevOnly exists, is boolean, and is set to true
    if (doc.HasMember("keepDevOnly") && doc["keepDevOnly"].IsBool() && doc["keepDevOnly"].GetBool())
        pak->flags |= PF_KEEP_DEV;

    if (doc.HasMember("starpakPath") && doc["starpakPath"].IsString())
        pak->primaryStarpakPath = doc["starpakPath"].GetStdString();

    Log("version: %i\n\n", rpakVersion);

    // build asset data
    // loop through all assets defined in the map json
    for (auto& file : doc["files"].GetArray())
    {
        pak->AddAsset(file);
    }

    std::filesystem::create_directories(sOutputDir); // create directory if it does not exist yet.

    BinaryIO out{ };

    out.open(sOutputDir + sRpakName + ".rpak", BinaryIOMode::Write);

    // write a placeholder header so we can come back and complete it
    // when we have all the info
    pak->WriteHeader(&out);

    // write string vectors for starpak paths and get the total length of each vector
    size_t StarpakRefLength = Utils::WriteStringVector(out, pak->m_vStarpakPaths);
    size_t OptStarpakRefLength = Utils::WriteStringVector(out, pak->m_vOptStarpakPaths);

    pak->SetStarpakPathsSize(StarpakRefLength, OptStarpakRefLength);

    // generate file relation vector to be written
    pak->GenerateFileRelations();
    pak->GenerateGuidData();

    // write the non-paged data to the file first
    WRITE_VECTOR(out, pak->m_vVirtualSegments);
    WRITE_VECTOR(out, pak->m_vPages);
    WRITE_VECTOR(out, pak->m_vDescriptors);
    pak->WriteAssets(&out);
    WRITE_VECTOR(out, pak->m_vGuidDescriptors);
    WRITE_VECTOR(out, pak->m_vFileRelations);

    // now the actual paged data
    // this should probably be writing by page instead of just hoping that
    // the data blocks are in the right order
    pak->WriteRPakRawDataBlocks(out);

    // get current time as FILETIME
    FILETIME ft = Utils::GetFileTimeBySystem();

    pak->m_Header.fileTime = static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime; // write the current time into the file as FILETIME

    pak->m_Header.compressedSize = out.tell();
    pak->m_Header.decompressedSize = out.tell();

    out.seek(0); // Go back to the beginning to finally write the rpakHeader now.

    pak->WriteHeader(&out);

    out.close();

    Debug("written rpak file with size %lld\n", pak->m_Header.compressedSize);

    // free the memory
    for (auto& it : pak->m_vRawDataBlocks)
    {
        delete it.m_nDataPtr;
    }

    // write starpak data
    if (pak->m_vStarpakPaths.size() == 1)
    {
        std::string sFullPath = pak->m_vStarpakPaths[0];
        std::filesystem::path path(sFullPath);

        std::string filename = path.filename().u8string();

        Debug("writing starpak %s with %lld data entries\n", filename.c_str(), pak->m_vStarpakDataBlocks.size());
        BinaryIO srpkOut;

        srpkOut.open(sOutputDir + filename, BinaryIOMode::Write);

        int magic = 'kPRS';
        int version = 1;
        uint64_t entryCount = pak->m_vStarpakDataBlocks.size();

        srpkOut.write(magic);
        srpkOut.write(version);

        // data blocks in starpaks are all aligned to 4096 bytes, including the header which gets filled with 0xCB after the magic
        // and version
        char* why = new char[4088];
        memset(why, 0xCB, 4088);

        srpkOut.getWriter()->write(why, 4088);

        for (auto& it : pak->m_vStarpakDataBlocks)
        {
            srpkOut.getWriter()->write((const char*)it.m_nDataPtr, it.m_nDataSize);
        }

        // starpaks have a table of sorts at the end of the file, containing the offsets and data sizes for every data block
        // as far as i'm aware, this isn't even used by the game, so i'm not entirely sure why it exists?
        for (auto& it : pak->m_vStarpakDataBlocks)
        {
            SRPkFileEntry fe{};
            fe.m_nOffset = it.m_nOffset;
            fe.m_nSize = it.m_nDataSize;

            srpkOut.write(fe);
        }

        srpkOut.write(entryCount);

        Debug("written starpak file with size %lld\n", srpkOut.tell());

        srpkOut.close();
    }

    delete pak;
    return EXIT_SUCCESS;
}