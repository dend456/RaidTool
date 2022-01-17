#pragma once
#include <filesystem>

class settings
{
public:
	static inline std::filesystem::path settingsPath = "raidtool/settings.txt";
	static inline std::filesystem::path itemIconsPath = "raidtool/itemicons.txt";
	static inline FILE* logFile = nullptr;

	static inline bool openWithRaidWindow = false;

	static void save() noexcept;
	static void load() noexcept;
};