#include <thread>
#include <new>

#include "thread.h"
#include "process.h"
#include "kernel.h"

struct ThreadEnvironment
{
	HandleReference self;
	HandleReference process;
};

static thread_local ThreadEnvironment *g_pThreadEnv;

struct ThreadEnvironmentGuard
{
	ThreadEnvironmentGuard(HandleID threadID, HandleID processID)
	{
		g_pThreadEnv = new ThreadEnvironment;

		g_pThreadEnv->self    = Kernel::GetHandleStorage().getHandle(threadID);
		g_pThreadEnv->process = Kernel::GetHandleStorage().getHandle(processID);
	}

	~ThreadEnvironmentGuard()
	{
		delete g_pThreadEnv;
		g_pThreadEnv = nullptr;
	}
};

void Thread::Start(TEntryFunc entry, kiv_hal::TRegisters context, HandleID threadID, HandleID processID)
{
	ThreadEnvironmentGuard environment(threadID, processID);

	Thread & self = Thread::Get();
	Process & process = Thread::GetProcess();

	self.setRunning(true);
	self.setStarted(true);

	Kernel::GetEventSystem().dispatchEvent(Event::THREAD_START, threadID);

	if (process.incrementThreadCount() == 1)
	{
		process.setStarted(true);

		// aktuální vlákno je první vlákno procesu
		Kernel::GetEventSystem().dispatchEvent(Event::PROCESS_START, processID);
	}

	// ====================
	// == Začátek vlákna ==
	// ====================

	entry(context);

	// ====================
	// ==  Konec vlákna  ==
	// ====================

	self.setRunning(false);

	Kernel::GetEventSystem().dispatchEvent(Event::THREAD_END, threadID);

	if (process.decrementThreadCount() == 0)
	{
		// aktuální vlákno je poslední běžící vlákno procesu
		Kernel::GetEventSystem().dispatchEvent(Event::PROCESS_END, processID);
	}
}

HandleReference Thread::Create(TEntryFunc entry, const kiv_hal::TRegisters & context, HandleID processID)
{
	HandleReference threadHandle = Kernel::GetHandleStorage().addHandle(std::make_unique<Thread>());
	if (!threadHandle)
	{
		return HandleReference();
	}

	std::thread thread(Start, entry, context, threadHandle.getID(), processID);

	thread.detach();

	return threadHandle;
}

bool Thread::HasContext()
{
	return g_pThreadEnv->self.isValid();
}

Thread & Thread::Get()
{
	return *g_pThreadEnv->self.as<Thread>();
}

HandleID Thread::GetID()
{
	return g_pThreadEnv->self.getID();
}

Process & Thread::GetProcess()
{
	return *g_pThreadEnv->process.as<Process>();
}

HandleID Thread::GetProcessID()
{
	return g_pThreadEnv->process.getID();
}

void Thread::SetExitCode(int exitCode)
{
	Thread::Get().m_exitCode.store(exitCode, std::memory_order_relaxed);
}

void Thread::SetSignalHandler(TEntryFunc handler)
{
	Thread::Get().m_signalHandler = handler;
}

void Thread::SetSignalEnabled(kiv_os::NSignal_Id signal, bool isEnabled)
{
	Thread & self = Thread::Get();

	const uint32_t signalBit = 0x1 << (static_cast<uint8_t>(signal) - 1);

	if (isEnabled)
	{
		self.m_signalMask |= signalBit;
	}
	else
	{
		self.m_signalMask &= ~signalBit;
	}

	self.m_pendingSignals &= ~signalBit;
}

void Thread::HandleSignals()
{
	Thread & self = Thread::Get();

	if (!self.m_signalHandler)
	{
		return;
	}

	uint32_t pendingSignals = self.m_pendingSignals.exchange(0, std::memory_order_relaxed);
	if (!pendingSignals)
	{
		return;
	}

	for (unsigned int i = 0; i < 32; i++)
	{
		const uint32_t signalBit = 0x1 << i;

		if (pendingSignals & signalBit && self.m_signalMask & signalBit)
		{
			kiv_hal::TRegisters context;
			context.rcx.e = i + 1;

			self.m_signalHandler(context);
		}
	}
}
