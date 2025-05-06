#include "pch.h"
#include "assets.h"
#include "public/texture_list.h"

static void TextureList_InternalAdd(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
	const std::string txlsPath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, "json");
	rapidjson::Document document;

	if (!JSON_ParseFromFile(txlsPath.c_str(), "texture list", document, false))
		Error("Failed to open texture_list asset \"%s\".\n", txlsPath.c_str());

	rapidjson::Value::ConstMemberIterator texturesIt;
	JSON_GetRequired(document, "textures", JSONFieldType_e::kArray, texturesIt);

	// Parse manually added entries here.
	const rapidjson::Value::ConstArray textureArray = texturesIt->value.GetArray();
	const size_t arraySize = textureArray.Size();

	if (arraySize == 0)
		Error("Texture list is empty.\n");

	PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
	PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(TextureListHeader_s), SF_HEAD, 8);

	TextureListHeader_s* const txlsHeader = (TextureListHeader_s*)hdrLump.data;
	txlsHeader->numTextures = arraySize;

	size_t stringBufSize = 0;

	{ // Calculate the total text buffer size and validate all array elems.
		size_t currArrayIdx = 0;

		for (const auto& texture : textureArray)
		{
			if (!texture.IsString())
			{
				Error("Texture #%zu is of type %s, but %s was expected.\n",
					currArrayIdx, JSON_TypeToString(JSON_ExtractType(texture)), JSON_TypeToString(JSONFieldType_e::kString));
			}

			const size_t stringLen = texture.GetStringLength();

			if (stringLen == 0)
				Error("Texture #%zu is an empty string.\n", currArrayIdx);

			// +1 for the null terminator.
			stringBufSize += stringLen + 1;
			currArrayIdx++;
		}
	}

	const bool keepClientOnly = pak->IsFlagSet(PF_KEEP_CLIENT);

	const size_t guidBufSize = keepClientOnly ? (arraySize * sizeof(PakGuid_t)) : 0;
	const size_t namePtrBufSize = arraySize * sizeof(PagePtr_t);

	const size_t totalBufSize = guidBufSize + namePtrBufSize + stringBufSize;
	PakPageLump_s dataLump = pak->CreatePageLump(totalBufSize, SF_CPU, 8);

	if (keepClientOnly)
		pak->AddPointer(hdrLump, offsetof(TextureListHeader_s, textureGuids), dataLump, 0);

	pak->AddPointer(hdrLump, offsetof(TextureListHeader_s, textureNames), dataLump, guidBufSize);

	// Write the values out.
	size_t currArrayIdx = 0;
	PakGuid_t* guidBuf = (PakGuid_t*)dataLump.data;

	char* const textBufStart = (char*)&dataLump.data[guidBufSize + namePtrBufSize];
	char* textBufCurr = textBufStart;

	for (const auto& texture : textureArray)
	{
		const char* const textureName = texture.GetString();

		// The guid array is client-only! This requires the textures to be
		// packed with the rpak, however the server doesn't need textures
		// so we need to drop them.
		if (keepClientOnly)
		{
			const std::string fullName = Utils::VFormat("texture/%s.rpak", textureName);
			const PakGuid_t textureGuid = RTech::StringToGuid(fullName.c_str());

			guidBuf[currArrayIdx] = textureGuid;
			Pak_RegisterGuidRefAtOffset(textureGuid, currArrayIdx * sizeof(PakGuid_t), dataLump, asset);
		}

		// Write the pointer to the start of the texture name we are about to copy.
		const size_t textBufBase = guidBufSize + namePtrBufSize;
		pak->AddPointer(dataLump, guidBufSize + (currArrayIdx * sizeof(PagePtr_t)), dataLump, textBufBase + (textBufCurr - textBufStart));

		const size_t stringLen = texture.GetStringLength();
		memcpy(textBufCurr, texture.GetString(), stringLen + 1);

		textBufCurr += stringLen + 1;
		currArrayIdx++;
	}

	asset.InitAsset(hdrLump.GetPointer(), sizeof(TextureListHeader_s), PagePtr_t::NullPtr(), TXLS_VERSION, AssetType::TXLS);
	asset.SetHeaderPointer(hdrLump.data);

	pak->FinishAsset();
}

void Assets::AddTextureListAsset_v1(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	UNUSED(mapEntry);
	TextureList_InternalAdd(pak, assetGuid, assetPath);
};
