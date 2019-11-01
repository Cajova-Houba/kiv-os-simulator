#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "../api/api.h"

// =========================================================================================
#define RTL_DEFINE_SHELL_PROGRAM(NAME)\
static int NAME##_main(const char *args);\
extern "C" __declspec(dllexport) size_t __stdcall NAME(const kiv_hal::TRegisters & context)\
{\
	RTL::ThreadEnvironment *pEnv = RTL::GetCurrentThreadEnv();\
	pEnv->process.stdIn   = static_cast<kiv_os::THandle>(context.rax.x);\
	pEnv->process.stdOut  = static_cast<kiv_os::THandle>(context.rbx.x);\
	pEnv->process.cmdLine = reinterpret_cast<const char*>(context.rdi.r);\
	RTL::SetCurrentThreadExitCode(NAME##_main(RTL::GetProcessCmdLine()));\
	return 0;\
}
// =========================================================================================

namespace RTL
{
	// ========================
	// ==  Procesy a vlákna  ==
	// ========================

	using ThreadMain = int (*)(void *param);
	using SignalHandler = void (*)(kiv_os::NSignal_Id signal);

	struct ProcessEnvironment
	{
		kiv_os::THandle stdIn  = 0;  // nula je lepší než -1 jako invalid handle
		kiv_os::THandle stdOut = 0;  // protože "if (handle)" je lepší než "if (handle != kiv_os::Invalid_Handle)"
		const char *cmdLine    = "";
	};

	struct ThreadEnvironment
	{
		kiv_os::NOS_Error lastError = kiv_os::NOS_Error::Success;
		SignalHandler signalHandler = nullptr;
		uint32_t signalMask = 0;

		// informace o procesu ve kterém je vlákno spuštěno
		ProcessEnvironment process;
	};

	/**
	 * @brief Vytvoří nový proces.
	 * Handle procesu by se měl uzavřít pomocí RTL::CloseHandle, pokud už není potřeba. Handle na ukončený proces je možné
	 * použít pro získání návratového kódu pomocí RTL::GetExitCode. Při uzavření handle na ukončený proces je tento proces
	 * odstraněn ze systému. Při uzavření handle na běžící proces je daný proces odstraněn automaticky po skončení. Všechny
	 * ukončené procesy jsou rovněž automaticky odstraněny po skončení procesu, který je spustil. Také proces běžící i po
	 * ukončení procesu, který jej spustil, je automaticky odstraněn po svém ukončení.
	 * @param program Řetězec ukončený nulou s názvem programu, který se má spustit. Například "dir".
	 * @param env Prostředí nového procesu.
	 * @return Handle na nový proces nebo 0, pokud došlo k chybě. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	kiv_os::THandle CreateProcess(const char *program, const ProcessEnvironment & env);

	inline kiv_os::THandle CreateProcess(const std::string & program, const ProcessEnvironment & env)
	{
		return CreateProcess(program.c_str(), env);
	}

	/**
	 * @brief Vytvoří nové vlákno v kontextu aktuálního procesu.
	 * Handle vlákna by se měl uzavřít pomocí RTL::CloseHandle, pokud už není potřeba. Handle na ukončené vlákno je možné
	 * použít pro získání návratového kódu pomocí RTL::GetExitCode. Při uzavření handle na ukončené vlákno je toto vlákno
	 * odstraněno ze systému. Při uzavření handle na běžící vlákno je dané vlákno odstraněno automaticky po skončení. Při
	 * ukončení procesu jsou také automaticky odstraněna všechna jeho vlákna. Proces se ukončí pouze tehdy, když skončí
	 * všechna jeho vlákna.
	 * @param mainFunc Main funkce nového vlákna.
	 * @param param Libovolná hodnota předaná main funkci nového vlákna jako parametr.
	 * @return Handle na nové vlákno nebo 0, pokud došlo k chybě. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	kiv_os::THandle CreateThread(ThreadMain mainFunc, void *param);

	/**
	 * @brief Blokuje, dokud se některý ze zadaných procesů nebo vláken neukončí.
	 * @param handles Pole obsahující handle procesů nebo vláken, na které se má čekat.
	 * @param count Celkový počet procesů nebo vláken, na které se má čekat.
	 * @return Index handle procesu nebo vlákna, který je ukončený nebo se ukončil během čekání. Pokud došlo k chybě, tak -1.
	 * Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	int WaitFor(const kiv_os::THandle *handles, uint16_t count);

	inline int WaitFor(const std::vector<kiv_os::THandle> handles)
	{
		return WaitFor(handles.data(), static_cast<uint16_t>(handles.size()));
	}

	/**
	 * @brief Vrátí návratový kód procesu nebo vlákna.
	 * Pokud proces nebo vlákno stále běží, tak tato funkce vrací vždy 0 s chybovým kódem kiv_os::NOS_Error::Success.
	 * Pokud parametr handle není validní handle na existující proces nebo vlákno, tak návratový kód je také 0, ale chybový
	 * kód se nastaví na kiv_os::NOS_Error::Invalid_Argument.
	 * @param handle Handle na proces nebo vlákno.
	 * @return Návratový kód procesu nebo vlákna.
	 */
	int GetExitCode(kiv_os::THandle handle);

	/**
	 * @brief Nastaví zpracování signálů pro aktuální vlákno.
	 * Signal handler je vždy pouze jeden společný pro všechny signály. Spouští se vždy v kontextu vlákna, ve kterém je
	 * nastaven. Po přijetí signálu dojde ke spuštění signal handleru při prvním vstupu daného vlákna do jádra (syscall).
	 * Pokud je signál přijat v době, kdy se dané vlákno nachází uvnitř jádra, tak dojde ke spuštění signal handleru před
	 * výstupem vlákna z jádra.
	 * Pokud je jeden z parametrů této funkce nulový, tak se vynuluje i druhý, a pro všechny signály se nastaví výchozí
	 * handler. Každé vlákno má na začátku nastaven stejný výchozí signal handler, který nic nedělá.
	 * @param handler Signal handler.
	 * @param signalMask Bitová maska, kde každý bit reprezentuje jeden signál. Pozice bitu odpovídá číslu signálu. Hodnota
	 * bitu určuje, zda se pro daný signál použije nastavený handler (1) nebo výchozí handler (0).
	 */
	void SetupSignals(SignalHandler handler, uint32_t signalMask);

	/**
	 * @brief Vrátí signal handler aktuálního vlákna.
	 * @return Signal handler nebo null, pokud signal handler není nastaven. Viz RTL::SetupSignals.
	 */
	SignalHandler GetSignalHandler();

	/**
	 * @brief Vrátí bitovou masku se signály aktivovanými pro aktuální vlákno.
	 * @return Viz RTL::SetupSignals.
	 */
	uint32_t GetSignalMask();

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
	 * @brief Vrátí pracovní adresář aktuálního procesu.
	 * @return Řetězec s absolutní cestou k pracovnímu adresáři.
	 */
	std::string GetWorkingDirectory();

	/**
	 * @brief Nastaví pracovní adresář aktuálního procesu.
	 * @param path Řetězec s absolutní nebo relativní cestou k pracovnímu adresáři.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool SetWorkingDirectory(const char *path);

	inline bool SetWorkingDirectory(const std::string & path)
	{
		return SetWorkingDirectory(path.c_str());
	}

	/**
	 * @brief Vrátí ukazatel na strukturu prostředí aktuálního vlákna.
	 * Tato funkce by se neměla používat mimo RTL.
	 * @return Prostředí aktuálního vlákna.
	 */
	ThreadEnvironment *GetCurrentThreadEnv();

	/**
	 * @brief Nastaví návratový kód aktuálního procesu nebo vlákna.
	 * Tato funkce by se neměla používat mimo RTL.
	 * @param exitCode Návratový kód.
	 */
	void SetCurrentThreadExitCode(int exitCode);

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

	/**
	 * @brief Převede chybový kód na řetězec s popisem chyby.
	 * @param error Chybový kód.
	 * @return Textová reprezentace chybového kódu.
	 */
	std::string ErrorToString(kiv_os::NOS_Error error);

	/**
	 * @brief Vrátí řetězec s popisem chybového kódu pro aktuální vlákno.
	 * @return Textová reprezentace aktuálního chybového kódu.
	 */
	inline std::string GetLastErrorString()
	{
		return ErrorToString(GetLastError());
	}

	// =========================================
	// ==  Soubory a standardní vstup/výstup  ==
	// =========================================

	/**
	 * @brief Otevře nebo vytvoří soubor nebo adresář.
	 * @param path Absolutní nebo relativní cesta k souboru nebo adresáři.
	 * @param flags Možnosti otevření nebo vytvoření souboru nebo adresáře.
	 * @param attributes Atributy souboru nebo adresáře.
	 * @return Handle na otevřený soubor nebo adresář. Případně nula, pokud se otevření nebo vytvoření souboru nebo adresáře
	 * nezdařilo. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	kiv_os::THandle OpenFile(const char *path, kiv_os::NOpen_File flags, kiv_os::NFile_Attributes attributes);

	inline kiv_os::THandle OpenFile(const std::string & path, kiv_os::NOpen_File flags, kiv_os::NFile_Attributes attributes)
	{
		return OpenFile(path.c_str(), flags, attributes);
	}

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

	/**
	 * @brief Změní nebo získá aktuální pozici v souboru.
	 * Lze také použít pro zjištění nebo změnu velikosti souboru.
	 * @param file Handle na soubor.
	 * @param action Určuje, co se má provést (Get_Position, Set_Position, Set_Size).
	 * @param base Určuje, od jakého místa se má vypočítat výsledná pozice nebo velikost (Beginning, Current, End).
	 * @param pos Určuje, o kolik se má pozice nebo velikost změnit. Při Get_Position se sem uloží aktuální pozice.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool SeekFile(kiv_os::THandle file, kiv_os::NFile_Seek action, kiv_os::NFile_Seek base, int64_t & pos);

	/**
	 * @brief Odstraní soubor nebo prázdný adresář.
	 * @param path Absolutní nebo relativní cesta k souboru nebo adresáři.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool DeleteFile(const char *path);

	inline bool DeleteFile(const std::string & path)
	{
		return DeleteFile(path.c_str());
	}

	// ===============
	// ==  Ostatní  ==
	// ===============

	struct Pipe
	{
		kiv_os::THandle readEnd  = 0;
		kiv_os::THandle writeEnd = 0;

		operator bool() const
		{
			return readEnd && writeEnd;
		}
	};

	/**
	 * @brief Vytvoří jednosměrnou rouru.
	 * Pro odstranění roury ze systému je potřeba uzavřít oba konce pomocí RTL::CloseHandle.
	 * @return Handle na čtecí a zapisovací konec roury, případně nulové handly, pokud se rouru nepodařilo vytvořit. Chybový
	 * kód je možné získat pomocí RTL::GetLastError.
	 */
	Pipe CreatePipe();

	/**
	 * @brief Uzavře libovolný handle.
	 * @param handle Handle k uzavření.
	 * @return Pokud byl handle úspěšně uzavřen, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool CloseHandle(kiv_os::THandle handle);

	/**
	 * @brief Provede řádné ukončení celého systému.
	 * Všem spuštěným procesům je odeslán signál Terminate.
	 */
	void Shutdown();
}



// tento nesmysl zde musí zůstat, protože je použit v api.cpp, který nemůžeme měnit :(
namespace kiv_os_rtl
{
	// globální proměnná s číslem poslední chyby MUSÍ být thread-local, jinak je k ničemu
	// std::atomic rozhodně nestačí
	extern kiv_os::NOS_Error Last_Error;  // NEPOUŽÍVAT
}
