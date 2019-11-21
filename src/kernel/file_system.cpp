#include "file_system.h"

void FileSystem::init()
{
	// TODO
}

EStatus FileSystem::query(const Path & path, FileInfo *pInfo)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->query(path, pInfo) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->read(path, buffer, bufferSize, offset, pRead) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->readDir(path, entries, entryCount, offset, pRead) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->write(path, buffer, bufferSize, offset, pWritten) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::create(const Path & path, const FileInfo & info)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->create(path, info) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::resize(const Path & path, uint64_t size)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->resize(path, size) : EStatus::FILE_NOT_FOUND;
}

EStatus FileSystem::remove(const Path & path)
{
	IFileSystem *pFileSystem = getFileSystem(path.getDiskLetter());

	return (pFileSystem) ? pFileSystem->remove(path) : EStatus::FILE_NOT_FOUND;
}
