#include "cmos.h"
#include "util.h"
#include "SimpleIni.h"

#define CMOS_CONFIG_DRIVE_RAM_DISK      "RAM_Disk"
#define CMOS_CONFIG_DRIVE_RAM_DISK_SIZE "RAM_Disk_Size"
#define CMOS_CONFIG_DRIVE_READ_ONLY     "Ready_Only"
#define CMOS_CONFIG_DRIVE_DISK_IMAGE    "Disk_Image"

static CSimpleIniA g_config;

bool CMOS::Init()
{
	const std::string configFilePath = Util::GetApplicationDirectory() + CMOS_CONFIG_FILENAME;

	if (g_config.LoadFile(configFilePath.c_str()) != SI_OK)
	{
		// nelze načíst soubor s konfigurací
		return false;
	}

	return true;
}

CMOS::DriveParameters CMOS::GetDriveParameters(uint8_t driveIndex)
{
	const std::string section = std::string("Drive_") + Util::NumberToHexString(driveIndex);

	DriveParameters result;
	
	if (g_config.GetSection(section.c_str()))
	{
		const long minRAMDiskSize = static_cast<long>(result.bytesPerSector);  // alespoň jeden sektor

		result.isPresent   = true;
		result.isRAMDisk   = g_config.GetBoolValue(section.c_str(), CMOS_CONFIG_DRIVE_RAM_DISK);
		result.isReadOnly  = g_config.GetBoolValue(section.c_str(), CMOS_CONFIG_DRIVE_READ_ONLY);
		result.diskImage   = g_config.GetValue(    section.c_str(), CMOS_CONFIG_DRIVE_DISK_IMAGE, "");
		result.RAMDiskSize = g_config.GetLongValue(section.c_str(), CMOS_CONFIG_DRIVE_RAM_DISK_SIZE, minRAMDiskSize);
	}

	return result;
}
