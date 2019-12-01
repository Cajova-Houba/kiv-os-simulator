#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

#include "handle.h"
#include "file.h"

namespace Util
{
	inline HandleID StringToHandleID(const char *string)
	{
		long result = std::atol(string);

		if (result < 0 || result > static_cast<HandleID>(-1))
		{
			result = 0;  // invalid handle
		}

		return static_cast<HandleID>(result);
	}

	inline HandleID StringToHandleID(const std::string & string)
	{
		return StringToHandleID(string.c_str());
	}

	inline size_t SetDirectoryEntry(DirectoryEntry & entry, uint16_t attributes, const char *name, size_t length)
	{
		entry.attributes = attributes;

		if (length >= sizeof entry.name)
		{
			length = sizeof entry.name - 1;
		}

		std::memcpy(entry.name, name, length);
		entry.name[length] = '\0';

		return length;
	}

	inline size_t SetDirectoryEntry(DirectoryEntry & entry, uint16_t attributes, const char *name)
	{
		return SetDirectoryEntry(entry, attributes, name, std::strlen(name));
	}

	inline size_t SetDirectoryEntry(DirectoryEntry & entry, uint16_t attributes, const std::string & name)
	{
		return SetDirectoryEntry(entry, attributes, name.c_str(), name.length());
	}

	inline uint64_t DivCeil(uint64_t numerator, uint64_t denominator)
	{
		return numerator / denominator + (numerator % denominator != 0);
	}
}
