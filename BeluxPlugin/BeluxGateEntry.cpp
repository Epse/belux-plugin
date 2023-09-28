#include "pch.h"
#include "BeluxGateEntry.hpp"
#include <string>
#include <time.h>
#include <boost/algorithm/string.hpp>

using namespace std;

BeluxGateEntry::BeluxGateEntry() {
    BeluxGateEntry("DUMMY", "ERR", "ERR");
}

BeluxGateEntry::BeluxGateEntry(string callsign, string airport, string gate) {
	this->callsign = callsign;
    this->airport = airport;
	this->gate = gate;
    this->isFetched = true;
    this->gate_has_changed = false;
	this->color = NULL;
    this->suggest25R = false;

    if (airport == "EBBR") {
        
        if (boost::algorithm::starts_with(gate, "MIL") || 
            boost::algorithm::starts_with(gate, "GA")  || 
            boost::algorithm::starts_with(gate, "9")   ||
            boost::algorithm::starts_with(gate, "5") 
           ) {
            // We detected a MIL/GA/CARGO stand
            this->suggest25R = true;
        }
        else {
            try {
                int ivalue = atoi(gate.c_str());
                if (ivalue >= 120 && ivalue <= 174 && ivalue % 2 == 0) {
                    // We detected APRON 1 north
                    this->suggest25R = true;
                }
            }
            catch (exception& e) {}
        }
    }
}