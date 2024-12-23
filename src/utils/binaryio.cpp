#include "pch.h"
#include "binaryio.h"
#include <sys/stat.h>

//-----------------------------------------------------------------------------
// Purpose: CIOStream constructors
//-----------------------------------------------------------------------------
BinaryIO::BinaryIO()
{
	m_size = 0;
	m_mode = Mode_e::None;
	m_flags = 0;
}

//-----------------------------------------------------------------------------
// Purpose: CIOStream destructor
//-----------------------------------------------------------------------------
BinaryIO::~BinaryIO()
{
	Close();
}

//-----------------------------------------------------------------------------
// Purpose: get internal stream mode from selected mode
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
// Purpose: opens the file in specified mode
// Input  : *filePath - 
//			mode - 
// Output : true if operation is successful
//-----------------------------------------------------------------------------
bool BinaryIO::Open(const char* const filePath, const Mode_e mode)
{
	m_flags = GetInternalStreamMode(mode);
	m_mode = mode;

	if (m_stream.is_open())
	{
		m_stream.close();
	}

	m_stream.open(filePath, GetInternalStreamMode(mode));

	if (!m_stream.is_open() || !m_stream.good())
	{
		return false;
	}

	if (IsReadMode())
	{
		struct _stat64 status;
		if (_stat64(filePath, &status) != NULL)
		{
			return false;
		}

		m_size = status.st_size;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: closes the stream
//-----------------------------------------------------------------------------
void BinaryIO::Close()
{
	m_stream.close();
	m_size = 0;
	m_mode = Mode_e::None;
	m_flags = 0;
}

//-----------------------------------------------------------------------------
// Purpose: flushes the ofstream
//-----------------------------------------------------------------------------
void BinaryIO::Flush()
{
	if (IsWritable())
		m_stream.flush();
}

//-----------------------------------------------------------------------------
// Purpose: gets the position of the current character in the stream
// Output : std::streampos
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
// Purpose: sets the position of the current character in the stream
// Input  : offset - 
//			way - 
//-----------------------------------------------------------------------------
void BinaryIO::SeekGet(const std::streamoff offset, const std::ios_base::seekdir way)
{
	assert(IsReadMode());
	m_stream.seekg(offset, way);
}
void BinaryIO::SeekPut(const std::streamoff offset, const std::ios_base::seekdir way)
{
	assert(IsWriteMode());

	m_stream.seekp(offset, way);
	const std::streamoff newOffset = m_stream.tellp();

	// seekp writes padding when we go beyond eof, therefore we should update
	// the size in case this happens.
	if (newOffset > m_size)
		m_size = newOffset;

}
void BinaryIO::Seek(const std::streamoff offset, const std::ios_base::seekdir way)
{
	if (IsReadMode())
		SeekGet(offset, way);
	if (IsWriteMode())
		SeekPut(offset, way);
}

//-----------------------------------------------------------------------------
// Purpose: returns the data
// Output : std::filebuf*
//-----------------------------------------------------------------------------
const std::filebuf* BinaryIO::GetData() const
{
	return m_stream.rdbuf();
}

//-----------------------------------------------------------------------------
// Purpose: returns the data size
// Output : std::streampos
//-----------------------------------------------------------------------------
const std::streamoff BinaryIO::GetSize() const
{
	return m_size;
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
// Purpose: checks if we are able to read the file
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool BinaryIO::IsReadable() const
{
	if (!IsReadMode() || !m_stream || m_stream.eof())
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: checks if we are able to write to file
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool BinaryIO::IsWritable() const
{
	if (!IsWriteMode() || !m_stream)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: checks if we hit the end of file
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool BinaryIO::IsEof() const
{
	return m_stream.eof();
}

//-----------------------------------------------------------------------------
// Purpose: reads a string from the file
// Input  : &svOut - 
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool BinaryIO::ReadString(std::string& out)
{
	if (!IsReadable())
		return false;

	while (!m_stream.eof())
	{
		const char c = Read<char>();

		if (c == '\0')
			break;

		out += c;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: reads a string from the file into a fixed size buffer
// Input  : *buf - 
//			len - 
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool BinaryIO::ReadString(char* const buf, const size_t len)
{
	if (!IsReadable())
		return false;

	size_t i = 0;

	while (i < len && !m_stream.eof())
	{
		const char c = Read<char>();

		if (c == '\0')
			break;

		buf[i++] = c;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: writes a string to the file
// Input  : &input - 
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool BinaryIO::WriteString(const std::string& input)
{
	if (!IsWritable())
		return false;

	const char* const text = input.c_str();
	const size_t len = input.length();

	m_stream.write(text, len);
	m_size += len;

	return true;
}

// limit number of io calls and allocations by just using this static buffer
// for padding out the stream.
static constexpr size_t PAD_BUF_SIZE = 4096;
const static char s_padBuf[PAD_BUF_SIZE];

//-----------------------------------------------------------------------------
// Purpose: pads the out stream up to count bytes
// Input  : count - 
//-----------------------------------------------------------------------------
void BinaryIO::Pad(const size_t count)
{
	assert(count > 0);
	size_t remainder = count;

	while (remainder)
	{
		const size_t writeCount = (std::min)(count, PAD_BUF_SIZE);
		Write(s_padBuf, writeCount);

		remainder -= writeCount;
	}
}
