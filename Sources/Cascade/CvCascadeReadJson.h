#pragma once
#ifndef CV_CASCADE_READ_JSON_H
#define CV_CASCADE_READ_JSON_H

#include <string>
#include "CvCascadeEnabler.h" // CvEntityAvailability

//
//	⛔ TEMPORARY harness -- the DLL-side readJson (#430). Parses a REAL entity JSON from Assets/Data via the
//	vendored picojson into a CvEntityAvailability (the uniform `requires`/`allowed` surface), independent of the
//	XML-loaded CvXxxInfo objects (fresh structures, cascade-engine-430 §2b/§3 -- so JSON and XML never collide on
//	one Info). The diagnostic gate endpoints + the doTurn slice run cascadeBuildable / cascadeEvalCondition over
//	the result and SHADOW it against the legacy gate. Purge/replace when the full readJson loader lands.
//
//	Coverage today: every count atom the condition engine evaluates (BUILDING/UNIT/TECH/BONUS/CIVIC/RELIGION/
//	CORPORATION/POPULATION/CITY at world/team/empire/city scope) + the predicates + the `allowed` self-cap.
//	Genuinely-unknown leaves (membership sugar, latitude/existedFor/..., unmodelled domains) are DROPPED and
//	summarized in szNotes ("pending: ...") -- never silently treated as satisfied.
//

// Locate an entity's JSON (by type prefix -> Assets/Data/<folder>, searching any grouping sub-folders) and parse
// its availability (requires.build/operate + allowed) into kOut. Works for any gate entity (BUILDING_/UNIT_/
// TECH_/CIVIC_/PROJECT_/RELIGION_/CORPORATION_). Returns false if the file can't be found/parsed; szNotes carries
// the per-leaf diagnostics. Call on the GAME thread (it reads GC type indices + the live mod path).
bool cascadeReadJsonAvailability(const char* szTypeKey, CvEntityAvailability& kOut, std::string& szNotes);

// GENERATION (partial): is this entity obsolete for the team -- i.e. has the team researched a tech whose JSON
// `obsoletes` edge names it? Lazily builds a reverse index from the tech JSONs on first call (game thread,
// cached). eDomain uses the CountDomain values (COUNTDOMAIN_BUILDING / COUNTDOMAIN_UNIT). The clean cascade
// answer to "is X obsolete" -- parsed from the JSON, NOT the legacy CvTeam::isObsoleteBuilding.
bool cascadeIsObsoleteForTeam(int eDomain, int iEntity, int iTeam);

// GENERATION (SpecialBuilding GROUP cap, owner 2026-06-17): is this building still allowed under its GROUP's cap?
// A SpecialBuilding is a building group (e.g. the 15 elite universities -> pick ONE); the member authors its group
// FORWARD (identity.specialBuildingType), the group authors the cap (allowed:{scope:N}). Returns false once the
// player holds N of the whole group at the cap's scope. Coexists with the member's own self-cap. True when the
// building is in no group, or its group is uncapped. Lazy group index (build JSON scan, cached), game thread.
bool cascadeBuildingGroupAllows(int iBuilding, const CvCascadeContext& kCtx);

// True when the building is REPLACED in the context city -- a successor (a building whose `replaces.buildings` names
// it) is active there (legacy CvCity.cpp:2917). The verdict's destructive `replaces` subtraction. Lazy index, game thread.
bool cascadeIsReplacedInCity(int iBuilding, const CvCascadeContext& kCtx);

// GENERATION (forward enables): is the entity tech-reachable -- has the team researched a tech whose JSON
// `enables` names it? Returns true when the entity has no tech enabler (enabled by non-tech / always).
// NOT used for buildability anymore: the multi-tech AND confirm lives in requires.build TECH atoms (enables
// cannot encode AND, so an OR-over-enables check over-offered -- the MODERN_ARMOR/tank-line bug). Retained for
// the upcoming GENERATION/frontier pass (proposing tech-unlocked children). eDomain uses the CountDomain values.
bool cascadeTechReachable(int eDomain, int iEntity, int iTeam);

// GENERATION (units): the ALL-BRANCHES-ALIVE upgrade resolver -- our CLEAN model of the legacy
// CvCity::allUpgradesAvailable intent (canTrainInternal:2265), minus its caching / supersededBy / forceUpgrade
// cruft (owner: "leave the raving old lunatic where it is"). A unit stays on the build list iff it is buildable
// AND NOT every upgrade branch is "alive": a branch is alive if some unit reachable along it (the next unit or
// one of its upgrades, recursively) is buildable. Hidden only when EVERY branch is alive (fully superseded);
// kept if any branch is dead -- which is what produces the band of recent units (infantry + modern_infantry).
// Driven purely by `succession.upgradesTo`; band errors are fixed in the DATA. Lazy upgrade index, game thread.
bool cascadeUnitTrainable(int iUnit, const CvCascadeContext& kCtx);

// The doTurn harness: parse two sample buildings, run cascadeBuildable vs the capital city, shadow the cap +
// the legacy canConstruct, stream [READJSON] lines to Cascade.log + /events. Gated by gPlayerLogLevel.
void cascadeReadJsonSlice();

// ===================== §14 H AUTO-PLACEMENT SHADOW (B-i) =====================
// The RUNTIME twin of the buildability sweep: the sweep only tests !hasBuilding buildability, so it never exercises
// the per-turn state maintainers that MUTATE the building set (changeHasBuilding). B-i is the riskiest cluster --
// auto-placement -- driven by TWO legacy maintainers in CvCity::doAutobuild: the bAutoBuild loop (BuildingsRepo::
// autoBuildings) and the property-band system (checkPropertyBuildings, CvPropertyInfo PropertyBuildings). The shadow
// compares, per city, the cascade's would-place decision against the maintainers' realized presence -- so the
// maintainers can be DELETED at the hard switch only once the cascade replicates them (map-before-delete, §14 H).

// Fill outBuildings with the AUTO-PLACED building roster (the two maintainers' targets, deduped), and outKind with a
// parallel bitmask per entry: bit0 (1) = legacy bAutoBuild loop target, bit1 (2) = property-band target. Game thread.
void cascadeAutoPlacedRoster(std::vector<int>& outBuildings, std::vector<int>& outKind);

// For one (auto-placed building, context city): would the CASCADE auto-place it here? (autoBuild marker + requires
// build/operate + allowed cap + not obsolete/replaced + group-allowed.) Sets bCascadeWouldPlace and returns the
// reason token for divergence triage: "place" / "noMarker" / "requiresBuild" / "requiresOperate" / "allowedCap" /
// "obsolete" / "replaced" / "groupCap". The legacy side (presence) is hasBuilding -- the caller reads it. Game thread.
const char* cascadePlacementReason(int iBuilding, const CvEntityAvailability& kAvail,
	const CvCascadeContext& kCtx, int iTeam, bool& bCascadeWouldPlace);

// Per-turn placement shadow (the cascadeReadJsonSlice twin for B-i): for the active player's cities x the auto-placed
// roster, emit [PLACEMENT] lines where cascade-would-place diverges from legacy presence. Gated by gPlayerLogLevel
// (>=1 headline counts, >=2 per-divergence). Streams to Cascade.log + /events.
void cascadePlacementShadow();

// ===================== §14 H DORMANCY SHADOW (B-ii) =====================
// The sweep's #1-recommended next shadow (state-mapping-2026-06-18.md): a BUILT building can be present-but-INACTIVE.
// Legacy folds three mechanisms into `isActiveBuilding` (= !isDisabledBuilding && hasBuilding -- resource-disabling +
// replacement-suppression via setDisabledBuilding) and `hasFullyActiveBuilding` (adds !isReligiouslyLimitedBuilding --
// religious dormancy). The cascade end-state is `requires.operate` dormancy (cascadeOperational). This shadow diffs,
// per BUILT building per city, cascade-active vs legacy-active -- the runtime twin for B-ii (cascade-mapping §B-ii).

// For one BUILT building in the context city: is it ACTIVE under the cascade (requires.operate holds)? Sets
// bCascadeActive and returns the reason token ("active" / "requiresOperate"). Game thread.
const char* cascadeDormancyReason(const CvEntityAvailability& kAvail, const CvCascadeContext& kCtx, bool& bCascadeActive);

// Why is the building legacy-DORMANT in this city: "religiousLimit" (isReligiouslyLimitedBuilding) / "disabled"
// (isDisabledBuilding -- resource/replacement) / "active" (fully active). The legacy-side cause-tag. Game thread.
const char* cascadeDormancyLegacyReason(const CvCity* pCity, int iBuilding);

// Per-turn dormancy shadow (the placement-shadow twin for B-ii): for the active player's cities x the buildings they
// HAVE, emit [DORMANCY] lines where cascade-active (requires.operate) diverges from legacy hasFullyActiveBuilding.
// Gated by gPlayerLogLevel (>=1 headline, >=2 per-divergence). Streams to Cascade.log + /events.
void cascadeDormancyShadow();

// ===================== LIVE STATE EVENT FEED (the "cameras") =====================
// Per-turn gated emitter streaming the broad game state to Cascade.log + /events so an autoplay session is fully
// narratable from the wire (the total-observability bar; state-mapping-2026-06-18.md gaps). Lines:
//   [STATE/game]  turn/state/era/winner/victory     -- end-detection (gap #1)             (gPlayerLogLevel >= 1)
//   [STATE/fin]   per player: gold/rate/maint/upkeep/strike/financialTrouble  (gap #3)    (>= 1)
//   [STATE/dip]  per player: AI_getAttitudeVal to each other player      (gap #5)          (>= 2)
//   [STATE/city] per city: happy/health/anger/timers/disorder/food-bar/GPP/culture (gap #2) (>= 2)
void cascadeStateLog();

#endif // CV_CASCADE_READ_JSON_H
