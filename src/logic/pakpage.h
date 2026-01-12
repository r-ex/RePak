#pragma once
#include "public/rpak.h"

#define PAK_MAX_SLAB_COUNT 20

// Pages can only be merged with other pages with equal flags and an alignment
// equal or higher than its own if the combined size aligned to the page's
// new alignment is below this value.
#define PAK_MAX_PAGE_MERGE_SIZE 0xffff

// A piece of data that belongs to the pak page, if PakPageLump_s::data is null
// the lump will be treated as alignment padding.
struct PakPageLump_s
{
	void Release()
	{
		if (data)
		{
			delete[] data;
			data = nullptr;
		}
	}

	// Gets the pointer to the page buffer at offset.
	inline PagePtr_t GetPointer(size_t offset = 0) const { return { pageInfo.index, static_cast<int>(pageInfo.offset + offset) }; };

	char* data;
	int size;
	int alignment;

	PagePtr_t pageInfo;
};

// A page of data, all lumps are aligned to the lump's alignment. The
// alignment of the page is equal to the page's lump with the highest
// alignment.
struct PakPage_s
{
	bool operator<(const PakPage_s& a) const
	{
		return header.slabIndex < a.header.slabIndex;
	}

	int index;
	int flags;
	PakPageHdr_s header;

	// ordered list of data chunks belonging to this page.
	std::vector<PakPageLump_s> lumps;
};

// A large piece of memory in which all pages matching the alignment and flags
// of the slab reside. The alignment of the slab is equal to the slab's page
// with the highest alignment.
struct PakSlab_s
{
	int index;
	PakSlabHdr_s header;
};

class CPakPageBuilder
{
public:
	CPakPageBuilder();
	~CPakPageBuilder();

	inline uint16_t GetSlabCount() const { return m_slabCount; }
	inline uint16_t GetPageCount() const { return static_cast<uint16_t>(m_pages.size()); }

	const PakPageLump_s CreatePageLump(const int size, const int flags, const int align, void* const buf = nullptr);

	void PadSlabsAndPages();

	void WriteSlabHeaders(BinaryIO& out) const;
	void WritePageHeaders(BinaryIO& out) const;
	void WritePageData(BinaryIO& out) const;

private:
	PakSlab_s& FindOrCreateSlab(const int flags, const int align);
	PakPage_s& FindOrCreatePage(const int flags, const int align, const int size);

private:
	std::array<PakSlab_s, PAK_MAX_SLAB_COUNT> m_slabs;
	uint16_t m_slabCount;

	std::vector<PakPage_s> m_pages;
};
