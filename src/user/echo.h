#pragma once
#include "..\api\api.h"

/*
	todo: co bude v jakych registrech
	todo: kde uložit hodnotu pøepínaèe on/off

	V regs.rax.r je oèekáván pointer na buffer s null-terminated textem k vypsání typovaný na char *.
	V regs.rbx.x je oèekáván file_handle do ktereho se bude vypisovat text typovaný na kiv_os::THandle.
	Vrací 0 pokud byl text vypsán.
*/
extern "C" size_t __stdcall echo(const kiv_hal::TRegisters &regs);
