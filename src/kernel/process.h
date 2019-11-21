#pragma once

#include <mutex>
#include <atomic>
#include <string>
#include <set>

#include "handle_reference.h"
#include "thread.h"
#include "path.h"

class Process : public IHandle
{
	std::set<HandleReference> m_handles;
	std::atomic<uint16_t> m_threadCount;
	HandleID m_mainThreadID;
	Path m_currentDirectory;
	std::string m_name;
	std::string m_cmdLine;
	std::mutex m_mutex;

	uint16_t incrementThreadCount()
	{
		return ++m_threadCount;
	}

	uint16_t decrementThreadCount()
	{
		return --m_threadCount;
	}

	friend class Thread;

public:
	Process() = default;

	EHandle getHandleType() const override
	{
		return EHandle::PROCESS;
	}

	uint16_t getThreadCount() const
	{
		return m_threadCount.load(std::memory_order_relaxed);
	}

	bool isRunning() const
	{
		return getThreadCount() > 0;
	}

	const std::string & getName() const
	{
		return m_name;  // nikdy se nemění, takže není potřeba synchronizace
	}

	const std::string & getCmdLine() const
	{
		return m_cmdLine;  // nikdy se nemění, takže není potřeba synchronizace
	}

	Path getWorkingDirectory()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		return m_currentDirectory;
	}

	std::string getWorkingDirectoryString()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		return m_currentDirectory.toString();
	}

	size_t getWorkingDirectoryStringBuffer(char *buffer, size_t bufferSize)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		return m_currentDirectory.toStringBuffer(buffer, bufferSize);
	}

	void setWorkingDirectory(Path && path)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		m_currentDirectory = std::move(path);
	}

	void makePathAbsolute(Path & path)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		path.makeAbsolute(m_currentDirectory);
	}

	HandleReference getMainThread();

	HandleReference getHandle(HandleID id);
	HandleReference getHandleOfType(HandleID id, EHandle type);

	bool hasHandle(HandleID id);
	bool hasHandleOfType(HandleID id, EHandle type);

	template<class Callback>
	bool forEachHandle(const HandleID *handles, size_t handleCount, Callback callback)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		for (size_t i = 0; i < handleCount; i++)
		{
			const HandleID id = handles[i];

			HandleReference key(id, nullptr);
			auto it = m_handles.find(key);
			if (it == m_handles.end())
			{
				return false;
			}

			const HandleReference & handleRef = *it;

			if (!callback(id, handleRef, i))
			{
				return false;
			}
		}

		return true;
	}

	void addHandle(HandleReference && handle);
	void removeHandle(HandleID id);

	// vytvoří nový proces
	static HandleReference Create(const char *name, const char *cmdLine, Path && path, TEntryFunc entry,
	                              HandleReference && stdIn, HandleReference && stdOut, bool useCurrentThread = false);
};
