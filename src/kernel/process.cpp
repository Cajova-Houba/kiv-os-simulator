#include "process.h"
#include "thread.h"
#include "kernel.h"

HandleReference Process::getMainThread()
{
	// je potřeba synchronizovat, protože m_mainThreadID se nastavuje až po vytvoření hlavního vlákna
	std::lock_guard<std::mutex> lock(m_mutex);

	return Kernel::GetHandleStorage().getHandle(m_mainThreadID);
}

HandleReference Process::getHandle(HandleID id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	HandleReference key(id, nullptr);
	auto it = m_handles.find(key);
	if (it == m_handles.end())
	{
		return HandleReference();
	}

	return Kernel::GetHandleStorage().getHandle(it->getID());
}

HandleReference Process::getHandleOfType(HandleID id, EHandle type)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	HandleReference key(id, nullptr);
	auto it = m_handles.find(key);
	if (it == m_handles.end() || it->get()->getHandleType() != type)
	{
		return HandleReference();
	}

	return Kernel::GetHandleStorage().getHandle(it->getID());
}

bool Process::hasHandle(HandleID id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	HandleReference key(id, nullptr);
	auto it = m_handles.find(key);
	if (it == m_handles.end())
	{
		return false;
	}

	return true;
}

bool Process::hasHandleOfType(HandleID id, EHandle type)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	HandleReference key(id, nullptr);
	auto it = m_handles.find(key);
	if (it == m_handles.end() || it->get()->getHandleType() != type)
	{
		return false;
	}

	return true;
}

void Process::addHandle(HandleReference && handle)
{
	if (!handle)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	m_handles.insert(std::move(handle));
}

void Process::removeHandle(HandleID id)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	HandleReference key(id, nullptr);
	auto it = m_handles.find(key);
	if (it == m_handles.end())
	{
		return;
	}

	m_handles.erase(it);
}

HandleReference Process::Create(const char *name, const char *cmdLine, Path && path, kiv_os::TThread_Proc entry,
                                HandleReference && stdIn, HandleReference && stdOut, bool useCurrentThread)
{
	HandleReference process = Kernel::GetHandleStorage().addHandle(std::make_unique<Process>());
	if (!process)
	{
		return HandleReference();
	}

	Process *self = process.as<Process>();

	self->m_name = name;
	self->m_cmdLine = cmdLine;
	self->m_currentDirectory = std::move(path);

	kiv_hal::TRegisters context;
	context.rax.x = stdIn.getID();
	context.rbx.x = stdOut.getID();
	context.rdi.r = reinterpret_cast<uint64_t>(self->m_cmdLine.c_str());

	if (stdIn)
	{
		self->m_handles.insert(std::move(stdIn));
	}

	if (stdOut)
	{
		self->m_handles.insert(std::move(stdOut));
	}

	if (useCurrentThread)
	{
		HandleReference mainThread = Kernel::GetHandleStorage().addHandle(std::make_unique<Thread>());
		if (!mainThread)
		{
			return HandleReference();
		}

		self->m_mainThreadID = mainThread.getID();
		self->m_handles.insert(std::move(mainThread));

		Thread::Start(entry, context, self->m_mainThreadID, process.getID());
	}
	else
	{
		std::lock_guard<std::mutex> lock(self->m_mutex);

		HandleReference mainThread = Thread::Create(entry, context, process.getID());
		if (!mainThread)
		{
			return HandleReference();
		}

		self->m_mainThreadID = mainThread.getID();
		self->m_handles.insert(std::move(mainThread));
	}

	return process;
}
