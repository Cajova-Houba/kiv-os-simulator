#pragma once

#include "types.h"

enum struct EStatus : uint16_t  // kiv_os::NOS_Error
{
	SUCCESS = 0,
	INVALID_ARGUMENT,
	FILE_NOT_FOUND,
	DIRECTORY_NOT_EMPTY,
	NOT_ENOUGH_DISK_SPACE,
	OUT_OF_MEMORY,
	PERMISSION_DENIED,
	IO_ERROR,

	UNKNOWN_ERROR = 0xFFFF
};
