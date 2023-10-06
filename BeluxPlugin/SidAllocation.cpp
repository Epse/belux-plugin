#include "pch.h"
#include "SidAllocation.h"

#include <sstream>
#include <variant>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>

SidAllocation::SidAllocation()
{
	entries = std::make_unique<std::vector<SidEntry>>();
}

size_t SidAllocation::parse_string(const std::string& input) const
{
	std::istringstream iss(input);
	entries->clear();

	for (std::string line; std::getline(iss, line);)
	{
		if (auto maybe_entry = parse_line(line); maybe_entry.has_value())
			entries->push_back(maybe_entry.value());
	}

	return entries->size();
}

std::optional<SidEntry> SidAllocation::find(const std::string& adep, const std::string& exit_point,
                                           const std::string& ades, const int engine_count,
                                           const std::string& runway,
                                           const tm& now,
                                           const std::vector<std::string>& active_areas) const
{
	for (auto entry : *entries)
	{
		if (entry.adep != adep)
			continue;
		if (entry.exit_point != exit_point)
			continue;
		if (entry.rwy != runway)
			continue;
		if (entry.ades.empty() && !does_ades_match(entry.ades, ades))
			continue;
		if (engine_count != 4 && entry.aircraft_class == four_engined)
			continue;
		if (engine_count == 4 && entry.aircraft_class == non_four_engined)
			continue;
		if (!does_activation_match(entry.time_activation, now))
			continue;
		if (!entry.tsa.empty())
		{
			bool tsa_match = false;
			for (auto& area: active_areas)
			{
				if (entry.tsa.find(area) != std::string::npos)
				{
					tsa_match = true;
					break;
				}
			}

			if (tsa_match)
				continue;
		}

		return entry;
	}

	return {};
}

std::set<std::string> SidAllocation::sids_for_airport(const std::string& adep) const
{
	std::set<std::string> result;
	for (auto& entry : *entries)
	{
		if (entry.adep != adep)
			continue;

		result.insert(entry.sid);
	}

	return result;
}

std::set<std::string> SidAllocation::fixes_for_airport(const std::string& airport) const
{
	std::set<std::string> result;
	for (auto& entry : *entries)
	{
		if (entry.adep != airport)
			continue;

		result.insert(entry.exit_point);
	}

	return result;
}

std::optional<SidEntry> SidAllocation::parse_line(const std::string& line) const
{
	if (boost::algorithm::starts_with(line, "--"))
		return {}; // Ignore comments

	/*
	 * The file format contains 12 columns, separated by pipe characters.
	 * The columns are:
	 * P1, P2, C, From time, To time, SID, ADEP, RWY, CFL, ADES, TSA, comment
	 * these map to the SidEntry fields of:
	 * exit_point, none, aircraft_class, from_time, to_time, sid, adep, rwy, none, ades, tsa, none
	 * the `none` entries are unused columns.
	 */

	std::vector<std::string> columns;
	boost::split(columns, line, boost::is_any_of("|"));

	if (columns.size() != 12)
		return {};

	SidEntry entry = {
		boost::trim_copy(columns[5]),
		boost::trim_copy(columns[0]),
		static_cast<aircraft_class>(std::stoi(columns[2])),
		parse_time_activation(boost::trim_copy(columns[3]),
		                      boost::trim_copy(columns[4])),
		boost::trim_copy(columns[6]),
		boost::trim_copy(columns[9]),
		boost::trim_copy(columns[7]),
		boost::trim_copy(columns[10]),
	};

	return entry;
}

std::optional<TimeActivation> SidAllocation::parse_time_activation(const std::string& line_start,
                                                                   const std::string& line_end) const
{
	const auto parsed_start = parse_activation_time_line(line_start);
	const auto parsed_end = parse_activation_time_line(line_end);

	if (!parsed_start.has_value() || !parsed_end.has_value())
		return {};

	const bool has_weekdays = parsed_start.value().first && parsed_end.value().first;
	return TimeActivation{
		has_weekdays,
		parsed_start.value().second,
		parsed_end.value().second,
	};
}

/**
 * \brief Extracts a singular activation time entry
 * \param line An entry
 * \return bool indicating presence of weekday, tm for other data
 */
std::optional<std::pair<bool, tm>> SidAllocation::parse_activation_time_line(const std::string& line) const
{
	// Just a time is 4 characters,
	if (line.size() < 4)
		return {};

	int hours = 0;
	int minutes = 0;
	int wday = 0;
	bool has_wday = false;
	std::string time = line;

	const auto space_pos = line.find(' ');
	// If a space is found, there _has_ to be content after it, due to trimmed
	if (space_pos != std::string::npos)
	{
		const std::string day_str = line.substr(0, space_pos);
		time = line.substr(space_pos + 1);

		has_wday = true;
		// Weird C wday indexing...
		if (day_str == "SUNDAY")
			wday = 0;
		else if (day_str == "MONDAY")
			wday = 1;
		else if (day_str == "TUESDAY")
			wday = 2;
		else if (day_str == "WEDNESDAY")
			wday = 3;
		else if (day_str == "THURSDAY")
			wday = 4;
		else if (day_str == "FRIDAY")
			wday = 5;
		else if (day_str == "SATURDAY")
			wday = 6;
		else
			has_wday = false;
	}

	const int numeric_time = std::stoi(time);
	hours = numeric_time / 100;
	minutes = numeric_time % 100;

	tm tm{};
	tm.tm_hour = hours;
	tm.tm_min = minutes;
	tm.tm_wday = wday;

	return std::pair{
		has_wday,
		tm,
	};
}

bool SidAllocation::does_ades_match(const std::string& reference, const std::string& in) const
{
	if (reference.empty())
		return true;

	if (reference.at(0) == '=')
		return reference.substr(1, 4) == in;

	if (reference.at(0) == '*')
		return reference.substr(1, 4) != in;

	return false;
}

bool SidAllocation::does_activation_match(const std::optional<TimeActivation>& reference, const tm& now)
{
	if (!reference.has_value())
		return true; // Always active

	const auto start = reference.value().tm_start;
	const auto end = reference.value().tm_end;

	const bool past_start_time = now.tm_hour > start.tm_hour
		|| (now.tm_hour == start.tm_hour && now.tm_min >= start.tm_min);
	const bool before_end_time = now.tm_hour < end.tm_hour
		|| (now.tm_hour == end.tm_hour && now.tm_min <= end.tm_min);


	if (reference.value().has_weekdays && start.tm_wday != end.tm_wday)
	{
		if (now.tm_wday == start.tm_wday)
			return past_start_time;

		if (now.tm_wday == end.tm_wday)
			return before_end_time;

		if (start.tm_wday < end.tm_wday)
		{
			return now.tm_wday > start.tm_wday && now.tm_wday < end.tm_wday;
		}

		return now.tm_wday > start.tm_wday || now.tm_wday < end.tm_wday;
	}

	// They start and end on the same day and today is not that day.
	if (reference.value().has_weekdays && now.tm_wday != start.tm_wday)
		return false;

	// Do the time
	// Simplified, I should in theory also compare the minutes.
	if (start.tm_hour > end.tm_hour)
		return past_start_time || before_end_time;

	return past_start_time && before_end_time;
}
