# Sestaveni a spusteni

1) Otevřít (ve visual studiu) a sestavit (zelená špika) projekt
msvc/kernel/kernel.vcxproj
	- Tím se v adresáři compiled kernel.dll
	- Teoreticky může hodit chybu 'boot.exe nenalezen' ale jestli to 
udělá kernel.dll je to v cajku

2) Otevřít (ve visual studiu) a spustit projekt msvc/boot/boot.vcxproj
	- Bez kernel.dll se v konzoli objeví chybová hláška
	- S existujícím kernel.dll se program spustí, vypíše nalezené 
disky a ukončí se

# Poznámky

 - `L` před řetězcem (`L"user.dll"`, `L'0'`, ...) označuje řetězce, které potřebují 16 bitů na znak místo normálních 8 (`wchar_t`)

 - Zavaděč kernelu je implementovaný v `kernel.cpp.Bootstrap_Loader`

 - Kernel načítá shell (to implementujeme my) jako dynamickou knihovnu `user.dll` (viz `kernel.cpp:Initialize_Kernel()`).
