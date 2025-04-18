#pragma once
#include "thirdparty/zstd/zstd.h"
#include "thirdparty/zstd/compress/zstd_compress_internal.h"
#include "thirdparty/zstd/decompress/zstd_decompress_internal.h"

struct ZSTDEncoder_s
{
	ZSTDEncoder_s();
	~ZSTDEncoder_s();

	ZSTD_CCtx cctx;
};

struct ZSTDDecoder_s
{
	ZSTDDecoder_s();
	~ZSTDDecoder_s();

	ZSTD_DCtx dctx;
};
