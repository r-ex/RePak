#include "pch.h"
#include "assets.h"
#include "public/studio.h"
#include "public/material.h"
#include <public/animrig.h>

char* AnimRig_ReadRigFile(const std::string& path)
{
    REQUIRE_FILE(path);

    size_t fileSize = Utils::GetFileSize(path);

    if (fileSize < sizeof(studiohdr_t))
        Error("invalid animrig file '%s'. must be at least %i bytes, found %lld\n", path.c_str(), sizeof(studiohdr_t), fileSize);

    char* buf = new char[fileSize];

    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    ifs.read(buf, fileSize);
    ifs.close();

    studiohdr_t* pHdr = reinterpret_cast<studiohdr_t*>(buf);

    if (pHdr->id != 'TSDI') // "IDST"
        Error("invalid animrig file '%s'. expected magic %x, found %x\n", path.c_str(), 'TSDI', pHdr->id);

    if (pHdr->version != 54)
        Error("invalid animrig file '%s'. expected version %i, found %i\n", path.c_str(), 54, pHdr->version);

    if (pHdr->length > fileSize)
        Error("invalid animrig file '%s'. studiohdr->length > fileSize (%i > %i)\n", path.c_str(), pHdr->length, fileSize);

    return buf;
}

bool AnimRig_AddSequenceRefs(CPakDataChunk* chunk, CPakFile* pak, AnimRigAssetHeader_t* hdr, rapidjson::Value& mapEntry, std::vector<PakAsset_t>* assetEntries)
{
    if (!JSON_IS_ARRAY(mapEntry, "sequences")) //mapEntry.HasMember("sequences") || !mapEntry["sequences"].IsArray())
        return false;

    std::vector<uint64_t> sequenceGuids;

    for (auto& it : mapEntry["sequences"].GetArray())
    {
        if (!it.IsString())
            continue;

        if (it.GetStringLength() == 0)
            continue;

        uint64_t guid = 0;

        if (!RTech::ParseGUIDFromString(it.GetString(), &guid))
        {
            Assets::AddAnimSeqAsset(pak, assetEntries, it.GetString());

            guid = RTech::StringToGuid(it.GetString());
        }

        sequenceGuids.emplace_back(guid);
        hdr->sequenceCount++;
    }

    CPakDataChunk guidsChunk = pak->CreateDataChunk(sizeof(uint64_t) * sequenceGuids.size(), SF_CPU, 64);

    uint64_t* pGuids = reinterpret_cast<uint64_t*>(guidsChunk.Data());
    for (int i = 0; i < sequenceGuids.size(); ++i)
    {
        pGuids[i] = sequenceGuids[i];
    }

    *chunk = guidsChunk;
    return true;
}

void Assets::AddAnimRigAsset_v4(CPakFile* pak, std::vector<PakAsset_t>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Log("Adding mdl_ asset '%s'\n", assetPath);

    std::vector<PakGuidRefHdr_t> guids{};

    std::string sAssetName = assetPath;

    std::string rigFilePath = pak->GetAssetPath() + sAssetName;

    // open and validate file to get buffer
    char* buf = AnimRig_ReadRigFile(rigFilePath);
    studiohdr_t* studiohdr = reinterpret_cast<studiohdr_t*>(buf);

    CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(AnimRigAssetHeader_t), SF_HEAD, 16);
    AnimRigAssetHeader_t* pHdr = reinterpret_cast<AnimRigAssetHeader_t*>(hdrChunk.Data());

    CPakDataChunk nameChunk = pak->CreateDataChunk(sAssetName.length() + 1, SF_CPU, 64);
    memcpy_s(nameChunk.Data(), sAssetName.length(), assetPath, sAssetName.length());

    CPakDataChunk rigChunk = pak->CreateDataChunk(studiohdr->length, SF_CPU, 64);
    memcpy_s(rigChunk.Data(), studiohdr->length, buf, studiohdr->length);

    pHdr->data = rigChunk.GetPointer();
    pHdr->name = nameChunk.GetPointer();

    CPakDataChunk guidsChunk;
    if (AnimRig_AddSequenceRefs(&guidsChunk, pak, pHdr, mapEntry, assetEntries))
    {
        pHdr->pSequences = guidsChunk.GetPointer();

        pak->AddPointer(hdrChunk.GetPointer(offsetof(AnimRigAssetHeader_t, pSequences)));

        for (int i = 0; i < pHdr->sequenceCount; ++i)
        {
            guids.emplace_back(guidsChunk.GetPointer(8 * i));
        }
    }

    delete[] buf;

    PakAsset_t asset;

    asset.InitAsset(sAssetName, hdrChunk.GetPointer(), hdrChunk.GetSize(), PagePtr_t::NullPtr(), -1, -1, (std::uint32_t)AssetType::ARIG);
    asset.version = 4;

    asset.pageEnd = pak->GetNumPages();
    asset.remainingDependencyCount = 2;

    asset.AddGuids(&guids);

    asset.EnsureUnique(assetEntries);
    assetEntries->push_back(asset);
}