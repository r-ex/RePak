#include "pch.h"
#include "assets.h"
#include "public/rui.h"

void Rui_loadFromPackage(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath) {
    
    UNUSED(assetGuid);
    const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("ruip");
    RuiPackage rui{inputFilePath};

    PakAsset_t& asset = pak->BeginAsset(assetGuid,assetPath);
    PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(RuiHeader_v30_s),SF_HEAD|SF_CLIENT,8);
    RuiHeader_v30_s* ruiHdr = reinterpret_cast<RuiHeader_v30_s*>(hdrChunk.data);
    *ruiHdr = rui.CreateRuiHeader_v30();
    
    PakPageLump_s nameChunk = pak->CreatePageLump(rui.name.size(),SF_CPU|SF_CLIENT,8);
    memcpy(nameChunk.data,rui.name.data(),rui.name.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,name),nameChunk,0);

    PakPageLump_s defaultValuesChunk = pak->CreatePageLump(rui.defaultData.size()+rui.defaultStrings.size(),SF_CPU|SF_CLIENT,8);
    memcpy(defaultValuesChunk.data,rui.defaultData.data(),rui.defaultData.size());
    memcpy(&defaultValuesChunk.data[rui.defaultData.size()],rui.defaultStrings.data(),rui.defaultStrings.size());
    for (uint16_t offset : rui.defaultStringOffsets) {
        uint64_t stringOffset = *reinterpret_cast<uint64_t*>(&rui.defaultData[offset])+rui.defaultData.size();
        pak->AddPointer(defaultValuesChunk,offset,defaultValuesChunk,stringOffset);
    }
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,dataStructInitData),defaultValuesChunk,0);
   
    PakPageLump_s transformDataChunk = pak->CreatePageLump(rui.transformData.size(),SF_CPU|SF_CLIENT,8);
    memcpy(transformDataChunk.data,rui.transformData.data(),rui.transformData.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,transformData),transformDataChunk,0);

    PakPageLump_s argClustersChunk = pak->CreatePageLump(rui.argCluster.size()*sizeof(ArgCluster_s),SF_CPU|SF_CLIENT,8);
    memcpy(argClustersChunk.data,rui.argCluster.data(),rui.argCluster.size()*sizeof(ArgCluster_s));
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,argClusters),argClustersChunk,0);

    PakPageLump_s argumentsChunk = pak->CreatePageLump(rui.arguments.size()*sizeof(Argument_s),SF_CPU|SF_CLIENT,8);
    memcpy(argumentsChunk.data,rui.arguments.data(),rui.arguments.size()*sizeof(Argument_s));
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,arguments),argumentsChunk,0);
    
    ruiHdr->argNames = 0;

    PakPageLump_s styleDescriptorChunk = pak->CreatePageLump(rui.styleDescriptors.size(),SF_CPU|SF_CLIENT,8);
    memcpy(styleDescriptorChunk.data,rui.styleDescriptors.data(),rui.styleDescriptors.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,styleDescriptors),styleDescriptorChunk,0);
    
    PakPageLump_s renderJobChunk = pak->CreatePageLump(rui.renderJobs.size(),SF_CPU|SF_CLIENT,8);
    memcpy(renderJobChunk.data,rui.renderJobs.data(),rui.renderJobs.size());
    pak->AddPointer(hdrChunk,offsetof(RuiHeader_v30_s,renderJobData),renderJobChunk,0);

    asset.InitAsset(hdrChunk.GetPointer(),sizeof(RuiHeader_v30_s),
        PagePtr_t::NullPtr(),30,AssetType::UI);
    asset.SetHeaderPointer(hdrChunk.data);

    pak->FinishAsset();

}

RuiHeader_v30_s RuiPackage::CreateRuiHeader_v30() {
    RuiHeader_v30_s ruiHdr;

    ruiHdr.elementWidth = hdr.elementWidth;
    ruiHdr.elementHeight = hdr.elementHeight;
    ruiHdr.elementWidthRcp = hdr.elementWidthRcp;
    ruiHdr.elementHeightRcp = hdr.elementHeightRcp;
    
    ruiHdr.argumentCount = hdr.argCount;
    ruiHdr.mappingCount = hdr.mappingCount;
    ruiHdr.dataStructSize = hdr.dataStructSize;
    ruiHdr.dataStructInitSize = hdr.defaultValuesSize;
    ruiHdr.styleDescriptorCount = hdr.styleDescriptorCount;
    ruiHdr.maxTransformIndex = 0;
    ruiHdr.renderJobCount = hdr.renderJobCount;
    ruiHdr.argClusterCount = hdr.argClusterCount;

    return ruiHdr;
}

RuiPackage::RuiPackage(const fs::path& inputPath) {
    FILE* f = NULL;
    errno_t errorCode = fopen_s(&f, inputPath.string().c_str(), "rb");
    if (errorCode == 0) {
        fread(&hdr,sizeof(hdr),1,f);
        if(hdr.magic != RUI_PACKAGE_MAGIC)
            Error("Attempted to load an invalid RUIP file (expected magic %x, got %x).\n", RUI_PACKAGE_MAGIC, hdr.magic);
        if(hdr.packageVersion != RUI_PACKAGE_VERSION)
            Error("Attempted to load an unsupported RUIP file (expected version %u, got %u).\n", RUI_PACKAGE_VERSION, hdr.packageVersion);
        
        fseek(f,(long)hdr.nameOffset,0);
        name.resize(hdr.nameSize);
        fread(name.data(), 1, hdr.nameSize, f);
        
        fseek(f,(long)hdr.defaultValuesOffset,0);
        defaultData.resize(hdr.defaultValuesSize);
        fread(defaultData.data(),1,hdr.defaultValuesSize,f);
        
        fseek(f,(long)hdr.defaultStringDataOffset,0);
        defaultStrings.resize(hdr.defaultStringsDataSize);
        fread(defaultStrings.data(),1,hdr.defaultStringsDataSize,f);
        
        fseek(f,(long)hdr.rpakPointersInDefaultDataOffset,0);
        defaultStringOffsets.resize(hdr.rpakPointersInDefaltDataCount);
        fread(defaultStringOffsets.data(),sizeof(uint16_t),hdr.rpakPointersInDefaltDataCount,f);
        
        fseek(f,(long)hdr.styleDescriptorOffset,0);
        styleDescriptors.resize(hdr.styleDescriptorCount*sizeof(StyleDescriptor_v30_s));
        fread(styleDescriptors.data(),sizeof(StyleDescriptor_v30_s),hdr.styleDescriptorCount,f);
        
        fseek(f,(long)hdr.renderJobOffset,0);
        renderJobs.resize(hdr.renderJobSize);
        fread(renderJobs.data(),1,hdr.renderJobSize,f);
        
        fseek(f,(long)hdr.transformDataOffset,0);
        transformData.resize(hdr.transformDataSize);
        fread(transformData.data(),1,hdr.transformDataSize,f);
        
        fseek(f,(long)hdr.argumentsOffset,0);
        arguments.resize(hdr.argCount);
        fread(arguments.data(),sizeof(Argument_s),hdr.argCount,f);

        fseek(f,(long)hdr.argClusterOffset,0);
        argCluster.resize(hdr.argClusterCount);
        fread(argCluster.data(),sizeof(ArgCluster_s),hdr.argClusterCount,f);

        fclose(f);
    }
    else {
        Error("Could not open ruip file %s with error %x",inputPath.string().c_str(),errorCode);
    }
}






void Assets::AddRuiAsset_v30(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
    UNUSED(mapEntry);
    Rui_loadFromPackage(pak,assetGuid,assetPath);
}