#pragma once

#include "file_system.h"

class ProcFS : public IFileSystem
{
public:
	ProcFS() = default;

	EStatus query(const Path & path, FileInfo *pInfo) override;
	EStatus read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead) override;
	EStatus readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead) override;

	EStatus write(const Path &, const char*, size_t, uint64_t, size_t*) override
	{
		return EStatus::PERMISSION_DENIED;
	}

	EStatus create(const Path &, const FileInfo &) override
	{
		return EStatus::PERMISSION_DENIED;
	}

	EStatus resize(const Path &, uint64_t) override
	{
		return EStatus::PERMISSION_DENIED;
	}

	EStatus remove(const Path &) override
	{
		return EStatus::PERMISSION_DENIED;
	}
};
