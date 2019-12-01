#include "fat.h"
#include "util.h"

#define VOLUME_DESCRIPTION "KIV/OS volume."
#define SIGNATURE          "kiv-os"

class Chunk
{
	int32_t m_start;
	int32_t m_size;

public:
	Chunk(int32_t start, int32_t size)
	: m_start(start),
	  m_size(size)
	{
	}

	int32_t getStart() const
	{
		return m_start;
	}

	int32_t getSize() const
	{
		return m_size;
	}
};

namespace
{
	/**
	 * @brief Vrati cislo prvniho datoveho sektoru na disku (ve FAT cluster 0).
	 */
	inline uint64_t FirstDataSector(const FAT::BootRecord & bootRecord)
	{
		const uint64_t sector = ALIGNED_BOOT_REC_SIZE
		  + sizeof (int32_t) * bootRecord.usable_cluster_count * bootRecord.fat_copies;

		return Util::DivCeil(sector, bootRecord.bytes_per_sector);
	}

	/**
	 * Returns the first unused cluster found or NO_CLUSTER.
	 */
	inline int32_t GetFreeCluster(const int32_t *fat, int32_t fatSize)
	{
		int32_t result = NO_CLUSTER;

		for (int32_t i = 0; i < fatSize; i++)
		{
			if (fat[i] == FAT_UNUSED)
			{
				result = i;
				break;
			}
		}

		return result;
	}

	/**
	 * @brief Prevede logicky cluster (poradove cislo) souboru na realny.
	 * Pokud je logCluster vetsi nez relany pocet souboru clusteru, bude vracen posledni.
	 */
	inline int32_t LogClusterToReal(const int32_t *fat, int32_t startCluster, int32_t logCluster)
	{
		int32_t realCluster = startCluster;

		for (int32_t i = 0; i < logCluster && fat[realCluster] != FAT_FILE_END; i++)
		{
			realCluster = fat[realCluster];
		}

		return realCluster;
	}

	inline int32_t FindLastFileCluster(const int32_t *fat, int32_t startCluster)
	{
		int32_t lastCluster = startCluster;

		while (fat[lastCluster] != FAT_FILE_END)
		{
			lastCluster = fat[lastCluster];
		}

		return lastCluster;
	}

	inline size_t BytesPerCluster(const FAT::BootRecord & bootRecord)
	{
		return bootRecord.bytes_per_sector * bootRecord.cluster_size;
	}

	inline size_t CountFileClusters(const int32_t *fat, int32_t startCluster)
	{
		size_t count = 1;

		for (int32_t cluster = startCluster; fat[cluster] != FAT_FILE_END; cluster = fat[cluster])
		{
			count++;
		}

		return count;
	}

	/**
	 * @brief Vrati pocet volnych clusteru ve FAT.
	 */
	inline size_t CountFreeClusters(const int32_t *fat, size_t fatLength)
	{
		size_t count = 0;

		for (size_t i = 0; i < fatLength; i++)
		{
			if (fat[i] == FAT_UNUSED)
			{
				count++;
			}
		}

		return count;
	}

	inline size_t MaxItemsInDir(size_t dirSizeBytes)
	{
		return dirSizeBytes / sizeof (FAT::Directory);
	}

	/**
	 * @biref Alokuje zadany pocet clusteru ve FAT od zadaneho poledniho clusteru.
	 * Tato funkce na nic nezapisuje, pouze modifikuje zadanou FAT.
	 */
	inline EStatus AllocateClusters(const FAT::BootRecord & bootRecord, int32_t *fatTable, int32_t lastCluster, size_t count)
	{
		const size_t freeClusters = CountFreeClusters(fatTable, bootRecord.usable_cluster_count);

		if (freeClusters < count)
		{
			return EStatus::NOT_ENOUGH_DISK_SPACE;
		}

		if (count == 0)
		{
			return EStatus::SUCCESS;
		}

		int32_t nextCluster = 0;

		for (size_t i = 0; i < count; i++)
		{
			nextCluster = GetFreeCluster(fatTable, bootRecord.usable_cluster_count);
			fatTable[lastCluster] = nextCluster;
			fatTable[nextCluster] = FAT_FILE_END;
			lastCluster = nextCluster;
		}

		return EStatus::SUCCESS;
	}

	/**
	 * @brief Rozdeli dany soubor na souvisle useky clusteru (startCluster + pocet souvislych clusteru).
	 * Funguje i pro adresare. Max velikost chunku je 1000.
	 */
	inline void SplitFileToChunks(const int32_t *fatTable, int32_t startCluster, std::vector<Chunk> & chunks)
	{
		int32_t currentCluster = startCluster;
		int32_t previousCluster = startCluster;
		int32_t chunkStart = startCluster;
		int32_t chunkSize = 1;

		while (currentCluster != FAT_FILE_END)
		{
			currentCluster = fatTable[currentCluster];

			if ((currentCluster - previousCluster) == 1 && chunkSize != 1000)
			{
				// predchozi cluster je tesne pred soucasnym, pokracuj v chunku
				chunkSize++;
			}
			else
			{
				// predchozi cluster je jinde v pameti
				chunks.emplace_back(chunkStart, chunkSize);
				chunkStart = currentCluster;
				chunkSize = 1;
			}

			previousCluster = currentCluster;
		}
	}

	/**
	 * @brief Vrati cislo clusteru ve kterem se nachazi dany offset.
	 */
	inline int32_t GetClusterByOffset(const int32_t *fatTable, int32_t startCluster, size_t offset, size_t clusterSize)
	{
		if (offset == 0)
		{
			return startCluster;
		}

		// kolikaty logicky cluster hledame
		size_t clusterNumber = offset / clusterSize;
		int32_t cluster = startCluster;

		while (clusterNumber > 0 && cluster != FAT_FILE_END)
		{
			cluster = fatTable[cluster];
			clusterNumber--;
		}

		return cluster;
	}

	inline FAT::Directory *GetDirectoryItem(const std::string & name, std::vector<FAT::Directory> & items)
	{
		for (FAT::Directory & item : items)
		{
			if (name.compare(item.name) == 0)
			{
				return &item;
			}
		}

		return nullptr;
	}

	/**
	 * @brief Zavola sluzbu BIOSu pro cteni z disku a data ulozi do buffer.
	 * @return
	 *  EStatus::SUCCESS Pokud nacteni probehne v poradku.
	 */
	inline EStatus ReadFromDisk(uint8_t diskNumber, uint64_t startSector, uint64_t sectorCount, char *buffer)
	{
		kiv_hal::TRegisters registers;
		kiv_hal::TDisk_Address_Packet addressPacket;

		addressPacket.lba_index = startSector;  // zacni na sektoru
		addressPacket.count = sectorCount;      // precti x sektoru
		addressPacket.sectors = buffer;         // data prenacti do bufferu

		registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Read_Sectors);  // jakou operaci nad diskem provest
		registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);             // info pro cteni dat
		registers.rdx.l = diskNumber;                                             // cislo disku ze ktereho cist

		kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);

		if (registers.flags.carry)
		{
			// chyba pri cteni z disku
			return EStatus::IO_ERROR;
		}

		return EStatus::SUCCESS;
	}

	/**
	 * @brief Zavola sluzbu BIOSu pro zapis dat z bufferu na disk.
	 * @return
	 *  EStatus::SUCCESS Pokud zapis probehne v poradku.
	 */
	inline EStatus WriteToDisk(uint8_t diskNumber, uint64_t startSector, uint64_t sectorCount, const char *buffer)
	{
		kiv_hal::TRegisters registers;
		kiv_hal::TDisk_Address_Packet addressPacket;

		addressPacket.lba_index = startSector;              // zacni na sektoru
		addressPacket.count = sectorCount;                  // zapis x sektoru
		addressPacket.sectors = const_cast<char*>(buffer);  // data pro zapis

		registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Write_Sectors);  // jakou operaci nad diskem provest
		registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);              // info pro cteni dat
		registers.rdx.l = diskNumber;                                              // cislo disku ze ktereho cist

		kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);

		if (registers.flags.carry)
		{
			kiv_hal::NDisk_Status error = static_cast<kiv_hal::NDisk_Status>(registers.rax.x);

			if (error == kiv_hal::NDisk_Status::Fixed_Disk_Write_Fault_On_Selected_Drive)
			{
				// disk je pouze pro cteni
				return EStatus::PERMISSION_DENIED;
			}
			else
			{
				// chyba pri cteni z disku
				return EStatus::IO_ERROR;
			}
		}

		return EStatus::SUCCESS;
	}

	/**
	 * @brief Precte zadany souvisly rozsah clusteru do bufferu.
	 *
	 * @param bufferSize Maximalni pocet bytu ktery nacist do bufferu. Relevantni jen pokud je tento pocet mensi nez pocet
	 * clusteru * velikost clusteru.
	 */
	inline EStatus ReadClusterRange(uint8_t diskNumber, const FAT::BootRecord & bootRecord, int32_t cluster,
	                                uint32_t clusterCount, char *buffer, size_t bufferSize, size_t offset)
	{
		uint64_t startSector = FirstDataSector(bootRecord) + cluster * bootRecord.cluster_size;
		uint64_t sectorCount = clusterCount * bootRecord.cluster_size;
		size_t bytesToRead = static_cast<size_t>(sectorCount) * bootRecord.bytes_per_sector;

		if (offset > bytesToRead)
		{
			return EStatus::SUCCESS;
		}

		std::vector<char> sectorBuffer;
		sectorBuffer.resize(static_cast<size_t>(sectorCount) * bootRecord.bytes_per_sector, 0);

		// nacti sektory obsahujici 1 cluster do sectorBufferu
		EStatus status = ReadFromDisk(diskNumber, startSector, sectorCount, sectorBuffer.data());
		if (status != EStatus::SUCCESS)
		{
			// chyba pri cteni z disku
			return status;
		}

		// ze sectorBufferu nacti cluster do bufferu
		// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
		// nacti jen to co muzes
		if (bytesToRead < bufferSize)
		{
			bytesToRead = bufferSize;
		}

		std::memcpy(buffer, sectorBuffer.data() + offset, bytesToRead - offset);

		return EStatus::SUCCESS;
	}

	/**
	 * @brief Zapise dany pocet clusteru z bufferu na disk.
	 * Predpoklada se, ze velikost buffer je >= poctu zapisovanych bytu.
	 */
	inline EStatus WriteClusterRange(uint8_t diskNumber, const FAT::BootRecord & bootRecord,
	                                 int32_t cluster, uint32_t clusterCount, const char *buffer)
	{
		uint64_t startSector = FirstDataSector(bootRecord) + cluster * bootRecord.cluster_size;
		uint64_t sectorCount = clusterCount * bootRecord.cluster_size;

		return WriteToDisk(diskNumber, startSector, sectorCount, buffer);
	}

	/**
	 * @brief Ulozi bootRecord, FAT a pripadne kopie.
	 */
	inline EStatus UpdateFAT(uint8_t diskNumber, const FAT::BootRecord & bootRecord, const int32_t *fat)
	{
		// zapiseme br, fat i jeji kopie najednou
		// pokud to bude mnoc pameti,
		// muzeme zmenit na postupne zapisovani (treba po 1 kb)
		const size_t fatSize = bootRecord.usable_cluster_count;
		const uint64_t sectorCount = FirstDataSector(bootRecord);
		const size_t bufferSize = static_cast<size_t>(sectorCount) * bootRecord.bytes_per_sector;

		std::vector<char> buffer;
		buffer.resize(bufferSize, 0);

		// boot record
		std::memcpy(buffer.data(), &bootRecord, sizeof (FAT::BootRecord));
		size_t bufferPointer = sizeof (FAT::BootRecord);

		// fat and copies
		for (size_t i = 0; i < bootRecord.fat_copies; i++)
		{
			bufferPointer += i * fatSize * sizeof (int32_t);
			std::memcpy(buffer.data() + bufferPointer, fat, fatSize * sizeof (int32_t));
		}

		EStatus status = WriteToDisk(diskNumber, 0, sectorCount, buffer.data());

		return status;
	}
}

EStatus FAT::Init(uint8_t diskNumber, const kiv_hal::TDrive_Parameters & diskParams)
{
	const uint16_t bytesPerSector = diskParams.bytes_per_sector;
	const uint64_t diskSize = diskParams.absolute_number_of_sectors * bytesPerSector;

	// kolik sektoru bude zabirat jeden cluster
	size_t sectorsPerCluster = static_cast<size_t>(PREFERRED_CLUSTER_SIZE / bytesPerSector);
	if (sectorsPerCluster == 0)
	{
		sectorsPerCluster = 1;
	}

	if (sectorsPerCluster > UINT16_MAX)
	{
		sectorsPerCluster = UINT16_MAX;
	}

	size_t clusterSize = sectorsPerCluster * bytesPerSector;

	// kolik mista z disku muzeme vyuzit
	// vzorecek: fatSize = sizeof(int32_t) * (HDD - bootSec - fatSize) / clusterSize
	uint64_t fatSize = (diskSize - ALIGNED_BOOT_REC_SIZE) / (clusterSize + sizeof (int32_t) * DEF_FAT_COPIES);
	if (fatSize > UINT32_MAX)
	{
		fatSize = UINT32_MAX;
	}

	// kontrola jestli vse vychazi
	size_t baseSectors = Util::DivCeil(ALIGNED_BOOT_REC_SIZE + fatSize * DEF_FAT_COPIES * sizeof (int32_t), bytesPerSector);
	size_t dataSectors = static_cast<size_t>(fatSize) * sectorsPerCluster;

	// tohle je trochu naivni pristup, ale teoreticky by to mel byt vyjimecny pripad, tak to snad nebude vadit
	while ((baseSectors + dataSectors) > diskParams.absolute_number_of_sectors)
	{
		if (fatSize == 0)
		{
			return EStatus::INVALID_ARGUMENT;
		}

		fatSize--;

		baseSectors = Util::DivCeil(ALIGNED_BOOT_REC_SIZE + fatSize * DEF_FAT_COPIES * sizeof (int32_t), bytesPerSector);
		dataSectors -= sectorsPerCluster;
	}

	// inicializace FAT
	std::vector<int32_t> fatTable;
	fatTable.resize(static_cast<size_t>(fatSize), FAT_UNUSED);

	fatTable[0] = FAT_FILE_END;

	// vytvoreni bufferu zapsani boot sectoru a FAT
	std::vector<char> buffer;
	buffer.resize(baseSectors * bytesPerSector, 0);

	BootRecord bootRecord;

	// FAT8
	// 1 cluster = 1 sector
	bootRecord.fat_type = 8;
	bootRecord.fat_copies = DEF_FAT_COPIES;
	bootRecord.usable_cluster_count = static_cast<uint32_t>(fatSize);
	bootRecord.cluster_size = static_cast<uint16_t>(sectorsPerCluster);
	bootRecord.bytes_per_sector = bytesPerSector;

	// prenaplneni popis 0, pak nakopirovani retezce
	std::memset(bootRecord.volume_descriptor, 0, DESCRIPTION_LEN);
	std::memcpy(bootRecord.volume_descriptor, VOLUME_DESCRIPTION, sizeof (VOLUME_DESCRIPTION));

	// to same pro podpis
	std::memset(bootRecord.signature, 0, SIGNATURE_LEN);
	std::memcpy(bootRecord.signature, SIGNATURE, sizeof (SIGNATURE));

	// boot sector
	std::memcpy(buffer.data(), &bootRecord, sizeof (BootRecord));

	// fat a kopie
	for (size_t i = 0; i < bootRecord.fat_copies; i++)
	{
		const size_t offset = ALIGNED_BOOT_REC_SIZE + i * static_cast<size_t>(fatSize) * sizeof (int32_t);
		std::memcpy(buffer.data() + offset, fatTable.data(), static_cast<size_t>(fatSize) * sizeof (int32_t));
	}

	// zapis metadat FAT
	EStatus status = WriteToDisk(diskNumber, 0, baseSectors, buffer.data());
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// zapis root clusteru, jen 0
	buffer.clear();
	buffer.resize(clusterSize, 0);

	status = WriteToDisk(diskNumber, FirstDataSector(bootRecord), 1, buffer.data());
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	return EStatus::SUCCESS;
}

EStatus FAT::Load(uint8_t diskNumber, const kiv_hal::TDrive_Parameters & diskParams,
                  BootRecord & bootRecord, std::vector<int32_t> & fatTable)
{
	// kolik sektoru nacist
	const uint64_t sectorCount = Util::DivCeil(ALIGNED_BOOT_REC_SIZE, diskParams.bytes_per_sector);

	// podle sektoru vypoctena potrebna velikost bufferu
	fatTable.resize((static_cast<size_t>(sectorCount) * diskParams.bytes_per_sector) / sizeof (int32_t), 0);

	EStatus status = ReadFromDisk(diskNumber, 0, sectorCount, reinterpret_cast<char*>(fatTable.data()));
	if (status != EStatus::SUCCESS)
	{
		// chyba pri cteni z disku
		return status;
	}

	// zkopirujeme nacteny boot sektor
	bootRecord = *reinterpret_cast<BootRecord*>(fatTable.data());

	if (!bootRecord.isValid())
	{
		// disk neobsahuje validni souborovy system FAT
		return EStatus::INVALID_ARGUMENT;
	}

	// boot sektor je nacten
	// ted je jeste treba nacist zbytek FAT tabulky

	const size_t fatSizeBytes = bootRecord.usable_cluster_count * sizeof (int32_t);

	// potrebuju nacist fatSizeBytes + ALIGNED_BOOT_REC_SIZE bytu, kolik je to sectoru?
	const uint64_t fatSectorCount = Util::DivCeil(fatSizeBytes + ALIGNED_BOOT_REC_SIZE, diskParams.bytes_per_sector);

	if (fatSectorCount > sectorCount)
	{
		fatTable.resize((static_cast<size_t>(fatSectorCount) * diskParams.bytes_per_sector) / sizeof (int32_t), 0);

		status = ReadFromDisk(diskNumber, 0, fatSectorCount, reinterpret_cast<char*>(fatTable.data()));
		if (status != EStatus::SUCCESS)
		{
			// chyba pri cteni z disku
			return status;
		}
	}

	// odstranime boot sektor nacteny na zacatku
	fatTable.erase(fatTable.begin(), fatTable.begin() + (ALIGNED_BOOT_REC_SIZE / sizeof (int32_t)));

	// odstranime sektorove zarovnani na konci
	fatTable.resize(bootRecord.usable_cluster_count, 0);

	return EStatus::SUCCESS;
}

EStatus FAT::ReadDirectory(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable,
                           const Directory & directory, std::vector<Directory> & result)
{
	// kontrola, ze je vazne potreba cokoliv delat
	if (!directory.isDirectory())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	const size_t dirClusterCount = CountFileClusters(fatTable, directory.start_cluster);
	const size_t bufferSize = dirClusterCount * bootRecord.bytes_per_sector * bootRecord.cluster_size;
	const size_t dirItemCount = bufferSize / sizeof (Directory);

	std::vector<char> buffer;
	buffer.resize(bufferSize, 0);

	size_t bytesRead = 0;

	// nactu cely soubor s adresarem
	EStatus status = ReadFile(diskNumber, bootRecord, fatTable, directory, buffer.data(), buffer.size(), bytesRead, 0);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// zkopirovani itemu v adresari do dirItems
	Directory *dirItems = reinterpret_cast<Directory*>(buffer.data());

	result.reserve(dirItemCount);

	for (size_t i = 0; i < dirItemCount; i++)
	{
		// vyber neprazdne polozky
		if (dirItems[i].hasName())
		{
			result.emplace_back(dirItems[i]);
		} 
	}

	return EStatus::SUCCESS;
}

EStatus FAT::ReadFile(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable, const Directory & file,
                      char *buffer, size_t bufferSize, size_t & bytesRead, uint64_t offset)
{
	bytesRead = 0;

	// neni co cist
	if (bufferSize == 0)
	{
		return EStatus::SUCCESS;
	}

	const size_t bytesPerCluster = BytesPerCluster(bootRecord);

	// od ktere casti ktereho clusteru budeme cist
	int32_t currentCluster = GetClusterByOffset(fatTable, file.start_cluster, static_cast<size_t>(offset), bytesPerCluster);
	if (currentCluster == FAT_FILE_END)
	{
		// offset je za koncem souboru => neni co cist
		return EStatus::SUCCESS;
	}

	size_t pos = offset % bytesPerCluster;
	size_t bytesToRead = bytesPerCluster - pos;

	std::vector<Chunk> chunks;

	// rozdelit zbytek soubor na chunky
	SplitFileToChunks(fatTable, currentCluster, chunks);

	bool isBufferFull = false;

	// ted nacitej po clusterech
	for (size_t i = 0; i < chunks.size() && !isBufferFull; i++)
	{
		// pokud uz je cilovy buffer plny a cely cluster se do nej nevejde,
		// nebo jsme precetli cely soubor
		// nacti jen to co muzes
		bytesToRead = chunks[i].getSize() * bytesPerCluster - pos;

		if (bytesRead + bytesToRead > bufferSize)
		{
			bytesToRead = bufferSize - bytesRead;
			isBufferFull = true;
		}
		else if (bytesRead + bytesToRead > file.size)
		{
			bytesToRead = file.size - bytesRead;
			isBufferFull = true;
		}

		int32_t cluster = chunks[i].getStart();
		uint32_t count = static_cast<uint32_t>(chunks[i].getSize());

		// nacti chunk do bufferu
		EStatus status = ReadClusterRange(diskNumber, bootRecord, cluster, count, buffer + bytesRead, bytesToRead, pos);
		if (status != EStatus::SUCCESS)
		{
			break;
		}

		bytesRead += bytesToRead;

		// s offsetem cteme jen pri ctnei prvniho clusteru
		pos = 0;
	}

	return EStatus::SUCCESS;
}

EStatus FAT::WriteFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable, Directory & file,
                       const char *buffer, size_t bufferSize, size_t & bytesWritten, uint64_t offset)
{
	bytesWritten = 0;

	if (file.isDirectory())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	if (bufferSize == 0)
	{
		return EStatus::SUCCESS;
	}

	const int32_t lastFileCluster = FindLastFileCluster(fatTable, file.start_cluster);
	const size_t fileClusterCount = CountFileClusters(fatTable, file.start_cluster);
	const size_t clusterSize = BytesPerCluster(bootRecord);

	std::vector<char> clusterBuffer;
	clusterBuffer.resize(clusterSize, 0);

	size_t fillBytes = 0;
	size_t newBytes = 0;
	size_t bytesToWrite = 0;

	// misto ktere uz je pro soubor alokovane
	const size_t allocatedSize = fileClusterCount * clusterSize;

	if ((offset + bufferSize) > allocatedSize)
	{
		// zapisovana data presahnou velikost souboru
		if (offset > allocatedSize)
		{
			// offset je za koncem souboru
			// musime alokovat misto na 0 a na data
			fillBytes = static_cast<size_t>(offset) - allocatedSize;
			newBytes = fillBytes + bufferSize;
		}
		else
		{
			// offset je pred koncem souboru, ale data presahnou konec souboru
			// musime alokovat clustery na data
			newBytes = bufferSize - (allocatedSize - static_cast<size_t>(offset));
		}
	}

	// kolik clusteru musime alokovat pro data?
	size_t clustersToAllocate = static_cast<size_t>(Util::DivCeil(newBytes, clusterSize));

	EStatus status = AllocateClusters(bootRecord, fatTable, lastFileCluster, clustersToAllocate);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	int32_t currentCluster = 0;

	// od ktereho clsuteru budeme zapisovat
	if (fillBytes > 0)
	{
		// budeme zapisovat vyplnove byty -> zacneme na konci soucasneho souboru
		currentCluster = GetClusterByOffset(fatTable, file.start_cluster, allocatedSize, clusterSize);
	}
	else
	{
		currentCluster = GetClusterByOffset(fatTable, file.start_cluster, static_cast<size_t>(offset), clusterSize);
	}

	size_t writtenBytes = 0;

	// zapis fill byty
	while (writtenBytes < fillBytes)
	{
		// TODO: tady by sla optimalizace zapisem po vice clusterech najednou
		status = WriteClusterRange(diskNumber, bootRecord, currentCluster, 1, clusterBuffer.data());
		if (status != EStatus::SUCCESS)
		{
			return status;
		}

		writtenBytes += clusterSize;
		currentCluster = fatTable[currentCluster];
	}

	// zapis data
	// musi se spravne zapsat od offsetu
	bytesWritten += writtenBytes;
	writtenBytes = 0;

	// nastavi bytesToWrite na velikost, ktera v poslednim clusteru  jeste zbyva
	// v pripade, ze by to bylo moc (bufferSize je mensi), nastavi se na mensi
	size_t clusterOffset = offset % clusterSize;
	bytesToWrite = clusterSize - (offset % clusterSize);
	if (bytesToWrite > bufferSize)
	{
		bytesToWrite = bufferSize;
	}

	status = ReadClusterRange(diskNumber, bootRecord, currentCluster, 1, clusterBuffer.data(), clusterSize, 0);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	std::memcpy(clusterBuffer.data() + clusterOffset, buffer, bytesToWrite);

	status = WriteClusterRange(diskNumber, bootRecord, currentCluster, 1, clusterBuffer.data());
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	writtenBytes += bytesToWrite;

	if ((writtenBytes + clusterSize) <= bufferSize)
	{
		// jeste zbyva dost dat k zapsani a muzeme zapisovat po celych clusterech
		bytesToWrite = clusterSize;

		while (writtenBytes <= (bufferSize - clusterSize))
		{
			currentCluster = fatTable[currentCluster];

			status = WriteClusterRange(diskNumber, bootRecord, currentCluster, 1, buffer + writtenBytes);
			if (status != EStatus::SUCCESS)
			{
				return status;
			}

			writtenBytes += bytesToWrite;
		}
	}

	currentCluster = fatTable[currentCluster];

	// jeste musime zapsat 'ocasek' dat na posledni cluster
	if (writtenBytes != bufferSize)
	{
		status = ReadClusterRange(diskNumber, bootRecord, currentCluster, 1, clusterBuffer.data(), clusterSize, 0);
		if (status != EStatus::SUCCESS)
		{
			return status;
		}

		// prepsat zacatek clusteru zbyvajicimi daty
		bytesToWrite = bufferSize - writtenBytes;
		std::memcpy(clusterBuffer.data(), buffer+ writtenBytes, bytesToWrite);

		// zapsat
		status = WriteClusterRange(diskNumber, bootRecord, currentCluster, 1, clusterBuffer.data());
		if (status != EStatus::SUCCESS)
		{
			return status;
		}
	}

	// spravne nastaveni velikosti souboru
	if ((offset + bufferSize) > file.size)
	{
		file.size = static_cast<uint32_t>(offset + bufferSize);
	}

	bytesWritten += writtenBytes;

	// update FAT
	return UpdateFAT(diskNumber, bootRecord, fatTable);
}

EStatus FAT::DeleteFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable,
                        const Directory & parentDirectory, Directory & file)
{
	std::string origFileName = file.name;
	int32_t cluster = file.start_cluster;
	int32_t prevCluster = 0;
	
	// update itemu v adresari
	file.clear();

	EStatus status = UpdateFile(diskNumber, bootRecord, fatTable, parentDirectory, origFileName.c_str(), file);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// prepsat FAT
	while (cluster != FAT_FILE_END)
	{
		prevCluster = cluster;
		cluster = fatTable[cluster];
		fatTable[prevCluster] = FAT_UNUSED;
	}

	return UpdateFAT(diskNumber, bootRecord, fatTable);
}

EStatus FAT::CreateFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable,
                        const Directory & parentDirectory, Directory & newFile)
{
	const int32_t dirLastCluster = FindLastFileCluster(fatTable, parentDirectory.start_cluster);

	// zjistit jestli je misto ve FAT
	int32_t nextFreeCluster = GetFreeCluster(fatTable, bootRecord.usable_cluster_count);
	if (nextFreeCluster == NO_CLUSTER)
	{
		return EStatus::NOT_ENOUGH_DISK_SPACE;
	}

	// vytvorit soubor
	newFile.start_cluster = nextFreeCluster;
	newFile.size = 0;

	fatTable[newFile.start_cluster] = FAT_FILE_END;

	std::vector<char> dirClusterBuffer;
	dirClusterBuffer.resize(BytesPerCluster(bootRecord), 0);

	// zavolat update s puvodnim jmenem '\0'
	// pokud bude file not found => rodicovsky adresar je plny
	EStatus status = UpdateFile(diskNumber, bootRecord, fatTable, parentDirectory, "", newFile);
	if (status == EStatus::FILE_NOT_FOUND)
	{
		// adresar je plny => alokovat novy cluster
		nextFreeCluster = GetFreeCluster(fatTable, bootRecord.usable_cluster_count);
		if (nextFreeCluster == NO_CLUSTER)
		{
			return EStatus::NOT_ENOUGH_DISK_SPACE;
		}

		fatTable[dirLastCluster] = nextFreeCluster;
		fatTable[nextFreeCluster] = FAT_FILE_END;

		// prepsat novy dir cluster 0
		status = WriteClusterRange(diskNumber, bootRecord, nextFreeCluster, 1, dirClusterBuffer.data());
		if (status != EStatus::SUCCESS)
		{
			return status;
		}

		status = UpdateFile(diskNumber, bootRecord, fatTable, parentDirectory, "", newFile);
		if (status != EStatus::SUCCESS)
		{
			// zase nejaka chyba, tady uz je neocekavana
			return status;
		}
	}
	else if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// prepsat cluster noveho souboru 0
	status = WriteClusterRange(diskNumber, bootRecord, newFile.start_cluster, 1, dirClusterBuffer.data());
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	// zapis update FAT
	return UpdateFAT(diskNumber, bootRecord, fatTable);
}

EStatus FAT::UpdateFile(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable,
                        const Directory & parentDirectory, const char *originalFileName, const Directory & file)
{
	const size_t clusterSize = BytesPerCluster(bootRecord);
	const size_t dirClusters = CountFileClusters(fatTable, parentDirectory.start_cluster);
	const size_t maxItemsInDir = (clusterSize * dirClusters) / sizeof (Directory);

	std::vector<char> buffer;
	buffer.resize(clusterSize * dirClusters, 0);

	size_t read = 0;

	// nacti cely adresar
	EStatus status = ReadFile(diskNumber, bootRecord, fatTable, parentDirectory, buffer.data(), buffer.size(), read, 0);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	size_t i = 0;
	Directory *dirItems = reinterpret_cast<Directory*>(buffer.data());

	// najdi item v adresari
	for (; i < maxItemsInDir; i++)
	{
		if (std::strcmp(originalFileName, dirItems[i].name) == 0)
		{
			break;
		}
	}

	// je item v adresari?
	if (i >= maxItemsInDir)
	{
		return EStatus::FILE_NOT_FOUND;
	}

	const size_t offset = i * sizeof (Directory);

	// update itemu
	std::memcpy(buffer.data() + offset, &file, sizeof (Directory));

	// podle offsetu pozname ktery cluster zapsat
	// directory k update je ve stejném clusteru
	const int32_t c1 = static_cast<int32_t>(offset / clusterSize);
	const int32_t c2 = static_cast<int32_t>((offset + sizeof (Directory) - 1) / clusterSize);

	const int32_t realC1 = LogClusterToReal(fatTable, parentDirectory.start_cluster, c1);
	const int32_t realC2 = LogClusterToReal(fatTable, parentDirectory.start_cluster, c2);

	status = WriteClusterRange(diskNumber, bootRecord, realC1, 1, buffer.data() + (c1 * clusterSize));
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	if (c1 != c2)
	{
		// dir je ve dvou ruznych clusterech -> musime prepsat oba
		status = WriteClusterRange(diskNumber, bootRecord, realC2, 1, buffer.data() + (c2 * clusterSize));
		if (status != EStatus::SUCCESS)
		{
			return status;
		}
	}

	return EStatus::SUCCESS;
}

EStatus FAT::ResizeFile(uint8_t diskNumber, const BootRecord & bootRecord, int32_t *fatTable,
                        const Directory & parentDirectory, Directory & file, uint64_t newSize)
{
	const size_t oldSize = file.size;
	const size_t bytesPerCluster = BytesPerCluster(bootRecord);
	const size_t currentClusterCount = static_cast<size_t>(Util::DivCeil(file.size, bytesPerCluster));

	size_t newClusterCount = static_cast<size_t>(Util::DivCeil(newSize, bytesPerCluster));
	if (newClusterCount == 0)
	{
		newClusterCount = 1;  // kazdy soubor ma alespon 1 cluster
	}

	bool fatDirty = false;
	bool dirDirty = false;

	file.size = static_cast<uint32_t>(newSize);

	if (newSize > oldSize)
	{
		dirDirty = true;

		size_t written;

		// v podstate je treba zapsat newSize - oldSize 0 na konec souboru
		// coz se da prevest jako zapis 1 0 na offsetu newSize-1
		EStatus status = WriteFile(diskNumber, bootRecord, fatTable, file, "", 1, written, newSize-1);
		if (status != EStatus::SUCCESS)
		{
			return status;
		}
	}
	else if (newSize < oldSize)
	{
		dirDirty = true;

		if (newClusterCount < currentClusterCount)
		{
			// je potreba dealokovat clustery
			fatDirty = true;

			int32_t currentCluster = file.start_cluster;
			size_t clusterCounter = 1;

			while (currentCluster != FAT_FILE_END)
			{
				int32_t tmpCluster = currentCluster;
				currentCluster = fatTable[currentCluster];

				if (clusterCounter > newClusterCount)
				{
					fatTable[tmpCluster] = FAT_UNUSED;
				}

				clusterCounter++;
			}
		}
	}

	// update dir polozku
	if (dirDirty)
	{
		EStatus status = UpdateFile(diskNumber, bootRecord, fatTable, parentDirectory, file.name, file);
		if (status != EStatus::SUCCESS)
		{
			return status;
		}
	}

	// update fat
	if (fatDirty)
	{
		EStatus status = UpdateFAT(diskNumber, bootRecord, fatTable);
		if (status != EStatus::SUCCESS)
		{
			return status;
		}
	}

	return EStatus::SUCCESS;
}

EStatus FAT::FindFile(uint8_t diskNumber, const BootRecord & bootRecord, const int32_t *fatTable,
                      const std::vector<std::string> & filePath, Directory & foundFile,
                      Directory & parentDirectory, uint32_t & matchCounter)
{
	matchCounter = 0;

	// pokud neni zadna cesta, vrat root
	if (filePath.empty())
	{
		foundFile.flags = FileAttributes::DIRECTORY;
		foundFile.start_cluster = ROOT_CLUSTER;

		return EStatus::SUCCESS;
	}

	std::vector<Directory> dirItems;

	// nacti obsah rootu
	parentDirectory.start_cluster = ROOT_CLUSTER;
	parentDirectory.flags = FileAttributes::DIRECTORY;
	parentDirectory.name[0] = '\0';

	EStatus status = ReadDirectory(diskNumber, bootRecord, fatTable, parentDirectory, dirItems);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	for (size_t i = 0; i < filePath.size(); i++)
	{
		Directory *pItem = GetDirectoryItem(filePath[i], dirItems);
		if (!pItem)
		{
			// item nenalezen
			return EStatus::FILE_NOT_FOUND;
		}

		// item nalezen
		// pokud jsme na konci filePath, tak jsme nasli hledany soubor
		// pokud ne, zanoreni do dalsi urovne
		if (i == filePath.size()-1)
		{
			foundFile = *pItem;
			matchCounter++;
			break;
		}

		if (!pItem->isDirectory())
		{
			// jeste nejsme na konci cesty, ale byl nalezen file misto adresare
			return EStatus::INVALID_ARGUMENT;
		}

		// nejsme na konci cesty
		// zanoreni do prave nalezeneho adresare
		parentDirectory = *pItem;
		matchCounter++;

		dirItems.clear();

		status = ReadDirectory(diskNumber, bootRecord, fatTable, parentDirectory, dirItems);
		if (status != EStatus::SUCCESS)
		{
			return status;
		}
	}

	return EStatus::SUCCESS;
}
