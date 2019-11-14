#pragma once

#include <mutex>
#include <atomic>
#include <string>
#include <set>
#include <vector>

#include "../api/api.h"

#include "handle_reference.h"

class Process : public IHandle
{
	std::set<HandleReference> m_handles;
	std::atomic<uint16_t> m_threadCount;
	HandleID m_mainThreadID;
	std::string m_currentDirectory;
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

	const std::string & getCmdLine() const
	{
		return m_cmdLine;  // nikdy se nemění, takže není potřeba synchronizace
	}

	void setWorkingDirectory(const char *path);

	void setWorkingDirectory(const std::string & path)
	{
		setWorkingDirectory(path.c_str());
	}

	size_t getWorkingDirectory(char *buffer, size_t bufferSize);

	std::string getWorkingDirectory()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		return m_currentDirectory;
	}

	HandleReference getMainThread();

	HandleReference getHandle(HandleID id);

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

	template<class Callback>
	bool forEachHandle(const std::vector<HandleID> & handles, Callback callback)
	{
		return forEachHandle(handles.data(), handles.size(), callback);
	}

	void addHandle(HandleReference && handle);
	void removeHandle(HandleID id);

	// vytvoří nový proces
	static HandleReference Create(kiv_os::TThread_Proc entry, const char *cmdLine, const char *path,
	                              HandleReference && stdIn, HandleReference && stdOut, bool useCurrentThread = false);
};
