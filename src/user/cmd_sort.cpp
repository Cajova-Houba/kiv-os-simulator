#include <deque>
#include <algorithm>

#include "rtl.h"
#include "util.h"

static bool ReadInput(std::deque<std::string> & lines)
{
	lines.resize(1);

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
			char ch = buffer[i];

			switch (ch)
			{
				case '\r':
				{
					break;
				}
				case '\n':
				{
					lines.emplace_back();  // nový řádek
					break;
				}
				default:
				{
					lines.back() += ch;
					break;
				}
			}
		}
	}

	if (lines.back().empty())
	{
		lines.pop_back();
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(sort)

int sort_main(const char *args)
{
	std::deque<std::string> lines;

	if (!ReadInput(lines))
	{
		return 1;
	}

	std::sort(lines.begin(), lines.end());

	for (const std::string & line : lines)
	{
		RTL::WriteStdOut(line);
		RTL::WriteStdOut("\n");
	}

	return 0;
}
