#include <string>
#include <cstdlib>
#include "fat.h"

uint16_t load_boot_record(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, Boot_record * bootRecord)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;
	
	// kolik sektoru nacist
	const uint64_t sectorsToRead = (uint64_t) ceil(ALIGNED_BOOT_REC_SIZE / (float)parameters.bytes_per_sector);

	// podle sektoru vypoctena potrebna velikost bufferu
	const uint64_t bufferSize = sectorsToRead * parameters.bytes_per_sector;
	char* buffer = new char[bufferSize];

	addressPacket.count = sectorsToRead;	// precti x sektoru
	addressPacket.lba_index = 0;			// zacni na sektoru 0
	addressPacket.sectors = buffer;			// data nacti do bufferu

	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Read_Sectors);		// jakou operaci nad diskem provest
	registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);					// info pro cteni dat
	registers.rdx.l = diskNumber;													// cislo disku ze ktereho cist
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);		// syscall pro praci s diskem

	if (registers.flags.carry) {
		// chyba pri cteni z disku
		delete[] buffer;
		return FsError::DISK_OPERATION_ERROR;
	}

	std::memcpy(bootRecord, buffer, sizeof(Boot_record));
	delete[] buffer;

	return FsError::SUCCESS;
}

uint16_t load_fat(const std::uint8_t diskNumber, const Boot_record& bootRecord, int32_t * dest)
{
	const size_t fatSizeBytes = (size_t)(bootRecord.usable_cluster_count * sizeof(int32_t));

	// potrebuju nacist fatSizeBytes + ALIGNED_BOOT_REC_SIZE bytu, kolik je to sectoru?
	const uint64_t sectorCount = (uint64_t)ceil((fatSizeBytes + ALIGNED_BOOT_REC_SIZE) / (float)bootRecord.bytes_per_sector);
	char* buffer = new char[sectorCount * bootRecord.bytes_per_sector];

	uint16_t readRes = read_from_disk(diskNumber, 0, sectorCount, buffer);
	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		delete[] buffer;
		return FsError::DISK_OPERATION_ERROR;
	}

	// zkopirovani FAT do dest
	memcpy(dest, &(buffer[ALIGNED_BOOT_REC_SIZE]), fatSizeBytes);

	// uklid 
	delete[] buffer;

	return FsError::SUCCESS;
}

uint16_t load_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int cluster, Directory * dest)
{
	// od ktereho sektoru zacit cist? 
	const int byteOffset = cluster_byte_offset(bootRecord, cluster);
	const uint64_t startSector = byteOffset / bootRecord.bytes_per_sector;

	// kolik sektoru nacist, aby obsahovaly vse co chci?
	const int bytesToRead = sizeof(Directory);
	const uint64_t sectorCount = sectors_to_read(startSector, byteOffset, bytesToRead, bootRecord.bytes_per_sector);
	char* buffer = new char[sectorCount * bootRecord.bytes_per_sector];

	// na kterem bytu v bufferu zacinaji relevantni data?
	const uint64_t bufferOffset = byteOffset - startSector * bootRecord.bytes_per_sector;

	uint16_t readRes = read_from_disk(diskNumber, startSector, sectorCount, buffer);
	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		delete[] buffer;
		return FsError::DISK_OPERATION_ERROR;
	}

	// zkopirovani adresare do dest
	memcpy(dest, &(buffer[bufferOffset]), bytesToRead);

	// uklid 
	delete[] buffer;

	return FsError::SUCCESS;
}

uint16_t load_items_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & dir, std::vector<Directory>& dest)
{
	// kontrola, ze je vazne potreba cokoliv delat
	if (dir.isFile) {
		return FsError::NOT_A_DIR;
	}

	int i = 0;

	// od ktereho sektoru zacit cist? 
	const int byteOffset = cluster_byte_offset(bootRecord, dir.start_cluster);
	const uint64_t startSector = byteOffset / bootRecord.bytes_per_sector;

	// kolik sektoru nacist, aby obsahovaly vse co chci?
	// pocitam adresare, takze vsechny itemy (i prazdne)
	const int itemsInDirectory = max_items_in_directory(bootRecord);
	const int bytesToRead = sizeof(Directory) * itemsInDirectory;
	const uint64_t sectorCount = sectors_to_read(startSector, byteOffset, bytesToRead, bootRecord.bytes_per_sector);
	char* buffer = new char[sectorCount * bootRecord.bytes_per_sector];
	Directory* dirItems = new Directory[itemsInDirectory];

	// na kterem bytu v bufferu zacinaji relevantni data?
	const uint64_t bufferOffset = byteOffset - startSector * bootRecord.bytes_per_sector;

	uint16_t readRes = read_from_disk(diskNumber, startSector, sectorCount, buffer);

	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		delete[] buffer;
		delete[] dirItems;
		return FsError::DISK_OPERATION_ERROR;
	}

	// zkopirovani itemu v adresari do dirItems
	memcpy(dirItems, &(buffer[bufferOffset]), bytesToRead);

	// kolik teda vlastne itemu je v tom adresari
	for (i = 0; i < itemsInDirectory; i++) {
		if (dirItems[i].name[0] != '\0') {
			// jmeno neni prazdne -> existujici soubor
			dest.push_back(dirItems[i]);
		}
	}

	// uklid
	delete[] buffer;
	delete[] dirItems;


	return FsError::SUCCESS;
}

uint16_t read_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t * fatTable, const Directory & fileToRead, char * buffer, size_t bufferLen)
{
	if (!fileToRead.isFile) {
		return FsError::NOT_A_FILE;
	}

	uint16_t res = FsError::UNKNOWN_ERROR,
			readRes = FsError::UNKNOWN_ERROR;
	int totalBytes = 0,
		byteOffset = 0,
		currentCluster = fileToRead.start_cluster;
	uint64_t startSector = 0,
		sectorCount = 0,
		bufferOffset = 0;
	char* sectorBuffer = NULL;
	size_t currBytesToRead = bootRecord.cluster_size;
	bool bufferFull = false;

	// po jednom nacitej clustery z disku
	while (currentCluster != FAT_FILE_END && !bufferFull) {
		// cteni po clusterech 
		byteOffset = cluster_byte_offset(bootRecord, currentCluster);
		startSector = byteOffset / bootRecord.bytes_per_sector;
		sectorCount = sectors_to_read(startSector, byteOffset, bootRecord.cluster_size, bootRecord.bytes_per_sector);
		bufferOffset = byteOffset - startSector * bootRecord.bytes_per_sector;
		sectorBuffer = new char[sectorCount * bootRecord.bytes_per_sector];

		// nacti sektory obsahujici 1 cluster do sectorBufferu
		readRes = read_from_disk(diskNumber, startSector, sectorCount, sectorBuffer);
		if (readRes != FsError::SUCCESS) {
			// chyba pri cteni z disku
			res = FsError::DISK_OPERATION_ERROR;
			delete[] sectorBuffer;
			break;
		}

		// ze sectorBufferu nacti cluster do bufferu
		// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
		// nacti jen to co muzes
		if (totalBytes + bootRecord.cluster_size > bufferLen) {
			currBytesToRead = bufferLen - (totalBytes);
			bufferFull = true;
		}
		memcpy(&(buffer[totalBytes]), &(sectorBuffer[bufferOffset]), currBytesToRead);
		totalBytes += currBytesToRead;
		// todo: text v testovacim prakladu ma na konci kazdeho sektoru byte '\0' coz znamena
		// ze i kdyz se cely text ze souboru nacte (ze vsech clusteru), je vypsana pouze prvni
		// cast (protoze pak je '\0' a az pak je dalsi cast).
		buffer[totalBytes-1] = ' ';
		
		// prejdi na dalsi cluster
		currentCluster = fatTable[currentCluster];
		delete[] sectorBuffer;
	}

	// uspesne jme vse precetli
	if (currentCluster == FAT_FILE_END || bufferFull) {
		res = FsError::SUCCESS;
	}

	return res;
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
	//max_dirs_in_cluster = max_items_in_directory_old(boot_record);
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

uint16_t find_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const std::vector<std::string>& filePath, Directory & foundFile, Directory & parentDirectory)
{
	// pokud neni zadna cesta, vrat root
	if (filePath.empty()) {
		foundFile.isFile = false;
		foundFile.start_cluster = ROOT_CLUSTER;
		return FsError::SUCCESS;
	}

	const int maxItemsInDir = max_items_in_directory(bootRecord);
	uint16_t opRes = 0;
	Directory tmpParentDir;
	std::vector<Directory> dirItems;
	bool searchForFile = true;
	uint16_t res = FsError::FILE_NOT_FOUND;
	int currPathItem = 0;
	int i = 0;

	// nacti obsah rootu
	tmpParentDir.start_cluster = ROOT_CLUSTER;
	tmpParentDir.isFile = false;
	tmpParentDir.name[0] = '\0';
	opRes = load_items_in_dir(diskNumber, bootRecord, tmpParentDir, dirItems);
	if (opRes != FsError::SUCCESS) {
		return opRes;
	}

	while (searchForFile) {
		// najdi currPathItem v dirItems
		i = is_item_in_dir(filePath[currPathItem], dirItems);
		if (i >= 0) {
			// item nalezen
			// pokud je currPathItem posledni ve filepath, pak 
			// jsme nasli hledany soubor
			// pokud ne, zanoreni do dalsi urovne
			if (currPathItem == filePath.size() - 1) {
				copy_dir(foundFile, dirItems[i]);
				copy_dir(parentDirectory, tmpParentDir);
				searchForFile = false;
				res = FsError::SUCCESS;
				break;
			}
			else if (currPathItem < filePath.size() - 1 && dirItems[i].isFile) {
				// jeste nejsme na konci cesty, ale byl nalezen file misto adresare
				searchForFile = false;
				res = FsError::NOT_A_DIR;
				break;
			}
			else {
				// nejsme na konci cesty
				// zanoreni do prave nalezeneho adresare
				currPathItem++;
				copy_dir(tmpParentDir, dirItems[i]);
				opRes = load_items_in_dir(diskNumber, bootRecord, tmpParentDir, dirItems);
				if (opRes != FsError::SUCCESS) {
					searchForFile = false;
					res = opRes;
				}
				break;
			}
		}
		else {
			// item nenalezen
			searchForFile = false;
			opRes = FsError::FILE_NOT_FOUND;
		}
	}

	return res;
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
    //int max_item_count = max_items_in_directory_old(boot_record);
	int max_item_count = 0;
    int item_count = 0;
    int i = 0;
    int res = NOK;

    items = (Directory*)malloc(sizeof(Directory) * max_item_count);
    //item_count = load_dir_old(file, boot_record, parent_dir_cluster, items);
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
    //int max_items_count = max_items_in_directory_old(boot_record);
    int max_items_count = 0;
    Directory *items = (Directory*)malloc(sizeof(Directory) * max_items_count);
    //int item_count = load_dir_old(file, boot_record, directory_cluster, items);
	int item_count = 0;
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

int is_item_in_dir(const std::string itemName, const std::vector<Directory>& items)
{
	for (size_t i = 0; i < items.size(); i++)
	{
		if (itemName.compare(items[i].name) == 0) {
			return (int)i;
		}
	}
	return -1;
}

void copy_dir(Directory & dest, const Directory & source)
{
	dest = source;
	strcpy_s(dest.name, source.name);
}

uint16_t read_from_disk(const std::uint8_t diskNumber, const uint64_t startSector, const uint64_t sectorCount, char * buffer)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;

	addressPacket.lba_index = startSector;	// zacni na sektoru 
	addressPacket.count = sectorCount;		// precti x sektoru
	addressPacket.sectors = buffer;			// data prenacti do bufferu

	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Read_Sectors);		// jakou operaci nad diskem provest
	registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);					// info pro cteni dat
	registers.rdx.l = diskNumber;													// cislo disku ze ktereho cist
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);		// syscall pro praci s diskem

	if (registers.flags.carry) {
		// chyba pri cteni z disku
		return FsError::DISK_OPERATION_ERROR;
	}

	return FsError::SUCCESS;
}

uint16_t write_to_disk(const std::uint8_t diskNumber, const uint64_t startSector, const uint64_t sectorCount, char * buffer)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;

	addressPacket.lba_index = startSector;	// zacni na sektoru 
	addressPacket.count = sectorCount;		// zapis x sektoru
	addressPacket.sectors = buffer;			// data pro zapis

	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Write_Sectors);		// jakou operaci nad diskem provest
	registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);					// info pro cteni dat
	registers.rdx.l = diskNumber;													// cislo disku ze ktereho cist
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);		// syscall pro praci s diskem

	if (registers.flags.carry) {
		// chyba pri cteni z disku
		return FsError::DISK_OPERATION_ERROR;
	}

	return FsError::SUCCESS;
}

bool is_valid_fat(Boot_record & bootRecord)
{
	return bootRecord.usable_cluster_count > 0 && bootRecord.bytes_per_sector > 0;
}

uint16_t init_fat(const kiv_hal::TDrive_Parameters parameters, char* buffer) {
    Boot_record new_record;
    std::string volume_desc("KIV/OS volume.");
    std::string signature("kiv-os");
    int32_t fat_table[252];
    int i = 0;

    fat_table[0] = ROOT_CLUSTER;
    for(i = 1; i < 251; i++) {
        fat_table[i] = FAT_UNUSED;
    }

    // fill the descriptor with 0s
    // then copy the actual description
    memset(new_record.volume_descriptor, 0x0, DESCRIPTION_LEN);
    volume_desc.copy(new_record.volume_descriptor, volume_desc.length());

    // do the same for the signature
    memset(new_record.signature, 0x0, SIGNATURE_LEN);
    signature.copy(new_record.signature, signature.length());

    // FAT8
    // 256 clusters, 4 reserved
    // 1 cluster = 256 B
	// todo: use parameters to calculate actual cluster size/count
    new_record.fat_type = 8;
    new_record.fat_copies = 2;
    new_record.usable_cluster_count = 252;
	new_record.cluster_size = 256;
	new_record.bytes_per_sector = parameters.bytes_per_sector;

	// boot sector
	memcpy(buffer, &new_record, sizeof(Boot_record));

    // fat table
	memcpy(&(buffer[ALIGNED_BOOT_REC_SIZE]), &fat_table, sizeof(fat_table));

    // fat table copy
	memcpy(&(buffer[ALIGNED_BOOT_REC_SIZE]), &fat_table, sizeof(fat_table));

    return FsError::SUCCESS;
}
