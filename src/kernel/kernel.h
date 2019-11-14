#pragma once

#include <cstdarg>

#include "../api/api.h"

#include "dll.h"
#include "handle_storage.h"
#include "event_system.h"
#include "console.h"
#include "compiler.h"

class Kernel
{
	DLL m_userDLL;
	HandleStorage m_handleStorage;
	EventSystem m_eventSystem;
	HandleReference m_consoleHandle;

	static Kernel *s_pInstance;

public:
	Kernel()
	: m_userDLL(),
	  m_handleStorage(),
	  m_eventSystem(),
	  m_consoleHandle()
	{
		s_pInstance = this;

		m_consoleHandle = m_handleStorage.addHandle(std::make_unique<Console>());
	}

	static HandleStorage & GetHandleStorage()
	{
		return s_pInstance->m_handleStorage;
	}

	static EventSystem & GetEventSystem()
	{
		return s_pInstance->m_eventSystem;
	}

	static DLL & GetUserDLL()
	{
		return s_pInstance->m_userDLL;
	}

	static HandleReference & GetConsoleHandle()
	{
		return s_pInstance->m_consoleHandle;
	}

	static void Log(const char *format, ...) COMPILER_PRINTF_ARGS_CHECK(1,2)
	{
		va_list args;
		va_start(args, format);
		s_pInstance->m_consoleHandle.as<Console>()->logV(format, args);
		va_end(args);
	}
};

// vstupní funkce jádra
void __stdcall Bootstrap_Loader(kiv_hal::TRegisters &);
