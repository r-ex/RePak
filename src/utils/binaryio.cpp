#include "pch.h"
#include "binaryio.h"
#include <sys/stat.h>

//-----------------------------------------------------------------------------
// constructors/destructors
//-----------------------------------------------------------------------------
BinaryIO::BinaryIO()
{
	Reset();
}
BinaryIO::~BinaryIO()
{
	Close();
}

//-----------------------------------------------------------------------------
// Get internal stream mode from selected mode
//-----------------------------------------------------------------------------
static std::ios_base::openmode GetInternalStreamMode(const BinaryIO::Mode_e mode)
{
	switch (mode)
	{
	case BinaryIO::Mode_e::Read:
		return (std::ios::in | std::ios::binary);
	case BinaryIO::Mode_e::Write:
		return (std::ios::out | std::ios::binary);
	case BinaryIO::Mode_e::ReadWrite:
		return (std::ios::in | std::ios::out | std::ios::binary);
	case BinaryIO::Mode_e::ReadWriteCreate:
		return (std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
	}

	assert(0); // code bug, can never reach this.
	return 0;
}

//-----------------------------------------------------------------------------
// Opens the file in specified mode
//-----------------------------------------------------------------------------
bool BinaryIO::Open(const char* const filePath, const Mode_e mode)
{
	m_flags = GetInternalStreamMode(mode);
	m_mode = mode;

	if (m_stream.is_open())
	{
		m_stream.close();
	}

	m_stream.open(filePath, m_flags);

	if (!m_stream.is_open() || !m_stream.good())
	{
		return false;
	}

	if (IsReadMode())
	{
		// Calculate the initial size.
		m_stream.seekg(0, std::ios::end);
		m_state.totalSize = m_stream.tellg();
		m_stream.seekg(0, std::ios::beg);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Resets the internal state
//-----------------------------------------------------------------------------
void BinaryIO::Reset()
{
	m_state.totalSize = 0;
	m_state.skipCount = 0;
	m_state.seekGap = 0;
	m_mode = Mode_e::None;
	m_flags = 0;
}

//-----------------------------------------------------------------------------
// Closes the stream
//-----------------------------------------------------------------------------
void BinaryIO::Close()
{
	m_stream.close();
	Reset();
}

//-----------------------------------------------------------------------------
// Flushes the ofstream
//-----------------------------------------------------------------------------
void BinaryIO::Flush()
{
	if (IsWritable())
		m_stream.flush();
}

//-----------------------------------------------------------------------------
// Gets the position of the current character in the stream
//-----------------------------------------------------------------------------
std::streamoff BinaryIO::TellGet()
{
	assert(IsReadMode());
	return m_stream.tellg();
}
std::streamoff BinaryIO::TellPut()
{
	assert(IsWriteMode());
	return m_stream.tellp();
}

//-----------------------------------------------------------------------------
// Sets the position of the current character in the stream
//-----------------------------------------------------------------------------
void BinaryIO::SeekGet(const std::streamoff offset, const std::ios_base::seekdir way)
{
	assert(IsReadMode());
	m_stream.seekg(offset, way);
}
//-----------------------------------------------------------------------------
// NOTE: if you seek beyond the end of the file to try and pad it out, use the
// Pad() method instead as the behavior of seek is operating system dependent
//-----------------------------------------------------------------------------
void BinaryIO::SeekPut(const std::streamoff offset, const std::ios_base::seekdir way)
{
	assert(IsWriteMode());

	UpdateSeekState(offset, way);
	m_stream.seekp(offset, way);
}
void BinaryIO::Seek(const std::streamoff offset, const std::ios_base::seekdir way)
{
	if (IsReadMode())
		SeekGet(offset, way);
	if (IsWriteMode())
		SeekPut(offset, way);
}

//-----------------------------------------------------------------------------
// Returns the stream buffer
//-----------------------------------------------------------------------------
const std::filebuf* BinaryIO::GetData() const
{
	return m_stream.rdbuf();
}

//-----------------------------------------------------------------------------
// Returns the data size
//-----------------------------------------------------------------------------
const std::streamoff BinaryIO::GetSize() const
{
	return m_state.totalSize;
}

bool BinaryIO::IsReadMode() const
{
	return (m_flags & std::ios::in);
}

bool BinaryIO::IsWriteMode() const
{
	return (m_flags & std::ios::out);
}

//-----------------------------------------------------------------------------
// Checks if we are able to read the file
//-----------------------------------------------------------------------------
bool BinaryIO::IsReadable() const
{
	if (!IsReadMode() || !m_stream || m_stream.eof())
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Checks if we are able to write to file
//-----------------------------------------------------------------------------
bool BinaryIO::IsWritable() const
{
	if (!IsWriteMode() || !m_stream)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Checks if we hit the end of file
//-----------------------------------------------------------------------------
bool BinaryIO::IsEof() const
{
	return m_stream.eof();
}

//-----------------------------------------------------------------------------
// Reads a string from the file
//-----------------------------------------------------------------------------
bool BinaryIO::ReadString(std::string& out)
{
	if (!IsReadable())
		return false;

	char c;

	while (m_stream.get(c))
	{
		if (c == '\0')
			return true;

		out += c;
	}

	return false; // EOF or error, result is truncated.
}

//-----------------------------------------------------------------------------
// Reads a string from the file into a fixed size buffer
//-----------------------------------------------------------------------------
bool BinaryIO::ReadString(char* const buf, const size_t len, size_t* const outWriteCount)
{
	assert(buf && len > 0);

	if (!IsReadable())
		return false;

	size_t i = 0;
	char c;
	bool fullRead = false;

	while (i < (len - 1) && m_stream.get(c))
	{
		if (c == '\0')
		{
			buf[i] = '\0';
			fullRead = true;

			break;
		}

		buf[i++] = c;
	}

	if (!fullRead)
		buf[i] = '\0';

	if (outWriteCount)
		*outWriteCount = i;

	return fullRead; // if false, string too long and result is truncated.
}

// limit number of io calls and allocations by just using this static buffer
// for padding out the stream.
static constexpr size_t PAD_BUF_SIZE = 4096;
const static char s_padBuf[PAD_BUF_SIZE];

//-----------------------------------------------------------------------------
// Pads the out stream up to count bytes
//-----------------------------------------------------------------------------
bool BinaryIO::Pad(const size_t count, size_t* const outPadCount)
{
	assert(count > 0);
	size_t remainder = count;

	while (remainder)
	{
		const size_t writeCount = (std::min)(remainder, PAD_BUF_SIZE);
		
		if (!Write(s_padBuf, writeCount))
			break;

		remainder -= writeCount;
	}

	if (outPadCount)
		*outPadCount = count - remainder;

	return remainder == 0;
}

//-----------------------------------------------------------------------------
// If we seek backwards, and then write new data, we should not add
// this to the total output size of the stream as we modify and not
// add. we have to keep by how much we shifted backwards and advanced
// forward until we can start adding again.
//-----------------------------------------------------------------------------
void BinaryIO::UpdateSeekState(const std::streamoff offset, const std::ios_base::seekdir way)
{
	std::streamoff targetPos = 0;

	switch (way) // determine the abs target pos of the seek.
	{
	case std::ios_base::beg:
		targetPos = offset;
		break;
	case std::ios_base::cur:
		targetPos = (m_state.totalSize - m_state.skipCount) + offset;
		break;
	case std::ios_base::end:
		targetPos = m_state.totalSize + offset;
		break;
	}

	if (targetPos > m_state.totalSize)
	{
		m_state.seekGap = targetPos - m_state.totalSize;
		m_state.skipCount = 0; // cannot have skip and gap.
	}
	else // seeking within logical bounds.
	{
		m_state.skipCount = m_state.totalSize - targetPos;
		m_state.seekGap = 0; // cannot have skip and gap.
	}
}

//-----------------------------------------------------------------------------
// Updates the logical file size after a write of `writeCount` bytes.
// This handles padding from gaps and appending new data.
//-----------------------------------------------------------------------------
void BinaryIO::PostWriteUpdate(const size_t writeCount)
{
	// if we had a gap, append it to the total size as it has been padded out.
	if (m_state.seekGap > 0)
	{
		m_state.totalSize += m_state.seekGap;
		m_state.seekGap = 0;
	}

	// account for overwriting/appending.
	if (m_state.skipCount > 0)
	{
		if (writeCount >= (size_t)m_state.skipCount)
		{
			m_state.totalSize += (writeCount - m_state.skipCount);
			m_state.skipCount = 0;
		}
		else // Writing within the logical bounds.
			m_state.skipCount -= writeCount;
	}
	else
		m_state.totalSize += writeCount;
}
