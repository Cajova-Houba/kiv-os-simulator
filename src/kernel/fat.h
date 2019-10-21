#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


#define DESCRIPTION_LEN         250
#define FAT_TYPE_LEN            1
#define FAT_COPIES_LEN          1
#define CLUSTER_SIZE_LEN        2
#define USABLE_CLUSTER_LEN      4
#define SIGNATURE_LEN           9

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
    char volume_descriptor[DESCRIPTION_LEN];    // FS description,                  250B
    int8_t fat_type;                            // FAT type (FAT12, FAT16...),      1B
    int8_t fat_copies;                          // number of FAT copies,            1B
    int16_t cluster_size;                       // cluster size                     2B
    int32_t usable_cluster_count;               // max number of cluster for data   4B
    char signature[SIGNATURE_LEN];              // orion login                      9B
} Boot_record;//                                                                    267B

/*
 * Definition of directory record.
 */
typedef struct {
    char name[13];                              // File name, 8+3+'\0'
    bool isFile;                                // Is file flag
    int32_t size;                               // Size of the file, 0 for directory
    int32_t start_cluster;                      // Start cluster
} Directory;

/*
 * Prints boot record.
 */
void print_boot_record(Boot_record* boot_record);


/*
 * Loads boot record from the file.
 * Boot record is expected to be at the beginning of the file.
 *
 * Returns:
 * OK: boot record loaded.
 * ERR_READING_FILE: error while reading file.
 */
int load_boot_record(FILE* file, Boot_record* boot_record);

/*
 * Loads a fat table from file. File table is expected to contain
 * boot_record->usable_cluster_count records.
 *
 * Returns:
 * OK: fat table loaded.
 * ERR_READING_FILE: error occurs.
 */
int load_fat_table(FILE *file, Boot_record* boot_record, int32_t* dest);

/*
 * Loads contents of directory from file (in a specified cluster) and stores them to dest.
 * Dest is expected to be an array.
 *
 * Returns:
 * number of items in the dir load
 * ERR_READING_FILE: error occurs.
 */
int load_dir(FILE *file, Boot_record *boot_record, int cluster, Directory *dest);

/*
 * Counts the number of items in directory.
 *
 * Returns:
 * count: 	everything is ok
 * NOK: 	if error occurs
 */
int count_items_in_dir(FILE *file, Boot_record *boot_record, Directory *dir);

/*
 * Prints directory structure to the buffer.
 * Number of tabs placed before the actual output is set by level.
 */
void print_dir(char* buffer, Directory *directory, int level);

/*
 * Returns the max number of items (Directory structs) in directory.
 * The dir can be max 1 cluster => num = cluster_size / sizeof(Directory)
 */
int max_items_in_directory(Boot_record *boot_record);

/*
 * Returns the first unused cluster found or NO_CLUSTER.
 */
int get_free_cluster(int32_t *fat, int fat_size);

/*
 * Returns the position where data clusters start.
 *
 * This position is computed as boot_record_size + fat_size*fat_copies
 */
int get_data_position(Boot_record *boot_record);

/*
 * Returns the position of 'filename' (not the whole path, just the name) in the parent_dir.
 * This position can then be multiplied by sizeof(Directory) to get the offset from the start of
 * the cluster.
 *
 * Returns:
 * NOK: file not found.
 * position: file found.
 */
int get_file_position(FILE *file, Boot_record *boot_record, int parent_dir_cluster, char *filename);

/*
 * Will iterate through items in cluster and returns the position of the first free directory found.
 * The position is relative to the start of the cluster.
 *
 * Returns:
 * position: position is found
 * NOK: no free position is found.
 * ERR_READING_FILE: error occurs
 */
int get_free_directory_in_cluster(FILE *file, Boot_record *boot_record, int32_t *fat, int cluster);

/*
 * Tries to locate the file by it's full filename (specified by path).
 * If the file is found, found_file and parent_directory will be filled (if not NULL).
 * if the file is in the root dir, parent_directory.start_cluster will be ROOT_CLUSTER.
 *
 * Returns:
 * file position in parent dir: file found.
 * NOK:	file not found.
 * ERR_READING_FILE: error while reading the file with fat.
 */
int find_file(FILE *file, Boot_record *boot_record, char **path, int path_length, Directory *found_file, Directory *parent_directory);

/*
 * Tries to locate the directory by it's full name (specified by path).
 * if the directory is found, found_directory and parent_directory will be filled (if not NULL).
 * If the directory is in the root dir, parent_directory.start_cluster will be ROOT_CLUSTER.
 *
 * If the path_length is 1, it's assumed that ROOT directory is to be located. In this case, both found_directory and parent_directory
 * will have it's start_cluster field set to ROOT_CLUSTER and 0 will be returned.
 *
 * Returns:
 * dir position in parent dir: dir found.
 * NOK: dir not found.
 * ERR_READING_FILE: error while reading the file with fat.
 */
int find_directory(FILE *file, Boot_record *boot_record, char **path, int path_length, Directory *found_directory, Directory *parent_directory);

/*
 * Will read the cluster and determines if it's bad - starts and ends with FFFFFF.
 * Cluster is expected to be a char array of cluster size.
 *
 *
 * Returns:
 * OK: Cluster is bad.
 * NOK: Cluster is ok.
 */
int is_cluster_bad(char *cluster, int cluster_size);

/*
 * Saves the fat to the file. Also saves all copies.
 *
 * Returns:
 * OK: Saved.
 * NOK: Error.
 */
int update_fat(FILE *file, Boot_record *boot_record, int32_t *fat);

/*
 * Searches dir for filename.
 *
 * Returns:
 * OK: filename found.
 * NOK: filename not found.
 */
int find_in_dir(FILE *file, Boot_record *boot_record, char *filename, int directory_cluster, bool is_file);

/*
 * Returns the number of clusters marked as UNUSED.
 */
int unused_cluster_count(int32_t *fat, int fat_length);
