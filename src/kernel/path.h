#pragma once

#include <string>
#include <vector>

#include "types.h"

class Path
{
	char m_diskLetter = '\0';
	unsigned int m_parentCount = 0;  // počet ".." před relativní cestou
	std::vector<std::string> m_path;

public:
	Path() = default;

	Path(const Path &) = default;

	Path(Path && other)
	: m_diskLetter(other.m_diskLetter),
	  m_parentCount(other.m_parentCount),
	  m_path(std::move(other.m_path))
	{
		other.clear();
	}

	Path & operator=(const Path &) = default;

	Path & operator=(Path && other)
	{
		if (this != &other)
		{
			m_diskLetter = other.m_diskLetter;
			m_parentCount = other.m_parentCount;
			m_path = std::move(other.m_path);

			other.clear();
		}

		return *this;
	}

	void clear()
	{
		m_diskLetter = '\0';
		m_parentCount = 0;
		m_path.clear();
	}

	explicit operator bool() const
	{
		return !isEmpty();
	}

	bool isEmpty() const
	{
		return !hasDiskLetter() && m_parentCount == 0 && m_path.empty();
	}

	bool isRelative() const
	{
		return !isAbsolute();
	}

	bool isAbsolute() const
	{
		return hasDiskLetter() && m_parentCount == 0;
	}

	bool hasDiskLetter() const
	{
		return m_diskLetter != '\0';
	}

	char getDiskLetter() const
	{
		return m_diskLetter;
	}

	unsigned int getParentCount() const
	{
		return m_parentCount;
	}

	size_t getComponentCount() const
	{
		return m_path.size();
	}

	const std::vector<std::string> & get() const
	{
		return m_path;
	}

	const std::string & operator[](size_t index) const
	{
		return m_path[index];
	}

	int compare(const Path & other) const;

	std::string toString() const;

	size_t toStringBuffer(char *buffer, size_t bufferSize) const;

	void append(const Path & other);

	Path & operator+=(const Path & other)
	{
		append(other);
		return *this;
	}

	bool makeAbsolute(const Path & base);

	static Path Parse(const char *path);

	static Path Parse(const std::string & path)
	{
		return Parse(path.c_str());
	}
};

inline bool operator==(const Path & a, const Path & b)
{
	return a.compare(b) == 0;
}

inline bool operator!=(const Path & a, const Path & b)
{
	return a.compare(b) != 0;
}

inline bool operator<(const Path & a, const Path & b)
{
	return a.compare(b) < 0;
}

inline bool operator>(const Path & a, const Path & b)
{
	return a.compare(b) > 0;
}

inline bool operator<=(const Path & a, const Path & b)
{
	return a.compare(b) <= 0;
}

inline bool operator>=(const Path & a, const Path & b)
{
	return a.compare(b) >= 0;
}
