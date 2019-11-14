#pragma once

#include <atomic>
#include <thread>

#include "../api/api.h"

#include "handle_reference.h"

class Process;

class Thread : public IHandle
{
	std::thread m_thread;
	std::atomic<int> m_exitCode;
	std::atomic<bool> m_isRunning;
	std::atomic<uint32_t> m_pendingSignals;
	kiv_os::TThread_Proc m_signalHandler;
	uint32_t m_signalMask;

	// vstupní bod vlákna
	static void Start(kiv_os::TThread_Proc entry, kiv_hal::TRegisters context, HandleID threadID, HandleID processID);

	friend class Process;

public:
	Thread() = default;

	EHandle getHandleType() const override
	{
		return EHandle::THREAD;
	}

	int getExitCode() const
	{
		return m_exitCode.load(std::memory_order_relaxed);
	}

	bool isRunning() const
	{
		return m_isRunning.load(std::memory_order_relaxed);
	}

	void raiseSignal(kiv_os::NSignal_Id signal)
	{
		m_pendingSignals |= 0x1 << static_cast<uint8_t>(signal);
	}

	// vytvoří nové vlákno
	static HandleReference Create(kiv_os::TThread_Proc entry, const kiv_hal::TRegisters & context, HandleID processID);

	static Thread *Get();
	static HandleID GetID();

	static Process *GetProcess();
	static HandleID GetProcessID();

	static void SetExitCode(int exitCode);
	static void SetSignalHandler(kiv_os::TThread_Proc handler);
	static void SetSignalEnabled(kiv_os::NSignal_Id signal, bool isEnabled);
	static void HandleSignals();
};
