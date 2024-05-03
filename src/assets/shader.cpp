#include "pch.h"
#include "assets.h"
#include "public/shader.h"
#include "utils/dxutils.h"

void Assets::AddShaderAsset_v8(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
	Log("Adding shdr asset '%s'\n", assetPath);

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderAssetHeader_v8_t), SF_HEAD, 8);

	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("fxc");

	if (!fs::exists(inputFilePath))
		Error("Failed to find compiled shader file for asset '%s' (%s).\n", assetPath, inputFilePath.string().c_str());

	const fs::path pakFilePath = fs::path(assetPath).replace_extension("");

	CPakDataChunk nameChunk = pak->CreateDataChunk(pakFilePath.string().length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), pakFilePath.string().c_str());

	ShaderAssetHeader_v8_t* hdr = reinterpret_cast<ShaderAssetHeader_v8_t*>(hdrChunk.Data());

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v8_t, name)));
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v8_t, shaderInputFlags)));

	hdr->name = nameChunk.GetPointer();
	hdr->type = GetShaderTypeByName(JSON_GET_STR(mapEntry, "shaderType", "invalid"));
	hdr->unk_C = 1;

	// File Data
	const size_t inputFileSize = Utils::GetFileSize(inputFilePath.string());

	CPakDataChunk bytecodeChunk = pak->CreateDataChunk(sizeof(ShaderByteCode_t) + inputFileSize, SF_CPU, 8);

	ShaderByteCode_t* bytecodeHdr = reinterpret_cast<ShaderByteCode_t*>(bytecodeChunk.Data());

	bytecodeHdr->data = bytecodeChunk.GetPointer(sizeof(ShaderByteCode_t));
	bytecodeHdr->dataSize = static_cast<int>(inputFileSize);

	std::ifstream inputFile(inputFilePath, std::ios::in | std::ios::binary);

	char* bytecodeDest = bytecodeChunk.Data() + sizeof(ShaderByteCode_t);
	inputFile.read(bytecodeDest, inputFileSize);

	inputFile.close();

	ParsedDXShaderData_t parsedData;

	if (DXUtils::GetParsedShaderData(bytecodeDest, inputFileSize, &parsedData))
	{
		if (parsedData.foundFlags & SHDR_FOUND_RDEF)
		{
			hdr->type = static_cast<eShaderType>(parsedData.pakShaderType);
		}
	}
	
	// ============= Input Flags =============
	// static size for now. this may need to become dynamic if someone ever actually uses
	// a shader with more than one bytecode buffer
	CPakDataChunk inputFlagChunk = pak->CreateDataChunk(8, SF_CPU, 1);
	*reinterpret_cast<uint64_t*>(inputFlagChunk.Data()) = JSON_GET_UINT64(mapEntry, "inputFlags", 0);

	hdr->shaderInputFlags = inputFlagChunk.GetPointer();
	// =======================================

	PakAsset_t asset;
	asset.InitAsset(
		pakFilePath.string() + ".rpak",
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		bytecodeChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::SHDR);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = 8;

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	asset.remainingDependencyCount = 1;

	pak->PushAsset(asset);

	printf("\n");
}