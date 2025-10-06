#pragma once

typedef void(*BinaryIOLogger_fn)(const char* fmt, ...);
inline BinaryIOLogger_fn g_iosmErrorCallback = nullptr;

class BinaryIO
{
public:
	enum class Mode_e
	{
		None = 0,
		Read,
		Write,
		ReadWrite, // For existing files only.
		ReadWriteCreate
	};

	BinaryIO();
	~BinaryIO();

	inline bool Open(const char* const filePath, const Mode_e mode) { return OpenEx(filePath, mode, strlen(filePath)); }
	inline bool Open(const std::string& filePath, const Mode_e mode) { return OpenEx(filePath.c_str(), mode, filePath.length()); };

	void Close();
	void Reset();
	void Flush();

	std::streamoff TellGet();
	std::streamoff TellPut();

	void SeekGet(const std::streamoff offset, const std::ios_base::seekdir way = std::ios::beg);
	void SeekPut(const std::streamoff offset, const std::ios_base::seekdir way = std::ios::beg);
	void Seek(const std::streamoff offset, const std::ios_base::seekdir way = std::ios::beg);

	const std::filebuf* GetData() const;
	const std::streamoff GetSize() const;

	bool IsReadMode() const;
	bool IsWriteMode() const;

	bool IsReadable() const;
	bool IsWritable() const;

	bool IsEof() const;

	//-----------------------------------------------------------------------------
	// Reads any value from the file with specified size
	//-----------------------------------------------------------------------------
	template<typename T>
	inline bool Read(T* const outBuf, const size_t readCount, size_t* const outReadCount = nullptr)
	{
		return DoRead(reinterpret_cast<char*>(outBuf), readCount, outReadCount);
	}

	//-----------------------------------------------------------------------------
	// Reads any value from the file
	//-----------------------------------------------------------------------------
	template<typename T>
	inline bool Read(T& value, size_t* const outReadCount = nullptr)
	{
		return Read<T>(&value, sizeof(T), outReadCount);
	}

	//-----------------------------------------------------------------------------
	// Reads any value from the file and returns it
	//-----------------------------------------------------------------------------
	template<typename T>
	inline T Read(size_t* const outReadCount = nullptr)
	{
		T value{};
		Read<T>(&value, sizeof(T), outReadCount);

		return value;
	}
	bool ReadString(std::string& svOut);
	bool ReadString(char* const pBuf, const size_t nLen, size_t* const outWriteCount = nullptr);

	//-----------------------------------------------------------------------------
	// Writes any value to the file with specified size
	//-----------------------------------------------------------------------------
	template<typename T>
	inline bool Write(const T* const inBuf, const size_t writeCount, size_t* const outWriteCount = nullptr)
	{
		return DoWrite(reinterpret_cast<const char*>(inBuf), writeCount, outWriteCount);
	}

	//-----------------------------------------------------------------------------
	// Writes any value to the file
	//-----------------------------------------------------------------------------
	template<typename T>
	inline bool Write(const T& value, size_t* const outWriteCount = nullptr)
	{
		return Write<T>(&value, sizeof(T), outWriteCount);
	}

	//-----------------------------------------------------------------------------
	// Writes a string to the file
	//-----------------------------------------------------------------------------
	inline bool WriteString(const std::string& input, const bool nullTerminate, size_t* const outWriteCount = nullptr)
	{
		return Write<char>(input.c_str(), input.length() + nullTerminate, outWriteCount);
	}

	bool Pad(const size_t count, size_t* const outPadCount = nullptr);

protected:
	bool OpenEx(const char* const filePath, const Mode_e mode, const size_t pathLen);

	void UpdateSeekState(const std::streamoff offset, const std::ios_base::seekdir way);
	void PostWriteUpdate(const std::streamoff writeCount);

	bool DoRead(char* const outBuf, const size_t readCount, size_t* const outReadCount);
	bool DoWrite(const char* const inBuf, const size_t writeCount, size_t* const outWriteCount);

private:
	struct CursorState_s
	{
		std::streamoff totalSize; // Total stream output size.
		std::streamoff skipCount; // Amount seeked into logical bounds.
		std::streamoff seekGap;   // Amount seeked beyond logical bounds.
	};

	std::fstream            m_stream; // I/O stream.
	CursorState_s           m_state;  // Stream state.
	std::ios_base::openmode m_flags;  // Stream flags.
	Mode_e                  m_mode;   // Stream mode.
	std::string             m_name;   // Stream name.
};
