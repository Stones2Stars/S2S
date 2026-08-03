#pragma once

#ifndef CyState_h__
#define CyState_h__

#include <string>

//
//	CyState -- the Python LIVE-STATE surface: the "what do I HAVE, right now?" half of the GAME-OBJECT read role
//	(patterns.md § THE TWO READ ROLES), exposed to script. Sibling of CyEnabler, which answers the "can I?" half.
//
//	⛔ THIS IS A NEW SURFACE, NOT A WIDENED BINDING ([DEC-cy-not-fixed]). The legacy per-type wrappers
//	(CyCity::getYieldRate, CyPlayer::getCommerceRate, ...) are NOT extended and NOT reused. Like CyEnabler it is
//	deliberately ID-BASED -- it takes plain player / city ids and holds no CyCity or CyPlayer -- so the legacy
//	wrappers can be CUT AWAY without touching it.
//	⚑ THAT IS WHY A CALLBACK HANDS OVER AN IDENTITY, NOT A HANDLE. The engine->Python CALLBACK direction is KEPT
//	(patterns.md: the cut is DIRECTIONAL), and a cut wrapper carries ZERO defs -- so a script handed one could ask
//	it nothing, not even which object it is. The event reports therefore push the (owner, id) PAIR this surface is
//	addressed by (Cy::PyIdentity, CvPython.h), which is what makes the kept direction usable at all.
//
//	THE GRAMMAR is the game-object half's, unchanged:
//	  - ONE READ PER GROUP, and the getter IS the group. There is NO scalar getter per channel; a script wanting
//	    one value indexes the returned list. The surface therefore grows by GROUPS (a handful), never by channels
//	    (hundreds) -- which is the whole point of the standardization.
//	  - THE EXISTING ENGINE ENUM INDEXES THE RESULT, never the call: getYields()[YieldTypes.YIELD_FOOD]. A family
//	    with no engine enum is indexed by its own kind enum, and the name says so (get<Family>Kinds).
//	  - A group read fills a caller-owned array on the C++ side; in script that is a plain Python LIST, returned
//	    whole. One call in, the whole group out.
//
//	⛔ SCOPE IS A SELECTOR, NEVER A NAME FRAGMENT ([DEC-scope-is-an-axis]). A group both scopes carry takes
//	(iPlayer, iCity) and reads the CITY when iCity >= 0, the EMPIRE when iCity < 0 -- the same selector shape the
//	cache endpoints already use (?player=N[&city=M], http-endpoints.md). A group only ONE scope carries takes only
//	the ids that scope needs, so a script cannot ask a scope for a group it does not have.
//
//	⛔ EVERY VALUE IS x100 NATIVE ([DEC-fixedpoint-x100]) -- there is no `100` in any name, no scale variant, and
//	no getter reduces. A script divides by 100 at the point it displays or compares against a whole game count.
//	A PERCENT is NOT scaled, so a percent-unit channel is already the number you want.
//
//	⛔ NO FINAL-STATE VALUE IS IN ANY GROUP (patterns.md rule 6). angryPopulation and healthRate are calculations
//	DOWNSTREAM of the four wellbeing channels, not slots in the array -- folding one in would put a computed
//	outcome in a slot meaning "a channel a source deposited into" and hide the opposing-pair structure.
//
//	⛔ EVERY READ IS A BARE FETCH -- nothing here gates, ensures or recomputes, exactly as on the C++ side, so a
//	missed invalidation shows up in script as a visibly wrong number rather than being silently repaired at the
//	boundary (state-repositories.md; [DEC-no-self-heal]).
//	⚖ A PROJECTION OVER A KNOWN IMMINENT CHANGE IS NOT A WHAT-IF (owner). getRealizedWellbeing takes the
//	population the city is ABOUT TO HAVE, because the engine's own realizedWellbeing does and because the anger
//	side is non-linear in population -- no consumer can derive it from the current-population answer. That is a
//	different thing from the banned shape: the AI's "what if I BUILT this?" is the cascade's expected* read on
//	the what-if plane (patterns.md), never a widened live-state getter. The test is whether the argument names a
//	CANDIDATE (banned here) or a state the object is already headed for (fine).
//
//	BOOST: this file uses ONLY the `python::` alias, never a bare `boost::` and never a using-directive -- the
//	tree carries TWO Boosts and an unqualified name can resolve to the wrong one through the PCH (engine.md).
//

class CyState
{
public:
	CyState() {}

	// ---- GROUPS both scopes carry: iCity >= 0 reads the CITY, iCity < 0 reads the EMPIRE ----
	python::list getYields(int iPlayer, int iCity) const;
	python::list getCommerces(int iPlayer, int iCity) const;
	python::list getWellbeing(int iPlayer, int iCity) const;
	python::list getDefenseKinds(int iPlayer, int iCity) const;
	python::list getMaintenanceKinds(int iPlayer, int iCity) const;
	python::list getBuildRateKinds(int iPlayer, int iCity) const;
	python::list getCombatKinds(int iPlayer, int iCity) const;
	python::list getExperienceKinds(int iPlayer, int iCity) const;
	python::list getRevolutionKinds(int iPlayer, int iCity) const;
	python::list getTradeRouteKinds(int iPlayer, int iCity) const;
	python::list getScalars(int iPlayer, int iCity) const;

	// ---- CITY-only groups ----
	python::list getHealKinds(int iPlayer, int iCity) const;
	python::list getUnderworldKinds(int iPlayer, int iCity) const;
	python::list getVisionKinds(int iPlayer, int iCity) const;
	// The REALIZED wellbeing: the deposits above PLUS the raw-state inputs no deposit produces (the anger
	// percents, the espionage counters, event anger -- modifier.md §2b). Distinct from getWellbeing on purpose;
	// a script that adds the two double-counts.
	// iExtraPopulation projects the answer onto a population the city is about to reach (+1 growing, -1
	// starving, 0 for the current state) -- the overcrowding and per-citizen anger terms are non-linear in
	// population, so this cannot be derived from the 0 answer.
	python::list getRealizedWellbeing(int iPlayer, int iCity, int iExtraPopulation) const;
	// The city's yield MODIFIER percents, indexed by YieldTypes -- the multiplier a base yield is scaled by.
	// A PERCENT, so it is NOT x100 and a reader never divides it ([DEC-fixedpoint-x100]).
	python::list getYieldModifiers(int iPlayer, int iCity) const;
	int getSight(int iPlayer, int iCity) const;   // the city's sight BUDGET (vision.md)

	// ---- THE CURRENT SELECTION, as an IDENTITY. ----
	// ⛔ Asked of the library, NOT of the EXE's CyInterface. getHeadSelectedCity/Unit there hand back a Cy*
	// HANDLE, and those wrappers carry ZERO defs -- so a script is given an object it cannot ask anything, not
	// even which one it is. That return type is the closed EXE's and cannot be changed, so the only way the
	// selection is reachable at all is to answer it HERE, in the (owner, id) pair every read on this surface
	// takes. It is the same identity a callback hands over (Cy::PyIdentity), from the other direction.
	// ⚠ Answers [-1, -1] when nothing is selected -- a real owner/id pair is never negative, so a caller tests
	// the id rather than inferring emptiness from a missing value.
	python::list getHeadSelectedCityId() const;
	python::list getHeadSelectedUnitId() const;

	// ---- ENUMERATION: which cities a player HAS, by id. ----
	// ⛔ The prerequisite of every id-based city read on this surface, and it is structural rather than
	// convenience: this library is deliberately ID-BASED so the legacy wrappers can be cut, but the cut leaves
	// CyCity carrying ZERO defs -- a registration-only marshalling handle. So a script handed a CyCity cannot
	// ask it anything, not even its own id, and player.cities() therefore hands back a list nothing can read.
	// Without an id enumeration the whole surface is unreachable for any screen that LISTS cities, which is most
	// of them. The ids are stable within a game and are what every other read here takes.
	python::list getCityIds(int iPlayer) const;

	// ---- CITY RANK groups: where this city places among its OWNER'S cities for each channel. ----
	// ⛔ The engine enum indexes the RESULT, never the call (grammar rule 2 above), so a rank read hands back the
	// WHOLE group and a script indexes it -- getYieldRateRanks(p, c)[YieldTypes.YIELD_FOOD]. A per-channel
	// findYieldRateRank(p, c, eYield) would put the channel in the CALL, which is the per-channel scalar shape
	// this surface exists to delete.
	// ⚠ A rank is an ORDINAL (1 = highest), not an amount -- so it is NOT x100 and a reader never divides it.
	python::list getYieldRateRanks(int iPlayer, int iCity) const;
	python::list getBaseYieldRateRanks(int iPlayer, int iCity) const;
	python::list getCommerceRateRanks(int iPlayer, int iCity) const;

	// ---- CITY plain FACTS: genuine lone values, so they stay bare typed reads (patterns.md category 4) rather
	// than being forced into a group that would mean nothing -- the getSight precedent. ----
	// A coordinate is the one PAIR here: x and y are meaningless apart, so they cross as one [x, y] list rather
	// than as two getters.
	python::list getCityPosition(int iPlayer, int iCity) const;
	int getCityPopulation(int iPlayer, int iCity) const;          // citizens; a whole game count, NOT x100
	int64_t getCityRealPopulation(int iPlayer, int iCity) const;  // the displayed head-count; exceeds 32 bits
	int getGreatPeopleRate(int iPlayer, int iCity) const;
	int getGreatPeopleProgress(int iPlayer, int iCity) const;
	// ⚠ Takes the unit id in the CALL, unlike a group read, and deliberately: this is not a channel family but a
	// SPARSE id-keyed quantity over the whole unit registry (thousands of entries, of which a handful are great
	// people). Handing back a list indexed by UnitTypes would cross thousands of zeros to answer about five.
	int getGreatPeopleUnitProgress(int iPlayer, int iCity, int iUnit) const;
	int getGreatPeopleThresholdNonMilitary(int iPlayer) const;
	int getMilitaryHappinessUnits(int iPlayer, int iCity) const;
	// Post-conquest resistance: whether it is running. The REMAINING TURNS are COUNTDOWN_OCCUPATION in the
	// countdown group below -- a turn counter is not a lone fact, and every other counter reads there.
	bool isOccupation(int iPlayer, int iCity) const;

	// ---- The city's RAW-STATE groups: live engine counters and the current order. ----
	// These are the state no deposit produces, so they answer off the city directly rather than off a package --
	// but they obey the same grammar as the deposit groups: one read per group, the group's own kind enum
	// indexes the RESULT (CityCountdownKind / CityOrderRead), and the surface grows by groups, not counters.
	// ⚠ NOT x100. Every slot is a whole game count -- a turn number, a citizen count, a hammer total -- so a
	// reader never divides one ([DEC-fixedpoint-x100] scales AMOUNTS; these are counts).
	python::list getCountdowns(int iPlayer, int iCity) const;
	python::list getOrder(int iPlayer, int iCity) const;
	python::list getGrowth(int iPlayer, int iCity) const;
	python::list getCulture(int iPlayer, int iCity) const;
	// Has this team SEEN the city. Fog state, so it is live and per-team -- an unrevealed city is one a screen
	// may know of but must not name.
	bool isCityRevealed(int iPlayer, int iCity, int iTeam) const;
	// The city governor's EMPHASIS flags -- what the player told this city to prioritise. Per-city view state
	// the engine stores, so it reads here; eEmphasize selects one flag, the sparse-selector shape again.
	bool isEmphasize(int iPlayer, int iCity, int iEmphasize) const;
	// The hurry QUOTE for one method: may I, and at what price. eHurry is a SELECTOR in the call because the
	// hurry registry is sparse and a city is asked about one method at a time -- the getGreatPeopleUnitProgress
	// shape, for the same reason.
	python::list getHurryQuote(int iPlayer, int iCity, int iHurry) const;

	// ---- The city screen's own VIEW state: which filter/sort/grouping the player left its lists on. ----
	// ⚠ Genuinely per-city state that the engine stores, not authored data and not a derived value -- so it is
	// live state and belongs here. ⛔ These are the READS only. The matching setters are a WRITE, and no write
	// surface exists yet ([roadmap] scope decision 6), so the lists READ correctly and do not yet re-sort on a
	// click. Reads run on every redraw; the writes fire only on user action, which is why the split is usable.
	bool isProductionAutomated(int iPlayer, int iCity) const;
	int getOrderQueueLength(int iPlayer, int iCity) const;
	bool getBuildingListFilterActive(int iPlayer, int iCity, int iFilter) const;
	int getBuildingListSorting(int iPlayer, int iCity) const;
	bool getUnitListFilterActive(int iPlayer, int iCity, int iFilter) const;
	int getUnitListGrouping(int iPlayer, int iCity) const;
	int getUnitListSorting(int iPlayer, int iCity) const;

	// ---- EMPIRE-only groups ----
	python::list getUpkeepKinds(int iPlayer) const;
	python::list getCostKinds(int iPlayer) const;
	python::list getStateReligionKinds(int iPlayer) const;
	python::list getDiplomacyKinds(int iPlayer) const;
	python::list getDurationKinds(int iPlayer) const;
	python::list getAirKinds(int iPlayer) const;
	python::list getCaptureKinds(int iPlayer) const;
	python::list getCargoKinds(int iPlayer) const;
	python::list getExtraYieldThresholds(int iPlayer) const;
	python::list getLessYieldThresholds(int iPlayer) const;

	// ---- Plain live FACTS: genuine lone values, so they stay bare typed reads (patterns.md category 4), never
	// forced into a group that would mean nothing. ----
	int getActivePlayer() const;
	int getGameTurn() const;
	// Is this player still in the game, and whose team are they on. Both are asked constantly by anything that
	// walks the player range, and neither belongs to a group -- they are lone facts about the slot itself.
	bool isPlayerAlive(int iPlayer) const;
	int getPlayerTeam(int iPlayer) const;
	bool isFinalInitialized() const;   // is the game up enough to be asked / shown a message
	// The closed CONSTANTS block (python-read-map: a small closed set, trivially served by the library).
	// Compile-time engine limits, so they are bare reads with no owner and no scope.
	int getMAX_PLAYERS() const;
	int getMAX_PC_PLAYERS() const;
	int getMAX_TEAMS() const;
	int getMAX_PC_TEAMS() const;
	int getBARBARIAN_PLAYER() const;

	// The global DEFINES. They sit on the live-state half rather than the vocabulary because a define is a
	// LIVE option -- user-changeable mid-game through the BUG bridge, which is exactly why nothing STATIC may be
	// gated on one (python-read-map). The READS are in scope; the writes are not.
	int getDefineINT(const std::string& szName) const;
	float getDefineFLOAT(const std::string& szName) const;
	int getAIAutoPlay(int iPlayer) const;
	std::wstring getPlayerName(int iPlayer) const;
	std::wstring getCityName(int iPlayer, int iCity) const;

	static void pythonPublish();
};

#endif // CyState_h__
