#include "pch.h"
#include "ProcedureAssigner.h"
#include "BeluxUtil.hpp"

#include <algorithm>
#include <iostream>
#include <list>

bool ProcedureAssigner::should_process(const EuroScopePlugIn::CFlightPlan& flight_plan) const
{
	if (flight_plan.GetClearenceFlag())
		return false; // No reason to change it from under the ATCO

	if (std::find(std::begin(airports), std::end(airports), std::string(flight_plan.GetFlightPlanData().GetOrigin())) ==
		std::end(airports))
		return false; // Not an airport we handle

	if (!flight_plan.IsValid() || !flight_plan.GetCorrelatedRadarTarget().IsValid())
		return false; // Invalid flight plan

	if (processed->find(std::string(flight_plan.GetCallsign())) != processed->end())
		return false; // Already processed

	if (strcmp(flight_plan.GetTrackingControllerId(), "") != 0 && !flight_plan.GetTrackingControllerIsMe())
		return false; // Is tracked and not tracked by me

	if (flight_plan.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() > 1500)
		return false; // Flying: Altitude > 1500 feet

	if (flight_plan.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() == 0)
		return false; // Altitude == 0 -> uncorrelated? altitude should never be zero

	return true;
}

std::string ProcedureAssigner::get_runway(const EuroScopePlugIn::CFlightPlan& flight_plan,
                                          const std::string& sid_fix) const
{
	if (std::string(flight_plan.GetFlightPlanData().GetOrigin()) != "EBBR")
	{
		return departure_runways->at(0);
	}

	// Minor optimisation
	if (departure_runways->size() == 1)
		return departure_runways->at(0);

	const bool has_25R = std::binary_search(std::begin(*departure_runways), std::end(*departure_runways),
	                                        std::string("25R"));
	const bool has_19 = std::binary_search(std::begin(*departure_runways), std::end(*departure_runways),
	                                       std::string("19"));
	const bool has_07R = std::binary_search(std::begin(*departure_runways), std::end(*departure_runways),
	                                        std::string("07R"));
	const char wtc = flight_plan.GetFlightPlanData().GetAircraftWtc();

	if (has_25R && has_19)
	{
		if (wtc == 'H' || wtc == 'J')
		{
			// This is a simplification, but in accordance with the procedures.
			return "25R";
		}

		if (sid_fix == "ELSIK" || sid_fix == "NIK" || sid_fix == "DENUT" || sid_fix == "KOK" || sid_fix == "CIV")
		{
			return "25R";
		}

		return "19";
	}

	return has_07R ? "07R" : departure_runways->at(0);
}

ProcedureAssigner::ProcedureAssigner()
{
	processed = new std::set<std::string>();
	departure_runways = new std::vector<std::string>();
}

void ProcedureAssigner::process_flight_plan(const EuroScopePlugIn::CFlightPlan& flight_plan) const
{
	if (!should_process(flight_plan))
		return;

	const auto route = flight_plan.GetExtractedRoute();
	std::string sid_fix;
	const std::string sid_fixes[] = {"CIV", "NIK", "LNO"};
	for (int i = 0; i < route.GetPointsNumber(); i++)
	{
		std::string fix = route.GetPointName(i);
		if (std::binary_search(std::begin(sid_fixes), std::end(sid_fixes), fix))
		{
			sid_fix = fix;
			break;
		}
	}

	const std::string runway = get_runway(flight_plan, sid_fix);

	const auto flight_plan_data = flight_plan.GetFlightPlanData();
	const auto maybe_sid = sid_allocation.find(flight_plan_data.GetOrigin(),
	                                           sid_fix,
	                                           flight_plan_data.GetDestination(),
	                                           flight_plan_data.GetEngineNumber(),
	                                           runway);

	if (!maybe_sid.has_value())
		return; // Maybe we should log this, but can't at the moment...
	auto sid = maybe_sid.value();

	// TODO assign CFL
	std::string route_string = flight_plan.GetFlightPlanData().GetRoute();
	route_string.replace(route_string.find(sid_fix), sid_fix.length() - 1, sid.sid + "/" + runway);
	flight_plan.GetFlightPlanData().SetRoute(route_string.c_str());

	processed->insert(std::string(flight_plan.GetCallsign()));
}

size_t ProcedureAssigner::fetch_sid_allocation() const
{
	const std::string url = "https://beluxvacc.org/files/navigation_department/datafiles/SID_ALLOCATION.txt";
	const std::string allocation = BeluxUtil::https_fetch_file(
		"https://beluxvacc.org/files/navigation_department/datafiles/SID_ALLOCATION.txt");
	return sid_allocation.parse_string(allocation);
}

void ProcedureAssigner::reprocess_all() const
{
	processed->clear();
}

void ProcedureAssigner::on_disconnect(const EuroScopePlugIn::CFlightPlan& flight_plan) const
{
	processed->erase(std::string(flight_plan.GetCallsign()));
}

void ProcedureAssigner::set_departure_runways(const std::vector<std::string>& active_departure_runways) const
{
	(*departure_runways) = active_departure_runways;
}
