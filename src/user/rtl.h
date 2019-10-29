#pragma once

#include <cstring>
#include <string>

#include "../api/api.h"

struct ThreadEnvironment
{
	kiv_os::NOS_Error lastError = kiv_os::NOS_Error::Success;
	kiv_os::THandle stdIn  = 0;  // nula je lepší než -1 jako invalid handle...
	kiv_os::THandle stdOut = 0;  // ...protože "if (handle)" je mnohem lepší než "if (handle != kiv_os::Invalid_Handle)"
	const char *processCmdLine = "";
};

// =========================================================================================
#define RTL_DEFINE_SHELL_PROGRAM(NAME)\
static int NAME##_main(const char *args);\
extern "C" __declspec(dllexport) size_t __stdcall NAME(const kiv_hal::TRegisters & context)\
{\
	ThreadEnvironment *pEnv = RTL::GetCurrentThreadEnv();\
	pEnv->stdIn  = static_cast<kiv_os::THandle>(context.rax.x);\
	pEnv->stdOut = static_cast<kiv_os::THandle>(context.rbx.x);\
	pEnv->processCmdLine = reinterpret_cast<const char*>(context.rdi.r);\
	return NAME##_main(pEnv->processCmdLine);\
}
// =========================================================================================

namespace RTL
{
	// ========================
	// ==  Procesy a vlákna  ==
	// ========================

	/**
	 * @brief Vrátí řetězec s argumenty aktuálního procesu.
	 * Stejný řetězec je předán jako parametr main funkci procesu.
	 * @return Argumenty procesu.
	 */
	const char *GetProcessCmdLine();

	/**
	 * @brief Vrátí handle na standardní vstup aktuálního procesu.
	 * @return Handle na standardní vstup.
	 */
	kiv_os::THandle GetStdInHandle();

	/**
	 * @brief Vrátí handle na standardní výstup aktuálního procesu.
	 * @return Handle na standardní výstup.
	 */
	kiv_os::THandle GetStdOutHandle();

	/**
	 * @brief Vrátí ukazatel na strukturu prostředí aktuálního vlákna.
	 * Tato funkce by se neměla používat mimo RTL.
	 * @return Prostředí aktuálního vlákna.
	 */
	ThreadEnvironment *GetCurrentThreadEnv();

	// ====================
	// ==  Chybové kódy  ==
	// ====================

	/**
	 * @brief Vrátí chybový kód pro aktuální vlákno.
	 * @return Aktuální chybový kód.
	 */
	kiv_os::NOS_Error GetLastError();

	/**
	 * @brief Nastaví chybový kód pro aktuální vlákno.
	 * @param error Chybový kód.
	 */
	void SetLastError(kiv_os::NOS_Error error);

	// =========================================
	// ==  Soubory a standardní vstup/výstup  ==
	// =========================================

	/**
	 * @brief Načte data ze souboru.
	 * @param file Deskriptor souboru.
	 * @param buffer Buffer pro uložení načtených dat.
	 * @param size Velikost bufferu pro uložení načtených dat v bajtech.
	 * @param pRead Volitelný ukazatel na proměnnou, kam se uloží počet načtených bajtů. Může být null.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool ReadFile(kiv_os::THandle file, char *buffer, size_t size, size_t *pRead = nullptr);

	/**
	 * @brief Načte data ze standardního vstupu procesu.
	 * @param buffer Buffer pro uložení načtených dat.
	 * @param size Velikost bufferu pro uložení načtených dat v bajtech.
	 * @param pRead Volitelný ukazatel na proměnnou, kam se uloží počet načtených bajtů. Může být null.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool ReadStdIn(char *buffer, size_t size, size_t *pRead = nullptr)
	{
		return ReadFile(GetStdInHandle(), buffer, size, pRead);
	}

	/**
	 * @brief Zapíše data do souboru.
	 * @param file Deskriptor souboru.
	 * @param buffer Ukazatel na začátek dat k zapsání.
	 * @param size Velikost dat k zapsání v bajtech.
	 * @param pWritten Volitelný ukazatel na proměnnou, kam se uloží počet zapsaných bajtů. Může být null.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool WriteFile(kiv_os::THandle file, const char *buffer, size_t size, size_t *pWritten = nullptr);

	/**
	 * @brief Zapíše řetězec ukončený nulou do souboru.
	 * @param file Deskriptor souboru.
	 * @param string Řetězec k zapsání.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteFile(kiv_os::THandle file, const char *string)
	{
		return WriteFile(file, string, std::strlen(string));
	}

	/**
	 * @brief Zapíše řetězec do souboru.
	 * @param file Deskriptor souboru.
	 * @param string Řetězec k zapsání.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteFile(kiv_os::THandle file, const std::string & string)
	{
		return WriteFile(file, string.c_str(), string.length());
	}

	/**
	 * @brief Zapíše data na standardní výstup procesu.
	 * @param buffer Ukazatel na začátek dat k zapsání.
	 * @param size Velikost dat k zapsání v bajtech.
	 * @param pWritten Volitelný ukazatel na proměnnou, kam se uloží počet zapsaných bajtů. Může být null.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteStdOut(const char *buffer, size_t size, size_t *pWritten = nullptr)
	{
		return WriteFile(GetStdOutHandle(), buffer, size, pWritten);
	}

	/**
	 * @brief Zapíše řetězec ukončený nulou na standardní výstup procesu.
	 * @param string Řetězec k zapsání.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteStdOut(const char *string)
	{
		return WriteFile(GetStdOutHandle(), string);
	}

	/**
	 * @brief Zapíše řetězec na standardní výstup procesu.
	 * @param string Řetězec k zapsání.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteStdOut(const std::string & string)
	{
		return WriteFile(GetStdOutHandle(), string);
	}
}



// tento nesmysl zde musí zůstat, protože je použit v api.cpp, který nemůžeme měnit :(
namespace kiv_os_rtl
{
	// globální proměnná s číslem poslední chyby MUSÍ být thread-local, jinak je k ničemu
	// std::atomic rozhodně nestačí
	extern kiv_os::NOS_Error Last_Error;  // NEPOUŽÍVAT
}
