#pragma once
#include <string>
#include <vector>
#include <optional>
#include <memory>

struct LaraEntry
{
	std::string name;
	// Actual calendar months, rather than the cursed 0-indexed ones in tm
	uint8_t start_month;
	// 1-31
	uint8_t start_date;
	// 0-24
	uint8_t start_hour;
	// 0-59
	uint8_t start_minute;
	uint8_t end_month;
	uint8_t end_date;
	uint8_t end_hour;
	uint8_t end_minute;
	/*
	 * Stored as a weekday to do simpler checks.
	 * Monday is 1, sunday is 7.
	 */
	std::string weekdays; // We store this as a string to just do silly simple checks
};

class LaraParser
{
public:
	LaraParser();
	/**
	 * \brief Parses a TopSkyAreasManualAct file.
	 * \param input String of a TopSkyAreasManualAct file.
	 * \return Amount of parsed entries.
	 */
	size_t parse_string(const std::string& input) const;
	std::vector<std::string> get_active(const tm& now) const;
	bool is_active(const std::string& area, const tm& now) const;
private:
	static bool is_entry_active(const LaraEntry& entry, const tm& now);
	std::unique_ptr<std::vector<LaraEntry>> entries;
	std::optional<LaraEntry> static parse_line(const std::string& line);
};

