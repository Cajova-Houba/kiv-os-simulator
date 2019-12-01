#include <cstring>
#include <array>

#include "procfs.h"
#include "kernel.h"
#include "process.h"
#include "util.h"

enum struct EProcessFile
{
	COMMAND_LINE,
	WORKING_DIRECTORY,
	PROCESS_NAME,
	THREAD_COUNT
};

constexpr std::array<const char*, 4> PROCESS_FILE_NAMES = { "args", "cwd", "name", "threads" };

static size_t CopyValue(const std::string & value, char *buffer, size_t bufferSize, uint64_t offset)
{
	size_t length = value.length();

	if (length < offset)
	{
		return 0;
	}

	length -= static_cast<size_t>(offset);

	if (length > bufferSize)
	{
		length = bufferSize;
	}

	std::memcpy(buffer, value.c_str() + offset, length);

	if (length < bufferSize)
	{
		buffer[length++] = '\n';  // odřádkování
	}

	return length;
}

static EStatus QueryProcessFile(const std::string & fileName, Process & process, FileInfo *pInfo)
{
	size_t i = 0;

	for (const char *name : PROCESS_FILE_NAMES)
	{
		if (fileName == name)
		{
			uint64_t size = 0;

			switch (static_cast<EProcessFile>(i))
			{
				case EProcessFile::COMMAND_LINE:
				{
					size = process.getCmdLine().length();
					break;
				}
				case EProcessFile::WORKING_DIRECTORY:
				{
					size = process.getWorkingDirectoryString().length();
					break;
				}
				case EProcessFile::PROCESS_NAME:
				{
					size = process.getName().length();
					break;
				}
				case EProcessFile::THREAD_COUNT:
				{
					size = std::to_string(process.getThreadCount()).length();
					break;
				}
			}

			if (pInfo)
			{
				pInfo->attributes = FileAttributes::READ_ONLY;
				pInfo->size = size;
			}

			return EStatus::SUCCESS;
		}

		i++;
	}

	return EStatus::FILE_NOT_FOUND;
}

static EStatus ReadProcessFile(const std::string & fileName, Process & process,
                               char *buffer, size_t size, uint64_t offset, size_t *pRead)
{
	size_t i = 0;

	for (const char *name : PROCESS_FILE_NAMES)
	{
		if (fileName == name)
		{
			size_t length = 0;

			switch (static_cast<EProcessFile>(i))
			{
				case EProcessFile::COMMAND_LINE:
				{
					length = CopyValue(process.getCmdLine(), buffer, size, offset);
					break;
				}
				case EProcessFile::WORKING_DIRECTORY:
				{
					length = CopyValue(process.getWorkingDirectoryString(), buffer, size, offset);
					break;
				}
				case EProcessFile::PROCESS_NAME:
				{
					length = CopyValue(process.getName(), buffer, size, offset);
					break;
				}
				case EProcessFile::THREAD_COUNT:
				{
					length = CopyValue(std::to_string(process.getThreadCount()), buffer, size, offset);
					break;
				}
			}

			if (pRead)
			{
				(*pRead) = length;
			}

			return EStatus::SUCCESS;
		}

		i++;
	}

	return EStatus::FILE_NOT_FOUND;
}

EStatus ProcFS::query(const Path & path, FileInfo *pInfo)
{
	switch (path.getComponentCount())
	{
		case 0:  // kořenový adresář procfs
		{
			if (pInfo)
			{
				pInfo->attributes = FileAttributes::READ_ONLY | FileAttributes::DIRECTORY;
				pInfo->size = 0;
			}

			break;
		}
		case 1:  // adresář nějakého procesu
		{
			if (path[0] != "self")
			{
				HandleID processID = Util::StringToHandleID(path[0]);

				if (!Kernel::GetHandleStorage().hasHandleOfType(processID, EHandle::PROCESS))
				{
					return EStatus::FILE_NOT_FOUND;
				}
			}

			if (pInfo)
			{
				pInfo->attributes = FileAttributes::READ_ONLY | FileAttributes::DIRECTORY;
				pInfo->size = 0;
			}

			break;
		}
		case 2:  // nějaký soubor v adresáři nějakého procesu
		{
			if (path[0] == "self")
			{
				Process & currentProcess = Thread::GetProcess();

				return QueryProcessFile(path[1], currentProcess, pInfo);
			}
			else
			{
				HandleID processID = Util::StringToHandleID(path[0]);

				HandleReference process = Kernel::GetHandleStorage().getHandle(processID);
				if (!process || process->getHandleType() != EHandle::PROCESS)
				{
					return EStatus::FILE_NOT_FOUND;
				}

				return QueryProcessFile(path[1], *process.as<Process>(), pInfo);
			}

			break;
		}
		default:
		{
			return EStatus::FILE_NOT_FOUND;
		}
	}

	return EStatus::SUCCESS;
}

EStatus ProcFS::read(const Path & path, char *buffer, size_t bufferSize, uint64_t offset, size_t *pRead)
{
	if (path.getComponentCount() == 2)
	{
		if (path[0] == "self")
		{
			Process & currentProcess = Thread::GetProcess();

			return ReadProcessFile(path[1], currentProcess, buffer, bufferSize, offset, pRead);
		}
		else
		{
			HandleID processID = Util::StringToHandleID(path[0]);

			HandleReference process = Kernel::GetHandleStorage().getHandle(processID);
			if (process && process->getHandleType() == EHandle::PROCESS)
			{
				return ReadProcessFile(path[1], *process.as<Process>(), buffer, bufferSize, offset, pRead);
			}
		}
	}

	return EStatus::FILE_NOT_FOUND;
}

EStatus ProcFS::readDir(const Path & path, DirectoryEntry *entries, size_t entryCount, size_t offset, size_t *pRead)
{
	switch (path.getComponentCount())
	{
		case 0:
		{
			std::vector<HandleReference> processes = Kernel::GetHandleStorage().getHandles(
				[](HandleID id, const IHandle *pHandle) -> bool
				{
					return pHandle->getHandleType() == EHandle::PROCESS;
				}
			);

			const uint16_t attributes = FileAttributes::READ_ONLY | FileAttributes::DIRECTORY;

			size_t i = 0;
			size_t pos = offset;
			while (i < entryCount && pos < processes.size())
			{
				Util::SetDirectoryEntry(entries[i], attributes, std::to_string(processes[pos].getID()));

				i++;
				pos++;
			}

			if (i < entryCount)
			{
				Util::SetDirectoryEntry(entries[i], attributes, "self");

				i++;
			}

			if (pRead)
			{
				(*pRead) = i;
			}

			break;
		}
		case 1:
		{
			if (path[0] != "self")
			{
				HandleID processID = Util::StringToHandleID(path[0]);

				if (!Kernel::GetHandleStorage().hasHandleOfType(processID, EHandle::PROCESS))
				{
					return EStatus::FILE_NOT_FOUND;
				}
			}

			const uint16_t attributes = FileAttributes::READ_ONLY;  // soubor

			size_t i = 0;
			size_t pos = offset;
			while (i < entryCount && pos < PROCESS_FILE_NAMES.size())
			{
				Util::SetDirectoryEntry(entries[i], attributes, PROCESS_FILE_NAMES[pos]);

				i++;
				pos++;
			}

			if (pRead)
			{
				(*pRead) = i;
			}

			break;
		}
		default:
		{
			return EStatus::FILE_NOT_FOUND;
		}
	}

	return EStatus::SUCCESS;
}
