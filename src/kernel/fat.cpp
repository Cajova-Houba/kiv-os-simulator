#include <string>
#include <cstdlib>
#include "fat.h"

uint16_t init_fat(const std::uint8_t diskNumber, kiv_hal::TDrive_Parameters parameters)
{
	Boot_record new_record;
	std::string volume_desc("KIV/OS volume.");
	std::string signature("kiv-os");
	int32_t fat_table[DEF_MIN_DATA_CL];
	char *buffer;
	size_t i = 0;
	uint64_t sectorCount = 0;
	uint16_t isError = 0;
	// boot record + 2*FAT + data clusters
	const uint64_t baseSize = ALIGNED_BOOT_REC_SIZE
		+ DEF_FAT_COPIES * DEF_MIN_DATA_CL * sizeof(int32_t);
	const uint64_t minReqSize = baseSize
		+ DEF_MIN_DATA_CL * parameters.bytes_per_sector;

	// muzeme vubec inicializovat na tomhle disku?
	if (parameters.bytes_per_sector < MIN_REQ_SECOTR_SIZE) {
		return FsError::INCOMPATIBLE_DISK;
	}

	// kontrola jestli je na disku misto na celou strukturu FAT
	if (parameters.absolute_number_of_sectors * parameters.bytes_per_sector < minReqSize) {
		return FsError::FULL_DISK;
	}

	fat_table[0] = ROOT_CLUSTER;
	for (i = 1; i < DEF_MIN_DATA_CL; i++) {
		fat_table[i] = FAT_UNUSED;
	}

	// vytvoreni bufferu zapsani boot sectoru a FAT
	sectorCount = (uint64_t)ceil(baseSize / (float)parameters.bytes_per_sector);
	buffer = new char[sectorCount * parameters.bytes_per_sector];

	// prenaplneni popis 0, pak nakopirovani retezce
	memset(new_record.volume_descriptor, 0x0, DESCRIPTION_LEN);
	volume_desc.copy(new_record.volume_descriptor, volume_desc.length());

	// to same pro podpis
	memset(new_record.signature, 0x0, SIGNATURE_LEN);
	signature.copy(new_record.signature, signature.length());

	// FAT8
	// 1 cluster = 1 secotr
	new_record.fat_type = 8;
	new_record.fat_copies = DEF_FAT_COPIES;
	new_record.usable_cluster_count = DEF_MIN_DATA_CL;
	new_record.cluster_size = 1;
	new_record.bytes_per_sector = parameters.bytes_per_sector;

	// boot sector
	memcpy(buffer, &new_record, sizeof(Boot_record));

	// fat a kopie
	for (i = 0; i < new_record.fat_copies; i++) {
		memcpy(&(buffer[ALIGNED_BOOT_REC_SIZE + i* DEF_MIN_DATA_CL * sizeof(int32_t)]), &fat_table, sizeof(fat_table));
	}

	// zapis metadat FAT
	isError = write_to_disk(diskNumber, 0, sectorCount, buffer);
	delete[] buffer;
	if (isError) {
		return isError;
	}

	// zapis root clusteru, jen 0
	buffer = new char[parameters.bytes_per_sector];
	memset(buffer, 0, parameters.bytes_per_sector);
	isError = write_to_disk(diskNumber, first_data_sector(new_record), 1, buffer);
	delete[] buffer;

	return isError;
}

uint16_t load_boot_record(const std::uint8_t diskNumber, const kiv_hal::TDrive_Parameters parameters, Boot_record & bootRecord)
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

	std::memcpy(&bootRecord, buffer, sizeof(Boot_record));
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

uint16_t load_items_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & dir, std::vector<Directory>& dest)
{
	// kontrola, ze je vazne potreba cokoliv delat
	if (dir.isFile) {
		return FsError::NOT_A_DIR;
	}

	int i = 0;

	// nactu 1 cluster s adresarem
	const size_t itemsInDirectory = max_items_in_directory(bootRecord);
	size_t bufferLen = bootRecord.bytes_per_sector * bootRecord.cluster_size;
	char* buffer = new char[bufferLen];
	Directory* dirItems = new Directory[itemsInDirectory];

	uint16_t readRes = read_cluster_range(diskNumber, bootRecord, dir.start_cluster, 1, buffer, bufferLen);

	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		delete[] buffer;
		delete[] dirItems;
		return FsError::DISK_OPERATION_ERROR;
	}

	// zkopirovani itemu v adresari do dirItems
	memcpy(dirItems, buffer, sizeof(Directory) * itemsInDirectory);

	// vyber neprazdne polozky
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

    const size_t bytesPerCluster = bytes_per_cluster(bootRecord);

	uint16_t res = FsError::UNKNOWN_ERROR,
			readRes = FsError::UNKNOWN_ERROR;
	int32_t currentCluster = fileToRead.start_cluster;
	
	size_t currBytesToRead = bytesPerCluster,
		totalBytes = 0;
	bool bufferFull = false;

	// todo: tady je prostor pro optimalizaci
	// lze predem pripravit seznam chunku clusteru
	// ktere se maji nacist a predat je metode read_cluster_range
	// misto soucasneho cteni po 1

	// po jednom nacitej clustery z disku
	while (currentCluster != FAT_FILE_END && !bufferFull) {
		// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
		// nacti jen to co muzes
		if (totalBytes + bytesPerCluster > bufferLen) {
			currBytesToRead = bufferLen - (totalBytes);
			bufferFull = true;
		}
		readRes = read_cluster_range(diskNumber, bootRecord, currentCluster, 1, &(buffer[totalBytes]), currBytesToRead);
		if (readRes != FsError::SUCCESS) {
			res = readRes;
			break;
		}
		totalBytes += currBytesToRead;
		// todo: text v testovacim prikladu ma na konci kazdeho sektoru byte '\0' coz znamena
		// ze i kdyz se cely text ze souboru nacte (ze vsech clusteru), je vypsana pouze prvni
		// cast (protoze pak je '\0' a az pak je dalsi cast).
		buffer[totalBytes-1] = ' ';
		
		// prejdi na dalsi cluster
		currentCluster = fatTable[currentCluster];
	}

	// uspesne jme vse precetli
	if (currentCluster == FAT_FILE_END || bufferFull) {
		res = FsError::SUCCESS;
	}

	return res;
}

uint16_t write_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t * fatTable, const Directory & fileToWriteTo, size_t offset, char * buffer, size_t bufferLen)
{
	// todo:
	if (!fileToWriteTo.isFile) {
		return FsError::NOT_A_FILE;
	}

	//const size_t bytesPerCluster = bytes_per_cluster(bootRecord);
	uint16_t res = FsError::UNKNOWN_ERROR,
		isReadError = FsError::UNKNOWN_ERROR;
	size_t bytesToWrite = bufferLen;
	char* clusterBuffer = new char[bootRecord.cluster_size];
	
	// od ktere casti ktereho clusteru se bude zapisovat?
	int32_t startCluster = (int32_t) offset / bootRecord.cluster_size;
	size_t clusterOffset = offset - startCluster * bootRecord.cluster_size;

	// pokud je offset vetsi nez velikost souboru, kolik nul
	// je potreba zapsat mezi konec souboru a data k zapsani?
	size_t fillBytesToWrite = 0;
	size_t lastClusterFillBytesOffset = 0;
	int32_t lastCluster = fileToWriteTo.start_cluster;
	size_t currBytesToWrite = 0;
	size_t fillClusterCount = 0;
	size_t i = 0;
	if (offset > fileToWriteTo.size) {
		fillBytesToWrite = offset - fileToWriteTo.size;

		// zapsat vyplnove 0 na konec soboru, pokud je treba
		while (fatTable[lastCluster] != FAT_FILE_END) {
			lastCluster = fatTable[lastCluster];
		}

		lastClusterFillBytesOffset = lastCluster * bootRecord.cluster_size - fileToWriteTo.size;
		isReadError = read_cluster_range(diskNumber, bootRecord, lastCluster, 1, clusterBuffer, bootRecord.cluster_size);
		if (isReadError) {
			delete[] clusterBuffer;
			return isReadError;
		}

		// prazdne misto v poslednim clusteru souboru se zaplni 0
		currBytesToWrite = bootRecord.cluster_size - lastClusterFillBytesOffset;
		memset(&(clusterBuffer[lastClusterFillBytesOffset]), 0, currBytesToWrite);
		fillBytesToWrite -= currBytesToWrite;

		// kolik dalsich clusteru bude potreba naalokovat?
		fillClusterCount = (size_t) ceil(fillBytesToWrite / (float)bootRecord.cluster_size);
		for (i = 0; i < fillClusterCount; i++) {
			lastCluster = get_free_cluster(fatTable, bootRecord.usable_cluster_count);
			if (lastCluster == NO_CLUSTER) {
				delete[] clusterBuffer;
				return FsError::FULL_DISK;
			}

			memset(clusterBuffer, 0, sizeof(clusterBuffer));
		}
	}

	return res;
}

uint16_t create_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t * fatTable, const Directory parentDirectory, Directory & newFile)
{
	const size_t maxItemsInDir = max_items_in_directory(bootRecord);
	const size_t bytesPerCluster = bytes_per_cluster(bootRecord);

	bool parentDirFull = false,
		fatFull = false;
	int16_t isError;
	std::vector<Directory> itemsInParentDir;
	int32_t nextFreeCluster = 0;
	char *dirBuffer;
	int dirPosition = 0;

	// zjistit jestli je misto ve FAT
	nextFreeCluster = get_free_cluster(fatTable, bootRecord.usable_cluster_count);
	if (nextFreeCluster == NO_CLUSTER) {
		return FsError::FULL_DISK;
	}

	// vytvorit soubor
	newFile.start_cluster = nextFreeCluster;
	if (newFile.isFile) {
		newFile.size = (uint32_t) bytesPerCluster;
	}
	else {
		newFile.size = 0;
	}
	
	// buffer na adresar veliksoti 1 clusteru
	dirBuffer = new char[newFile.size];
	isError = read_cluster_range(diskNumber, bootRecord, parentDirectory.start_cluster, 1, dirBuffer, bytesPerCluster);
	if (isError) {
		delete[] dirBuffer;
		return isError;
	}

	// zjistit jestli je misto v adresari
	dirPosition = get_free_dir_item_in_cluster(dirBuffer, maxItemsInDir);
	if (dirPosition < 0) {
		delete[] dirBuffer;
		return FsError::FULL_DIR;
	}

	// vlozit novy soubor do clusteru a zapsat novy cluster pro soubor
	memcpy(&(dirBuffer[dirPosition * sizeof(Directory)]), &newFile, sizeof(Directory));
	write_cluster_range(diskNumber, bootRecord, parentDirectory.start_cluster, 1, dirBuffer);

	memset(dirBuffer, 0, bytesPerCluster);
	write_cluster_range(diskNumber, bootRecord, newFile.start_cluster, 1, dirBuffer);
	delete[] dirBuffer;

	// update FAT
	fatTable[nextFreeCluster] = FAT_FILE_END;
	return update_fat(diskNumber, bootRecord, fatTable);
}

int get_free_cluster(const int32_t *fat, const int fat_size) {
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

int get_free_directory_in_cluster_old(FILE *file, Boot_record *boot_record, int32_t *fat, int cluster) {
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

uint16_t update_fat(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t *fat) {
	// zapiseme br, fat i jeji kopie najednou
	// pokud to bude mnoc pameti, 
	// muzeme zmenit na postupne zapisovani (treba po 1 kb)
	const size_t fatLen = bootRecord.usable_cluster_count;
	const size_t bytesToWrite = sizeof(bootRecord) + bootRecord.usable_cluster_count*bootRecord.fat_copies * fatLen;
	const uint64_t sectorCount = first_data_sector(bootRecord);
	char *buffer = new char[sectorCount * bootRecord.bytes_per_sector];
	size_t i = 0,
		bufferPointer = 0;
	uint16_t isError = 0;

	// prednaplnime buffer 0, aby tam nebylo nic divnyho
	memset(buffer, 0, sizeof(buffer));

	// boot record
	memcpy(buffer, &bootRecord, sizeof(Boot_record));
	bufferPointer += sizeof(Boot_record);

	// fat and copies
	for (i = 0; i < bootRecord.fat_copies; i++) {
		bufferPointer += i * fatLen * sizeof(int32_t);
		memcpy(&(buffer[bufferPointer]), fat, fatLen);
	}

	isError = write_to_disk(diskNumber, 0, sectorCount, buffer);
	delete[] buffer;
	return isError;
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

int get_free_dir_item_in_cluster(char * clusterBuffer, size_t dirItemCount)
{
	Directory *dirItems = new Directory[dirItemCount];
	size_t i = 0;
	int res = -1;

	memcpy(dirItems, clusterBuffer, sizeof(Directory) * dirItemCount);
	for (i = 0; i < dirItemCount; i++) {
		if (dirItems[i].name[0] == '\0') {
			res = (int)i;
			break;
		}
	}

	delete[] dirItems;
	return res;
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

uint16_t read_cluster_range(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t cluster, const uint32_t clusterCount, char* buffer, const size_t bufferLen)
{
	uint64_t startSector = first_data_sector(bootRecord) + cluster * bootRecord.cluster_size;
	uint64_t sectorCount = clusterCount * bootRecord.cluster_size;
	size_t bytesToRead = sectorCount * bootRecord.bytes_per_sector;
	char* sectorBuffer = new char[sectorCount * bootRecord.bytes_per_sector];

	// nacti sektory obsahujici 1 cluster do sectorBufferu
	uint16_t readRes = read_from_disk(diskNumber, startSector, sectorCount, sectorBuffer);
	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		delete[] sectorBuffer;
		return readRes;
	}

	// ze sectorBufferu nacti cluster do bufferu
	// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
	// nacti jen to co muzes
	if (bytesToRead < bufferLen) {
		bytesToRead = bufferLen;
	}
	memcpy(buffer, sectorBuffer, bytesToRead);
	return FsError::SUCCESS;
}

uint16_t write_cluster_range(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t cluster, const uint32_t clusterCount, char * buffer)
{
	uint64_t startSector = first_data_sector(bootRecord) + cluster * bootRecord.cluster_size;
	uint64_t sectorCount = clusterCount * bootRecord.cluster_size;

	return write_to_disk(diskNumber, startSector, sectorCount, buffer);
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

uint64_t first_data_sector(const Boot_record & bootRecord)
{
	return (uint64_t)ceil((ALIGNED_BOOT_REC_SIZE
		+ sizeof(int32_t) * bootRecord.usable_cluster_count * bootRecord.fat_copies)
		/ (float)bootRecord.bytes_per_sector);
}

bool is_valid_fat(Boot_record & bootRecord)
{
	return bootRecord.usable_cluster_count > 0;
}
