#include <atomic>
#include <random>

#include "rtl.h"
#include "util.h"

static std::atomic<bool> g_isRunning;

static void SetRunning(bool isRunning)
{
	g_isRunning.store(isRunning, std::memory_order_relaxed);
}

static bool IsRunning()
{
	return g_isRunning.load(std::memory_order_relaxed);
}

static int WorkerMain(void *param)
{
	std::default_random_engine generator;
	std::uniform_real_distribution<double> distribution;

	while (IsRunning())
	{
		double number = distribution(generator);

		RTL::WriteStdOutFormat("%f\n", number);
	}

	return 0;
}

static bool WaitForEOF()
{
	char buffer[256];

	while (true)
	{
		size_t length = 0;
		if (!RTL::ReadStdIn(buffer, sizeof buffer, &length))
		{
			return false;
		}

		if (length == 0 || Util::IsEOF(buffer[length-1]))
		{
			break;
		}
	}

	return true;
}

RTL_DEFINE_SHELL_PROGRAM(rgen)

int rgen_main(const char *args)
{
	SetRunning(true);

	RTL::Thread worker;
	worker.mainFunc = WorkerMain;

	if (!worker.start())
	{
		RTL::WriteStdOutFormat("rgen: Nelze vytvorit vlakno: %s\n", RTL::GetLastErrorMsg().c_str());
		return 1;
	}

	WaitForEOF();

	SetRunning(false);
	worker.join();

	return 0;
}
