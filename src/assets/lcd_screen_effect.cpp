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

	rlcd->interlaceX = JSON_GetNumberRequired<float>(document, "interlaceX");
	rlcd->interlaceY = JSON_GetNumberRequired<float>(document, "interlaceY");
	rlcd->attenuation = JSON_GetNumberRequired<float>(document, "attenuation");
	rlcd->contrast = JSON_GetNumberRequired<float>(document, "contrast");
	rlcd->gamma = JSON_GetNumberRequired<float>(document, "gamma");
	rlcd->washout = JSON_GetNumberRequired<float>(document, "washout");
	rlcd->shutterBandingIntensity = JSON_GetNumberRequired<float>(document, "shutterBandingIntensity");
	rlcd->shutterBandingFrequency = JSON_GetNumberRequired<float>(document, "shutterBandingFrequency");
	rlcd->shutterBandingSpacing = JSON_GetNumberRequired<float>(document, "shutterBandingSpacing");
	rlcd->exposure = JSON_GetNumberRequired<float>(document, "exposure");
	rlcd->reserved = JSON_GetNumberOrDefault(document, "reserved", 0);
	rlcd->noiseAmount = JSON_GetNumberRequired<float>(document, "noiseAmount");

	asset.InitAsset(hdrLump.GetPointer(), sizeof(LcdScreenEffect_s), PagePtr_t::NullPtr(), RLCD_VERSION, AssetType::RLCD);
	asset.SetHeaderPointer(hdrLump.data);

	pak->FinishAsset();
}

void Assets::AddLcdScreenEffect_v0(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& /*mapEntry*/)
{
	LcdScreenEffect_InternalAddRLCD(pak, assetGuid, assetPath);
}
