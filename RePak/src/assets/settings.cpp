#include "pch.h"
#include "Assets.h"
#include "assets/settings.h"

std::vector<std::string> strsplit(std::string str, std::string deli = " ")
{
	std::vector<std::string> StringArray;

	int start = 0;
	int end = str.find(deli);
	while (end != -1) {
		StringArray.push_back(str.substr(start, end - start));
		start = end + deli.size();
		end = str.find(deli, start);
	}
	StringArray.push_back(str.substr(start, end - start));
	return StringArray;
}

std::unordered_map<std::string, SettingsFieldType> SettingsTypeMap =
{
	{ "bool", SettingsFieldType::Bool },
	{ "int", SettingsFieldType::Int},
	{ "float", SettingsFieldType::Float},
	{ "float2", SettingsFieldType::Float2},
	{ "float3", SettingsFieldType::Float3},
	{ "string", SettingsFieldType::String},
	{ "asset", SettingsFieldType::Asset},
	{ "asset2", SettingsFieldType::Asset2},
	{ "array", SettingsFieldType::Array},
	{ "array2", SettingsFieldType::Array2},
};

SettingsFieldType GetFieldTypeFromString(std::string sType)
{
	std::transform(sType.begin(), sType.end(), sType.begin(), ::tolower);

	for (const auto& [key, value] : SettingsTypeMap) // Iterate through unordered_map.
	{
		if (sType.compare(key) == 0) // Do they equal?
			return value;
	}

	return SettingsFieldType::String;
}

size_t FieldTypeToSize(SettingsFieldType FieldType)
{
	switch (FieldType)
	{
	case SettingsFieldType::String:
	case SettingsFieldType::Asset:
	case SettingsFieldType::Asset2: return sizeof(RPakPtr);
	case SettingsFieldType::Int: return sizeof(int);
	case SettingsFieldType::Bool: return sizeof(bool);
	case SettingsFieldType::Float: return sizeof(float);
	case SettingsFieldType::Float2: return sizeof(Vector2);
	case SettingsFieldType::Float3: return sizeof(Vector3);
	case SettingsFieldType::Array2: return sizeof(uint64_t);
	}
}

void SetupBuffersFromFieldType(rapidjson::Value& Value, size_t& ValueBuffer, size_t& StringBuffer)
{
	if (Value.HasMember("value") && Value.HasMember("type") && Value["type"].IsString())
	{
		std::string type = Value["type"].GetStdString();
		SettingsFieldType FieldType = GetFieldTypeFromString(type);
		size_t size = FieldTypeToSize(FieldType);
		uint64_t ValueOffset = 0;

		if (Value.HasMember("offset") && Value["offset"].IsUint64() && Value["offset"].GetUint64() != 0)
			ValueOffset = Value["offset"].GetUint64();

		switch (FieldType)
		{
		case SettingsFieldType::String:
		case SettingsFieldType::Asset:
		case SettingsFieldType::Asset2:
			StringBuffer += Value["value"].GetStdString().length() + 1;

		case SettingsFieldType::Int:
		case SettingsFieldType::Bool:
		case SettingsFieldType::Float:
		case SettingsFieldType::Float2:
		case SettingsFieldType::Float3:
		case SettingsFieldType::Array2:
			if (ValueOffset != 0)
				ValueBuffer = ValueOffset + size;
			else ValueBuffer += ValueOffset + size;
			break;

		default:
			Error("Unknown Item Type Found in Settings asset Exiting...\n");
		}
	}
}

void Assets::AddSettingsLayoutAsset_v0(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	std::string sAssetName = assetPath;
	sAssetName = "settings_layout/settings_" + sAssetName + "_layout.rpak";
	uint32_t assetPathSize = (sAssetName.length() + 1);

	Log("\n==============================\n");
	Log("Asset tlts -> '%s'\n", sAssetName.c_str());

	SettingsLayoutHeader* hdr = new SettingsLayoutHeader();

	if (!mapEntry.HasMember("layout"))
		Error("Required field 'layout' not found for tlts asset '%s'. Exiting...\n", assetPath);
	else if (!mapEntry["layout"].IsArray())
		Error("'layout' field is not of required type 'array' for tlts asset '%s'. Exiting...\n", assetPath);

	std::vector<SettingsLayoutItem> items;

	size_t StringBufferSize = 0;
	for (auto& it : mapEntry["layout"].GetArray()) // build string buffer size
	{
		if (it.HasMember("name") && it["name"].IsString())
			StringBufferSize += it["name"].GetStdString().length() + 1;
		else Error("Required field 'name' not found for layout item. Exiting...\n");
	}

	char* StringBufferData = new char[StringBufferSize];

	size_t StringBufferOffset = 0;
	for (auto& it : mapEntry["layout"].GetArray()) // build items
	{
		SettingsLayoutItem item;

		if (it.HasMember("type") && it["type"].IsString())
			item.type = GetFieldTypeFromString(it["type"].GetStdString());
		else item.type = SettingsFieldType::String;

		if (it.HasMember("name") && it["name"].IsString())
		{
			std::string Name = it["name"].GetStdString();
			snprintf(StringBufferData + StringBufferOffset, Name.length() + 1, "%s", Name.c_str());

			item.ValueOffset = items.size() * sizeof(uint64_t);

			item.NameOffset = StringBufferOffset;

			StringBufferOffset += Name.length() + 1;
		}

		items.push_back(item);
	}

	hdr->itemsCount = items.size();

	uint64_t itemDataSize = items.size() * sizeof(SettingsLayoutItem);

	// asset header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(SettingsLayoutHeader), SF_HEAD /*| SF_CLIENT*/, 8);

	// name data
	_vseginfo_t nameinfo = pak->CreateNewSegment(assetPathSize, SF_CPU, assetPathSize % 4);
	uint32_t dataBufSize = (sAssetName.length() + (sAssetName.length() % 4));
	char* nameData = new char[dataBufSize];
	{
		snprintf(nameData, assetPathSize, "%s", sAssetName.c_str());
		hdr->pName = { nameinfo.index, 0 };

		pak->AddPointer(nameinfo.index, offsetof(SettingsLayoutHeader, pName));
	}

	// item data
	_vseginfo_t iteminfo = pak->CreateNewSegment(itemDataSize, SF_CPU, 8);
	char* itemData = new char[itemDataSize];
	{
		memcpy(itemData, items.data(), itemDataSize);
		hdr->pItems = { iteminfo.index, 0 };
		pak->AddPointer(iteminfo.index, offsetof(SettingsLayoutHeader, pItems));
	}

	// string buffer data
	_vseginfo_t stringbufferinfo = pak->CreateNewSegment(StringBufferSize, SF_CPU, 8);
	hdr->pStringBuf = { stringbufferinfo.index , 0 };
	pak->AddPointer(stringbufferinfo.index, offsetof(SettingsLayoutHeader, pStringBuf));

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)hdr });
	pak->AddRawDataBlock({ nameinfo.index, nameinfo.size, (uint8_t*)nameData });
	pak->AddRawDataBlock({ iteminfo.index, iteminfo.size, (uint8_t*)itemData });
	pak->AddRawDataBlock({ stringbufferinfo.index, stringbufferinfo.size, (uint8_t*)StringBufferData });

	//create and init the asset entry
	RPakAssetEntry asset;
	asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, iteminfo.index, 0, -1, -1, (std::uint32_t)AssetType::STLT);
	asset.version = 0;

	asset.pageEnd = stringbufferinfo.index + 1; // number of the highest page that the asset references pageidx + 1
	asset.unk1 = 2;

	asset.usesStartIdx = 0;
	asset.usesCount = 0; // the asset should only use 1 other asset for the atlas

	// add the asset entry
	assetEntries->push_back(asset);
}

void Assets::AddSettingsAsset_v1(CPakFile* pak, std::vector<RPakAssetEntry>* assetEntries, const char* assetPath, rapidjson::Value& mapEntry)
{
	std::vector<RPakGuidDescriptor> guids{};

	std::string sAssetName = assetPath;
	sAssetName = "settings/" + sAssetName + ".rpak";
	uint32_t assetPathSize = (sAssetName.length() + 1);

	Log("\n==============================\n");
	Log("Asset stgs -> '%s'\n", sAssetName.c_str());

	//ParseLayout(sAssetName);
	SettingsHeader* hdr = new SettingsHeader();

	//header
	_vseginfo_t subhdrinfo = pak->CreateNewSegment(sizeof(SettingsHeader), SF_HEAD /*| SF_CLIENT*/, 8);

	pak->AddPointer(subhdrinfo.index, 0);

	if (mapEntry.HasMember("kvp")) // KVP Buffer Size
	{
		if (mapEntry["kvp"].IsUint64() && mapEntry["kvp"].GetUint64() != 0)
			hdr->KvpBufferSize = mapEntry["kvp"].GetUint64();

		else Error("Required field 'layout' has to be type 'Uint64' for settings item. Exiting...\n");
	}
	else hdr->KvpBufferSize = 0x1024 + (0x8 * assetEntries->size());

	if (mapEntry.HasMember("layout")) // layout guid
	{
		if (mapEntry["layout"].IsString() && mapEntry["layout"].GetStdString() != "")
			hdr->LayoutGUID = RTech::StringToGuid(std::string("settings_layout/settings_" + mapEntry["layout"].GetStdString() + "_layout.rpak").c_str());
		else if (mapEntry["layout"].IsUint64() && mapEntry["layout"].GetUint64() != 0)
			hdr->LayoutGUID = mapEntry["layout"].GetUint64();
		else Error("Required field 'layout' has to be type 'Uint64' or 'string' for settings item. Exiting...\n");
	}
	else Error("Required field 'layout' not found for settings item. Exiting...\n");

	pak->AddGuidDescriptor(&guids, subhdrinfo.index, offsetof(SettingsHeader, LayoutGUID));

	uint32_t nameBufSize = sAssetName.length() + 1;
	_vseginfo_t nameinfo = pak->CreateNewSegment(nameBufSize, SF_CPU, 64);
	char* nameData = new char[nameBufSize];
	{
		snprintf(nameData, sAssetName.length() + 1, "%s", sAssetName.c_str());
		hdr->Name = { nameinfo.index, 0 };
	}

	size_t ValueBufferSize = 0;
	size_t StringBufferSize = 0;

	// build buffers
	for (auto& it : mapEntry["items"].GetArray())
		SetupBuffersFromFieldType(it, ValueBufferSize, StringBufferSize);

	ValueBufferSize += sizeof(uint64_t);

	// item data
	_vseginfo_t valueinfo = pak->CreateNewSegment(ValueBufferSize, SF_CPU, 64);
	hdr->Values = { valueinfo.index , 0 };

	// string buffer data
	_vseginfo_t stringbufinfo = pak->CreateNewSegment(StringBufferSize, SF_CPU, 64);
	hdr->StringBuf = { stringbufinfo.index , 0 };

	char* ValueBufferData = new char[ValueBufferSize];
	char* StringBufferData = new char[StringBufferSize];

	pak->AddPointer(subhdrinfo.index, offsetof(SettingsHeader, Name));
	pak->AddPointer(subhdrinfo.index, offsetof(SettingsHeader, Values));
	pak->AddPointer(subhdrinfo.index, offsetof(SettingsHeader, StringBuf));

	size_t StringBufferOffset = 0;
	size_t ValueBufferOffset = 0;

	rmem valbuf(ValueBufferData);

	for (auto& it : mapEntry["items"].GetArray()) // build buffers
	{
		SettingsFieldType FieldType = SettingsFieldType::String;
		size_t ValueSize = 0;
		uint64_t ValueOffset = 0;

		if (it.HasMember("type") && it["type"].IsString())
		{
			std::string string = it["type"].GetStdString();
			if (string != "")
			{
				FieldType = GetFieldTypeFromString(string);
				ValueSize = FieldTypeToSize(FieldType);
			}
		}
		else Error("Required field 'type' not found or is not a string for settings item. Exiting...\n");

		if (it.HasMember("offset") && it["offset"].IsUint64() && it["offset"].GetUint64() != 0)
			ValueOffset = it["offset"].GetUint64();

		if (it.HasMember("value"))
		{
			if (ValueOffset != 0)
				ValueBufferOffset = ValueOffset;

			Debug("Offset: %d -> Type: %d -> Size: %d\n", ValueBufferOffset, (uint32_t)FieldType, (uint32_t)ValueSize);

			switch (FieldType)
			{
			case SettingsFieldType::String:
			case SettingsFieldType::Asset:
			case SettingsFieldType::Asset2:
			{
				snprintf(StringBufferData + StringBufferOffset, it["value"].GetStdString().length() + 1, "%s", it["value"].GetStdString().c_str());

				RPakPtr StringPtr{ stringbufinfo.index, StringBufferOffset };

				valbuf.write(StringPtr, ValueBufferOffset);

				StringBufferOffset += it["value"].GetStdString().length() + 1;

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}
			case SettingsFieldType::Int:
			{
				int value = it["value"].GetInt();
				valbuf.write(value, ValueBufferOffset);

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}
			case SettingsFieldType::Bool:
			{
				bool value = it["value"].GetBool();
				valbuf.write(value, ValueBufferOffset);

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}
			case SettingsFieldType::Float:
			{
				float value = it["value"].GetFloat();
				valbuf.write(value, ValueBufferOffset);

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}

			case SettingsFieldType::Float2:
			{
				auto vector_array = it["value"].GetArray();

				Vector2 value{ vector_array[0].GetFloat() , vector_array[1].GetFloat() };
				valbuf.write(value, ValueBufferOffset);

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}

			case SettingsFieldType::Float3:
			{
				auto vector_array = it["value"].GetArray();

				Vector3 value{ vector_array[0].GetFloat() , vector_array[1].GetFloat(), vector_array[2].GetFloat() };
				valbuf.write(value, ValueBufferOffset);

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}

			case SettingsFieldType::Array2:
			{
				uint64_t arraydata = 0;

				valbuf.write(arraydata, ValueBufferOffset);

				if (ValueOffset == 0)
					ValueBufferOffset += ValueSize;
				break;
			}
			default:
				Error("Unknown Item Type Found in Settings asset Exiting...\n");
			}

			pak->AddPointer(valueinfo.index, ValueBufferOffset);
		}
	}

	Debug("Unk1: %d / 0x%llX\n", hdr->Unk1, hdr->Unk1);
	Debug("kvp: %d / 0x%llX\n", hdr->KvpBufferSize, hdr->KvpBufferSize);

	pak->AddRawDataBlock({ subhdrinfo.index, subhdrinfo.size, (uint8_t*)hdr });
	pak->AddRawDataBlock({ nameinfo.index, nameinfo.size, (uint8_t*)nameData });
	pak->AddRawDataBlock({ valueinfo.index, valueinfo.size, (uint8_t*)ValueBufferData });
	pak->AddRawDataBlock({ stringbufinfo.index, stringbufinfo.size, (uint8_t*)StringBufferData });

	RPakAssetEntry asset;
	asset.InitAsset(RTech::StringToGuid(sAssetName.c_str()), subhdrinfo.index, 0, subhdrinfo.size, -1, 0, -1, -1, (std::uint32_t)AssetType::STGS);
	asset.version = 1;
	asset.unk1 = 2;

	asset.pageEnd = stringbufinfo.index + 1; // number of the highest page that the asset references pageidx + 1

	// add the asset entry
	assetEntries->push_back(asset);
}