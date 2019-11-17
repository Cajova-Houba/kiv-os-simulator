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


constexpr int32_t find_last_file_cluster(const int32_t * fat, const int32_t startCluster)
{
	int32_t lastCluster = startCluster;

	while (fat[lastCluster] != FAT_FILE_END && fat[lastCluster] != FAT_DIRECTORY) {
		lastCluster = fat[lastCluster];
	}
	return lastCluster;
}

constexpr size_t bytes_per_cluster(const Boot_record& bootRecord) {
	return bootRecord.bytes_per_sector * bootRecord.cluster_size;
}

constexpr size_t count_file_clusters(const int32_t* fat, const int32_t startCluster) {
	size_t cnt = 1;
	int32_t cl = startCluster;
	while (fat[cl] != FAT_FILE_END && fat[cl] != FAT_DIRECTORY) {
		cnt++;
		cl = fat[cl];
	}

	return cnt;
}

/**
 * @brief Vrati pocet volnych clusteru ve FAT.
 */
constexpr size_t count_free_clusters(const int32_t *fat, const size_t fatLen) {
	size_t count = 0,
		i = 0;
	for (i = 0; i < fatLen; i++)
	{
		if (fat[i] == FAT_UNUSED) {
			count++;
		}
	}

	return count;
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
 * @param includeEmpty Vrati i nepouzite polozky.
 *
 * @return
 *	FsError::SUCCESS polozky nacteny.
 *	FsError::NOT_A_DIR pokud dir neni slozka.
 */
uint16_t load_items_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & dir, std::vector<Directory>& dest, const bool includeEmpty = false);

/**
 * @brief Precte obsah soboru do bufferu.
 * 
 * @param bufferLen Velikost buffero do ktereho se cte. V podstate udava, kolik max bytu precist ze souboru.
 * @param offset Offset od ktereho cist. Pokud je vetsi nebo roven soucasne velikosti souboru, nic nebude precteno.
 *
 * @return
 *	FsError::SUCCESS obsah souboru precten.
 *  FsError::NOT_A_FILE fileToRead neni soubor (ale adresar).
 */
uint16_t read_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t* fatTable, const Directory & fileToRead, char * buffer, const size_t bufferLen, const size_t offset = 0);

/**
 * @brief Zapise do existujiciho souboru data z bufferu.
 *
 * @param offset Offset v bytech od ktereho se ma zapisovat. Pokud presahuje soucasnou velikost souboru, souboru bude rozsiren o nuly.
 *
 * @return
 *	FsError::SUCCESS uspesne zapsano do souboru.
 */
uint16_t write_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t* fatTable, Directory & fileToWriteTo, const size_t offset, char* buffer, const size_t bufferLen);

/**
 * @brief Smaze zadany soubor. Data realne zustanou na disku, pouze se upravi FAT a directory zaznam v rodicovskem adresari. fileToDelete bude po zavolani
 *	teto funkce obsahovat pouze 0 (pouzije se funkce memset).
 */
uint16_t delete_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t* fatTable, const Directory & parentDirectory, Directory & fileToDelete);

/**
 * @biref Alokuje zadany pocet clusteru ve FAT od zadaneho poledniho clusteru.
 * Tato funkce na nic nezapisuje, pouze modifikuje zadanou FAT.
 */
uint16_t allocate_clusters(const Boot_record & bootRecord, int32_t* fatTable, const int32_t lastCluster, const size_t clusterCount);

/**
 * @brief Prida na konec souboru dany pocet nul.
 * 
 * Kontrola, jestli je dost mista pocita s poctem clusteru k zapisu jako ceil(zeroCount / bytes_per_sector) coz muze teoreticky zpusobit, ze funkce vrati
 * DISK_FULL protoze nepocita s pripadnym mistem 
 *
 */
uint16_t append_zero_to_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t* fatTable, const int32_t lastSector, const size_t zeroCount);

/**
 * @brief Vytvori novy soubor v danem rodicovskem adresari
 * 
 * @return
 *	FsError::SUCCESS soubor vytvoren, newFile ma nastaveny start_cluster a size.
 */
uint16_t create_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t* fatTable, const Directory & parentDirectory, Directory & newFile);

/**
 * @brief Prehraje zadanou polozku (fileToUpdate) v zadanem adresari (parentDirectory).
 *
 * @param originalFileName Puvodni jmeno souboru, ktery ma byt nahrazen.
 *
 * @return
 *	FsError::SUCCESS pokud vse v poradku
 *	FsError::FILE_NOT_FOUND pokud nebyl fileToUpdate nalezen v rodicovskem adresari.
 */
uint16_t update_file_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & parentDirectory, const std::string originalFileName, const Directory & fileToUpdate);

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
 * @brief Pokusi se najit soubor podle zadane filePath. Pokud je sobor nalezen, je ulozen do foundFile a jeho rodicovsky adresar do parentDirectory.
 *        V pripade, ze soubor je v rootu, parentDirectory.start_cluster je nastaven na ROOT_CLUSTER.
 *		  Pokud je filePath prazdna, foundFile.start_cluster je nastaven na ROOT_CLUSTER a foundFile.isFile je nastaven na false.
 * 
 * @param filePath Absolutni cesta obsahujici jmena jednotlivych adresaru. Posledni prvek je jmeno hledaneho souboru.
 * @param foundFile Reference na strukturu ktera bude naplnena nalezenym souborem.
 * @param parentDirectory Reference na strukturu ktera bude naplnena poslednim nalezenym adresarem z cesty. Tj v pripade nalezeni souboru bude obsahovat jeho rodicovksy adresar.
 *		V pripade, ze filePath.size() > 0, bude obsahovat minimalne root adresar.
 * @param matchCounter Pocitadlo nalezenych itemu. V pripade, ze byl nalezen cilovy soubor, bude rovne poctu itemu v ceste. Pokud byl nalezen jen rodicovsky adresar 
 *		ciloveho souboru, bude rovne poctu itemu v ceste - 1 ...
 *
 * @return
 *	FsError::SUCCESS pokud byl soubor nalezen.
 *  FsError::FILE_NOT_FOUND pokud soubor nebyl nalezen.
 *	FsError::NOT_A_DIR pokud nejaky z prvku cesty (krome posledniho) neni adresar.
 */
uint16_t find_file(const std::uint8_t diskNumber, const Boot_record & bootRecord,
	const std::vector<std::string> & filePath,
	Directory & foundFile, Directory & parentDirectory, uint32_t & matchCounter);

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
 * @brief Rozdeli dany soubor na souvisle useky clusteru (startCluster + pocet souvislych clusteru). Funguje i pro adresare.
 *	Max velikost chunku je 1000.
 *
 *
 */
void split_file_to_chunks(const int32_t* fatTable, const int32_t startCluster, std::vector<std::array<int32_t,2>> & chunks);

/**
 * @brief Vrati cislo clusteru ve kterem se nachazi dany offset
 */
int32_t get_cluster_by_offset(const int32_t* fatTable, const int32_t startCluster, const size_t offset, const size_t clusterSize);

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
uint16_t read_cluster_range(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t cluster, const uint32_t clusterCount, char* buffer, const size_t bufferLen, const size_t readOffset = 0);

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