#include <windows.h>

#include "util.h"

#define DIRECTORY_SEPARATOR '\\'

std::string Util::GetApplicationDirectory()
{
	char buffer[4096];

	DWORD length = GetModuleFileNameA(NULL, buffer, sizeof buffer);
	if (length == 0 || length >= sizeof buffer)
	{
		// nastala nějaká chyba, takže vracíme prázdný řetězec
		return std::string();
	}

	// název spustitelného souboru aplikace je potřeba odstranit, aby výsledný řetězec obsahoval pouze adresář
	for (int i = length - 1; i >= 0; i--)
	{
		if (buffer[i] == DIRECTORY_SEPARATOR)
		{
			length = i + 1;  // výsledný řetězec vždy obsahuje lomítko na konci
			buffer[length] = '\0';
			break;
		}
	}

	return std::string(buffer);
}
