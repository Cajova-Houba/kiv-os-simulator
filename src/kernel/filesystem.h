// TODO: tento soubor odstranit
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
	 * @param diskNumber Číslo disku, na kterém má být inicializovaný nový FS.
	 * @param parameters Parametry disku.
	 *
	 * @return true pokud inicializace proběhla v pořádku.
	 */
	uint16_t InitNewFileSystem(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters);

	/**
	 * @brief Vrati popis filesystemu na danem disku.
	 * @param diskNumber Číslo disku s FS.
	 * @param parameters Parametry disku, ze ktereho se bude cist.
	 * @param boot_record Reference na strukturu do ktere se naplni data o FS.
	 * @return 
	 *	FsError::SUCCESS pokud disk obsahuje validní file system
	 *  FsError::NO_FILE_SYSTEM pokud disk neobsahuje validní file system
	 *  FsError::DISK_OPERATION_ERROR pokud dojde k chybe pri cteni disku
	 */
	uint16_t GetFilesystemDescription(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, Boot_record & bootRecord);

	/**
	 * @brief Nacte obsah slozky dane fileName.
	 * 
	 * @param diskNumber Cislo disku, ze ktereho se ma cist.
	 * @param fileName Absolutni cesta k adresari.
	 * @param dest Reference na vektor, ktery ma byt naplnen obsahem adresare.
	 *
	 * @return
	 *	FsError::SUCCESS pokud byl obsah v poradku nacten
	 *	FsError::NOT_A_DIR pokud cilovy soubor neni adresar.
	 *  FsError::FILE_NOT_FOUND pokud cilovy soubor nebyl nalezen
	 *  FsError::DISK_OPERATION_ERROR pokud dojde k chybe pri cteni z disku.
	 */
	uint16_t LoadDirContents(const std::uint8_t diskNumber, const std::string fileName, std::vector<Directory>& dest);

	/**
	 * @brief Nacte obsah souboru do bufferu.
	 *
	 * @param diskNumber Cislo disku, ze ktereho se ma cist.
	 * @param fileName Absolutni cesta k souboru.
	 * @param buffer Ukazatel na buffer do ktereho se bude zapisovat obsah souboru.
	 * @param bufferLen Maximalni pouzitelna velikost bufferu
	 *
	 * @return
	 *	FsError::SUCCESS pokud byl obsah v poradku nacten.
	 */
	uint16_t ReadFileContents(const std::uint8_t diskNumber, const std::string fileName, char* buffer, const size_t bufferLen, const size_t offset = 0);

	/**
	 * @brief Zapise obsah bufferu do souboru od daneho offsetu.
	 *
	 * @param diskNumber Cislo disku, na ktery se bude zapisovat.
	 * @param fileName Absolutni cesta k souboru.
	 * @param offset Offset v bytech od ktereho se bude zapisovat do souboru.
	 * @param buffer Buffer ze ktereho se budou zapisovat data na disk.
	 * @param bufferLen Maximalni delka bufferu ze ktereho se zapisuje.
	 */
	uint16_t WriteFileContents(const std::uint8_t diskNumber, const std::string fileName, char* buffer, const size_t bufferLen, const size_t offset = 0);

	/**
	 * @brief Vytvori zadany adresar.
	 *
	 * @param diskNumber Cislo na disku na kterem bude adresar vytvoren.
	 * @param dirName Absolutni cesta k adresari, nekoncici '/'. 
	 *
	 * @return
	 *	FsError::SUCCESS adresar vytvoren.
	 *  FsError::FILE_NOT_FOUND rodicovksy adresar nenalezen.
	 */
	uint16_t CreateDirectory(const std::uint8_t diskNumber, const std::string dirName);

	/**
	 * @brief Vytvori zadany soubor.
	 *
	 * @param fileName Absolutni cesta k souboru.
	 *
	 * @return 
	 *	FsError::SUCCESS adresar vytvoren
	 *	FsError::FILE_NOT_FOUND rodicovsky adresar nenalezen
	 */
	uint16_t CreateFile(const std::uint8_t diskNumber, const std::string fileName);

	/**
	 * @brief Smaze zadany soubor. Pouze odstrani polozku v adresari a smaze zaznamy ve FAT, data na disku zustavaji nezmenena.
	 * 
	 * @param diskNumber Cislo disku ze ktereho se bude mazat.
	 * @param fileName Absolutni cesta k souboru, ktery ma byt smazan.
	 */
	uint16_t DeleteFile(const std::uint8_t diskNumber, const std::string fileName);

	/**
	 * @brief Nacte parametry daneho disku.
	 *
	 * @param diskNumber Cislo disku jehoz parametry nacist.
	 * @param parameters Reference na strukturu, ktera bude naplnena.
	 *
	 * @return
	 *	FsError::SUCCESS pokud byly parametry nacteny.
	 */
	uint16_t LoadDiskParameters(const std::uint8_t diskNumber, kiv_hal::TDrive_Parameters & parameters);

	/**
	 * @brief Zjisti, jestli zadany soubor existuje.
	 * 
	 * @param fileName Absolutni cesta k souboru.
	 */
	uint16_t FileExists(const std::uint8_t diskNumber, const std::string fileName);

	/**
	 * @brief Vrati cislo disku odpovidajici danemu identifikatoru (C -> 0x80, D -> 0x81, ...)
	 *
	 * @param diskIdentifier Jednoznakovy identifikator disku.
	 */
	uint8_t ResolveDiskNumber(const char diskIdentifier);
}

