#include <cstdio>
#include <cstdarg>

#include "../api/hal.h"

#include "console.h"

static void HALWriteChar(char ch)
{
	kiv_hal::TRegisters context;
	context.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_Control_Char);
	context.rdx.l = ch;

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, context);
}

static void HALWriteString(const char *buffer, size_t bufferSize)
{
	kiv_hal::TRegisters context;
	context.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_String);
	context.rdx.r = reinterpret_cast<uint64_t>(buffer);
	context.rcx.r = bufferSize;

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, context);
}

static bool HALReadChar(char & ch)
{
	kiv_hal::TRegisters context;
	context.rax.h = static_cast<uint8_t>(kiv_hal::NKeyboard::Read_Char);

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Keyboard, context);

	ch = context.rax.l;

	if (!context.flags.non_zero && ch != static_cast<char>(kiv_hal::NControl_Codes::EOT))
	{
		return false;
	}

	return true;
}

static int HALReadLine(char *buffer, size_t bufferSize)
{
	size_t length = 0;

	while (length < bufferSize)
	{
		char ch;
		if (!HALReadChar(ch))
		{
			return -1;
		}

		switch (ch)
		{
			case '\0':
			{
				return static_cast<int>(length);
			}
			case '\b':  // Backspace
			{
				if (length > 0)
				{
					length--;
					HALWriteChar('\b');
				}

				break;
			}
			case '\n':
			{
				break;
			}
			case '\r':  // Enter
			{
				buffer[length++] = '\n';
				HALWriteChar('\n');

				return static_cast<int>(length);
			}
			case 3:     // Ctrl + C
			case 4:     // Ctrl + D
			case 26:    // Ctrl + Z
			case '\t':  // Tabulátor
			{
				buffer[length++] = ch;

				return static_cast<int>(length);
			}
			default:
			{
				buffer[length++] = ch;
				HALWriteChar(ch);

				break;
			}
		}
	}

	return static_cast<int>(length);
}

EStatus Console::read(char *buffer, size_t bufferSize, size_t *pRead)
{
	if (buffer == nullptr || bufferSize == 0)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	std::lock_guard<std::mutex> lock(m_readerMutex);

	EStatus status = EStatus::SUCCESS;

	int length = HALReadLine(buffer, bufferSize);
	if (length < 0)
	{
		length = 0;
		status = EStatus::IO_ERROR;
	}

	if (pRead)
	{
		(*pRead) = length;
	}

	return status;
}

EStatus Console::write(const char *buffer, size_t bufferSize, size_t *pWritten)
{
	if (buffer == nullptr || bufferSize == 0)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	std::lock_guard<std::mutex> lock(m_writerMutex);

	HALWriteString(buffer, bufferSize);

	if (pWritten)
	{
		// VGA BIOS nevrací počet zapsaných znaků
		(*pWritten) = bufferSize;
	}

	return EStatus::SUCCESS;
}

void Console::log(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	logV(format, args);
	va_end(args);
}

void Console::logV(const char *format, va_list args)
{
	char buffer[4096];
	int status = std::vsnprintf(buffer, sizeof buffer, format, args);
	if (status < 0)
	{
		return;
	}

	size_t length = status;
	if (length >= sizeof buffer)
	{
		length = sizeof buffer - 1;
	}

	// nahrazení ukončovacího nulového znaku oddělovačem řádků
	buffer[length++] = '\n';
	// výsledný řetězec už není ukončený nulou!

	std::lock_guard<std::mutex> lock(m_writerMutex);

	HALWriteString(buffer, length);
}
