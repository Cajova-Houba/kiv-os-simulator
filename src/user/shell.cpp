#include "rtl.h"

RTL_DEFINE_SHELL_PROGRAM(shell)

int shell_main(const char *args)
{
	// TODO

	const char *prompt = "C:\\>";
	const char *intro  = "Vitejte v kostre semestralni prace z KIV/OS.\n"
	                     "Shell zobrazuje echo zadaneho retezce. Prikaz exit ukonci shell.\n";

	RTL::WriteStdOut(intro);

	char buffer[4096];

	do
	{
		RTL::WriteStdOut(prompt);

		size_t read = 0;
		if (!RTL::ReadStdIn(buffer, (sizeof buffer)-1, &read) || read == 0)
		{
			break;  // EOF
		}

		buffer[read] = '\0';

		RTL::WriteStdOut(buffer);
	}
	while (strcmp(buffer, "exit\n") != 0);

	return 0;
}
