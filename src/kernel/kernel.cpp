#include "kernel.h"
//#include "util.h"
//#include "fat.h"
//#include "filesystem.h"
//#include "fserrors.h"
#include "syscall.h"
#include "process.h"

Kernel *Kernel::s_pInstance;

static void RunShell()
{
	HandleID console = Kernel::GetConsoleHandle().getID();

	HandleReference stdIn  = Kernel::GetHandleStorage().getHandle(console);
	HandleReference stdOut = Kernel::GetHandleStorage().getHandle(console);

	kiv_os::TThread_Proc entry = Kernel::GetUserDLL().getSymbol<kiv_os::TThread_Proc>("shell");
	if (!entry)
	{
		Kernel::Log("Shell nenalezen!");
		return;
	}

	Process::Create("shell", "", Path::Parse("C:"), entry, std::move(stdIn), std::move(stdOut), true);
}

void __stdcall Bootstrap_Loader(kiv_hal::TRegisters &)
{
	// inicializace globálních komponent kernelu
	Kernel kernel;

	// nastavení handleru pro syscall interrupt
	kiv_hal::Set_Interrupt_Handler(kiv_os::System_Int_Number, SysCall::Entry);

	// inicializace disků
	Kernel::GetFileSystem().init();

	// v rámci ukázky ještě vypíšeme dostupné disky
	// TODO: tohle odstranit
	for (int i = 0; i < 256; i++)
	{
		kiv_hal::TDrive_Parameters params;

		kiv_hal::TRegisters registers;
		registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Drive_Parameters);
		registers.rdi.r = reinterpret_cast<uint64_t>(&params);
		registers.rdx.l = i;
		kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);

		if (registers.flags.carry)
		{
			continue;
		}

		const uint64_t diskSize = params.absolute_number_of_sectors * params.bytes_per_sector;

		Kernel::Log("Nalezen disk 0x%hhX o velikosti %llu B", registers.rdx.l, diskSize);

/*
		// inicializace/kontrola pritomnosti FS
		Boot_record fsBootRec;
		uint16_t status = Filesystem::GetFilesystemDescription(i, params, fsBootRec);

		if (FsError::NO_FILE_SYSTEM == status) {
			PrintMsg("Nenalezen zadny filesystem.\n");
			status = Filesystem::InitNewFileSystem(i, params);
			if (FsError::SUCCESS != status) {
				// chyba pri inicializaci
				PrintMsg("Chyba pri inicializaci FS: ");
				PrintMsg(Util::NumberToString(status));
				PrintMsg("\n");
			}
			else {
				// ok
				PrintMsg("Filesystem inicializovan na disku ");
				PrintMsg(Util::NumberToHexString(i));
				PrintMsg("\n");
			}
		}
		else if (FsError::SUCCESS == status) {
			int count = 0;

			PrintMsg("Nalezen boot record fat.\n");
			PrintMsg("Deskriptor: '");
			PrintMsg(fsBootRec.volume_descriptor);
			PrintMsg("'\n");
			PrintMsg("Pouzitelnych clusteru: ");
			PrintMsg(Util::NumberToString(fsBootRec.usable_cluster_count));
			PrintMsg("\n");
			PrintMsg("Bytes per sector: ");
			PrintMsg(Util::NumberToString(fsBootRec.bytes_per_sector));
			PrintMsg("\n");

			// tohle je jen testovaci vypis a v ostre verzi bude smazan
			std::vector<Directory> itemsInRoot;
			Filesystem::LoadDirContents(i, "/", itemsInRoot);
			PrintMsg("Pocet itemu v root addr: ");
			PrintMsg(Util::NumberToString(itemsInRoot.size()));
			PrintMsg("\n");
			for (size_t i = 0; i < itemsInRoot.size(); i++)
			{
				PrintMsg("- ");
				PrintMsg(itemsInRoot[i].name);
				PrintMsg("\n");
			}

			char fileBuffer[4*4096];
			uint16_t isReadError = Filesystem::ReadFileContents(i, "/pohadka.txt", fileBuffer, 547);
			fileBuffer[547-1] = '\0';
			if (!isReadError) {
				PrintMsg("Obsah souboru /pohadka.txt: \n");
				PrintMsg(fileBuffer);
				PrintMsg("\n");
			}
			else {
				PrintMsg("Chyba pri cteni souboru: ");
				PrintMsg(Util::NumberToString(isReadError));
				PrintMsg("\n");
			}
			
		}
		else {
			PrintMsg("Chyba pri cteni FS: ");
			PrintMsg(Util::NumberToString(status));
			PrintMsg("\n");
		}
*/

	}

	if (!Kernel::GetUserDLL().load("user.dll"))
	{
		Kernel::Log("Nelze nacist user.dll!");
		return;
	}

	RunShell();
}
