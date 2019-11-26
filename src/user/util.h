#pragma once

#include <string>
#include <vector>

namespace Util
{
	inline bool IsEOF(char ch)
	{
		return ch == 3 || ch == 4 || ch == 26;  // Ctrl + C, Ctrl + D, Ctrl + Z
	}

	template<class Callback>
	inline void ForEachArg(const char *args, Callback callback)
	{
		size_t begin = 0;
		bool isReady = false;
		int quotationMark = 0;

		for (size_t i = 0; args[i]; i++)
		{
			char ch = args[i];

			switch (ch)
			{
				case ' ':
				case '\t':
				case '\r':
				case '\n':
				{
					if (quotationMark == 0 && isReady)
					{
						// je potřeba řetězec ukončený nulou
						callback(std::string(args + begin, i - begin));

						isReady = false;
					}
					break;
				}
				case '\'':
				{
					if (quotationMark == 0)
					{
						quotationMark = 1;
					}
					else if (quotationMark == 1)
					{
						quotationMark = 0;
					}
					break;
				}
				case '\"':
				{
					if (quotationMark == 0)
					{
						quotationMark = 2;
					}
					else if (quotationMark == 2)
					{
						quotationMark = 0;
					}
					break;
				}
				default:
				{
					if (!isReady)
					{
						isReady = true;
						begin = i;
					}
					break;
				}
			}
		}

		if (isReady)
		{
			callback(std::string(args + begin));
		}
	}
}
