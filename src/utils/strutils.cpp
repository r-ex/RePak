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

static string FormatV(const char* szFormat, va_list args)
{
    // Initialize use of the variable argument array.
    va_list argsCopy;
    va_copy(argsCopy, args);

    // Dry run to obtain required buffer size.
    const int iLen = std::vsnprintf(nullptr, 0, szFormat, argsCopy);
    va_end(argsCopy);

    assert(iLen >= 0);
    string result;

    if (iLen > 0)
    {
        // NOTE: reserve enough buffer size for the string + the terminating
        // NULL character, then resize it to just the string len so we don't
        // count the NULL character in the string's size (i.e. when calling
        // string::size()).
        result.reserve(iLen + 1);
        result.resize(iLen);

        std::vsnprintf(&result[0], iLen + 1, szFormat, args);
    }

    return result;
}

// purpose: formats a standard string with printf like syntax (see 'https://stackoverflow.com/a/49812018')
const std::string Utils::VFormat(const char* const zcFormat, ...)
{
    string result;

    va_list args;
    va_start(args, zcFormat);
    result = FormatV(zcFormat, args);
    va_end(args);

    return result;
}

void Utils::FourCCToString(FourCCString_t& buf, const unsigned int n)
{
    buf[0] = (char)((n & 0x000000ff) >> 0);
    buf[1] = (char)((n & 0x0000ff00) >> 8);
    buf[2] = (char)((n & 0x00ff0000) >> 16);
    buf[3] = (char)((n & 0xff000000) >> 24);
    buf[4] = '\0';
};
