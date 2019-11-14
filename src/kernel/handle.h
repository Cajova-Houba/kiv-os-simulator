#pragma once

#include "status.h"
#include "types.h"

using HandleID = uint16_t;  // kiv_os::THandle

constexpr size_t MAX_HANDLE_COUNT = static_cast<HandleID>(-1);  // 65535


enum struct EHandle
{
	FILE, THREAD, PROCESS
};

struct IHandle
{
	virtual ~IHandle() = default;

	virtual EHandle getHandleType() const = 0;
};


enum struct EFileHandle
{
	REGULAR_FILE, DIRECTORY, CONSOLE, PIPE_READ_END, PIPE_WRITE_END
};

struct IFileHandle : public IHandle
{
	EHandle getHandleType() const override final
	{
		return EHandle::FILE;
	}

	virtual EFileHandle getFileHandleType() const = 0;

	virtual void close() = 0;

	virtual EStatus read(char *buffer, size_t bufferSize, size_t *pRead) = 0;
	virtual EStatus write(const char *buffer, size_t bufferSize, size_t *pWritten) = 0;
};
