#include "syscall.h"
#include "kernel.h"
#include "thread.h"
#include "process.h"
#include "pipe.h"

static EStatus OpenFile(Path && path, uint16_t attributes, HandleReference & result)
{
	const bool wantsDirectory = attributes & FileAttributes::DIRECTORY;
	const bool wantsReadOnly = attributes & FileAttributes::READ_ONLY;

	FileInfo info;
	EStatus status = Kernel::GetFileSystem().query(path, &info);
	if (status != EStatus::SUCCESS)
	{
		// soubor nebo adresář neexistuje, takže jej nelze otevřít
		return status;
	}

	if (info.isDirectory() != wantsDirectory)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	if (info.isReadOnly() && !wantsReadOnly)
	{
		return EStatus::PERMISSION_DENIED;
	}

	if (wantsReadOnly)
	{
		info.attributes |= FileAttributes::READ_ONLY;
	}

	result = Kernel::GetHandleStorage().addHandle(std::make_unique<File>(std::move(path), info));

	return EStatus::SUCCESS;
}

static EStatus CreateFile(Path && path, uint16_t attributes, HandleReference & result)
{
	FileInfo info;
	info.attributes = attributes;
	info.size = 0;

	EStatus status = Kernel::GetFileSystem().create(path, info);
	if (status != EStatus::SUCCESS)
	{
		// soubor nebo adresář nelze vytvořit
		return status;
	}

	if (info.isReadOnly())
	{
		// nově vytvořený soubor je otevřen vždy i pro zápis
		info.attributes &= ~FileAttributes::READ_ONLY;
	}

	result = Kernel::GetHandleStorage().addHandle(std::make_unique<File>(std::move(path), info));

	return EStatus::SUCCESS;
}

static EStatus Open(const char *pathString, uint8_t flags, uint16_t attributes, HandleID & result)
{
	bool wantsOpenAlways = flags & static_cast<uint8_t>(kiv_os::NOpen_File::fmOpen_Always);

	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	Path path = Path::Parse(pathString);
	if (!path)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	if (!path.isAbsolute())
	{
		pCurrentProcess->makePathAbsolute(path);
	}

	HandleReference handle;

	EStatus openStatus = OpenFile(std::move(path), attributes, handle);
	if (openStatus != EStatus::SUCCESS)
	{
		if (wantsOpenAlways)
		{
			return openStatus;
		}
		else
		{
			if (openStatus != EStatus::FILE_NOT_FOUND)
			{
				return openStatus;
			}

			EStatus createStatus = CreateFile(std::move(path), attributes, handle);
			if (createStatus != EStatus::SUCCESS)
			{
				return createStatus;
			}
		}
	}

	if (!handle)
	{
		return EStatus::OUT_OF_MEMORY;
	}

	result = handle.getID();

	pCurrentProcess->addHandle(std::move(handle));

	return EStatus::SUCCESS;
}

static EStatus Write(HandleID id, const char *buffer, size_t bufferSize, size_t & written)
{
	if (buffer == nullptr || bufferSize == 0)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandleOfType(id, EHandle::FILE);
	if (!handle)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	return handle.as<IFileHandle>()->write(buffer, bufferSize, &written);
}

static EStatus Read(HandleID id, char *buffer, size_t bufferSize, size_t & read)
{
	if (buffer == nullptr || bufferSize == 0)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandleOfType(id, EHandle::FILE);
	if (!handle)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	return handle.as<IFileHandle>()->read(buffer, bufferSize, &read);
}

static EStatus Seek(HandleID id, uint16_t type, int64_t offset, uint64_t & result)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandleOfType(id, EHandle::FILE);
	if (!handle || handle.as<IFileHandle>()->getFileHandleType() != EFileHandle::REGULAR_FILE)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	kiv_os::NFile_Seek command = static_cast<kiv_os::NFile_Seek>(type >> 8);
	kiv_os::NFile_Seek base    = static_cast<kiv_os::NFile_Seek>(type & 0xFF);

	return handle.as<File>()->seek(command, base, offset, result);
}

static EStatus Close(HandleID id)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandle(id);
	if (!handle)
	{
		// daný handle neexistuje nebo k němu proces nemá přístup
		return EStatus::INVALID_ARGUMENT;
	}

	if (handle->getHandleType() == EHandle::FILE)
	{
		handle.as<IFileHandle>()->close();
	}

	pCurrentProcess->removeHandle(id);

	return EStatus::SUCCESS;
}

static EStatus Delete(const char *pathString)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	Path path = Path::Parse(pathString);
	if (!path)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	if (!path.isAbsolute())
	{
		pCurrentProcess->makePathAbsolute(path);
	}

	return Kernel::GetFileSystem().remove(path);
}

static EStatus SetWorkingDirectory(const char *pathString)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	Path path = Path::Parse(pathString);
	if (!path)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	if (!path.isAbsolute())
	{
		pCurrentProcess->makePathAbsolute(path);
	}

	FileInfo info;
	EStatus status = Kernel::GetFileSystem().query(path, &info);
	if (status != EStatus::SUCCESS)
	{
		return status;
	}

	if (!info.isDirectory())
	{
		return EStatus::INVALID_ARGUMENT;
	}

	pCurrentProcess->setWorkingDirectory(std::move(path));

	return EStatus::SUCCESS;
}

static EStatus GetWorkingDirectory(char *buffer, size_t bufferSize, size_t & length)
{
	if (buffer == nullptr || bufferSize == 0)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	length = pCurrentProcess->getWorkingDirectoryStringBuffer(buffer, bufferSize);

	return EStatus::SUCCESS;
}

static EStatus CreatePipe(HandleID *pipe)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference readEnd;
	HandleReference writeEnd;
	if (!Pipe::Create(readEnd, writeEnd))
	{
		return EStatus::OUT_OF_MEMORY;
	}

	pipe[0] = writeEnd.getID();
	pipe[1] = readEnd.getID();

	pCurrentProcess->addHandle(std::move(readEnd));
	pCurrentProcess->addHandle(std::move(writeEnd));

	return EStatus::SUCCESS;
}

EStatus SysCall::HandleIO(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_os::NOS_File_System>(context.rax.l))
	{
		case kiv_os::NOS_File_System::Open_File:
		{
			return Open(reinterpret_cast<const char*>(context.rdx.r), context.rcx.l, context.rdi.i, context.rax.x);
		}
		case kiv_os::NOS_File_System::Write_File:
		{
			return Write(context.rdx.x, reinterpret_cast<const char*>(context.rdi.r), context.rcx.r, context.rax.r);
		}
		case kiv_os::NOS_File_System::Read_File:
		{
			return Read(context.rdx.x, reinterpret_cast<char*>(context.rdi.r), context.rcx.r, context.rax.r);
		}
		case kiv_os::NOS_File_System::Seek:
		{
			return Seek(context.rdx.x, context.rcx.x, context.rdi.r, context.rax.r);
		}
		case kiv_os::NOS_File_System::Close_Handle:
		{
			return Close(context.rdx.x);
		}
		case kiv_os::NOS_File_System::Delete_File:
		{
			return Delete(reinterpret_cast<const char*>(context.rdx.r));
		}
		case kiv_os::NOS_File_System::Set_Working_Dir:
		{
			return SetWorkingDirectory(reinterpret_cast<const char*>(context.rdx.r));
		}
		case kiv_os::NOS_File_System::Get_Working_Dir:
		{
			return GetWorkingDirectory(reinterpret_cast<char*>(context.rdx.r), context.rcx.r, context.rax.r);
		}
		case kiv_os::NOS_File_System::Create_Pipe:
		{
			return CreatePipe(reinterpret_cast<HandleID*>(context.rdx.r));
		}
	}

	return EStatus::INVALID_ARGUMENT;
}
