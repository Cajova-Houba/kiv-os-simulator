#include "rtl.h"
#include "util.h"

static bool MakeDirectory(const std::string & name)
{
	RTL::Directory dir;

	if (!dir.create(name))
	{
		RTL::WriteStdOutFormat("md: %s: %s\n", name.c_str(), RTL::GetLastErrorMsg().c_str());
		return false;
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(md)

int md_main(const char *args)
{
	Util::ForEachArg(args, [](const std::string & arg) { MakeDirectory(arg); });

	return 0;
}
