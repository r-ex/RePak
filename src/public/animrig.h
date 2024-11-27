#pragma once
#include "public/rpak.h"

struct AnimRigAssetHeader_t
{
	PagePtr_t data;
	PagePtr_t name;
	char gap_10[4];
	int sequenceCount;
	PagePtr_t pSequences;
	char gap_20[8];
};

static_assert(sizeof(AnimRigAssetHeader_t) == 40);