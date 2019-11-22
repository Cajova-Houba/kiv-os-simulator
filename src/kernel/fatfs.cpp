#include "fatfs.h"

EStatus FatFS::query(const Path & path, FileInfo * pInfo)
{
	return EStatus();
}

EStatus FatFS::read(const Path & path, char * buffer, size_t bufferSize, uint64_t offset, size_t * pRead)
{
	return EStatus();
}

EStatus FatFS::readDir(const Path & path, DirectoryEntry * entries, size_t entryCount, size_t offset, size_t * pRead)
{
	return EStatus();
}

EStatus FatFS::write(const Path &, const char *, size_t, uint64_t, size_t *)
{
	return EStatus();
}

EStatus FatFS::create(const Path &, const FileInfo &)
{
	return EStatus();
}

EStatus FatFS::resize(const Path &, uint64_t)
{
	return EStatus();
}

EStatus FatFS::remove(const Path &)
{
	return EStatus();
}
