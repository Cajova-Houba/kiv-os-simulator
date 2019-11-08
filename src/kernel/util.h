#pragma once

#include <cstdint>
#include <string>
#include <algorithm>
#include <sstream>
#include <vector>

namespace Util
{
	inline std::string NumberToString(uint64_t number, int base = 10, bool upperCase = true)
	{
		if (base < 2 || base > 36)
		{
			return std::string();
		}

		if (number == 0)
		{
			return std::string("0");
		}

		std::string result;

		while (number > 0)
		{
			const int digit = number % base;

			if (digit < 10)
			{
				result += '0' + digit;
			}
			else
			{
				const int letter = digit - 10;

				result += (upperCase) ? 'A' + letter : 'a' + letter;
			}

			number /= base;
		}

		std::reverse(result.begin(), result.end());

		return result;
	}

	inline std::string NumberToHexString(uint64_t number, bool upperCase = true)
	{
		return std::string("0x") + NumberToString(number, 16, upperCase);
	}

	inline std::string SignedNumberToString(int64_t number)
	{
		return (number < 0) ? std::string("-") + NumberToString(-number) : NumberToString(number);
	}

	inline void SplitPath(std::string filePath, std::vector<std::string> & dest) {
		std::istringstream iss{ filePath };
		std::string item;
		while (std::getline(iss, item, '/')) {
			dest.push_back(item);
		}
	}
}
