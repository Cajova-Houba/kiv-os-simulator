#include "rtl.h"
#include "util.h"

static bool RemoveDirectory(const std::string & name, bool recursively)
{
	if (!RTL::DeleteDirectory(name, recursively))
	{
		RTL::WriteStdOutFormat("rd: %s: %s\n", name.c_str(), RTL::GetLastErrorMsg().c_str());
		return false;
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(rd)

int rd_main(const char *args)
{
	bool recursively = false;
	bool unknownParam = false;

	std::vector<std::string> directories;

	Util::ForEachArg(args,
		[&](std::string && arg)
		{
			if (arg.length() == 2 && arg[0] == '/')  // nějaký parametr
			{
				char param = arg[1];

				switch (param)
				{
					case 's':
					case 'S':  // odstranit i adresáře, ve kterých něco je
					{
						recursively = true;
						break;
					}
					default:
					{
						unknownParam = true;
						RTL::WriteStdOutFormat("rd: Nepodporovany parametr '%c'\n", param);
						break;
					}
				}
			}
			else
			{
				directories.emplace_back(std::move(arg));
			}
		}
	);

	if (unknownParam)
	{
		return 2;
	}

	for (const std::string & name : directories)
	{
		RemoveDirectory(name, recursively);
	}

	return 0;
}
