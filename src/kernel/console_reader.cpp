#include <thread>
#include <new>

#include "../api/hal.h"

#include "console_reader.h"

class ReaderCountGuard
{
	ConsoleReader *m_self;

public:
	ReaderCountGuard(ConsoleReader *self)
	: m_self(self)
	{
		m_self->m_readerCount++;
	}

	~ReaderCountGuard()
	{
		m_self->m_readerCount--;
	}
};

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

static bool HALReadLine(std::string & line)
{
	while (true)
	{
		char ch;
		if (!HALReadChar(ch))
		{
			return false;
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
			break;
		}
	}

	return true;
}

void ConsoleReader::workerLoop()
{
	std::string line;

	while (waitForReader())
	{
		if (!HALReadLine(line))
		{
			setOpen(false);
			break;
		}

		pushLine(std::move(line));
		line.clear();
	}
}

bool ConsoleReader::waitForReader()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	while (isOpen())
	{
		m_workerCV.wait(lock);

		if (getReaderCount() > 0)
		{
			return true;
		}
	}

	return false;
}

void ConsoleReader::readLine(char *buffer, size_t bufferSize, size_t *pRead)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	if (m_lineQueue.empty() && isOpen())
	{
		ReaderCountGuard readerCountGuard(this);

		// potřebujeme něco přečíst ze vstupu, takže vzbudíme čtecí vlákno
		m_workerCV.notify_one();

		do
		{
			m_readerCV.wait(lock);
		}
		while (m_lineQueue.empty() && isOpen());
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
	m_readerCV.notify_one();
}

void ConsoleReader::close()
{
	if (isOpen())
	{
		setOpen(false);

		m_readerCV.notify_all();
		m_workerCV.notify_all();
	}
}

static ConsoleReader *g_pInstance;

ConsoleReader & ConsoleReader::GetInstance()
{
	if (g_pInstance == nullptr)
	{
		g_pInstance = new ConsoleReader;

		g_pInstance->setOpen(true);

		std::thread(&workerLoop, g_pInstance).detach();
	}

	return *g_pInstance;
}
