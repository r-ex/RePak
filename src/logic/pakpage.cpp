//=============================================================================//
//
// Pak page builder and management class
//
//=============================================================================//
#include "pch.h"
#include "pakpage.h"

//-----------------------------------------------------------------------------
// Constructors/Destructors
//-----------------------------------------------------------------------------
CPakPageBuilder::CPakPageBuilder()
{
}
CPakPageBuilder::~CPakPageBuilder()
{
	for (PakPage_s& page : m_pages)
	{
		for (PakPageLump_s& chunk : page.lumps)
		{
			chunk.Release();
		}
	}
}

//-----------------------------------------------------------------------------
// Find the first slab that matches the requested flags, with an alignment that
// is also as close as possible to requested. If no slabs can be found with
// requested flags, a new one will be created.
//-----------------------------------------------------------------------------
PakSlab_s& CPakPageBuilder::FindOrCreateSlab(const int flags, const int align, const int size)
{
	// Caller must provide the aligned size.
	assert((IALIGN(size, align) - size) == 0);

	PakSlab_s* toReturn = nullptr;
	int lastAlignDiff = INT32_MAX;

	// Try and find a segment that has the same flags, and the closest alignment
	// of request.
	for (size_t i = 0; i < m_slabs.size(); i++)
	{
		PakSlab_s& slab = m_slabs[i];
		PakSlabHdr_s& header = slab.header;

		if (header.flags != flags)
			continue;

		if (header.alignment != align)
		{
			const int alignDiff = abs(header.alignment - align);

			if (alignDiff < lastAlignDiff)
			{
				lastAlignDiff = alignDiff;
				toReturn = &slab;
			}

			continue;
		}

		toReturn = &slab;
		break;
	}

	if (toReturn)
	{
		PakSlabHdr_s& header = toReturn->header;

		// If the segment's alignment is less than our requested alignment, we
		// can increase it as increasing the alignment will still allow the
		// previous data to be aligned to the same boundary (all alignments are
		// powers of two).
		if (header.alignment < align)
			header.alignment = align;

		header.dataSize += size;
		return *toReturn;
	}

	// If we didn't have a close match, create a new segment.
	PakSlab_s& newSegment = m_slabs.emplace_back();

	newSegment.index = static_cast<int>(m_slabs.size() - 1);
	newSegment.header.flags = flags;
	newSegment.header.alignment = align;
	newSegment.header.dataSize = size;

	return newSegment;
}

//-----------------------------------------------------------------------------
// Find the first page that matches the requested flags, with an alignment that
// is also as close as possible to requested and check if there is room for new
// data to be added. If no pages can be found with requested flags and room, a
// new one will be created.
//-----------------------------------------------------------------------------
PakPage_s& CPakPageBuilder::FindOrCreatePage(const int flags, const int align, const int size)
{
	// Caller must provide the aligned size.
	assert((IALIGN(size, align) - size) == 0);

	PakSlab_s& slab = FindOrCreateSlab(flags, align, size);

	PakPage_s* toReturn = nullptr;
	int lastAlignDiff = INT32_MAX;

	for (size_t i = 0; i < m_pages.size(); i++)
	{
		PakPage_s& page = m_pages[i];

		if (page.flags != flags)
			continue;

		PakPageHdr_s& header = page.header;

		// Note: check on aligned page size, because the page data size is not
		// necessarily aligned to its own alignment when we are still building
		// pages, the alignment and padding happens after all pages have been
		// built. The data should remain below PAK_MAX_PAGE_MERGE_SIZE when it
		// has been padded out, else a new page should be created.
		if (IALIGN(header.dataSize, max(header.alignment, align)) + size > PAK_MAX_PAGE_MERGE_SIZE)
			continue;

		if (header.alignment != align)
		{
			const int alignDiff = abs(header.alignment - align);

			if (alignDiff < lastAlignDiff)
			{
				lastAlignDiff = alignDiff;
				toReturn = &page;
			}

			continue;
		}

		toReturn = &page;
		break;
	}

	// Same principle as FindOrCreateSlab.
	if (toReturn)
	{
		PakPageHdr_s& header = toReturn->header;

		if (header.alignment < align)
			header.alignment = align;

		header.dataSize += size;
		return *toReturn;
	}

	PakPage_s& page = m_pages.emplace_back();

	page.index = static_cast<int>(m_pages.size() - 1);
	page.flags = flags;
	page.header.slabIndex = slab.index;
	page.header.alignment = align;
	page.header.dataSize = size;

	return page;
}

//-----------------------------------------------------------------------------
// Create a page lump, which is a piece of data that will be placed inside the
// page with user requested alignment.
//-----------------------------------------------------------------------------
const PakPageLump_s CPakPageBuilder::CreatePageLump(const int size, const int flags, const int align, void* const buf)
{
	// this assert is replicated in r5sdk
	assert(align != 0 && align < UINT8_MAX);
	assert(IsPowerOfTwo(align));

	const int alignedSize = IALIGN(size, align);
	PakPage_s& page = FindOrCreatePage(flags, align, alignedSize);

	const int pagePadAmount = IALIGN(page.header.dataSize, align) - page.header.dataSize;

	// If the requested alignment requires padding the previous asset to align
	// this one, a null-lump should be created. These are handled specially in
	// WritePageData.
	if (pagePadAmount > 0)
	{
		PakPageLump_s& pad = page.lumps.emplace_back();

		pad.data = nullptr;
		pad.size = pagePadAmount;
		pad.alignment = align;
		pad.pageInfo = PagePtr_t::NullPtr();

		// Grow the slab and page size to accommodate the page align padding.
		page.header.dataSize += pagePadAmount;
		m_slabs[page.header.slabIndex].header.dataSize += pagePadAmount;
	}

	const int lumpPadAmount = alignedSize - size;

	// If the lump is smaller than its size with requested alignment, we should
	// pad the remainder out. Unlike the page padding above, we shouldn't grow
	// the slab and page sizes because the aligned size was already provided to
	// FindOrCreatePage. That function expects the full aligned size because it
	// has to check if it fits to be merged in a page with matching flags.
	if (lumpPadAmount > 0)
	{
		PakPageLump_s& pad = page.lumps.emplace_back();

		pad.data = nullptr;
		pad.size = lumpPadAmount;
		pad.alignment = align;
		pad.pageInfo = PagePtr_t::NullPtr();
	}

	char* targetBuf;

	// Note: we don't have to allocate the buffer with the aligned size since
	// these buffers are individual and are padded out with null-lumps when
	// writing out the pages, so we could save on memory here.
	if (!buf)
	{
		targetBuf = new char[size];
		memset(targetBuf, 0, size);
	}
	else
		targetBuf = reinterpret_cast<char*>(buf);

	PakPageLump_s& lump = page.lumps.emplace_back();

	lump.data = targetBuf;
	lump.size = size;
	lump.alignment = page.header.alignment;

	lump.pageInfo.index = page.index;
	lump.pageInfo.offset = page.header.dataSize - size;

	assert(lump.pageInfo.offset >= 0);
	return lump;
}

//-----------------------------------------------------------------------------
// During build time, the slabs and pages are not necessarily aligned to their
// own alignments as an asset could request its own alignment, i.e., we have a
// page with an alignment of 64 but an asset could still be aligned to 8 within
// this page. Its possible we add several assets with align 8 into a 64 aligned
// page, and we then only have to pad each asset out to an 8 bytes boundary to
// align them rather than doing the full 64. The slabs and pages must however
// be padded out to match their alignments after we are done building the pages
// but before we start writing out to the stream. This optimization saves a lot
// of data for paks with a large amount of assets merged into single pages.
//-----------------------------------------------------------------------------
void CPakPageBuilder::PadSlabsAndPages()
{
	for (PakPage_s& page : m_pages)
	{
		PakPageHdr_s& pageHdr = page.header;
		PakSlab_s& slab = m_slabs[pageHdr.slabIndex];
		PakSlabHdr_s& slabHdr = slab.header;

		const int pagePadAmount = IALIGN(pageHdr.dataSize, pageHdr.alignment) - pageHdr.dataSize;

		if (pagePadAmount > 0)
		{
			PakPageLump_s& pad = page.lumps.emplace_back();

			pad.data = nullptr;
			pad.size = pagePadAmount;
			pad.alignment = pageHdr.alignment;
			pad.pageInfo = PagePtr_t::NullPtr();

			// grow the slab and page size to accommodate alignment padding
			pageHdr.dataSize += pagePadAmount;
			slabHdr.dataSize += pagePadAmount;
		}

		// Note: we aligned the page above, and since we grew its size, we have
		// to grow the slab size too. Growing the page and slab sizes together
		// does not necessarily mean they are both synced and aligned to their
		// respective alignments as slabs with an alignment of 1, can be turned
		// into a slab with an alignment of 8, and a page of alignment of 1 can
		// still use this slab without being padded to 8, as an alignment of 8
		// will still align a page to anything below in the powers of 2. The
		// pages with lower alignment than the slabs them selfs will cause the
		// desync if we do not realign the slab to its own alignment below.
		const size_t slabPadAmount = IALIGN(slabHdr.dataSize, slabHdr.alignment) - slabHdr.dataSize;

		if (slabPadAmount > 0)
			slabHdr.dataSize += slabPadAmount;
	}
}

//-----------------------------------------------------------------------------
// Write out the slab headers in the order they were created
//-----------------------------------------------------------------------------
void CPakPageBuilder::WriteSlabHeaders(BinaryIO& out) const
{
	for (const PakSlab_s& slab : m_slabs)
	{
		out.Write(slab.header);
	}
}

//-----------------------------------------------------------------------------
// Write out the page headers in the order they were created
//-----------------------------------------------------------------------------
void CPakPageBuilder::WritePageHeaders(BinaryIO& out) const
{
	for (const PakPage_s& page : m_pages)
	{
		out.Write(page.header);
	}
}

//-----------------------------------------------------------------------------
// Write out the paged data
//-----------------------------------------------------------------------------
void CPakPageBuilder::WritePageData(BinaryIO& out) const
{
	for (const PakPage_s& page : m_pages)
	{
		for (const PakPageLump_s& lump : page.lumps)
		{
			if (lump.data)
				out.Write(lump.data, lump.size);
			else // Lump is padding to align the next asset or our current page.
				out.SeekPut(lump.size, std::ios::cur);
		}
	}
}
