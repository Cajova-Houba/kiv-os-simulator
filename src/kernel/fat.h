#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <vector>
#include "../api/hal.h"
#include "fserrors.h"


#define DESCRIPTION_LEN         250
#define FAT_TYPE_LEN            1
#define FAT_COPIES_LEN          1
#define CLUSTER_SIZE_LEN        2
#define USABLE_CLUSTER_LEN      4
#define SIGNATURE_LEN           9
#define ALIGNED_BOOT_REC_SIZE   272

/*
 * BAD_BYTE * BAD_SEQ_LEN at the start and end of the cluster => bad cluster.
 */
#define BAD_BYTE                (char)0xFF
#define BAD_SEQ_LEN             3

// +<dir name> <start cluster of dir>
#define DIR_PRINT_FORMAT        "+%s %d\n"

// -<file name> <start cluster of file> <size>
#define FILE_PRINT_FORMAT       "-%s %d %d\n"
#define PATH_DELIMITER			"/\0"

/*
 * Cluster where the root dir is.
 */
#define ROOT_CLUSTER            0

/*
 * No cluster.
 */
#define NO_CLUSTER				-1

/**
 * Default FAT metadata
 */
#define DEF_FAT_COPIES			 1
#define DEF_MIN_DATA_CL          252
#define MIN_REQ_SECOTR_SIZE		 256	

#define MAX_NAME_LEN             12

/*
 * Erros returned by functions declared in this header.
 */
typedef enum {

	OK = -100,
	NOK,
	NO_THREAD,
	FILE_CHANGED,
	ERR_READING_FILE,
	ERR_FAT_NOT_FOUND,
	ERR_FILE_TOO_BIG,
	ERR_PATH_NOT_FOUND,
	ERR_PATH_NOT_EMPTY,
	ERR_NO_FREE_ROOM,
	ERR_ALREADY_EXISTS

} FatErrors;

typedef enum {
    FAT_UNUSED = INT32_MAX - 1,
    FAT_FILE_END = INT32_MAX - 2,
    FAT_BAD_CLUSTERS = INT32_MAX -3,
    FAT_DIRECTORY = INT32_MAX -4
} Fat_special;

/*
 * Definition of boot record.
 */
typedef struct{
    char volume_descriptor[DESCRIPTION_LEN];    // FS description,							250B
    int8_t fat_type;                            // FAT type (FAT12, FAT16...),				1B
    int8_t fat_copies;                          // number of FAT copies,					1B
    int16_t cluster_size;                       // cluster size (# of sectors per cluster)	2B
    int32_t usable_cluster_count;               // max number of cluster for data			4B
	int16_t bytes_per_sector;					// number of bytes in 1 sector				2B
    char signature[SIGNATURE_LEN];              // orion login								9B
} Boot_record;//																			269B

/*
 * Definition of directory record.
 */
typedef struct {
    char name[MAX_NAME_LEN];                    // File name, 8+3+'\0'
    bool isFile;                                // Is file flag
    uint32_t size;                              // Size of the file, 0 for directory
    int32_t start_cluster;                      // Start cluster
} Directory;


constexpr size_t bytes_per_cluster(const Boot_record& bootRecord) {
	return bootRecord.bytes_per_sector * bootRecord.cluster_size;
}

/**
 * @brief Vrati offset na kterem zacina dany cluster od zacatku pameti (od bytu 0) v bytech.
 */
constexpr uint64_t cluster_byte_offset(const Boot_record& bootRecord, int clusterNum) {
	return (uint64_t)ALIGNED_BOOT_REC_SIZE 
		+ sizeof(int32_t)*bootRecord.usable_cluster_count*(bootRecord.fat_copies) 
		+ clusterNum*(bytes_per_cluster(bootRecord));
}

constexpr uint64_t sectors_to_read(const uint64_t startSector, const int byteOffset, const int bytesToRead, const uint64_t bytes_per_sector) {
	// endSector = byteOffset + bytesToRead -> to je posledni sektor ktery jeste musime precist abychom meli vsechna data
	// endSector - startSector + 1 -> celkovy pocet sektoru mezi startSector a endSector ktery je potreba precist abychom vse nacetli
	return ((byteOffset + bytesToRead) / bytes_per_sector) - startSector + 1;
}

/**
 * @brief Vrati maximalni mozny pocet itemu v adresari pro danou FAT.
 */
constexpr size_t max_items_in_directory(const Boot_record& bootRecord) {
	return bytes_per_cluster(bootRecord) / sizeof(Directory);
}

/**
 * @brief Zkontroluje jestli je zadany bootRecord validni.
 */
bool is_valid_fat(Boot_record & bootRecord);

/**
 * @brief Na danem disku inicializuje boot record, fat tabulku a vytvori root cluster.
 *
 * Podobnosti:
 * - cluster_size je 1 (sectors per cluster)
 * - fat_copies is set to 1
 * - fat_type is set to 0
 *
 * @param diskNumber Cislo disku na ktery se bude zapisovat.
 * @param parameters Parametry disku na kterem se ma FAT inicializovat.
 */
uint16_t init_fat(const std::uint8_t diskNumber, kiv_hal::TDrive_Parameters parameters);

/**
 * @brief Nacte boot record z disku.
 * 
 * @param diskNumber Cislo disku ze ktereho se bude cist.
 * @param parameters Parametry daneho disku, ze ktereho se bude cist.
 * @param bootRecord Pointer na strukturu do ktere bude nacten boot record.
 */
uint16_t load_boot_record(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, Boot_record& bootRecord);

/**
 * @brief Loads the FAT table from disk and stores it to dest.
 * @return
 *	FsError::SUCCESS tabulka nactena.
 *  FsError::DISK_OPERATION_ERROR chyba pri cteni z disku.
 */
uint16_t load_fat(const std::uint8_t diskNumber, const Boot_record& bootRecord, int32_t* dest);

/**
 * @brief Nacte polozky v adresari do dest.
 *
 * @return
 *	FsError::SUCCESS polozky nacteny.
 *	FsError::NOT_A_DIR pokud dir neni slozka.
 */
uint16_t load_items_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & dir, std::vector<Directory>& dest);

/**
 * @brief Precte obsah soboru do bufferu.
 * 
 * @return
 *	FsError::SUCCESS obsah souboru precten.
 *  FsError::NOT_A_FILE fileToRead neni soubor (ale adresar).
 */
uint16_t read_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t* fatTable, const Directory & fileToRead, char * buffer, size_t bufferLen);

/**
 * @brief Zapise do existujiciho souboru data z bufferu.
 *
 * @param offset Offset v bytech od ktereho se ma zapisovat. Pokud presahuje soucasnou velikost souboru, souboru bude rozsiren o nuly.
 *
 * @return
 *	FsError::SUCCESS uspesne zapsano do souboru.
 */
uint16_t write_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t* fatTable, const Directory & fileToWriteTo, size_t offset, char* buffer, size_t bufferLen);

/**
 * @brief Vytvori novy soubor v danem rodicovskem adresari
 * 
 * @return
 *	FsError::SUCCESS soubor vytvoren, newFile ma nastaveny start_cluster a size.
 */
uint16_t create_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t* fatTable, const Directory parentDirectory, Directory & newFile);

/**
 * Returns the first unused cluster found or NO_CLUSTER.
 */
int get_free_cluster(const int32_t *fat, const int fat_size);

/**
 * Returns the position where data clusters start.
 *
 * This position is computed as boot_record_size + fat_size*fat_copies
 */
int get_data_position(Boot_record *boot_record);

/**
 * Returns the position of 'filename' (not the whole path, just the name) in the parent_dir.
 * This position can then be multiplied by sizeof(Directory) to get the offset from the start of
 * the cluster.
 *
 * @return
 * NOK: file not found.
 * position: file found.
 */
int get_file_position(FILE *file, Boot_record *boot_record, int parent_dir_cluster, char *filename);

/**
 * Will iterate through items in cluster and returns the position of the first free directory found.
 * The position is relative to the start of the cluster.
 *
 * @return
 * position: position is found
 * NOK: no free position is found.
 * ERR_READING_FILE: error occurs
 */
int get_free_directory_in_cluster_old(FILE *file, Boot_record *boot_record, int32_t *fat, int cluster);

/**
 * @brief Pokusi se najit soubor podle zadane filePath. Pokud je sobor nalezen, je ulozen do foundFile a jeho rodicovsky adresar do parentDirectory.
 *        V pripade, ze soubor je v rootu, parentDirectory.start_cluster je nastaven na ROOT_CLUSTER.
 *		  Pokud je filePath prazdna, foundFile.start_cluster je nastaven na ROOT_CLUSTER a foundFile.isFile je nastaven na false.
 * 
 * @param filePath Absolutni cesta obsahujici jmena jednotlivych adresaru. Posledni prvek je jmeno hledaneho souboru.
 *
 * @return
 *	FsError::SUCCESS pokud byl soubor nalezen.
 *  FsError::FILE_NOT_FOUND pokud soubor nebyl nalezen.
 *	FsError::NOT_A_DIR pokud nejaky z prvku cesty (krome posledniho) neni adresar.
 */
uint16_t find_file(const std::uint8_t diskNumber, const Boot_record & bootRecord,
	const std::vector<std::string> & filePath,
	Directory & foundFile, Directory & parentDirectory);

/**
 * Will read the cluster and determines if it's bad - starts and ends with FFFFFF.
 * Cluster is expected to be a char array of cluster size.
 *
 * @return OK: Cluster is bad. NOK: Cluster is ok.
 */
int is_cluster_bad(char *cluster, int cluster_size);

/**
 * @brief Ulozi bootRecord, FAT a pripadne kopie.
 *
 * @return OK: Saved. NOK: Error.
 */
uint16_t update_fat(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t *fat);

/**
 * Searches dir for filename.
 *
 * @return OK: filename found. NOK: filename not found.
 */
int find_in_dir(FILE *file, Boot_record *boot_record, char *filename, int directory_cluster, bool is_file);

/**
 * Returns the number of clusters marked as UNUSED.
 *
 * @param fat Pointer to the array with FAT.
 * @param fat_length Number of records in FAT to go through.
 *
 * @return Number of cluster marked as UNUSED.
 */
int unused_cluster_count(int32_t *fat, int fat_length);

/**************************
	POMOCNE FUNKCE
****************************/

/**
 * @brief Vrati prvni neobsazenou Directory polozku v clusteru adresare.
 *
 * @return Cislo polozky (od 0) nebo -1 pokud neni misto.
 */
int get_free_dir_item_in_cluster(char* clusterBuffer, size_t dirItemCount);

/**
 * @brief Vrati index do items pokud v items existuje polozka se jmenem itemName.
 *
 * @return 
 *  >=0 pokud je polozka nalezena
 *  < 0 pokud neni nalezena.
 */
int is_item_in_dir(const std::string itemName, const std::vector<Directory> & items);

/**
 * @brief Zkopiruje informace ze source do dest.
 */
void copy_dir(Directory & dest, const Directory & source);

/**
 * @brief Precte zadany, souvisly rozsah clusteru do bufferu.
 * 
 * @param bufferLen Maximalni pocet bytu ktery nacist do bufferu. Relevantni jen pokud je tento pocet mensi nez pocet clusteru * velikost clusteru.
 */
uint16_t read_cluster_range(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t cluster, const uint32_t clusterCount, char* buffer, const size_t bufferLen);

/**
 * @brief Zapise dany pocet clusteru z bufferu na disk. Predpoklada se, ze velikost buffer je >= poctu zapisovanych bytu.
 */
uint16_t write_cluster_range(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t cluster, const uint32_t clusterCount, char* buffer);

/**
 * @brief Zavola syscall na cteni z disku a data ulozi do buffer.
 * @return 
 *	FsError::SUCCESS Pokud nacteni probehne v poradku.
 */
uint16_t read_from_disk(const std::uint8_t diskNumber, const uint64_t startSector, const uint64_t sectorCount, char* buffer);

/**
 * @brief Zavola syscall pro zapis dat z bufferu na disk.
 * @return
 *  FsError::SUCCESS Pokud zapis probehne v poradku.
 */
uint16_t write_to_disk(const std::uint8_t diskNumber, const uint64_t startSector, const uint64_t sectorCount, char* buffer);

/**
 * @brief Vrati cislo prvniho datoveho sektoru na disku (ve FAT cluster 0).
 */
uint64_t first_data_sector(const Boot_record& bootRecord);