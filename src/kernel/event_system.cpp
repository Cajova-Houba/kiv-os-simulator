#include <condition_variable>

#include "event_system.h"
#include "process.h"

static bool ValidateHandles(const HandleID *handles, uint16_t handleCount, int events, int & result)
{
	auto callback = [&](HandleID, const HandleReference & handle, size_t index) -> bool
	{
		int currentEvent = 0;

		switch (handle->getHandleType())
		{
			case EHandle::THREAD:
			{
				currentEvent = (handle.as<Thread>()->isRunning()) ? Event::THREAD_START : Event::THREAD_END;
				break;
			}
			case EHandle::PROCESS:
			{
				currentEvent = (handle.as<Process>()->isRunning()) ? Event::PROCESS_START : Event::PROCESS_END;
				break;
			}
			default:
			{
				// na tento typ handle se nedá čekat
				// konec validace
				return false;
			}
		}

		if (currentEvent & events)
		{
			// na daném handle už došlo k některé z požadovaných událostí
			result = static_cast<int>(index);
			// konec validace
			return false;
		}

		// pokračujeme ve validaci
		return true;
	};

	return Thread::GetProcess().forEachHandle(handles, handleCount, callback);
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

	// během kontroly jednotlivých handle je potřeba pozdržet všechny příchozí události, aby se předešlo race condition
	std::unique_lock<std::mutex> lock(m_mutex);

	int validateResult = -1;
	if (!ValidateHandles(handles, handleCount, events, validateResult))
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
