#pragma once

#include <string>

class DLL
{
	void *m_handle = nullptr;

	void *getSymbolAddress(const char *name) const;
	void unload();

public:
	DLL() = default;

	DLL(const DLL &) = delete;

	DLL(DLL && other)
	: m_handle(other.m_handle)
	{
		other.m_handle = nullptr;
	}

	DLL & operator=(const DLL &) = delete;

	DLL & operator=(DLL && other)
	{
		if (this != &other)
		{
			release();

			m_handle = other.m_handle;

			other.m_handle = nullptr;
		}

		return *this;
	}

	~DLL()
	{
		release();
	}

	bool load(const char *file);

	bool load(const std::string & file)
	{
		return load(file.c_str());
	}

	bool isLoaded() const
	{
		return m_handle != nullptr;
	}

	explicit operator bool() const
	{
		return isLoaded();
	}

	template<class T>
	T getSymbol(const char *name) const
	{
		return reinterpret_cast<T>(getSymbolAddress(name));
	}

	template<class T>
	T getSymbol(const std::string & name) const
	{
		return reinterpret_cast<T>(getSymbolAddress(name.c_str()));
	}

	void release()
	{
		if (m_handle)
		{
			unload();
		}
	}
};
