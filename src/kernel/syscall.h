#pragma once

#include "../api/api.h"

#include "status.h"

namespace SysCall
{
	// vstupní bod systémových volání
	void __stdcall Entry(kiv_hal::TRegisters & context);   // syscall.cpp

	EStatus HandleIO(kiv_hal::TRegisters & context);       // syscall_io.cpp
	EStatus HandleProcess(kiv_hal::TRegisters & context);  // syscall_process.cpp
}
