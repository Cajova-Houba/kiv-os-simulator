#include <cstring>  // std::memmove

#include "rtl.h"

class ShellParser
{
public:
	struct Result
	{
		std::string inputFileName;
		std::string outputFileName;
		bool truncateOutputFile;             // ">" = true, ">>" = false
		std::vector<RTL::Process> commands;  // název příkazu a jeho parametry připravené rovnou v RTL::Process
	};

private:
	enum struct EState
	{
		COMMAND, ARGS, INPUT_FILE, OUTPUT_FILE, DONE, PARSE_ERROR
	};

	char m_buffer[4096];
	size_t m_length = 0;
	bool m_isInputEmpty = false;

public:
	ShellParser() = default;

	bool isInputEmpty() const
	{
		return m_isInputEmpty;
	}

	bool readInput(Result & result)
	{
		if (m_isInputEmpty)
		{
			return false;
		}

		EState state = EState::COMMAND;
		size_t cmdIndex = 0;
		int quotationMark = 0;
		char lastChar = '\0';

		result.commands.resize(1);

		std::string *pCurrentToken = &result.commands[0].name;

		// dokud nebude načten jeden celý řádek
		while (lastChar != '\n' && lastChar != 3 && !m_isInputEmpty)
		{
			// v bufferu už možná je načtena část aktuálního řádku
			if (m_length == 0)
			{
				if (!RTL::ReadStdIn(m_buffer, sizeof m_buffer, &m_length) || m_length == 0)
				{
					m_isInputEmpty = true;
					break;
				}

				char endChar = m_buffer[m_length-1];

				if (endChar == 4 || endChar == 26)  // Ctrl + D nebo Ctrl + Z
				{
					m_isInputEmpty = true;
				}
			}

			size_t pos = 0;
			for (; pos < m_length; pos++)
			{
				char ch = m_buffer[pos];

				if (state == EState::DONE)
				{
					// úspěšně načten celý řádek
					break;
				}
				else if (state == EState::PARSE_ERROR)
				{
					if (lastChar == '\n')
					{
						// načten celý chybný řádek
						break;
					}

					lastChar = ch;

					continue;
				}

				bool resetLastChar = false;

				switch (ch)
				{
					case ' ':
					case '\t':
					{
						if (quotationMark || state == EState::ARGS)
						{
							(*pCurrentToken) += ch;
						}
						else if (!pCurrentToken->empty())
						{
							state = EState::ARGS;
							pCurrentToken = &result.commands[cmdIndex].cmdLine;
						}
						break;
					}
					case '^':
					{
						if (lastChar == '^')
						{
							(*pCurrentToken) += ch;
							resetLastChar = true;
						}
						break;
					}
					case '|':
					{
						if (lastChar == '^')
						{
							(*pCurrentToken) += ch;
						}
						else if (!result.outputFileName.empty())
						{
							RTL::WriteStdOut("shell: Neplatne presmerovani\n");
							state = EState::PARSE_ERROR;
						}
						else if (state == EState::INPUT_FILE && result.inputFileName.empty())
						{
							RTL::WriteStdOut("shell: Chybi vstupni soubor\n");
							state = EState::PARSE_ERROR;
						}
						else
						{
							if (!result.commands[cmdIndex].name.empty())
							{
								cmdIndex++;
								result.commands.emplace_back();
							}

							state = EState::COMMAND;
							pCurrentToken = &result.commands[cmdIndex].name;
						}
						break;
					}
					case '>':
					{
						if (lastChar == '^')
						{
							(*pCurrentToken) += ch;
						}
						else if (lastChar == '>' && state == EState::OUTPUT_FILE)
						{
							result.truncateOutputFile = false;
							resetLastChar = true;
						}
						else if (state != EState::COMMAND && state != EState::ARGS)
						{
							RTL::WriteStdOut("shell: Neplatne presmerovani\n");
							state = EState::PARSE_ERROR;
						}
						else
						{
							state = EState::OUTPUT_FILE;
							result.truncateOutputFile = true;
							pCurrentToken = &result.outputFileName;
							pCurrentToken->clear();
						}
						break;
					}
					case '<':
					{
						if (lastChar == '^')
						{
							(*pCurrentToken) += ch;
						}
						else if (cmdIndex != 0 || (state != EState::COMMAND && state != EState::ARGS))
						{
							RTL::WriteStdOut("shell: Neplatne presmerovani\n");
							state = EState::PARSE_ERROR;
						}
						else
						{
							state = EState::INPUT_FILE;
							pCurrentToken = &result.inputFileName;
							pCurrentToken->clear();
						}
						break;
					}
					case '@':
					{
						if (pCurrentToken->empty() && state == EState::COMMAND)
						{
							// dávkové soubory nejsou podporovány
							// takže "@" na začátku příkazu ignorujeme
						}
						else
						{
							(*pCurrentToken) += ch;
						}
						break;
					}
					case '\'':
					{
						if (quotationMark == 0 && state != EState::ARGS)
						{
							quotationMark = 1;
						}
						else if (quotationMark == 1 && state != EState::ARGS)
						{
							quotationMark = 0;
						}
						else
						{
							(*pCurrentToken) += ch;
						}
						break;
					}
					case '\"':
					{
						if (quotationMark == 0 && state != EState::ARGS)
						{
							quotationMark = 2;
						}
						else if (quotationMark == 2 && state != EState::ARGS)
						{
							quotationMark = 0;
						}
						else
						{
							(*pCurrentToken) += ch;
						}
						break;
					}
					case '\n':
					{
						if (state == EState::INPUT_FILE && result.inputFileName.empty())
						{
							RTL::WriteStdOut("shell: Chybi vstupni soubor\n");
							state = EState::PARSE_ERROR;
						}
						else if (state == EState::OUTPUT_FILE && result.outputFileName.empty())
						{
							RTL::WriteStdOut("shell: Chybi vystupni soubor\n");
							state = EState::PARSE_ERROR;
						}
						else
						{
							state = EState::DONE;
						}
						break;
					}
					default:
					{
						if (ch > 32)  // žádné kontrolní znaky a mezery
						{
							(*pCurrentToken) += ch;
						}
						break;
					}
				}

				lastChar = (resetLastChar) ? '\0' : ch;
			}

			if (pos < m_length)
			{
				// přesun načtené části následujícího řádku na začátek bufferu
				std::memmove(m_buffer, m_buffer + pos, m_length - pos);
			}

			m_length -= pos;
		}

		if (result.commands[cmdIndex].name.empty())
		{
			result.commands.pop_back();
		}

		if (lastChar == 3 || lastChar == 4 || lastChar == 26)
		{
			RTL::WriteStdOut("\n");
		}

		return state == EState::DONE;
	}
};

class Shell
{
	bool m_isRunning = true;
	bool m_isEchoOn = true;
	std::string m_prompt;
	ShellParser m_parser;

	static Shell *s_pInstance;

	void updatePrompt()
	{
		m_prompt = RTL::GetWorkingDirectory() + '>';
	}

	bool prepareFiles(const ShellParser::Result & result, RTL::File & inputFile, RTL::File & outputFile);
	bool executeCommands(std::vector<RTL::Process> & commands, RTL::Handle input, RTL::Handle output);
	bool handleInternalCommand(const std::string & command, const std::string & args, RTL::Handle input, RTL::Handle output);

public:
	Shell()
	{
		s_pInstance = this;

		updatePrompt();
	}

	~Shell()
	{
		s_pInstance = nullptr;
	}

	int run()
	{
		while (m_isRunning)
		{
			if (m_isEchoOn)
			{
				RTL::WriteStdOut(m_prompt);
			}

			ShellParser::Result result;

			if (!m_parser.readInput(result))
			{
				if (m_parser.isInputEmpty())
				{
					break;
				}

				continue;
			}

			RTL::File inputFile;
			RTL::File outputFile;

			if (!prepareFiles(result, inputFile, outputFile))
			{
				continue;
			}

			RTL::Handle inputHandle = (inputFile) ? inputFile.handle : RTL::GetStdInHandle();
			RTL::Handle outputHandle = (outputFile) ? outputFile.handle : RTL::GetStdOutHandle();

			if (!executeCommands(result.commands, inputHandle, outputHandle))
			{
				continue;
			}
		}

		return 0;
	}

	static void SignalHandler(RTL::Signal signal)
	{
		if (s_pInstance && signal == RTL::Signal::TERMINATE)
		{
			s_pInstance->m_isRunning = false;
		}
	}
};

Shell *Shell::s_pInstance;

bool Shell::prepareFiles(const ShellParser::Result & result, RTL::File & inputFile, RTL::File & outputFile)
{
	if (!result.inputFileName.empty())
	{
		const char *fileName = result.inputFileName.c_str();

		if (!inputFile.open(fileName, true))  // pouze pro čtení
		{
			RTL::WriteStdOutFormat("shell: %s: %s\n", fileName, RTL::GetLastErrorMsg().c_str());
			return false;
		}
	}

	if (!result.outputFileName.empty())
	{
		const char *fileName = result.outputFileName.c_str();

		if (!outputFile.create(fileName))
		{
			RTL::WriteStdOutFormat("shell: %s: %s\n", fileName, RTL::GetLastErrorMsg().c_str());
			return false;
		}

		if (result.truncateOutputFile)
		{
			outputFile.setSize(0);
		}
	}

	return true;
}

bool Shell::executeCommands(std::vector<RTL::Process> & commands, RTL::Handle input, RTL::Handle output)
{
	const size_t commandCount = commands.size();

	if (commandCount == 0)
	{
		return true;
	}

	const size_t pipeCount = commandCount - 1;

	std::vector<RTL::Pipe> pipes;
	pipes.reserve(pipeCount);

	for (size_t i = 0; i < pipeCount; i++)
	{
		pipes.emplace_back(RTL::CreatePipe());
		if (!pipes.back())
		{
			RTL::WriteStdOutFormat("shell: Nelze vytvorit rouru: %s\n", RTL::GetLastErrorMsg().c_str());
			return false;
		}
	}

	for (size_t i = commandCount; i-- > 0;)
	{
		RTL::Process & process = commands[i];

		process.stdIn = (i > 0) ? pipes[i-1].readEnd : input;
		process.stdOut = (i < pipeCount) ? pipes[i].writeEnd : output;

		if (handleInternalCommand(process.name, process.cmdLine, process.stdIn, process.stdOut))
		{
			if (i > 0)
			{
				pipes[i-1].closeReadEnd();
			}

			if (i < pipeCount)
			{
				pipes[i].closeWriteEnd();
			}
		}
		else if (!process.start())
		{
			if (RTL::GetLastError() == RTL::Error::FILE_NOT_FOUND)
			{
				RTL::WriteStdOutFormat("shell: %s: Prikaz nenalezen\n", process.name.c_str());
			}
			else
			{
				RTL::WriteStdOutFormat("shell: Nelze vytvorit proces: %s\n", RTL::GetLastErrorMsg().c_str());
			}

			return false;
		}
	}

	std::vector<RTL::Handle> processHandles;
	processHandles.reserve(commandCount);

	for (size_t i = 0; i < commandCount; i++)
	{
		const RTL::Handle handle = commands[i].handle;

		if (handle)
		{
			processHandles.push_back(handle);
		}
	}

	while (processHandles.size() > 0)
	{
		const int index = RTL::WaitForMultiple(processHandles);
		if (index < 0)
		{
			RTL::WriteStdOutFormat("shell: Chyba pri cekani na proces: %s\n", RTL::GetLastErrorMsg().c_str());
			return false;
		}

		const RTL::Handle handle = processHandles[index];

		for (size_t i = 0; i < commandCount; i++)
		{
			if (commands[i].handle == handle)
			{
				if (i > 0)
				{
					pipes[i-1].closeReadEnd();
				}

				if (i < pipeCount)
				{
					pipes[i].closeWriteEnd();
				}

				break;
			}
		}

		processHandles.erase(processHandles.begin() + index);
	}

	return true;
}

bool Shell::handleInternalCommand(const std::string & command, const std::string & args, RTL::Handle input, RTL::Handle output)
{
	if (command.length() == 2 && command[1] == ':')
	{
		if (!RTL::SetWorkingDirectory(command))
		{
			RTL::WriteFileFormat(output, "shell: '%s': %s\n", command.c_str(), RTL::GetLastErrorMsg().c_str());
		}

		updatePrompt();
	}
	else if (command == "cd")
	{
		if (!RTL::SetWorkingDirectory(args))
		{
			RTL::WriteFileFormat(output, "shell: cd: %s: %s\n", args.c_str(), RTL::GetLastErrorMsg().c_str());
		}

		updatePrompt();
	}
	else if (command == "exit")
	{
		m_isRunning = false;
	}
	else if (command == "echo" && args == "off")
	{
		m_isEchoOn = false;
	}
	else if (command == "echo" && args == "on")
	{
		m_isEchoOn = true;
	}
	else
	{
		return false;
	}

	return true;
}


// ============================================================================

RTL_DEFINE_SHELL_PROGRAM(shell)

int shell_main(const char *args)
{
	Shell shell;

	RTL::SetupSignals(Shell::SignalHandler, RTL::SignalBits::TERMINATE);

	return shell.run();
}
