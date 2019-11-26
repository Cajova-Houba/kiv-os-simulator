#include <cstring>  // std::strcmp
#include <algorithm>

#include "rtl.h"
#include "util.h"

static int64_t GetFileSize(const std::string & path)
{
	RTL::File file;

	if (!file.open(path, true) || !file.setPos(0, RTL::Position::END))
	{
		return -1;
	}

	return file.getPos();
}

static bool ShowFile(const std::string & path)
{
	int64_t size = GetFileSize(path);
	if (size < 0)
	{
		return false;
	}

	// chceme jen název souboru a ne celou cestu
	int namePos = 0;
	int nameLength = 0;
	bool hasNameEnd = false;
	for (size_t i = path.length(); i-- > 0;)
	{
		if (path[i] == '\\' || path[i] == '/')
		{
			if (hasNameEnd)
			{
				break;
			}
		}
		else
		{
			if (hasNameEnd)
			{
				namePos--;
				nameLength++;
			}
			else
			{
				namePos = static_cast<int>(i);
				nameLength = 1;
				hasNameEnd = true;
			}
		}
	}

	RTL::WriteStdOutFormat("%14s %.*s\n", std::to_string(size).c_str(), nameLength, path.c_str() + namePos);

	return true;
}

static bool ListDirectory(const std::string & dirName)
{
	RTL::Directory dir;

	if (!dir.open(dirName))
	{
		if (RTL::GetLastError() == RTL::Error::INVALID_ARGUMENT && ShowFile(dirName))
		{
			return true;
		}

		RTL::WriteStdOutFormat("dir: %s: %s\n", dirName.c_str(), RTL::GetLastErrorMsg().c_str());

		return false;
	}

	std::vector<RTL::DirectoryEntry> content = dir.getContent();

	std::sort(content.begin(), content.end(),
		[](const RTL::DirectoryEntry & a, const RTL::DirectoryEntry & b)
		{
			return std::strcmp(a.name, b.name) < 0;
		}
	);

	RTL::WriteStdOut("<DIR>          .\n");
	RTL::WriteStdOut("<DIR>          ..\n");

	for (const RTL::DirectoryEntry & entry : content)
	{
		if (entry.isDirectory())
		{
			RTL::WriteStdOutFormat("<DIR>          %s\n", entry.name);
		}
		else
		{
			std::string fileName = dirName;
			fileName += '\\';
			fileName += entry.name;

			if (!ShowFile(fileName))
			{
				RTL::WriteStdOutFormat("           N/A %s\n", entry.name);
			}
		}
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(dir)

int dir_main(const char *args)
{
	std::vector<std::string> directories;

	Util::ForEachArg(args, [&](std::string && arg) { directories.emplace_back(std::move(arg)); });

	if (directories.empty())
	{
		ListDirectory(".");  // aktuální adresář
	}
	else if (directories.size() == 1)
	{
		ListDirectory(directories.back());
	}
	else
	{
		const std::string & firstDir = directories.front();

		RTL::WriteStdOutFormat("%s:\n", firstDir.c_str());
		ListDirectory(firstDir);

		for (size_t i = 1; i < directories.size(); i++)
		{
			const std::string & dir = directories[i];

			RTL::WriteStdOutFormat("\n%s:\n", dir.c_str());
			ListDirectory(dir);
		}
	}

	return 0;
}
