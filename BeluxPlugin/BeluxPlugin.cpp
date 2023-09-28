#include "pch.h"
#include "BeluxPlugin.hpp"
#include <string>
#include <map>
#include <set>
#include <utility>
#include <iomanip>
#include <Windows.h>

using namespace std;
using namespace EuroScopePlugIn;

using boost::asio::ip::tcp;

#ifdef _DEBUG
bool DEBUG_print = true;
#else
bool DEBUG_print = false;
#endif
UINT8 on_gate_change = 2; // 2: blink. 1: quiet. 0: none
bool function_fetch_gates = true;
bool function_set_initial_climb = true;
bool function_mach_visualisation = true;
bool function_check_runway_and_sid = false;
bool force_new_procedure = false;

int timeout_value = 1000;

// internal ID lists
constexpr int TAG_ITEM_GATE_ASGN = 1;
constexpr int TAG_FUNCTION_REFRESH_GATE = 2;
constexpr int TAG_ITEM_MACH_NUMBER = 3;
constexpr int TAG_FUNCTION_FORCE_SID = 4;

// Time (in seconds) before we request new information about this flight from the API.
constexpr int DATA_RETENTION_LENGTH = 60;

set<string>* processed;
set<string>* beluxAirports;

set<string> brusselsSidWaypoints = {
	"ELSIK", "NIK", "HELEN", "DENUT", "KOK", "CIV", "ROUSY", "PITES", "SPI", "LNO", "SOPOK"
};
set<string> brusselSids = {};
BeluxGatePlanner gatePlanner;
BeluxUtil utils;
ProcedureAssigner* procedureAssigner;

vector<string> activeDepRunways;
vector<string> activeArrRunways;

int liege_QNH = 0;

string last_processed = "";


BeluxPlugin::BeluxPlugin(void) : CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE, MY_PLUGIN_NAME, MY_PLUGIN_VERSION,
                                         MY_PLUGIN_DEVELOPER, MY_PLUGIN_COPYRIGHT)
{
	versionCheck();
	loadJSONconfig();

	procedureAssigner = new ProcedureAssigner([this](const std::string& text)
	{
		printDebugMessage("SID", text);
	});

	getActiveRunways("EBBR");
	procedureAssigner->set_departure_runways(activeDepRunways);
	beluxAirports = new set<string>({"EBBR", "ELLX", "EBOS", "EBAW", "EBLG", "EBKT", "EBCI"});
	processed = new set<string>();

	// Register Tag item(s).
	RegisterTagItemType("Assigned Gate", TAG_ITEM_GATE_ASGN);
	RegisterTagItemFunction("refresh assigned gate", TAG_FUNCTION_REFRESH_GATE);
	RegisterTagItemType("Mach number", TAG_ITEM_MACH_NUMBER);
	RegisterTagItemFunction("Force RWY/SID", TAG_FUNCTION_FORCE_SID);

	ProcessMETAR("EBLG", GetAirportInfo("EBLG"));
	if (function_fetch_gates)
		FetchAndProcessGates();

	if (function_mach_visualisation)
	{
		printDebugMessage("API", "fetched weather file " + utils.fetch_weather_file());
	}
	try
	{
		const auto parsed = procedureAssigner->fetch_sid_allocation();
		printDebugMessage("SID", "Parsed SID allocations: " + std::to_string(parsed));
		if (parsed < 1)
		{
			printMessage("SID", "Could not parse SID allocation file.");
		}
	}
	catch (exception _e)
	{
		printDebugMessage("SID", "Caught an exception");
	}
}

BeluxPlugin::~BeluxPlugin()
{
	delete beluxAirports;
	delete processed;
}

void BeluxPlugin::versionCheck()
{
	string loadingMessage = MY_PLUGIN_VERSION;
	loadingMessage += " loaded.";
	DisplayUserMessage("Message", "Belux Plugin", loadingMessage.c_str(), true, true, true, false, false);
	DisplayUserMessage("Belux Plugin", "Plugin version", loadingMessage.c_str(), true, true, true, false, false);

	if (!boost::algorithm::ends_with(MY_PLUGIN_VERSION, "BETA"))
	{
		string latest_version = GetLatestPluginVersion();
		if (latest_version != "S_ERR")
		{
			if (strcmp(MY_PLUGIN_VERSION, latest_version.c_str()) < 0)
			{
				string message = "You are using an older version of the Belux plugin. Updating to " + latest_version +
					" is adviced. For safety, API-based functions have been disabled";
				function_fetch_gates = false;
				MessageBox(0, message.c_str(), "Belux plugin version", MB_OK | MB_ICONQUESTION);
			}
			else
			{
				DisplayUserMessage("Belux Plugin", "Plugin version", "You are using the latest version", true, true,
				                   true, false, false);
			}
		}
		else
		{
			DisplayUserMessage("Belux Plugin", "Plugin version", "Failed verifying latest version", true, true, true,
			                   false, false);
		}
	}
	else
	{
		DisplayUserMessage("Belux Plugin", "Plugin version", "using BETA version --> skiped verifying version", true,
		                   true, true, false, false);
	}
}

void BeluxPlugin::loadJSONconfig()
{
	// Getting the DLL file folder
	char DllPathFile[_MAX_PATH];
	string DllPath;

	GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
	DllPath = DllPathFile;
	DllPath.resize(DllPath.size() - strlen("belux.dll"));
	string configPath = DllPath + "\\belux_config.json";

	stringstream ss;
	ifstream ifs(configPath.c_str(), std::ios::binary);
	if (ifs.is_open())
	{
		ss << ifs.rdbuf();
		ifs.close();
		printDebugMessage("config", "Loading belux JSON config... ");
		Document document;
		if (document.Parse<0>(ss.str().c_str()).HasParseError())
		{
			AfxMessageBox(
				"An error parsing Belux configuration occurred.\nOnce fixed, reload the config by typing '.belux reload'",
				MB_OK);
		}
		if (document.HasMember("debug_mode"))
		{
			DEBUG_print = document["debug_mode"].GetBool();
			printDebugMessage("config", "debug mode " + string(DEBUG_print ? "enabled" : "disabled"));
		}
		if (document.HasMember("API_timeout"))
		{
			timeout_value = document["API_timeout"].GetInt();
			printDebugMessage("config", "API timeout to " + std::to_string(timeout_value));
		}
		if (document.HasMember("on_gate_change"))
		{
			// TODO: now why don't you work from the start....
			const string ogc = document["on_gate_change"].GetString();
			if (ogc == "blink")
			{
				on_gate_change = 2;
				printDebugMessage("config", "Alert on gate change: on");
			}
			else if (ogc == "quiet")
			{
				on_gate_change = 1;
				printDebugMessage("config", "Alert on gate change: quietly");
			}
			else if (ogc == "none")
			{
				on_gate_change = 0;
				printDebugMessage("config", "Alert on gate change: off");
			}
			else
			{
				printDebugMessage("config", "Alert on gate change: invalid (default to on)");
			}
		}
		else { printDebugMessage("config", "No alert on gate change config."); }
		if (document.HasMember("functionalities"))
		{
			if (document["functionalities"].GetObject().HasMember("fetch_gates"))
			{
				function_fetch_gates = document["functionalities"].GetObject()["fetch_gates"].GetBool();
				printDebugMessage("config", "Gate assigner " + string(function_fetch_gates ? "enabled" : "disabled"));
			}
			if (document["functionalities"].GetObject().HasMember("set_initial_climb"))
			{
				function_set_initial_climb = document["functionalities"].GetObject()["set_initial_climb"].GetBool();
				printDebugMessage(
					"config", "Initial climb " + string(function_set_initial_climb ? "enabled" : "disabled"));
			}
			if (document["functionalities"].GetObject().HasMember("mach_visualisation"))
			{
				function_mach_visualisation = document["functionalities"].GetObject()["mach_visualisation"].GetBool();
				printDebugMessage(
					"config", "Mach visualisation " + string(function_mach_visualisation ? "enabled" : "disabled"));
			}
			if (document["functionalities"].GetObject().HasMember("rwy_sid_assigner"))
			{
				function_check_runway_and_sid = document["functionalities"].GetObject()["rwy_sid_assigner"].GetBool();
				printDebugMessage(
					"config", "rwy/sid assigner " + string(function_check_runway_and_sid ? "enabled" : "disabled"));
			}
		}
	}
}

void BeluxPlugin::OnFlightPlanFlightPlanDataUpdate(CFlightPlan FlightPlan)
{
}

void BeluxPlugin::ProcessFlightPlans()
{
	for (EuroScopePlugIn::CRadarTarget rt = this->RadarTargetSelectFirst(); rt.IsValid(); rt = this->
	     RadarTargetSelectNext(rt))
	{
		EuroScopePlugIn::CRadarTargetPositionData pos = rt.GetPosition();
		EuroScopePlugIn::CFlightPlan fp = rt.GetCorrelatedFlightPlan();

		string dep_airport = fp.GetFlightPlanData().GetOrigin();
		string callsign = fp.GetCallsign();

		if (function_check_runway_and_sid || force_new_procedure)
		{
			if (procedureAssigner->process_flight_plan(fp, force_new_procedure))
			{
				printDebugMessage("SID", "Processed " + std::string(fp.GetCallsign()));
			}
		}

		if (beluxAirports->find(dep_airport) == beluxAirports->end() // IF Not found in belux airport list
			|| !fp.IsValid() || !fp.GetCorrelatedRadarTarget().IsValid()
			// OR flightplan has not been loaded/correleted correctly?
			|| processed->find(callsign) != processed->end() // OR was already processed
			|| (strcmp(fp.GetTrackingControllerId(), "") != 0 && !fp.GetTrackingControllerIsMe())
			// OR aircraft is tracked (with exception of aircraft tracked by current controller)
			|| fp.GetCorrelatedRadarTarget().GetGS() > 5 // OR moving: Ground speed > 5knots
			|| fp.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() > 1500
			// OR flying: Altitude > 1500 feet
			|| fp.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() == 0)
		{
			// OR altitude == 0 -> uncorrelated? alitude should never be zero
			continue; // THEN SKIP 
		}
		processed->insert(callsign);

		if (function_set_initial_climb)
		{
			//Saftey check: only set CFL once
			int CFL = 0;
			if (dep_airport == "EBBR" || dep_airport == "EBOS")
			{
				CFL = 6000;
			}
			else if (dep_airport == "ELLX" || dep_airport == "EBCI")
			{
				CFL = 4000;
			}
			else if (dep_airport == "EBAW" || dep_airport == "EBKT")
			{
				CFL = 3000;
			}
			else if (dep_airport == "EBLG")
			{
				if (liege_QNH == 0)
					ProcessMETAR("EBLG", GetAirportInfo("EBLG"));

				if (liege_QNH != 0 && liege_QNH < 995)
				{
					CFL = 6000;
				}
				else
				{
					CFL = 5000;
				}
			}

			if (CFL > 0 && fp.GetFlightPlanData().GetFinalAltitude() > CFL && fp.GetControllerAssignedData().
				GetClearedAltitude() != CFL)
			{
				fp.GetControllerAssignedData().SetClearedAltitude(CFL);
			}
		}
	}
	force_new_procedure = false;
}

void BeluxPlugin::OnFlightPlanDisconnect(CFlightPlan FlightPlan)
{
	if (function_set_initial_climb)
	{
		processed->erase(FlightPlan.GetCallsign());
	}
	procedureAssigner->on_disconnect(FlightPlan);
}

void BeluxPlugin::OnNewMetarReceived(const char* sStation, const char* sFullMetar)
{
	if (function_set_initial_climb)
	{
		if (sStation == "EBLG")
		{
			string metar = sFullMetar;
			ProcessMETAR("EBLG", metar);
		}
	}
}

void BeluxPlugin::ProcessMETAR(string airport, string metar)
{
	if (airport == "EBLG")
	{
		try
		{
			size_t pos = metar.find("Q") + 1;
			liege_QNH = stoi(metar.substr(pos, 4));
			if (DEBUG_print)
			{
				char buffer[50];
				sprintf_s(buffer, "SET EBLG QNH Q%d", liege_QNH);
				DisplayUserMessage("Belux Plugin", "CFL setter", buffer, true, true, true, false, false);
			}
		}
		catch (const exception& e)
		{
		}
	}
}

void BeluxPlugin::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
	switch (FunctionId)
	{
	case TAG_FUNCTION_REFRESH_GATE:
		if (function_fetch_gates)
			FetchAndProcessGates();
		break;
	case TAG_FUNCTION_FORCE_SID:
		if (!procedureAssigner->process_flight_plan(FlightPlanSelectASEL(), true))
		{
			printMessage("SID", "Failed to assign requested SID/RWY");
		}
	}
}

void BeluxPlugin::OnTimer(int Counter)
{
	if (function_fetch_gates && (Counter % DATA_RETENTION_LENGTH == 0))
	{
		FetchAndProcessGates();
	}

	if (Counter % 5 == 0)
	{
		ProcessFlightPlans();
	}

	if (function_mach_visualisation && (Counter % (30 * 60) == 0))
	{
		printDebugMessage("API", "fetched weather file " + utils.fetch_weather_file());
	}
}

void BeluxPlugin::OnAirportRunwayActivityChanged(void)
{
	getActiveRunways("EBBR");
	procedureAssigner->set_departure_runways(activeDepRunways);
	procedureAssigner->reprocess_all();
}

void BeluxPlugin::FetchAndProcessGates()
{
	gatePlanner.fetch_json(GetGateInfo());
	for (std::map<string, BeluxGateEntry>::iterator iter = gatePlanner.gate_list.begin(); iter != gatePlanner.gate_list.
	     end(); ++iter)
	{
		string cs = iter->first;
		BeluxGateEntry entry = iter->second;
		EuroScopePlugIn::CFlightPlan fp = FlightPlanSelect(cs.c_str());
		string dest = fp.GetFlightPlanData().GetDestination();
		string gate = gatePlanner.gate_list[cs].gate;

		if (gatePlanner.gate_list[cs].gate_has_changed)
		{
			//---GATE Change detected------
			string message = cs + " ==> " + gatePlanner.gate_list[cs].gate;
			if (on_gate_change)
			{
				const bool blink = on_gate_change > 1;
				DisplayUserMessage("Belux Plugin", "GATE CHANGE", message.c_str(),
				                   blink, blink, blink, blink, blink);
			}
			gatePlanner.gate_list[cs].color = RGB(50, 205, 50);
			fp.GetControllerAssignedData().SetFlightStripAnnotation(4, gate.c_str());
		}

		if (string(fp.GetControllerAssignedData().GetFlightStripAnnotation(4)) != gate)
		{
			fp.GetControllerAssignedData().SetFlightStripAnnotation(4, gate.c_str());
			//FlightPlan.GetControllerAssignedData().SetScratchPadString(gatePlanner.gate_list[cs].gate.c_str());
		}


		// Assign arrival RWY, as of yet not used
		if (function_check_runway_and_sid && false)
		{
			bool ops25r25l = (find(activeArrRunways.begin(), activeArrRunways.end(), "25L") != activeArrRunways.end() &&
				find(activeArrRunways.begin(), activeArrRunways.end(), "25R") != activeArrRunways.end());
			bool ops07l07r = (find(activeArrRunways.begin(), activeArrRunways.end(), "07L") != activeArrRunways.end() &&
				find(activeArrRunways.begin(), activeArrRunways.end(), "07R") != activeArrRunways.end());

			if (ops25r25l || ops07l07r)
			{
				string route = fp.GetFlightPlanData().GetRoute();
				string last_waypoint = route.substr(route.find_last_of(" ") + 1);

				if (ops25r25l && (boost::algorithm::starts_with(gate, "MIL") ||
					boost::algorithm::starts_with(gate, "GA") || boost::algorithm::starts_with(gate, "9") ||
					boost::algorithm::starts_with(gate, "5")))
				{
					if (last_waypoint != "EBBR/25R")
					{
						if (last_waypoint == "EBBR/25L" || last_waypoint == "EBBR/19" || last_waypoint == "EBBR/07L" ||
							last_waypoint == "EBBR/07R")
						{
							route = route.substr(0, route.find_last_of(" ") - 1);
						}
						route += string(" EBBR/25R");
						//fp.GetFlightPlanData().SetRoute(route.c_str());
						//fp.GetFlightPlanData().AmendFlightPlan();
					}
				}
				else if (ops07l07r && (boost::algorithm::starts_with(gate, "MIL") || boost::algorithm::starts_with(
					gate, "GA")))
				{
					if (last_waypoint != "EBBR/07L")
					{
						if (last_waypoint == "EBBR/25L" || last_waypoint == "EBBR/19" || last_waypoint == "EBBR/25R" ||
							last_waypoint == "EBBR/07R")
						{
							route = route.substr(0, route.find_last_of(" ") - 1);
						}
						route += string(" EBBR/07L");
						//fp.GetFlightPlanData().SetRoute(route.c_str());
						//fp.GetFlightPlanData().AmendFlightPlan();
					}
				}
			}
		}
	}
}

void BeluxPlugin::OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget,
                               int ItemCode, int TagData,
                               char sItemString[16], int* pColorCode,
                               COLORREF* pRGB, double* pFontSize)
{
	double lat, lon, hdg, mach;
	tuple<double, double> result;
	int gs, FL, ias;
	string cs;
	// Only work on tag items we actually care about.
	switch (ItemCode)
	{
	case TAG_ITEM_MACH_NUMBER:
		if (function_mach_visualisation)
		{
			cs = FlightPlan.GetCallsign();
			gs = FlightPlan.GetCorrelatedRadarTarget().GetGS();
			FL = (int)(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetFlightLevel() / 100);
			hdg = FlightPlan.GetCorrelatedRadarTarget().GetTrackHeading();
			lat = FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude;
			lon = FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude;
			if (gs > 0)
			{
				result = utils.calculate_mach(cs, FL, gs, hdg, lat, lon);
				//printDebugMessage(to_string(gs) + " " + to_string(FL) + " " + to_string(hdg) + " " + to_string(lat) + " " + to_string(lon) + "--> " + to_string(mach));
				mach = get<0>(result);
				ias = (int)get<1>(result);
				std::stringstream stream;
				stream << std::fixed << std::setprecision(3) << mach;
				string output;
				if (FL < 245)
				{
					output = "/" + to_string(ias);
				}
				else
				{
					output = stream.str().substr(1) + "/" + to_string(ias);
				}
				strcpy_s(sItemString, 16, output.c_str());
			}
		}
		break;

	case TAG_ITEM_GATE_ASGN:
		if (function_fetch_gates)
		{
			string cs = FlightPlan.GetCallsign();
			if (gatePlanner.gate_list.find(cs) != gatePlanner.gate_list.end())
			{
				if (gatePlanner.gate_list[cs].color != NULL)
				{
					(*pColorCode) = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
					(*pRGB) = gatePlanner.gate_list[cs].color;
				}

				string gateItem = gatePlanner.gate_list[cs].gate + (gatePlanner.gate_list[cs].suggest25R ? "*" : "");
				strcpy_s(sItemString, 8, gateItem.c_str());
			}
		}
	}
}

string BeluxPlugin::GetAirportInfo(string airport)
{
	const string host = "metar.vatsim.net";
	const string uri = "/search_metar.php?id=" + airport;

	// Form the request.
	std::stringstream request;
	request << "GET " << uri << " HTTP/1.1\r\n";;
	request << "Host: " << host << "\r\n\r\n";

	string response = GetHttpsRequest(host, uri, request.str(), false);
	return response;
}

void BeluxPlugin::printDebugMessage(string function, string message)
{
	if (DEBUG_print)
		DisplayUserMessage(string("Belux Plugin DEBUG - " + function).c_str(), "DEBUG", message.c_str(), true, true,
		                   true, true, true);
}

void BeluxPlugin::printMessage(string topic, string message)
{
	DisplayUserMessage("Belux Plugin", topic.c_str(), message.c_str(), true, true, true, true, true);
}

string BeluxPlugin::GetLatestPluginVersion()
{
	const string host = "api.beluxvacc.org";
	const string uri = "/belux-gate-manager-api/version/plugin";

	// Form the request.
	std::stringstream request;
	request << "GET " << uri << " HTTP/1.1\r\n";;
	request << "Host: " << host << "\r\n\r\n";

	string response = GetHttpsRequest(host, uri, request.str(), false);
	response = response.substr(response.length() - 2 - 5, 5);
	return response;
}

std::string BeluxPlugin::GetGateInfo()
{
	const string host = "api.beluxvacc.org";
	const string uri = "/belux-gate-manager-api/get_all_assigned_gates/";

	// Form the request.
	std::stringstream request;
	request << "GET " << uri << " HTTP/1.1\r\n";;
	request << "Host: " << host << "\r\n\r\n";

	string response = GetHttpsRequest(host, uri, request.str(), true);
	return response;
}

void BeluxPlugin::SendDiscordMessage(string msg)
{
	const string host = "discord.com";
	const string uri =
		"/api/webhooks/892874115172683896/Q7o-SPJCH65q0uQlVBEpq7WpRCAf3Vd9L_AdD3rJyurWq_I95RL_qqtCg2yJhmIOdcgo";

	std::stringstream body;
	body << "{\"content\": null,\r\n";
	body << "\"embeds\": [{\r\n";
	body << "\"title\": \"" << ControllerMyself().GetFullName() << " is requesting backup at " << ControllerMyself().
		GetCallsign() << "\",\r\n";
	body << "\"color\": 16711680,\r\n";
	body << "\"author\": {\"name\" : \"Belux Backup Bot\"},\r\n";
	body << "\"fields\": [\r\n";
	body << "{\"name\": \"Message\", \"value\" : \"" << msg << "\",\"inline\" : true}]\r\n";
	body << "}\r\n]\r\n}\r\n";

	// Form the request.
	std::stringstream request;
	request << "POST " << uri << " HTTP/1.1\r\n";;
	request << "Host: " << host << "\r\n";
	request << "Content-Type: application/json\r\n";
	request << "Content-Length: " << body.str().length() << "\r\n\r\n";
	request << body.str().c_str();


	string response = GetHttpsRequest(host, uri, request.str(), true);
}

void BeluxPlugin::getActiveRunways(string airport)
{
	activeDepRunways.clear();
	activeArrRunways.clear();

	// Auto load the airport config on ASR opened.
	CSectorElement rwy;

	for (rwy = SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
	     rwy.IsValid();
	     rwy = SectorFileElementSelectNext(rwy, SECTOR_ELEMENT_RUNWAY))
	{
		if (startsWith(airport.c_str(), rwy.GetAirportName()))
		{
			if (rwy.IsElementActive(true, 0))
			{
				activeDepRunways.push_back(rwy.GetRunwayName(0));
			}
			if (rwy.IsElementActive(false, 0))
			{
				activeArrRunways.push_back(rwy.GetRunwayName(0));
			}

			if (rwy.IsElementActive(true, 1))
			{
				activeDepRunways.push_back(rwy.GetRunwayName(1));
			}
			if (rwy.IsElementActive(false, 1))
			{
				activeArrRunways.push_back(rwy.GetRunwayName(1));
			}
		}
	}
	string outputmsg = "active runways are: D=";
	for (int i = 0; i < activeDepRunways.size(); i++)
	{
		outputmsg += activeDepRunways[i] + ",";
	}
	outputmsg += "; A=";
	for (int i = 0; i < activeArrRunways.size(); i++)
	{
		outputmsg += activeArrRunways[i] + ",";
	}
	printDebugMessage("rwysid", outputmsg);
}

string BeluxPlugin::SwapGate(string callsign, string gate)
{
	const string host = "api.beluxvacc.org";
	const string uri = "/belux-gate-manager-api/swap_gate/";

	// Form the request.
	std::stringstream request;
	request << "POST " << uri << " HTTP/1.1\r\n";;
	request << "Host: " << host << "\r\n";
	request << "Content-Type: application/x-www-form-urlencoded\r\n";
	request << "Content-Length: " << (18 + callsign.length() + gate.length()) << "\r\n\r\n";
	request << "callsign=" + callsign + "&gate_id=" + gate << "\r\n";

	string response = GetHttpsRequest(host, uri, request.str(), false);
	return response;
}

string BeluxPlugin::GetHttpsRequest(string host, string uri, string request_string, bool expect_long_json)
{
	string data = "";
	try
	{
		// Initialize the asio service.
		boost::asio::io_service io_service;
		boost::asio::ssl::context context(boost::asio::ssl::context::sslv23);
		boost::asio::ssl::stream<tcp::socket> ssock(io_service, context);
		if (!SSL_set_tlsext_host_name(ssock.native_handle(), host.c_str()))
		{
			boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
			throw boost::system::system_error{ec};
		}

		// Get a list of endpoints corresponding to the server name.
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(host, "https");
		auto it = resolver.resolve(query);
		boost::asio::connect(ssock.lowest_layer(), it);
		ssock.lowest_layer().set_option(
			boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{timeout_value});
		ssock.lowest_layer().set_option(
			boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_SNDTIMEO>{timeout_value});

		// Do the SSL handshake
		ssock.handshake(boost::asio::ssl::stream_base::handshake_type::client);

		// Send the request.
		boost::asio::streambuf request;
		ostream request_stream(&request);
		request_stream << request_string;
		boost::asio::write(ssock, request);

		// Read the response line.
		boost::asio::streambuf response;
		response.prepare(4 * 1024 * 1024); //4MB
		boost::asio::read_until(ssock, response, "\r\n");

		// Check that response is OK.
		istream response_stream(&response);
		string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		string status_message;
		getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/" || status_code != 200)
		{
			throw exception("HTTPS status: " + status_code);
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(ssock, response, "\r\n\r\n");
		string header;
		while (getline(response_stream, header) && header != "\r")
			continue;

		// Write whatever content we already have to output.
		ostringstream stream;
		if (response.size() > 0)
		{
			stream << &response;
		}

		if (expect_long_json && stream.str().back() != ']')
		{
			// Read until EOF, writing data to output as we go.
			boost::system::error_code error;
			while (boost::asio::read(ssock, response,
			                         boost::asio::transfer_at_least(1), error))
			{
				stream << &response;
				if (stream.str().back() == ']')
					break;
			}
		}
		data = stream.str();
		string dbgmsg = "received: " + data;
		printDebugMessage("API", dbgmsg);

		return data;
	}
	catch (exception& e)
	{
		DisplayUserMessage("Belux Plugin", "HTTPS error", e.what(), true, true, true, false, false);
		return "HTTPS_ERROR";
	}
}

bool BeluxPlugin::OnCompileCommand(const char* sCommandLine)
{
	string buffer{sCommandLine};
	if (boost::algorithm::starts_with(sCommandLine, ".belux help"))
	{
		printMessage("-", "Belux CLI");
		printMessage("-", ".belux reload                          - reload json config");
		printMessage("-", ".belux force-sid                       - force fresh SID/RWY assignment");
		printMessage("-", ".belux gate(on/off)                    - enable/disable gate assignment");
		printMessage("-", ".belux climb(on/off)                   - enable/disable initial climb assignment");
		printMessage("-", ".belux mach(on/off)                    - enable/disable mach visualisation");
		printMessage("-", ".belux timeout <integer>               - set the API timeout value");
		printMessage("-", ".belux refreshgates                    - refresh gates from the API");
		printMessage("-", ".belux setgate <gate>                  - assign gate to selected aircraft");
		printMessage("-", ".belux alert_gates <blink|quiet|none>  - toggle flashing on gate reassignment");
		return true;
	}


	if (boost::algorithm::starts_with(sCommandLine, ".belux reload"))
	{
		loadJSONconfig();
		printMessage("Config", "reloaded JSON config file...");
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux pfp"))
	{
		ProcessFlightPlans();
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux reqbackup"))
	{
		string message = buffer.erase(0, 16);
		SendDiscordMessage(message);
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux setgate") || boost::algorithm::starts_with(
		sCommandLine, ".bsg"))
	{
		if (ControllerMyself().GetFacility() >= 3 || DEBUG_print)
		{
			bool longcmd = boost::algorithm::starts_with(sCommandLine, ".belux setgate");
			string gate = (longcmd ? buffer.erase(0, 15) : buffer.erase(0, 4));

			string selected_callsign = RadarTargetSelectASEL().GetCallsign();
			if (selected_callsign != "" && gate != "")
			{
				string result = SwapGate(selected_callsign, gate);
				string message;
				if (result != "HTTPS_ERROR")
				{
					message = selected_callsign + " succesfully assigned to gate " + gate;
					gatePlanner.gate_list[selected_callsign].gate = gate;
					gatePlanner.gate_list[selected_callsign].isFetched = true;

					EuroScopePlugIn::CFlightPlan fp = FlightPlanSelect(selected_callsign.c_str());
					fp.GetControllerAssignedData().SetFlightStripAnnotation(4, gate.c_str());
				}
				else
				{
					message = "Something went wrong when trying to assign gate " + gate + "  to " + selected_callsign;
				}
				printMessage("Gate assignment", message);
				return true;
			}
		}
		return false;
	}
	if (boost::algorithm::starts_with(sCommandLine, ".belux gateon") || boost::algorithm::starts_with(
		sCommandLine, ".belux gateoff"))
	{
		function_fetch_gates = (buffer.erase(0, 11) == "on");
		printMessage("Functionalities", "Gate assignment " + string(function_fetch_gates ? "enabled" : "disabled"));
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux climbon") || boost::algorithm::starts_with(
		sCommandLine, ".belux climboff"))
	{
		function_set_initial_climb = (buffer.erase(0, 12) == "on");
		printMessage("Functionalities",
		             "Initial climb assignement " + string(function_set_initial_climb ? "enabled" : "disabled"));
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux machon") || boost::algorithm::starts_with(
		sCommandLine, ".belux machoff"))
	{
		function_mach_visualisation = (buffer.erase(0, 11) == "on");
		printMessage("Functionalities",
		             "Mach visualisation " + string(function_mach_visualisation ? "enabled" : "disabled"));
		if (function_mach_visualisation)
			printDebugMessage("API", "fetched weather file " + utils.fetch_weather_file());
		return true;
	}
	if (boost::algorithm::starts_with(sCommandLine, ".belux machupdate"))
	{
		printDebugMessage("API", "fetched weather file " + utils.fetch_weather_file());
		return true;
	}
	if (boost::algorithm::starts_with(sCommandLine, ".belux rwysidon") || boost::algorithm::starts_with(
		sCommandLine, ".belux rwysidoff"))
	{
		function_check_runway_and_sid = (buffer.erase(0, 13) == "on");
		printMessage("Functionalities",
		             "RWY and SID assignment " + string(function_check_runway_and_sid ? "enabled" : "disabled"));
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux debugon") || boost::algorithm::starts_with(
			sCommandLine, ".dbgon")
		|| boost::algorithm::starts_with(sCommandLine, ".belux debugoff") || boost::algorithm::starts_with(
			sCommandLine, ".dbgoff"))
	{
		DEBUG_print = (buffer == ".dbgon" || buffer.erase(0, 12) == "on");
		printMessage("Functionalities", "Debug mode " + string(DEBUG_print ? "enabled" : "disabled"));
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux timeout"))
	{
		string buffer{sCommandLine};
		buffer.erase(0, 15);
		try
		{
			timeout_value = stoi(buffer);
			printMessage("Functionalities", "Set timeout to " + std::to_string(timeout_value));
			return true;
		}
		catch (exception& e)
		{
			return false;
		}
	}
	if (boost::algorithm::starts_with(sCommandLine, ".belux refreshgates") || boost::algorithm::starts_with(
		sCommandLine, ".rf"))
	{
		if (function_fetch_gates)
			FetchAndProcessGates();
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux force-sid"))
	{
		procedureAssigner->reprocess_all();
		force_new_procedure = true;
		return true;
	}

	if (boost::algorithm::starts_with(sCommandLine, ".belux alert_gates"))
	{
		string value = buffer.erase(0, string(".belux alert_gates ").length());
		if (value == "blink")
		{
			on_gate_change = 2;
			printMessage("Functionalities", "Alert on gate change: blink");
		}
		else if (value == "quiet")
		{
			on_gate_change = 1;
			printMessage("Functionalities", "Alert on gate change: quiet");
		}
		else if (value == "none")
		{
			on_gate_change = 0;
			printMessage("Functionalities", "Alert on gate change: none");
		}
		else
		{
			printMessage("Functionalities", "Alert on gate change: invalid (default blink)");
		}

		return true;
	}
	return false;
}


BeluxPlugin* gpMyPlugin = NULL;

void __declspec (dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	// create the instance
	*ppPlugInInstance = gpMyPlugin = new BeluxPlugin();
}


//---EuroScopePlugInExit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInExit(void)
{
	// delete the instance
	delete gpMyPlugin;
}
