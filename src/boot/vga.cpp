#include <string>
#include <windows.h>

#include "vga.h"

static bool g_isStdOutConsole = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR;

static void WriteString(const char *string, size_t length)
{
	std::string buffer;
	buffer.reserve(length);

	for (size_t i = 0; i < length; i++)
	{
		const char ch = string[i];
		switch (ch)
		{
			case '\b':  // backspace
			{
				if (g_isStdOutConsole)
				{
					buffer += '\b';
					buffer += ' ';
					buffer += '\b';
				}
				else
				{
					buffer += '\b';
				}
				break;
			}
			case '\n':  // newline
			{
				// CRLF
				buffer += '\r';
				buffer += '\n';
				break;
			}
			default:
			{
				buffer += ch;
				break;
			}
		}
	}

	HANDLE file = GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD written;
	WriteFile(file, buffer.c_str(), static_cast<DWORD>(buffer.length()), &written, NULL);
}

void __stdcall VGA::InterruptHandler(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_hal::NVGA_BIOS>(context.rax.h))
	{
		case kiv_hal::NVGA_BIOS::Write_Control_Char:
		{
			const char ch = context.rdx.l;
			WriteString(&ch, 1);
			break;
		}
		case kiv_hal::NVGA_BIOS::Write_String:
		{
			const char *string = reinterpret_cast<const char*>(context.rdx.r);
			const size_t length = context.rcx.r;
			WriteString(string, length);
			break;
		}
		default:
		{
			context.flags.carry = 1;
			break;
		}
	}
}
