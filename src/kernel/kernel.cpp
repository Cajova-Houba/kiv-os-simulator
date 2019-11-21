#include "kernel.h"
#include "syscall.h"
#include "process.h"

Kernel *Kernel::s_pInstance;

static void RunShell()
{
	HandleID console = Kernel::GetConsoleHandle().getID();

	HandleReference stdIn  = Kernel::GetHandleStorage().getHandle(console);
	HandleReference stdOut = Kernel::GetHandleStorage().getHandle(console);

	TEntryFunc entry = Kernel::GetUserDLL().getSymbol<TEntryFunc>("shell");
	if (!entry)
	{
		Kernel::Log("Shell nenalezen!");
		return;
	}

	Process::Create("shell", "", Path::Parse("C:"), entry, std::move(stdIn), std::move(stdOut), true);
}

// vstupní funkce jádra
void __stdcall Bootstrap_Loader(kiv_hal::TRegisters &)
{
	// inicializace kernelu
	Kernel kernel;

	// nastavení handleru pro syscall interrupt
	kiv_hal::Set_Interrupt_Handler(kiv_os::System_Int_Number, SysCall::Entry);

	// inicializace disků
	Kernel::GetFileSystem().init();

	// načtení uživatelského prostoru
	if (!Kernel::GetUserDLL().load("user.dll"))
	{
		Kernel::Log("Nelze nacist user.dll!");
		return;
	}

	// předání řízení shellu
	RunShell();
}
