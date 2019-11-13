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

	return write_to_disk(diskNumber, 0, 6, buffer);
}

uint16_t Filesystem::GetFilesystemDescription(const std::uint8_t diskNumber,const kiv_hal::TDrive_Parameters parameters, Boot_record & bootRecord)
{
	load_boot_record(diskNumber, parameters, bootRecord);
	return is_valid_fat(bootRecord) ? FsError::SUCCESS : FsError::NO_FILE_SYSTEM;
}

uint16_t Filesystem::LoadDirContents(const std::uint8_t diskNumber, const  std::string fileName, std::vector<Directory>& dest)
{
	Boot_record fatBootRec;
	uint16_t opRes = 0;
	int dirCluster = 0;
	std::vector<std::string> filePathItems;
	Directory dir;
	Directory parentDir;
	kiv_hal::TDrive_Parameters parameters;

	// nacti parametry disku
	opRes = LoadDiskParameters(diskNumber, parameters);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti BR
	opRes = load_boot_record(diskNumber, parameters, fatBootRec);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// rozdel fileName na jmena
	Util::SplitPath(fileName, filePathItems);
	opRes = find_file(diskNumber, fatBootRec, filePathItems, dir, parentDir);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti itemy a vrat vysledek
	opRes = load_items_in_dir(diskNumber, fatBootRec, dir, dest);
	return opRes;
}

uint16_t Filesystem::ReadFileContents(const std::uint8_t diskNumber, const std::string fileName, char * buffer, size_t bufferLen)
{
	Boot_record fatBootRec;
	uint16_t opRes = 0;
	int dirCluster = 0;
	std::vector<std::string> filePathItems;
	Directory fileToRead;
	Directory parentDir;
	int32_t* fatTable = NULL;
	kiv_hal::TDrive_Parameters parameters;

	// nacti parametry disku
	opRes = LoadDiskParameters(diskNumber, parameters);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti BR
	opRes = load_boot_record(diskNumber, parameters, &fatBootRec);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti FAT
	fatTable = new int32_t[fatBootRec.usable_cluster_count];
	opRes = load_fat(diskNumber, fatBootRec, fatTable);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	// rozdel fileName na jmena
	Util::SplitPath(fileName, filePathItems);

	// najdi soubor
	opRes = find_file(diskNumber, fatBootRec, filePathItems, fileToRead, parentDir);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	// nacti soubor
	opRes = read_file(diskNumber, fatBootRec, fatTable, fileToRead, buffer, bufferLen);

	// uklid
	delete[] fatTable;

	return opRes;
}

uint16_t Filesystem::WriteFileContents(const std::uint8_t diskNumber, const std::string fileName, const uint32_t offset, char * buffer, size_t bufferLen)
{
	Boot_record fatBootRec;
	uint16_t opRes = 0;
	int dirCluster = 0;
	std::vector<std::string> filePathItems;
	Directory fileToWriteTo;
	Directory parentDir;
	int32_t* fatTable = NULL;
	kiv_hal::TDrive_Parameters parameters;

	// nacti parametry disku
	opRes = LoadDiskParameters(diskNumber, parameters);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti BR
	opRes = load_boot_record(diskNumber, parameters, &fatBootRec);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	// nacti FAT
	fatTable = new int32_t[fatBootRec.usable_cluster_count];
	opRes = load_fat(diskNumber, fatBootRec, fatTable);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	// najdi soubor
	// rozdel fileName na jmena
	Util::SplitPath(fileName, filePathItems);
	opRes = find_file(diskNumber, fatBootRec, filePathItems, fileToWriteTo, parentDir);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	opRes = write_file(diskNumber, fatBootRec, fatTable, fileToWriteTo, offset, buffer, bufferLen);

	delete[] fatTable;

	return opRes;
}

uint16_t Filesystem::CreateDirectory(const std::uint8_t diskNumber, const std::string dirName)
{
	return FsError::UNKNOWN_ERROR;
}

uint16_t Filesystem::LoadDiskParameters(const std::uint8_t diskNumber, kiv_hal::TDrive_Parameters & parameters)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Drive_Parameters);
	registers.rdi.r = reinterpret_cast<uint64_t>(&parameters);
	registers.rdx.l = diskNumber;
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);

	if (registers.flags.carry)
	{
		FsError::DISK_OPERATION_ERROR;
	}
	return FsError::SUCCESS;
}


