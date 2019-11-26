#pragma once

#include <map>
#include <mutex>
#include <memory>
#include <vector>

#include "handle_reference.h"

class HandleStorage
{
	struct HandleData
	{
		std::unique_ptr<IHandle> handle;
		uint32_t refCount;
	};

	std::map<HandleID, HandleData> m_handles;
	std::mutex m_mutex;
	HandleID m_lastID = 0;

	void removeRef(HandleID id);

	friend class HandleReference;

public:
	HandleStorage() = default;

	HandleReference addHandle(std::unique_ptr<IHandle> && handle);

	HandleReference getHandle(HandleID id);
	HandleReference getHandleOfType(HandleID id, EHandle type);

	bool hasHandle(HandleID id);
	bool hasHandleOfType(HandleID id, EHandle type);

	template<class Predicate>
	std::vector<HandleReference> getHandles(Predicate predicate)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		std::vector<HandleReference> result;
		for (auto it = m_handles.begin(); it != m_handles.end(); ++it)
		{
			const HandleID id = it->first;
			const IHandle *pHandle = it->second.handle.get();

			if (predicate(id, pHandle))
			{
				it->second.refCount++;
				result.emplace_back(id, it->second.handle.get());
			}
		}

		return result;
	}

	size_t getHandleCount();
};
