#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

class ConsoleReader
{
	std::queue<std::string> m_lineQueue;
	std::condition_variable m_cv;
	std::mutex m_mutex;

	ConsoleReader() = default;

public:
	void readLine(char *buffer, size_t bufferSize, size_t *pRead);

	void pushLine(std::string && line);

	void close();

	static ConsoleReader & GetInstance();
};
