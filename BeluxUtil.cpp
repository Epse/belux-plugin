#include "pch.h"
#include "BeluxUtil.hpp"
#define CURL_STATICLIB
#include <curl\curl.h>
#include <curl\easy.h>

using namespace std;
using namespace rapidjson;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BeluxUtil::BeluxUtil() {
    char DllPathFile[_MAX_PATH];
    GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
    plugin_path = DllPathFile;
    plugin_path.resize(plugin_path.size() - strlen("Belux.dll"));
}

namespace
{
    std::size_t callback(
        const char* in,
        std::size_t size,
        std::size_t num,
        std::string* out)
    {
        const std::size_t totalBytes(size * num);
        out->append(in, totalBytes);
        return totalBytes;
    }
}

string BeluxUtil::http_download(string url) {
    CURL* curl;
    CURLcode res;
    std::unique_ptr<std::string> httpData(new std::string());
    curl = curl_easy_init();
    if (curl) {
        //fopen_s(&fp, location.c_str(), "wb");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res == CURLcode::CURLE_OK) {
            return *httpData.get();
        }
        else {
            throw exception(string("failed fetching weather file. Error code: " + to_string(int(res))).c_str());
        }
    }
}

string BeluxUtil::fetch_weather_file() {
    try {
        string response = http_download("http://www.wachters.be/windy/weather.json");
        if (weatherdoc.Parse<0>(response.c_str()).HasParseError()) {
            throw exception("weather.json could not be parsed succesfully.");
        }
        return weatherdoc["info"].GetObject()["datestring"].GetString();
    }
    catch (exception& e) {
        AfxMessageBox(e.what(), MB_OK);
        return "nil";
    }
}


tuple<double, double> BeluxUtil::calculate_mach(string callsign, int flightlevel, int gs, double hdg, double lat, double lon) {
    if (mach_timeout.find(callsign) == mach_timeout.end() || ( get<1>(mach_timeout[callsign]) + 5) < std::time(0) && weatherdoc != NULL && !weatherdoc.HasParseError()) {
        const int possibleFL_length = 17;
        vector<int> possibleFL = { 0, 1, 30, 50, 64, 2, 100, 3, 140, 4, 180, 5, 240, 6, 300, 340, 390 };
        int FL = closest(possibleFL, flightlevel);
        string closest = closest_location(lat, lon);

        //double kelvin = 0, windheading = 0, windspeed = 0;
        double kelvin = stod(weatherdoc["data"].GetObject()[closest.c_str()].GetObject()[to_string(FL).c_str()].GetObject()["T(K)"].GetString());
        double windheading = stod(weatherdoc["data"].GetObject()[closest.c_str()].GetObject()[to_string(FL).c_str()].GetObject()["windhdg"].GetString());
        double windspeed = stod(weatherdoc["data"].GetObject()[closest.c_str()].GetObject()[to_string(FL).c_str()].GetObject()["windspeed"].GetString());

        //string test = to_string(kelvin) + " " + to_string(windheading) + " " + to_string(windspeed);
        //AfxMessageBox("renewing", MB_OK);

        double LSS = 643.855 * (pow((kelvin / 273.15), 0.5));
        double hdgdiff = abs(hdg - windheading);
        double raddiff = (hdgdiff * M_PI) / 180;
        double cosval = cos(raddiff);
        double trueAS = gs + (cosval * windspeed);
        double mach = trueAS / LSS;
        double IAS = trueAS / (1 + (flightlevel / 10) * 0.0175);

        get<1>(mach_timeout[callsign]) = std::time(0);
        get<0>(mach_timeout[callsign]) = tuple<double, double>(mach, IAS);
    }

    return  get<0>(mach_timeout[callsign]) ;
}


double BeluxUtil::haversine(double lat1, double lon1, double lat2, double lon2)
{
    // distance between latitudes 
    // and longitudes 
    double dLat = (lat2 - lat1) *
        M_PI / 180.0;
    double dLon = (lon2 - lon1) *
        M_PI / 180.0;

    // convert to radians 
    lat1 = (lat1)*M_PI / 180.0;
    lat2 = (lat2)*M_PI / 180.0;

    // apply formulae 
    double a = pow(sin(dLat / 2), 2) +
        pow(sin(dLon / 2), 2) *
        cos(lat1) * cos(lat2);
    double rad = 6371;
    double c = 2 * asin(sqrt(a));
    return rad * c;
}

int BeluxUtil::closest(std::vector<int> const& vec, int value) {
    int closest_value = 0;
    int difference = 9999;
    for (unsigned int i = 0;i < vec.size();i++) {
        if (abs(vec[i] - value) < difference) {
            difference = abs(vec[i] - value);
            closest_value = vec[i];
        }
    }
    return closest_value;
}

string BeluxUtil::closest_location(double lat, double lon) {
    string closest_location = "";
    double closest_distance = 9999999.999;
    map<string, tuple<double, double>>::iterator it;
    for (it = locations.begin(); it != locations.end(); it++)
    {
        double distance = haversine(get<0>(it->second), get<1>(it->second), lat, lon);
        if (distance < closest_distance) {
            closest_distance = distance;
            closest_location = it->first;
        }
    }
    return closest_location;

}
