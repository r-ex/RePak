#pragma once

template <typename T>
inline bool IsPowerOfTwo(T x)
{
	return (x & (x - 1)) == 0;
}

template <typename T>
inline T NextPowerOfTwo(T x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}
