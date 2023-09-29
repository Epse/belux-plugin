#include "LaraParser.h"

#include <sstream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>

LaraParser::LaraParser()
{
	entries = std::make_unique<std::vector<LaraEntry>>();
}

size_t LaraParser::parse_string(const std::string& input) const
{
	/*
	 * Example lines:
	 * Name:start_date:end_date:weekdays:start_time:end_time:lower_alt:upper_alt:comment
	 * EBR03:0102:1224:12345:0001:2359:
	 * EBR03:0102:1224:6:0001:0537:
	 * EBR41A:0103:0320:12345:0700:2300:0:3800:/MIL
     *
	 * The weekdays list the applicable weekdays, monday is 1, sunday is 7 somehow
	 * We will mostly bother with name, start and end date and time and weekdays
	 */

	std::istringstream iss(input);
	entries->clear();

	for (std::string line; std::getline(iss, line);)
	{
		if (auto maybe_entry = parse_line(line); maybe_entry.has_value())
			entries->push_back(maybe_entry.value());
	}

	return entries->size();
}

std::vector<std::string> LaraParser::get_active(const tm& now) const
{
	std::vector<std::string> out;
	for (const auto& entry: *entries)
	{
		if (is_entry_active(entry, now))
			out.emplace_back(entry.name);
	}
	return out;
}

std::optional<LaraEntry> LaraParser::parse_line(const std::string& line)
{
	std::vector<std::string> columns;
	boost::split(columns, line, boost::is_any_of(":"));

	if (columns.size() < 6)
		return {};

	const uint32_t full_start_date = std::stoi(columns[1]);
	const uint32_t full_end_date = std::stoi(columns[2]);
	const uint32_t full_start_time = std::stoi(columns[4]);
	const uint32_t full_end_time = std::stoi(columns[5]);

	const auto start_month = static_cast<uint8_t>(full_start_date / 100);
	const auto start_date = static_cast<uint8_t>(full_start_date % 100);
	const auto start_hour = static_cast<uint8_t>(full_start_time / 100);
	const auto start_minute = static_cast<uint8_t>(full_start_time % 100);
	const auto end_month = static_cast<uint8_t>(full_end_date / 100);
	const auto end_date = static_cast<uint8_t>(full_end_date % 100);
	const auto end_hour = static_cast<uint8_t>(full_end_time / 100);
	const auto end_minute = static_cast<uint8_t>(full_end_time % 100);

	LaraEntry entry = {
		columns[0],
		start_month,
		start_date,
		start_hour,
		start_minute,
		end_month,
		end_date,
		end_hour,
		end_minute,
		columns[3],
	};

	return entry;
}

bool LaraParser::is_active(const std::string& area, const tm& now) const
{
	for (const auto& entry : *entries)
	{
		if (entry.name != area)
			continue;

		if (is_entry_active(entry, now))
			return true;
	}

	return false; // Nothing found
}

bool LaraParser::is_entry_active(const LaraEntry& entry, const tm& now)
{
	// TODO: some buffer time perhaps?
	// Map sunday to 7 instead of 0 and make it a string
	const std::string weekday = std::to_string(now.tm_wday ? now.tm_wday > 0 : 7);
	if (entry.weekdays.find(weekday) == std::string::npos)
		return false; // Incorrect weekday

	// Check between start and end date / month, careful cuz of weird month indexing
	const int sane_month = now.tm_mon + 1;
	if (entry.start_month <= entry.end_month)
	{
		if (sane_month < entry.start_month || sane_month > entry.end_month)
			return false;

		if (sane_month == entry.start_month && now.tm_mday < entry.start_date)
			return false;

		if (sane_month == entry.end_month && now.tm_mday > entry.end_date)
			return false;

		// We're now in the valid zone and can return false onto time checks
	}
	else
	{
		// start_month > end_month, so we gotta be larger than start or smaller than end
		// Or, we return false the loop if we are before start but after end, respecting dates
		if (sane_month < entry.start_month && sane_month > entry.end_month)
			return false;

		// Now, we should handle the case where we are in the start month
		if (sane_month >= entry.start_month && now.tm_mday < entry.start_date)
			return false;
		// The case where we are in the end month
		if (sane_month <= entry.end_month && now.tm_mday > entry.end_date)
			return false;
		// We're good, check the time
	}

	// Check between start and end time
	const bool past_start_time = now.tm_hour > entry.start_hour || (now.tm_hour == entry.start_hour && now.tm_min >=
		entry.start_minute);
	const bool before_end_time = now.tm_hour < entry.end_hour || (now.tm_hour == entry.end_hour && now.tm_min < entry.
		end_minute);
	if (entry.start_hour <= entry.end_hour
		&& !(past_start_time && before_end_time))
		return false;
	if (entry.start_hour > entry.end_hour
		&& !(past_start_time || before_end_time))
		return false;

	return true;
}
