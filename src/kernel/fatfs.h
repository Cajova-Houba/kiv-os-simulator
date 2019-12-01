#pragma once

#include <mutex>

#include "../api/hal.h"  // kiv_hal::TDrive_Parameters

#include "file_system.h"

class FatFS : public IFileSystem
{
	std::mutex m_mutex;
	uint8_t m_diskNumber;
	kiv_hal::TDrive_Parameters m_diskParams;

public:
	FatFS(uint8_t diskNumber)
	: m_mutex(),
	  m_diskNumber(diskNumber),
	  m_diskParams()
	{
	}

	EStatus init(const kiv_hal::TDrive_Parameters & diskParams);

	EStatus query(const Path & path, FileInfo *pInfo) override;

	EStatus read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead) override;
	EStatus readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead) override;
	EStatus write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten) override;

	EStatus create(const Path & path, const FileInfo & info) override;
	EStatus resize(const Path & path, uint64_t size) override;
	EStatus remove(const Path & path) override;
};
