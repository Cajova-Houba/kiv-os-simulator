#pragma once

#include "cmos.h"
#include "../api/hal.h"

#include <fstream>
#include <vector>

class CDisk_Drive {
protected:
	size_t mBytes_Per_Sector;
	size_t mDisk_Size;
	void Set_Status(kiv_hal::TRegisters &context, const kiv_hal::NDisk_Status status);
		bool Check_DAP(kiv_hal::TRegisters &context);	
		//vrati true, pokud by cteni/zapis nezpusobilo pristup za velikost disku
		//v takovem pripade vraci false a nastavi chybu
public:
	CDisk_Drive(const TCMOS_Drive_Parameters &cmos_parameters);
	void Drive_Parameters(kiv_hal::TRegisters &context);

	virtual void Read_Sectors(kiv_hal::TRegisters &context) = 0;
	virtual void Write_Sectors(kiv_hal::TRegisters &context) = 0;	
	
};

class CDisk_Image : public CDisk_Drive {
protected:
	std::fstream mDisk_Image;
public:
	CDisk_Image(const TCMOS_Drive_Parameters &cmos_parameters);
	
	/**
	 * Precte sektory z disku.
	 *
	 * context.rdi.r by mìl obsahovat pointer na kiv_hal::TDisk_Address_Packet
	 * Vysledek operace uložen fo context.flags.carry
	 *    - kiv_hal::NDisk_Status::No_Error: Pokud je vse ok.
	 *    - kiv_hal::NDisk_Status::Sector_Not_Found: Pokud nebyl pozadovany sektor nalezen (napr. presazena velikost disku).
	 *    - kiv_hal::NDisk_Status::Address_Mark_Not_Found_Or_Bad_Sector: Pokud selhalho cteni.
	 */
	virtual void Read_Sectors(kiv_hal::TRegisters &context )final;

	/**
	 * Zapise sektory na disk.
	 *
	 * context.rdi.r by mìl obsahovat pointer na kiv_hal::TDisk_Address_Packet
	 * Vysledek operace uložen fo context.flags.carry
	 *    - kiv_hal::NDisk_Status::No_Error: Pokud je vse ok
	 *    - kiv_hal::NDisk_Status::Sector_Not_Found: Pokud nebyl pozadovany sektor nalezen (napr. presazena velikost disku).
	 *    - kiv_hal::NDisk_Status::Fixed_Disk_Write_Fault_On_Selected_Drive: Pokud zapis selahl.
	 */
	virtual void Write_Sectors(kiv_hal::TRegisters &context) final;
};


class CRAM_Disk : public CDisk_Drive {
protected:
	std::vector<char> mDisk_Image;
public:
	CRAM_Disk(const TCMOS_Drive_Parameters &cmos_parameters);

	/**
	 * Precte sektory z disku.
	 *
	 * context.rdi.r by mìl obsahovat pointer na kiv_hal::TDisk_Address_Packet
	 * Vysledek operace uložen fo context.flags.carry
	 *    - kiv_hal::NDisk_Status::No_Error: Pokud je vse ok.
	 *    - kiv_hal::NDisk_Status::Sector_Not_Found: Pokud nebyl pozadovany sektor nalezen (napr. presazena velikost disku).
	 */
	virtual void Read_Sectors(kiv_hal::TRegisters &context) final;

	/**
	 * Zapise sektory na disk.
	 *
	 * context.rdi.r by mìl obsahovat pointer na kiv_hal::TDisk_Address_Packet
	 * Vysledek operace uložen fo context.flags.carry
	 *    - kiv_hal::NDisk_Status::No_Error: Pokud je vse ok
	 *    - kiv_hal::NDisk_Status::Sector_Not_Found: Pokud nebyl pozadovany sektor nalezen (napr. presazena velikost disku).
	 */
	virtual void Write_Sectors(kiv_hal::TRegisters &context) final;
};

/*
	Obsluha preruseni od disku.
*/
void __stdcall Disk_Handler(kiv_hal::TRegisters &context);