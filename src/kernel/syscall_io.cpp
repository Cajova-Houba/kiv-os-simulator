#include "syscall.h"
#include "kernel.h"
#include "thread.h"
#include "process.h"
#include "pipe.h"

static EStatus Open(const char *path, uint8_t flags, uint16_t attributes, HandleID & result)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	// TODO
	return EStatus::UNKNOWN_ERROR;
}

static EStatus Write(HandleID id, const char *buffer, size_t bufferSize, size_t & written)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandle(id);
	if (!handle || handle->getHandleType() != EHandle::FILE)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	return handle.as<IFileHandle>()->write(buffer, bufferSize, &written);
}

static EStatus Read(HandleID id, char *buffer, size_t bufferSize, size_t & read)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandle(id);
	if (!handle || handle->getHandleType() != EHandle::FILE)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	return handle.as<IFileHandle>()->read(buffer, bufferSize, &read);
}

static EStatus Seek(HandleID id, uint16_t type, size_t pos, size_t & result)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	HandleReference handle = pCurrentProcess->getHandle(id);
	if (!handle || handle->getHandleType() != EHandle::FILE)
	{
		return EStatus::INVALID_ARGUMENT;
	}

	IFileHandle *pFile = handle.as<IFileHandle>();

	kiv_os::NFile_Seek command = static_cast<kiv_os::NFile_Seek>(type >> 8);
	kiv_os::NFile_Seek base    = static_cast<kiv_os::NFile_Seek>(type & 0xFF);

	// TODO
	return EStatus::UNKNOWN_ERROR;
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

static EStatus Delete(const char *path)
{
	// TODO
	return EStatus::UNKNOWN_ERROR;
}

static EStatus SetWorkingDirectory(const char *path)
{
	Process *pCurrentProcess = Thread::GetProcess();
	if (!pCurrentProcess)
	{
		return EStatus::UNRECOGNIZED_THREAD;
	}

	// TODO: zkontrolovat, zda cesta opravdu existuje

	pCurrentProcess->setWorkingDirectory(path);

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

	length = pCurrentProcess->getWorkingDirectory(buffer, bufferSize);

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
