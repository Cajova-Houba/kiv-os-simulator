#pragma once

#include <mutex>
#include <vector>

#include "handle.h"

namespace Event
{
	enum
	{
		THREAD_START  = (1 << 0),
		THREAD_END    = (1 << 1),
		PROCESS_START = (1 << 2),
		PROCESS_END   = (1 << 3)
	};
}

class EventSystem
{
	struct WaitInfo;

	std::mutex m_mutex;
	std::vector<WaitInfo*> m_waiting;

public:
	EventSystem() = default;

	// uspí aktuální vlákno, dokud nenastane nějaká událost na některém ze zadaných handle
	// result je výsledný index handle, na kterém došlo k nějaké události
	EStatus waitForMultiple(const HandleID *handles, uint16_t handleCount, int events, uint16_t & result);

	// uspí aktuální vlákno, dokud nenastane nějaká událost na daném handle
	EStatus waitForSingle(HandleID handle, int events)
	{
		uint16_t result;
		return waitForMultiple(&handle, 1, events, result);
	}

	// vyvolá událost
	void dispatchEvent(int event, HandleID handle);
};
