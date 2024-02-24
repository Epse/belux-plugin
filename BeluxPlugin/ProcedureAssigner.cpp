#include "pch.h"
#include "ProcedureAssigner.h"
#include "BeluxUtil.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <variant>

bool ProcedureAssigner::should_process(const EuroScopePlugIn::CFlightPlan& flight_plan,
                                       bool ignore_already_assigned) const
{
	if (flight_plan.GetClearenceFlag())
		return false; // No reason to change it from under the ATCO

	const auto adep = std::string(flight_plan.GetFlightPlanData().GetOrigin());
	if (std::find(std::begin(airports), std::end(airports), adep) ==
		std::end(airports))
		return false; // Not an airport we handle

	if (!flight_plan.IsValid() || !flight_plan.GetCorrelatedRadarTarget().IsValid())
		return false; // Invalid flight plan

	if (strcmp(flight_plan.GetTrackingControllerId(), "") != 0 && !flight_plan.GetTrackingControllerIsMe())
		return false; // Is tracked and not tracked by me

	if (flight_plan.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() > 1500)
		return false; // Flying: Altitude > 1500 feet

	if (flight_plan.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() == 0)
		return false; // Altitude == 0 -> uncorrelated? altitude should never be zero

	const std::string callsign = flight_plan.GetCallsign();
	if (!ignore_already_assigned)
	{
		const std::string route = flight_plan.GetFlightPlanData().GetRoute();
		debug_printer("Checking if we have a SID for " + callsign + " in " + route);
		const auto sids = sid_allocation.sids_for_airport(adep);
		for (auto& sid : sids)
		{
			if (route.find(sid) != std::string::npos)
			{
				debug_printer("Found a SID for " + callsign);
				return false; // We assume a SID was already assigned by a controller
			}
		}
		debug_printer("Did not find SID for " + callsign + " at " + adep);
	}

	return true;
}

std::optional<std::string> ProcedureAssigner::get_runway(const EuroScopePlugIn::CFlightPlan& flight_plan,
                                                         const std::string& sid_fix) const
{
	const std::string origin = flight_plan.GetFlightPlanData().GetOrigin();
	if (departure_runways->find(origin) == departure_runways->end())
	{
		return {};
	}
	const auto& origin_runways = departure_runways->at(origin);
	if (origin_runways.empty())
		return {};

	// Tiny optimisation, also takes care of our single-runway minors
	if (departure_runways->at(origin).size() == 1)
		return origin_runways.at(0);

	if (origin == "EBLG")
	{
		// For EBLG, we never assign 22R/04L by default
		const bool has_04R = std::binary_search(origin_runways.begin(), origin_runways.end(), std::string("04R"));
		const bool has_22L = std::binary_search(origin_runways.begin(), origin_runways.end(), std::string("22L"));

		if (has_04R)
			return "04R";

		if (has_22L)
			return "22L";

		return origin_runways.at(0); // IDK what to do here woops
	}

	// Should only get here at EBBR
	const bool has_25R = std::binary_search(std::begin(origin_runways), std::end(origin_runways),
	                                        std::string("25R"));
	const bool has_19 = std::binary_search(std::begin(origin_runways), std::end(origin_runways),
	                                       std::string("19"));
	const bool has_07R = std::binary_search(std::begin(origin_runways), std::end(origin_runways),
	                                        std::string("07R"));
	const char wtc = flight_plan.GetFlightPlanData().GetAircraftWtc();

	if (has_25R && has_19)
	{
		if (wtc == 'H' || wtc == 'J')
		{
			// This is a simplification, but in accordance with the procedures.
			return "25R";
		}

		if (sid_fix == "ELSIK" || sid_fix == "NIK" || sid_fix == "HELEN" || sid_fix == "DENUT" || sid_fix == "KOK" ||
			sid_fix == "CIV")
		{
			return "25R";
		}

		return "19";
	}

	return has_07R ? "07R" : origin_runways.at(0);
}

ProcedureAssigner::ProcedureAssigner(std::function<void(const std::string&)> printer)
{
	departure_runways = new std::map<std::string, std::vector<std::string>>();
	debug_printer = std::move(printer);
}

// TODO: a lot of the data in this loop should really be cached between planes in one iteration...
std::optional<SidEntry> ProcedureAssigner::process_flight_plan(const EuroScopePlugIn::CFlightPlan& flight_plan,
                                                               bool force)
{
	const std::string callsign = flight_plan.GetCallsign();

	if (!should_process(flight_plan, force))
	{
		debug_printer(callsign + ": Unconcerned during process");
		return {};
	}

	const auto maybe_sid = suggest(flight_plan, force);

	if (!maybe_sid.has_value())
	{
		debug_printer(callsign + ": Empty Sid entry during process");
		return {};
	}

	const auto sid = maybe_sid.value();

	auto flight_plan_data = flight_plan.GetFlightPlanData();
	std::string route_string = flight_plan_data.GetRoute();
	/*
	 * We now need to turn our route into something like CIV5C/25R CIV ...
	 * Whilst keeping in mind our flight plan may be any of the following funsies:
	 * - EBBR/07L CIV MEDIL
	 * - CIV MEDIL
	 * - CIV
	 * - CIV8J
	 * - Or literally anything else
	 *
	 * We only know that somewhere in there, we *should* find 'CIV '.
	 * We will thus simply remove everything before the SID exit fix, and start afresh
	 */
	const size_t sid_pos = route_string.rfind(sid.exit_point);
	if (sid_pos >= route_string.length())
	{
		debug_printer(callsign + ": No SID exit during processing");
		return {};
	}

	size_t end = route_string.find(' ', sid_pos);
	end = end == std::string::npos ? sid_pos + sid.exit_point.length() : end + 1;

	route_string = std::string(flight_plan_data.GetOrigin()) + "/" + sid.rwy
		+ " " + sid.sid
		+ " " + sid.exit_point
		+ " " + route_string.substr(end);
	flight_plan_data.SetRoute(route_string.c_str());

	return flight_plan_data.AmendFlightPlan() ? sid : std::optional<SidEntry>{};
}

size_t ProcedureAssigner::fetch_sid_allocation() const
{
	const std::string url = "https://beluxvacc.org/files/navigation_department/datafiles/SID_ALLOCATION.txt";
	const std::string allocation = BeluxUtil::https_fetch_file(url);
	return sid_allocation.parse_string(allocation);
}

size_t ProcedureAssigner::setup_lara(bool always) const
{
	if (!always && lara_parser.count() > 0)
		return lara_parser.count();

	// Getting the DLL file folder
	char dll_path_file[_MAX_PATH];
	GetModuleFileNameA(reinterpret_cast<HINSTANCE>(&__ImageBase), dll_path_file, sizeof(dll_path_file));
	std::string lara_path = dll_path_file;
	lara_path.resize(lara_path.size() - strlen("Belux\\Belux.dll"));
	lara_path += "\\TopSky\\TopSkyAreasManualAct.txt";
	std::ifstream ifs(lara_path);
	const std::string allocation_file((std::istreambuf_iterator<char>(ifs)),
	                                  (std::istreambuf_iterator<char>()));
	return lara_parser.parse_string(allocation_file);
}

void ProcedureAssigner::reprocess_all()
{
	cache.erase(cache.begin(), cache.end());
}

void ProcedureAssigner::on_disconnect(const EuroScopePlugIn::CFlightPlan& flight_plan)
{
	if (const auto found = cache.find(std::string(flight_plan.GetCallsign())); found != cache.end())
		cache.erase(found);
}

void ProcedureAssigner::set_departure_runways(
	const std::map<std::string, std::vector<std::string>>& active_departure_runways)
{
	(*departure_runways) = active_departure_runways;
	airports.erase(airports.begin(), airports.end());


	for (const auto& [fst, snd] : active_departure_runways)
	{
		airports.insert(fst);
	}

	cache.erase(cache.begin(), cache.end());
}

std::optional<SidEntry> ProcedureAssigner::suggest(const EuroScopePlugIn::CFlightPlan& flight_plan,
                                                               bool ignore_already_assigned)
{
	const std::string callsign = flight_plan.GetCallsign();
	// See if we have it cached
	if (cache.find(callsign) != cache.end())
	{
		return cache.at(callsign);
	}

	// Dit not have it cached, recalculate
	const auto route_text = std::string(flight_plan.GetFlightPlanData().GetRoute());
	std::string sid_fix;
	const auto sid_fixes = sid_allocation.fixes_for_airport(flight_plan.GetFlightPlanData().GetOrigin());

	typedef boost::split_iterator<std::string::const_iterator> SplitIter;
	for (SplitIter i = boost::make_split_iterator(route_text, boost::token_finder(boost::is_space()));
	     i != SplitIter(); ++i)
	{
		auto route_element = boost::copy_range<std::string>(*i);
		for (auto& fix : sid_fixes)
		{
			if (route_element.find(fix) != std::string::npos)
			{
				sid_fix = fix;
				goto found_sid; // I know, I know
			}
		}
	}
found_sid:

	if (sid_fix.empty())
	{
		debug_printer(callsign + ": No sid fix in suggest");
		cache.insert_or_assign(callsign, std::optional<SidEntry>{});
		return {};
	}

	const auto maybe_runway = get_runway(flight_plan, sid_fix);
	if (!maybe_runway.has_value())
	{
		debug_printer(callsign + ": No RWY in suggest");
		cache.insert_or_assign(callsign, std::optional<SidEntry>{});
		return {};
	}

	const auto& runway = maybe_runway.value();

	const auto flight_plan_data = flight_plan.GetFlightPlanData();
	time_t raw_time;
	time(&raw_time);
	// Add 20 minutes, in seconds
	raw_time += PREACTIVE_MINUTES * 60;
	// Get a tm struct for now in UTC
	tm now;
	gmtime_s(&now, &raw_time);

	const auto areas = lara_parser.get_active(now);

	auto entry = sid_allocation.find(flight_plan_data.GetOrigin(),
	                                       sid_fix,
	                                       flight_plan_data.GetDestination(),
	                                       flight_plan_data.GetEngineNumber(),
	                                       runway, now, areas);
	cache.insert_or_assign(callsign, entry);
	return entry;
}
