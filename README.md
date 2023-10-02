# Belux Euroscope Plugin

## Features

### SID and Runway Selection

On startup, the plugin will fetch a SID allocation file from the Belux navdata site.
This will then be used to continuously calculate a suggested runway and SID for each of your active aerodromes.
Suggestions are calculated based on the active runway in Euroscope and take into account TRA restrictions, time of day and weekday, engine count, weight class, runway, ...
Runway selections follow the published aerodrome procedures.

These suggestions can be shown using the Tag Item `Belux / Procedure Suggestion`,
which you may add to your departure list.
This entry will take the `NON_CONCERNED` colour if suggestion and selected procedure match,
or the `INFORMATION` colour in case of a mismatch.
You can pair this item with the `Belux / Assign RWY/SID` action (e.g. on left click)
to then assign the suggestion to the aircraft.

You may also set the `rwy_sid_assigner` flag to `true` in your `belux_config.json` to enable automatic assignment.
This will automatically assign the suggestion to every plane once, unless they already have a SID in their flightplan, regardless of if this is controller assigned or prefiled.

A third method exists to assign suggestions, this is the `.belux force-sid` command.
This acts as if the controller clicked the `Assign RWY/SID` tag function for every flight.
This will override controller choices, be careful.

#### Restrictions

The plugin will under no circumstance modify the flight plan of a flight matching any of these conditions:
- Clearence flag set
- Not departing from a Belux airport
- Invalid flightplan or invalid radar target
- Assumed by a controller that's not me
- Airborne (defined as above 1500ft)
- Underground

#### Data sources

The plugin takes active runways and airports from Euroscope, active TSA/TRA is determined by parsing the `TopSkyAreasManualActivation.txt`.