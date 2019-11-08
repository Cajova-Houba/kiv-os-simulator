#pragma once
#include <cstdint>
#include <string>
#include "fat.h"

/**
 * Abstrakce nad implementac� file syst�mu. Vol� Disk_IO syscall.
 */
namespace Filesystem {

	enum Error : uint16_t {
		SUCCESS = 0,
		NO_FILE_SYSTEM,
		DISK_OPERATION_ERROR,

		UNKNOWN_ERROR = 0xFFFF
	};

	/**
	 * @brief Inicializuje filesystem na zadan�m disku.
	 * @param diskNumber ��slo disku, na kter�m m� b�t inicializovan� nov� FS.
	 *
	 * @return true pokud inicializace prob�hla v po��dku.
	 */
	uint16_t InitNewFileSystem(std::uint8_t diskNumber);

	/**
	 * @brief Vrati popis filesystemu na danem disku.
	 * @param diskNumber ��slo disku s FS.
	 * @param boot_record Reference na strukturu do ktere se naplni data o FS.
	 * @return 
	 *	Error::SUCCESS pokud disk obsahuje validn� file system
	 *  Error::NO_FILE_SYSTEM pokud disk neobsahuje validn� file system
	 *  Error::DISK_OPERATION_ERROR pokud dojde k chybe pri cteni disku
	 */
	uint16_t GetFilesystemDescription(std::uint8_t diskNumber, Boot_record* bootRecord);
}
