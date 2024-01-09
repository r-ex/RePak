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

	void Set(float newX, float newY, float newZ)
	{
		this->x = newX;
		this->y = newY;
		this->z = newZ;
	}

	float x, y, z;
};
