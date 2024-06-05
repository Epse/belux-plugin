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
		const auto parsed = parse(data);
		const auto document = parsed.as_array().at(0).as_object();
		for (const auto& airport_entry : document)
		{
			const auto airport = std::string(airport_entry.key());
			const auto entries = airport_entry.value().as_object();
			for (const auto& entry : entries)
			{
				const std::string callsign = entry.key();
				const std::string gate = std::string(entry.value().as_string());
				BeluxGateEntry gate_entry(callsign, airport, gate);
				if (this->gate_list.find(callsign) != this->gate_list.end())
				{
					gate_entry.gate_has_changed = this->gate_list.at(callsign).gate == gate;
				}
				temp_gate_list.insert_or_assign(std::string(entry.key()), gate_entry);
			}
		}
	} catch (std::exception const& e)
	{
		// Should probably log this, but we'll just ignore at the moment
		const auto copy = e.what();
	}

    this->gate_list = temp_gate_list;
}