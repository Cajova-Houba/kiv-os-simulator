#include "file.h"
#include "kernel.h"

static EStatus ValidateFile(const Path & path, FileInfo & info)
{
	FileInfo currentInfo;
	EStatus queryStatus = Kernel::GetFileSystem().query(path, &currentInfo);
	if (queryStatus != EStatus::SUCCESS)
	{
		return queryStatus;
	}

	if (info.isDirectory() != currentInfo.isDirectory())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	info.size = currentInfo.size;

	return EStatus::SUCCESS;
}

EStatus File::read(char *buffer, size_t bufferSize, size_t *pRead)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	size_t read = 0;
	EStatus status = EStatus::INVALID_ARGUMENT;

	if (m_isOpen)
	{
		if (m_info.isDirectory())
		{
			DirectoryEntry *entries = reinterpret_cast<DirectoryEntry*>(buffer);
			size_t entryCount = bufferSize / sizeof (DirectoryEntry);

			status = Kernel::GetFileSystem().readDir(m_path, entries, entryCount, m_pos, &read);
			m_pos += read;

			read *= sizeof (DirectoryEntry);
		}
		else
		{
			status = Kernel::GetFileSystem().read(m_path, buffer, bufferSize, m_pos, &read);
			m_pos += read;
		}
	}

	if (pRead)
	{
		(*pRead) = read;
	}

	return status;
}

EStatus File::write(const char *buffer, size_t bufferSize, size_t *pWritten)
{
	if (m_info.isReadOnly())
	{
		return EStatus::PERMISSION_DENIED;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	size_t written = 0;
	EStatus status = EStatus::INVALID_ARGUMENT;

	if (m_isOpen)
	{
		status = Kernel::GetFileSystem().write(m_path, buffer, bufferSize, m_pos, &written);
		m_pos += written;
	}

	if (pWritten)
	{
		(*pWritten) = written;
	}

	return status;
}

EStatus File::seek(kiv_os::NFile_Seek command, kiv_os::NFile_Seek base, int64_t offset, uint64_t & result)
{
	if (m_info.isReadOnly() || m_info.isDirectory())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	if (!m_isOpen)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	// získáme aktuální informace o daném souboru pro případ, že se stejným souborem manipuluje i nějaký další proces
	EStatus validateStatus = ValidateFile(m_path, m_info);
	if (validateStatus != EStatus::SUCCESS)
	{
		// původní soubor už neexistuje, takže uzavřeme náš handle
		m_isOpen = false;

		return validateStatus;
	}

	switch (command)
	{
		case kiv_os::NFile_Seek::Get_Position:
		{
			if (base != kiv_os::NFile_Seek::Beginning)
			{
				return EStatus::INVALID_ARGUMENT;
			}

			result = m_pos;

			break;
		}
		case kiv_os::NFile_Seek::Set_Position:
		case kiv_os::NFile_Seek::Set_Size:
		{
			uint64_t newPos = 0;

			switch (base)
			{
				case kiv_os::NFile_Seek::Beginning:
				{
					newPos = (offset < 0) ? 0 : offset;
					break;
				}
				case kiv_os::NFile_Seek::Current:
				{
					newPos = (offset < 0 && static_cast<uint64_t>(-offset) > m_pos) ? 0 : m_pos + offset;
					break;
				}
				case kiv_os::NFile_Seek::End:
				{
					uint64_t endPos = m_info.size;
					newPos = (offset < 0 && static_cast<uint64_t>(-offset) > endPos) ? 0 : endPos + offset;
					break;
				}
				default:
				{
					return EStatus::INVALID_ARGUMENT;
				}
			}

			if (command == kiv_os::NFile_Seek::Set_Size)
			{
				EStatus resizeStatus = Kernel::GetFileSystem().resize(m_path, newPos);
				if (resizeStatus != EStatus::SUCCESS)
				{
					return resizeStatus;
				}
			}
			else
			{
				if (newPos > m_pos)
				{
					newPos = m_pos;
				}
			}

			m_pos = newPos;

			break;
		}
		default:
		{
			return EStatus::INVALID_ARGUMENT;
		}
	}

	return EStatus::SUCCESS;
}
