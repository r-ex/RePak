#include "pch.h"
#include "assets.h"
#include "particle_operators_params.h"
#include "public/particle_effect.h"
#include "utils/DmxTools.h"

#define PARTICLE_DEFINITION_ENC "binary"
#define PARTICLE_DEFINITION_ENC_VER 5
#define PARTICLE_DEFINITION_FMT "pcf"
#define PARTICLE_DEFINITION_FMT_VER 2

static void ParticleEffect_ValidateHeader(const DmxHeader_s& hdr)
{
	if (hdr.formatVersion != PARTICLE_DEFINITION_FMT_VER)
		Error("Particle effect is version %d, but %d was expected!", hdr.formatVersion, PARTICLE_DEFINITION_FMT_VER);
}

/*static*/ void ParticleEffect_BakeElement(DmSymbolTable& /*symbolTable*/, DmElement_s& /*elem*/)
{
	//inheritEntityScale = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "Inherit entity scale", true);
	//ParticleEffect_BakeParams();
}

struct ParticlePageMemory_s
{
	size_t stringPoolStart;
	size_t stringDictStart;
	size_t elementsStart;
};

/*static*/ void ParticleEffect_CreatePageLayoutAndAlloc(const DmContext_s& ctx, ParticlePageMemory_s& outMem, const char* const particleName)
{
	const size_t nameBufLen = strlen(particleName) +1;

	outMem.stringPoolStart = sizeof(EffectAssetData_s) + nameBufLen;
	outMem.stringDictStart = IALIGN8(outMem.stringPoolStart + ctx.symbolTable.StringBytesRetained());
	outMem.elementsStart = outMem.stringDictStart + ctx.symbolTable.NumStringsRetained() * sizeof(PagePtr_t);
}

static void ParticleEffect_ProcessSymbolTable(DmContext_s& ctx)
{
	const DmSymbolTable::SymbolId_t target = ctx.symbolTable.Find("DmeParticleOperator");

	if (target == DmSymbolTable::npos)
		return; // Nothing to process, no string dictionary to build.

	// note(kawe): since baked particle effects must store and expose all fields
	//             including default fields, we don't need to store the field
	//             names inside the baked dictionary. Mark everything that we
	//             plan to discard later so the symbol table is aware and knows
	//             the total pool size we need for the page buffer.
	for (const DmElement_s& elem : ctx.elementList)
	{
		if (elem.type.s != target)
			continue;

		ctx.symbolTable.MarkAsDiscarded(elem.name.s);

		for (const DmAttribute_s& at : elem.attr)
		{
			if (at.type == AT_STRING)
				ctx.symbolTable.MarkAsDiscarded(at.value.stringSym.s);
		}
	}
}

static void ParticleEffect_InternalBake(CPakFileBuilder* const pak, /*PakAsset_t& asset,*/ const PakGuid_t /*assetGuid*/, const char* const assetPath)
{
	BinaryIO input;

	if (!input.Open(pak->GetAssetPath() + assetPath, BinaryIO::Mode_e::Read))
		Error("Failed to open particle effect asset \"%s\".\n", assetPath);

	DmxHeader_s header;

	if (!Dmx_ParseHdr(input, header))
		return;

	ParticleEffect_ValidateHeader(header);
	DmContext_s ctx;
	
	if (!Dmx_DeserializeBinary(ctx, input))
		return;

	ParticleEffect_ProcessSymbolTable(ctx);

	std::vector<ParticleDefintionParams_s> list;
	for (auto& elem : ctx.elementList)
	{
		ParticleDefintionParams_s& params = list.emplace_back();
		ParticleEffect_BakeParams(ctx.symbolTable, elem, params);
	}
}

void Assets::AddParticleEffect_v2(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& /*mapEntry*/)
{
	ParticleEffect_InternalBake(pak, assetGuid, assetPath);
}
