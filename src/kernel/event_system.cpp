#include <condition_variable>

#include "event_system.h"
#include "process.h"

class EventSystem::WaitDescriptor
{
	int m_events;
	int m_signaledIndex;
	const HandleID *m_handles;
	uint16_t m_handleCount;
	std::vector<WaitDescriptor*> *m_pContainer;
	std::condition_variable m_cv;

public:
	WaitDescriptor(int events, const HandleID *handles, uint16_t handleCount, std::vector<WaitDescriptor*> & container)
	: m_events(events),
	  m_signaledIndex(-1),
	  m_handles(handles),
	  m_handleCount(handleCount),
	  m_pContainer(&container),
	  m_cv()
	{
		m_pContainer->push_back(this);
	}

	~WaitDescriptor()
	{
		for (auto it = m_pContainer->begin(); it != m_pContainer->end();)
		{
			if (*it == this)
			{
				it = m_pContainer->erase(it);
				break;
			}
			else
			{
				++it;
			}
		}
	}

	void wait(std::unique_lock<std::mutex> & lock)
	{
		while (m_signaledIndex < 0)
		{
			m_cv.wait(lock);
		}
	}

	void onEvent(int event, HandleID handle)
	{
		if (m_signaledIndex < 0 && m_events & event)
		{
			for (uint16_t i = 0; i < m_handleCount; i++)
			{
				if (m_handles[i] == handle)
				{
					m_signaledIndex = i;
					m_cv.notify_one();
					break;
				}
			}
		}
	}

	uint16_t getSignaledIndex() const
	{
		return static_cast<uint16_t>(m_signaledIndex);
	}
};

static bool ValidateHandles(const HandleID *handles, uint16_t handleCount, int events, int & result)
{
	auto callback = [&](const HandleReference & handle, size_t index) -> bool
	{
		int currentState = 0;

		switch (handle->getHandleType())
		{
			case EHandle::THREAD:
			{
				currentState = handle.as<Thread>()->isRunning() ? Event::THREAD_START : Event::THREAD_END;
				break;
			}
			case EHandle::PROCESS:
			{
				currentState = handle.as<Process>()->isRunning() ? Event::PROCESS_START : Event::PROCESS_END;
				break;
			}
			default:
			{
				// na tento typ handle se nedá čekat
				// konec validace
				return false;
			}
		}

		if (currentState & events)
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

	WaitDescriptor descriptor(events, handles, handleCount, m_waiting);

	// čekání na událost
	descriptor.wait(lock);

	result = descriptor.getSignaledIndex();

	return EStatus::SUCCESS;
}

void EventSystem::dispatchEvent(int event, HandleID handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	for (WaitDescriptor *pDescriptor : m_waiting)
	{
		pDescriptor->onEvent(event, handle);
	}
}
