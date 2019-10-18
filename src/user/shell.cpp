#pragma once
#include <string>
#include "shell.h"

#include "rtl.h"
#include "echo.h"

size_t __stdcall shell(const kiv_hal::TRegisters &regs) {

	const kiv_os::THandle std_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle std_out = static_cast<kiv_os::THandle>(regs.rbx.x);

	// input buffer na 256 znak�
	const size_t buffer_size = 256;
	char buffer[buffer_size];
	size_t counter;

	// jmeno prikazu echo
	std::string echo_cmd("echo ");

	// registry na predavani parametru
	kiv_hal::TRegisters param_regs;
	
	const char* intro = "Vitejte v kostre semestralni prace z KIV/OS.\n" \
						"Shell zobrazuje echo zadaneho retezce. Prikaz exit ukonci shell.\n";
	kiv_os_rtl::Write_File(std_out, intro, strlen(intro), counter);


	const char* prompt = "C:\\>";

	do {
		// vypsan� promptu na std_out
		kiv_os_rtl::Write_File(std_out, prompt, strlen(prompt), counter);

		// jak u�ivatel p�e na konzoli, �tu po znaku a ukl�d�m do bufferu
		// dokud nen� pln�
		// vzhledem k tomu, �e se nic nenuluje, tak tohle p�episuje buffer
		if (kiv_os_rtl::Read_File(std_in, buffer, buffer_size, counter) && (counter > 0)) {
			if (counter == buffer_size) counter--;

			// na konec pln�ho bufferu um�st�m \0
			buffer[counter] = 0;	//udelame z precteneho vstup null-terminated retezec
		}
		else {
			break;	//EOF
		}

		// �et�zec v bufferu za��n� "echo " -> zkontroluj
		// jestli n�co n�sleduje a vypi� to
		if (echo_cmd.compare(0, 5, buffer, 5) == 0 ) {

			// napl� parameters
			// buffer[5:] je �et�zec po "echo "
			param_regs.rax.r = reinterpret_cast<uint64_t>(buffer + 5 * sizeof(char));
			param_regs.rbx.x = static_cast<uint32_t>(std_out);

			// zavolej echo
			echo(param_regs);
		}
		else {
			// nerozpoznany prikaz
			// vyp�u "\n<obsah bufferu>\n" na std_out
			const char* new_line = "\n";
			kiv_os_rtl::Write_File(std_out, new_line, strlen(new_line), counter);
			kiv_os_rtl::Write_File(std_out, buffer, strlen(buffer), counter);	//a vypiseme ho
			kiv_os_rtl::Write_File(std_out, new_line, strlen(new_line), counter);
		}
	} while (strcmp(buffer, "exit") != 0);

	

	return 0;	
}