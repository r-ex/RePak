#pragma once

struct ColorB
{
	ColorB()
	{
		this->Set(0, 0, 0, 0);
	};

	ColorB(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
	{
		this->Set(r, g, b, a);
	}

	void Set(uint8_t newR, uint8_t newG, uint8_t newB, uint8_t newA)
	{
		this->r = newR;
		this->g = newG;
		this->b = newB;
		this->a = newA;
	}

	uint8_t r, g, b, a;
};

struct ColorF
{
	ColorF()
	{
		this->Set(0.0f, 0.0f, 0.0f, 0.0f);
	};

	ColorF(float r, float g, float b, float a)
	{
		this->Set(r, g, b, a);
	}

	void Set(float newR, float newG, float newB, float newA)
	{
		this->r = newR;
		this->g = newG;
		this->b = newB;
		this->a = newA;
	}

	float r, g, b, a;
};
