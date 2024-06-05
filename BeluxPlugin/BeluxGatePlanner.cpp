#include "pch.h"
#include <string>
#include "BeluxGatePlanner.hpp"
#include <boost/json.hpp>

using namespace boost::json;

/*
 * Parses JSON in the following form into a map of {callsign: BeluxGateEntry}
 * [{"airport_ICAO":{"callsign":"gate", "callsign":"gate"}}]
 */
void BeluxGatePlanner::parse_json(std::string const& data) {
    /* received no data or error */
    if (data.empty() || data == "HTTPS_ERROR" || data == "[]" || data.back() != ']') {
        return;
    }

    std::map<std::string, BeluxGateEntry> temp_gate_list;

	// I use the throwing version of all this, then just set the gate list empty if something goes wrong..
	try {
		const auto document = parse(data).as_array();
		for (const value& airport_entry : document)
		{
			const auto airport = std::string(airport_entry.as_object().begin()->key());
			const auto entries = airport_entry.as_object().at(airport).as_object();
			for (const auto& entry : entries)
			{
				temp_gate_list.insert_or_assign(std::string(entry.key()),
					BeluxGateEntry(
						std::string(entry.key()),
						airport,
						std::string(entry.value().as_string())));
			}
		}
	} catch (std::exception const& e)
	{
		// Should probably log this, but we'll just ignore at the moment
	}

    this->gate_list = temp_gate_list;
}