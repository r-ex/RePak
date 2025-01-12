#include "pch.h"
#include "assets.h"
#include "public/texture_anim.h"

static char* TextureAnim_ParseFromTXAN(const char* const assetPath, unsigned int& layerCount, unsigned int& slotCount)
{
	BinaryIO bio;

	if (!bio.Open(assetPath, BinaryIO::Mode_e::Read))
		Error("Failed to open texture animation file \"%s\".\n", assetPath);

	const size_t fileSize = bio.GetSize();

	if (bio.GetSize() <= sizeof(TextureAnimFileHeader_t))
		Error("Texture animation file \"%s\" appears truncated (%zu <= %zu).\n", assetPath, fileSize, sizeof(TextureAnimFileHeader_t));

	TextureAnimFileHeader_t hdr;
	bio.Read(hdr);

	if (hdr.magic != TXAN_FILE_MAGIC)
		Error("Attempted to load an invalid texture animation file (expected magic %x, got %x).\n", TXAN_FILE_MAGIC, hdr.magic);

	if (hdr.fileVersion != TXAN_FILE_VERSION)
		Error("Attempted to load an unsupported texture animation file (expected file version %x, got %x).\n", TXAN_FILE_VERSION, hdr.fileVersion);

	if (hdr.assetVersion != TXAN_VERSION)
		Error("Attempted to load an unsupported texture animation file (expected asset version %x, got %x).\n", TXAN_VERSION, hdr.assetVersion);

	const size_t totLayerBufLen = hdr.layerCount * sizeof(TextureAnimLayer_t);
	const size_t totSlotBufLen = hdr.slotCount * sizeof(uint8_t);

	char* const buf = new char[totLayerBufLen + totSlotBufLen];
	bio.Read(buf, totLayerBufLen + totSlotBufLen);

	layerCount = hdr.layerCount;
	slotCount = hdr.slotCount;

	return buf;
}

static void TextureAnim_InternalAddTextureAnim(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
	const std::string txanPath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, "txan");

	unsigned int layerCount, slotCount;
	char* const cpuBuf = TextureAnim_ParseFromTXAN(txanPath.c_str(), layerCount, slotCount);

	PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);

	PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(TextureAnimAssetHeader_t), SF_HEAD | SF_CLIENT, 8);
	TextureAnimAssetHeader_t* const pHdr = reinterpret_cast<TextureAnimAssetHeader_t*>(hdrLump.data);

	const size_t layerBufLen = layerCount * sizeof(TextureAnimLayer_t);
	const size_t totBufLen = layerBufLen + slotCount; // unaligned!

	PakPageLump_s cpuLump = pak->CreatePageLump(totBufLen, SF_CPU | SF_CLIENT, 4, cpuBuf);

	pak->AddPointer(hdrLump, offsetof(TextureAnimAssetHeader_t, layers), cpuLump, 0);
	pak->AddPointer(hdrLump, offsetof(TextureAnimAssetHeader_t, slots), cpuLump, layerBufLen);

	pHdr->layerCount = layerCount;

	asset.InitAsset(hdrLump.GetPointer(), sizeof(TextureAnimAssetHeader_t), PagePtr_t::NullPtr(), TXAN_VERSION, AssetType::TXAN);
	asset.SetHeaderPointer(hdrLump.data);

	pak->FinishAsset();
}

bool TextureAnim_AutoAddTextureAnim(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
	PakAsset_t* const existingAsset = pak->GetAssetByGuid(assetGuid, nullptr, true);

	if (existingAsset)
		return false; // already present in the pak.

	Log("Auto-adding 'txan' asset \"%s\".\n", assetPath);
	TextureAnim_InternalAddTextureAnim(pak, assetGuid, assetPath);

	return true;
}

void Assets::AddTextureAnimAsset_v1(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& /*mapEntry*/)
{
	TextureAnim_InternalAddTextureAnim(pak, assetGuid, assetPath);
}
