# Belux Euroscope Plugin

## Open Source

The original version of this plugin by Nicola Macoir was published with a license note of GNU GPL.
On request of a user of this plugin, the source code was published.

This is not a typical Open Source project.
Expect no response to issues or PRs.
This project only exists for the benefit of Belux vACC.
I will try to be a good steward, but keep the above in mind.

All code in here is unless otherwise noted licensed as in the LICENSE file.

## Features

### Initial Climb Selection

The plugin assigns the initial climb for several airports,
if the flightplan in question is departing from a covered airport,
the flightplan and radar target are "valid" (in Euroscope terms, valid doesn't mean much here),
it's not assumed by another controller, not flying and not moving.

It follows the following rules:

- ELLX: FL40
- EBCI: FL40
- EBAW: FL30
- EBKT: FL30
- EBBR, EBOS or EBLG: FL60 if QNH > 995hPa; FL70 if 959hPa < QNH <= 995; FL80 otherwise

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

If at any time these suggestions appear wrong to you, the command `.belux fresh-sid` (not to be confused by `force-sid`) will re-do all the math to suggest procedures.
This may be helpful in edge cases where a bad suggestions is cached.

This features absolutely relies on every valid SID being in the SID_Allocation file in order to detect when a controller has selected a valid sid and prevent overwriting.

#### Restrictions

The plugin will under no circumstance modify the flight plan of a flight matching any of these conditions:

- Clearence flag set
- Not departing from a Belux airport
- Invalid flightplan or invalid radar target
- Assumed by a controller that's not me
- Airborne (defined as above 1500ft)
- Underground

#### Data sources

The plugin takes active runways and airports from Euroscope, active TSA/TRA is determined by parsing the `TopSkyAreasManualActivation.txt`. It shifts the activation times of these areas forwards by 20 minutes, in an attempt to avoid reroutes on departures and unnecessary long departures.
