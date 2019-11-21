#pragma once

#include <mutex>

#include "../api/api.h"  // kiv_os::NFile_Seek

#include "handle.h"
#include "path.h"
#include "types.h"

namespace FileAttributes  // kiv_os::NFile_Attributes
{
	enum
	{
		READ_ONLY   = (1 << 0),
		HIDDEN      = (1 << 1),
		SYSTEM_FILE = (1 << 2),
		VOLUME_ID   = (1 << 3),
		DIRECTORY   = (1 << 4),
		ARCHIVE     = (1 << 5)
	};
}

struct FileInfo
{
	uint16_t attributes;  // FileAttributes
	uint64_t size;

	bool isReadOnly() const
	{
		return attributes & FileAttributes::READ_ONLY;
	}

	bool isDirectory() const
	{
		return attributes & FileAttributes::DIRECTORY;
	}
};

struct DirectoryEntry  // rtl.h
{
	uint16_t attributes;  // FileAttributes
	char name[62];        // řetězec ukončený nulou

	bool isReadOnly() const
	{
		return attributes & FileAttributes::READ_ONLY;
	}

	bool isDirectory() const
	{
		return attributes & FileAttributes::DIRECTORY;
	}
};

// handle na soubor nebo adresář
class File : public IFileHandle
{
	std::mutex m_mutex;
	uint64_t m_pos;
	FileInfo m_info;
	Path m_path;
	bool m_isOpen;

public:
	File(Path && path, const FileInfo & info)
	: m_mutex(),
	  m_pos(0),
	  m_info(info),
	  m_path(std::move(path)),
	  m_isOpen(true)
	{
	}

	EFileHandle getFileHandleType() const override
	{
		// atributy se nemění, takže zde není potřeba zamykat mutex
		return m_info.isDirectory() ? EFileHandle::DIRECTORY : EFileHandle::REGULAR_FILE;
	}

	void close() override
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		m_isOpen = false;
	}

	EStatus read(char *buffer, size_t bufferSize, size_t *pRead) override;
	EStatus write(const char *buffer, size_t bufferSize, size_t *pWritten) override;

	EStatus seek(kiv_os::NFile_Seek command, kiv_os::NFile_Seek base, int64_t offset, uint64_t & result);
};
