#pragma once
#include "EuroScopePlugIn.h"
#include "BeluxGatePlanner.hpp"
#include "ProcedureAssigner.h"
#include "BeluxUtil.hpp"
#include <time.h>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/ostreamwrapper.h>



#define MY_PLUGIN_NAME      "Belux"
#define MY_PLUGIN_VERSION   "1.6.0"
#define MY_PLUGIN_DEVELOPER "Nicola Macoir, Stef Pletinck for Belux vACC"
#define MY_PLUGIN_COPYRIGHT "GPL v3"
#define MY_PLUGIN_VIEW_AVISO  "Belux vACC"

#ifndef AUTH_TOKEN
#pragma message("No AUTH_TOKEN found")
#define AUTH_SECRET "Placeholder"
#else
// Behold, fuckery to quote things
#define stringify_literal( x ) # x
#define stringify_expanded( x ) stringify_literal( x )
#define AUTH_SECRET stringify_expanded( AUTH_TOKEN )
#pragma message("AUTH_TOKEN is defined to: " AUTH_SECRET )
#endif

using namespace std;
using namespace EuroScopePlugIn;

enum TagDefinitions: int {
	item_gate_assign = 1,
	function_gate_refresh,
	item_mach_number,
	function_force_sid,
	item_proc_suggestion,
	vspeed,
};

class BeluxPlugin :
	public EuroScopePlugIn::CPlugIn
{
public:
	BeluxPlugin();
	virtual ~BeluxPlugin();
	virtual void OnNewMetarReceived(const char* sStation, const char* sFullMetar);
	virtual void OnFlightPlanFlightPlanDataUpdate(CFlightPlan FlightPlan);
	virtual void OnFlightPlanDisconnect(CFlightPlan FlightPlan);
	virtual void OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize);
	virtual void OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area);
	virtual bool OnCompileCommand(const char* sCommandLine);
	virtual void OnTimer(int Counter);
	virtual void OnAirportRunwayActivityChanged(void);


protected:
	unsigned int vspeed_threshold = 300;
	UINT8 on_gate_change = 2; // 2: blink. 1: quiet. 0: none
	bool function_fetch_gates = true;
	bool function_set_initial_climb = true;
	bool function_mach_visualisation = true;
	bool function_check_runway_and_sid = false;
	bool force_new_procedure = false;
	
	int timeout_value = 1000;

	set<string>* processed;
	set<string> activeAirports;

	BeluxGatePlanner gatePlanner;
	BeluxUtil utils;
	ProcedureAssigner* procedureAssigner;

	map<string, vector<string>> activeDepRunways;
	map<string, vector<string>> activeArrRunways;

	map<string, int> QNH{{"EBLG", 0}, {"EBBR", 0}, {"EBOS", 0}};

	string GetHttpsRequest(string host, string uri, string request, bool expect_long_json);
	string GetGateInfo();
	string GetAirportInfo(string airport);
	string GetLatestPluginVersion();
	string SwapGate(string callsign, string gate);
	void ProcessMETAR(string airport, string metar);
	/**
	 * \brief Sets all active runways in the plugin properties
	 */
	void getActiveRunways();
	void ProcessFlightPlans();
	void FetchAndProcessGates();
	void versionCheck();
	void loadJSONconfig();

	void printDebugMessage(const string& function, const string& message);
	void printMessage(const string& topic, const string& message);
};

inline static bool startsWith(const char* pre, const char* str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
};