#include <cctype>
#include <cstring>

#include "path.h"

int Path::compare(const Path & other) const
{
	if (isRelative() && other.isAbsolute())
	{
		return -1;
	}
	else if (isAbsolute() && other.isRelative())
	{
		return 1;
	}

	if (m_parentCount < other.m_parentCount)
	{
		return -1;
	}
	else if (m_parentCount > other.m_parentCount)
	{
		return 1;
	}

	const size_t size = (m_path.size() <= other.m_path.size()) ? m_path.size() : other.m_path.size();

	for (size_t i = 0; i < size; i++)
	{
		int result = m_path[i].compare(other.m_path[i]);
		if (result != 0)
		{
			return result;
		}
	}

	return 0;
}

std::string Path::toString() const
{
	std::string result;

	if (isAbsolute())
	{
		result += m_diskLetter;
		result += ':';
		result += '\\';
	}
	else
	{
		for (unsigned int i = 0; i < m_parentCount; i++)
		{
			result += "..\\";
		}
	}

	if (!m_path.empty())
	{
		result += m_path[0];

		for (size_t i = 1; i < m_path.size(); i++)
		{
			result += '\\';
			result += m_path[i];
		}
	}

	return result;
}

size_t Path::toStringBuffer(char *buffer, size_t bufferSize) const
{
	const std::string result = toString();

	size_t length = result.length();
	if (length >= bufferSize)
	{
		length = bufferSize - 1;
	}

	std::memcpy(buffer, result.c_str(), length);
	buffer[length] = '\0';

	return length;
}

void Path::append(const Path & other)
{
	for (unsigned int i = 0; i < other.m_parentCount; i++)
	{
		if (!m_path.empty())
		{
			m_path.pop_back();
		}
		else if (isRelative())
		{
			m_parentCount++;
		}
		else
		{
			break;
		}
	}

	for (const std::string & component : other.m_path)
	{
		m_path.emplace_back(component);
	}
}

bool Path::makeAbsolute(const Path & base)
{
	if (isAbsolute() || !base.isAbsolute())
	{
		return false;
	}

	std::vector<std::string> newPath;
	newPath.reserve(base.m_path.size() + m_path.size());

	for (const std::string & component : base.m_path)
	{
		newPath.emplace_back(component);
	}

	for (const std::string & component : m_path)
	{
		newPath.emplace_back(component);
	}

	m_path = std::move(newPath);

	return true;
}

Path Path::Parse(const char *path)
{
	Path result;

	if (path == nullptr)
	{
		return result;
	}

	std::string component;
	for (size_t i = 0; path[i]; i++)
	{
		char ch = path[i];

		switch (ch)
		{
			case ':':
			{
				if (i == 1)
				{
					char diskLetter = path[i-1];

					if (std::isalnum(diskLetter))
					{
						result.m_diskLetter = std::toupper(diskLetter);
					}

					component.clear();
				}
				else
				{
					component += '_';  // nepodporovaný znak
				}
				break;
			}
			case '/':
			case '\\':
			{
				if (!component.empty())
				{
					result.m_path.emplace_back(std::move(component));
					component.clear();
				}
				break;
			}
			case '.':
			{
				auto IsSeparator = [](char ch) -> bool { return ch == '/' || ch == '\\' || ch == '\0'; };

				if (i > 0 && IsSeparator(path[i-1]))
				{
					char next = path[i+1];

					if (IsSeparator(next))  // cesta obsahuje "/./"
					{
						i++;
					}
					else if (next == '.' && IsSeparator(path[i+2]))  // cesta obsahuje "/../"
					{
						if (!result.m_path.empty())
						{
							result.m_path.pop_back();
						}
						else if (result.isRelative())
						{
							result.m_parentCount++;
						}

						i += 2;
					}
					else
					{
						component += '.';
						component += next;

						i++;
					}
				}
				else
				{
					component += '.';
				}
				break;
			}
			case ' ':
			case ',':
			case '+':
			case '-':
			case '_':
			case '!':
			case '(':
			case ')':
			case '[':
			case ']':
			case '~':
			{
				component += ch;
				break;
			}
			default:
			{
				component += std::isalnum(ch) ? ch : '_';  // nepodporovaný znak
				break;
			}
		}
	}

	if (!component.empty())
	{
		result.m_path.emplace_back(std::move(component));
	}

	return result;
}
