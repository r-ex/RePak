#include "pch.h"
#include "strutils.h"

std::vector<std::string> Utils::StringSplit(std::string input, const char delim, const size_t max)
{
    std::string subString;
    std::vector<string> subStrings;

    input = input + delim;

    for (size_t i = 0; i < input.size(); i++)
    {
        if ((i != (input.size() - 1) && subStrings.size() >= max)
            || input[i] != delim)
        {
            subString += input[i];
        }
        else
        {
            if (subString.size() != 0)
            {
                subStrings.push_back(subString);
            }

            subString.clear();
        }
    }

    return subStrings;
}

// purpose: formats a standard string with printf like syntax (see 'https://stackoverflow.com/a/49812018')
const std::string Utils::VFormat(const char* const zcFormat, ...)
{
    // initialize use of the variable argument array
    va_list vaArgs;
    va_start(vaArgs, zcFormat);

    // reliably acquire the size
    // from a copy of the variable argument array
    // and a functionally reliable call to mock the formatting
    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int iLen = std::vsnprintf(NULL, 0, zcFormat, vaArgsCopy);
    va_end(vaArgsCopy);

    // return a formatted string without risking memory mismanagement
    // and without assuming any compiler or platform specific behavior
    std::vector<char> zc(iLen + 1);
    std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
    va_end(vaArgs);
    return std::string(zc.data(), iLen);
}

void Utils::FourCCToString(FourCCString_t& buf, const unsigned int n)
{
    buf[0] = (char)((n & 0x000000ff) >> 0);
    buf[1] = (char)((n & 0x0000ff00) >> 8);
    buf[2] = (char)((n & 0x00ff0000) >> 16);
    buf[3] = (char)((n & 0xff000000) >> 24);
    buf[4] = '\0';
};
