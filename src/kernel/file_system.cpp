#include "../api/hal.h"

#include "file_system.h"
#include "procfs.h"
#include "fatfs.h"
#include "kernel.h"

static bool HALGetDriveParameters(uint8_t driveNumber, kiv_hal::TDrive_Parameters & driveParams)
{
	kiv_hal::TRegisters registers;
	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Drive_Parameters);
	registers.rdi.r = reinterpret_cast<uint64_t>(&driveParams);
	registers.rdx.l = driveNumber;

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);

	if (registers.flags.carry)
	{
		return false;
	}

	return true;
}

static std::unique_ptr<IFileSystem> InitFAT(uint8_t diskNumber, const kiv_hal::TDrive_Parameters & diskParams)
{
	std::unique_ptr<FatFS> fat = std::make_unique<FatFS>(diskNumber);

	EStatus status = fat->init(diskParams);
	if (status != EStatus::SUCCESS)
	{
		Kernel::Log("Nelze inicializovat FAT na disku 0x%X: Kod chyby %d", diskNumber, static_cast<int>(status));
		fat.reset();
	}

	return fat;
}

void FileSystem::init()
{
	// "0:\"
	m_filesystems['0'] = std::make_unique<ProcFS>();

	char diskLetter = 'A';
	kiv_hal::TDrive_Parameters diskParams;

	for (int i = 0; i < 256; i++)
	{
		uint8_t diskNumber = static_cast<uint8_t>(i);

		if (diskNumber == 128)  // 0x80 = první disk
		{
			diskLetter = 'C';
		}

		if (HALGetDriveParameters(diskNumber, diskParams))
		{
			std::unique_ptr<IFileSystem> fs = InitFAT(diskNumber, diskParams);
			if (fs)
			{
				m_filesystems[diskLetter] = std::move(fs);

				if (diskLetter == 'B')
				{
					// už máme 2 diskety, takže skočíme rovnou na první disk
					i = 127;  // for-smyčka inkrementuje na 128
					// písmeno disku se nastaví na C
				}
				else if (diskLetter == 'Z')
				{
					// už nemáme volná písmenka :)
					break;
				}
				else
				{
					diskLetter++;
				}
			}
		}
	}
}

EStatus FileSystem::query(const Path & path, FileInfo *pInfo)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->query(path, pInfo) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->read(path, buffer, bufferSize, offset, pRead) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->readDir(path, entries, entryCount, offset, pRead) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->write(path, buffer, bufferSize, offset, pWritten) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::create(const Path & path, const FileInfo & info)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->create(path, info) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::resize(const Path & path, uint64_t size)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->resize(path, size) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::remove(const Path & path)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->remove(path) : EStatus::FILE_NOT_FOUND;
}
