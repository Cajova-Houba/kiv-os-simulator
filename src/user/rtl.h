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
	pEnv->process.stdIn   = static_cast<RTL::Handle>(context.rax.x);\
	pEnv->process.stdOut  = static_cast<RTL::Handle>(context.rbx.x);\
	pEnv->process.cmdLine = reinterpret_cast<const char*>(context.rdi.r);\
	RTL::SetCurrentThreadExitCode(NAME##_main(RTL::GetProcessCmdLine()));\
	return 0;\
}
// =========================================================================================

namespace RTL
{
	// invalid handle je nula
	using Handle = uint16_t;  // kiv_os::THandle

	enum struct Error : uint16_t  // kiv_os::NOS_Error
	{
		SUCCESS = 0,
		INVALID_ARGUMENT,
		FILE_NOT_FOUND,
		DIRECTORY_NOT_EMPTY,
		NOT_ENOUGH_DISK_SPACE,
		OUT_OF_MEMORY,
		PERMISSION_DENIED,
		IO_ERROR,

		UNKNOWN_ERROR = 0xFFFF
	};

	/**
	 * @brief Uzavře libovolný handle.
	 * @param handle Handle k uzavření.
	 * @return Pokud byl handle úspěšně uzavřen, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool CloseHandle(Handle handle);

	// ========================
	// ==  Procesy a vlákna  ==
	// ========================

	enum struct Signal : uint8_t  // kiv_os::NSignal_Id
	{
		TERMINATE = 15  // SIGTERM
	};

	using ThreadMain = int (*)(void *param);
	using SignalHandler = void (*)(Signal signal);

	struct ProcessEnvironment
	{
		Handle stdIn = 0;
		Handle stdOut = 0;
		const char *cmdLine = "";
	};

	struct ThreadEnvironment
	{
		Error lastError = Error::SUCCESS;
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
	Handle CreateProcess(const char *program, const ProcessEnvironment & env);

	inline Handle CreateProcess(const std::string & program, const ProcessEnvironment & env)
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
	Handle CreateThread(ThreadMain mainFunc, void *param);

	/**
	 * @brief Blokuje, dokud se některý ze zadaných procesů nebo vláken neukončí.
	 * @param handles Pole obsahující handle procesů nebo vláken, na které se má čekat.
	 * @param count Celkový počet procesů nebo vláken, na které se má čekat.
	 * @return Index handle procesu nebo vlákna, který je ukončený nebo se ukončil během čekání. Pokud došlo k chybě, tak -1.
	 * Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	int WaitForMultiple(const Handle *handles, uint16_t count);

	inline int WaitForMultiple(const std::vector<Handle> & handles)
	{
		return WaitForMultiple(handles.data(), static_cast<uint16_t>(handles.size()));
	}

	inline bool WaitForSingle(Handle handle)
	{
		return WaitForMultiple(&handle, 1) == 0;
	}

	/**
	 * @brief Vrátí návratový kód procesu nebo vlákna.
	 * Pokud proces nebo vlákno stále běží, tak tato funkce vrací vždy 0 s chybovým kódem Error::SUCCESS.
	 * Pokud parametr handle není validní handle na existující proces nebo vlákno, tak návratový kód je také 0, ale chybový
	 * kód se nastaví na Error::INVALID_ARGUMENT.
	 * @param handle Handle na proces nebo vlákno.
	 * @return Návratový kód procesu nebo vlákna.
	 */
	int GetExitCode(Handle handle);

	struct Process
	{
		Handle handle = 0;
		Handle stdIn = 0;
		Handle stdOut = 0;
		std::string name;
		std::string cmdLine;

		~Process()
		{
			if (isStarted())
			{
				CloseHandle(handle);
			}
		}

		bool isStarted() const
		{
			return handle != 0;
		}

		bool start()
		{
			ProcessEnvironment env;
			env.stdIn   = stdIn;
			env.stdOut  = stdOut;
			env.cmdLine = cmdLine.c_str();

			handle = CreateProcess(name, env);

			return isStarted();
		}

		bool waitFor()
		{
			return WaitForSingle(handle);
		}

		int getExitCode()
		{
			return GetExitCode(handle);
		}
	};

	struct Thread
	{
		Handle handle = 0;
		ThreadMain mainFunc = nullptr;
		void *param = nullptr;

		~Thread()
		{
			if (isStarted())
			{
				CloseHandle(handle);
			}
		}

		bool isStarted() const
		{
			return handle != 0;
		}

		bool start()
		{
			handle = CreateThread(mainFunc, param);

			return isStarted();
		}

		bool join()
		{
			return WaitForSingle(handle);
		}

		int getExitCode()
		{
			return GetExitCode(handle);
		}
	};

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
	Handle GetStdInHandle();

	/**
	 * @brief Vrátí handle na standardní výstup aktuálního procesu.
	 * @return Handle na standardní výstup.
	 */
	Handle GetStdOutHandle();

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
	Error GetLastError();

	/**
	 * @brief Nastaví chybový kód pro aktuální vlákno.
	 * @param error Chybový kód.
	 */
	void SetLastError(Error error);

	/**
	 * @brief Převede chybový kód na řetězec s popisem chyby.
	 * @param error Chybový kód.
	 * @return Textová reprezentace chybového kódu.
	 */
	std::string ErrorToString(Error error);

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

	namespace FileAttributes  // kiv_os::NFile_Attributes
	{
		enum
		{
			READ_ONLY   = (1 << 0),
			HIDDEN      = (1 << 1),
			SYSTEM_FILE = (1 << 2),
			VOLUME_ID   = (1 << 3),
			DIRECTORY   = (1 << 4),
			ARCHIVE     = (1 << 5)
		};
	}

	// přesně stejnou strukturu musí používat i jádro!
	// to svinstvo z api.h s délkou názvu omezenou na 8+1+3 znaků používat nebudem
	// dokonce tam ani není místo pro ukončovací nulový znak, takže pracovat s takovou věcí by byla opravdu radost...
	struct DirectoryEntry
	{
		uint16_t attributes;  // FileAttributes
		char name[62];        // řetězec ukončený nulou, reálné moderní OS používají většinou 240 až 256 znaků
		// velikost celé struktury zarovnaná na 64 bajtů
	};

	/**
	 * @brief Otevře soubor.
	 * @param path Absolutní nebo relativní cesta k souboru.
	 * @param readOnly True, pokud se má soubor otevřít pouze pro čtení, jinak false.
	 * @return Handle na otevřený soubor nebo nula, pokud se otevření souboru nezdařilo. Chybový kód je možné získat pomocí
	 * RTL::GetLastError.
	 */
	Handle OpenFile(const char *path, bool readOnly = false);

	inline Handle OpenFile(const std::string & path, bool readOnly = false)
	{
		return OpenFile(path.c_str(), readOnly);
	}

	/**
	 * @brief Otevře adresář.
	 * @param path Absolutní nebo relativní cesta k adresáři.
	 * @return Handle na otevřený adresář nebo nula, pokud se otevření adresáře nezdařilo. Chybový kód je možné získat pomocí
	 * RTL::GetLastError.
	 */
	Handle OpenDirectory(const char *path);

	inline Handle OpenDirectory(const std::string & path)
	{
		return OpenDirectory(path.c_str());
	}

	/**
	 * @brief Vytvoří nebo otevře existující soubor.
	 * @param path Absolutní nebo relativní cesta k souboru.
	 * @return Handle na vytvořený nebo existující soubor nebo nula, pokud se vytvoření nebo otevření souboru nezdařilo.
	 * Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	Handle CreateFile(const char *path);

	inline Handle CreateFile(const std::string & path)
	{
		return CreateFile(path.c_str());
	}

	/**
	 * @brief Vytvoří nebo otevře existující adresář.
	 * @param path Absolutní nebo relativní cesta k adresáři.
	 * @return Handle na vytvořený nebo existující adresář nebo nula, pokud se vytvoření nebo otevření adresáře nezdařilo.
	 * Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	Handle CreateDirectory(const char *path);

	inline Handle CreateDirectory(const std::string & path)
	{
		return CreateDirectory(path.c_str());
	}

	/**
	 * @brief Získá obsah adresáře.
	 * @param handle Handle na adresář.
	 * @param result Výsledné položky v adresáři.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool GetDirectoryContent(Handle handle, std::vector<DirectoryEntry> & result);

	/**
	 * @brief Načte data ze souboru.
	 * @param file Deskriptor souboru.
	 * @param buffer Buffer pro uložení načtených dat.
	 * @param size Velikost bufferu pro uložení načtených dat v bajtech.
	 * @param pRead Volitelný ukazatel na proměnnou, kam se uloží počet načtených bajtů. Může být null.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool ReadFile(Handle file, void *buffer, size_t size, size_t *pRead = nullptr);

	/**
	 * @brief Načte data ze standardního vstupu procesu.
	 * @param buffer Buffer pro uložení načtených dat.
	 * @param size Velikost bufferu pro uložení načtených dat v bajtech.
	 * @param pRead Volitelný ukazatel na proměnnou, kam se uloží počet načtených bajtů. Může být null.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool ReadStdIn(void *buffer, size_t size, size_t *pRead = nullptr)
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
	bool WriteFile(Handle file, const void *buffer, size_t size, size_t *pWritten = nullptr);

	/**
	 * @brief Zapíše řetězec ukončený nulou do souboru.
	 * @param file Deskriptor souboru.
	 * @param string Řetězec k zapsání.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteFile(Handle file, const char *string)
	{
		return WriteFile(file, string, std::strlen(string));
	}

	/**
	 * @brief Zapíše řetězec do souboru.
	 * @param file Deskriptor souboru.
	 * @param string Řetězec k zapsání.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	inline bool WriteFile(Handle file, const std::string & string)
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
	inline bool WriteStdOut(const void *buffer, size_t size, size_t *pWritten = nullptr)
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

	enum struct Position
	{
		BEGIN,    //!< Začátek souboru.
		CURRENT,  //!< Aktuální pozice v souboru.
		END       //!< Konec souboru.
	};

	/**
	 * @brief Získá aktuální pozici v souboru.
	 * @param file Handle na soubor.
	 * @param result Proměnná pro uložení hodnoty pozice.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool GetFilePos(Handle file, int64_t & result);

	/**
	 * @brief Nastaví pozici v souboru.
	 * @param file Handle na soubor.
	 * @param pos Hodnota pozice, která se má nastavit.
	 * @param base Referenční bod výsledné pozice.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool SetFilePos(Handle file, int64_t pos, Position base = Position::BEGIN);

	/**
	 * @brief Nastaví pozici a velikost souboru.
	 * Velikost souboru se nastaví na výslednou pozici.
	 * @param file Handle na soubor.
	 * @param pos Hodnota pozice, která se má nastavit.
	 * @param base Referenční bod výsledné pozice.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool SetFileSize(Handle file, int64_t pos, Position base = Position::BEGIN);

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

	/**
	 * @brief Odstraní adresář.
	 * @param path Absolutní nebo relativní cesta k adresáři.
	 * @param recursively True, pokud se má rekurzivně odstranit i případný obsah adresáře, jinak false.
	 * @return Pokud vše proběhlo v pořádku, tak true, jinak false. Chybový kód je možné získat pomocí RTL::GetLastError.
	 */
	bool DeleteDirectory(const char *path, bool recursively = true);

	inline bool DeleteDirectory(const std::string & path, bool recursively = true)
	{
		return DeleteDirectory(path.c_str(), recursively);
	}

	struct File
	{
		Handle handle = 0;

		~File()
		{
			close();
		}

		bool open(const char *path, bool readOnly = false)
		{
			close();

			handle = OpenFile(path, readOnly);

			return isOpen();
		}

		bool open(const std::string & path, bool readOnly = false)
		{
			close();

			handle = OpenFile(path, readOnly);

			return isOpen();
		}

		bool create(const char *path)
		{
			close();

			handle = CreateFile(path);

			return isOpen();
		}

		bool create(const std::string & path)
		{
			close();

			handle = CreateFile(path);

			return isOpen();
		}

		bool isOpen() const
		{
			return handle != 0;
		}

		explicit operator bool() const
		{
			return isOpen();
		}

		bool read(void *buffer, size_t size, size_t *pRead = nullptr)
		{
			return ReadFile(handle, buffer, size, pRead);
		}

		bool write(const void *buffer, size_t size, size_t *pWritten = nullptr)
		{
			return WriteFile(handle, buffer, size, pWritten);
		}

		bool write(const char *string)
		{
			return WriteFile(handle, string);
		}

		bool write(const std::string & string)
		{
			return WriteFile(handle, string);
		}

		int64_t getPos()
		{
			int64_t result = 0;
			GetFilePos(handle, result);

			return result;
		}

		bool setPos(int64_t pos, Position base = Position::BEGIN)
		{
			return SetFilePos(handle, pos, base);
		}

		bool setSize(int64_t pos, Position base = Position::BEGIN)
		{
			return SetFileSize(handle, pos, base);
		}

		void close()
		{
			if (handle)
			{
				CloseHandle(handle);
				handle = 0;
			}
		}
	};

	struct Directory
	{
		Handle handle = 0;

		~Directory()
		{
			close();
		}

		bool open(const char *path)
		{
			close();

			handle = OpenDirectory(path);

			return isOpen();
		}

		bool open(const std::string & path)
		{
			close();

			handle = OpenDirectory(path);

			return isOpen();
		}

		bool create(const char *path)
		{
			close();

			handle = CreateDirectory(path);

			return isOpen();
		}

		bool create(const std::string & path)
		{
			close();

			handle = CreateDirectory(path);

			return isOpen();
		}

		bool isOpen() const
		{
			return handle != 0;
		}

		explicit operator bool() const
		{
			return isOpen();
		}

		void close()
		{
			if (handle)
			{
				CloseHandle(handle);
				handle = 0;
			}
		}

		std::vector<DirectoryEntry> getContent()
		{
			std::vector<DirectoryEntry> result;
			GetDirectoryContent(handle, result);

			return result;
		}
	};

	// ===============
	// ==  Ostatní  ==
	// ===============

	struct Pipe
	{
		Handle readEnd = 0;
		Handle writeEnd = 0;

		~Pipe()
		{
			closeWriteEnd();
			closeReadEnd();
		}

		bool isReadEndOpen() const
		{
			return readEnd != 0;
		}

		bool isWriteEndOpen() const
		{
			return writeEnd != 0;
		}

		bool isOpen() const
		{
			return isReadEndOpen() && isWriteEndOpen();
		}

		explicit operator bool() const
		{
			return isOpen();
		}

		void closeReadEnd()
		{
			if (readEnd)
			{
				CloseHandle(readEnd);
				readEnd = 0;
			}
		}

		void closeWriteEnd()
		{
			if (writeEnd)
			{
				CloseHandle(writeEnd);
				writeEnd = 0;
			}
		}
	};

	/**
	 * @brief Vytvoří jednosměrnou rouru.
	 * @return Otevřenou rouru nebo uzavřenou rouru, pokud se rouru nepodařilo vytvořit. Chybový kód je možné získat pomocí
	 * RTL::GetLastError.
	 */
	Pipe CreatePipe();

	/**
	 * @brief Provede řádné ukončení celého systému.
	 * Všem spuštěným procesům je odeslán signál Signal::TERMINATE.
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
