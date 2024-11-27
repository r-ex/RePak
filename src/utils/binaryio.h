#pragma once

//
// adapted from https://karlboghossian.com/2012/12/29/how-to-mimic-csharp-binaryreader-binarywriter-in-cplusplus/
//
enum class FileMode
{
	Open = 0,
	Create,
	Append
};

enum class BinaryIOMode
{
	None = 0,
	Read,
	Write
};

class BinaryIO
{
	// the output file stream to write onto a file
	std::ofstream writer;
	// the input file stream to read from a file
	std::ifstream reader;
	// the filepath of the file we're working with
	std::string filePath;
	// the current active mode.
	BinaryIOMode currentMode;

public:
	BinaryIO()
	{
		currentMode = BinaryIOMode::None;
	}

	BinaryIO(const std::string& path, BinaryIOMode mode)
	{
		open(path, mode);
	}

	BinaryIO(const std::filesystem::path& path, BinaryIOMode mode)
	{
		open(path.string(), mode);
	}

	// the destructor will be responsible for checking if we forgot to close
	// the file
	~BinaryIO()
	{
		if (writer.is_open())
		{
			writer.close();
		}

		if (reader.is_open())
		{
			reader.close();
		}
	}

	// opens a file with either read or write mode. Returns whether
	// the open operation was successful
	bool open(const std::string& fileFullPath, BinaryIOMode mode)
	{
		filePath = fileFullPath;

		// Write mode
		if (mode == BinaryIOMode::Write)
		{
			currentMode = mode;
			// check if we had a previously opened file to close it
			if (writer.is_open())
				writer.close();

			writer.open(filePath.c_str(), std::ios::binary);
			if (!writer.is_open())
			{
				currentMode = BinaryIOMode::None;
			}
		}
		// Read mode
		else if (mode == BinaryIOMode::Read)
		{
			currentMode = mode;
			// check if we had a previously opened file to close it
			if (reader.is_open())
				reader.close();

			reader.open(filePath.c_str(), std::ios::binary);
			if (!reader.is_open())
			{
				currentMode = BinaryIOMode::None;
			}
		}

		// if the mode is still the NONE/initial one -> we failed
		return currentMode == BinaryIOMode::None ? false : true;
	}

	// closes the file
	void close()
	{
		if (currentMode == BinaryIOMode::Write)
		{
			writer.close();
		}
		else if (currentMode == BinaryIOMode::Read)
		{
			reader.close();
		}
	}

	// checks whether we're allowed to write or not.
	bool checkWritabilityStatus()
	{
		if (currentMode != BinaryIOMode::Write)
		{
			return false;
		}
		return true;
	}

	// helper to check if we're allowed to read
	bool checkReadabilityStatus()
	{
		if (currentMode != BinaryIOMode::Read)
		{
			return false;
		}

		// check if we hit the end of the file.
		if (reader.eof())
		{
			reader.close();
			currentMode = BinaryIOMode::None;
			return false;
		}

		return true;
	}

	// so we can check if we hit the end of the file
	bool eof()
	{
		return reader.eof();
	}

	// Generic write method that will write any value to a file (except a string,
	// for strings use writeString instead)
	template <typename T>
	void write(T& value)
	{
		if (!checkWritabilityStatus())
			return;

		// write the value to the file.
		writer.write((const char*)&value, sizeof(value));
	}

	// Writes a string to the file
	void writeString(std::string str)
	{
		if (!checkWritabilityStatus())
			return;

		// first add a \0 at the end of the string so we can detect
		// the end of string when reading it
		str += '\0';

		// create char pointer from string.
		char* text = (char*)(str.c_str());
		// find the length of the string.
		size_t size = str.size();

		// write the whole string including the null.
		writer.write((const char*)text, size);
	}

	// reads any type of value except strings.
	template <typename T>
	T read()
	{
		checkReadabilityStatus();

		T value;
		reader.read((char*)&value, sizeof(value));
		return value;
	}

	// reads any type of value except strings.
	template <typename T>
	void read(T& value)
	{
		if (checkReadabilityStatus())
		{
			reader.read((char*)&value, sizeof(value));
		}
	}

	std::ifstream* getReader() {
		if (currentMode != BinaryIOMode::Read) {
			return NULL;
		}
		return &reader;
	}
	std::ofstream* getWriter() {
		if (currentMode != BinaryIOMode::Write) {
			return NULL;
		}
		return &writer;
	}

	// read a string value
	std::string readString()
	{
		if (checkReadabilityStatus())
		{
			char c;
			std::string result = "";
			while (!reader.eof() && (c = read<char>()) != '\0')
			{
				result += c;
			}

			return result;
		}
		return "";
	}

	// read a string value
	void readString(std::string& result)
	{
		if (checkReadabilityStatus())
		{
			char c;
			result = "";
			while (!reader.eof() && (c = read<char>()) != '\0')
			{
				result += c;
			}
		}
	}

	size_t tell()
	{
		switch (currentMode)
		{
		case BinaryIOMode::Write:
			return writer.tellp();
		case BinaryIOMode::Read:
			return reader.tellg();
		default:
			assert(0); // Code bug: no mode selected.
			return 0;
		}
	}

	void seek(size_t off, std::ios::seekdir dir = std::ios::beg)
	{
		switch (currentMode)
		{
		case BinaryIOMode::Write:
			writer.seekp(off, dir);
			break;
		case BinaryIOMode::Read:
			reader.seekg(off, dir);
			break;
		default:
			break;
		}
	}
};