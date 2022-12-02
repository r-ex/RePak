#pragma once

#include <pch.h>

#pragma pack(push, 2)
struct UIImageHeader
{
	float WidthRatio = 0.0;
	float HeightRatio = 0.0;
	uint16_t Width = 1;
	uint16_t Height = 1;
	uint16_t TextureOffsetsCount = 0;
	uint16_t TextureCount = 0;
	RPakPtr pTextureOffsets{};
	RPakPtr pTextureDimensions{};
	RPakPtr pUnk{};
	RPakPtr pTextureHashes{};
	RPakPtr pTextureNames{};
	uint64_t TextureGuid = 0;
};

struct UIImageUV
{
	inline void InitUIImageUV(Vector2 start, Vector2 resolution)
	{
		this->uv0 = start;
		this->uv1 = resolution;
	}
	// maybe the uv coords for top left?
	// just leave these as 0 and it should be fine
	Vector2 uv0 = { 0.0f , 0.0f };

	// these two seem to be the uv coords for the bottom right corner
	// examples:
	// uv1x = 10;
	// | | | | | | | | | |
	// uv1x = 5;
	// | | | | |
	Vector2 uv1 = { 1.0f , 1.0f };
};

// examples of changes from these values: https://imgur.com/a/l1YDXaz
struct UIImageOffset
{
	inline void InitUIImageOffset(Vector2 start, Vector2 end)
	{
		this->start = start;
		this->end = end;
		this->tile = { 1.0f - 2.0f * -start.x ,  1.0f - 2.0f * -start.y }; // doesnt seem to always 100% of the time match up but its very close

	}
	// these don't seem to matter all that much as long as they are a valid float number
	Vector2 aspect_ratio = { 0.0f , 0.0f };

	// endX and endY define where the edge of the image is, with 1.f being the full length of the image and 0.5f being half of the image
	Vector2 end = { 1.0f , 1.0f };

	// startX and startY define where the top left corner is in proportion to the full image dimensions
	Vector2 start = { 0.0f , 0.0f };

	// changing these 2 values causes the image to be distorted on each axis
	Vector2 tile = { 1.0f , 1.0f };
};
#pragma pack(pop)
