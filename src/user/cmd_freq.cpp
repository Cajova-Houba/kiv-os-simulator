#include <array>

#include "rtl.h"
#include "util.h"

using Table = std::array<uint32_t, 256>;

static bool ReadInput(Table & table)
{
	char buffer[4096];
	size_t length;

	bool hasEOF = false;
	while (!hasEOF)
	{
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
			uint8_t byte = buffer[i];

			table[byte]++;
		}
	}

	return true;
}

static void ShowResult(Table & table)
{
	StringBuffer<4096> result;

	for (size_t i = 0; i < table.size(); i++)
	{
		uint32_t value = table[i];

		if (value > 0)
		{
			result.append_f("0x%hhx : %u\n", static_cast<uint8_t>(i), value);
		}
	}

	RTL::WriteStdOut(result);
}

RTL_DEFINE_SHELL_PROGRAM(freq)

int freq_main(const char *args)
{
	Table table;
	table.fill(0);

	if (!ReadInput(table))
	{
		return 1;
	}

	ShowResult(table);

	return 0;
}
