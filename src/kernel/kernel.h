#pragma once


#include "..\api\hal.h"
#include "..\api\api.h"

void Set_Error(const bool failed, kiv_hal::TRegisters &regs);

/*
	Obsluha p�eru�en�, kter� zavede OS.
*/
void __stdcall Bootstrap_Loader(kiv_hal::TRegisters &context);