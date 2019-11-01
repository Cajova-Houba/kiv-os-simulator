#include <new>

#include "rtl.h"

// data předaná nově vytvořenému vláknu
struct ThreadEntryData
{
	RTL::ThreadMain mainFunc = nullptr;
	void *param = nullptr;

	// informace o procesu ve kterém je nově vytvořené vlákno spuštěno
	RTL::ProcessEnvironment process;
};


static thread_local RTL::ThreadEnvironment g_threadEnv;


// vstupní bod pro všechna nově vytvořená vlákna kromě hlavního vlákna každého procesu
static size_t __stdcall ThreadEntry(const kiv_hal::TRegisters & context)
{
	RTL::ThreadMain threadMain;
	void *param;

	{
		ThreadEntryData *pData = reinterpret_cast<ThreadEntryData*>(context.rdi.r);

		threadMain = pData->mainFunc;
		param = pData->param;

		// nastavení informací o procesu
		g_threadEnv.process = pData->process;

		delete pData;
	}

	// spuštění main funkce nového vlákna
	int exitCode = threadMain(param);

	RTL::SetCurrentThreadExitCode(exitCode);

	return 0;
}

// vstupní bod obsluhy signálu
static size_t __stdcall SignalEntry(const kiv_hal::TRegisters & context)
{
	kiv_os::NSignal_Id signal = static_cast<kiv_os::NSignal_Id>(context.rcx.l);

	if (g_threadEnv.signalHandler)
	{
		// spuštění handleru nastaveného pomocí RTL::SetupSignals
		g_threadEnv.signalHandler(signal);
	}

	return 0;
}

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

static void ConfigureSignal(kiv_os::NSignal_Id signal, uint32_t signalMask)
{
	const uint8_t signalNumber = static_cast<uint8_t>(signal);
	const bool isActive = (0x1 << signalNumber) & signalMask;

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Register_Signal_Handler);
	registers.rcx.l = signalNumber;
	registers.rdx.r = (isActive) ? reinterpret_cast<uint64_t>(SignalEntry) : 0;

	SysCall(registers);
}


// ========================
// ==  Procesy a vlákna  ==
// ========================

kiv_os::THandle RTL::CreateProcess(const char *program, const RTL::ProcessEnvironment & env)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Clone);
	registers.rcx.l = static_cast<uint8_t>(kiv_os::NClone::Create_Process);
	registers.rdx.r = reinterpret_cast<uint64_t>(program);
	registers.rdi.r = reinterpret_cast<uint64_t>(env.cmdLine);
	registers.rbx.e = (env.stdIn << 16) | env.stdOut;

	if (!SysCall(registers))
	{
		return 0;
	}

	return registers.rax.x;
}

kiv_os::THandle RTL::CreateThread(RTL::ThreadMain mainFunc, void *param)
{
	ThreadEntryData *pData = new (std::nothrow) ThreadEntryData;
	if (!pData)
	{
		SetLastError(kiv_os::NOS_Error::Out_Of_Memory);
		return 0;
	}

	pData->mainFunc = mainFunc;
	pData->param = param;

	pData->process = g_threadEnv.process;

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Clone);
	registers.rcx.l = static_cast<uint8_t>(kiv_os::NClone::Create_Thread);
	registers.rdx.r = reinterpret_cast<uint64_t>(ThreadEntry);
	registers.rdi.r = reinterpret_cast<uint64_t>(pData);

	if (!SysCall(registers))
	{
		return 0;
	}

	return registers.rax.x;
}

int RTL::WaitFor(const kiv_os::THandle *handles, uint16_t count)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Wait_For);
	registers.rdx.r = reinterpret_cast<uint64_t>(handles);
	registers.rcx.x = count;

	if (!SysCall(registers))
	{
		return -1;
	}

	return registers.rax.x;
}

int RTL::GetExitCode(kiv_os::THandle handle)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Read_Exit_Code);
	registers.rdx.x = static_cast<uint16_t>(handle);

	if (!SysCall(registers))
	{
		return 0;
	}

	return registers.rcx.x;
}

void RTL::SetupSignals(RTL::SignalHandler handler, uint32_t signalMask)
{
	if (handler == nullptr || signalMask == 0)
	{
		handler = nullptr;
		signalMask = 0;
	}

	g_threadEnv.signalHandler = handler;
	g_threadEnv.signalMask = signalMask;

	// aktualizace nastavení všech signálů
	ConfigureSignal(kiv_os::NSignal_Id::Terminate, signalMask);  // API definuje pouze jeden signál
}

RTL::SignalHandler RTL::GetSignalHandler()
{
	return g_threadEnv.signalHandler;
}

uint32_t RTL::GetSignalMask()
{
	return g_threadEnv.signalMask;
}

const char *RTL::GetProcessCmdLine()
{
	return g_threadEnv.process.cmdLine;
}

kiv_os::THandle RTL::GetStdInHandle()
{
	return g_threadEnv.process.stdIn;
}

kiv_os::THandle RTL::GetStdOutHandle()
{
	return g_threadEnv.process.stdOut;
}

std::string RTL::GetWorkingDirectory()
{
	char buffer[4096];

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Get_Working_Dir);
	registers.rdx.r = reinterpret_cast<uint64_t>(buffer);
	registers.rcx.r = sizeof buffer;

	if (!SysCall(registers))
	{
		return std::string();
	}

	const size_t length = registers.rax.r;

	return std::string(buffer, length);
}

bool RTL::SetWorkingDirectory(const char *path)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Set_Working_Dir);
	registers.rdx.r = reinterpret_cast<uint64_t>(path);

	if (!SysCall(registers))
	{
		return false;
	}

	return true;
}

RTL::ThreadEnvironment *RTL::GetCurrentThreadEnv()
{
	return &g_threadEnv;
}

void RTL::SetCurrentThreadExitCode(int exitCode)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Exit);
	registers.rcx.x = static_cast<uint16_t>(exitCode);

	SysCall(registers);
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

std::string RTL::ErrorToString(kiv_os::NOS_Error error)
{
	switch (error)
	{
		case kiv_os::NOS_Error::Success:               return "Vse v poradku";
		case kiv_os::NOS_Error::Invalid_Argument:      return "Neplatny parametr";
		case kiv_os::NOS_Error::File_Not_Found:        return "Soubor nenalezen";
		case kiv_os::NOS_Error::Directory_Not_Empty:   return "Adresar neni prazdny";
		case kiv_os::NOS_Error::Not_Enough_Disk_Space: return "Nedostatek mista na disku";
		case kiv_os::NOS_Error::Out_Of_Memory:         return "Nedostatek pameti";
		case kiv_os::NOS_Error::Permission_Denied:     return "Pristup odepren";
		case kiv_os::NOS_Error::IO_Error:              return "Chyba IO";
		case kiv_os::NOS_Error::Unknown_Error:         break;
	}

	return "Neznama chyba";
}


// =========================================
// ==  Soubory a standardní vstup/výstup  ==
// =========================================

kiv_os::THandle RTL::OpenFile(const char *path, kiv_os::NOpen_File flags, kiv_os::NFile_Attributes attributes)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Open_File);
	registers.rcx.l = static_cast<uint8_t>(flags);
	registers.rdi.i = static_cast<uint16_t>(attributes);
	registers.rdx.r = reinterpret_cast<uint64_t>(path);

	if (!SysCall(registers))
	{
		return 0;
	}

	return registers.rax.x;
}

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

bool RTL::SeekFile(kiv_os::THandle file, kiv_os::NFile_Seek action, kiv_os::NFile_Seek base, int64_t & pos)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Seek);
	registers.rcx.h = static_cast<uint8_t>(action);
	registers.rcx.l = static_cast<uint8_t>(base);
	registers.rdx.x = file;
	registers.rdi.r = pos;

	if (!SysCall(registers))
	{
		return false;
	}

	if (action == kiv_os::NFile_Seek::Get_Position)
	{
		pos = registers.rax.r;
	}

	return true;
}

bool RTL::DeleteFile(const char *path)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Close_Handle);
	registers.rdx.r = reinterpret_cast<uint64_t>(path);

	if (!SysCall(registers))
	{
		return false;
	}

	return true;
}


// ===============
// ==  Ostatní  ==
// ===============

RTL::Pipe RTL::CreatePipe()
{
	kiv_os::THandle handles[2];

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Create_Pipe);
	registers.rdx.r = reinterpret_cast<uint64_t>(handles);

	if (!SysCall(registers))
	{
		return Pipe();
	}

	Pipe pipe;
	pipe.readEnd  = handles[1];
	pipe.writeEnd = handles[0];

	return pipe;
}

bool RTL::CloseHandle(kiv_os::THandle handle)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Close_Handle);
	registers.rdx.x = handle;

	if (!SysCall(registers))
	{
		return false;
	}

	return true;
}

void RTL::Shutdown()
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Shutdown);

	SysCall(registers);
}



kiv_os::NOS_Error kiv_os_rtl::Last_Error;  // NEPOUŽÍVAT
