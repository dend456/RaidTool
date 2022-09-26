#include <fmt/core.h>
#include <fstream>
#include <sstream>
#include "settings.h"

void settings::save() noexcept
{
	try
	{
		std::ofstream out(settings::settingsPath);
		if (!out.good())
		{
			fmt::print(settings::logFile, "Error opening settings file. {}\n", settings::settingsPath.string());
			return;
		}

		out << fmt::format(
			"openWithRaidWindow {:d}\n"
			, settings::openWithRaidWindow);

		out.close();
	}
	catch (const std::exception& e)
	{
		fmt::print(settings::logFile, "Error saving settings: {}\n", std::string(e.what()));
	}
}

void settings::load() noexcept
{
	try
	{
		std::ifstream in(settings::settingsPath);
		if (!in.good())
		{
			fmt::print(settings::logFile, "Error opening settings file. {}\n", settings::settingsPath.string());
			return;
		}

		std::stringstream ss;
		ss << in.rdbuf();
		in.close();

		std::string item;
		std::getline(ss, item, ' ');
		if (item == "openWithRaidWindow")
		{
			std::getline(ss, item, '\n');
			settings::openWithRaidWindow = std::stoi(item);
		}
	}
	catch (const std::exception& e)
	{
		fmt::print(settings::logFile, "Error loading settings: {}\n", std::string(e.what()));
	}
}