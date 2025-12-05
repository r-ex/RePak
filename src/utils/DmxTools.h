#pragma once
#include "binaryio.h"
#include "public/dmexchange.h"

extern bool Dmx_ParseHdr(BinaryIO& bio, DmxHeader_s& hdr);
extern bool Dmx_DeserializeBinary(DmContext_s& ctx, BinaryIO& bio);
