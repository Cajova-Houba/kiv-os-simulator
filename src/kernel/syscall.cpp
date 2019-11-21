#include "syscall.h"
#include "thread.h"

void __stdcall SysCall::Entry(kiv_hal::TRegisters & context)
{
	EStatus status = EStatus::UNKNOWN_ERROR;

	if (Thread::HasContext())
	{
		Thread::HandleSignals();

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
			default:
			{
				status = EStatus::INVALID_ARGUMENT;  // neznámé číslo služby
				break;
			}
		}

		Thread::HandleSignals();
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
}
