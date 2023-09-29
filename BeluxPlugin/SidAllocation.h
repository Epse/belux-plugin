#pragma once
#include <string>
#include <optional>
#include <cstdint>
#include <vector>
#include <ctime>
#include <set>
#include <memory>

struct TimeActivation
{
	bool has_weekdays;
	tm tm_start;
	tm tm_end;
};

enum aircraft_class
{
	any = 1,
	four_engined = 2,
	non_four_engined = 3
};

struct SidEntry
{
	std::string sid;
	std::string exit_point;
	aircraft_class aircraft_class;
	std::optional<TimeActivation> time_activation;
	std::string adep;
	std::string ades; // Should parse the prefixes of = and * too, later
	std::string rwy;
	std::string tsa; // Not used in EBBR, will add to others later
};

class SidAllocation
{
public:
	SidAllocation();
	/**
	 * \brief Parses a multiline SID allocation string into SidEntries to be filtered later.
	 * \param input A string representation of https://beluxvacc.org/files/navigation_department/datafiles/SID_ALLOCATION.txt,
	 * newline delimited, pipe separated.
	 * \return The amount of SidEntries parsed
	 */
	size_t parse_string(const std::string& input) const;
	/**
	 * \brief Attempts to select a SID based on provided data.
	 * This *does not* attempt to do the WTC / RWY matching, this is out of scope. We do match against runways provided.
	 * TSA status is not yet checked, we would need a LARA validation for that.
	 * We return the first matching SID.
	 * \param adep Departure ICAO
	 * \param exit_point First FIX in flightplan, expected SID exit point
	 * \param ades Destination ICAO
	 * \param engine_count self-explanatory
	 * \param now A fakeable reference to the current time
	 * \param active_areas A vector of active TRA/TSA areas to check against
	 * \return A SID entry if one matches the provided rules
	 */
	std::optional<SidEntry> find(const std::string& adep, const std::string& exit_point, const std::string& ades,
	                             const int engine_count, const std::string& runway,
	                             const tm& now, const std::vector<std::string>& active_areas) const;
	std::set<std::string> for_airport(const std::string& adep) const;

private:
	std::unique_ptr<std::vector<SidEntry>> entries;
	std::optional<SidEntry> parse_line(const std::string& line) const;
	std::optional<TimeActivation> parse_time_activation(const std::string& line_start,
	                                                    const std::string& line_end) const;
	std::optional<std::pair<bool, tm>> parse_activation_time_line(const std::string& line) const;
	/**
	 * \brief Checks if the given ADES matches the entry ADES given under reference.
	 * These references may contain a * prefix, indicating they will match any ADES that is not the provided one,
	 * or an = prefix indicating they match ONLY the provided one.
	 * \param reference An ADES spec consisting of a one-character prefix and a four-character ICAO code
	 * \param in ICAO code to check against reference
	 * \return Whether the ADES rules match, as defined above.
	 */
	bool does_ades_match(const std::string& reference, const std::string& in) const;
	static bool does_activation_match(const std::optional<TimeActivation>& reference, const tm& now);
};
