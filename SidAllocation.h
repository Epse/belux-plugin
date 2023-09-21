#pragma once
#include <string>
#include <optional>
#include <cstdint>
#include <vector>

struct SidEntry
{
	std::string sid;
	std::string exit_point;
	uint8_t aircraft_class;
	std::string from_time; // Temporary, should be a parsed form later
	std::string to_time;
	std::string adep;
	std::string ades; // Should parse the prefixes of = and * too, later
	std::string rwy;
	std::string cfl; // Need to parse this to a union of lightlevel / altitude ideally
	std::string tsa; // Not used in EBBR, will add to others later
};

class SidAllocation
{
public:
	SidAllocation();
	void parse_string(const std::string &input) const;
	std::optional<SidEntry> find(const std::string& adep, const std::string& exit_point, const std::string& ades, const int engine_count, const std::string& runway) const;
private:
	std::vector<SidEntry> *entries;
	std::optional<SidEntry> parse_line(const std::string& line) const;
	bool does_ades_match(const std::string& reference, const std::string& in) const;
};
