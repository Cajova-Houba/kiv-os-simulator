#include <cstring>
#include <array>
#include <memory>
#include <vector>
#include <fstream>

#include "disk.h"
#include "cmos.h"

class IDiskDrive
{
protected:
	size_t m_bytesPerSector;
	size_t m_diskSize;

	void setStatus(kiv_hal::TRegisters & context, const kiv_hal::NDisk_Status status)
	{
		if (status == kiv_hal::NDisk_Status::No_Error)
		{
			context.flags.carry = 0;
		}
		else
		{
			context.flags.carry = 1;
			context.rax.x = static_cast<uint16_t>(status);
		}
	}

	// vrátí true, pokud by čtení/zápis nezpůsobilo přístup za velikost disku
	// v takovém případě vrací false a nastaví chybu
	bool checkDAP(kiv_hal::TRegisters & context)
	{
		kiv_hal::TDisk_Address_Packet *pDAP = reinterpret_cast<kiv_hal::TDisk_Address_Packet*>(context.rdi.r);

		if (m_bytesPerSector * (pDAP->lba_index + pDAP->count) >= m_diskSize)
		{
			// nemůžeme dovolit, výsledkem by byl přístup za velikost disku
			setStatus(context, kiv_hal::NDisk_Status::Sector_Not_Found);
			return false;
		}

		return true;
	}

public:
	IDiskDrive(const CMOS::DriveParameters & params)
	: m_bytesPerSector(params.bytesPerSector),
	  m_diskSize(0)
	{
	}

	void getDriveParameters(kiv_hal::TRegisters & context)
	{
		const size_t MB = 1024 * 1024;  // 1 MiB

		kiv_hal::TDrive_Parameters *pParams = reinterpret_cast<kiv_hal::TDrive_Parameters*>(context.rdi.r);
		bool isAssistedTranslation = true;

		if (m_diskSize < 504 * MB)
		{
			pParams->heads = 16;
		}
		else if (m_diskSize < 1008 * MB)
		{
			pParams->heads = 32;
		}
		else if (m_diskSize < 2016 * MB)
		{
			pParams->heads = 64;
		}
		else if (m_diskSize < 4032 * MB)
		{
			pParams->heads = 128;
		}
		else if (m_diskSize < 8032 * MB)
		{
			pParams->heads = 255;
		}
		else
		{
			// pro tahle čísla nemáme standardní přepočet, takže nastavíme vše na max
			pParams->sectors_per_track = 0xFFFFFFFF;
			pParams->heads             = 0xFFFFFFFF;
			pParams->cylinders         = 0xFFFFFFFF;

			isAssistedTranslation = false;
		}

		if (isAssistedTranslation)
		{
			pParams->sectors_per_track = 63;
			const size_t sum = pParams->sectors_per_track * pParams->heads * m_bytesPerSector;
			pParams->cylinders = static_cast<uint32_t>(m_diskSize / sum);
		}

		pParams->bytes_per_sector = static_cast<uint16_t>(m_bytesPerSector);
		pParams->absolute_number_of_sectors = m_diskSize / m_bytesPerSector;

		setStatus(context, kiv_hal::NDisk_Status::No_Error);
	}

	virtual void readSectors(kiv_hal::TRegisters & context) = 0;
	virtual void writeSectors(kiv_hal::TRegisters & context) = 0;
	
};

class CDiskImage : public IDiskDrive
{
protected:
	std::fstream m_diskImage;

public:
	CDiskImage(const CMOS::DriveParameters & params)
	: IDiskDrive(params)
	{
		auto open_mode = std::ios::binary | std::ios::in;
		if (!params.isReadOnly)
		{
			open_mode |= std::ios::out;
		}

		m_diskImage.open(params.diskImage, open_mode);

		m_diskImage.seekg(0, std::ios::end);
		m_diskSize = m_diskImage.tellg();
	}

	void readSectors(kiv_hal::TRegisters & context) override
	{
		if (!checkDAP(context))
		{
			return;
		}

		kiv_hal::TDisk_Address_Packet *pDAP = reinterpret_cast<kiv_hal::TDisk_Address_Packet*>(context.rdi.r);

		const size_t bytesToRead = pDAP->count * m_bytesPerSector;

		m_diskImage.seekg(m_bytesPerSector * pDAP->lba_index, std::ios::beg);
		m_diskImage.read(static_cast<char*>(pDAP->sectors), bytesToRead);

		if (m_diskImage.gcount() == bytesToRead)
		{
			setStatus(context, kiv_hal::NDisk_Status::No_Error);
		}
		else
		{
			setStatus(context, kiv_hal::NDisk_Status::Address_Mark_Not_Found_Or_Bad_Sector);
		}
	}

	void writeSectors(kiv_hal::TRegisters & context) override
	{
		if (!checkDAP(context))
		{
			return;
		}

		kiv_hal::TDisk_Address_Packet *pDAP = reinterpret_cast<kiv_hal::TDisk_Address_Packet*>(context.rdi.r);

		const size_t bytesToWrite = pDAP->count * m_bytesPerSector;

		m_diskImage.seekg(m_bytesPerSector * pDAP->lba_index, std::ios::beg);
		const auto before = m_diskImage.tellp();
		m_diskImage.write(static_cast<char*>(pDAP->sectors), bytesToWrite);
		const auto bytesWritten = m_diskImage.tellp() - before;

		if (bytesWritten == bytesToWrite)
		{
			setStatus(context, kiv_hal::NDisk_Status::No_Error);
		}
		else
		{
			setStatus(context, kiv_hal::NDisk_Status::Fixed_Disk_Write_Fault_On_Selected_Drive);
		}
	}
};

class CRAMDisk : public IDiskDrive
{
protected:
	std::vector<char> m_buffer;

public:
	CRAMDisk(const CMOS::DriveParameters & params)
	: IDiskDrive(params)
	{
		m_diskSize = params.RAMDiskSize;
		m_buffer.resize(m_diskSize);
	}

	void readSectors(kiv_hal::TRegisters & context) override
	{
		if (!checkDAP(context))
		{
			return;
		}

		kiv_hal::TDisk_Address_Packet *pDAP = reinterpret_cast<kiv_hal::TDisk_Address_Packet*>(context.rdi.r);

		const size_t pos = pDAP->lba_index * m_bytesPerSector;
		const size_t length = pDAP->count * m_bytesPerSector;

		std::memcpy(pDAP->sectors, m_buffer.data() + pos, length);

		setStatus(context, kiv_hal::NDisk_Status::No_Error);
	}

	void writeSectors(kiv_hal::TRegisters & context) override
	{
		if (!checkDAP(context))
		{
			return;
		}

		kiv_hal::TDisk_Address_Packet *pDAP = reinterpret_cast<kiv_hal::TDisk_Address_Packet*>(context.rdi.r);

		const size_t pos = pDAP->lba_index * m_bytesPerSector;
		const size_t length = pDAP->count * m_bytesPerSector;

		std::memcpy(m_buffer.data() + pos, pDAP->sectors, length);

		setStatus(context, kiv_hal::NDisk_Status::No_Error);
	}
};

static std::array<std::unique_ptr<IDiskDrive>, 256> g_diskDrives;

void __stdcall Disk::InterruptHandler(kiv_hal::TRegisters & context)
{
	const uint8_t diskIndex = context.rdx.l;

	if (!g_diskDrives[diskIndex])
	{
		const CMOS::DriveParameters params = CMOS::GetDriveParameters(diskIndex);

		if (params.isPresent)
		{
			if (params.isRAMDisk)
			{
				g_diskDrives[diskIndex] = std::make_unique<CRAMDisk>(params);
			}
			else
			{
				g_diskDrives[diskIndex] = std::make_unique<CDiskImage>(params);
			}
		}
	}

	if (g_diskDrives[diskIndex])
	{
		switch (static_cast<kiv_hal::NDisk_IO>(context.rax.h))
		{
			case kiv_hal::NDisk_IO::Read_Sectors:
			{
				g_diskDrives[diskIndex]->readSectors(context);
				break;
			}
			case kiv_hal::NDisk_IO::Write_Sectors:
			{
				g_diskDrives[diskIndex]->writeSectors(context);
				break;
			}
			case kiv_hal::NDisk_IO::Drive_Parameters:
			{
				g_diskDrives[diskIndex]->getDriveParameters(context);
				break;
			}
			default:
			{
				context.flags.carry = 1;
				context.rax.x = static_cast<uint16_t>(kiv_hal::NDisk_Status::Bad_Command);
				break;
			}
		}
	}
	else
	{
		context.flags.carry = 1;
		context.rax.x = static_cast<uint16_t>(kiv_hal::NDisk_Status::Drive_Not_Ready);
	}
}
