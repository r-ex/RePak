#include "pch.h"
#include "assets.h"
#include "public/material_for_aspect.h"

static void Material4Aspect_InternalAdd(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
	const std::string mt4aPath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, "json");
	rapidjson::Document document;

	if (!JSON_ParseFromFile(mt4aPath.c_str(), "material for aspect", document, false))
		Error("Failed to open material_for_aspect asset \"%s\".\n", mt4aPath.c_str());

	// Parse manually added entries here.
	rapidjson::Value::ConstMemberIterator materialsIt;
	JSON_GetRequired(document, "materials", JSONFieldType_e::kArray, materialsIt);

	const rapidjson::Value::ConstArray materialArray = materialsIt->value.GetArray();
	const size_t arraySize = materialArray.Size();

	if (arraySize == 0)
		Error("Array \"materials\" is empty.\n");

	if (arraySize > MaterialShaderType_e::_TYPE_COUNT)
		Error("Array \"materials\" is too large (%zu > %zu).\n", arraySize, (size_t)MaterialShaderType_e::_TYPE_COUNT);

	PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
	PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(MaterialForAspect_s), SF_HEAD | SF_CLIENT, 8);

	MaterialForAspect_s* const mt4a = reinterpret_cast<MaterialForAspect_s*>(hdrLump.data);

	int64_t matIndex = -1;
	for (const auto& material : materialArray)
	{
		matIndex++;

		char buffer[32]; const char* base = "material #";
		char* current = std::copy(base, base + 10, buffer);
		std::to_chars_result result = std::to_chars(current, buffer + sizeof(buffer), matIndex);

		*result.ptr = '\0';

		const char* dependencyName = nullptr;
		const PakGuid_t guid = Pak_ParseGuidFromObject(material, buffer, dependencyName);

		mt4a->materials[matIndex] = guid;
		Pak_RegisterGuidRefAtOffset(guid, offsetof(MaterialForAspect_s, materials) + matIndex * sizeof(PakGuid_t), hdrLump, asset);
	}

	asset.InitAsset(hdrLump.GetPointer(), sizeof(MaterialForAspect_s), PagePtr_t::NullPtr(), MT4A_VERSION, AssetType::MT4A);
	asset.SetHeaderPointer(hdrLump.data);

	pak->FinishAsset();
}

void Assets::AddMaterialForAspectAsset_v3(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	UNUSED(mapEntry);
	Material4Aspect_InternalAdd(pak, assetGuid, assetPath);
};
