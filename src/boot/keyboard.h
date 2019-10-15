#pragma once

#include "../api/hal.h"

bool Init_Keyboard();

/*
	Obsluha preruseni klavesnice.
*/
void __stdcall Keyboard_Handler(kiv_hal::TRegisters &context);