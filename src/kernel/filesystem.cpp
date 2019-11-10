#include "filesystem.h"
#include "../api/hal.h"
#include <cstring>
#include <string>
#include <cmath>
#include "util.h"

uint16_t Filesystem::InitNewFileSystem(const std::uint8_t diskNumber, const  kiv_hal::TDrive_Parameters parameters)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;
	// todo: velikost bufferu by mela byt rizena podle parametru
	uint64_t bytesPerSector = parameters.bytes_per_sector;
	char buffer[3072] = {0};		// 6 sektoru, to je minimum, 2888 B je potreba pro zapis struktury FAT

	// vytvori boot rec pro FAT a samotnou FAT
	init_fat(parameters, buffer);

	addressPacket.lba_index = 0;			// zacni na sektoru 0
	addressPacket.count = 6;				// zapis 6 sektoru
	addressPacket.sectors = buffer;			// data pro zapis na disk

	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Write_Sectors);		// jakou operaci nad diskem provest
	registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);					// info pro cteni dat
	registers.rdx.l = diskNumber;													// cislo disku na ktery zapisovat
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);		// syscall pro praci s diskem
	if (registers.flags.carry) {
		// chyba pri zapisu na disk
		return FsError::DISK_OPERATION_ERROR;
	}
	return FsError::SUCCESS;
}

uint16_t Filesystem::GetFilesystemDescription(const std::uint8_t diskNumber,const  kiv_hal::TDrive_Parameters parameters, Boot_record* bootRecord)
{
	load_boot_record(diskNumber, parameters, bootRecord);
	return is_valid_fat(bootRecord) ? FsError::SUCCESS : FsError::NO_FILE_SYSTEM;
}

uint16_t Filesystem::LoadDirContents(const std::uint8_t diskNumber, const  kiv_hal::TDrive_Parameters parameters, const  std::string fileName, std::vector<Directory>& dest)
{
	Boot_record fatBootRec;
	uint16_t opRes = 0;
	int dirCluster = 0;
	std::vector<std::string> filePathItems;
	Directory dir;
	Directory parentDir;

	// nacti BR
	opRes = load_boot_record(diskNumber, parameters, &fatBootRec);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// rozdel fileName na jmena
	Util::SplitPath(fileName, filePathItems);
	opRes = find_file(diskNumber, parameters, fatBootRec, filePathItems, dir, parentDir);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti itemy a vrat vysledek
	opRes = load_items_in_dir(diskNumber, parameters, fatBootRec, dir, dest);
	return opRes;
}

uint16_t Filesystem::ReadFileContents(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, const std::string fileName, char * buffer)
{
	Boot_record fatBootRec;
	uint16_t opRes = 0;
	int dirCluster = 0;
	std::vector<std::string> filePathItems;
	Directory fileToRead;
	Directory parentDir;
	int32_t* fatTable = NULL;

	// nacti BR
	opRes = load_boot_record(diskNumber, parameters, &fatBootRec);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti FAT
	fatTable = new int32_t[fatBootRec.usable_cluster_count];
	opRes = load_fat(diskNumber, parameters, fatBootRec, fatTable);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	// rozdel fileName na jmena
	Util::SplitPath(fileName, filePathItems);

	// najdi soubor
	opRes = find_file(diskNumber, parameters, fatBootRec, filePathItems, fileToRead, parentDir);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	// nacti soubor
	opRes = read_file(diskNumber, parameters, fatBootRec, fatTable, fileToRead, buffer);

	// uklid
	delete[] fatTable;

	return opRes;
}

void Filesystem::countRoot(std::uint8_t diskNumber, kiv_hal::TDrive_Parameters params, Boot_record & bootRec, int & cnt)
{
	Directory root;
	std::vector<Directory> items;
	root.start_cluster = ROOT_CLUSTER;
	root.isFile = false;
	load_items_in_dir(diskNumber, params, bootRec, root, items);
	cnt = items.size();
}


