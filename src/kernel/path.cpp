#include <cctype>
#include <cstring>
#include <algorithm>

#include "path.h"

int Path::compare(const Path & other) const
{
	if (isRelative() != other.isRelative())
	{
		return (isRelative()) ? -1 : 1;
	}

	if (isAbsolute())
	{
		if (m_diskLetter != other.m_diskLetter)
		{
			return (m_diskLetter < other.m_diskLetter) ? -1 : 1;
		}
	}
	else
	{
		if (m_parentCount != other.m_parentCount)
		{
			return (m_parentCount < other.m_parentCount) ? -1 : 1;
		}
	}

	const size_t componentCount = std::min(m_path.size(), other.m_path.size());

	for (size_t i = 0; i < componentCount; i++)
	{
		int result = m_path[i].compare(other.m_path[i]);
		if (result != 0)
		{
			return result;
		}
	}

	if (m_path.size() != other.m_path.size())
	{
		return (m_path.size() < other.m_path.size()) ? -1 : 1;
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

	const size_t baseComponentCount = (m_parentCount < base.m_path.size()) ? base.m_path.size() - m_parentCount : 0;

	std::vector<std::string> newPath;
	newPath.reserve(baseComponentCount + m_path.size());

	for (size_t i = 0; i < baseComponentCount; i++)
	{
		newPath.emplace_back(base.m_path[i]);
	}

	for (size_t i = 0; i < m_path.size(); i++)
	{
		newPath.emplace_back(std::move(m_path[i]));
	}

	m_diskLetter = base.m_diskLetter;
	m_parentCount = 0;
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
			case '\'':
			case '\"':
			{
				break;
			}
			case '.':
			{
				auto IsSeparator = [](char ch) -> bool { return ch == '/' || ch == '\\' || ch == '\0'; };

				if (i == 0 || IsSeparator(path[i-1]))
				{
					char next = path[i+1];

					if (!IsSeparator(next))  // "."
					{
						if (next == '.' && IsSeparator(path[i+2]))  // ".."
						{
							if (!result.m_path.empty())
							{
								result.m_path.pop_back();
							}
							else if (result.isRelative())
							{
								result.m_parentCount++;
							}
						}
						else
						{
							component += '.';
							component += next;
						}

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
