#pragma once

#include <atomic>
#include <thread>

#include "../api/api.h"

#include "handle_reference.h"

using TEntryFunc = size_t (__stdcall *)(const kiv_hal::TRegisters & context);  // kiv_os::TThread_Proc

class Process;

class Thread : public IHandle
{
	std::thread m_thread;
	std::atomic<int> m_exitCode;
	std::atomic<bool> m_isRunning;
	std::atomic<uint32_t> m_pendingSignals;
	TEntryFunc m_signalHandler;
	uint32_t m_signalMask;

	// vstupní bod vlákna
	static void Start(TEntryFunc entry, kiv_hal::TRegisters context, HandleID threadID, HandleID processID);

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
	static HandleReference Create(TEntryFunc entry, const kiv_hal::TRegisters & context, HandleID processID);

	static bool HasContext();

	static Thread & Get();
	static HandleID GetID();

	static Process & GetProcess();
	static HandleID GetProcessID();

	static void SetExitCode(int exitCode);
	static void SetSignalHandler(TEntryFunc handler);
	static void SetSignalEnabled(kiv_os::NSignal_Id signal, bool isEnabled);
	static void HandleSignals();
};
