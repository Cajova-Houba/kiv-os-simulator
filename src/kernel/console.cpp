#include <cstdio>
#include <cstdarg>

#include "../api/hal.h"

#include "console.h"
#include "console_reader.h"

static void HALWriteString(const char *buffer, size_t bufferSize)
{
	kiv_hal::TRegisters context;
	context.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_String);
	context.rdx.r = reinterpret_cast<uint64_t>(buffer);
	context.rcx.r = bufferSize;

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, context);
}

Console::Console()
{
	m_pReader = &ConsoleReader::GetInstance();
}

void Console::close()
{
	m_pReader->close();
}

EStatus Console::read(char *buffer, size_t bufferSize, size_t *pRead)
{
	m_pReader->readLine(buffer, bufferSize, pRead);

	return EStatus::SUCCESS;
}

EStatus Console::write(const char *buffer, size_t bufferSize, size_t *pWritten)
{
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

	write(buffer, length, nullptr);
}
