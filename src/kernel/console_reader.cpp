#include <atomic>
#include <thread>
#include <new>

#include "../api/hal.h"

#include "console_reader.h"

static ConsoleReader *g_pInstance;
static std::atomic<bool> g_isRunning;

static void HALWriteChar(char ch)
{
	kiv_hal::TRegisters context;
	context.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_Control_Char);
	context.rdx.l = ch;

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, context);
}

static bool HALReadChar(char & ch)
{
	kiv_hal::TRegisters context;
	context.rax.h = static_cast<uint8_t>(kiv_hal::NKeyboard::Read_Char);

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Keyboard, context);

	ch = context.rax.l;

	if (!context.flags.non_zero && ch != static_cast<char>(kiv_hal::NControl_Codes::EOT))
	{
		return false;
	}

	return true;
}

static bool IsRunning()
{
	return g_isRunning.load(std::memory_order_relaxed);
}

static void SetRunning(bool isRunning)
{
	g_isRunning.store(isRunning, std::memory_order_relaxed);
}

static void ReaderWorker()
{
	std::string line;

	while (IsRunning())
	{
		char ch;
		if (!HALReadChar(ch))
		{
			SetRunning(false);
			break;
		}

		bool isLineComplete = false;

		switch (ch)
		{
			case '\0':
			{
				isLineComplete = true;
				break;
			}
			case '\b':  // Backspace
			{
				if (!line.empty())
				{
					line.pop_back();
					HALWriteChar('\b');
				}
				break;
			}
			case '\n':
			{
				break;
			}
			case '\r':  // Enter
			{
				line += '\n';
				HALWriteChar('\n');
				isLineComplete = true;
				break;
			}
			case 3:     // Ctrl + C
			case 4:     // Ctrl + D
			case 26:    // Ctrl + Z
			case '\t':  // Tabulátor
			{
				line += ch;
				isLineComplete = true;
				break;
			}
			default:
			{
				line += ch;
				HALWriteChar(ch);
				break;
			}
		}

		if (isLineComplete)
		{
			g_pInstance->pushLine(std::move(line));
			line.clear();
		}
	}
}

void ConsoleReader::readLine(char *buffer, size_t bufferSize, size_t *pRead)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	while (m_lineQueue.empty() && IsRunning())
	{
		m_cv.wait(lock);
	}

	size_t length = 0;

	if (!m_lineQueue.empty())
	{
		std::string & line = m_lineQueue.front();
		length = (line.length() < bufferSize) ? line.length() : bufferSize;

		line.copy(buffer, length);

		if (length < line.length())
		{
			line.erase(0, length);
		}
		else
		{
			m_lineQueue.pop();
		}
	}

	if (pRead)
	{
		(*pRead) = length;
	}
}

void ConsoleReader::pushLine(std::string && line)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_lineQueue.push(std::move(line));

	lock.unlock();
	m_cv.notify_one();
}

void ConsoleReader::close()
{
	SetRunning(false);

	m_cv.notify_all();
}

ConsoleReader & ConsoleReader::GetInstance()
{
	if (g_pInstance == nullptr)
	{
		g_pInstance = new ConsoleReader;

		SetRunning(true);

		std::thread(ReaderWorker).detach();
	}

	return *g_pInstance;
}
