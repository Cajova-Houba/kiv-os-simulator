#pragma once
#include "..\api\api.h"

/*
	todo: co bude v jakych registrech
	todo: kde ulo�it hodnotu p�ep�na�e on/off

	V regs.rax.r je o�ek�v�n pointer na buffer s null-terminated textem k vyps�n� typovan� na char *.
	V regs.rbx.x je o�ek�v�n file_handle do ktereho se bude vypisovat text typovan� na kiv_os::THandle.
	Vrac� 0 pokud byl text vyps�n.
*/
extern "C" size_t __stdcall echo(const kiv_hal::TRegisters &regs);
