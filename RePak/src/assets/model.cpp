#include "pch.h"
#include "Assets.h"

void Assets::AddModelAsset(std::vector<RPakAssetEntryV8>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
    Debug("Adding mdl_ asset '%s'\n", assetPath);

    std::string sAssetName = std::string(assetPath) + ".rmdl";

    ModelHeader* pHdr = new ModelHeader();

    std::string rmdlFilePath = g_sAssetsDir + sAssetName;
    std::string vgFilePath = g_sAssetsDir + std::string(assetPath) + ".vg";

    ///-------------------
    // Begin skeleton(.rmdl) input
    BinaryIO skelInput;
    skelInput.open(rmdlFilePath, BinaryIOMode::BinaryIOMode_Read);

    studiohdrshort_t mdlhdr = skelInput.read<studiohdrshort_t>();

    if (mdlhdr.id != 0x54534449)
    {
        Warning("invalid file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x54534449, mdlhdr.id);
        return;
    }

    if (mdlhdr.version != 54)
    {
        Warning("invalid version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 0x36, mdlhdr.version);
        return;
    }

    // go back to the beginning of the file to read all the data
    skelInput.seek(0);

    uint32_t fileNameDataSize = sAssetName.length() + 1;

    char* pDataBuf = new char[fileNameDataSize + mdlhdr.dataLength];

    // write the model file path into the data buffer
    snprintf(pDataBuf, fileNameDataSize, "%s", sAssetName.c_str());
    // write the skeleton data into the data buffer
    skelInput.getReader()->read(pDataBuf + fileNameDataSize, mdlhdr.dataLength);
    skelInput.close();

    ///--------------------
    // Add VG data
    BinaryIO vgInput;
    vgInput.open(vgFilePath, BinaryIOMode::BinaryIOMode_Read);

    BasicRMDLVGHeader bvgh = vgInput.read<BasicRMDLVGHeader>();

    if (bvgh.magic != 0x47567430)
    {
        Warning("invalid vg file magic for model asset '%s'. expected %x, found %x. skipping asset...\n", sAssetName.c_str(), 0x47567430, bvgh.magic);
        return;
    }

    if (bvgh.version != 1)
    {
        Warning("invalid vg version for model asset '%s'. expected %i, found %i. skipping asset...\n", sAssetName.c_str(), 1, bvgh.version);
        return;
    }

    vgInput.seek(0, std::ios::end);

    uint32_t vgFileSize = vgInput.tell();
    char* vgBuf = new char[vgFileSize];

    vgInput.seek(0);
    vgInput.getReader()->read(vgBuf, vgFileSize);
    vgInput.close();

    // static name for now
    RePak::AddStarpakReference("paks/Win64/repak.starpak");

    SRPkDataEntry de{ -1, vgFileSize, (uint8_t*)vgBuf };
    uint64_t starpakOffset = RePak::AddStarpakDataEntry(de);

    pHdr->StreamedDataSize = vgFileSize;

    RPakVirtualSegment SubHeaderSegment{};
    uint32_t shsIdx = RePak::CreateNewSegment(sizeof(ModelHeader), 0, 8, SubHeaderSegment);

    RPakVirtualSegment DataSegment{};
    uint32_t dataSegIdx = RePak::CreateNewSegment(mdlhdr.dataLength + fileNameDataSize, 1, 64, DataSegment);

    //RPakVirtualSegment VGSegment{};
    //uint32_t vgIdx = RePak::CreateNewSegment(vgFileSize, 67, SegmentType::Unknown, DataSegment);

    pHdr->SkeletonPtr.Index = dataSegIdx;
    pHdr->SkeletonPtr.Offset = fileNameDataSize;


    pHdr->NamePtr.Index = dataSegIdx;
    pHdr->NamePtr.Offset = 0;

    //hdr->VGPtr.Index = vgIdx;
    //hdr->VGPtr.Offset = 0;

    RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, SkeletonPtr));
    RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, NamePtr));
    //RePak::RegisterDescriptor(shsIdx, offsetof(ModelHeader, VGPtr));


    RPakRawDataBlock shdb{ shsIdx, SubHeaderSegment.DataSize, (uint8_t*)pHdr };
    RePak::AddRawDataBlock(shdb);

    RPakRawDataBlock rdb{ dataSegIdx, DataSegment.DataSize, (uint8_t*)pDataBuf };
    RePak::AddRawDataBlock(rdb);

    //RPakRawDataBlock vgdb{ vgIdx, vgFileSize, (uint8_t*)vgBuf };
    //RePak::AddRawDataBlock(vgdb);

    // now time to add the higher level asset entry
    RPakAssetEntryV8 asset;

    asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), shsIdx, 0, SubHeaderSegment.DataSize, -1, 0, starpakOffset, -1, (std::uint32_t)AssetType::RMDL);
    asset.Version = RMDL_VERSION;
    // i have literally no idea what these are
    asset.HighestPageNum = dataSegIdx + 1;
    asset.Un2 = 2;

    // note: models use an implicit guid reference to their materials
    // the guids aren't registered but are converted during the loading of the material asset
    // during (what i assume is) regular reference conversion
    // a potential solution to the material guid conversion issue could be just registering the guids?
    // 
    size_t fileRelationIdx = RePak::AddFileRelation(assetEntries->size());
    asset.UsesStartIndex = fileRelationIdx;
    asset.UsesCount = 1;

    assetEntries->push_back(asset);
}