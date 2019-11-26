#pragma once

#include "file_system.h"

class ProcFS : public IFileSystem
{
public:
	ProcFS() = default;

	EStatus query(const Path & path, FileInfo *pInfo) override;
	EStatus read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead) override;
	EStatus readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead) override;

	EStatus write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten) override
	{
		return EStatus::PERMISSION_DENIED;
	}

	EStatus create(const Path & path, const FileInfo & info) override
	{
		return EStatus::PERMISSION_DENIED;
	}

	EStatus resize(const Path & path, uint64_t size) override
	{
		return EStatus::PERMISSION_DENIED;
	}

	EStatus remove(const Path & path) override
	{
		return EStatus::PERMISSION_DENIED;
	}
};
