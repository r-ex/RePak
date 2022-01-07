#pragma once

namespace RePak
{
	uint32_t CreateNewSegment(uint64_t size, SegmentType type, RPakVirtualSegment& seg);
	void AddRawDataBlock(RPakRawDataBlock block);
};