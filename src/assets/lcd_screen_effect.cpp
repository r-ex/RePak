#include "pch.h"
#include "assets.h"
#include "public/lcd_screen_effect.h"

static void LcdScreenEffect_InternalAddRLCD(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
	const std::string rlcdPath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, "json");
	rapidjson::Document document;

	if (!JSON_ParseFromFile(rlcdPath.c_str(), "lcd screen effect", document, false))
		Error("Failed to open lcd_screen_effect asset \"%s\".\n", rlcdPath.c_str());

	PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);
	PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(LcdScreenEffect_s), SF_HEAD | SF_CLIENT, 4);

	LcdScreenEffect_s* const rlcd = reinterpret_cast<LcdScreenEffect_s*>(hdrLump.data);

	// Interlacing effects.
	rlcd->pixelScaleX1 = JSON_GetNumberRequired<float>(document, "pixelScaleX1");
	rlcd->pixelScaleX2 = JSON_GetNumberRequired<float>(document, "pixelScaleX2");
	rlcd->pixelScaleY = JSON_GetNumberRequired<float>(document, "pixelScaleY");

	// Image effects.
	rlcd->brightness = JSON_GetNumberRequired<float>(document, "brightness");
	rlcd->contrast = JSON_GetNumberRequired<float>(document, "contrast");

	// Shutter banding effects.
	rlcd->waveOffset = JSON_GetNumberRequired<float>(document, "waveOffset");
	rlcd->waveScale = JSON_GetNumberRequired<float>(document, "waveScale");
	rlcd->waveSpeed = JSON_GetNumberRequired<float>(document, "waveSpeed");
	rlcd->wavePeriod = JSON_GetNumberRequired<float>(document, "wavePeriod");

	// Noise effects.
	rlcd->bloomAdd = JSON_GetNumberRequired<float>(document, "bloomAdd");
	rlcd->reserved = JSON_GetNumberOrDefault(document, "reserved", 0u);
	rlcd->pixelFlicker = JSON_GetNumberRequired<float>(document, "pixelFlicker");

	asset.InitAsset(hdrLump.GetPointer(), sizeof(LcdScreenEffect_s), PagePtr_t::NullPtr(), RLCD_VERSION, AssetType::RLCD);
	asset.SetHeaderPointer(hdrLump.data);

	pak->FinishAsset();
}

void Assets::AddLcdScreenEffect_v0(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& /*mapEntry*/)
{
	LcdScreenEffect_InternalAddRLCD(pak, assetGuid, assetPath);
}
