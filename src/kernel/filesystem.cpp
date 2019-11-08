#include "filesystem.h"
#include "../api/hal.h"
#include <cstring>
#include <string>
#include <cmath>
#include "util.h"

uint16_t Filesystem::InitNewFileSystem(std::uint8_t diskNumber, kiv_hal::TDrive_Parameters parameters)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;
	uint64_t bytesPerSector = parameters.bytes_per_sector;
	char buffer[3072] = {0};		// 6 sektoru, to je minimum, 2888 B je potreba pro zapis struktury FAT

	// vytvori boot rec pro FAT a samotnou FAT
	init_fat(buffer, parameters);

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

uint16_t Filesystem::GetFilesystemDescription(std::uint8_t diskNumber, kiv_hal::TDrive_Parameters parameters, Boot_record* bootRecord)
{
	load_boot_record(diskNumber, parameters, bootRecord);
	return is_valid_fat(bootRecord) ? FsError::SUCCESS : FsError::NO_FILE_SYSTEM;
}

uint16_t Filesystem::LoadDirContents(std::uint8_t diskNumber, kiv_hal::TDrive_Parameters parameters, std::string fileName, std::vector<Directory>& dest)
{
	Boot_record fatBootRec;
	uint16_t opRes = 0;
	int dirCluster = 0;
	uint32_t* fatTable;
	std::vector<std::string> filePathItems;

	// nacti BR
	opRes = load_boot_record(diskNumber, parameters, &fatBootRec);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// BR nacten, nacti FAT
	fatTable = new uint32_t[fatBootRec.usable_cluster_count];
	opRes = load_fat(diskNumber, parameters, fatBootRec, NULL);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	// rozdel fileName na jmena
	Util::SplitPath(fileName, filePathItems);



	// todo
	return FsError::UNKNOWN_ERROR;
}

void Filesystem::countRoot(std::uint8_t diskNumber, kiv_hal::TDrive_Parameters params, Boot_record & bootRec, int & cnt)
{
	Directory root;
	root.start_cluster = ROOT_CLUSTER;
	root.isFile = false;
	count_items_in_dir(diskNumber, params, bootRec, root, cnt);
}


