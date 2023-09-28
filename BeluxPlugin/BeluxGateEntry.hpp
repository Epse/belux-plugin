#pragma once
#include <time.h>
#include <Windows.h>
#include <map>
#include <string>

using namespace std;


class BeluxGateEntry
{
public:
    BeluxGateEntry();
    BeluxGateEntry(string callsign, string airport, string gate);
    virtual ~BeluxGateEntry() {};

    string callsign;
    string airport;
    string gate;
    bool isFetched;
    bool gate_has_changed;
    COLORREF color;
    bool suggest25R;
};

