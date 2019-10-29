#pragma once

#include "../api/hal.h"

namespace Disk
{
	void __stdcall InterruptHandler(kiv_hal::TRegisters & context);
}
