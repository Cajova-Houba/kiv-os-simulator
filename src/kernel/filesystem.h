#pragma once
#include <cstdint>
#include <string>
#include "fat.h"

/**
 * Abstrakce nad implementací file systému. Volá Disk_IO syscall.
 */
namespace Filesystem {

	enum Error : uint16_t {
		SUCCESS = 0,
		NO_FILE_SYSTEM,
		DISK_OPERATION_ERROR,

		UNKNOWN_ERROR = 0xFFFF
	};

	/**
	 * @brief Inicializuje filesystem na zadaném disku.
	 * @param diskNumber Èíslo disku, na kterém má být inicializovaný nový FS.
	 *
	 * @return true pokud inicializace probìhla v poøádku.
	 */
	uint16_t InitNewFileSystem(std::uint8_t diskNumber);

	/**
	 * @brief Vrati popis filesystemu na danem disku.
	 * @param diskNumber Èíslo disku s FS.
	 * @param boot_record Reference na strukturu do ktere se naplni data o FS.
	 * @return 
	 *	Error::SUCCESS pokud disk obsahuje validní file system
	 *  Error::NO_FILE_SYSTEM pokud disk neobsahuje validní file system
	 *  Error::DISK_OPERATION_ERROR pokud dojde k chybe pri cteni disku
	 */
	uint16_t GetFilesystemDescription(std::uint8_t diskNumber, Boot_record* bootRecord);
}
