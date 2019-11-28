// TODO: tento soubor odstranit
#include "filesystem.h"
#include "../api/hal.h"
#include <cstring>
#include <string>
#include <cmath>
#include <sstream>
#include <vector>

namespace Util
{
	inline void SplitPath(std::string filePath, std::vector<std::string> & dest) {
		// todo: podpora pro relativni cesty
		std::istringstream iss{ filePath };
		std::string item;
		bool first = true;
		while (std::getline(iss, item, '/')) {
			if (!first) {
				dest.push_back(item);
			}
			first = false;
		}
	}
}

namespace Filesystem {

	// 'private' funkce
	namespace {
		uint16_t _CreateFileInternal(std::uint8_t diskNumber, const std::string dirName, uint8_t flags) {
			Boot_record fatBootRec;
			uint16_t isError = 0;
			std::vector<std::string> filePathItems;
			Directory newFile,
				parentDir,
				tmp;
			std::vector<int32_t> fatTable;
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
			fatTable.resize(fatBootRec.usable_cluster_count);

			// nacti FAT
			isError = load_fat(diskNumber, fatBootRec, &(fatTable[0]));
			if (isError) {
				return isError;
			}

			// posledni item z filePathItems a pouzij ho jako jmeno noveho souboru
			memset(newFile.name, 0, MAX_NAME_LEN);
			memcpy(newFile.name, filePathItems[filePathItems.size() - 1].c_str(), MAX_NAME_LEN - 1);

			// najdi soubor a jeho rodicovksy adresar
			// pokud metoda vrati FILE_NOT_FOUND a matchCounter bude roven filePathItems.size() - 1
			// vime ze rodicovsky adresar byl nalezen a lze v nem vytvori cilovy soubor
			// pokud metoda vrati SUCCESS, vime ze cilovy soubor jiz existuje a je treba vratit chybu
			isError = find_file(diskNumber, fatBootRec, &(fatTable[0]), filePathItems, tmp, parentDir, matchCounter);
			if (isError == FsError::SUCCESS) {
				return FsError::FILE_ALREADY_EXISTS;
			}
			else if (isError == FsError::FILE_NOT_FOUND &&
				(matchCounter != filePathItems.size() - 1)) {
				// soubor nenalezen a match counter ma spatnou hodnotu -> chybi cast zadane cesty
				return isError;
			}

			// adresar/soubor
			newFile.flags = flags;
			isError = create_file(diskNumber, fatBootRec, &(fatTable[0]), parentDir, newFile);

			return isError;
		}

		uint16_t _LoadFat(const std::uint8_t diskNumber, const Boot_record &fatBootRec, std::vector<int32_t> & fatTable) {
			fatTable.resize(fatBootRec.usable_cluster_count);
			return load_fat(diskNumber, fatBootRec, &(fatTable[0]));
		}
	}
	
	uint16_t InitNewFileSystem(const std::uint8_t diskNumber, const  kiv_hal::TDrive_Parameters parameters)
	{
		return init_fat(diskNumber, parameters);
	}

	uint16_t GetFilesystemDescription(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, Boot_record & bootRecord)
	{
		uint16_t isError = load_boot_record(diskNumber, parameters, bootRecord);
		if (isError) {
			return isError;
		}
		return is_valid_fat(bootRecord) ? FsError::SUCCESS : FsError::NO_FILE_SYSTEM;
	}

	uint16_t LoadDirContents(const std::uint8_t diskNumber, const  std::string fileName, std::vector<Directory>& dest)
	{
		Boot_record fatBootRec;
		uint16_t isError = 0;
		std::vector<std::string> filePathItems;
		Directory dir;
		Directory parentDir;
		kiv_hal::TDrive_Parameters parameters;
		uint32_t matchCounter;
		std::vector<int32_t> fatTable;

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
		isError = _LoadFat(diskNumber, fatBootRec, fatTable);
		if (isError) {
			return isError;
		}

		// rozdel fileName na jmena
		Util::SplitPath(fileName, filePathItems);
		isError = find_file(diskNumber, fatBootRec, &(fatTable[0]), filePathItems, dir, parentDir, matchCounter);
		if (isError) {
			return isError;
		}

		// nacti itemy a vrat vysledek
		isError = load_items_in_dir(diskNumber, fatBootRec, &(fatTable[0]), dir, dest);
		return isError;
	}

	uint16_t ReadFileContents(const std::uint8_t diskNumber, const std::string fileName, char * buffer, const size_t bufferLen, const size_t offset)
	{
		Boot_record fatBootRec;
		uint16_t isError = 0;
		std::vector<std::string> filePathItems;
		std::vector<int32_t> fatTable;
		Directory fileToRead;
		Directory parentDir;
		kiv_hal::TDrive_Parameters parameters;
		uint32_t matchCounter;
		size_t readBytes = 0;

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
		isError = _LoadFat(diskNumber, fatBootRec, fatTable);
		if (isError) {
			return isError;
		}

		// rozdel fileName na jmena
		Util::SplitPath(fileName, filePathItems);

		// najdi soubor
		isError = find_file(diskNumber, fatBootRec, &(fatTable[0]), filePathItems, fileToRead, parentDir, matchCounter);
		if (isError) {
			return isError;
		}

		// nacti soubor
		isError = read_file(diskNumber, fatBootRec, &(fatTable[0]), fileToRead, buffer, bufferLen, readBytes, offset);

		return isError;
	}

	uint16_t WriteFileContents(const std::uint8_t diskNumber, const std::string fileName, char* buffer, const size_t bufferLen, const size_t offset)
	{
		Boot_record fatBootRec;
		uint16_t isError = 0;
		std::vector<std::string> filePathItems;
		Directory fileToWriteTo;
		Directory parentDir;
		std::vector<int32_t> fatTable;
		kiv_hal::TDrive_Parameters parameters;
		uint32_t matchCounter;

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
		isError = _LoadFat(diskNumber, fatBootRec, fatTable);
		if (isError) {
			return isError;
		}

		// najdi soubor
		// rozdel fileName na jmena
		Util::SplitPath(fileName, filePathItems);
		isError = find_file(diskNumber, fatBootRec, &(fatTable[0]), filePathItems, fileToWriteTo, parentDir, matchCounter);
		if (isError) {
			return isError;
		}

		isError = write_file(diskNumber, fatBootRec, &(fatTable[0]), fileToWriteTo, offset, buffer, bufferLen);

		// update zaznamu souboru v parent adresari
		isError = update_file_in_dir(diskNumber, fatBootRec, &(fatTable[0]), parentDir, fileToWriteTo.name, fileToWriteTo);

		return isError;
	}

	uint16_t CreateDirectory(const std::uint8_t diskNumber, const std::string dirName)
	{
		return _CreateFileInternal(diskNumber, dirName, (uint8_t)kiv_os::NFile_Attributes::Directory);
	}

	uint16_t CreateFile(const std::uint8_t diskNumber, const std::string fileName)
	{
		return _CreateFileInternal(diskNumber, fileName, 0);
	}

	uint16_t DeleteFile(const std::uint8_t diskNumber, const std::string fileName)
	{
		Boot_record fatBootRec;
		uint16_t opRes = 0;
		std::vector<std::string> filePathItems;
		std::vector<int32_t> fatTable;
		Directory fileToDelete;
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
		fatTable.resize(fatBootRec.usable_cluster_count);

		// nacti FAT
		opRes = _LoadFat(diskNumber, fatBootRec, fatTable);
		if (opRes != FsError::SUCCESS) {
			return opRes;
		}

		// najdi soubor
		// rozdel fileName na jmena
		Util::SplitPath(fileName, filePathItems);
		opRes = find_file(diskNumber, fatBootRec, &(fatTable[0]), filePathItems, fileToDelete, parentDir, matchCounter);
		if (opRes != FsError::SUCCESS) {
			return opRes;
		}

		// tohle maze jen sobory, ne adresare
		if (is_dir(fileToDelete)) {
			return FsError::NOT_A_FILE;
		}

		opRes = delete_file(diskNumber, fatBootRec, &(fatTable[0]), parentDir, fileToDelete);

		return opRes;
	}

	uint16_t LoadDiskParameters(const std::uint8_t diskNumber, kiv_hal::TDrive_Parameters & parameters)
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


	uint16_t FileExists(const std::uint8_t diskNumber, const std::string fileName)
	{
		Boot_record fatBootRec;
		uint16_t isError = 0;
		std::vector<std::string> filePathItems;
		Directory parentDir,
			tmp;
		kiv_hal::TDrive_Parameters parameters;
		uint32_t matchCounter;
		std::vector<int32_t> fat;

		// rozdel fileName na jmena
		Util::SplitPath(fileName, filePathItems);
		if (filePathItems.size() == 0) {
			// root existuje
			return FsError::SUCCESS;
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

		isError = _LoadFat(diskNumber, fatBootRec, fat);
		if (isError) {
			return isError;
		}

		Util::SplitPath(fileName, filePathItems);
		return find_file(diskNumber, fatBootRec, &(fat[0]), filePathItems, tmp, parentDir, matchCounter);
	}

	uint8_t ResolveDiskNumber(const char diskIdentifier)
	{
		// todo:
		return 0x81;
	}
}

