#pragma once
#include "echo.h"

#include "rtl.h"

const char* NEW_LINE = "\n";

size_t __stdcall echo(const kiv_hal::TRegisters &regs) {

	// pointer na buffer s textem k vypsani
	char * buffer = reinterpret_cast<char*>(regs.rax.r);

	// handle souboru do ktereho bude zapsan vystup
	kiv_os::THandle file_handle = static_cast<kiv_os::THandle>(regs.rbx.x);

	size_t counter = 0;
	bool res = false;

	// posli data na zapis
	// je pouzito write_file misto write_console, protoze file_handle
	// nemusi byt std_out, ale treba soubor (v pripade pouziti presmerovani)
	res = kiv_os_rtl::Write_File(file_handle, NEW_LINE, strlen(NEW_LINE), counter);
	res = kiv_os_rtl::Write_File(file_handle, buffer, strlen(buffer), counter);	//a vypiseme ho
	res = kiv_os_rtl::Write_File(file_handle, NEW_LINE, strlen(NEW_LINE), counter);
	
	// vrat 0 pokud vse ok
	return res ? 0 : 1;
}
