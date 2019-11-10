#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "fat.h"
#include "fserrors.h"

/**
 * Abstrakce nad implementací file systému. Volá Disk_IO syscall.
 */
namespace Filesystem {

	/**
	 * @brief Inicializuje filesystem na zadaném disku.
	 * @param diskNumber Èíslo disku, na kterém má být inicializovaný nový FS.
	 * @param parameters Parametry disku.
	 *
	 * @return true pokud inicializace probìhla v poøádku.
	 */
	uint16_t InitNewFileSystem(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters);

	/**
	 * @brief Vrati popis filesystemu na danem disku.
	 * @param diskNumber Èíslo disku s FS.
	 * @param parameters Parametry disku, ze ktereho se bude cist.
	 * @param boot_record Reference na strukturu do ktere se naplni data o FS.
	 * @return 
	 *	FsError::SUCCESS pokud disk obsahuje validní file system
	 *  FsError::NO_FILE_SYSTEM pokud disk neobsahuje validní file system
	 *  FsError::DISK_OPERATION_ERROR pokud dojde k chybe pri cteni disku
	 */
	uint16_t GetFilesystemDescription(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, Boot_record* bootRecord);

	/**
	 * @brief Nacte obsah slozky dane fileName.
	 * 
	 * @param diskNumber Cislo disku, ze ktereho se ma cist.
	 * @param parameters Parametry disku ze ktereho se bude cist.
	 * @param fileName Absolutni cesta k adresari.
	 * @param dest Reference na vektor, ktery ma byt naplnen obsahem adresare.
	 *
	 * @return
	 *	FsError::SUCCESS pokud byl obsah v poradku nacten
	 *	FsError::NOT_A_DIR pokud cilovy soubor neni adresar.
	 *  FsError::FILE_NOT_FOUND pokud cilovy soubor nebyl nalezen
	 *  FsError::DISK_OPERATION_ERROR pokud dojde k chybe pri cteni z disku.
	 */
	uint16_t LoadDirContents(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, const std::string fileName, std::vector<Directory>& dest);

	/**
	 * @brief Nacte obsah souboru do bufferu.
	 *
	 * @param diskNumber Cislo disku, ze ktereho se ma cist.
	 * @param parameters Parametry disku ze ktereho se bude cist.
	 * @param fileName Absolutni cesta k souboru.
	 * @param buffer Ukazatel na buffer do ktereho se bude zapisovat obsah souboru.
	 *
	 * @return
	 *	FsError::SUCCESS pokud byl obsah v poradku nacten.
	 */
	uint16_t ReadFileContents(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, const std::string fileName, char* buffer);

	// todo: jenom testovaci, bude smazano
	void countRoot(std::uint8_t diskNumber, kiv_hal::TDrive_Parameters params, Boot_record& bootRec, int& cnt);
}
