#pragma once

#include "handle.h"

class HandleReference
{
	HandleID m_id = 0;
	IHandle *m_pHandle = nullptr;

	void destroy() noexcept;

public:
	HandleReference() = default;

	HandleReference(HandleID id, IHandle *pHandle)
	: m_id(id),
	  m_pHandle(pHandle)
	{
	}

	HandleReference(const HandleReference &) = delete;

	HandleReference(HandleReference && other)
	: m_id(other.m_id),
	  m_pHandle(other.m_pHandle)
	{
		other.m_id = 0;
		other.m_pHandle = nullptr;
	}

	HandleReference & operator=(const HandleReference &) = delete;

	HandleReference & operator=(HandleReference && other)
	{
		if (this != &other)
		{
			release();

			m_id = other.m_id;
			m_pHandle = other.m_pHandle;

			other.m_id = 0;
			other.m_pHandle = nullptr;
		}

		return *this;
	}

	~HandleReference()
	{
		release();
	}

	void release()
	{
		if (isValid())
		{
			destroy();
		}

		m_id = 0;
		m_pHandle = nullptr;
	}

	bool isValid() const
	{
		return m_id && m_pHandle;
	}

	explicit operator bool() const
	{
		return isValid();
	}

	HandleID getID() const
	{
		return m_id;
	}

	IHandle *get() const
	{
		return m_pHandle;
	}

	template<class T>
	T *as() const
	{
		return static_cast<T*>(m_pHandle);
	}

	IHandle *operator->() const
	{
		return m_pHandle;
	}
};

inline bool operator==(const HandleReference & a, const HandleReference & b)
{
	return a.getID() == b.getID();
}

inline bool operator!=(const HandleReference & a, const HandleReference & b)
{
	return a.getID() != b.getID();
}

inline bool operator<(const HandleReference & a, const HandleReference & b)
{
	return a.getID() < b.getID();
}

inline bool operator>(const HandleReference & a, const HandleReference & b)
{
	return a.getID() > b.getID();
}

inline bool operator<=(const HandleReference & a, const HandleReference & b)
{
	return a.getID() <= b.getID();
}

inline bool operator>=(const HandleReference & a, const HandleReference & b)
{
	return a.getID() >= b.getID();
}
