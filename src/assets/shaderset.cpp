#include "pch.h"
#include "assets.h"
#include "utils/dxutils.h"
#include "utils/utils.h"
#include "public/shaderset.h"
#include "public/shader.h"
#include "public/multishader.h"

static void ShaderSet_LoadFromMSW(CPakFileBuilder* const pak, const char* const assetPath, CMultiShaderWrapperIO::ShaderCache_t& shaderCache)
{
	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");
	MSW_ParseFile(inputFilePath, shaderCache, MultiShaderWrapperFileType_e::SHADERSET);
}

template <typename ShaderSetAssetHeader_t>
static void ShaderSet_SetInputSlots(ShaderSetAssetHeader_t* const hdr, PakAsset_t* const /*shader*/, const bool isVertexShader, const CMultiShaderWrapperIO::ShaderSet_t* const shaderSet, const int assetVersion)
{
	uint16_t* inputCount;

	// note: in v8 shader sets the vertex shader is set in the first slot, whereas
	// in v11 the vertex shader is set in the second slot and the first texture
	if (assetVersion == 8)
		inputCount = isVertexShader ? &hdr->textureInputCounts[0] : &hdr->textureInputCounts[1];
	else
		inputCount = isVertexShader ? &hdr->textureInputCounts[1] : &hdr->textureInputCounts[0];

	const uint8_t textureCount = isVertexShader ? (uint8_t)shaderSet->numVertexShaderTextures : (uint8_t)shaderSet->numPixelShaderTextures;
	//const char* const shaderName = isVertexShader ? "vertex" : "pixel";

	// If we have the shader in this pak, perform this check to make sure the
	// shader provided by the user isn't corrupt or otherwise incompatible.
	//if (shader)
	//{
	//	const ParsedDXShaderData_t* const shaderData = reinterpret_cast<const ParsedDXShaderData_t*>(shader->PublicData());

	//	// cannot be null at this point
	//	assert(shaderData);

	//	if (shaderData->mtlTexSlotCount != textureCount)
	//		Error("Texture slot count mismatch between shader set and %s shader (expected %hhu, got %hhu).\n", shaderName, shaderData->mtlTexSlotCount, textureCount);
	//}

	*inputCount = textureCount;
}

extern bool Shader_AutoAddShader(CPakFileBuilder* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const int shaderAssetVersion);

static void ShaderSet_AutoAddEmbeddedShader(CPakFileBuilder* const pak, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const int assetVersion)
{
	if (shader)
	{
		const std::string assetPath = Utils::VFormat("embedded_%llx", shaderGuid);

		// note: shaderset v8 is tied to shader v8, shaderset v11 is tied to shader v12.
		Shader_AutoAddShader(pak, assetPath.c_str(), shader, shaderGuid, assetVersion == 8 ? 8 : 12);
	}
}

template <typename ShaderSetAssetHeader_t>
void ShaderSet_InternalCreateSet(CPakFileBuilder* const pak, const char* const assetPath, const CMultiShaderWrapperIO::ShaderSet_t* const shaderSet, const PakGuid_t shaderSetGuid, const int assetVersion)
{
	ShaderSet_AutoAddEmbeddedShader(pak, shaderSet->vertexShader, shaderSet->vertexShaderGuid, assetVersion);
	ShaderSet_AutoAddEmbeddedShader(pak, shaderSet->pixelShader, shaderSet->pixelShaderGuid, assetVersion);

	PakAsset_t asset;

	PakPageLump_s hdrChunk = pak->CreatePageLump(sizeof(ShaderSetAssetHeader_t), SF_HEAD, 8);
	ShaderSetAssetHeader_t* const hdr = reinterpret_cast<ShaderSetAssetHeader_t*>(hdrChunk.data);

	if (pak->IsFlagSet(PF_KEEP_DEV))
	{
		char pathStem[256];
		const size_t stemLen = Pak_ExtractAssetStem(assetPath, pathStem, sizeof(pathStem));

		if (stemLen > 0)
		{
			PakPageLump_s nameChunk = pak->CreatePageLump(stemLen + 1, SF_CPU | SF_DEV, 1);
			memcpy(nameChunk.data, pathStem, stemLen + 1);

			pak->AddPointer(hdrChunk, offsetof(ShaderSetAssetHeader_t, name), nameChunk, 0);
		}
	}

	// === Shader Inputs ===
	hdr->vertexShader = shaderSet->vertexShaderGuid;
	hdr->pixelShader = shaderSet->pixelShaderGuid;

	PakAsset_t* vertexShader = nullptr;
	PakAsset_t* pixelShader = nullptr;

	// todo: can shader refs be null???
	if (hdr->vertexShader != 0)
	{
		Pak_RegisterGuidRefAtOffset(hdr->vertexShader, offsetof(ShaderSetAssetHeader_t, vertexShader), hdrChunk, asset);
		vertexShader = pak->GetAssetByGuid(hdr->vertexShader);
	}

	if (hdr->pixelShader != 0)
	{
		Pak_RegisterGuidRefAtOffset(hdr->pixelShader, offsetof(ShaderSetAssetHeader_t, pixelShader), hdrChunk, asset);
		pixelShader = pak->GetAssetByGuid(hdr->pixelShader);
	}

	ShaderSet_SetInputSlots(hdr, vertexShader, true, shaderSet, assetVersion);
	ShaderSet_SetInputSlots(hdr, pixelShader, false, shaderSet, assetVersion);

	// On v11 shaders, the input count is added on top of the second one.
	if (assetVersion == 11)
		hdr->textureInputCounts[1] += hdr->textureInputCounts[0];

	hdr->numSamplers = shaderSet->numSamplers;

	// only used for ui/ui_world shadersets in R5
	// for now this will have to be manually set if used, because i cannot figure out a way to programmatically
	// detect these resources without incorrectly identifying some, and i doubt that's a good thing
	hdr->firstResourceBindPoint = shaderSet->firstResourceBindPoint;
	hdr->numResources = shaderSet->numResources;

	asset.InitAsset(
		assetPath,
		shaderSetGuid,
		hdrChunk.GetPointer(), hdrChunk.size,
		PagePtr_t::NullPtr(), UINT64_MAX, UINT64_MAX, AssetType::SHDS);
	asset.SetHeaderPointer(hdrChunk.data);

	asset.version = assetVersion;
	asset.pageEnd = pak->GetNumPages();

	pak->PushAsset(asset);
}

// TODO:
// Parse numSamplers from pixelShader
// Parse textureInputCount from pixelShader
// Figure out the other count variable before texture input count
// See if any of the other unknown variables are actually required

template <typename ShaderSetAssetHeader_t>
static void ShaderSet_AddFromMap(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry, const int assetVersion)
{
	UNUSED(mapEntry);

	CMultiShaderWrapperIO::ShaderCache_t cache = {};
	ShaderSet_LoadFromMSW(pak, assetPath, cache);

	ShaderSet_InternalCreateSet<ShaderSetAssetHeader_t>(pak, assetPath, &cache.shaderSet, assetGuid, assetVersion);
}

void Assets::AddShaderSetAsset_v8(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	ShaderSet_AddFromMap<ShaderSetAssetHeader_v8_t>(pak, assetGuid, assetPath, mapEntry, 8);
}

void Assets::AddShaderSetAsset_v11(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	ShaderSet_AddFromMap<ShaderSetAssetHeader_v11_t>(pak, assetGuid, assetPath, mapEntry, 11);
}
