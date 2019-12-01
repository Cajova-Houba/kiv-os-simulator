#include <windows.h>

#include "vga.h"
#include "string_buffer.h"

static bool g_isStdOutConsole = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR;

static void WriteString(const char *string, size_t length)
{
	StringBuffer<4096> buffer;
	buffer.makeSpaceFor(length);

	for (size_t i = 0; i < length; i++)
	{
		char ch = string[i];

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

	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD written;
	WriteFile(output, buffer.get(), static_cast<DWORD>(buffer.getLength()), &written, NULL);
}

void __stdcall VGA::InterruptHandler(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_hal::NVGA_BIOS>(context.rax.h))
	{
		case kiv_hal::NVGA_BIOS::Write_Control_Char:
		{
			char ch = context.rdx.l;
			WriteString(&ch, 1);
			break;
		}
		case kiv_hal::NVGA_BIOS::Write_String:
		{
			WriteString(reinterpret_cast<const char*>(context.rdx.r), static_cast<size_t>(context.rcx.r));
			break;
		}
		default:
		{
			context.flags.carry = 1;
			break;
		}
	}
}
