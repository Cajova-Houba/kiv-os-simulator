#pragma once

#include <mutex>

#include "handle.h"
#include "compiler.h"

class Console : public IFileHandle
{
	std::mutex m_readerMutex;
	std::mutex m_writerMutex;

public:
	Console() = default;

	EFileHandle getFileHandleType() const override
	{
		return EFileHandle::CONSOLE;
	}

	void close() override
	{
	}

	EStatus read(char *buffer, size_t bufferSize, size_t *pRead) override;
	EStatus write(const char *buffer, size_t bufferSize, size_t *pWritten) override;

	void log(const char *format, ...) COMPILER_PRINTF_ARGS_CHECK(2,3);
	void logV(const char *format, va_list args);
};
