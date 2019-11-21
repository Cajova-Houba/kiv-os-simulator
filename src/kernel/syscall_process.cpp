#include "syscall.h"
#include "kernel.h"
#include "process.h"

static EStatus CreateProcess(const char *program, const char *cmdLine, HandleID stdInID, HandleID stdOutID, HandleID & result)
{
	if (!program || !cmdLine)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	Process & currentProcess = Thread::GetProcess();

	TEntryFunc entry = Kernel::GetUserDLL().getSymbol<TEntryFunc>(program);
	if (!entry)
	{
		// program neexistuje
		return EStatus::FILE_NOT_FOUND;
	}

	HandleReference stdIn;
	HandleReference stdOut;

	if (stdInID)
	{
		stdIn = currentProcess.getHandleOfType(stdInID, EHandle::FILE);
		if (!stdIn)
		{
			return EStatus::INVALID_ARGUMENT;
		}
	}

	if (stdOutID)
	{
		stdOut = currentProcess.getHandleOfType(stdOutID, EHandle::FILE);
		if (!stdOut)
		{
			return EStatus::INVALID_ARGUMENT;
		}
	}

	Path path = currentProcess.getWorkingDirectory();

	HandleReference process = Process::Create(program, cmdLine, std::move(path), entry, std::move(stdIn), std::move(stdOut));
	if (!process)
	{
		return EStatus::OUT_OF_MEMORY;
	}

	result = process.getID();

	currentProcess.addHandle(std::move(process));

	return EStatus::SUCCESS;
}

static EStatus CreateThread(TEntryFunc entry, void *param, HandleID & result)
{
	if (!entry)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	Process & currentProcess = Thread::GetProcess();

	kiv_hal::TRegisters context;
	context.rdi.r = reinterpret_cast<uint64_t>(param);

	HandleReference thread = Thread::Create(entry, context, Thread::GetProcessID());
	if (!thread)
	{
		return EStatus::OUT_OF_MEMORY;
	}

	result = thread.getID();

	currentProcess.addHandle(std::move(thread));

	return EStatus::SUCCESS;
}

static EStatus Clone(const kiv_hal::TRegisters & context, HandleID & result)
{
	switch (static_cast<kiv_os::NClone>(context.rcx.l))
	{
		case kiv_os::NClone::Create_Process:
		{
			const char *program = reinterpret_cast<const char*>(context.rdx.r);
			const char *cmdLine = reinterpret_cast<const char*>(context.rdi.r);
			HandleID stdInID  = static_cast<HandleID>(context.rbx.e >> 16);
			HandleID stdOutID = static_cast<HandleID>(context.rbx.e);

			return CreateProcess(program, cmdLine, stdInID, stdOutID, result);
		}
		case kiv_os::NClone::Create_Thread:
		{
			TEntryFunc entry = reinterpret_cast<TEntryFunc>(context.rdx.r);
			void *param = reinterpret_cast<void*>(context.rdi.r);

			return CreateThread(entry, param, result);
		}
	}

	return EStatus::INVALID_ARGUMENT;
}

static EStatus WaitFor(const HandleID *handles, uint16_t handleCount, uint16_t & result)
{
	if (handles == nullptr || handleCount == 0)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	return Kernel::GetEventSystem().waitForMultiple(handles, handleCount, Event::THREAD_END | Event::PROCESS_END, result);
}

static EStatus GetExitCode(HandleID id, uint16_t & exitCode)
{
	Process & currentProcess = Thread::GetProcess();

	HandleReference handle = currentProcess.getHandle(id);
	if (!handle)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	switch (handle->getHandleType())
	{
		case EHandle::THREAD:
		{
			exitCode = handle.as<Thread>()->getExitCode();
			break;
		}
		case EHandle::PROCESS:
		{
			exitCode = handle.as<Process>()->getMainThread().as<Thread>()->getExitCode();
			break;
		}
		default:
		{
			return EStatus::INVALID_ARGUMENT;
		}
	}

	return EStatus::SUCCESS;
}

static EStatus Exit(uint16_t exitCode)
{
	// pouze nastavíme návratový kód aktuálního vlákna
	// samotné ukončení musí vlákno provést samo pomocí návratu ze vstupní funkce
	Thread::SetExitCode(exitCode);

	return EStatus::SUCCESS;
}

static EStatus SystemShutdown()
{
	// získáme handle na všechna uživatelská vlákna v systému
	std::vector<HandleReference> threads = Kernel::GetHandleStorage().getHandles(
		[](HandleID, const IHandle *pHandle) -> bool
		{
			return pHandle->getHandleType() == EHandle::THREAD;
		}
	);

	// a pošleme všem signál Terminate
	for (HandleReference & handle : threads)
	{
		handle.as<Thread>()->raiseSignal(kiv_os::NSignal_Id::Terminate);
	}

	return EStatus::SUCCESS;
}

static EStatus SetupSignal(uint8_t signalNumber, TEntryFunc signalHandler)
{
	if (signalNumber > 32)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	kiv_os::NSignal_Id signal = static_cast<kiv_os::NSignal_Id>(signalNumber);

	if (signalHandler)
	{
		Thread::SetSignalEnabled(signal, true);
		Thread::SetSignalHandler(signalHandler);
	}
	else
	{
		Thread::SetSignalEnabled(signal, false);
	}

	return EStatus::SUCCESS;
}

EStatus SysCall::HandleProcess(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_os::NOS_Process>(context.rax.l))
	{
		case kiv_os::NOS_Process::Clone:
		{
			return Clone(context, context.rax.x);
		}
		case kiv_os::NOS_Process::Wait_For:
		{
			return WaitFor(reinterpret_cast<HandleID*>(context.rdx.r), context.rcx.x, context.rax.x);
		}
		case kiv_os::NOS_Process::Read_Exit_Code:
		{
			return GetExitCode(context.rdx.x, context.rax.x);
		}
		case kiv_os::NOS_Process::Exit:
		{
			return Exit(context.rcx.x);
		}
		case kiv_os::NOS_Process::Shutdown:
		{
			return SystemShutdown();
		}
		case kiv_os::NOS_Process::Register_Signal_Handler:
		{
			return SetupSignal(context.rcx.l, reinterpret_cast<TEntryFunc>(context.rdx.r));
		}
	}

	return EStatus::INVALID_ARGUMENT;
}
