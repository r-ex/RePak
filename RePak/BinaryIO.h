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
	BinaryIOMode_None = 0,
	BinaryIOMode_Read,
	BinaryIOMode_Write
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
		currentMode = BinaryIOMode::BinaryIOMode_None;
	}

	// the destructor will be responsible for checking if we forgot to close
	// the file
	~BinaryIO()
	{
		if (writer.is_open())
		{
			//LOG_ERROR(LoggingClass_BinaryIO, "You forgot to call close() after finishing with the file! Closing it...");
			writer.close();
		}

		if (reader.is_open())
		{
			//LOG_ERROR(LoggingClass_BinaryIO, "You forgot to call close() after finishing with the file! Closing it...");
			reader.close();
		}
	}

	// opens a file with either read or write mode. Returns whether
	// the open operation was successful
	bool open(std::string fileFullPath, BinaryIOMode mode)
	{
		filePath = fileFullPath;

		//LOG_INFO(LoggingClass_BinaryIO, "Opening file: " + filePath);

		// Write mode
		if (mode == BinaryIOMode::BinaryIOMode_Write)
		{
			currentMode = mode;
			// check if we had a previously opened file to close it
			if (writer.is_open())
				writer.close();

			writer.open(filePath.c_str(), std::ios::binary);
			if (!writer.is_open())
			{
				//LOG_ERROR(LoggingClass_BinaryIO, "Could not open file for write: " + filePath);
				currentMode = BinaryIOMode::BinaryIOMode_None;
			}
		}
		// Read mode
		else if (mode == BinaryIOMode::BinaryIOMode_Read)
		{
			currentMode = mode;
			// check if we had a previously opened file to close it
			if (reader.is_open())
				reader.close();

			reader.open(filePath.c_str(), std::ios::binary);
			if (!reader.is_open())
			{
				//LOG_ERROR(LoggingClass_BinaryIO, "Could not open file for read: " + filePath);
				currentMode = BinaryIOMode::BinaryIOMode_None;
			}
		}

		// if the mode is still the NONE/initial one -> we failed
		return currentMode == BinaryIOMode::BinaryIOMode_None ? false : true;
	}

	// closes the file
	void close()
	{
		if (currentMode == BinaryIOMode::BinaryIOMode_Write)
		{
			writer.close();
		}
		else if (currentMode == BinaryIOMode::BinaryIOMode_Read)
		{
			reader.close();
		}
	}

	// checks whether we're allowed to write or not.
	bool checkWritabilityStatus()
	{
		if (currentMode != BinaryIOMode::BinaryIOMode_Write)
		{
			//LOG_ERROR(LoggingClass_BinaryIO, "Trying to write with a non Writable mode!");
			return false;
		}
		return true;
	}

	// helper to check if we're allowed to read
	bool checkReadabilityStatus()
	{
		if (currentMode != BinaryIOMode::BinaryIOMode_Read)
		{
			//LOG_ERROR(LoggingClass_BinaryIO, "Trying to read with a non Readable mode!");
			return false;
		}

		// check if we hit the end of the file.
		if (reader.eof())
		{
			//LOG_ERROR(LoggingClass_BinaryIO, "Trying to read but reached the end of file!");
			reader.close();
			currentMode = BinaryIOMode::BinaryIOMode_None;
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
		if (currentMode != BinaryIOMode::BinaryIOMode_Read) {
			return NULL;
		}
		return &reader;
	}
	std::ofstream* getWriter() {
		if (currentMode != BinaryIOMode::BinaryIOMode_Write) {
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
		return reader.tellg();
	}

	void seek(size_t off, std::ios::seekdir dir = std::ios::beg)
	{
		switch (currentMode)
		{
		case BinaryIOMode::BinaryIOMode_Write:
			writer.seekp(off, dir);
			break;
		case BinaryIOMode::BinaryIOMode_Read:
			reader.seekg(off, dir);
			break;
		default:
			break;
		}
	}
};