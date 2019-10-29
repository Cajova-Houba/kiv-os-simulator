#pragma once

#include "../api/hal.h"

namespace Keyboard
{
	bool Init();

	void __stdcall InterruptHandler(kiv_hal::TRegisters & context);
}
