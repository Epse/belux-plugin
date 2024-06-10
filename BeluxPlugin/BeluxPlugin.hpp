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
#define MY_PLUGIN_VERSION   "1.4.2"
#define MY_PLUGIN_DEVELOPER "Nicola Macoir, Stef Pletinck for Belux vACC"
#define MY_PLUGIN_COPYRIGHT "GPL v3"
#define MY_PLUGIN_VIEW_AVISO  "Belux vACC"

using namespace std;
using namespace EuroScopePlugIn;

enum TagDefinitions: int {
	item_gate_assign = 1,
	function_gate_refresh,
	item_mach_number,
	function_force_sid,
	item_proc_suggestion,
};

class BeluxPlugin :
	public EuroScopePlugIn::CPlugIn
{
public:
	BeluxPlugin();
	virtual ~BeluxPlugin();
	virtual void BeluxPlugin::OnNewMetarReceived(const char* sStation, const char* sFullMetar);
	virtual void BeluxPlugin::OnFlightPlanFlightPlanDataUpdate(CFlightPlan FlightPlan);
	virtual void BeluxPlugin::OnFlightPlanDisconnect(CFlightPlan FlightPlan);
	virtual void BeluxPlugin::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan, EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize);
	virtual void BeluxPlugin::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area);
	virtual bool BeluxPlugin::OnCompileCommand(const char* sCommandLine);
	virtual void BeluxPlugin::OnTimer(int Counter);
	virtual void BeluxPlugin::OnAirportRunwayActivityChanged(void);


protected:
	string BeluxPlugin::GetHttpsRequest(string host, string uri, string request, bool expect_long_json);
	string BeluxPlugin::GetGateInfo();
	string BeluxPlugin::GetAirportInfo(string airport);
	string BeluxPlugin::GetLatestPluginVersion();
	string BeluxPlugin::SwapGate(string callsign, string gate);
	void BeluxPlugin::ProcessMETAR(string airport, string metar);
	/**
	 * \brief Sets all active runways in the plugin properties
	 */
	void BeluxPlugin::getActiveRunways();
	void BeluxPlugin::ProcessFlightPlans();
	void BeluxPlugin::SendDiscordMessage(string msg);
	void BeluxPlugin::FetchAndProcessGates();
	void BeluxPlugin::versionCheck();
	void BeluxPlugin::loadJSONconfig();

	void BeluxPlugin::printDebugMessage(const string& function, const string& message);
	void BeluxPlugin::printMessage(const string& topic, const string& message);
};

inline static bool startsWith(const char* pre, const char* str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
};