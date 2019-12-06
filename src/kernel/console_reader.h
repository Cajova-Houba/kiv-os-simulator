#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ConsoleReader
{
	std::atomic<bool> m_isOpen;
	std::atomic<unsigned int> m_readerCount;
	std::queue<std::string> m_lineQueue;
	std::condition_variable m_readerCV;
	std::condition_variable m_workerCV;
	std::mutex m_mutex;

	ConsoleReader() = default;

	void setOpen(bool isOpen)
	{
		m_isOpen.store(isOpen, std::memory_order_relaxed);
	}

	void workerLoop();
	bool waitForReader();

	friend class ReaderCountGuard;

public:
	bool isOpen() const
	{
		return m_isOpen.load(std::memory_order_relaxed);
	}

	unsigned int getReaderCount() const
	{
		return m_readerCount.load(std::memory_order_relaxed);
	}

	void readLine(char *buffer, size_t bufferSize, size_t *pRead);

	void pushLine(std::string && line);

	void close();

	static ConsoleReader & GetInstance();
};
