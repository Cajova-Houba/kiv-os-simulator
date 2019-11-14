#pragma once

#include <array>
#include <mutex>
#include <condition_variable>

#include "handle_reference.h"

namespace Pipe
{
	bool Create(HandleReference & readEnd, HandleReference & writeEnd);
}

class PipeWriteEnd;

class PipeReadEnd : public IFileHandle
{
	std::array<char, 1024> m_buffer;
	size_t m_readerPos;
	size_t m_writerPos;
	bool m_isFull;
	bool m_isClosed;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	PipeWriteEnd *m_pWriteEnd;

	size_t push(const char *data, size_t dataLength);
	void onWriteEndClosed();

	friend class PipeWriteEnd;
	friend bool Pipe::Create(HandleReference &, HandleReference &);

public:
	PipeReadEnd() = default;

	~PipeReadEnd()
	{
		close();
	}

	EFileHandle getFileHandleType() const override
	{
		return EFileHandle::PIPE_READ_END;
	}

	void close() override;

	EStatus read(char *buffer, size_t bufferSize, size_t *pRead) override;

	EStatus write(const char*, size_t, size_t*) override
	{
		return EStatus::INVALID_ARGUMENT;
	}
};

class PipeWriteEnd : public IFileHandle
{
	std::mutex m_mutex;
	PipeReadEnd *m_pReadEnd;

	void onReadEndClosed();

	friend class PipeReadEnd;
	friend bool Pipe::Create(HandleReference &, HandleReference &);

public:
	PipeWriteEnd() = default;

	~PipeWriteEnd()
	{
		close();
	}

	EFileHandle getFileHandleType() const override
	{
		return EFileHandle::PIPE_WRITE_END;
	}

	void close() override;

	EStatus read(char*, size_t, size_t*) override
	{
		return EStatus::INVALID_ARGUMENT;
	}

	EStatus write(const char *buffer, size_t bufferSize, size_t *pWritten) override;
};
