﻿#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>

#include "EuroScopePlugIn.h"
#include "SidAllocation.h"

class ProcedureAssigner
{
private:
	const std::string airports[1] = {"EBBR"};
	std::set<std::string>* processed;
	std::map<std::string, std::vector<std::string>>* departure_runways;
	std::function<void(const std::string&)> debug_printer;
	/**
 * \brief Verifies if we should still process the flight plan. This checks, but does not modify the `processed` var,
 * ensure the caller inserts the callsign into this set.
 * \param flight_plan A flight plan to check
 * \param ignore_already_assigned Ignore the fact that this plan may already have a SID
 * \return true if the flight plan should be processed
 */
	bool should_process(const EuroScopePlugIn::CFlightPlan& flight_plan, bool ignore_already_assigned = false) const;
	SidAllocation sid_allocation;
	std::string get_runway(const EuroScopePlugIn::CFlightPlan& flight_plan, const std::string& sid_fix) const;

public:
	ProcedureAssigner(std::function<void(const std::string&)> printer);
	/**
	 * \brief Processes a flight plan, potentially assigning a new RWY and SID.
	 * This method is idempotent and can be called as often as desired on the same flight plan.
	 * Changes by controllers will not be overwritten.
	 * \param flight_plan Flight plan to process
	 * \return true if successfully processed
	 */
	bool process_flight_plan(const EuroScopePlugIn::CFlightPlan& flight_plan, bool force = false) const;
	/**
	 * \brief Fetches the SID allocations from the internet and parses these.
	 * \return Amount of allocation rules retrieved and parsed.
	 */
	size_t fetch_sid_allocation() const;
	/**
	 * \brief Marks all flight plans for processing on next update, regardless of previous state.
	 */
	void reprocess_all() const;
	/**
	 * \brief Handles a pilot disconnecting and freeing a callsign
	 * \param flight_plan The disconnected flight plan
	 */
	void on_disconnect(const EuroScopePlugIn::CFlightPlan& flight_plan) const;
	void set_departure_runways(const std::map<std::string, std::vector<std::string>>& active_departure_runways) const;
};
