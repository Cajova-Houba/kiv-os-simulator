#include <new>

#include "rtl.h"

static thread_local RTL::ThreadEnvironment *g_pThreadEnv;

// vstupní bod pro všechna nově vytvořená vlákna kromě hlavního vlákna každého procesu
static size_t __stdcall ThreadEntry(const kiv_hal::TRegisters & context)
{
	RTL::ThreadEnvironmentGuard environment(reinterpret_cast<RTL::ThreadEnvironment*>(context.rdi.r));

	RTL::ThreadMain threadMain = g_pThreadEnv->mainFunc;
	void *param = g_pThreadEnv->param;

	// spuštění main funkce nového vlákna
	int exitCode = threadMain(param);

	RTL::SetCurrentThreadExitCode(exitCode);

	return 0;
}

// vstupní bod obsluhy signálu
static size_t __stdcall SignalEntry(const kiv_hal::TRegisters & context)
{
	RTL::Signal signal = static_cast<RTL::Signal>(context.rcx.l);

	RTL::SignalHandler handler = g_pThreadEnv->signalHandler;
	if (handler)
	{
		// spuštění handleru nastaveného pomocí RTL::SetupSignals
		handler(signal);
	}

	return 0;
}

static bool SysCall(kiv_hal::TRegisters & context)
{
	kiv_os::Sys_Call(context);

	if (context.flags.carry)
	{
		RTL::SetLastError(static_cast<RTL::Error>(context.rax.r));
		return false;
	}

	RTL::SetLastError(RTL::Error::SUCCESS);

	return true;
}

static void ConfigureSignal(RTL::Signal signal, uint32_t signalMask)
{
	const uint8_t signalNumber = static_cast<uint8_t>(signal);
	const bool isActive = (0x1 << (signalNumber - 1)) & signalMask;

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Register_Signal_Handler);
	registers.rcx.l = signalNumber;
	registers.rdx.r = (isActive) ? reinterpret_cast<uint64_t>(SignalEntry) : 0;

	SysCall(registers);
}

static RTL::Handle OpenFileHandle(const char *path, uint8_t flags, uint8_t attributes)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Open_File);
	registers.rdx.r = reinterpret_cast<uint64_t>(path);
	registers.rcx.l = flags;
	registers.rdi.i = attributes;

	if (!SysCall(registers))
	{
		return 0;
	}

	return registers.rax.x;
}

static bool SeekFile(RTL::Handle file, kiv_os::NFile_Seek command, int64_t & pos, RTL::Position base)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Seek);
	registers.rcx.h = static_cast<uint8_t>(command);
	registers.rdx.x = file;
	registers.rdi.r = pos;

	switch (base)
	{
		case RTL::Position::BEGIN:
		{
			registers.rcx.l = static_cast<uint8_t>(kiv_os::NFile_Seek::Beginning);
			break;
		}
		case RTL::Position::CURRENT:
		{
			registers.rcx.l = static_cast<uint8_t>(kiv_os::NFile_Seek::Current);
			break;
		}
		case RTL::Position::END:
		{
			registers.rcx.l = static_cast<uint8_t>(kiv_os::NFile_Seek::End);
			break;
		}
	}

	if (!SysCall(registers))
	{
		return false;
	}

	if (command == kiv_os::NFile_Seek::Get_Position)
	{
		pos = registers.rax.r;
	}

	return true;
}


// ============================================================================

bool RTL::CloseHandle(RTL::Handle handle)
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

RTL::ThreadEnvironmentGuard::ThreadEnvironmentGuard(Handle stdIn, Handle stdOut, const char *cmdLine)
{
	g_pThreadEnv = new ThreadEnvironment;

	ProcessEnvironment & env = g_pThreadEnv->process;
	env.stdIn = stdIn;
	env.stdOut = stdOut;
	env.cmdLine = cmdLine;
}

RTL::ThreadEnvironmentGuard::ThreadEnvironmentGuard(ThreadEnvironment *pThreadEnv)
{
	g_pThreadEnv = pThreadEnv;
}

RTL::ThreadEnvironmentGuard::~ThreadEnvironmentGuard()
{
	delete g_pThreadEnv;
	g_pThreadEnv = nullptr;
}


// ========================
// ==  Procesy a vlákna  ==
// ========================

RTL::Handle RTL::CreateProcess(const char *program, const RTL::ProcessEnvironment & env)
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

RTL::Handle RTL::CreateThread(RTL::ThreadMain mainFunc, void *param)
{
	ThreadEnvironment *pEnv = new (std::nothrow) ThreadEnvironment;
	if (!pEnv)
	{
		SetLastError(RTL::Error::OUT_OF_MEMORY);
		return 0;
	}

	pEnv->mainFunc = mainFunc;
	pEnv->param = param;

	pEnv->process = g_pThreadEnv->process;

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Clone);
	registers.rcx.l = static_cast<uint8_t>(kiv_os::NClone::Create_Thread);
	registers.rdx.r = reinterpret_cast<uint64_t>(ThreadEntry);
	registers.rdi.r = reinterpret_cast<uint64_t>(pEnv);

	if (!SysCall(registers))
	{
		return 0;
	}

	return registers.rax.x;
}

int RTL::WaitForMultiple(const RTL::Handle *handles, uint16_t count)
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

int RTL::GetExitCode(RTL::Handle handle)
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

	g_pThreadEnv->signalHandler = handler;
	g_pThreadEnv->signalMask = signalMask;

	// aktualizace nastavení všech signálů
	ConfigureSignal(RTL::Signal::TERMINATE, signalMask);  // API definuje pouze jeden signál
}

RTL::SignalHandler RTL::GetSignalHandler()
{
	return g_pThreadEnv->signalHandler;
}

uint32_t RTL::GetSignalMask()
{
	return g_pThreadEnv->signalMask;
}

const char *RTL::GetProcessCmdLine()
{
	return g_pThreadEnv->process.cmdLine;
}

RTL::Handle RTL::GetStdInHandle()
{
	return g_pThreadEnv->process.stdIn;
}

RTL::Handle RTL::GetStdOutHandle()
{
	return g_pThreadEnv->process.stdOut;
}

std::string RTL::GetWorkingDirectory()
{
	char buffer[4096];
	buffer[0] = '\0';

	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Get_Working_Dir);
	registers.rdx.r = reinterpret_cast<uint64_t>(buffer);
	registers.rcx.r = sizeof buffer;

	if (!SysCall(registers))
	{
		return std::string();
	}

	const size_t length = static_cast<size_t>(registers.rax.r);

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
	return g_pThreadEnv;
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

RTL::Error RTL::GetLastError()
{
	return g_pThreadEnv->lastError;
}

void RTL::SetLastError(RTL::Error error)
{
	g_pThreadEnv->lastError = error;
}

std::string RTL::ErrorToString(RTL::Error error)
{
	switch (error)
	{
		case RTL::Error::SUCCESS:               return "Vse v poradku";
		case RTL::Error::INVALID_ARGUMENT:      return "Neplatny parametr";
		case RTL::Error::FILE_NOT_FOUND:        return "Soubor nebo adresar nenalezen";
		case RTL::Error::DIRECTORY_NOT_EMPTY:   return "Adresar neni prazdny";
		case RTL::Error::NOT_ENOUGH_DISK_SPACE: return "Nedostatek mista na disku";
		case RTL::Error::OUT_OF_MEMORY:         return "Nedostatek pameti";
		case RTL::Error::PERMISSION_DENIED:     return "Pristup odepren";
		case RTL::Error::IO_ERROR:              return "Chyba IO";
		case RTL::Error::UNKNOWN_ERROR:         break;
	}

	std::string unknown = "Neznama chyba (";
	unknown += std::to_string(static_cast<uint16_t>(error));
	unknown += ")";

	return unknown;
}


// =========================================
// ==  Soubory a standardní vstup/výstup  ==
// =========================================

RTL::Handle RTL::OpenFile(const char *path, bool readOnly)
{
	uint8_t flags = static_cast<uint8_t>(kiv_os::NOpen_File::fmOpen_Always);
	uint8_t attributes = 0;

	if (readOnly)
	{
		attributes |= FileAttributes::READ_ONLY;
	}

	return OpenFileHandle(path, flags, attributes);
}

RTL::Handle RTL::OpenDirectory(const char *path)
{
	uint8_t flags = static_cast<uint8_t>(kiv_os::NOpen_File::fmOpen_Always);
	uint8_t attributes = 0;

	attributes |= FileAttributes::READ_ONLY;
	attributes |= FileAttributes::DIRECTORY;

	return OpenFileHandle(path, flags, attributes);
}

RTL::Handle RTL::CreateFile(const char *path)
{
	uint8_t flags = 0;
	uint8_t attributes = 0;

	return OpenFileHandle(path, flags, attributes);
}

RTL::Handle RTL::CreateDirectory(const char *path)
{
	uint8_t flags = 0;
	uint8_t attributes = 0;

	attributes |= FileAttributes::DIRECTORY;

	return OpenFileHandle(path, flags, attributes);
}

bool RTL::GetDirectoryContent(RTL::Handle handle, std::vector<RTL::DirectoryEntry> & result)
{
	result.resize(32);

	size_t pos = 0;
	while (true)
	{
		size_t read = 0;
		if (!ReadFile(handle, result.data() + pos, (result.size() - pos) * sizeof (DirectoryEntry), &read))
		{
			return false;
		}

		pos += read / sizeof (DirectoryEntry);

		if (pos < result.size())
		{
			result.resize(pos);
			break;
		}
		else
		{
			result.resize(result.size() * 2);
		}
	}

	return true;
}

bool RTL::ReadFile(kiv_os::THandle file, void *buffer, size_t size, size_t *pRead)
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
		(*pRead) = static_cast<size_t>(registers.rax.r);
	}

	return true;
}

bool RTL::WriteFile(kiv_os::THandle file, const void *buffer, size_t size, size_t *pWritten)
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
		(*pWritten) = static_cast<size_t>(registers.rax.r);
	}

	return true;
}

bool RTL::WriteFileFormatV(RTL::Handle file, size_t *pWritten, const char *format, va_list args)
{
	StringBuffer<4096> buffer;
	buffer.append_vf(format, args);

	return WriteFile(file, buffer.get(), buffer.getLength(), pWritten);
}

bool RTL::GetFilePos(RTL::Handle file, int64_t & result)
{
	return SeekFile(file, kiv_os::NFile_Seek::Get_Position, result, RTL::Position::BEGIN);
}

bool RTL::SetFilePos(RTL::Handle file, int64_t pos, RTL::Position base)
{
	return SeekFile(file, kiv_os::NFile_Seek::Set_Position, pos, base);
}

bool RTL::SetFileSize(RTL::Handle file, int64_t pos, RTL::Position base)
{
	return SeekFile(file, kiv_os::NFile_Seek::Set_Size, pos, base);
}

bool RTL::DeleteFile(const char *path)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::File_System);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Delete_File);
	registers.rdx.r = reinterpret_cast<uint64_t>(path);

	if (!SysCall(registers))
	{
		return false;
	}

	return true;
}

bool RTL::DeleteDirectory(const char *path, bool recursively)
{
	if (recursively)
	{
		Directory dir;
		if (!dir.open(path))
		{
			return false;
		}

		std::string dirPath = path;
		if (!dirPath.empty() && dirPath.back() != '\\')
		{
			dirPath += '\\';
		}

		for (const DirectoryEntry & entry : dir.getContent())
		{
			bool status = true;

			if (entry.attributes & FileAttributes::DIRECTORY)
			{
				// rekurze... není to úplně dobré, ale neměl by s tím být žádný problém
				status = DeleteDirectory(dirPath + entry.name, recursively);
			}
			else
			{
				status = DeleteFile(dirPath + entry.name);
			}

			if (!status)
			{
				return false;
			}
		}
	}

	return DeleteFile(path);
}


// ===============
// ==  Ostatní  ==
// ===============

RTL::Pipe RTL::CreatePipe()
{
	kiv_os::THandle handles[2] = { 0, 0 };

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

void RTL::Shutdown()
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_os::NOS_Service_Major::Process);
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_Process::Shutdown);

	SysCall(registers);
}



kiv_os::NOS_Error kiv_os_rtl::Last_Error;  // NEPOUŽÍVAT
