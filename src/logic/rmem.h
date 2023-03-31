#pragma once

enum class rseekdir : uint8_t {
	beg, // from beginning
	cur, // from current position
	end  // from end of the buffer (requires size to be set)
};

// helper class for working with memory buffers
// wraps around a pointer and allows easy R/W of data
class rmem
{
private:
	void* _pbase = nullptr; // the original pointer. equal to _pbuf - curpos
	void* _pbuf = nullptr; // active buffer pointer. _pbase + _curpos
	unsigned __int64 _curpos = 0;
	unsigned __int64 _bufsize = 0;

public:
	rmem(void* pbuf, unsigned __int64 bufsize = -1)
	{
		this->_pbase = pbuf;
		this->_pbuf = pbuf;
		this->_bufsize = bufsize;
	}

	inline void setBufferSize(unsigned __int64 new_size)
	{
		this->_bufsize = new_size;
	}

	void seek(unsigned __int64 pos, rseekdir dir)
	{
		if (dir == rseekdir::cur && (this->_curpos + pos) < this->_bufsize)
		{
			this->_curpos += pos;
			char* pbuf = (char*)this->_pbuf;
			pbuf += pos;
		}
		else if (dir == rseekdir::beg && pos < this->_bufsize)
		{
			this->_curpos = pos;
			char* pbase = (char*)this->_pbase;
			this->_pbuf = pbase + pos;
		}
	}

	void* getBasePtr() { return this->_pbase; };
	void* getPtr() { return this->_pbuf; };
	unsigned __int64 getPosition() { return this->_curpos; };

public: // read/write
	
	// params:
	// advancebuf - whether the current position should be updated after reading
	template <typename T>
	T read(bool advancebuf=true)
	{
		if (_curpos + sizeof(T) > _bufsize)
			throw "failed to read from buffer: attempted to read past the end of the buffer";

		T val = *(T*)_pbuf;

		if (advancebuf)
		{
			_pbuf = static_cast<char*>(_pbuf) + sizeof(T);
			_curpos += sizeof(T);
		}

		return val;
	}

	template <typename T>
	T* get()
	{
		if(_curpos + sizeof(T) > _bufsize)
			throw "failed to get ptr from buffer: attempted to read past the end of the buffer";

		T* ptr = (T*)_pbuf;

		return ptr;
	}

	template<typename T>
	void write(T val)
	{
		if (_curpos + sizeof(T) > _bufsize)
			throw "failed to write to buffer: attempted to write past the end of the buffer";

		*(T*)_pbuf = val;

		_pbuf = static_cast<char*>(_pbuf) + sizeof(T);
		_curpos += sizeof(T);
	}

	template<typename T>
	void write(T val, unsigned __int64 offset)
	{
		if (offset > _bufsize)
			throw "failed to write to buffer: attempted to write past the end of the buffer";


		*(T*)((char*)_pbase + offset) = val;
	}

//private: // internal helper stuff
	//void _AdvanceBuffer(unsigned __int64 amount);
};

