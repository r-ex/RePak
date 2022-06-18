#include "pch.h"
#include "RePak.h"
#include <Assets.h>

static uint64_t nextStarpakOffset = 0x1000;

// purpose: add new starpak file path to be used by the rpak
// returns: void
void RePak::AddStarpakReference(std::string path)
{
    for (auto& it : Assets::g_vsStarpakPaths)
    {
        if (it == path)
            return;
    }
    Assets::g_vsStarpakPaths.push_back(path);
}

// purpose: add data entry to be written to the starpak
// returns: offet to data entry in starpak
uint64_t RePak::AddStarpakDataEntry(SRPkDataEntry block)
{
    size_t ns = Utils::PadBuffer((char**)&block.m_nDataPtr, block.m_nDataSize, 4096);

    block.m_nDataSize = ns;
    block.m_nOffset = nextStarpakOffset;

    Assets::g_vSRPkDataEntries.push_back(block);

    nextStarpakOffset += block.m_nDataSize;

    return block.m_nOffset;
}