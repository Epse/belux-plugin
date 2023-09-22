#pragma once
#include <time.h>
#include <Windows.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <vector>
#include <cmath> 
#include <tuple>
#include <math.h>  
#include <fstream>
#include <streambuf>
#include <ostream>
#include <sstream>
#include <ctime>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/ostreamwrapper.h>

using namespace std;
using namespace rapidjson;

class BeluxUtil
{
public:
    BeluxUtil();
    virtual ~BeluxUtil() {};

    int closest(std::vector<int> const& vec, int value);
    string closest_location(double lat, double lon);
    double haversine(double lat1, double lon1, double lat2, double lon2);
    tuple<double, double> calculate_mach(string callsign, int flightlevel, int gs, double hdg, double lat, double lon);

    static string http_download(string url);
    string fetch_weather_file();

    string plugin_path;
    Document weatherdoc;

    map<string, tuple<tuple<double,double>, time_t>> mach_timeout;
    time_t RETENTION = 1000;

    static string https_fetch_file(string url);

    map<string, tuple<double, double>> BeluxUtil::locations = {
        {"DVR",  {51.160,1.331}},
        {"KOK",  {51.103,2.623}},
        {"FERDI" , {50.877,3.591}},
        {"BUB" , {50.888,4.516}} ,
        {"TUTSO" , {50.506,5.207}} ,
        {"LENDO" , {50.606,6.239}} ,
        {"DIK" , {50.063,5.983}} ,
        {"MTZ" , {49.339,6.299}} ,
        {"POGOL" , {48.651,6.262}} ,
        {"FAMEN" , {49.912,5.146}} ,
        {"BELDI" , {50.032,2.931}} ,
        {"COA" , {51.359,2.889}} ,
        {"HELEN" , {51.197,3.758}} ,
        {"EBAW" , {51.187,4.462}} ,
        {"BROGY" , {51.164,5.469}} ,
        {"EHRD" , {51.809,4.712}} ,
        {"EDDL" , {51.255,6.684}} ,
        {"FLEXS" , {49.790,6.973}}
    };
};
