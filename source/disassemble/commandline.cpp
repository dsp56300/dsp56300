#include"commandline.h"

#include <cmath>

CommandLine* CommandLine::m_instance = nullptr;

CommandLine::CommandLine(const int _argc, char* _argv[])
{
	m_instance = this;

	std::string currentArg;

	for (int i = 1; i < _argc; ++i)
	{
		std::string arg(_argv[i]);
		if (arg.empty())
			continue;

		if (arg[0] == '-')
		{
			if (!currentArg.empty())
				m_args.insert(currentArg);

			currentArg = arg.substr(1);
		}
		else
		{
			if (!currentArg.empty())
			{
				m_argsWithValues.insert(std::make_pair(currentArg, arg));
				currentArg.clear();
			}
			else
			{
				m_args.insert(arg);
			}
		}
	}

	if (!currentArg.empty())
		m_args.insert(currentArg);
}

std::string CommandLine::tryGet(const std::string& _key, const std::string& _value) const
{
	const auto it = m_argsWithValues.find(_key);
	if (it == m_argsWithValues.end())
		return _value;
	return it->second;
}

std::string CommandLine::get(const std::string& _key) const
{
	const auto it = m_argsWithValues.find(_key);
	if (it == m_argsWithValues.end())
	{
		const std::string msg = "ERROR: command line argument " + _key + " has no value";
		LOG(msg)
		throw std::runtime_error(msg.c_str());
	}
	return it->second;
}

float CommandLine::getFloat(const std::string& _key) const
{
	const std::string stringResult = get(_key);
	const double result = atof(stringResult.c_str());
	if (std::isinf(result) || std::isnan(result))
	{
		const std::string msg = "ERROR: invalid value " + stringResult + " for argument " + _key;
		LOG(msg)
		throw std::runtime_error(msg.c_str());
	}
	return static_cast<float>(result);
}

int CommandLine::getInt(const std::string& _key) const
{
	const std::string stringResult = get(_key);
	return atoi(stringResult.c_str());
}

bool CommandLine::contains(const std::string& _key) const
{
	return m_args.find(_key) != m_args.end() || m_argsWithValues.find(_key) != m_argsWithValues.end();
}
