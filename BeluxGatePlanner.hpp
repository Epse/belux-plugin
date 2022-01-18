#pragma once
#include <time.h>
#include <Windows.h>
#include <map>
#include <string>
#include "BeluxGateEntry.hpp"

using namespace std;

class BeluxGatePlanner
{
public:
    BeluxGatePlanner();
    virtual ~BeluxGatePlanner();
    map<int, string> string_split(string data, char delimiter);
    map<string, BeluxGateEntry> gate_list;
    void fetch_json(string data);
    void fetch_json_old(string data);
};
