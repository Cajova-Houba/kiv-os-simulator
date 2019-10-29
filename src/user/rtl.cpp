#include "rtl.h"

static thread_local ThreadEnvironment g_threadEnv;


static bool SysCall(kiv_hal::TRegisters & context)
{
	kiv_os::Sys_Call(context);

	if (context.flags.carry)
	{
		RTL::SetLastError(static_cast<kiv_os::NOS_Error>(context.rax.r));
		return false;
	}

	RTL::SetLastError(kiv_os::NOS_Error::Success);

	return true;
}


// ========================
// ==  Procesy a vlákna  ==
// ========================

const char *RTL::GetProcessCmdLine()
{
	return g_threadEnv.processCmdLine;
}

kiv_os::THandle RTL::GetStdInHandle()
{
	return g_threadEnv.stdIn;
}

kiv_os::THandle RTL::GetStdOutHandle()
{
	return g_threadEnv.stdOut;
}

ThreadEnvironment *RTL::GetCurrentThreadEnv()
{
	return &g_threadEnv;
}


// ====================
// ==  Chybové kódy  ==
// ====================

kiv_os::NOS_Error RTL::GetLastError()
{
	return g_threadEnv.lastError;
}

void RTL::SetLastError(kiv_os::NOS_Error error)
{
	g_threadEnv.lastError = error;
}


// =========================================
// ==  Soubory a standardní vstup/výstup  ==
// =========================================

bool RTL::ReadFile(kiv_os::THandle file, char *buffer, size_t size, size_t *pRead)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Read_File);
	registers.rdx.x = file;
	registers.rdi.r = reinterpret_cast<uint64_t>(buffer);
	registers.rcx.r = size;

	if (!SysCall(registers))
	{
		return false;
	}

	if (pRead)
	{
		(*pRead) = registers.rax.r;
	}

	return true;
}

bool RTL::WriteFile(kiv_os::THandle file, const char *buffer, size_t size, size_t *pWritten)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Write_File);
	registers.rdx.x = file;
	registers.rdi.r = reinterpret_cast<uint64_t>(buffer);
	registers.rcx.r = size;

	if (!SysCall(registers))
	{
		return false;
	}

	if (pWritten)
	{
		(*pWritten) = registers.rax.r;
	}

	return true;
}



kiv_os::NOS_Error kiv_os_rtl::Last_Error;  // NEPOUŽÍVAT
