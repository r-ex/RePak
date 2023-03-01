#pragma once

struct Vector3
{
	Vector3()
	{
		this->Set(0.0f, 0.0f, 0.0f);
	}

	Vector3(float x, float y, float z)
	{
		this->Set(x, y, z);
	}

	void Set(float x, float y, float z)
	{
		this->x = x;
		this->y = y;
		this->z = z;
	}

	float x, y, z;
};
