#include "rtl.h"

RTL_DEFINE_SHELL_PROGRAM(echo)

int echo_main(const char *args)
{
	StringBuffer<4096> buffer;
	buffer += args;
	buffer += '\n';

	if (!RTL::WriteStdOut(buffer))
	{
		return 1;
	}

	return 0;
}
