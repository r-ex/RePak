#include "pch.h"
#include "zstdutils.h"

ZSTDEncoder_s::ZSTDEncoder_s()
{
	cctx.customMem = ZSTD_defaultCMem;
	ZSTD_initCCtx(&cctx);
}

ZSTDEncoder_s::~ZSTDEncoder_s()
{
	ZSTD_freeCCtxContent(&cctx);
}

ZSTDDecoder_s::ZSTDDecoder_s()
{
	dctx.customMem = ZSTD_defaultCMem;
	ZSTD_initDCtx(&dctx);
}

ZSTDDecoder_s::~ZSTDDecoder_s()
{
	ZSTD_freeDCtxContent(&dctx);
}
