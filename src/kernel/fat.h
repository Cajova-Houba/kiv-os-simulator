#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "../api/hal.h"

#include "file.h"  // FileAttributes
#include "types.h"
#include "status.h"

#define DESCRIPTION_LEN         250
#define FAT_TYPE_LEN            1
#define FAT_COPIES_LEN          1
#define CLUSTER_SIZE_LEN        2
#define USABLE_CLUSTER_LEN      4
#define SIGNATURE_LEN           9
#define ALIGNED_BOOT_REC_SIZE   272

// Cluster where the root directory is.
#define ROOT_CLUSTER            0

// No cluster.
#define NO_CLUSTER              -1

// Default FAT metadata.
#define DEF_FAT_COPIES          1
#define DEF_MIN_DATA_CL         252
#define MIN_REQ_SECOTR_SIZE     256
#define PREFERRED_CLUSTER_SIZE  256

#define MAX_NAME_LEN            12

enum
{
	FAT_UNUSED       = INT32_MAX - 1,
	FAT_FILE_END     = INT32_MAX - 2,
	FAT_BAD_CLUSTERS = INT32_MAX - 3
};

namespace FAT
{
	// Definition of boot record.
	struct BootRecord
	{
		char volume_descriptor[DESCRIPTION_LEN];  // FS description                            250 B
		uint8_t fat_type;                         // FAT type (FAT12, FAT16, ...)              1 B
		uint8_t fat_copies;                       // number of FAT copies                      1 B
		uint16_t cluster_size;                    // cluster size (# of sectors per cluster)   2 B
		uint32_t usable_cluster_count;            // max number of cluster for data            4 B
		uint16_t bytes_per_sector;                // number of bytes in 1 sector               2 B
		char signature[SIGNATURE_LEN];            // orion login                               9 B
		// 269 B

		BootRecord()
		: volume_descriptor(),
		  fat_type(),
		  fat_copies(),
		  cluster_size(),
		  usable_cluster_count(),
		  bytes_per_sector(),
		  signature()
		{
		}

		/**
		 * @brief Zkontroluje jestli je zadany boot record validni.
		 */
		bool isValid() const
		{
			return usable_cluster_count > 0;
		}
	};

	// Definition of directory record.
	struct Directory
	{
		char name[MAX_NAME_LEN];  // File name, 8+3+'\0'
		uint8_t flags;            // File attributes
		uint32_t size;            // Size of the file, 0 for directory
		int32_t start_cluster;    // Start cluster

		Directory()
		: name(),
		  flags(FileAttributes::DIRECTORY),
		  size(),
		  start_cluster(ROOT_CLUSTER)
		{
		}

		bool isDirectory() const
		{
			return flags & FileAttributes::DIRECTORY;
		}

		bool hasName() const
		{
			return name[0] != '\0';
		}

		void clear()
		{
			std::memset(name, 0, sizeof name);
			flags = 0;
			size = 0;
			start_cluster = 0;
		}
	};

	/**
	 * @brief Na danem disku inicializuje boot record, fat tabulku a vytvori root cluster.
	 *
	 * Podrobnosti:
	 * - cluster_size je 1 (sectors per cluster)
	 * - fat_copies is set to 1
	 * - fat_type is set to 0
	 *
	 * @param diskNumber Cislo disku na ktery se bude zapisovat.
	 * @param parameters Parametry disku na kterem se ma FAT inicializovat.
	 */
	EStatus Init(uint8_t diskNumber, const kiv_hal::TDrive_Parameters & diskParams);

	/**
	 * @brief Nacte boot record a FAT tabulku z disku.
	 * @return
	 *  EStatus::SUCCESS vse v poradku.
	 *  EStatus::IO_ERROR chyba pri cteni z disku.
	 */
	EStatus Load(uint8_t diskNumber, const kiv_hal::TDrive_Parameters & diskParams,
	             BootRecord & bootRecord, std::vector<int32_t> & fatTable);

	/**
	 * @brief Nacte polozky v adresari do result.
	 *
	 * @return
	 *  EStatus::SUCCESS polozky nacteny.
	 *  EStatus::INVALID_ARGUMENT pokud dir neni slozka.
	 */
	EStatus ReadDirectory(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable,
	                      const Directory & directory, std::vector<Directory> & result);

	/**
	 * @brief Precte obsah souboru do bufferu.
	 *
	 * @param bufferLen Velikost buffero do ktereho se cte. V podstate udava, kolik max bytu precist ze souboru.
	 * @param offset Offset od ktereho cist. Pokud je vetsi nebo roven soucasne velikosti souboru, nic nebude precteno.
	 *
	 * @return
	 *  EStatus::SUCCESS obsah souboru precten.
	 *  EStatus::INVALID_ARGUMENT fileToRead neni soubor (ale adresar).
	 */
	EStatus ReadFile(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable, const Directory & file,
	                 char *buffer, size_t bufferSize, size_t & bytesRead, uint64_t offset = 0);

	/**
	 * @brief Zapise do existujiciho souboru data z bufferu.
	 *
	 * @param offset Offset v bytech od ktereho se ma zapisovat. Pokud presahuje soucasnou velikost souboru, soubor bude
	 * rozsiren o nuly.
	 *
	 * @return
	 *  EStatus::SUCCESS uspesne zapsano do souboru.
	 */
	EStatus WriteFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable, Directory & file,
	                  const char *buffer, size_t bufferSize, size_t & bytesWritten, uint64_t offset = 0);

	/**
	 * @brief Smaze zadany soubor.
	 * Data realne zustanou na disku, pouze se upravi FAT a directory zaznam v rodicovskem adresari. Parametr file bude po
	 * zavolani teto funkce obsahovat pouze nuly.
	 */
	EStatus DeleteFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable,
	                   const Directory & parentDirectory, Directory & file);

	/**
	 * @brief Vytvori novy soubor v danem rodicovskem adresari
	 *
	 * @return
	 *  EStatus::SUCCESS soubor vytvoren, newFile ma nastaveny start_cluster a size.
	 */
	EStatus CreateFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable,
	                   const Directory & parentDirectory, Directory & newFile);

	/**
	 * @brief Prehraje zadanou polozku (file) v zadanem adresari (parentDirectory).
	 *
	 * @param originalFileName Puvodni jmeno souboru, ktery ma byt nahrazen.
	 *
	 * @return
	 *  EStatus::SUCCESS pokud vse v poradku
	 *  EStatus::FILE_NOT_FOUND pokud nebyl file nalezen v rodicovskem adresari.
	 */
	EStatus UpdateFile(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable,
	                   const Directory & parentDirectory, const char *originalFileName, const Directory & file);

	/**
	 * @brief Zmeni velikost souboru.
	 * Z disku se realne nic nemaze, pouze se upravuje FAT a polozka Directory.size.
	 * Pri zvetseni souboru se prazdne misto vyplni 0.
	 */
	EStatus ResizeFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable,
	                   const Directory & parentDirectory, Directory & file, uint64_t newSize);

	/**
	 * @brief Pokusi se najit soubor podle zadane filePath.
	 * Pokud je soubor nalezen, je ulozen do foundFile a jeho rodicovsky adresar do parentDirectory. V pripade, ze soubor je
	 * v rootu, parentDirectory.start_cluster je nastaven na ROOT_CLUSTER. Pokud je filePath prazdna, foundFile.start_cluster
	 * je nastaven na ROOT_CLUSTER a foundFile.isFile je nastaven na false.
	 *
	 * @param filePath Absolutni cesta obsahujici jmena jednotlivych adresaru. Posledni prvek je jmeno hledaneho souboru.
	 * @param foundFile Reference na strukturu ktera bude naplnena nalezenym souborem.
	 * @param parentDirectory Reference na strukturu ktera bude naplnena poslednim nalezenym adresarem z cesty. V pripade
	 * nalezeni souboru bude obsahovat jeho rodicovksy adresar. V pripade, ze filePath.size() > 0, bude obsahovat minimalne
	 * root adresar.
	 * @param matchCounter Pocitadlo nalezenych itemu. V pripade, ze byl nalezen cilovy soubor, bude rovne poctu itemu v
	 * ceste. Pokud byl nalezen jen rodicovsky adresar ciloveho souboru, bude rovne poctu itemu v ceste - 1 ...
	 *
	 * @return
	 *  EStatus::SUCCESS pokud byl soubor nalezen.
	 *  EStatus::FILE_NOT_FOUND pokud soubor nebyl nalezen.
	 *  EStatus::INVALID_ARGUMENT pokud nejaky z prvku cesty (krome posledniho) neni adresar.
	 */
	EStatus FindFile(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable,
	                 const std::vector<std::string> & filePath, Directory & foundFile,
	                 Directory & parentDirectory, uint32_t & matchCounter);
}
