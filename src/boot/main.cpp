#include <iostream>
#include <windows.h>

#include "../api/hal.h"

#include "cmos.h"
#include "idt.h"
#include "keyboard.h"

static bool Setup_HW()
{
	// příkazová řádka ve Windows je velmi velmi pochybná, takže zde raději nebudeme vypisovat chybové hlášky s diakritikou

	if (!CMOS::Init())
	{
		std::cout << "Nepodarilo se nacist konfiguracni soubor " CMOS_CONFIG_FILENAME "!" << std::endl;
		return false;
	}

	if (!Keyboard::Init())
	{
		std::cout << "Nepodarilo se inicializovat klavesnici!" << std::endl;
		return false;
	}

	// připravíme tabulku vektorů přerušení
	if (!IDT::Init())
	{
		std::cout << "Nepodarilo se inicializovat IDT!" << std::endl;
		return false;
	}

	// disky tady inicializovat nebudeme, ty ať si klidně selžou třeba během chodu systému

	return true;
}

int __cdecl main()
{
	if (!Setup_HW())
	{
		return 1;
	}

	// HW je nastaven, zavedeme simulovaný operační systém
	HMODULE kernel = LoadLibraryA("kernel.dll");
	if (!kernel)
	{
		std::cout << "Nelze nacist kernel.dll!" << std::endl;
		return 1;
	}

	kiv_hal::TRegisters context;

	// v tuto chvíli DLLMain v kernel.dll měla nahrát na NInterrupt::Bootstrap_Loader adresu vstupní funkce jádra
	// takže ji spustíme
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Bootstrap_Loader, context);

	// a až simulovaný OS skončí, uvolníme zdroje z paměti
	FreeLibrary(kernel);
	TlsFree(kiv_hal::Expected_Tls_IDT_Index);

	return 0;
}
