#include <cstring>  // std::memcpy

#include "pipe.h"
#include "kernel.h"

bool Pipe::Create(HandleReference & readEnd, HandleReference & writeEnd)
{
	HandleReference readEndHandle = Kernel::GetHandleStorage().addHandle(std::make_unique<PipeReadEnd>());
	if (!readEndHandle)
	{
		return false;
	}

	HandleReference writeEndHandle = Kernel::GetHandleStorage().addHandle(std::make_unique<PipeWriteEnd>());
	if (!writeEndHandle)
	{
		return false;
	}

	readEndHandle.as<PipeReadEnd>()->m_pWriteEnd = writeEndHandle.as<PipeWriteEnd>();
	writeEndHandle.as<PipeWriteEnd>()->m_pReadEnd = readEndHandle.as<PipeReadEnd>();

	readEnd = std::move(readEndHandle);
	writeEnd = std::move(writeEndHandle);

	return true;
}

size_t PipeReadEnd::push(const char *data, size_t dataLength)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	if (!m_pWriteEnd)
	{
		return 0;
	}

	size_t totalLength = 0;

	while (totalLength < dataLength)
	{
		while (m_isFull)
		{
			m_cv.wait(lock);
		}

		size_t length = (m_writerPos >= m_readerPos) ? m_buffer.size() - m_writerPos : m_readerPos - m_writerPos;

		if (length + totalLength > dataLength)
		{
			length = dataLength - totalLength;
		}

		std::memcpy(m_buffer.data() + m_writerPos, data + totalLength, length);

		totalLength += length;

		m_writerPos += length;
		if (m_writerPos >= m_buffer.size())
		{
			m_writerPos = 0;
		}

		if (m_writerPos == m_readerPos)
		{
			m_isFull = true;
		}
	}

	lock.unlock();
	m_cv.notify_one();

	return totalLength;
}

void PipeReadEnd::onWriteEndClosed()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_pWriteEnd = nullptr;

	lock.unlock();
	m_cv.notify_one();  // někdo uzavřel zapisovací konec roury, takže probudíme čtecí vlákno čekající na další data
}

void PipeReadEnd::close()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_isClosed = true;

	if (m_pWriteEnd)
	{
		m_pWriteEnd->onReadEndClosed();
		m_pWriteEnd = nullptr;
	}

	lock.unlock();
	m_cv.notify_one();
}

EStatus PipeReadEnd::read(char *buffer, size_t bufferSize, size_t *pRead)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	if (m_isClosed)
	{
		if (pRead)
		{
			(*pRead) = 0;
		}

		return EStatus::INVALID_ARGUMENT;
	}

	while (m_readerPos == m_writerPos && !m_isFull)  // buffer je prázdný
	{
		if (m_pWriteEnd)
		{
			m_cv.wait(lock);
		}
		else
		{
			if (pRead)
			{
				(*pRead) = 0;
			}

			return EStatus::SUCCESS;
		}
	}

	size_t totalLength = 0;

	while (m_readerPos != m_writerPos && totalLength < bufferSize)
	{
		size_t length = (m_readerPos < m_writerPos) ? m_writerPos - m_readerPos : m_buffer.size() - m_readerPos;

		if (length + totalLength > bufferSize)
		{
			length = bufferSize - totalLength;
		}

		std::memcpy(buffer + totalLength, m_buffer.data() + m_readerPos, length);

		totalLength += length;

		m_readerPos += length;
		if (m_readerPos >= m_buffer.size())
		{
			m_readerPos = 0;
		}
	}

	if (totalLength > 0)
	{
		m_isFull = false;
	}

	lock.unlock();
	m_cv.notify_one();

	if (pRead)
	{
		(*pRead) = totalLength;
	}

	return EStatus::SUCCESS;
}

void PipeWriteEnd::onReadEndClosed()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_pReadEnd = nullptr;
}

void PipeWriteEnd::close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_pReadEnd)
	{
		m_pReadEnd->onWriteEndClosed();
		m_pReadEnd = nullptr;
	}
}

EStatus PipeWriteEnd::write(const char *buffer, size_t bufferSize, size_t *pWritten)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (!m_pReadEnd)
	{
		if (pWritten)
		{
			(*pWritten) = 0;
		}

		return EStatus::INVALID_ARGUMENT;
	}

	size_t length = m_pReadEnd->push(buffer, bufferSize);

	if (pWritten)
	{
		(*pWritten) = length;
	}

	return EStatus::SUCCESS;
}
