#include "fatfs.h"
#include "fat.h"
#include <vector>
#include <string.h>

EStatus FatFS::init(const kiv_hal::TDrive_Parameters & diskParams)
{
	uint16_t isError = init_fat(m_diskNumber, diskParams);

	if (!isError) {
		this->m_diskParams = diskParams;
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::query(const Path & path, FileInfo *pInfo)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	std::vector<int32_t> fatTable;
	Directory fileToRead;
	Directory parentDir;
	uint32_t matchCounter;

	// nacti BR
	isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);

	// nacti FAT
	if (!isError) {
		isError = this->loadFat(fatBootRec, fatTable);
	}

	// najdi soubor
	if (!isError) {
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), fileToRead, parentDir, matchCounter);
	}

	if (!isError) {
		pInfo->size = fileToRead.size;
		pInfo->attributes = fileToRead.flags;
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	std::vector<int32_t> fatTable;
	Directory fileToRead;
	Directory parentDir;
	uint32_t matchCounter;

	// nacti BR
	isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);

	// nacti FAT
	if (!isError) {
		isError = this->loadFat(fatBootRec, fatTable);
	}

	// najdi soubor
	if (!isError) {
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), fileToRead, parentDir, matchCounter);
	}

	// precti soubor
	if (!isError) {
		isError = read_file(m_diskNumber, fatBootRec, &(fatTable[0]), fileToRead, buffer, bufferSize, *pRead, offset);
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	Directory dir;
	Directory parentDir;
	uint32_t matchCounter;
	std::vector<int32_t> fatTable;
	std::vector<Directory> dirItems;

	// nacti BR
	isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);

	// nacti FAT
	if (!isError) {
		isError = this->loadFat( fatBootRec, fatTable);
	}

	// rozdel fileName na jmena
	if (!isError) {
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), dir, parentDir, matchCounter);
	}

	// nacti itemy a vrat vysledek
	if (!isError) {
		isError = load_items_in_dir(m_diskNumber, fatBootRec, &(fatTable[0]), dir, dirItems);
	}

	if (!isError) {
		*pRead = 0;
		for (Directory& dirEntry : dirItems) {
			DirectoryEntry de;
			de.attributes = dirEntry.flags;
			strcpy_s(de.name, MAX_NAME_LEN, dirEntry.name);
			de.name[MAX_NAME_LEN] = '\0';

			entries[*pRead] = de;
			*pRead = (*pRead) + 1;
		}
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	Directory fileToWriteTo;
	Directory parentDir;
	std::vector<int32_t> fatTable;
	uint32_t matchCounter;

	// nacti BR
	isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);

	// nacti FAT
	if (!isError) {
		isError = this->loadFat(fatBootRec, fatTable);
	}

	// najdi soubor
	if (!isError) {
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), fileToWriteTo, parentDir, matchCounter);
	}

	// zapis
	if (!isError) {
		isError = write_file(m_diskNumber, fatBootRec, &(fatTable[0]), fileToWriteTo, offset, buffer, bufferSize, pWritten);
	}

	// update zaznamu souboru v parent adresari
	if (!isError) {
		isError = update_file_in_dir(m_diskNumber, fatBootRec, &(fatTable[0]), parentDir, fileToWriteTo.name, fileToWriteTo);
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::create(const Path & path, const FileInfo & info)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	Directory newFile,
		parentDir,
		tmp;
	std::vector<int32_t> fatTable;
	uint32_t matchCounter;

	// kontrola cesty a delky jmena
	if (path.get().size() == 0) {
		// nemuzeme znova vytvorit root
		isError = FsError::UNKNOWN_ERROR;
	}

	if (!isError) {
		if (path.get()[path.get().size() - 1].size() > MAX_NAME_LEN - 1) {
			isError =  FsError::FILE_NAME_TOO_LONG;
		}
	}

	// nacti BR
	if (!isError) {
		isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);
	}

	// nacti FAT
	if (!isError) {
		isError = this->loadFat(fatBootRec, fatTable);
	}

	// vytvoreni souboru
	if (!isError) {
		// posledni item z path pouzij jako jmeno noveho souboru
		memset(newFile.name, 0, MAX_NAME_LEN);
		memcpy(newFile.name, path.get()[path.get().size() - 1].c_str(), MAX_NAME_LEN - 1);
		newFile.flags = (uint8_t)info.attributes;

		// najdi soubor a jeho rodicovksy adresar
		// pokud metoda vrati FILE_NOT_FOUND a matchCounter bude roven path.size() - 1
		// vime ze rodicovsky adresar byl nalezen a lze v nem vytvori cilovy soubor
		// pokud metoda vrati SUCCESS, vime ze cilovy soubor jiz existuje a je treba vratit chybu
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), tmp, parentDir, matchCounter);
		if (isError == FsError::SUCCESS) {
			isError = FsError::FILE_ALREADY_EXISTS;
		}
		else if (isError == FsError::FILE_NOT_FOUND &&
			(matchCounter == path.get().size() - 1)) {
			// soubor nenalezen a match counter ma spravnou velikost hodnotu -> nalezen parrent dir
			isError = create_file(m_diskNumber, fatBootRec, &(fatTable[0]), parentDir, newFile);
		}
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::resize(const Path & path, uint64_t size)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	Directory fileToResize;
	Directory parentDir;
	std::vector<int32_t> fatTable;
	uint32_t matchCounter;

	// root nemuzeme resizenout
	if (path.get().size() == 0) {
		isError = FsError::UNKNOWN_ERROR;
	}

	// nacti BR
	if (!isError) {
		isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);
	}

	// nacti FAT
	if (!isError) {
		isError = this->loadFat(fatBootRec, fatTable);
	}

	// najdi soubor
	if (!isError) {
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), fileToResize, parentDir, matchCounter);
	}

	// resize
	if (!isError) {
		isError = resize_file(m_diskNumber, fatBootRec, &(fatTable[0]), parentDir, fileToResize, size);
	}

	return FsErrorToStatus(isError);
}

EStatus FatFS::remove(const Path & path)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	Boot_record fatBootRec;
	uint16_t isError = 0;
	Directory fileToDelete;
	Directory parentDir;
	std::vector<int32_t> fatTable;
	uint32_t matchCounter;

	// root nemuzeme resizenout
	if (path.get().size() == 0) {
		isError = FsError::UNKNOWN_ERROR;
	}

	// nacti BR
	if (!isError) {
		isError = load_boot_record(m_diskNumber, m_diskParams, fatBootRec);
	}

	// nacti FAT
	if (!isError) {
		isError = this->loadFat(fatBootRec, fatTable);
	}

	// najdi soubor
	if (!isError) {
		isError = find_file(m_diskNumber, fatBootRec, &(fatTable[0]), path.get(), fileToDelete, parentDir, matchCounter);
	}

	if (!isError) {
		isError = delete_file(m_diskNumber, fatBootRec, &(fatTable[0]), parentDir, fileToDelete);
	}

	return FsErrorToStatus(isError);
}

uint16_t FatFS::loadFat(const Boot_record& fatBootRec, std::vector<int32_t>& fatTable)
{
	fatTable.resize(fatBootRec.usable_cluster_count);
	return load_fat(m_diskNumber, fatBootRec, &(fatTable[0]));
}
