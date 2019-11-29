#include "rtl.h"
#include "util.h"

static void ShowFileError(const std::string & name)
{
	RTL::WriteStdOutFormat("type: %s: %s\n", name.c_str(), RTL::GetLastErrorMsg().c_str());
}

static bool PrintFile(const std::string & name)
{
	RTL::File file;

	if (!file.open(name, true))  // jen pro čtení
	{
		ShowFileError(name);
		return false;
	}

	char buffer[4096];
	size_t length;

	do
	{
		length = 0;
		if (!file.read(buffer, sizeof buffer, &length))
		{
			ShowFileError(name);
			return false;
		}

		if (length > 0)
		{
			RTL::WriteStdOut(buffer, length);
		}
	}
	while (length == sizeof buffer);

	return true;
}

static bool PrintStdIn()
{
	char buffer[4096];

	bool hasEOF = false;
	while (!hasEOF)
	{
		size_t length = 0;
		if (!RTL::ReadStdIn(buffer, sizeof buffer, &length))
		{
			return false;
		}

		if (length == 0)
		{
			hasEOF = true;
		}
		else if (Util::IsEOF(buffer[length-1]))
		{
			hasEOF = true;
			length--;
		}

		if (length > 0)
		{
			RTL::WriteStdOut(buffer, length);
		}
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(type)

int type_main(const char *args)
{
	unsigned int fileCount = 0;

	Util::ForEachArg(args, [&](const std::string & arg) { PrintFile(arg); fileCount++; });

	if (fileCount == 0)
	{
		PrintStdIn();
	}

	return 0;
}
