#include "rtl.h"

RTL_DEFINE_SHELL_PROGRAM(shutdown)

int shutdown_main(const char *args)
{
	RTL::Shutdown();

	return 0;
}
