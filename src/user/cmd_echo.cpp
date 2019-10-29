#include "rtl.h"

RTL_DEFINE_SHELL_PROGRAM(echo)

int echo_main(const char *args)
{
	std::string result;
	result += args;
	result += '\n';

	if (!RTL::WriteStdOut(result))
	{
		return 1;
	}

	return 0;
}
