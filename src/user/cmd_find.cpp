#include "rtl.h"
#include "util.h"

static bool ReadInput(uint64_t & lineCount)
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

		for (size_t i = 0; i < length; i++)
		{
			if (buffer[i] == '\n')
			{
				lineCount++;
			}
		}
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(find)

int find_main(const char *args)
{
	bool count = false;
	bool inverse = false;
	bool invalidParam = false;
	std::string match;

	Util::ForEachArg(args,
		[&](const std::string & arg)
		{
			if (arg[0] == '/')  // nějaký parametr
			{
				char param = arg[1];

				switch (param)
				{
					case 'c':
					case 'C':
					{
						count = true;
						break;
					}
					case 'v':
					case 'V':
					{
						inverse = true;
						break;
					}
					default:
					{
						invalidParam = true;
						RTL::WriteStdOutFormat("find: Nepodporovany parametr '%c'\n", param);
						break;
					}
				}
			}
			else if (match.empty())
			{
				match = arg;
			}
			else
			{
				RTL::WriteStdOut("find: Prilis mnoho parametru\n");
			}
		}
	);

	if (invalidParam)
	{
		return 2;
	}

	if (!count || !inverse || !match.empty())  // podporujeme teda jen find /V /C ""
	{
		RTL::WriteStdOut("find: Nepodporovana kombinace parametru\n");
		return 3;
	}

	uint64_t lineCount = 0;

	if (!ReadInput(lineCount))
	{
		return 1;
	}

	RTL::WriteStdOutFormat("%s\n", std::to_string(lineCount).c_str());

	return 0;
}
