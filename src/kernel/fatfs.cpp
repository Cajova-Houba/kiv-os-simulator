#include "fatfs.h"
#include "fat.h"

EStatus FatFS::init(const kiv_hal::TDrive_Parameters & diskParams)
{
	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::query(const Path & path, FileInfo *pInfo)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::write(const Path & path, const char *buffer, size_t bufferSize, uint64_t offset, size_t *pWritten)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::create(const Path & path, const FileInfo & info)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::resize(const Path & path, uint64_t size)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}

EStatus FatFS::remove(const Path & path)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// TODO

	return EStatus::SUCCESS;
}
