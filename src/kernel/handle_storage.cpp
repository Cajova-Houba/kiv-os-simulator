#include "handle_storage.h"

void HandleStorage::removeRef(HandleID id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_handles.find(id);
	if (it == m_handles.end())
	{
		return;
	}

	it->second.refCount--;

	if (it->second.refCount == 0)
	{
		m_handles.erase(it);
	}
}

HandleReference HandleStorage::addHandle(std::unique_ptr<IHandle> && handle)
{
	if (!handle)
	{
		return HandleReference();
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_handles.size() == MAX_HANDLE_COUNT)
	{
		return HandleReference();
	}

	do
	{
		m_lastID++;
	}
	while (m_lastID == 0 || m_handles.find(m_lastID) != m_handles.end());

	const HandleID id = m_lastID;

	HandleData & data = m_handles[id];
	data.handle = std::move(handle);
	data.refCount = 1;

	return HandleReference(id, data.handle.get());
}

HandleReference HandleStorage::getHandle(HandleID id)
{
	if (!id)
	{
		return HandleReference();
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_handles.find(id);
	if (it == m_handles.end())
	{
		return HandleReference();
	}

	it->second.refCount++;

	return HandleReference(it->first, it->second.handle.get());
}

HandleReference HandleStorage::getHandleOfType(HandleID id, EHandle type)
{
	if (!id)
	{
		return HandleReference();
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_handles.find(id);
	if (it == m_handles.end() || it->second.handle->getHandleType() != type)
	{
		return HandleReference();
	}

	it->second.refCount++;

	return HandleReference(it->first, it->second.handle.get());
}

bool HandleStorage::hasHandle(HandleID id)
{
	if (!id)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_handles.find(id);
	if (it == m_handles.end())
	{
		return false;
	}

	return true;
}

bool HandleStorage::hasHandleOfType(HandleID id, EHandle type)
{
	if (!id)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_handles.find(id);
	if (it == m_handles.end() || it->second.handle->getHandleType() != type)
	{
		return false;
	}

	return true;
}

size_t HandleStorage::getHandleCount()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	return m_handles.size();
}
