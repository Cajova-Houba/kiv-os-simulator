#include "filesystem.h"
#include "../api/hal.h"
#include <cstring>
#include <string>
#include <cmath>
#include "util.h"

uint16_t Filesystem::InitNewFileSystem(const std::uint8_t diskNumber, const  kiv_hal::TDrive_Parameters parameters)
{
	return init_fat(diskNumber, parameters);
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
	uint32_t matchCounter;

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
	opRes = find_file(diskNumber, fatBootRec, filePathItems, dir, parentDir, matchCounter);
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
	uint32_t matchCounter;

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
	opRes = find_file(diskNumber, fatBootRec, filePathItems, fileToRead, parentDir, matchCounter);
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
	uint32_t matchCounter;

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
	opRes = find_file(diskNumber, fatBootRec, filePathItems, fileToWriteTo, parentDir, matchCounter);
	if (opRes != FsError::SUCCESS) {
		delete[] fatTable;
		return opRes;
	}

	opRes = write_file(diskNumber, fatBootRec, fatTable, fileToWriteTo, offset, buffer, bufferLen);

	delete[] fatTable;

	// update zaznamu souboru v parent adresari
	opRes = update_file_in_dir(diskNumber, fatBootRec, parentDir, fileToWriteTo);

	return opRes;
}

uint16_t Filesystem::CreateDirectory(const std::uint8_t diskNumber, const std::string dirName)
{
	return _CreateFileInternal(diskNumber, dirName, false);
}

uint16_t Filesystem::CreateFile(const std::uint8_t diskNumber, const std::string fileName)
{
	return _CreateFileInternal(diskNumber, fileName, true);
}

uint16_t Filesystem::DeleteFile(const std::uint8_t diskNumber, const std::string fileName)
{
	// todo:
	return uint16_t();
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


uint16_t Filesystem::_CreateFileInternal(std::uint8_t diskNumber, const std::string dirName, const bool isFile) {
	Boot_record fatBootRec;
	uint16_t isError = 0;
	int dirCluster = 0;
	std::vector<std::string> filePathItems;
	Directory newFile,
		parentDir,
		tmp;
	int32_t* fatTable = NULL;
	kiv_hal::TDrive_Parameters parameters;
	uint32_t matchCounter;

	// rozdel fileName na jmena
	Util::SplitPath(dirName, filePathItems);
	if (filePathItems.size() == 0) {
		// nemuzeme znova vytvorit root
		return FsError::UNKNOWN_ERROR;
	}

	if (filePathItems[filePathItems.size() - 1].size() > MAX_NAME_LEN - 1) {
		return FsError::FILE_NAME_TOO_LONG;
	}

	// nacti parametry disku
	isError = LoadDiskParameters(diskNumber, parameters);
	if (isError) {
		return isError;
	}

	// nacti BR
	isError = load_boot_record(diskNumber, parameters, fatBootRec);
	if (isError) {
		return isError;
	}

	// nacti FAT
	fatTable = new int32_t[fatBootRec.usable_cluster_count];
	isError = load_fat(diskNumber, fatBootRec, fatTable);
	if (isError) {
		delete[] fatTable;
		return isError;
	}

	// posledni item z filePathItems a pouzij ho jako jmeno noveho souboru
	memset(newFile.name, 0, MAX_NAME_LEN);
	memcpy(newFile.name, filePathItems[filePathItems.size() - 1].c_str(), MAX_NAME_LEN - 1);

	// najdi soubor a jeho rodicovksy adresar
	// pokud metoda vrati FILE_NOT_FOUND a matchCounter bude roven filePathItems.size() - 1
	// vime ze rodicovsky adresar byl nalezen a lze v nem vytvori cilovy soubor
	// pokud metoda vrati SUCCESS, vime ze cilovy soubor jiz existuje a je treba vratit chybu
	isError = find_file(diskNumber, fatBootRec, filePathItems, tmp, parentDir, matchCounter);
	if (isError == FsError::SUCCESS) {
		delete[] fatTable;
		return FsError::FILE_ALREADY_EXISTS;
	} else if (isError == FsError::FILE_NOT_FOUND &&
		(matchCounter != filePathItems.size() - 1)) {
		// soubor nenalezen a match counter ma spatnou hodnotu -> chybi cast zadane cesty
		delete[] fatTable;
		return isError;
	}

	// adresar/soubor
	newFile.isFile = isFile;
	isError = create_file(diskNumber, fatBootRec, fatTable, parentDir, newFile);
	delete[] fatTable;

	return isError;
}

