#include <windows.h>

#include "keyboard.h"

static bool g_isStdInConsole = GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR;

bool Keyboard::Init()
{
	if (g_isStdInConsole)
	{
		HANDLE console = GetStdHandle(STD_INPUT_HANDLE);

		DWORD mode;
		if (!GetConsoleMode(console, &mode))
		{
			return false;
		}

		mode |= ENABLE_EXTENDED_FLAGS;

		mode &= ~ENABLE_ECHO_INPUT;
		mode &= ~ENABLE_LINE_INPUT;
		mode &= ~ENABLE_INSERT_MODE;
		mode &= ~ENABLE_MOUSE_INPUT;
		mode &= ~ENABLE_WINDOW_INPUT;
		mode &= ~ENABLE_PROCESSED_INPUT;
		mode &= ~ENABLE_QUICK_EDIT_MODE;

		if (!SetConsoleMode(console, mode))
		{
			return false;
		}
	}

	return true;
}

static bool PeekChar()
{
	if (g_isStdInConsole)
	{
		HANDLE console = GetStdHandle(STD_INPUT_HANDLE);

		INPUT_RECORD record;
		DWORD read;
		if (!PeekConsoleInputA(console, &record, 1, &read) || read == 0 || record.EventType != KEY_EVENT)
		{
			return false;
		}
	}

	return true;
}

static char ReadChar()
{
	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);

	char ch;
	DWORD read;
	BOOL status;

	if (g_isStdInConsole)
	{
		status = ReadConsoleA(input, &ch, 1, &read, NULL);
	}
	else
	{
		status = ReadFile(input, &ch, 1, &read, NULL);
	}

	if (!status)
	{
		return kiv_hal::NControl_Codes::EOT;
	}

	if (read == 0)
	{
		return kiv_hal::NControl_Codes::NUL;
	}

	return ch;
}

void __stdcall Keyboard::InterruptHandler(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_hal::NKeyboard>(context.rax.h))
	{
		case kiv_hal::NKeyboard::Peek_Char:
		{
			context.flags.non_zero = PeekChar() ? 1 : 0;
			break;
		}
		case kiv_hal::NKeyboard::Read_Char:
		{
			char ch = ReadChar();
			context.rax.x = ch;
			context.flags.non_zero = (ch == kiv_hal::NControl_Codes::EOT) ? 0 : 1;
			break;
		}
		default:
		{
			context.flags.carry = 1;
			break;
		}
	}
}
