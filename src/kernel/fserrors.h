#pragma once
#include <cstdint>

enum FsError : uint16_t {
	SUCCESS = 0,
	NO_FILE_SYSTEM,
	DISK_OPERATION_ERROR,
	NOT_A_DIR,
	NOT_A_FILE,
	FILE_NOT_FOUND,
	FULL_DISK,

	UNKNOWN_ERROR = 0xFFFF
};
