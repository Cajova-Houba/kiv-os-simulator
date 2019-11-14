// toto by měl být jediný platform-specific kód v celém kernelu :)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "dll.h"

void *DLL::getSymbolAddress(const char *name) const
{
	if (!m_handle)
	{
		return nullptr;
	}

	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
}

void DLL::unload()
{
	FreeLibrary(static_cast<HMODULE>(m_handle));

	m_handle = nullptr;
}

bool DLL::load(const char *file)
{
	release();

	if (!file)
	{
		return false;
	}

	m_handle = LoadLibraryA(file);

	if (!m_handle)
	{
		return false;
	}

	return true;
}
