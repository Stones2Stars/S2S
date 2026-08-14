//
//	UnitEnabler -- the UNITS domain on the standardized enabler component (see the header): the per-city
//	trainable vector's seed + O(delta) event appliers. The domain arrays are the ONLY mutable state; every
//	HAVE-event applies its source's unit edges DIRECTLY through the ONE kernel applier.
//

#include "CvGameCoreDLL.h"
#include "Infos/CvClassificationIds.h"   // the generated SKILL_/TAG_/CAPABILITY_ id table
#include "Enabler/CvUnitEnabler.h"
#include "Enabler/CvEnabler.h"            // EnablerDomain/CityEnabler -- the standardized per-city domain (CvCity::m_enabler)
#include "Enabler/CvEnablerKernel.h"      // EnablerKernel::applyEdges / obsoletedByHeldTech / scanCondDeps / wireOperatingBuildings
#include "Enabler/CvBuildingEnabler.h"    // BuildingEnabler::augmentWaived -- the SHARED AugmentState waiver (one evaluator)
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx/Flags + cascadeEvalCondition + cascadeGateOk
#include "Tally/CvTally.h"       // cascadeTally -- the empire live-count leg of the instance cap
#include "Spine/CvEventSpine.h"         // spineGameLoadInProgress -- load gates once at GAME_LOAD_FINISHED, never mid-read
#include "CvInfo.h"
#include "CvUnitInfo.h"           // spawnOnly / m_superseding -- the static exclusion + the replacedBy poco read
#include "CvBuildingInfo.h"
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect (the tech HAVE axis)
#include "CvCivicInfo.h"
#include "CvReligionInfo.h"
#include "CvBonusInfo.h"
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"        // GET_PLAYER
#include "AI/CvTeamAI.h"          // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvGame.h"        // getUnitCreatedCount / GAMEOPTION_NO_NATIONAL_UNIT_LIMIT (the cap legs)
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include <map>


// the source's cascade info per axis (the tech axis redirects the TECH_GAME_START root to cascadeStartNode).
// Units' HAVE axes per the store's enables.units inversion sources: techs, buildings, bonuses, religions, civics.
static const CvInfo* ud_techInfo(int iTech)
{
	if (iTech == GC.getInfoTypeForString("TECH_GAME_START", true)) return &cascadeStartNode();
	return InfoRepo<CvTechInfo>::get().get(iTech);
}

// forward decls -- the requires-gate helpers (the gate section below); the appliers call them post-apply
static void ud_gateSet(const CvCity& kCity, const std::set<int>& ids);
static void ud_touched(const CvInfo* j, std::set<int>& touched);

// `identity.enabledCivilizations` is a WHITELIST -- empty means every civilization, non-empty means ONLY those.
static bool ud_barredByCivilization(const CvUnitInfo* ju, const CvPlayer& kPlayer)
{
	if (ju == NULL)
	{
		return false;
	}
	const std::vector<int>& aiCivs = ju->getEnabledCivilizations();
	if (aiCivs.empty())
	{
		return false;
	}
	const int iCivilization = (int)kPlayer.getCivilizationType();
	for (std::vector<int>::const_iterator it = aiCivs.begin(); it != aiCivs.end(); ++it)
	{
		if (*it == iCivilization)
		{
			return false;
		}
	}
	return true;
}

// The CITY-CREATED applier (founding init + the load read's start, BEFORE the city's own in-read emits): init
// the domain (size + the spawnOnly static exclusions) and fold ONLY the cross-scope HAVE that predates the
// city -- team techs + player civics. The city's OWN facts (buildings/religions/bonuses) arrive as DOMAIN
// events -- at load from the in-read reseed emits, at founding from the real grant/build emits -- one
// mechanism (DEC-spine-reseed).
void UnitEnabler::onCityCreated(const CvCity& kCity)
{
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnablerDomain& d = kCity.m_enabler.units;
	d.init(GC.getNumUnitInfos());
	for (int u = 0; u < GC.getNumUnitInfos(); ++u)
	{
		const CvUnitInfo* ju = (const CvUnitInfo*)InfoRepo<CvUnitInfo>::get().get(u);
		// never trainable (placed by systems), or barred to this civilization -- `identity.enabledCivilizations`
		// is a WHITELIST (empty = every civ, non-empty = ONLY those), static for the city's life, so it sits on
		// the static-exclusion plane like the other membership bar (enabler.md par.8).
		if (ju != NULL && (ju->isSpawnOnly() || ud_barredByCivilization(ju, kPlayer))) d.setStaticExcluded(u, true);
	}
	for (int t = 0; t < GC.getNumTechInfos(); ++t)   // the root IS a held tech (the load backfill guarantees it)
		if (kTeam.isHasTech((TechTypes)t)) EnablerKernel::applyEdges(d, ud_techInfo(t), EDGEB_UNITS, +1);
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c != NO_CIVIC) EnablerKernel::applyEdges(d, InfoRepo<CvCivicInfo>::get().get((int)c), EDGEB_UNITS, +1);
	}
	// gate the fold's entrants (no events carried them). Inside the load bracket the GAME_LOAD_FINISHED pass
	// gates instead (a mid-read evaluation is the hazard, see the gate header below).
	if (!spineGameLoadInProgress()) gateCity(kCity);
}

// The tech delta. Broad emit -> the PLAYER tech domain's held flag is the flip guard; the invalidation route
// runs this BEFORE TechEnabler::onTechChanged flips it (the ordering contract, shared with the buildings domain).
void UnitEnabler::onCityTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = ud_techInfo((int)eTech);
	const std::vector<int>* obsB = jt ? jt->edge(EDGEF_OBSOLETES, EDGEB_BUILDINGS) : NULL;
	const CvTeam& kTeam = GET_TEAM(eTeam);
	const bool bGate = !spineGameLoadInProgress();   // load gates once at GAME_LOAD_FINISHED
	std::set<int> touched;
	if (bGate) ud_touched(jt, touched);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;
		const int iDelta = bHas ? +1 : -1;
		foreach_(CvCity* pCity, kPlayer.cities())
		{
			EnablerDomain& d = pCity->m_enabler.units;
			if (!d.isSeeded()) continue;
			EnablerKernel::applyEdges(d, jt, EDGEB_UNITS, iDelta);
			// the obsolete-present ripple: a PRESENT building this tech obsoletes stops/resumes enabling its units
			if (obsB != NULL)
				for (size_t i = 0; i < obsB->size(); ++i)
				{
					const int b = (*obsB)[i];
					if (!pCity->hasBuilding((BuildingTypes)b)) continue;
					const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(b);
					if (!EnablerKernel::obsoletedByOtherHeldTech(jb, kTeam, eTech)) EnablerKernel::applyEdges(d, jb, EDGEB_UNITS, -iDelta);
				}
			if (bGate) ud_gateSet(*pCity, touched);   // gate-on-entry + the par.7.1 step-2 re-gates
		}
	}
}

void UnitEnabler::onCityBuildingChanged(const CvCity& kCity, int iBuilding, bool bPresent)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || iBuilding < 0) return;
	// the flip guard is the BUILDINGS domain's held flag (pre-flip: this runs BEFORE BuildingEnabler's handler)
	if (kCity.m_enabler.buildings.isSeeded() && kCity.m_enabler.buildings.isHeld(iBuilding) == bPresent) return;
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	const CvInfo* jb = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (!EnablerKernel::obsoletedByHeldTech(jb, GET_TEAM(kPlayer.getTeam())))
		EnablerKernel::applyEdges(d, jb, EDGEB_UNITS, bPresent ? +1 : -1);
	if (!spineGameLoadInProgress())
	{
		// gate the building's touched units + the units requiring a BONUS this building supplies in-vicinity
		// (provides.bonuses -> the operating-buildings supply the evaluator reads; the bonus's REQUIRED_BY
		// unit bucket names its dependents)
		std::set<int> touched;
		ud_touched(jb, touched);
		const CvProvides* prov = (jb != NULL) ? jb->getProvides() : NULL;
		if (prov != NULL)
			for (size_t i = 0; i < prov->bonuses.size(); ++i)
			{
				const CvInfo* jBonus = InfoRepo<CvBonusInfo>::get().get(prov->bonuses[i]);
				const std::vector<int>* dep = (jBonus != NULL) ? jBonus->edge(EDGEF_REQUIRED_BY, EDGEB_UNITS) : NULL;
				if (dep != NULL) touched.insert(dep->begin(), dep->end());
			}
		ud_gateSet(kCity, touched);
	}
}

void UnitEnabler::onCityReligionChanged(const CvCity& kCity, int iReligion, bool bHas)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded()) return;   // flip-guarded emit (setHasReligion)
	const CvInfo* jr = InfoRepo<CvReligionInfo>::get().get(iReligion);
	EnablerKernel::applyEdges(d, jr, EDGEB_UNITS, bHas ? +1 : -1);
	if (!spineGameLoadInProgress())
	{
		std::set<int> touched;
		ud_touched(jr, touched);
		ud_gateSet(kCity, touched);
	}
}

void UnitEnabler::onCityBonusChanged(const CvCity& kCity, int iBonus, int iChange)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || iBonus < 0 || iChange == 0) return;
	// GATE-ONLY (owner ruling 2026-07-15): a plot-group-carried bonus never drives tree membership -- the
	// crossing re-gates the bonus's requires-dependents; the buildings-domain twin documents the model.
	const int iNew = kCity.getNumBonuses((BonusTypes)iBonus);
	const int iOld = iNew - iChange;
	if ((iOld > 0) == (iNew > 0)) return;   // HAVE = count > 0: re-gate only on a presence crossing
	if (spineGameLoadInProgress()) return;  // load: the one GAME_LOAD_FINISHED gate pass covers it
	std::set<int> touched;
	ud_touched(InfoRepo<CvBonusInfo>::get().get(iBonus), touched);
	ud_gateSet(kCity, touched);
}

// The LOCAL-presence twin: a vicinity flip is a pure gate re-check (the buildings-domain twin documents it).
void UnitEnabler::onCityVicinityBonusChanged(const CvCity& kCity, int iBonus)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || iBonus < 0) return;
	if (spineGameLoadInProgress()) return;  // load: the one GAME_LOAD_FINISHED gate pass covers it
	std::set<int> touched;
	ud_touched(InfoRepo<CvBonusInfo>::get().get(iBonus), touched);
	ud_gateSet(kCity, touched);
}

void UnitEnabler::onPlayerCivicsChanged(PlayerTypes ePlayer, int iOldCivic, int iNewCivic)
{
	if (ePlayer == NO_PLAYER || iOldCivic == iNewCivic) return;
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	const bool bGate = !spineGameLoadInProgress();
	std::set<int> touched;
	if (bGate)
	{
		ud_touched(iOldCivic >= 0 ? InfoRepo<CvCivicInfo>::get().get(iOldCivic) : NULL, touched);
		ud_touched(iNewCivic >= 0 ? InfoRepo<CvCivicInfo>::get().get(iNewCivic) : NULL, touched);
	}
	foreach_(CvCity* pCity, kPlayer.cities())
	{
		EnablerDomain& d = pCity->m_enabler.units;
		if (!d.isSeeded()) continue;
		if (iOldCivic >= 0) EnablerKernel::applyEdges(d, InfoRepo<CvCivicInfo>::get().get(iOldCivic), EDGEB_UNITS, -1);
		if (iNewCivic >= 0) EnablerKernel::applyEdges(d, InfoRepo<CvCivicInfo>::get().get(iNewCivic), EDGEB_UNITS, +1);
		if (bGate) ud_gateSet(*pCity, touched);
	}
}

// ==================== THE REQUIRES GATE (enabler.md par.7.1 steps 2+3; the par.3 unit machine) ====================
// The parity-proven canTrain gate legs, restored onto the domain component (the pre-rewrite uc_* verdicts,
// verified to full canTrain parity): the INSTANCE CAPS (world = lifetime-created + in-production `making`;
// empire = live tally count + making vs the ERA-SCALED base-5 national cap, waived under
// GAMEOPTION_NO_NATIONAL_UNIT_LIMIT unless unlimitedException -- units have no team cap), the ENTITY-LEVEL
// enabled/disabled gate (DEC-entity-gate), requires.build (STRICT, the ONE evaluator, the SHARED AugmentState
// waiver + the standing operating-buildings wiring -- vicinity `provides` supply included), the SUPERSEDER
// removal (replacedBy.units: HIDDEN the moment any superseder is AVAILABLE -- read from the poco's
// m_superseding, NOT j->edge("replacedBy") which returns NULL, the inert-read trap; mirrors
// the engine's superseder rule), and the UPGRADE-TREE dormancy (requires.build.dormant.all -- uc_reachable, the
// spec'd cycle-guarded closure, enabler.md par.3: a unit hides only when EVERY direct upgrade resolves to a
// reachable-trainable unit; one dead branch keeps it buildable. Do NOT replace with a one-level scheme).
// LOAD follows the par.7.1 order rule's "gate once after the stream ends" option (GAME_LOAD_FINISHED gates
// every city; a mid-read evaluation would ensure the operating-buildings cache against half-read state).

// Unit instance cap (StoneBase UnitEnabler.Capped).
static bool ud_capped(const CvInfo* j, int eU, const CvPlayer& kPlayer, bool noNationalLimit)
{
	if (j == NULL) return false;
	const int making = kPlayer.getUnitMaking((UnitTypes)eU);
	const int wcap = j->allowedCap(ALLOWEDCAP_WORLD);
	if (wcap >= 0 && GC.getGame().getUnitCreatedCount((UnitTypes)eU) + making >= wcap) return true;
	const int ecap = j->allowedCap(ALLOWEDCAP_EMPIRE);
	if (ecap >= 0 && !(noNationalLimit && !GC.getUnitInfo((UnitTypes)eU).hasSkill(CLS_SKILL_UNLIMITED_EXCEPTION)))
	{
		const int era = (int)kPlayer.getCurrentEra();
		const int cap = (ecap == 5 && era > 0) ? ecap + era * 5 : ecap;   // era-scaled base-5 national cap
		if (CvCascadeTally::unitCount((int)kPlayer.getID(), eU, CASCADE_COUNT_EMPIRE) + making >= cap) return true;
	}
	return false;
}

// The per-gate-pass context: the ONE per-city setup (waived / ec / operating buildings / flags) computed once,
// plus the memo caches so a gate set shares availability/reachability lookups (file scope: VC7.1 forbids local
// types as arguments to the helpers below).
struct UdGateCtx
{
	const CvPlayer* player;
	const CvTeam* team;
	CvCascadeEvalCtx* ec;
	const CvCascadeEvalFlags* flags;
	bool noNationalLimit;
	std::map<int, bool> availCache;   // u -> ud_isAvailable(u)
	std::map<int, bool> reachCache;   // v -> ud_reachable(v)
	std::set<int> inProgress;         // reachable() cycle guard (always fully unwound between roots)
};

// The ONE availability predicate: spawnOnly / tech-obsolete / instance-cap / entity gate / requires.build STRICT.
static bool ud_isAvailable(int u, UdGateCtx& x)
{
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
	if (j != NULL && ((const CvUnitInfo*)j)->isSpawnOnly()) return false;
	if (EnablerKernel::obsoletedByHeldTech(j, *x.team)) return false;
	if (ud_capped(j, u, *x.player, x.noNationalLimit)) return false;
	if (j != NULL && !cascadeGateOk(j->getGate(), *x.ec, *x.flags)) return false;   // entity-level enabled/disabled
	// through the ONE gate surface (DEC-single-implementation): requiresMet sets buildingAtomsPresence -- gate
	// atoms read the §7 presence has-list, never the operate-derived ACTIVE set (a direct evaluator call here
	// made unit gates fail on present-but-dormant prereq buildings, un-superseding whole upgrade chains).
	if (!EnablerKernel::requiresMet(j, *x.ec)) return false;
	return true;
}

static bool ud_availMemo(int u, UdGateCtx& x)
{
	std::map<int, bool>::const_iterator it = x.availCache.find(u);
	if (it != x.availCache.end()) return it->second;
	const bool r = ud_isAvailable(u, x);
	x.availCache[u] = r;
	return r;
}

// reachable(v) (StoneBase UnitEnabler.Reachable) -- the ONE upgrade-reachability closure, driven by the
// memoized availability predicate: v is itself available OR some DIRECT upgrade of v (its dormant triggers =
// requires.build.dormant.all) is reachable. Cycle-guarded (a cycle -> self-available terminal).
static bool ud_reachable(int v, UdGateCtx& x)
{
	std::map<int, bool>::const_iterator c = x.reachCache.find(v);
	if (c != x.reachCache.end()) return c->second;
	if (!x.inProgress.insert(v).second) return ud_availMemo(v, x);   // cycle -> self-available terminal
	bool r = ud_availMemo(v, x);
	if (!r)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(v);
		if (j != NULL)
		{
			const std::vector<int>& dorm = j->dormantTriggers();
			for (size_t i = 0; i < dorm.size() && !r; ++i)
				r = ud_reachable(dorm[i], x);
		}
	}
	x.inProgress.erase(v);
	x.reachCache[v] = r;
	return r;
}

// The ONE per-unit gate verdict: available ∧ NOT upgrade-dormant ∧ NOT superseded-by-an-available-replacer.
//	WHY this unit is not offered ([enabler.md] par.6: the gate carries the reason, so a greyed unit says what is
//	missing instead of leaving the player and the AI to guess). It mirrors ud_isAvailable's clause ORDER exactly,
//	because that is the order the verdict is actually decided in.
//	⚑ Only the TOP candidate needs a reason -- the reachability closure below it stays a bool, since "some
//	upgrade of this is reachable" has no single clause to name.
static unsigned char ud_gateReason(int u, UdGateCtx& x)
{
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
	if (j != NULL)
	{
		//	Superseded by progress: the tech that obsoletes it is held, so nothing brings it back.
		if (EnablerKernel::obsoletedByHeldTech(j, *x.team))
		{
			return (unsigned char)EnablerDomain::GATEREASON_REPLACED;
		}
		if (ud_capped(j, u, *x.player, x.noNationalLimit))
		{
			return (unsigned char)EnablerDomain::GATEREASON_CAP_SELF;
		}
		if (!cascadeGateOk(j->getGate(), *x.ec, *x.flags))
		{
			return (unsigned char)EnablerDomain::GATEREASON_OPTION;
		}
		//	The ATOM KIND that refused, not a bundled verdict ([enabler.md] par.6) -- GATEREASON_NONE when it holds.
		const unsigned char eRequires = EnablerKernel::requiresGateReason(j, *x.ec);
		if (eRequires != (unsigned char)EnablerDomain::GATEREASON_NONE)
		{
			return eRequires;
		}

		//	DORMANT only when EVERY direct upgrade resolves to a reachable-trainable unit -- one dead branch keeps
		//	this unit buildable (the fail-safe default, [enabler.md] par.3).
		const std::vector<int>& dorm = j->dormantTriggers();
		if (!dorm.empty())
		{
			bool dormant = true;
			for (size_t i = 0; i < dorm.size() && dormant; ++i)
				if (!ud_reachable(dorm[i], x)) dormant = false;
			if (dormant) return (unsigned char)EnablerDomain::GATEREASON_DORMANT;
		}
		// the superseder removal: the poco's m_superseding (the curated replacedBy.units), never j->edge
		const CvUnitInfo* ju = (const CvUnitInfo*)j;
		foreach_(const int sup, ju->getReplacedByUnits())
		{
			if (sup >= 0 && ud_availMemo(sup, x)) return (unsigned char)EnablerDomain::GATEREASON_REPLACED;
		}
	}
	//	The memo is the authority on availability (it is what the closure above consults), so a unit it refuses
	//	for a reason not named above is still refused -- reported as the greyable default rather than silently
	//	passing the gate.
	if (!ud_availMemo(u, x))
	{
		return (unsigned char)EnablerDomain::GATEREASON_REQUIRES;
	}
	return (unsigned char)EnablerDomain::GATEREASON_NONE;
}

// One city's gate context, set up once per gate pass.
static void ud_setupCtx(const CvCity& kCity, const CvPlayer& kPlayer, const CvTeam& kTeam,
	std::set<int>& waived, CvCascadeEvalCtx& ec, CvCascadeEvalFlags& flags, UdGateCtx& x)
{
	BuildingEnabler::augmentWaived(kPlayer, kTeam, waived);
	kCity.getCityContext().fillEvalCtx(ec);       // city+plot -- the contexts fill the eval state (contexts.md)
	kPlayer.getEmpireContext().fillEvalCtx(ec);   // player+team (kTeam IS the player's team at every caller)
	ec.waivedPrereqBuildings = &waived;
	EnablerKernel::wireOperatingBuildings(&kCity, ec);
	flags.strictStateReligionForBuild = true;
	x.player = &kPlayer; x.team = &kTeam; x.ec = &ec; x.flags = &flags;
	x.noNationalLimit = GC.getGame().isOption(GAMEOPTION_NO_NATIONAL_UNIT_LIMIT);
}

static void ud_gateSet(const CvCity& kCity, const std::set<int>& ids)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || ids.empty()) return;
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	std::set<int> waived; CvCascadeEvalCtx ec; CvCascadeEvalFlags flags; UdGateCtx x;
	ud_setupCtx(kCity, kPlayer, GET_TEAM(kPlayer.getTeam()), waived, ec, flags, x);
	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
		if (d.inTree(*it)) d.setGateReason(*it, ud_gateReason(*it, x));
}

// The touched candidate set of one HAVE-event source (O(delta) off its own info): its enables/removal unit
// targets + its EDGEF_REQUIRED_BY unit dependents (the readJson requires-reverse-index).
static void ud_touched(const CvInfo* j, std::set<int>& touched)
{
	if (j == NULL) return;
	static const EnEdgeFamily FAMS[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int f = 0; FAMS[f] != NUM_EDGEF; ++f)
	{
		const std::vector<int>* p = j->edge(FAMS[f], EDGEB_UNITS);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}
}

// The CLASS candidate lists + the unit-relation maps (load-compiled once, game-thread statics): the no-FK
// event classes, the DYNAMIC per-turn set, referencedUnit -> dependents (a unit's requires naming another
// unit's count), and upgrade -> predecessors (a flip of an upgrade's availability re-checks the units it
// dorms). EnablerKernel::scanCondDeps is the ONE dependency-signature scanner.
static std::set<int> s_udClass[UnitEnabler::NUM_GATE_CLASSES];
static std::map<int, std::vector<int> > s_udUnitDeps;      // referencedUnitId -> {units whose requires references it}
static std::map<int, std::vector<int> > s_udUpgradePred;   // upgradeUnitId  -> {units that name it as a dormant-trigger}
// The unit twin of the building enabler's plot-atom index: (atom kind, atom id) -> the units whose requires
// names it. Same reason it exists there -- the plot substrate carries no EDGEF_REQUIRED_BY.
static std::map<std::pair<int, int>, std::vector<int> > s_udPlotAtomConsumers;
static bool s_udClassBuilt = false;
static void ud_recordPlotAtoms(int eKind, const std::set<int>& ids, int u)
{
	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		s_udPlotAtomConsumers[std::make_pair(eKind, *it)].push_back(u);
	}
}
static void ud_buildClasses()
{
	if (s_udClassBuilt) return;
	s_udClassBuilt = true;
	for (int u = 0; u < GC.getNumUnitInfos(); ++u)
	{
		const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (j == NULL) continue;
		CascadeCondDeps deps;
		EnablerKernel::scanCondDeps(j->requiresBuild(), deps, /*bTrackUnits*/ true, /*bMarkDynamic*/ true);
		if (deps.pop)           s_udClass[UnitEnabler::GATE_POP].insert(u);
		if (deps.power)         s_udClass[UnitEnabler::GATE_POWER].insert(u);
		if (deps.goldenAge)     s_udClass[UnitEnabler::GATE_GOLDEN_AGE].insert(u);
		if (deps.stateReligion) s_udClass[UnitEnabler::GATE_STATE_RELIGION].insert(u);
		if (deps.dynamic)       s_udClass[UnitEnabler::GATE_DYNAMIC].insert(u);
		ud_recordPlotAtoms(PLOTATOM_TERRAIN,     deps.terrains,       u);
		ud_recordPlotAtoms(PLOTATOM_FEATURE,     deps.features,       u);
		ud_recordPlotAtoms(PLOTATOM_IMPROVEMENT, deps.improvements,   u);
		ud_recordPlotAtoms(PLOTATOM_ROUTE,       deps.routes,         u);
		ud_recordPlotAtoms(PLOTATOM_MAPCATEGORY, deps.mapCategories,  u);
		ud_recordPlotAtoms(PLOTATOM_PREDICATE,   deps.plotPredicates, u);
		for (std::set<int>::const_iterator it = deps.units.begin(); it != deps.units.end(); ++it) s_udUnitDeps[*it].push_back(u);
		const std::vector<int>& dorm = j->dormantTriggers();
		for (size_t i = 0; i < dorm.size(); ++i) s_udUpgradePred[dorm[i]].push_back(u);
	}
}

void UnitEnabler::gateCity(const CvCity& kCity)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded()) return;
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	std::set<int> waived; CvCascadeEvalCtx ec; CvCascadeEvalFlags flags; UdGateCtx x;
	ud_setupCtx(kCity, kPlayer, GET_TEAM(kPlayer.getTeam()), waived, ec, flags, x);
	for (int u = 0; u < GC.getNumUnitInfos(); ++u)
		if (d.inTree(u)) d.setGateReason(u, ud_gateReason(u, x));
}

// The decomposition: each gate leg evaluated independently against the SAME per-city gate context the
// real gate uses -- the endpoint's attribution surface (never a read path).
void UnitEnabler::explain(const CvCity& kCity, int iUnit, Explain& out)
{
	const EnablerDomain& d = kCity.m_enabler.units;
	out.bInTree = d.isSeeded() && d.inTree(iUnit);
	out.bListed = d.isSeeded() && d.listed(iUnit);
	const CvPlayer& kPlayer = GET_PLAYER(kCity.getOwner());
	std::set<int> waived; CvCascadeEvalCtx ec; CvCascadeEvalFlags flags; UdGateCtx x;
	ud_setupCtx(kCity, kPlayer, GET_TEAM(kPlayer.getTeam()), waived, ec, flags, x);
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iUnit);
	if (j == NULL) return;
	out.bSpawnOnly = ((const CvUnitInfo*)j)->isSpawnOnly();
	out.bObsoleteTech = EnablerKernel::obsoletedByHeldTech(j, *x.team);
	out.bCapped = ud_capped(j, iUnit, kPlayer, x.noNationalLimit);
	out.bEntityGateFail = !cascadeGateOk(j->getGate(), ec, flags);
	out.bRequiresFail = !EnablerKernel::requiresMet(j, ec);
	const std::vector<int>& dorm = j->dormantTriggers();
	if (!dorm.empty())
	{
		bool dormant = true;
		for (size_t i = 0; i < dorm.size() && dormant; ++i)
			if (!ud_reachable(dorm[i], x)) dormant = false;
		out.bUpgradeDormant = dormant;
	}
	const CvUnitInfo* ju = (const CvUnitInfo*)j;
	foreach_(const int sup, ju->getReplacedByUnits())
	{
		if (sup >= 0 && ud_availMemo(sup, x)) { out.bSuperseded = true; out.iSupersededBy = sup; break; }
	}
}

void UnitEnabler::gateAllCities()
{
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		foreach_(const CvCity* pCity, kPlayer.cities())
			gateCity(*pCity);
	}
}

void UnitEnabler::onLoadFinished()
{
	gateAllCities();
}

void UnitEnabler::onCityGateClass(const CvCity& kCity, int eClass)
{
	if (spineGameLoadInProgress()) return;   // the load-end pass gates
	ud_buildClasses();
	ud_gateSet(kCity, s_udClass[eClass]);
}

void UnitEnabler::onPlayerGateClass(PlayerTypes ePlayer, int eClass)
{
	if (spineGameLoadInProgress() || ePlayer == NO_PLAYER) return;
	ud_buildClasses();
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	foreach_(const CvCity* pCity, kPlayer.cities())
		ud_gateSet(*pCity, s_udClass[eClass]);
}

// The unit twin of BuildingEnabler::plotAtomCensus.
void UnitEnabler::plotAtomCensus(int& iKeysOut, int& iEntriesOut)
{
	ud_buildClasses();
	iKeysOut = (int)s_udPlotAtomConsumers.size();
	iEntriesOut = 0;
	for (std::map<std::pair<int, int>, std::vector<int> >::const_iterator it = s_udPlotAtomConsumers.begin();
	     it != s_udPlotAtomConsumers.end(); ++it)
	{
		iEntriesOut += (int)it->second.size();
	}
}

// The unit twin of BuildingEnabler::gateClassCensus -- same instrument, the other registry.
void UnitEnabler::gateClassCensus(int (&aiCountsOut)[NUM_GATE_CLASSES], int& iTotalOut)
{
	ud_buildClasses();
	for (int iClass = 0; iClass < NUM_GATE_CLASSES; ++iClass)
	{
		aiCountsOut[iClass] = (int)s_udClass[iClass].size();
	}
	iTotalOut = GC.getNumUnitInfos();
}

// The unit twin of BuildingEnabler::onPlotAtomChanged: a plot atom moved on a tile this city can work, so
// exactly the units whose `requires` names it re-gate (par.7.1 step 2). Units carry `build` only, but that
// build gate reads the same plot atoms a building's does.
void UnitEnabler::onPlotAtomChanged(const CvCity& kCity, int eKind, int iId)
{
	EnablerDomain& d = kCity.m_enabler.units;
	if (!d.isSeeded() || iId < 0) return;
	if (spineGameLoadInProgress()) return;  // load: the one GAME_LOAD_FINISHED gate pass covers it
	ud_buildClasses();
	std::vector<std::pair<int, int> > atoms;
	EnablerKernel::plotAtomSeeds(eKind, iId, atoms);   // a TERRAIN fact also seeds its map categories
	std::set<int> touched;
	for (size_t iAtom = 0; iAtom < atoms.size(); ++iAtom)
	{
		std::map<std::pair<int, int>, std::vector<int> >::const_iterator it = s_udPlotAtomConsumers.find(atoms[iAtom]);
		if (it != s_udPlotAtomConsumers.end()) touched.insert(it->second.begin(), it->second.end());
	}
	ud_gateSet(kCity, touched);
}

// The CAP/RELATION crossing (par.7.1 step 3): a unit's empire count changed (trained / lost / queued --
// SEVT_UNIT_COUNT). An uncapped, unreferenced, non-upgrade unit flips nothing -- skip (the combat common
// case, the pre-rewrite guard). Otherwise re-gate the changed unit + its requires-dependents + the units it
// dorms, across the OWNER's cities (empire caps); a WORLD-capped unit re-gates on every seeded city.
void UnitEnabler::onUnitCountChanged(PlayerTypes ePlayer, int eUnit)
{
	if (spineGameLoadInProgress() || ePlayer == NO_PLAYER || eUnit < 0) return;
	ud_buildClasses();
	const CvInfo* jU = InfoRepo<CvUnitInfo>::get().get(eUnit);
	const bool bCapped = (jU != NULL && jU->getAllowed() != NULL && !jU->getAllowed()->isEmpty());
	const bool bReferenced = (s_udUnitDeps.find(eUnit) != s_udUnitDeps.end());
	const bool bUpgrade = (s_udUpgradePred.find(eUnit) != s_udUpgradePred.end());
	if (!bCapped && !bReferenced && !bUpgrade) return;
	std::set<int> affected;
	affected.insert(eUnit);
	{
		std::map<int, std::vector<int> >::const_iterator it = s_udUnitDeps.find(eUnit);
		if (it != s_udUnitDeps.end()) affected.insert(it->second.begin(), it->second.end());
		it = s_udUpgradePred.find(eUnit);
		if (it != s_udUpgradePred.end()) affected.insert(it->second.begin(), it->second.end());
	}
	const bool bWorldCap = (jU != NULL && jU->allowedCap(ALLOWEDCAP_WORLD) >= 0);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		if (!bWorldCap && (PlayerTypes)iP != ePlayer) continue;   // empire caps reach the owner only
		CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
		foreach_(const CvCity* pCity, kP.cities())
			ud_gateSet(*pCity, affected);
	}
}
