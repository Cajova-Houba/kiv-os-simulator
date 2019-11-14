#include "syscall.h"
#include "thread.h"

void __stdcall SysCall::Entry(kiv_hal::TRegisters & context)
{
	Thread::HandleSignals();

	EStatus status = EStatus::INVALID_ARGUMENT;  // neznámé číslo služby

	switch (static_cast<kiv_os::NOS_Service_Major>(context.rax.h))
	{
		case kiv_os::NOS_Service_Major::File_System:
		{
			status = HandleIO(context);
			break;
		}
		case kiv_os::NOS_Service_Major::Process:
		{
			status = HandleProcess(context);
			break;
		}
	}

	if (status == EStatus::SUCCESS)
	{
		context.flags.carry = 0;
	}
	else
	{
		context.flags.carry = 1;
		context.rax.x = static_cast<uint16_t>(status);
	}

	Thread::HandleSignals();
}
