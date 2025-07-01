#pragma once

struct LcdScreenEffect_s
{
	float interlaceX;
	float interlaceY;
	float attenuation;
	float contrast;
	float gamma;
	float washout;
	float shutterBandingIntensity;
	float shutterBandingFrequency;
	float shutterBandingSpacing;
	float exposure;
	int reserved; // [amos]: always 0 and appears to do nothing in the runtime.
	float noiseAmount;
};
