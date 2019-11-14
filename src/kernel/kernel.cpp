#include <windows.h>

#include "kernel.h"
#include "io.h"
#include "util.h"
#include "fat.h"
#include "filesystem.h"
#include "fserrors.h"

static void PrintMsg(const char *msg, size_t length)
{
	// TODO
	kiv_hal::TRegisters registers;
	registers.rax.l = static_cast<uint8_t>(kiv_os::NOS_File_System::Write_File);
	registers.rdi.r = reinterpret_cast<uint64_t>(msg);
	registers.rcx.r = length;
	Handle_IO(registers);
}

static void PrintMsg(const char *msg)
{
	PrintMsg(msg, strlen(msg));
}

static void PrintMsg(const std::string & msg)
{
	PrintMsg(msg.c_str(), msg.length());
}

/**
 * @brief Vstupní bod systémového volání.
 * @param context Registr rax.h by měl obsahovat hlavní číslo OS služby, která se má volat (syscall který se má vykonat).
 */
static void __stdcall SysCallEntry(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_os::NOS_Service_Major>(context.rax.h))
	{
		case kiv_os::NOS_Service_Major::File_System:
		{
			Handle_IO(context);
			break;
		}
		case kiv_os::NOS_Service_Major::Process:
		{
			// TODO
			break;
		}
	}
}

void __stdcall Bootstrap_Loader(kiv_hal::TRegisters & context)
{
	// nastavení handleru pro syscall interrupt
	kiv_hal::Set_Interrupt_Handler(kiv_os::System_Int_Number, SysCallEntry);

	// v rámci ukázky ještě vypíšeme dostupné disky
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

		PrintMsg("Nalezen disk ");
		PrintMsg(Util::NumberToHexString(registers.rdx.l));
		PrintMsg(" o velikosti ");
		PrintMsg(Util::NumberToString(diskSize));
		PrintMsg(" B\n");

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

	}

	HMODULE userDLL = LoadLibraryA("user.dll");
	if (!userDLL)
	{
		PrintMsg("Nelze nacist user.dll!\n");
		return;
	}

	// spustíme shell - v reálném OS bychom ovšem spouštěli init
	kiv_os::TThread_Proc shell = (kiv_os::TThread_Proc) GetProcAddress(userDLL, "shell");
	if (shell)
	{
		// správně se má shell spustit přes clone!
		// ale ten v kostře pochopitelně není implementován
		shell(context);
	}
	else
	{
		PrintMsg("Shell nenalezen.\n");
	}

	FreeLibrary(userDLL);
}
