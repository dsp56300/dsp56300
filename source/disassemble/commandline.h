#pragma once

#include "../dsp56kEmu/logging.h"

#include <string>
#include <map>
#include <set>
#include <cassert>

class CommandLine
{
public:
	CommandLine(int _argc, char* _argv[]);

	static CommandLine& getInstance()
	{
		assert(m_instance);
		return *m_instance;
	}

	std::string tryGet(const std::string& _key, const std::string& _value = std::string()) const;

	std::string get(const std::string& _key) const;

	float getFloat(const std::string& _key) const;

	int getInt(const std::string& _key) const;

	bool contains(const std::string& _key) const;

private:
	std::map<std::string, std::string> m_argsWithValues;
	std::set<std::string> m_args;
	static CommandLine* m_instance;
};
