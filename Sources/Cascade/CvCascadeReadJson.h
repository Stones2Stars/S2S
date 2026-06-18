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

#endif // CV_CASCADE_READ_JSON_H
