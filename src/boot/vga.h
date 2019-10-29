#pragma once

#include "../api/hal.h"

namespace VGA
{
	void __stdcall InterruptHandler(kiv_hal::TRegisters & context);
}
