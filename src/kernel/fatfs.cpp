#include "fatfs.h"
#include "fat.h"
#include "util.h"

EStatus FatFS::init(const kiv_hal::TDrive_Parameters & diskParams)
{
	m_diskParams = diskParams;

	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	// disk uz mozna obsahuje souborovy system FAT
	EStatus status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return FAT::Init(m_diskNumber, m_diskParams);
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::query(const Path & path, FileInfo *pInfo)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory file;
	FAT::Directory parentDirectory;
	uint32_t matchCounter;

	// najdi soubor
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), file, parentDirectory, matchCounter);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	if (pInfo)
	{
		pInfo->size = file.size;
		pInfo->attributes = file.flags;
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory file;
	FAT::Directory parentDirectory;
	uint32_t matchCounter;

	// najdi soubor
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), file, parentDirectory, matchCounter);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	size_t read = 0;

	// precti soubor
	status = FAT::ReadFile(m_diskNumber, bootRecord, fatTable.data(), file, buffer, bufferSize, read, offset);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	if (pRead)
	{
		(*pRead) = read;
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory directory;
	FAT::Directory parentDirectory;
	uint32_t matchCounter;

	// rozdel fileName na jmena
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), directory, parentDirectory, matchCounter);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	std::vector<FAT::Directory> items;

	// nacti itemy a vrat vysledek
	status = FAT::ReadDirectory(m_diskNumber, bootRecord, fatTable.data(), directory, items);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	size_t read = 0;

	for (const FAT::Directory & entry : items)
	{
		if (offset > 0)
		{
			offset--;
		}
		else
		{
			Util::SetDirectoryEntry(entries[read++], entry.flags, entry.name);
		}
	}

	if (pRead)
	{
		(*pRead) = read;
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory file;
	FAT::Directory parentDirectory;
	uint32_t matchCounter;

	// najdi soubor
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), file, parentDirectory, matchCounter);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	size_t written = 0;

	// zapis
	status = FAT::WriteFile(m_diskNumber, bootRecord, fatTable.data(), file, buffer, bufferSize, written, offset);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// update zaznamu souboru v parent adresari
	status = FAT::UpdateFile(m_diskNumber, bootRecord, fatTable.data(), parentDirectory, file.name, file);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	if (pWritten)
	{
		(*pWritten) = written;
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::create(const Path & path, const FileInfo & info)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// kontrola cesty a delky jmena
	if (path.isEmpty())
	{
		// nemuzeme znova vytvorit root
		return EStatus::INVALID_ARGUMENT;
	}

	// posledni item z path pouzij jako jmeno noveho souboru
	const std::string & fileName = path.get().back();

	// novy soubor nebo adresar
	FAT::Directory file;
	file.flags = static_cast<uint8_t>(info.attributes);

	if (fileName.length() >= sizeof file.name)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	for (size_t i = 0; i < sizeof file.name; i++)
	{
		file.name[i] = (i < fileName.length()) ? fileName[i] : '\0';
	}

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory parentDirectory;
	FAT::Directory tmp;
	uint32_t matchCounter;

	// najdi soubor a jeho rodicovksy adresar
	// pokud metoda vrati FILE_NOT_FOUND a matchCounter bude roven path.getComponentCount() - 1
	// vime ze rodicovsky adresar byl nalezen a lze v nem vytvori cilovy soubor
	// pokud metoda vrati SUCCESS, vime ze cilovy soubor jiz existuje a je treba vratit chybu
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), tmp, parentDirectory, matchCounter);
	if (status == EStatus::SUCCESS)
	{
		// soubor nebo adresar uz existuje
		return EStatus::INVALID_ARGUMENT;
	}
	else if (status != EStatus::FILE_NOT_FOUND || matchCounter != path.getComponentCount()-1)
	{
		return status;
	}

	// soubor nenalezen a match counter ma spravnou velikost hodnotu -> nalezen parrent dir
	status = FAT::CreateFile(m_diskNumber, bootRecord, fatTable.data(), parentDirectory, file);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::resize(const Path & path, uint64_t size)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// root nemuzeme resizenout
	if (path.isEmpty())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory file;
	FAT::Directory parentDirectory;
	uint32_t matchCounter;

	// najdi soubor
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), file, parentDirectory, matchCounter);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// resize
	status = FAT::ResizeFile(m_diskNumber, bootRecord, fatTable.data(), parentDirectory, file, size);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	return EStatus::SUCCESS;
}

EStatus FatFS::remove(const Path & path)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// root nemuzeme odstranit
	if (path.isEmpty())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	EStatus status;
	FAT::BootRecord bootRecord;
	std::vector<int32_t> fatTable;

	status = FAT::Load(m_diskNumber, m_diskParams, bootRecord, fatTable);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	FAT::Directory file;
	FAT::Directory parentDirectory;
	uint32_t matchCounter;

	// najdi soubor
	status = FAT::FindFile(m_diskNumber, bootRecord, fatTable.data(), path.get(), file, parentDirectory, matchCounter);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// kontrola, ze adresar je prazdny
	if (file.isDirectory())
	{
		std::vector<FAT::Directory> items;

		status = FAT::ReadDirectory(m_diskNumber, bootRecord, fatTable.data(), file, items);
		if (status != EStatus::SUCCESS)
		{
			return status;
		}

		if (!items.empty())
		{
			return EStatus::DIRECTORY_NOT_EMPTY;
		}
	}

	status = FAT::DeleteFile(m_diskNumber, bootRecord, fatTable.data(), parentDirectory, file);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	return EStatus::SUCCESS;
}
