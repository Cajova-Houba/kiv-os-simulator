
#include <string>
#include <cstdlib>
#include "fat.h"


/*
 * Loads boot record from the file.
 * Boot record is expected to be at the beginning of the file.
 * The file is assumed to be already opened.
 *
 *
 * Returns:
 * OK: boot record loaded.
 * ERR_READING_FILE: error while reading file.
 */
int load_boot_record(FILE* file, Boot_record* boot_record) {
    int bytes_read = 0;

    if(file == NULL || boot_record == NULL) {
        return ERR_READING_FILE;
    }

    bytes_read = (int)fread(boot_record, sizeof(Boot_record), 1, file);
    if(bytes_read != 1) {
        return ERR_READING_FILE;
    }
    return OK;
}


/*
 * Prints boot record.
 */
void print_boot_record(Boot_record* boot_record) {
    if(boot_record == NULL) {
        return;
    }

    printf("Volume description: %s\nFat type: %hhd\nFat copies: %hhd\nCluster size: %hdB\nUsable clusters: %d\nSignature: %s\n",
            boot_record->volume_descriptor, boot_record->fat_type, boot_record->fat_copies, boot_record->cluster_size, boot_record->usable_cluster_count, boot_record->signature);
}

/*
 * Loads a fat table from file. File table is expected to contain
 * boot_record->usable_cluster_count records.
 */
int load_fat_table(FILE* file, Boot_record* boot_record, int32_t* dest) {
    int status = 0;
    int size = 0;

    // seek to the start of fat table
    size = sizeof(Boot_record);
    errno = 0;
    status = fseek(file, size, SEEK_SET);
    if(status != 0) {
        return ERR_READING_FILE;
    }

    // load the fat table
    size = sizeof(int32_t)*boot_record->usable_cluster_count;
    status = (int)fread(dest, (size_t)size, 1, file);
    if(status != 1) {
        return ERR_READING_FILE;
    }

    return OK;
}

/*
 * Loads contents of root directory from file and stores them to dest.
 * Dest is expected to be an array.
 *
 * Returns the number of items in dir or ERR_READING_FILE.
 */
int load_dir(FILE *file, Boot_record *boot_record, int cluster, Directory *dest) {
    int status = 0;
    int size = 0;
    int dir_struct_size = 0;
    int item_count = 0;
    int items_read = 0;
    int max_dirs_in_cluster = 0;
    Directory tmp_dir;

    if(file == NULL || boot_record == NULL) {
        return ERR_READING_FILE;
    }

    // seek to the start of root dir
    // boot record size + fat and fat copies size + cluster
    size = get_data_position(boot_record) + cluster*boot_record->cluster_size;
    errno = 0;
    status = fseek(file, size, SEEK_SET);
    if(status != 0) {
        return ERR_READING_FILE;
    }

    // start loading the contents
	// go through the whole cluster and if the dir isn't free, increment the item counter
	item_count = 0;
	max_dirs_in_cluster = max_items_in_directory(boot_record);
	dir_struct_size = sizeof(Directory);
	for(items_read = 0; items_read < max_dirs_in_cluster; items_read++) {
		// read next dir
		errno = 0;
		status = (int)fread(&tmp_dir, (size_t)dir_struct_size, 1, file);
		if(status != 1) {
			return ERR_READING_FILE;
		}

		// dir is not free, add it to dest array
		if(tmp_dir.name[0] != '\0') {
			dest[item_count] = tmp_dir;
            strcpy_s(dest[item_count].name, tmp_dir.name);
			item_count++;
		}
	}

    return item_count;
}

int count_items_in_dir(FILE *file, Boot_record *boot_record, Directory *dir) {
    int status = 0;
    int size = 0;
    int item_count = 0;
    int items_read = 0;
    int dir_struct_size = 0;
    int max_dirs_in_cluster = 0;
    Directory tmp_dir;

    if(file == NULL || boot_record == NULL) {
        return ERR_READING_FILE;
    }

    // seek to the start of root dir
    // boot record size + fat and fat copies size + cluster
    size = get_data_position(boot_record) + dir->start_cluster*boot_record->cluster_size;
    errno = 0;
    status = fseek(file, size, SEEK_SET);
    if(status != 0) {
        return ERR_READING_FILE;
    }

    // start loading the contents
    // go through the whole cluster and if the dir isn't free, increment the item counter
    item_count = 0;
    max_dirs_in_cluster = max_items_in_directory(boot_record);
    dir_struct_size = sizeof(Directory);
    for(items_read = 0; items_read < max_dirs_in_cluster; items_read++) {
    	// read next dir
		errno = 0;
		status = (int)fread(&tmp_dir, (size_t)dir_struct_size, 1, file);
		if(status != 1) {
			return ERR_READING_FILE;
		}

		// dir is not free
		if(tmp_dir.name[0] != '\0') {
			item_count++;
		}
    }

    return item_count;
}

int max_items_in_directory(Boot_record *boot_record) {
    return boot_record->cluster_size / sizeof(Directory);
}

int get_free_cluster(int32_t *fat, int fat_size) {
	int i = 0;
	int ret = NO_CLUSTER;


	for(i = 0; i < fat_size; i++) {
		if(fat[i] == FAT_UNUSED) {
			ret = i;
			break;
		}
	}

	return ret;
}

int get_data_position(Boot_record *boot_record) {
	return sizeof(Boot_record) + sizeof(int32_t)*boot_record->usable_cluster_count*boot_record->fat_copies;
}

int get_free_directory_in_cluster(FILE *file, Boot_record *boot_record, int32_t *fat, int cluster) {
	int status = 0;
	int size = 0;
	int items_read = 0;
	int dir_struct_size = 0;
	int max_dirs_in_cluster = 0;
	int free_dir_pos = 0;
	Directory tmp_dir;

	if(file == NULL || boot_record == NULL) {
		return ERR_READING_FILE;
	}

	// seek to the start of dir
	// boot record size + fat and fat copies size + cluster
	size = get_data_position(boot_record) + cluster*boot_record->cluster_size;
	errno = 0;
	status = fseek(file, size, SEEK_SET);
	if(status != 0) {
		return ERR_READING_FILE;
	}

	// start loading the contents
	// go through the whole cluster and try to find a free directory
	// if no directory is found return NOK.
	max_dirs_in_cluster = max_items_in_directory(boot_record);
	dir_struct_size = sizeof(Directory);
	free_dir_pos = NOK;
	for(items_read = 0; items_read < max_dirs_in_cluster; items_read++) {
		// read next dir
		errno = 0;
		status = (int)fread(&tmp_dir, (size_t)dir_struct_size, 1, file);
		if(status != 1) {
			return ERR_READING_FILE;
		}

		// dir is free
		if(tmp_dir.name[0] == '\0') {
			free_dir_pos = items_read * dir_struct_size;
			break;
		}
	}

	return free_dir_pos;
}

int find_file(FILE *file, Boot_record *boot_record, char **path, int path_length, Directory *found_file, Directory *parent_directory) {
	int max_items_in_dir = 0;
	Directory *dir_items = NULL;
	int item_count = 0;
	int ret = NOK;
	int fp_item = 0;
	int fp_item_found = NOK;
	int file_found = NOK;
	int item = 0;
	Directory tmp_found_file;
	Directory tmp_parent_directory;

	// load root dir
	max_items_in_dir = max_items_in_directory(boot_record);
	dir_items = (Directory *)malloc(sizeof(Directory) * max_items_in_dir);
	item_count = load_dir(file, boot_record, ROOT_CLUSTER, dir_items);
	if(item_count == ERR_READING_FILE) {
		free(dir_items);
		return ERR_READING_FILE;
	}
	tmp_parent_directory.start_cluster = ROOT_CLUSTER;
	tmp_parent_directory.name[0] = '\0';

	// iterate through the items in the current directory and try to find the path item
	// if the path item is found and it's a dir, load that dir and search again.
	// if the path item is the last one (file we're searching for) break the loop.
	file_found = NOK;
	while(file_found == NOK) {
		fp_item_found = NOK;
		for(item = 0; item < item_count; item++) {
			if(fp_item == path_length -1 ) {
				// we're searching for the last path item = file
				if(dir_items[item].isFile && strcmp(dir_items[item].name, path[fp_item]) == 0) {
					// file found
					fp_item_found = OK;
					file_found = OK;
					ret = item;
					tmp_found_file = dir_items[item];
					strcpy_s(tmp_found_file.name, dir_items[item].name);
					break;
				}
			} else {
				// we're searching for other path item = dir
				if(!dir_items[item].isFile && strcmp(dir_items[item].name, path[fp_item]) == 0) {
					fp_item_found = OK;
					tmp_parent_directory = dir_items[item];
					strcpy_s(tmp_parent_directory.name, dir_items[item].name);
					break;
				}
			}
		}

		// check if the file path item was found in the current directory
		if(fp_item_found == OK) {
			if(file_found != OK) {
				// file path item found, but the not the file => move to the next dir
				// clear the dir_items array and load the contens of the next directory
				memset(dir_items, '\0', sizeof(Directory)*max_items_in_dir);
				item_count = load_dir(file, boot_record, tmp_parent_directory.start_cluster, dir_items);
				if(item_count == ERR_READING_FILE) {
					free(dir_items);
					return ERR_READING_FILE;
				}
				fp_item++;
			} else {
				// file found
				if(found_file != NULL) {
					*found_file = tmp_found_file;
					strcpy_s(found_file->name, tmp_found_file.name);
				}
				if(parent_directory != NULL) {
					*parent_directory = tmp_parent_directory;
					strcpy_s(parent_directory->name, tmp_parent_directory.name);
				}

			}
		} else {
			// the file was not found
			free(dir_items);
			break;
		}
	}

	return ret;
}

int find_directory(FILE *file, Boot_record *boot_record, char **path, int path_length, Directory *found_directory, Directory *parent_directory) {
	int max_items_in_dir = 0;
	Directory *dir_items = NULL;
	int item_count = 0;
	int ret = NOK;
	int fp_item = 0;
	int fp_item_found = NOK;
	int dir_found = NOK;
	int item = 0;
	Directory tmp_found_directory;
	Directory tmp_parent_directory;

	if (path_length == 1) {
		// root directory
		if(found_directory != NULL) {
			found_directory->start_cluster = ROOT_CLUSTER;
		}

		if(parent_directory != NULL) {
			parent_directory->start_cluster = ROOT_CLUSTER;
		}

		return 0;
	}

	// load root dir
	max_items_in_dir = max_items_in_directory(boot_record);
	dir_items = (Directory *)malloc(sizeof(Directory) * max_items_in_dir);
	item_count = load_dir(file, boot_record, ROOT_CLUSTER, dir_items);
	if(item_count == ERR_READING_FILE) {
		free(dir_items);
		return ERR_READING_FILE;
	}
	tmp_parent_directory.start_cluster = ROOT_CLUSTER;
	tmp_parent_directory.name[0] = '\0';

	// iterate through the items in the current directory and try to find the path item
	// if the path item is found and it's a dir, load that dir and search again.
	// if the path item is the last one break the loop.
	dir_found = NOK;
	fp_item = 1;
	while(dir_found == NOK) {
		fp_item_found = NOK;
		for(item = 0; item < item_count; item++) {
			if(fp_item == path_length -1 ) {
				// we're searching for the last path item
				if(!dir_items[item].isFile && strcmp(dir_items[item].name, path[fp_item]) == 0) {
					// dir found
					fp_item_found = OK;
					dir_found = OK;
					ret = item;
					tmp_found_directory = dir_items[item];
					strcpy_s(tmp_found_directory.name, dir_items[item].name);
					break;
				}
			} else {
				// we're searching for other path item
				if(!dir_items[item].isFile && strcmp(dir_items[item].name, path[fp_item]) == 0) {
					fp_item_found = OK;
					tmp_parent_directory = dir_items[item];
					strcpy_s(tmp_parent_directory.name, dir_items[item].name);
					break;
				}
			}
		}

		// check if the file path item was found in the current directory
		if(fp_item_found == OK) {
			if(dir_found != OK) {
				// file path item found, but the not the file => move to the next dir
				// clear the dir_items array and load the contents of the next directory
				memset(dir_items, '\0', sizeof(Directory)*max_items_in_dir);
				item_count = load_dir(file, boot_record, tmp_parent_directory.start_cluster, dir_items);
				if(item_count == ERR_READING_FILE) {
					free(dir_items);
					return ERR_READING_FILE;
				}
				fp_item++;
			} else {
				// directory found
				if(found_directory != NULL) {
					*found_directory = tmp_found_directory;
					strcpy_s(found_directory->name, tmp_found_directory.name);
				}
				if(parent_directory != NULL) {
					*parent_directory = tmp_parent_directory;
					strcpy_s(parent_directory->name, tmp_parent_directory.name);
				}

			}
		} else {
			// the file was not found
			free(dir_items);
			break;
		}
	}

	return ret;
}

int is_cluster_bad(char *cluster, int cluster_size) {
    int i = 0;

    // the cluster is too small, assume it's bad
    if(cluster_size < 2*BAD_SEQ_LEN) {
        return OK;
    }

    // check the cluster
    for(i = 0; i < BAD_SEQ_LEN; i++) {

        // start of the cluster
        if(cluster[i] != BAD_BYTE) {
            return NOK;
        }

        // end of the cluster
        if(cluster[cluster_size-1-i] != BAD_BYTE) {
            return NOK;
        }
    }

    return OK;
}

int get_file_position(FILE *file, Boot_record *boot_record, int parent_dir_cluster, char *filename) {
    Directory *items = NULL;
    int max_item_count = max_items_in_directory(boot_record);
    int item_count = 0;
    int i = 0;
    int res = NOK;

    items = (Directory*)malloc(sizeof(Directory) * max_item_count);
    item_count = load_dir(file, boot_record, parent_dir_cluster, items);
    for(i = 0; i < item_count; i++) {
        if(strcmp(filename, items[i].name) == 0) {
            res = i;
            break;
        }
    }
    free(items);

    return res;
}

int update_fat(FILE *file, Boot_record *boot_record, int32_t *fat) {
    int fat_position = sizeof(Boot_record);
    int tmp = 0;
    int i = 0;

    tmp = fseek(file, fat_position, SEEK_SET);
    if(tmp < 0) {
        return NOK;
    }
    for(i = 0; i < boot_record->fat_copies; i++) {
        tmp = (int)fwrite(fat, sizeof(int32_t), (size_t )boot_record->usable_cluster_count, file);
        if(tmp < 0) {
            return NOK;
        }
    }

    return OK;
}

int find_in_dir(FILE *file, Boot_record *boot_record, char *filename, int directory_cluster, bool is_file) {
    int max_items_count = max_items_in_directory(boot_record);
    Directory *items = (Directory*)malloc(sizeof(Directory) * max_items_count);
    int item_count = load_dir(file, boot_record, directory_cluster, items);
    int i = 0;
    int found = NOK;

    for(i = 0; i < item_count; i++) {
        if(strcmp(filename, items[i].name) == 0 && items[i].isFile == is_file) {
            found = OK;
            break;
        }
    }

    free(items);
    return found;
}

int unused_cluster_count(int32_t *fat, int fat_length) {
    int i = 0;
    int count = 0;

    for(i = 0; i < fat_length; i++) {
        if(fat[i] == FAT_UNUSED) {
            count++;
        }
    }

    return count;
}
