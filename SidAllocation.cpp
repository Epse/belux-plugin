#include "pch.h"
#include "SidAllocation.h"

#include <sstream>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>

SidAllocation::SidAllocation()
{
	entries = new std::vector<SidEntry>();
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
                                            const std::string& runway) const
{
	for (auto i = entries->begin(); i != entries->end(); ++i)
	{
		auto entry = *i;
		if (entry.adep != adep)
			continue;
		if (entry.exit_point != exit_point)
			continue;
		if (entry.ades.empty() && !does_ades_match(entry.ades, ades))
			continue;
		if (engine_count == 4 && entry.aircraft_class == 2)
			continue;
		if (engine_count != 4 && entry.aircraft_class == 3)
			continue;
		if (entry.rwy != runway)
			continue;

		// TODO: TSA status, time
		return entry;
	}

	return {};
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
	 * exit_point, none, aircraft_class, from_time, to_time, sid, adep, rwy, cfl, ades, tsa, none
	 * the `none` entries are unused columns.
	 */

	std::vector<std::string> columns;
	boost::split(columns, line, boost::is_any_of("|"));

	if (columns.size() != 12)
		return {};

	 SidEntry entry = {
		boost::trim_copy(columns[5]),
		boost::trim_copy(columns[0]),
		static_cast<uint8_t>(std::stoi(columns[2])),
		boost::trim_copy(columns[3]),
		boost::trim_copy(columns[4]),
		boost::trim_copy(columns[6]),
		boost::trim_copy(columns[9]),
		boost::trim_copy(columns[7]),
		boost::trim_copy(columns[8]),
		boost::trim_copy(columns[10]),
	};

	return entry;
}

bool SidAllocation::does_ades_match(const std::string& reference, const std::string& in) const
{
	if (reference.at(0) == '=')
		return reference.substr(1, 4) == in;

	if (reference.at(0) == '*')
		return reference.substr(1, 4) != in;

	return false;
}
