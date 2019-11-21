#pragma once

#include <map>
#include <memory>

#include "file.h"

// všechny souborové systémy implementují toto rozhraní
struct IFileSystem
{
	virtual EStatus query(const Path & path, FileInfo *pInfo) = 0;

	virtual EStatus read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead) = 0;
	virtual EStatus readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead) = 0;
	virtual EStatus write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten) = 0;

	virtual EStatus create(const Path & path, const FileInfo & info) = 0;
	virtual EStatus resize(const Path & path, uint64_t size) = 0;
	virtual EStatus remove(const Path & path) = 0;
};

// správce souborových systémů
class FileSystem
{
	// mapuje písmena disků na jednotlivé instance souborových systémů
	// po úvodní inicializaci se už nemění, takže není třeba mutex
	std::map<char, std::unique_ptr<IFileSystem>> m_filesystems;

	IFileSystem *getFileSystem(char diskLetter) const
	{
		auto it = m_filesystems.find(diskLetter);
		if (it == m_filesystems.end())
		{
			return nullptr;
		}

		return it->second.get();
	}

public:
	FileSystem() = default;

	void init();

	EStatus query(const Path & path, FileInfo *pInfo = nullptr);

	EStatus read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead);
	EStatus readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead);
	EStatus write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten);

	EStatus create(const Path & path, const FileInfo & info);
	EStatus resize(const Path & path, uint64_t size);
	EStatus remove(const Path & path);
};
