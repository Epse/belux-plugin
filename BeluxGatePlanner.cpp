#include "pch.h"
#include <string>
#include "BeluxGatePlanner.hpp"
#include <iostream>

using namespace std;

BeluxGatePlanner::BeluxGatePlanner() {
}

void BeluxGatePlanner::fetch_json(string data) {
    /* received no data or error */
    if (data.empty() || data == "HTTPS_ERROR" || data == "[]" || data.back() != ']') {
        return;
    }

    map<string, BeluxGateEntry> temp_gate_list;

    //Remove outer {}s
    size_t openBracket = data.find_first_of('{');
    data = data.substr(openBracket + 1);

    size_t startPos, endPos;
    do {
        startPos = data.find_first_of('"');
        endPos = data.find_first_of('"', startPos + 1);
        string airport = data.substr(startPos + 1, endPos - startPos - 1);

        startPos = data.find_first_of("{", endPos);
        endPos = data.find_first_of("}", endPos);
        string flight_objects = data.substr(startPos + 1, endPos - startPos - 1);

        data = data.substr(endPos + 2);

        std::map<int, std::string> flights = string_split(flight_objects, ',');
        std::string callsign, gate;
        for (int i = 0; i < flights.size(); i++) {
            std::map<int, std::string> jsonset = string_split(flights[i], ':');

            startPos = jsonset[0].find_first_of('"');
            endPos = jsonset[0].find_last_of('"');
            callsign = jsonset[0].substr(startPos + 1, endPos - startPos - 1);

            startPos = jsonset[1].find_first_of('"');
            endPos = jsonset[1].find_last_of('"');
            gate = jsonset[1].substr(startPos + 1, endPos - startPos - 1);

            temp_gate_list[callsign] = BeluxGateEntry(callsign, airport, gate);
            if (this->gate_list.find(callsign) != this->gate_list.end()) {
                if (this->gate_list[callsign].gate != gate) {
                    temp_gate_list[callsign].gate_has_changed = true;
                }
            }
        }
        
    } while (data[0] != ']');
    this->gate_list = temp_gate_list;
}

void BeluxGatePlanner::fetch_json_old(string data) {
    /* received no data or error */
    if (data.empty() || data == "HTTPS_ERROR"|| data == "[]" || data.back() != ']') {
        return;
    }

    map<string, BeluxGateEntry> temp_gate_list;

    //Remove outer {}s
    size_t openBracket = data.find_first_of('[');
    data = data.substr(openBracket + 1);

    std::string entry;
    do{
        size_t openCurly = data.find_first_of('{'); // Should be 0.
        size_t closeCurly = data.find_first_of('}'); // Should be length-1.
        entry = data.substr(openCurly + 1, closeCurly - 1);

        std::cout << entry << std::endl;

        //Split by commas
        std::map<int, std::string> pieces = string_split(entry, ',');
        std::string key, value;
        size_t startPos, endPos;

        std::string airport, gate, callsign;
        // Loop over each JSON k/v set.
        for (int i = 0; i < pieces.size(); i++) {
            std::map<int, std::string> jsonset;
            jsonset = string_split(pieces[i], ':');

            startPos = jsonset[0].find_first_of('"');
            endPos = jsonset[0].find_last_of('"');
            key = jsonset[0].substr(startPos + 1, endPos - startPos - 1);

            startPos = jsonset[1].find_first_of('"');
            endPos = jsonset[1].find_last_of('"');
            value = jsonset[1].substr(startPos + 1, endPos - startPos - 1);

            // Assign value to the correct member.
            if (key == "assigned_to") {
                callsign = value;
            }
            else if (key == "gate") {
                gate = value;
            } 
            else if (key == "airport") {
                airport = value;
            }
        }
        temp_gate_list[callsign] = BeluxGateEntry(callsign, airport, gate);
        if (this->gate_list.find(callsign) != this->gate_list.end()) {
            if (this->gate_list[callsign].gate != gate) {
                temp_gate_list[callsign].gate_has_changed = true;
            }
        }

        data = data.substr(closeCurly + 1);
    } while (data[0] != ']');
    this->gate_list = temp_gate_list;
}

BeluxGatePlanner::~BeluxGatePlanner() {}

std::map<int, std::string> BeluxGatePlanner::string_split(std::string data, char delimiter) {
    std::map<int, std::string> pieces;
    size_t pos = 0, prevpos = 0, c = 0;
    while ((pos = data.find_first_of(delimiter, prevpos)) != std::string::npos) {
        pieces[c] = data.substr(prevpos, pos - prevpos);
        prevpos = pos + 1;
        c++;
    }
    // No comma found doesn't mean we're done, add the last section.
    pieces[c] = data.substr(prevpos);
    return pieces;
}