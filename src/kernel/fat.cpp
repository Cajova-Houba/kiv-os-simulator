#include <string>
#include <cstdlib>
#include <array>
#include "fat.h"

uint16_t init_fat(const std::uint8_t diskNumber, kiv_hal::TDrive_Parameters parameters)
{
	const uint64_t hdd_size = parameters.absolute_number_of_sectors * parameters.bytes_per_sector;
	Boot_record new_record;
	std::string volume_desc("KIV/OS volume.");
	std::string signature("kiv-os");
	std::vector<int32_t> fat_table;
	std::vector<char> buffer;
	size_t i = 0;
	uint16_t isError = 0;
	size_t sectors_per_cluster = 0;
	size_t cluster_size = 0;
	int64_t fat_size;
	size_t base_sectors = 0;
	size_t data_sectors = 0;

	// kolik sektoru bude zabirat jeden cluster
	sectors_per_cluster = (size_t) PREFERRED_CLUSTER_SIZE / parameters.bytes_per_sector;
	if (sectors_per_cluster == 0) {
		sectors_per_cluster = 1;
	}

	if (sectors_per_cluster > UINT16_MAX) {
		sectors_per_cluster = UINT16_MAX;
	}
	cluster_size = sectors_per_cluster * parameters.bytes_per_sector;


	// kolik mista z disku muzeme vyuzit
	// vzorecek: fat_size = sizeof(int32_t) * (HDD - bootSec - fat_size) / cluster_size
	fat_size = (int64_t)(hdd_size - ALIGNED_BOOT_REC_SIZE) / (cluster_size + sizeof(int32_t)*DEF_FAT_COPIES);
	if (fat_size > UINT32_MAX) {
		fat_size = UINT32_MAX;
	}

	// kontrola jestli vse vychazi
	base_sectors = (size_t)ceil((ALIGNED_BOOT_REC_SIZE + fat_size * DEF_FAT_COPIES * sizeof(int32_t)) / (float)parameters.bytes_per_sector);
	data_sectors = fat_size * sectors_per_cluster;
	if (base_sectors + data_sectors > parameters.absolute_number_of_sectors) {
		// tohle je trochu naivni pristup, ale teoreticky by to mel byt vyjimecny pripad, tak to snad nebude vadit
		do {
			fat_size--;
			base_sectors = (size_t)ceil((ALIGNED_BOOT_REC_SIZE + fat_size * DEF_FAT_COPIES * sizeof(int32_t)) / (float)parameters.bytes_per_sector);
			data_sectors -= sectors_per_cluster;
		} while (base_sectors + data_sectors > parameters.absolute_number_of_sectors && fat_size >= 0);
	}

	if (fat_size <= 0) {
		return FsError::INCOMPATIBLE_DISK;
	}

	// inicializace FAT
	fat_table.resize((size_t)fat_size);
	fat_table[0] = FAT_DIRECTORY;
	for (i = 1; i < (size_t)fat_size; i++) {
		fat_table[i] = FAT_UNUSED;
	}

	// vytvoreni bufferu zapsani boot sectoru a FAT
	buffer.resize(base_sectors * parameters.bytes_per_sector, 0);

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
	new_record.usable_cluster_count = (uint32_t) fat_size;
	new_record.cluster_size = (uint16_t) sectors_per_cluster;
	new_record.bytes_per_sector = (uint16_t) parameters.bytes_per_sector;

	// boot sector
	memcpy(&(buffer[0]), &new_record, sizeof(Boot_record));

	// fat a kopie
	for (i = 0; i < new_record.fat_copies; i++) {
		memcpy(&(buffer[ALIGNED_BOOT_REC_SIZE + i* fat_size * sizeof(int32_t)]), &(fat_table[0]), fat_size *sizeof(int32_t));
	}

	// zapis metadat FAT
	isError = write_to_disk(diskNumber, 0, base_sectors, &(buffer[0]));
	if (isError) {
		return isError;
	}

	// zapis root clusteru, jen 0
	buffer.resize(cluster_size);
	memset(&(buffer[0]), 0, cluster_size);
	isError = write_to_disk(diskNumber, first_data_sector(new_record), 1, &(buffer[0]));

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

uint16_t load_items_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & dir, std::vector<Directory>& dest, const bool includeEmpty)
{
	// kontrola, ze je vazne potreba cokoliv delat
	if (dir.isFile) {
		return FsError::NOT_A_DIR;
	}

	// nactu 1 cluster s adresarem
	const size_t itemsInDirectory = max_items_in_directory(bootRecord);
	size_t bufferLen = bootRecord.bytes_per_sector * bootRecord.cluster_size,
		i = 0;
	char* buffer = new char[bufferLen];
	Directory * dirItems = nullptr;

	uint16_t readRes = read_cluster_range(diskNumber, bootRecord, dir.start_cluster, 1, buffer, bufferLen);

	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		delete[] buffer;
		return FsError::DISK_OPERATION_ERROR;
	}

	// zkopirovani itemu v adresari do dirItems
	dirItems = (Directory *)buffer;

	// vyber neprazdne polozky
	for (i = 0; i < itemsInDirectory; i++) {
		if (includeEmpty || dirItems[i].name[0] != '\0') {
			// bud chceme vse, nebo jen neprazdne polozky
			dest.push_back(dirItems[i]);
		} 
	}

	// uklid
	delete[] buffer;

	return FsError::SUCCESS;
}

uint16_t read_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t* fatTable, const Directory & fileToRead, char * buffer, const size_t bufferLen, const size_t offset)
{
	if (!fileToRead.isFile) {
		return FsError::NOT_A_FILE;
	}

	// neni co cist
	if (offset >= fileToRead.size || bufferLen == 0) {
		return FsError::SUCCESS;
	}

    const size_t bytesPerCluster = bytes_per_cluster(bootRecord);

	uint16_t res = FsError::UNKNOWN_ERROR,
			isError = FsError::UNKNOWN_ERROR;
	int32_t currentCluster = fileToRead.start_cluster;
	std::vector<std::array<int32_t, 2>> chunks;
	
	size_t currBytesToRead = bytesPerCluster,
		totalBytes = 0,
		currOffset = 0,
		i = 0;
	bool bufferFull = false;

	// od ktere casti ktereho clusteru budeme cist
	currentCluster = get_cluster_by_offset(fatTable, fileToRead.start_cluster, offset, bytesPerCluster);
	currOffset = offset % bytesPerCluster;
	currBytesToRead = bytesPerCluster - currOffset;

	// rozdelit zbytek soubor na chunky
	split_file_to_chunks(fatTable, currentCluster, chunks);

	// ted nacitej po clusterech
	for (i = 0; i < chunks.size() && !bufferFull; i++) {
		// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
		// nacti jen to co muzes
		currBytesToRead = chunks[i][1] * bytesPerCluster - currOffset;
		if (totalBytes + currBytesToRead > bufferLen) {
			currBytesToRead = bufferLen - totalBytes;
			bufferFull = true;
		}

		// nacti chunk do bufferu
		isError = read_cluster_range(diskNumber, bootRecord, chunks[i][0], (uint32_t)chunks[i][1], &(buffer[totalBytes]), currBytesToRead, currOffset);
		if (isError) {
			res = isError;
			break;
		}
		totalBytes += currBytesToRead;

		// s offsetem cteme jen pri ctnei prvniho clusteru
		currOffset = 0;
	}

	// uspesne jme vse precetli
	if (currentCluster == FAT_FILE_END || bufferFull) {
		res = FsError::SUCCESS;
	} 

	return res;
}

uint16_t write_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t * fatTable, Directory & fileToWriteTo, const size_t offset, char * buffer, const size_t bufferLen)
{
	if (!fileToWriteTo.isFile) {
		return FsError::NOT_A_FILE;
	}

	if (bufferLen == 0) {
		return FsError::SUCCESS;
	}

	uint16_t isError = FsError::UNKNOWN_ERROR;
	const int32_t lastFileCluster = find_last_file_cluster(fatTable, fileToWriteTo.start_cluster);
	const size_t freeClusterCount = count_free_clusters(fatTable, bootRecord.usable_cluster_count);
	const size_t fileClusterCount = count_file_clusters(fatTable, fileToWriteTo.start_cluster);
	const size_t clusterSize = bytes_per_cluster(bootRecord);
	size_t clustersToAllocate = 0,
		fillBytes = 0,
		newBytes = 0,
		writtenBytes = 0,
		bytesToWrite = 0,
		clusterOffset = 0;
	int32_t currCluster = 0;
	std::vector<char> clusterBuffer(clusterSize, 0);

	// misto ktere uz je pro soubor alokovane
	const size_t allocatedSize = fileClusterCount * clusterSize;

	if (offset + bufferLen > allocatedSize) {
		// zapisovana data presahnou velikost souboru
		if (offset > allocatedSize) {
			// offset je za koncem souboru, musime alokovat
			// misto na 0 a na data
			fillBytes = offset - allocatedSize;
			newBytes = fillBytes + bufferLen;
		}
		else {
			// offset je pred koncem souboru, ale data presahnou konec souboru
			// musime alokovat clustery na data
			newBytes = bufferLen - (allocatedSize - offset);
		}
	}

	// kolik clusteru musime alokovat pro data?
	clustersToAllocate = (size_t)ceil(newBytes / (float)clusterSize);

	isError = allocate_clusters(bootRecord, fatTable, lastFileCluster, clustersToAllocate);
	if (isError) {
		return isError;
	}

	// od ktereho clsuteru budeme zapisovat
	if (fillBytes > 0) {
		// budeme zapisovat vyplnove byty -> zacneme na konci soucasneho souboru
		currCluster = get_cluster_by_offset(fatTable, fileToWriteTo.start_cluster, allocatedSize, clusterSize);
	}
	else {
		currCluster = get_cluster_by_offset(fatTable, fileToWriteTo.start_cluster, offset, clusterSize);
	}

	// zapis fill byty
	while (writtenBytes < fillBytes && !isError) {
		// todo: tady by sla optimalizace zapisem po vice clusterech najednou
		isError = write_cluster_range(diskNumber, bootRecord, currCluster, 1, &(clusterBuffer[0]));
		writtenBytes += clusterSize;
		currCluster = fatTable[currCluster];
	}

	if (isError) {
		return isError;
	}

	// zapis data
	// musi se spravne zapsat od offsetu
	writtenBytes = 0;

	// nastavi bytesToWrite na velikost, ktera v 
	// poslednim clusteru  jeste zbyva
	// v pripade, ze by to bylo moc (bufferLen je mensi), 
	// nastavi se na mensi
	clusterOffset = offset % clusterSize;
	bytesToWrite = clusterSize - (offset % clusterSize);
	if (bytesToWrite > bufferLen) {
		bytesToWrite = bufferLen;
	}
	isError = read_cluster_range(diskNumber, bootRecord, currCluster, 1, &(clusterBuffer[0]), clusterSize);
	if (isError) {
		return isError;
	}

	memcpy(&(clusterBuffer[clusterOffset]), buffer, bytesToWrite);
	isError = write_cluster_range(diskNumber, bootRecord, currCluster, 1, &(clusterBuffer[0]));
	if (isError) {
		return isError;
	}
	writtenBytes += bytesToWrite;

	if (writtenBytes + clusterSize <= bufferLen) {
		// jeste zbyva dost dat k zapsani a muzeme zapisovat po celych clusterech
		bytesToWrite = clusterSize;
		while (writtenBytes <= (bufferLen - clusterSize) && !isError) {
			currCluster = fatTable[currCluster];
			isError = write_cluster_range(diskNumber, bootRecord, currCluster, 1, &(buffer[writtenBytes]));
			writtenBytes += bytesToWrite;
		}

		if (isError) {
			return isError;
		}
	}
	currCluster = fatTable[currCluster];

	// jeste musime zapsat 'ocasek' dat na posledni cluster
	if (writtenBytes != bufferLen) {
		isError = read_cluster_range(diskNumber, bootRecord, currCluster, 1, &(clusterBuffer[0]), clusterSize);
		if (isError) {
			return isError;
		}

		// prepsat zacatek clusteru zbyvajicimi daty
		bytesToWrite = bufferLen - writtenBytes;
		memcpy(&(clusterBuffer[0]), &(buffer[writtenBytes]), bytesToWrite);

		// zapsat
		isError = write_cluster_range(diskNumber, bootRecord, currCluster, 1, &(clusterBuffer[0]));
	}

	// spravne nastaveni velikosti souboru
	if (offset + bufferLen > fileToWriteTo.size) {
		fileToWriteTo.size = (uint32_t)(offset + bufferLen);
	}

	// update FAT
	return update_fat(diskNumber, bootRecord, fatTable);
}

uint16_t delete_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t * fatTable, const Directory & parentDirectory, Directory & fileToDelete)
{
	int32_t cluster = fileToDelete.start_cluster;
	int32_t prevCluster = 0;
	uint16_t isError = 0;
	std::string origFileName(fileToDelete.name);
	
	// update itemu v adresari
	memset(&fileToDelete, 0, sizeof(Directory));

	isError = update_file_in_dir(diskNumber, bootRecord, parentDirectory, origFileName, fileToDelete);
	if (isError) {
		return isError;
	}

	// prepsat FAT
	while (cluster != FAT_FILE_END) {
		prevCluster = cluster;
		cluster = fatTable[cluster];
		fatTable[prevCluster] = FAT_UNUSED;
	}

	return update_fat(diskNumber, bootRecord, fatTable);
}

uint16_t allocate_clusters(const Boot_record & bootRecord, int32_t * fatTable, const int32_t lastCluster, const size_t clusterCount)
{
	const size_t freeClusters = count_free_clusters(fatTable, bootRecord.usable_cluster_count);
	if (freeClusters < clusterCount) {
		return FsError::FULL_DISK;
	}

	if (clusterCount == 0) {
		return FsError::SUCCESS;
	}

	int32_t nextCluster = 0,
		tmpLastCluster = lastCluster;
	size_t i = 0;


	for (i = 0; i < clusterCount; i++) {
		nextCluster = get_free_cluster(fatTable, bootRecord.usable_cluster_count);
		fatTable[tmpLastCluster] = nextCluster;
		fatTable[nextCluster] = FAT_FILE_END;
		tmpLastCluster = nextCluster;
	}

	return FsError::SUCCESS;
}

uint16_t create_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, int32_t * fatTable, const Directory & parentDirectory, Directory & newFile)
{
	const size_t maxItemsInDir = max_items_in_directory(bootRecord);
	const size_t bytesPerCluster = bytes_per_cluster(bootRecord);

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
	newFile.size = 0;
	
	// buffer na adresar veliksoti 1 clusteru
	dirBuffer = new char[bytesPerCluster];
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
	fatTable[newFile.start_cluster] = newFile.isFile ? FAT_FILE_END : FAT_DIRECTORY;
	return update_fat(diskNumber, bootRecord, fatTable);
}

uint16_t update_file_in_dir(const std::uint8_t diskNumber, const Boot_record & bootRecord, const Directory & parentDirectory, const std::string originalFileName, const Directory & fileToUpdate)
{
	const std::string fName(fileToUpdate.name);
	const size_t clusterSize = bytes_per_cluster(bootRecord);
	const size_t maxItemsInDir = max_items_in_directory(bootRecord);
	Directory * dirItems;
	uint16_t isError = 0;
	char *clusterBuffer;
	size_t i = 0;

	// update itemu
	clusterBuffer = new char[bytes_per_cluster(bootRecord)];
	isError = read_cluster_range(diskNumber, bootRecord, parentDirectory.start_cluster, 1, clusterBuffer, clusterSize);
	if (isError) {
		delete[] clusterBuffer;
		return isError;
	}

	// najdi item v adresari
	dirItems = (Directory *)clusterBuffer;
	for (i = 0; i < maxItemsInDir; i++) {
		if (originalFileName.compare(dirItems[i].name) == 0) {
			break;
		}
	}

	// je item v adresari?
	if (i == maxItemsInDir) {
		delete[] clusterBuffer;
		return FsError::FILE_NOT_FOUND;
	}

	// update itemu
	memcpy(&(clusterBuffer[i * sizeof(Directory)]), &fileToUpdate, sizeof(Directory));

	isError = write_cluster_range(diskNumber, bootRecord, parentDirectory.start_cluster, 1, clusterBuffer);
	delete[] clusterBuffer;

	return isError;
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

uint16_t find_file(const std::uint8_t diskNumber, const Boot_record & bootRecord, const std::vector<std::string>& filePath, Directory & foundFile, Directory & parentDirectory, uint32_t & matchCounter)
{
	// pokud neni zadna cesta, vrat root
	if (filePath.empty()) {
		matchCounter = 0;
		foundFile.isFile = false;
		foundFile.start_cluster = ROOT_CLUSTER;
		return FsError::SUCCESS;
	}

	uint16_t opRes = 0;
	std::vector<Directory> dirItems;
	bool searchForFile = true;
	uint16_t res = FsError::FILE_NOT_FOUND;
	int currPathItem = 0;
	int i = 0; 
	matchCounter = 0;

	// nacti obsah rootu
	parentDirectory.start_cluster = ROOT_CLUSTER;
	parentDirectory.isFile = false;
	parentDirectory.name[0] = '\0';
	opRes = load_items_in_dir(diskNumber, bootRecord, parentDirectory, dirItems);
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
				copy_dir(parentDirectory, parentDirectory);
				searchForFile = false;
				res = FsError::SUCCESS;
				matchCounter++;
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
				matchCounter++;
				currPathItem++;
				copy_dir(parentDirectory, dirItems[i]);
				dirItems.clear();
				opRes = load_items_in_dir(diskNumber, bootRecord, parentDirectory, dirItems);
				if (opRes != FsError::SUCCESS) {
					searchForFile = false;
					res = opRes;
				}
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

uint16_t update_fat(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t *fat) {
	// zapiseme br, fat i jeji kopie najednou
	// pokud to bude mnoc pameti, 
	// muzeme zmenit na postupne zapisovani (treba po 1 kb)
	const size_t fatLen = bootRecord.usable_cluster_count;
	const size_t bytesToWrite = sizeof(bootRecord) + bootRecord.usable_cluster_count*bootRecord.fat_copies * fatLen;
	const uint64_t sectorCount = first_data_sector(bootRecord);
	const size_t bufferLen = sectorCount * bootRecord.bytes_per_sector;
	char *buffer = new char[bufferLen];
	size_t i = 0,
		bufferPointer = 0;
	uint16_t isError = 0;

	// prednaplnime buffer 0, aby tam nebylo nic divnyho
	memset(buffer, 0, bufferLen);

	// boot record
	memcpy(buffer, &bootRecord, sizeof(Boot_record));
	bufferPointer += sizeof(Boot_record);

	// fat and copies
	for (i = 0; i < bootRecord.fat_copies; i++) {
		bufferPointer += i * fatLen * sizeof(int32_t);
		memcpy(&(buffer[bufferPointer]), fat, fatLen * sizeof(int32_t));
	}

	isError = write_to_disk(diskNumber, 0, sectorCount, buffer);
	delete[] buffer;
	return isError;
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

void split_file_to_chunks(const int32_t* fatTable, const int32_t startCluster, std::vector<std::array<int32_t, 2>> & chunks)
{
	int32_t currCluster = startCluster,
		prevCluster = startCluster,
		chunkStart = startCluster,
		chunkSize = 1;

	while (currCluster != FAT_FILE_END && currCluster != FAT_DIRECTORY) {
		currCluster = fatTable[currCluster];
		if (currCluster - prevCluster == 1 && chunkSize != 1000 ) {
			// predchozi cluster je tesne pred soucasnym, pokracuj v chunku
			chunkSize++;
		}
		else {
			// predchozi cluster je jinde v pameti
			chunks.push_back({ {chunkStart, chunkSize} });
			chunkStart = currCluster;
			chunkSize = 1;
		}
		prevCluster = currCluster;
	}
}

int32_t get_cluster_by_offset(const int32_t * fatTable, const int32_t startCluster, const size_t offset, const size_t clusterSize)
{
	if (offset == 0) {
		return startCluster;
	}

	// kolikaty logicky cluster hledame
	size_t clusterNum = offset / clusterSize;
	int32_t cluster = startCluster;

	while (clusterNum > 0 && cluster != FAT_FILE_END) {
		cluster = fatTable[cluster];
		clusterNum--;
	}

	return cluster;
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

uint16_t read_cluster_range(const std::uint8_t diskNumber, const Boot_record & bootRecord, const int32_t cluster, const uint32_t clusterCount, char* buffer, const size_t bufferLen, const size_t readOffset)
{
	uint64_t startSector = first_data_sector(bootRecord) + cluster * bootRecord.cluster_size;
	uint64_t sectorCount = clusterCount * bootRecord.cluster_size;
	size_t bytesToRead = sectorCount * bootRecord.bytes_per_sector;
	std::vector<char> sectorBuffer(sectorCount * bootRecord.bytes_per_sector, 0);

	if (readOffset > bytesToRead) {
		return FsError::SUCCESS;
	}

	// nacti sektory obsahujici 1 cluster do sectorBufferu
	uint16_t readRes = read_from_disk(diskNumber, startSector, sectorCount, &(sectorBuffer[0]));
	if (readRes != FsError::SUCCESS) {
		// chyba pri cteni z disku
		return readRes;
	}

	// ze sectorBufferu nacti cluster do bufferu
	// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
	// nacti jen to co muzes
	if (bytesToRead < bufferLen) {
		bytesToRead = bufferLen;
	}
	memcpy(buffer, &(sectorBuffer[readOffset]), bytesToRead - readOffset);
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
