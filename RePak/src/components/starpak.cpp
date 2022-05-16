#include "pch.h"
#include "RePak.h"

static uint64_t nextStarpakOffset = 0x1000;

// purpose: add new starpak file path to be used by the rpak
// returns: void
void RePak::AddStarpakReference(std::string path)
{
    for (auto& it : g_vsStarpakPaths)
    {
        if (it == path)
            return;
    }
    g_vsStarpakPaths.push_back(path);
}

// purpose: add data entry to be written to the starpak
// returns: offet to data entry in starpak
uint64_t RePak::AddStarpakDataEntry(SRPkDataEntry block)
{
    size_t ns = Utils::PadBuffer((char**)&block.dataPtr, block.dataSize, 4096);

    block.dataSize = ns;
    block.offset = nextStarpakOffset;

    g_vSRPkDataEntries.push_back(block);

    nextStarpakOffset += block.dataSize;

    return block.offset;
}