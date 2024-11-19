#include "pch.h"
#include "assets.h"
#include "public/shader.h"
#include "public/multishader.h"
#include "utils/dxutils.h"

int ShaderV8_CreateFromMSW(CPakFile* pak, CPakDataChunk& hdrChunk, const fs::path& inputPath, CPakDataChunk& cpuDataChunk, ParsedDXShaderData_t* firstShaderData)
{
	CMultiShaderWrapperIO io = {};

	CMultiShaderWrapperIO::ShaderFile_t file = {};
	if (!io.ReadFile(inputPath.string().c_str(), &file))
	{
		Error("Failed to parse MSW file '%s'.\n", inputPath.string().c_str());
		return -1;
	}

	const size_t numShaderBuffers = file.shader->entries.size();

	ShaderAssetHeader_v8_t* hdr = reinterpret_cast<ShaderAssetHeader_v8_t*>(hdrChunk.Data());

	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	hdr->unk_C = static_cast<int>(numShaderBuffers);

	size_t totalShaderDataSize = 0;

	for (auto& it : file.shader->entries)
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
		const CMultiShaderWrapperIO::ShaderEntry_t& entry = file.shader->entries[i];

		ShaderByteCode_t* bc = reinterpret_cast<ShaderByteCode_t*>(cpuDataChunk.Data() + (i * entrySize));

		if (entry.buffer)
		{
			assert(entry.size > 0);

			bc->data = cpuDataChunk.GetPointer(nextBytecodeBufferOffset);
			bc->dataSize = entry.size;

			if (hdr->type == eShaderType::Vertex)
			{
				bc->inputSignatureBlob = bc->data;
				bc->unk = bc->dataSize;

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

	return static_cast<int>(file.shader->entries.size());
}

void Assets::AddShaderAsset_v8(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
	Log("Adding shdr asset '%s'\n", assetPath);

	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");

	if (!fs::exists(inputFilePath))
		Error("Failed to find compiled shader file for asset '%s' (%s).\n", assetPath, inputFilePath.string().c_str());

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderAssetHeader_v8_t), SF_HEAD, 8);

	const fs::path pakFilePath = fs::path(assetPath).replace_extension(""); // Make sure the path has no extension, just in case.
	CPakDataChunk nameChunk = pak->CreateDataChunk(pakFilePath.string().length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), pakFilePath.string().c_str());

	ShaderAssetHeader_v8_t* hdr = reinterpret_cast<ShaderAssetHeader_v8_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v8_t, name)));

	CPakDataChunk dataChunk = {};
	ParsedDXShaderData_t* shaderData = new ParsedDXShaderData_t;
	int numShaders = ShaderV8_CreateFromMSW(pak, hdrChunk, inputFilePath, dataChunk, shaderData);

	// Data chunk used by pointer "unk_10" and "shaderInputFlags"
	// The first set of data in the buffer is reserved data for when the asset is loaded (equal to 16 bytes per shader entry)
	// 
	// The remainder of the data is used for the input layout flags of each shader.
	// This data consists of 2 QWORDs per shader entry, with the first being set to the flags
	// and the second being unknown.
	const size_t inputFlagsDataSize = numShaders * (2 * sizeof(uint64_t));
	const size_t reservedDataSize = numShaders * (16);
	CPakDataChunk shaderInfoChunk = pak->CreateDataChunk(reservedDataSize + inputFlagsDataSize, SF_CPU, 1);

	// Get a pointer to the beginning of the reserved data section
	hdr->unk_10 = shaderInfoChunk.GetPointer();
	// Get a pointer after the end of the reserved data section
	hdr->shaderInputFlags = shaderInfoChunk.GetPointer(reservedDataSize);

	// Register the pointers we have just written.
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v8_t, unk_10)));
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v8_t, shaderInputFlags)));

	const uint8_t numFlagsPerShader = hdr->type == eShaderType::Vertex ? 2 : 1;

	// get input layout flags from json
	// this will eventually be taken from the bytecode itself, but that code will be very annoying so for now
	// the values are manually provided
	if (JSON_IS_ARRAY(mapEntry, "inputFlags") && mapEntry["inputFlags"].GetArray().Size() == static_cast<size_t>(numFlagsPerShader * numShaders))
	{
		uint64_t* inputFlags = reinterpret_cast<uint64_t*>(shaderInfoChunk.Data() + reservedDataSize);

		size_t i = 0;
		for (auto& elem : mapEntry["inputFlags"].GetArray())
		{
			uint64_t flag = 0;
			if (elem.IsUint64())
			{
				flag = elem.GetUint64();
			}
			else if (elem.IsString())
			{
				flag = strtoull(elem.GetString(), NULL, 0);
			}
			else
				Error("Found invalid input flag (idx %lld) for shader asset %s. Flag must be either an integer literal or a hex number as a string.\n", i, assetPath);


			if (flag == 0)
				Warning("Found potentially invalid input flag (idx %lld) for shader asset %s. Flag value is zero.\n", i, assetPath);

			// vertex shaders seem to have data every 8 bytes, unlike (seemingly) every other shader that only uses 8 out of every 16 bytes

			inputFlags[(3 - numFlagsPerShader) * i] = flag;

			i++;
		}
	}
	else
		Error("Invalid \"inputFlags\" field for shader asset %s. Must be an array with the same number of elements as there are shader buffers in the MSW file (%i).\n", assetPath, numShaders);

	// =======================================

	PakAsset_t asset;
	asset.InitAsset(
		pakFilePath.string() + ".rpak",
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::SHDR);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = 8;
	asset.SetPublicData(shaderData);

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	asset.remainingDependencyCount = 1;

	pak->PushAsset(asset);

	printf("\n");
}

int ShaderV12_CreateFromMSW(CPakFile* pak, CPakDataChunk& hdrChunk, const fs::path& inputPath, CPakDataChunk& cpuDataChunk, ParsedDXShaderData_t* firstShaderData)
{
	CMultiShaderWrapperIO io = {};
	
	CMultiShaderWrapperIO::ShaderFile_t file = {};
	if (!io.ReadFile(inputPath.string().c_str(), &file))
	{
		Error("Failed to parse MSW file '%s'.\n", inputPath.string().c_str());
		return -1;
	}

	ShaderAssetHeader_v12_t* hdr = reinterpret_cast<ShaderAssetHeader_v12_t*>(hdrChunk.Data());

	// Set to invalid so we can update it when a buffer is found, and detect that it has been set.
	hdr->type = eShaderType::Invalid;
	memcpy_s(hdr->shaderFeatures, sizeof(hdr->shaderFeatures), file.shader->features, sizeof(file.shader->features));

	const size_t numShaderBuffers = file.shader->entries.size();

	size_t totalShaderDataSize = 0;

	for (auto& it : file.shader->entries)
	{
		if(it.buffer != nullptr)
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
		const CMultiShaderWrapperIO::ShaderEntry_t& entry = file.shader->entries[i];

		ShaderByteCode_t* bc = reinterpret_cast<ShaderByteCode_t*>(cpuDataChunk.Data() + (i * entrySize));

		if (entry.buffer)
		{
			assert(entry.size > 0);

			bc->data = cpuDataChunk.GetPointer(nextBytecodeBufferOffset);
			bc->dataSize = entry.size;

			if (hdr->type == eShaderType::Vertex)
			{
				bc->inputSignatureBlob = bc->data;
				bc->unk = bc->dataSize;

				pak->AddPointer(cpuDataChunk.GetPointer((i * entrySize) + offsetof(ShaderByteCode_t, inputSignatureBlob)));
			}

			// Register the data pointer at the 
			pak->AddPointer(cpuDataChunk.GetPointer( (i * entrySize) + offsetof(ShaderByteCode_t, data) ));

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

	return static_cast<int>(file.shader->entries.size());
}

void Assets::AddShaderAsset_v12(CPakFile* pak, const char* assetPath, rapidjson::Value& mapEntry)
{
	Log("Adding shdr asset '%s'\n", assetPath);

	const fs::path inputFilePath = pak->GetAssetPath() / fs::path(assetPath).replace_extension("msw");

	if (!fs::exists(inputFilePath))
		Error("Failed to find compiled shader file for asset '%s' (%s).\n", assetPath, inputFilePath.string().c_str());

	CPakDataChunk hdrChunk = pak->CreateDataChunk(sizeof(ShaderAssetHeader_v12_t), SF_HEAD, 8);

	const fs::path pakFilePath = fs::path(assetPath).replace_extension(""); // Make sure the path has no extension, just in case.
	CPakDataChunk nameChunk = pak->CreateDataChunk(pakFilePath.string().length() + 1, SF_CPU | SF_DEV, 1);
	strcpy_s(nameChunk.Data(), nameChunk.GetSize(), pakFilePath.string().c_str());

	ShaderAssetHeader_v12_t* hdr = reinterpret_cast<ShaderAssetHeader_v12_t*>(hdrChunk.Data());

	hdr->name = nameChunk.GetPointer();

	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v12_t, name)));

	CPakDataChunk dataChunk = {};
	ParsedDXShaderData_t* shaderData = new ParsedDXShaderData_t;
	int numShaders = ShaderV12_CreateFromMSW(pak, hdrChunk, inputFilePath, dataChunk, shaderData);

	// Data chunk used by pointer "unk_10" and "shaderInputFlags"
	// The first set of data in the buffer is reserved data for when the asset is loaded (equal to 16 bytes per shader entry)
	// 
	// The remainder of the data is used for the input layout flags of each shader.
	// This data consists of 2 QWORDs per shader entry, with the first being set to the flags
	// and the second being unknown.
	const size_t inputFlagsDataSize = numShaders * (2 * sizeof(uint64_t));
	const size_t reservedDataSize   = numShaders * (16);
	CPakDataChunk shaderInfoChunk = pak->CreateDataChunk(reservedDataSize + inputFlagsDataSize, SF_CPU, 1);

	// Get a pointer to the beginning of the reserved data section
	hdr->unk_10 = shaderInfoChunk.GetPointer();
	// Get a pointer after the end of the reserved data section
	hdr->shaderInputFlags = shaderInfoChunk.GetPointer(reservedDataSize);

	// Register the pointers we have just written.
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v12_t, unk_10)));
	pak->AddPointer(hdrChunk.GetPointer(offsetof(ShaderAssetHeader_v12_t, shaderInputFlags)));

	const uint8_t numFlagsPerShader = hdr->type == eShaderType::Vertex ? 2 : 1;

	// get input layout flags from json
	// this will eventually be taken from the bytecode itself, but that code will be very annoying so for now
	// the values are manually provided
	if (JSON_IS_ARRAY(mapEntry, "inputFlags") && mapEntry["inputFlags"].GetArray().Size() == static_cast<size_t>(numFlagsPerShader * numShaders))
	{
		uint64_t* inputFlags = reinterpret_cast<uint64_t*>(shaderInfoChunk.Data() + reservedDataSize);

		size_t i = 0;
		for (auto& elem : mapEntry["inputFlags"].GetArray())
		{
			uint64_t flag = 0;
			if (elem.IsUint64())
			{
				flag = elem.GetUint64();
			}
			else if (elem.IsString())
			{
				flag = strtoull(elem.GetString(), NULL, 0);
			}
			else
				Error("Found invalid input flag (idx %lld) for shader asset %s. Flag must be either an integer literal or a hex number as a string.\n", i, assetPath);


			if (flag == 0)
				Warning("Found potentially invalid input flag (idx %lld) for shader asset %s. Flag value is zero.\n", i, assetPath);

			// vertex shaders seem to have data every 8 bytes, unlike (seemingly) every other shader that only uses 8 out of every 16 bytes

			inputFlags[(3 - numFlagsPerShader) * i] = flag;

			i++;
		}
	}
	else
		Error("Invalid \"inputFlags\" field for shader asset %s. Must be an array with the same number of elements as there are shader buffers in the MSW file (%i).\n", assetPath, numShaders);

	// =======================================


	uint64_t assetGuid = 0;

	if (JSON_IS_UINT64(mapEntry, "$guid"))
	{
		assetGuid = JSON_GET_UINT64(mapEntry, "$guid", 0);
	}
	else if (JSON_IS_STR(mapEntry, "$guid"))
	{
		RTech::ParseGUIDFromString(JSON_GET_STR(mapEntry, "$guid", ""), &assetGuid);
	}
	else
	{
		assetGuid = RTech::StringToGuid((pakFilePath.string() + ".rpak").c_str());
	}

	if (assetGuid == 0)
		Error("Invalid GUID provided for asset '%s'.\n", assetPath);

	PakAsset_t asset;
	asset.InitAsset(
		assetGuid,
		hdrChunk.GetPointer(), hdrChunk.GetSize(),
		dataChunk.GetPointer(), UINT64_MAX, UINT64_MAX, AssetType::SHDR);
	asset.SetHeaderPointer(hdrChunk.Data());

	asset.version = 12;
	asset.SetPublicData(shaderData);

	asset.pageEnd = pak->GetNumPages();

	// this doesnt account for external dependencies atm
	asset.remainingDependencyCount = 1;

	pak->PushAsset(asset);

	printf("\n");
}