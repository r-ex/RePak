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

	void Set(float r, float g, float b, float a)
	{
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
	}

	float r, g, b, a;
};
