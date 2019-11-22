// TODO: tento soubor odstranit
#pragma once
#include <cstdint>
#include "status.h"

enum FsError : uint16_t {
	SUCCESS = 0,
	NO_FILE_SYSTEM,
	DISK_OPERATION_ERROR,
	NOT_A_DIR,
	NOT_A_FILE,
	FILE_NOT_FOUND,
	FULL_DISK,
	FULL_DIR,
	INCOMPATIBLE_DISK,			// pokud se pokousime initializovat FS na divnem disku (napr velikost sektoru 1b)
	FILE_ALREADY_EXISTS,		// pokousime se vytvorit soubor ktery uz existuje
	FILE_NAME_TOO_LONG,

	UNKNOWN_ERROR = 0xFFFF
};


static EStatus FsErrorToStatus(uint16_t err) {
	switch (err)
	{
	case SUCCESS:
		return EStatus::SUCCESS;

	case NOT_A_DIR:
	case NOT_A_FILE:
	case FILE_ALREADY_EXISTS:
	case FILE_NAME_TOO_LONG:
		return EStatus::INVALID_ARGUMENT;

	case FILE_NOT_FOUND:
		return EStatus::FILE_NOT_FOUND;

	case FULL_DISK:
	case FULL_DIR:
		return EStatus::NOT_ENOUGH_DISK_SPACE;

	case NO_FILE_SYSTEM:
	case DISK_OPERATION_ERROR:
	case INCOMPATIBLE_DISK:
		return EStatus::IO_ERROR;

	case UNKNOWN_ERROR:
	default:
		return EStatus::UNKNOWN_ERROR;
		break;
	}
}