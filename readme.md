# Struktura

 - `src`
 	- Složka obsahuje všechny zdrojové kódy aplikace.
 - `msvc`
 	- Složka obsahuje solution soubory pro Visual Studio
 - `doc`
 	- Složka s dokumentací.
 - `compiled`
 	- Složka kam se ukládá kompilovaný výstup (už nakonfigurováno)

# Sestaveni a spusteni

1) Otevřít (ve visual studiu) a sestavit (zelená špika) projekt
`msvc/kernel/kernel.vcxproj`
	- Tím se v adresáři compiled kernel.dll
	- Teoreticky může hodit chybu 'boot.exe nenalezen' ale jestli to 
udělá `kernel.dll` je to v cajku

2) Otevřít (ve visual studiu) a spustit projekt `msvc/boot/boot.vcxproj`
	- Bez `kernel.dll` se v konzoli objeví chybová hláška
	- S existujícím `kernel.dll` se program spustí, vypíše nalezené 
disky a ukončí se

3) Otevřít (ve visual studio) a spustit projekt `msvc/user/user.vcxproj`
	- Kdyby linker vyhazoval chyby, zkomentujte v `src/api/user.def` řádky tasklist a find 
		tyhle příkazy nejsou nikde naimplementované / nemají hlavičku, takže je knihovna ani nemůže poskytovat
	- Vytvoří knihovnu `user.dll`, která obsahuje implementaci shellu
	- Bez ní to sice funguje, ale vypíše to 'Shell nenalezen' a vypne se to.

## Spuštění

Po předchozích krocích by mělo v cmd jít spustit `boot.exe` s podobným výstupem

```
Nalezen disk: 0x81
Vitejte v kostre semestralni prace z KIV/OS.
Shell zobrazuje echo zadaneho retezce. Prikaz exit ukonci shell.
C:\>ls
ls
C:\>dir
dir
C:\>exit
exit
```


# Poznámky

 - `L` před řetězcem (`L"user.dll"`, `L'0'`, ...) označuje řetězce, které potřebují 16 bitů na znak místo normálních 8 (`wchar_t`)

 - Zavaděč kernelu je implementovaný v `kernel.cpp.Bootstrap_Loader`

 - Kernel načítá shell (to implementujeme my) jako dynamickou knihovnu `user.dll` (viz `kernel.cpp:Initialize_Kernel()`).


# TODO

- [x] Boot
  - [x] Refactoring
- [ ] Kernel
  - [x] Handle system
  - [x] Procesy + vlákna + signály
  - [x] Události (waitFor)
  - [x] Roura
  - [ ] File system
    - [x] Jednotné API + syscally
    - [x] procfs
    - [x] FAT
- [ ] User-space
  - [x] RTL
  - [x] Shell
  - [ ] Příkazy
    - [x] dir
    - [x] echo
    - [x] find
    - [x] freq
    - [x] md
    - [x] rd
    - [ ] rgen
    - [x] shutdown
    - [x] sort
    - [x] tasklist
    - [x] type

