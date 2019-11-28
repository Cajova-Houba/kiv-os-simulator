#include <cctype>  // std::isdigit

#include "rtl.h"

static bool LoadProcContent(std::vector<RTL::DirectoryEntry> & content)
{
	RTL::Directory proc;

	if (!proc.open("0:"))
	{
		RTL::WriteStdOutFormat("tasklist: Nelze otevrit 'proc' adresar: %s\n", RTL::GetLastErrorMsg().c_str());
		return false;
	}

	content = proc.getContent();

	return true;
}

static bool IsProcessID(const char *string)
{
	if (!string || !string[0])
	{
		return false;
	}

	for (size_t i = 0; string[i]; i++)
	{
		if (!std::isdigit(string[i]))
		{
			return false;
		}
	}

	return true;
}

static bool GetProcessAttribute(const char *attribute, const char *pid, char *buffer, size_t bufferSize)
{
	StringBuffer<256> path;
	path += "0:";
	path += '\\';
	path += pid;
	path += '\\';
	path += attribute;

	RTL::File file;

	if (!file.open(path.get(), true))  // jen pro čtení
	{
		RTL::WriteStdOutFormat("tasklist: Nelze otevrit '%s': %s\n", path.get(), RTL::GetLastErrorMsg().c_str());
		return false;
	}

	size_t length = 0;
	if (!file.read(buffer, bufferSize, &length))
	{
		RTL::WriteStdOutFormat("tasklist: Nelze cist z '%s': %s\n", path.get(), RTL::GetLastErrorMsg().c_str());
		return false;
	}

	if (length > 0)
	{
		length--;  // nechceme odřádkování na konci
	}

	buffer[length] = '\0';

	return true;
}

static bool DumpProcess(const char *pid)
{
	char nameBuffer[64];
	if (!GetProcessAttribute("name", pid, nameBuffer, sizeof nameBuffer))
	{
		return false;
	}

	RTL::WriteStdOutFormat("%5s %s\n", pid, nameBuffer);

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(tasklist)

int tasklist_main(const char *args)
{
	std::vector<RTL::DirectoryEntry> proc;
	if (!LoadProcContent(proc))
	{
		return 1;
	}

	for (const RTL::DirectoryEntry & entry : proc)
	{
		if (entry.isDirectory() && IsProcessID(entry.name))
		{
			DumpProcess(entry.name);
		}
	}

	return 0;
}
