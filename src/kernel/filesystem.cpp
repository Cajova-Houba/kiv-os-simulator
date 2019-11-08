#include "filesystem.h"
#include "../api/hal.h"
#include <cstring>
#include <string>

uint16_t Filesystem::InitNewFileSystem(std::uint8_t diskNumber)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;
	uint64_t bytes_per_sector = 512;
	char buffer[3072] = {0};		// 6 sektoru, to je minimum, 2888 B je potreba pro zapis struktury FAT

	// vytvori boot rec pro FAT a samotnou FAT
	init_fat(buffer);

	addressPacket.count = 6;				// zapis 6 sektoru
	addressPacket.lba_index = 0;			// zacni na sektoru 0
	addressPacket.sectors = buffer;			// data pro zapis na disk

	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Write_Sectors);		// jakou operaci nad diskem provest
	registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);					// info pro cteni dat
	registers.rdx.l = diskNumber;													// cislo disku na ktery zapisovat
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);		// syscall pro praci s diskem
	if (registers.flags.carry) {
		// chyba pri zapisu na disk
		return Filesystem::Error::DISK_OPERATION_ERROR;
	}
	return Filesystem::Error::UNKNOWN_ERROR;
}

uint16_t Filesystem::GetFilesystemDescription(std::uint8_t diskNumber, Boot_record* bootRecord)
{
	kiv_hal::TRegisters registers;
	kiv_hal::TDisk_Address_Packet addressPacket;
	// todo: velikost bufferu by byt dana podle bytes/sector
	// daneho disku tzn to musi byt nekde ulozene
	char buffer[1024] = { 0 };
	uint64_t bytes_per_sector = 512;
	// boot record ma po zarovnani 272 bytu
	uint64_t sectors_to_read = 272 / bytes_per_sector;

	addressPacket.count = 1;				// precti 1 sektor
	addressPacket.lba_index = 0;			// zacni na sektoru 0
	addressPacket.sectors = buffer;			// data prenacti do bufferu

	registers.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Read_Sectors);		// jakou operaci nad diskem provest
	registers.rdi.r = reinterpret_cast<uint64_t>(&addressPacket);					// info pro cteni dat
	registers.rdx.l = diskNumber;													// cislo disku ze ktereho cist
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, registers);		// syscall pro praci s diskem

	if (registers.flags.carry) {
		// chyba pri cteni z disku
		return Filesystem::Error::DISK_OPERATION_ERROR;
	}

	std::memcpy(bootRecord, buffer, sizeof(Boot_record));
	return is_valid_fat(bootRecord) ? Filesystem::Error::SUCCESS : Filesystem::Error::NO_FILE_SYSTEM;
}

