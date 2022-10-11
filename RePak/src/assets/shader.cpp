#include "pch.h"
#include "Assets.h"
#include "assets/shader.h"

std::unordered_map<std::string, ShaderType> DataTableColumnMap =
{
	{ "pixel",   ShaderType::Pixel },
	{ "vertex",   ShaderType::Vertex },
	{ "geometry",   ShaderType::Geometry },
	{ "hardware",   ShaderType::Hardware },
	{ "domain",   ShaderType::Domain },
	{ "compute",   ShaderType::Compute },
};

ShaderType GetShaderTypeFromString(std::string sType)
{
	std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

	for (const auto& [key, value] : DataTableColumnMap) // Iterate through unordered_map.
	{
		if (sType.compare(key) == 0) // Do they equal?
			return value;
	}

	return ShaderType::Pixel;
}

void Assets::AddShaderSetAsset_stub(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	Error("RPak version 7 (Titanfall 2) Shader Set !!!!NOT IMPLEMENTED!!!!");
}

void Assets::AddShaderSetAsset_v11(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	std::vector<RPakGuidDescriptor> guids{};

	Log("\n==============================\n");
	Log("Asset shds -> '%s'\n", assetPath);

	std::string sAssetName = assetPath;

	uint32_t NameDataSize = sAssetName.length() + 1;
	uint32_t NameAlignment = NameDataSize % 4;
	NameDataSize += NameAlignment;
	char* pDataBuf = new char[NameDataSize];

	ShaderSetHeader* pHdr = new ShaderSetHeader();

	// Segments
	// asset header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(ShaderSetHeader), SF_HEAD, 16);

	// data segment
	_vseginfo_t dataseginfo = pak->CreateNewSegment(NameDataSize, SF_CPU, 64);

	// Write Shader Set Name
	snprintf(pDataBuf, NameDataSize, "%s", sAssetName.c_str());

	pHdr->pName = { dataseginfo.index, 0 };

	pak->AddPointer(subhdrinfo.index, offsetof(ShaderSetHeader, pName));

	pHdr->TextureInputCount = 7;

	if (mapEntry.HasMember("textures") && mapEntry["textures"].IsInt() && mapEntry["textures"].GetInt() != 0)
		pHdr->TextureInputCount = (uint16_t)mapEntry["textures"].GetInt();

	pHdr->Count1 = pHdr->TextureInputCount;
	pHdr->Count3 = 3;

	uint32_t Relations = 0;
	if (mapEntry.HasMember("vertex") && mapEntry["vertex"].IsString())
	{
		std::string PixelShaderName = mapEntry["vertex"].GetStdString();

		pHdr->VertexShaderGUID = RTech::StringToGuid(PixelShaderName.c_str());

		RPakAssetEntry* shaderAsset = pak->GetAssetByGuid(pHdr->VertexShaderGUID, nullptr);

		if (shaderAsset)
		{
			shaderAsset->AddRelation(assetEntries->size());
			pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(ShaderSetHeader, VertexShaderGUID));
			Relations++;
		}
		else
			Warning("unable to find vertex shader '%s' for shaderset '%s' within the local assets\n", PixelShaderName.c_str(), sAssetName.c_str());
	}

	if (mapEntry.HasMember("pixel") && mapEntry["pixel"].IsString())
	{
		std::string PixelShaderName = mapEntry["pixel"].GetStdString();

		pHdr->PixelShaderGUID = RTech::StringToGuid(PixelShaderName.c_str());

		RPakAssetEntry* shaderAsset = pak->GetAssetByGuid(pHdr->PixelShaderGUID, nullptr);

		if (shaderAsset)
		{
			shaderAsset->AddRelation(assetEntries->size());
			pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(ShaderSetHeader, PixelShaderGUID));
			Relations++;
		}
		else
			Warning("unable to find pixel shader '%s' for shaderset '%s' within the local assets\n", PixelShaderName.c_str(), sAssetName.c_str());
	}

	// write shaderdata

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
	pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf });

	uint64_t GUID = RTech::StringToGuid(sAssetName.c_str());
	Log("-> GUID: 0x%llX\n", GUID);

	RPakAssetEntry asset;
	asset.InitAsset(GUID, subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::SHDS);

	asset.version = 11;

	asset.AddRelation(assetEntries->size());
	asset.AddRelation(assetEntries->size());

	asset.relationCount = Relations;
	asset.usesCount = Relations;

	asset.AddGuids(&guids);
	assetEntries->push_back(asset);
}

void Assets::AddShaderAsset_v12(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	std::string AssetName = assetPath;
	AssetName = AssetName;

	Log("\n==============================\n");
	Log("Asset shdr -> '%s'\n", AssetName.c_str());
	uint64_t GUID = RTech::StringToGuid(assetPath);

	std::string shaderFilePath = g_sAssetsDir + AssetName + ".fxc";

	REQUIRE_FILE(shaderFilePath);

	uint32_t Textures = 0;
	if (mapEntry.HasMember("textures") && mapEntry["textures"].IsInt() && mapEntry["textures"].GetInt() != 0)
		Textures = (uint16_t)mapEntry["textures"].GetInt();

	ShaderHeader* pHdr = new ShaderHeader();
	{
		if (mapEntry.HasMember("type") && mapEntry["type"].IsString())
			pHdr->ShaderType = GetShaderTypeFromString(mapEntry["type"].GetStdString());
		else
		{
			Warning("Type Not Found Using Pixel");
			pHdr->ShaderType = ShaderType::Pixel;
		}

		switch (pHdr->ShaderType)
		{
		case ShaderType::Pixel:
		{
			if (mapEntry.HasMember("width") && mapEntry["width"].IsNumber())
				pHdr->max_width = mapEntry["width"].GetInt();

			if (mapEntry.HasMember("height") && mapEntry["height"].IsNumber())
				pHdr->max_height = mapEntry["height"].GetInt();

			if (mapEntry.HasMember("minsize") && mapEntry["minsize"].IsNumber())
				pHdr->min_widthheight = mapEntry["minsize"].GetInt();

			Log("-> min dimensions: %ix%i\n", pHdr->min_widthheight, pHdr->min_widthheight);
			Log("-> max dimensions: %ix%i\n", pHdr->max_width, pHdr->max_height);
			break;
		}
		case ShaderType::Vertex:
		{
			uint8_t unk = 255;
			uint16_t min_widthheight = 256;

			uint16_t max_width = 0;
			uint16_t max_height = 2;
			break;
		}

		default:
			return;
		}
	}

	// Read ShaderData
	BinaryIO shdrInput;
	shdrInput.open(shaderFilePath, BinaryIOMode::Read);

	shdrInput.seek(0, std::ios::end);

	uint32_t shdrFileSize = shdrInput.tell();

	shdrInput.seek(0);

	size_t DataBufferSize = sizeof(ShaderDataHeader) + shdrFileSize;
	char* pDataBuf = new char[DataBufferSize];

	// write the skeleton data into the data buffer
	shdrInput.getReader()->read(pDataBuf + sizeof(ShaderDataHeader), shdrFileSize);
	shdrInput.close();

	rmem writer(pDataBuf);

	// Segments
	// asset header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(ShaderHeader), SF_HEAD, 16);

	_vseginfo_t metadataseginfo = pak->CreateNewSegment((16 * Textures) + (16 * Textures), SF_CPU, 64);
	char* pMetaDataBuf = new char[(16 * Textures) + (16 * Textures)];
	rmem metawriter(pMetaDataBuf);

	// data segment
	_vseginfo_t dataseginfo = pak->CreateNewSegment(DataBufferSize, SF_CPU | SF_TEMP | SF_CLIENT, 1);

	ShaderDataHeader SDRData;
	{
		SDRData.DataSize = shdrFileSize;
		SDRData.pData = { dataseginfo.index, sizeof(ShaderDataHeader) };
		// write shaderdata
		writer.write(SDRData, 0);

		pHdr->pIndex0 = { metadataseginfo.index, 0 };
		pHdr->pTextureSlotData = { metadataseginfo.index, (16 * Textures) };

		for (int i = 0; i < Textures * 2; i++)
		{
			TextureSlotData Data{};
			metawriter.write(Data, (16 * i));
		}
	}

	pak->AddPointer(subhdrinfo.index, offsetof(ShaderHeader, pName));
	pak->AddPointer(subhdrinfo.index, offsetof(ShaderHeader, pIndex0));
	pak->AddPointer(subhdrinfo.index, offsetof(ShaderHeader, pTextureSlotData));

	pak->AddPointer(dataseginfo.index, offsetof(ShaderDataHeader, pData));

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)pHdr });
	pak->AddRawDataBlock({ metadataseginfo.index, metadataseginfo.size, (uint8_t*)pMetaDataBuf });
	pak->AddRawDataBlock({ dataseginfo.index, dataseginfo.size, (uint8_t*)pDataBuf });

	Log("-> GUID: 0x%llX\n", GUID);

	RPakAssetEntry asset;
	asset.InitAsset(GUID, subhdrinfo.index, 0, subhdrinfo.size, dataseginfo.index, 0, -1, -1, (std::uint32_t)AssetType::SHDR);

	asset.version = 12;

	assetEntries->push_back(asset);
}