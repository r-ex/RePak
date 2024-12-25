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
PakSlab_s& CPakPageBuilder::FindOrCreateSlab(const int flags, const int align)
{
	PakSlab_s* toReturn = nullptr;
	int lastAlignDiff = INT32_MAX;

	// Try and find a slab that has the same flags, and the closest alignment
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

		// If the slab's alignment is less than our requested alignment, we
		// can increase it as increasing the alignment will still allow the
		// previous data to be aligned to the same boundary (all alignments
		// are powers of two).
		if (header.alignment < align)
			header.alignment = align;

		return *toReturn;
	}

	// If we didn't have a close match, create a new slab.
	PakSlab_s& newSlab = m_slabs.emplace_back();

	newSlab.index = static_cast<int>(m_slabs.size() - 1);
	newSlab.header.flags = flags;
	newSlab.header.alignment = align;
	newSlab.header.dataSize = 0;

	return newSlab;
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

	PakSlab_s& slab = FindOrCreateSlab(flags, align);

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

		return *toReturn;
	}

	PakPage_s& newPage = m_pages.emplace_back();

	newPage.index = static_cast<int>(m_pages.size() - 1);
	newPage.flags = flags;
	newPage.header.slabIndex = slab.index;
	newPage.header.alignment = align;
	newPage.header.dataSize = 0;

	return newPage;
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

	const int sizeAligned = IALIGN(size, align);

	PakPage_s& page = FindOrCreatePage(flags, align, sizeAligned);
	PakSlab_s& slab = m_slabs[page.header.slabIndex];

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
		slab.header.dataSize += pagePadAmount;
	}

	page.header.dataSize += sizeAligned;
	slab.header.dataSize += sizeAligned;

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

	const int lumpPadAmount = sizeAligned - size;

	// Reserve for 2 because we need to add a padding lump afterwards to pad the
	// data lump out to its alignment boundary, this avoids reallocation.
	if (lumpPadAmount > 0)
		page.lumps.reserve(page.lumps.size() + 2);

	PakPageLump_s& lump = page.lumps.emplace_back();

	lump.data = targetBuf;
	lump.size = size;
	lump.alignment = page.header.alignment;

	lump.pageInfo.index = page.index;
	lump.pageInfo.offset = page.header.dataSize - sizeAligned;

	assert(lump.pageInfo.offset >= 0);

	// If the lump is smaller than its size with requested alignment, we should
	// pad the remainder out. Unlike the page padding above, we shouldn't grow
	// the slab and page sizes because the aligned size was already added.
	if (lumpPadAmount > 0)
	{
		PakPageLump_s& pad = page.lumps.emplace_back();

		pad.data = nullptr;
		pad.size = lumpPadAmount;
		pad.alignment = align;
		pad.pageInfo = PagePtr_t::NullPtr();
	}

	return lump;
}

//-----------------------------------------------------------------------------
// There are 2 important things we have to take into account here:
// 
// - if a page inside a slab has a higher alignment than the previous page, the
//   previous page needs to be padded out in the runtime to align that page.
// - pages are not padded out to their alignment boundaries in the pak file,
//   this is done by the runtime when loading the pages.
// 
// The extra sizes required by this is not added to the slab when we are
// building the pages as their alignments can change when a lump requests a
// higher alignment.
//-----------------------------------------------------------------------------
void CPakPageBuilder::PadSlabsAndPages()
{
	int lastPageSizeAligned = 0;
	int lastPageAlign = 0;

	for (PakPage_s& page : m_pages)
	{
		PakPageHdr_s& pageHdr = page.header;
		PakSlab_s& slab = m_slabs[pageHdr.slabIndex];
		PakSlabHdr_s& slabHdr = slab.header;

		const int pageSize = pageHdr.dataSize;
		const int pageAlign = pageHdr.alignment;

		// The runtime pads the previous page to align our current page, we have
		// to add this extra size to the slab to accommodate for this.
		if (lastPageSizeAligned > 0 && pageAlign > lastPageAlign)
			slabHdr.dataSize += IALIGN(lastPageSizeAligned, pageAlign) - lastPageSizeAligned;

		const int pagePadAmount = IALIGN(pageSize, pageAlign) - pageSize;

		// The runtime allocates the page to its alignment boundary, which means
		// it will pad out the remainder if a page inside the pak file isn't as
		// large as it will be when allocated to its required boundary. We have
		// to add this extra size to the slab to accommodate for this.
		if (pagePadAmount > 0)
			slabHdr.dataSize += pagePadAmount;

		lastPageSizeAligned = pageSize + pagePadAmount;
		lastPageAlign = pageAlign;
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
			else
			{
				// Lump is padding to either:
				// - pad out the previous asset to align our current asset.
				// - pad out the current asset to its full aligned size.
				// - pad out the page to its full aligned size.
				out.SeekPut(lump.size, std::ios::cur);
			}
		}
	}
}
