#include "pch.h"
#include "assets.h"
#include "public/shader.h"
#include "public/multishader.h"
#include "utils/dxutils.h"

static void Shader_LoadFromMSW(CPakFile* const pak, const char* const assetPath, CMultiShaderWrapperIO::ShaderCache_t& shaderCache)
{
	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");
	MSW_ParseFile(inputFilePath, shaderCache, MultiShaderWrapperFileType_e::SHADER);
}

template <typename ShaderAssetHeader_t>
static void Shader_CreateFromMSW(CPakFile* const pak, CPakDataChunk& cpuDataChunk, ParsedDXShaderData_t* const firstShaderData,
								const CMultiShaderWrapperIO::Shader_t* shader, ShaderAssetHeader_t* const hdr)
{
	const size_t numShaderBuffers = shader->entries.size();
	size_t totalShaderDataSize = 0;

	for (auto& it : shader->entries)
	{
		if (it.buffer != nullptr)
			totalShaderDataSize += IALIGN(it.size, 8);

		// If the shader type hasn't been found yet, parse this buffer and find out what we want to set it as.
		if (hdr->type == eShaderType::Invalid)
		{
			if (DXUtils::GetParsedShaderData(it.buffer, it.size, firstShaderData))
			{
				if (firstShaderData->foundFlags & SHDR_FOUND_RDEF)
				{
					hdr->type = static_cast<eShaderType>(firstShaderData->pakShaderType);
				}
			}
		}
	}
	assert(totalShaderDataSize != 0);

	const int8_t entrySize = hdr->type == eShaderType::Vertex ? 24 : 16;

	// Size of the data that describes each shader bytecode buffer
	const size_t descriptorSize = numShaderBuffers * entrySize;

	const size_t shaderBufferChunkSize = descriptorSize + totalShaderDataSize;
	cpuDataChunk = pak->CreateDataChunk(shaderBufferChunkSize, SF_CPU | SF_TEMP, 8);

	// Offset at which the next bytecode buffer will be written.
	// Initially starts at the end of the descriptors and then gets increased every time a buffer is written.
	size_t nextBytecodeBufferOffset = descriptorSize;

	for (size_t i = 0; i < numShaderBuffers; ++i)
	{
		const CMultiShaderWrapperIO::ShaderEntry_t& entry = shader->entries[i];

		ShaderByteCode_t* bc = reinterpret_cast<ShaderByteCode_t*>(cpuDataChunk.Data() + (i * entrySize));

		if (entry.buffer)
		{
			assert(entry.size > 0);

			bc->data = cpuDataChunk.GetPointer(nextBytecodeBufferOffset);
			bc->dataSize = entry.size;

			if (hdr->type == eShaderType::Vertex)
			{
				bc->inputSignatureBlobSize = bc->dataSize;
				bc->inputSignatureBlob = bc->data;

				pak->AddPointer(cpuDataChunk.GetPointer((i * entrySize) + offsetof(ShaderByteCode_t, inputSignatureBlob)));
			}

			// Register the data pointer at the 
			pak->AddPointer(cpuDataChunk.GetPointer((i * entrySize) + offsetof(ShaderByteCode_t, data)));

			memcpy_s(cpuDataChunk.Data() + nextBytecodeBufferOffset, entry.size, entry.buffer, entry.size);

			nextBytecodeBufferOffset += IALIGN(entry.size, 8);
		}
		else
		{
			// Null shader
			if (entry.refIndex == UINT16_MAX)
			{
				// technically, these shaders can actually have a pointer to an empty dx bytecode buffer (with no actual bytecode from what i can tell?)
				// but the logic of the function that reads them should mean that the pointer is never accessed if entry.size is <= 0
			}
			else // Shader entry references another shader instead of defining its own buffer
			{
				bc->dataSize = ~entry.refIndex;
			}
		}
	}

	// Data chunk used by pointer "unk_10" and "shaderInputFlags"
	// The first set of data in the buffer is reserved data for when the asset is loaded (equal to 16 bytes per shader entry)
	// 
	// The remainder of the data is used for the input layout flags of each shader.
	// This data consists of 2 QWORDs per shader entry, with the first being set to the flags
	// and the second being unknown.
	const size_t inputFlagsDataSize = numShaderBuffers * (2 * sizeof(uint64_t));
	const size_t reservedDataSize = numShaderBuffers * (16);
	CPakDataChunk shaderInfoChunk = pak->CreateDataChunk(reservedDataSize + inputFlagsDataSize, SF_CPU, 1);

	// Get a pointer to the beginning of the reserved data section
	hdr->unk_10 = shaderInfoChunk.GetPointer();
	// Get a pointer after the end of the reserved data section
	hdr->shaderInputFlags = shaderInfoChunk.GetPointer(reservedDataSize);

	uint64_t* const inputFlags = reinterpret_cast<uint64_t*>(shaderInfoChunk.Data() + reservedDataSize);
	size_t i = 0;

	// vertex shaders seem to have data every 8 bytes, unlike (seemingly) every other shader that only uses 8 out of every 16 bytes
	for (auto& it : shader->entries)
	{
		inputFlags[i] = it.flags[0];
		inputFlags[i+1] = it.flags[1];

		i+=2;
	}
}

static void Shader_SetupHeader(ShaderAssetHeader_v8_t* const hdr, const CMultiShaderWrapperIO::Shader_t* const shader)
{
	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	hdr->numShaderBuffers = static_cast<int>(shader->entries.size());
}

static void Shader_SetupHeader(ShaderAssetHeader_v12_t* const hdr, const CMultiShaderWrapperIO::Shader_t* const shader)
{
	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	memcpy_s(hdr->shaderFeatures, sizeof(hdr->shaderFeatures), shader->features, sizeof(shader->features));
}

template<typename ShaderAssetHeader_t>
static void Shader_InternalAddShader(CPakFile* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, 
									const PakGuid_t shaderGuid, const int assetVersion, const bool errorOnDuplicate)
{
	PakAsset_t* const existingAsset = pak->GetAssetByGuid(shaderGuid, nullptr, true);
	if (existingAsset)
	{
		if (errorOnDuplicate)
			Error("Tried to add shader asset \"%s\" twice.\n", assetPath);

		return;
	}

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderAssetHeader_t), SF_HEAD, 8);

	ShaderAssetHeader_t* const hdr = reinterpret_cast<ShaderAssetHeader_t*>(hdrChunk.Data());
	Shader_SetupHeader(hdr, shader);

	const size_t nameLen = shader->name.length();

	if (nameLen > 0)
	{
		CPakDataChunk nameChunk = pak->CreateDataChunk(nameLen + 1, SF_CPU | SF_DEV, 1);
		strcpy_s(nameChunk.Data(), nameChunk.GetSize(), shader->name.c_str());

		hdr->name = nameChunk.GetPointer();
		pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_t, name)));
	}

	CPakDataChunk dataChunk = {};
	ParsedDXShaderData_t* const shaderData = new ParsedDXShaderData_t;

	Shader_CreateFromMSW(pak, dataChunk, shaderData, shader, hdr);

	// Register the pointers we have just written.
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_t, unk_10)));
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_t, shaderInputFlags)));

	// =======================================

	PakAsset_t asset;
	asset.InitAsset(
		assetPath,
		shaderGuid,
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::SHDR);

	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = assetVersion;
	asset.SetPublicData(shaderData);

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	// note(amos): do shaders even depend on external dependencies?
	//             remove the comments if its confirmed to not depend on external dependencies.
	asset.remainingDependencyCount = 1;

	pak->PushAsset(asset);
}

void Shader_AddShaderV8(CPakFile* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const bool errorOnDuplicate)
{
	Shader_InternalAddShader<ShaderAssetHeader_v8_t>(pak, assetPath, shader, shaderGuid, 8, errorOnDuplicate);
}

void Shader_AddShaderV12(CPakFile* const pak, const char* const assetPath, const CMultiShaderWrapperIO::Shader_t* const shader, const PakGuid_t shaderGuid, const bool errorOnDuplicate)
{
	Shader_InternalAddShader<ShaderAssetHeader_v12_t>(pak, assetPath, shader, shaderGuid, 12, errorOnDuplicate);
}

void Assets::AddShaderAsset_v8(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	CMultiShaderWrapperIO::ShaderCache_t cache = {};

	Shader_LoadFromMSW(pak, assetPath, cache);
	Shader_AddShaderV8(pak, assetPath, cache.shader, Pak_GetGuidOverridable(mapEntry, assetPath), true);
}

void Assets::AddShaderAsset_v12(CPakFile* const pak, const char* const assetPath, const rapidjson::Value& mapEntry)
{
	CMultiShaderWrapperIO::ShaderCache_t cache = {};

	Shader_LoadFromMSW(pak, assetPath, cache);
	Shader_AddShaderV12(pak, assetPath, cache.shader, Pak_GetGuidOverridable(mapEntry, assetPath), true);
}
