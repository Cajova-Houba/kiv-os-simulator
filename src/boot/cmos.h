#pragma once

#include <cstdint>
#include <string>

#define CMOS_CONFIG_FILENAME "boot.ini"

namespace CMOS
{
	struct DriveParameters
	{
		bool isPresent = false;
		bool isRAMDisk = true;  // buď vytvoříme RAM disk
		bool isReadOnly = true;

		std::string diskImage = "";  // anebo použijeme soubor z disku

		size_t RAMDiskSize = 0;
		size_t bytesPerSector = 512;
	};

	bool Init();

	DriveParameters GetDriveParameters(uint8_t driveIndex);
}
