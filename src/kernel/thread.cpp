#include "thread.h"
#include "process.h"
#include "kernel.h"

struct ThreadData
{
	HandleReference self;
	HandleReference process;
};

static thread_local ThreadData g_threadData;

void Thread::Start(kiv_os::TThread_Proc entry, kiv_hal::TRegisters context, HandleID threadID, HandleID processID)
{
	g_threadData.self    = Kernel::GetHandleStorage().getHandle(threadID);
	g_threadData.process = Kernel::GetHandleStorage().getHandle(processID);

	g_threadData.self.as<Thread>()->m_isRunning = true;

	Kernel::GetEventSystem().dispatchEvent(Event::THREAD_START, threadID);

	if (g_threadData.process.as<Process>()->incrementThreadCount() == 1)
	{
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

	g_threadData.self.as<Thread>()->m_isRunning = false;

	Kernel::GetEventSystem().dispatchEvent(Event::THREAD_END, threadID);

	if (g_threadData.process.as<Process>()->decrementThreadCount() == 0)
	{
		// aktuální vlákno je poslední běžící vlákno procesu
		Kernel::GetEventSystem().dispatchEvent(Event::PROCESS_END, processID);
	}
}

HandleReference Thread::Create(kiv_os::TThread_Proc entry, const kiv_hal::TRegisters & context, HandleID processID)
{
	HandleReference thread = Kernel::GetHandleStorage().addHandle(std::make_unique<Thread>());
	if (!thread)
	{
		return HandleReference();
	}

	thread.as<Thread>()->m_thread = std::thread(Start, entry, context, thread.getID(), processID);

	return thread;
}

Thread *Thread::Get()
{
	return g_threadData.self.as<Thread>();
}

HandleID Thread::GetID()
{
	return g_threadData.self.getID();
}

Process *Thread::GetProcess()
{
	return g_threadData.process.as<Process>();
}

HandleID Thread::GetProcessID()
{
	return g_threadData.process.getID();
}

void Thread::SetExitCode(int exitCode)
{
	Thread *self = Thread::Get();
	if (!self)
	{
		return;
	}

	self->m_exitCode.store(exitCode, std::memory_order_relaxed);
}

void Thread::SetSignalHandler(kiv_os::TThread_Proc handler)
{
	Thread *self = Thread::Get();
	if (!self)
	{
		return;
	}

	self->m_signalHandler = handler;
}

void Thread::SetSignalEnabled(kiv_os::NSignal_Id signal, bool isEnabled)
{
	Thread *self = Thread::Get();
	if (!self)
	{
		return;
	}

	const uint32_t signalFlag = 0x1 << static_cast<uint8_t>(signal);

	if (isEnabled)
	{
		self->m_signalMask |= signalFlag;
	}
	else
	{
		self->m_signalMask &= ~signalFlag;
	}

	self->m_pendingSignals &= ~signalFlag;
}

void Thread::HandleSignals()
{
	Thread *self = Thread::Get();
	if (!self || !self->m_signalHandler)
	{
		return;
	}

	uint32_t pendingSignals = self->m_pendingSignals.exchange(0, std::memory_order_relaxed);
	if (!pendingSignals)
	{
		return;
	}

	for (unsigned int i = 0; i < 32; i++)
	{
		const uint32_t signalFlag = 0x1 << i;

		if (pendingSignals & signalFlag && self->m_signalMask & signalFlag)
		{
			kiv_hal::TRegisters context;
			context.rcx.e = i;

			self->m_signalHandler(context);
		}
	}
}
