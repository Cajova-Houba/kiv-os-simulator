#include <condition_variable>

#include "event_system.h"
#include "process.h"
#include "thread.h"

static int CheckHandle(const HandleReference & handle, int events)
{
	switch (handle->getHandleType())
	{
		case EHandle::THREAD:
		{
			if (handle.as<Thread>()->isRunning())
			{
				if (events & Event::THREAD_START)
				{
					return 1;
				}
			}
			else
			{
				if (events & Event::THREAD_END)
				{
					return 1;
				}
			}
			break;
		}
		case EHandle::PROCESS:
		{
			if (handle.as<Process>()->isRunning())
			{
				if (events & Event::PROCESS_START)
				{
					return 1;
				}
			}
			else
			{
				if (events & Event::PROCESS_END)
				{
					return 1;
				}
			}
			break;
		}
		default:
		{
			return -1;  // na tento typ handle se nedá čekat
		}
	}

	return 0;  // je potřeba čekat
}

static bool ValidateHandles(const HandleID *handles, uint16_t handleCount, int events, Process *pCurrentProcess, int & result)
{
	return pCurrentProcess->forEachHandle(handles, handleCount,
		[&](HandleID, const HandleReference & handle, size_t index) -> bool
		{
			int status = CheckHandle(handle, events);
			if (status != 0)
			{
				if (status > 0)
				{
					result = static_cast<int>(index);
				}

				return false;  // konec validace
			}

			return true;  // pokračujeme ve validaci
		}
	);
}

struct EventSystem::WaitInfo
{
	int events;
	int signaledIndex;
	const HandleID *handles;
	uint16_t handleCount;
	std::condition_variable cv;
};

EStatus EventSystem::waitForMultiple(const HandleID *handles, uint16_t handleCount, int events, uint16_t & result)
{
	if (!events)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	// během kontroly jednotlivých handle je potřeba pozdržet všechny příchozí události, aby se předešlo race condition
	std::unique_lock<std::mutex> lock(m_mutex);

	int validateResult = -1;

	if (!ValidateHandles(handles, handleCount, events, pCurrentProcess, validateResult))
	{
		if (validateResult < 0)
		{
			// nějaký handle neexistuje nebo k němu aktuální proces nemá přístup nebo se na něj nedá čekat
			return EStatus::INVALID_ARGUMENT;
		}
		else
		{
			// na nějaký handle už není potřeba čekat, protože na něm už došlo k některé z požadovaných událostí
			result = static_cast<uint16_t>(validateResult);
			return EStatus::SUCCESS;
		}
	}

	WaitInfo info;
	info.events = events;
	info.signaledIndex = -1;
	info.handles = handles;
	info.handleCount = handleCount;

	m_waiting.push_back(&info);

	// čekání
	while (info.signaledIndex < 0)
	{
		info.cv.wait(lock);
	}

	result = static_cast<uint16_t>(info.signaledIndex);

	return EStatus::SUCCESS;
}

void EventSystem::dispatchEvent(int event, HandleID handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	for (WaitInfo *pInfo : m_waiting)
	{
		if (pInfo->events & event)
		{
			for (uint16_t i = 0; i < pInfo->handleCount; i++)
			{
				if (pInfo->handles[i] == handle)
				{
					pInfo->signaledIndex = i;
					pInfo->cv.notify_one();
					break;
				}
			}
		}
	}
}
