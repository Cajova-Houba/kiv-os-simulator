#include <windows.h>

#include "../api/hal.h"

#include "idt.h"
#include "keyboard.h"
#include "vga.h"
#include "disk.h"

extern kiv_hal::TInterrupt_Handler *interrupt_descriptor_table;  // api.cpp

kiv_hal::TInterrupt_Handler g_IDT_storage[256];

bool IDT::Init()
{
	memset(g_IDT_storage, 0, sizeof g_IDT_storage);
	interrupt_descriptor_table = g_IDT_storage;

	// nastavení přerušení pro VGA, Disk IO a klávesnici
	kiv_hal::Set_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, VGA::InterruptHandler);
	kiv_hal::Set_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, Disk::InterruptHandler);
	kiv_hal::Set_Interrupt_Handler(kiv_hal::NInterrupt::Keyboard, Keyboard::InterruptHandler);

	// bez možnosti úpravy api.cpp se tohoto svinstva nelze zbavit :(
	DWORD index = TlsAlloc();
	if (kiv_hal::Expected_Tls_IDT_Index != index || !TlsSetValue(index, interrupt_descriptor_table))
	{
		TlsFree(index);
		return false;
	}

	return true;
}
