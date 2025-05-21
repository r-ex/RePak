#include <pch.h>

//-----------------------------------------------------------------------------
// Purpose: sets the windows terminal background color
// Input  : color - 
//-----------------------------------------------------------------------------
static void Console_SetBackgroundColor(COLORREF color)
{
    CONSOLE_SCREEN_BUFFER_INFOEX sbInfoEx{ 0 };
    sbInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

    HANDLE consoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfoEx(consoleOut, &sbInfoEx);

    // The +='' 1 is required, else the window will shrink
    // by '1' column and row each time this function is
    // getting called on the same console window. The
    // lower right bounds are detected inclusively on the
    // 'GetConsoleScreenBufferEx' call and exclusively
    // on the 'SetConsoleScreenBufferEx' call.
    sbInfoEx.srWindow.Right += 1;
    sbInfoEx.srWindow.Bottom += 1;

    sbInfoEx.ColorTable[0] = color;
    SetConsoleScreenBufferInfoEx(consoleOut, &sbInfoEx);
}

//-----------------------------------------------------------------------------
// Purpose: terminal color setup
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool Console_ColorInit()
{
    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = NULL;

    GetConsoleMode(hOutput, &dwMode); // Some editions of Windows have 'VirtualTerminalLevel' disabled by default.
    dwMode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(hOutput, dwMode))
        return false; // Failure.

    Console_SetBackgroundColor(0x00000000);
    Logger_colorInit();

    return true;
}