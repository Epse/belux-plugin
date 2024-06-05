#pragma once
#include <time.h>
#include <Windows.h>
#include <map>
#include <string>
#include "BeluxGateEntry.hpp"

class BeluxGatePlanner
{
public:
    std::map<std::string, BeluxGateEntry> gate_list;
    void parse_json(std::string const& data);
};
