#pragma once

struct Color
{
	Color()
	{
		this->Set(0.0f, 0.0f, 0.0f, 0.0f);
	};

	Color(float r, float g, float b, float a)
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
