#pragma once

template <typename T>
inline bool IsPowerOfTwo(T x)
{
	return (x & (x - 1)) == 0;
}

