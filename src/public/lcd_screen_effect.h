#pragma once

struct LcdScreenEffect_s
{
	float pixelScaleX1;
	float pixelScaleX2;
	float pixelScaleY;
	float brightness;
	float contrast;
	float waveScale;
	float waveOffset;
	float waveSpeed;
	float wavePeriod;
	float bloomAdd;
	uint32_t reserved; // [amos]: always 0 and appears to do nothing in the runtime.
	float pixelFlicker;
};
