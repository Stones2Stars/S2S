//
//	CvCascadeMovement -- the DLL-side movement RESOLVER (#430). See CvCascadeMovement.h.
//	Parses the plot-substrate move costs (terrain/feature/route) from Assets/Data via picojson, and resolves a
//	per-(unit, edge) move cost by reproducing CvPlot::movementCost with the plot-substrate sourced from that JSON.
//	The movement shadow (in CvHttpServer) diffs this against the FRESH legacy decomposition. ⛔ TEMPORARY harness.
//
//	cm-prefixed file-local helpers -- FastBuild unity batches concatenate .cpp, so generic anon-namespace names
//	could collide with a sibling in the blob.
//

#include "CvGameCoreDLL.h"   // PCH: engine globals + picojson + windows.h
#include "CvCascadeMovement.h"
#include "CvCascadeModifier.h"           // ModifierCareLevel (CARE_FINE..CARE_MELTDOWN) -- the shared care scale
#include "Defines/CvGlobals.h"           // GC
#include "Infrastructure/CvInitCore.h"   // getDLLPath
#include "AI/CvTeamAI.h"                 // GET_TEAM -- isHasTech (team route/domain reconstruction)
#include "AI/CvPlayerAI.h"              // GET_PLAYER -- hasTrait (national range reconstruction)
#include "Engine/CvUnit.h"
#include "Engine/CvPlot.h"
#include "CvInfos.h"                     // GC.get{Terrain,Feature,Route,Unit,Promotion,Tech}Info().getType() (umbrella
                                         // -- matches the sibling readJson harness; the CvInfos.h retirement is separate)
#include "CvUnitCombatInfo.h"            // GC.getUnitCombatInfo().getType() (not pulled by the CvInfos.h umbrella)
#include "CvTraitInfo.h"                 // GC.getTraitInfo().getType() (the national-range source)
#include <fstream>
#include <sstream>
#include <cctype>

namespace
{
	std::string cmToLower(const std::string& s)
	{
		std::string r(s);
		for (size_t i = 0; i < r.size(); ++i) r[i] = (char)std::tolower((unsigned char)r[i]);
		return r;
	}

	bool cmSlurp(const std::string& sPath, std::string& sOut)
	{
		std::ifstream f(sPath.c_str(), std::ios::in | std::ios::binary);
		if (!f.good()) return false;
		std::ostringstream ss;
		ss << f.rdbuf();
		sOut = ss.str();
		return !sOut.empty();
	}

	// Read <DLLPath>\Data\<folder>\<lowercase typekey>.json. Tries the FLAT layout first (terrain/feature/route are
	// flat, verified 2026-06-20), then each immediate sub-directory (units/promotions can be foldered by era/source/
	// category -- mirrors the readJson harness's rjLocateEntityJson). Absence is a MISS, never silently a default.
	bool cmReadEntityJson(const char* szTypeKey, const char* szFolder, std::string& sOut)
	{
		const std::string sBase = std::string(GC.getInitCore().getDLLPath().c_str()) + "\\Data\\" + szFolder;
		const std::string sFile = cmToLower(szTypeKey) + ".json";
		if (cmSlurp(sBase + "\\" + sFile, sOut)) return true; // flat layout

		WIN32_FIND_DATAA fd;
		const std::string sGlob = sBase + "\\*";
		HANDLE h = FindFirstFileA(sGlob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE) return false;
		bool bFound = false;
		do
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
				&& strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
			{
				if (cmSlurp(sBase + "\\" + fd.cFileName + "\\" + sFile, sOut)) { bFound = true; break; }
			}
		} while (FindNextFileA(h, &fd) != 0);
		FindClose(h);
		return bFound;
	}

	// Pull an int field out of the entity's `identity` object. Returns false (leaves out untouched) when the entity
	// JSON is missing/unparseable or the identity field is absent -- the caller treats that as a substrate MISS.
	bool cmIdentityInt(const std::string& sContent, const char* szField, int& iOut)
	{
		picojson::value root;
		if (!picojson::parse(root, sContent).empty() || !root.is<picojson::object>()) return false;
		const picojson::object& o = root.get<picojson::object>();
		picojson::object::const_iterator itId = o.find("identity");
		if (itId == o.end() || !itId->second.is<picojson::object>()) return false;
		const picojson::object& oId = itId->second.get<picojson::object>();
		picojson::object::const_iterator itF = oId.find(szField);
		if (itF == oId.end() || !itF->second.is<double>()) return false;
		iOut = (int)itF->second.get<double>();
		return true;
	}

	CvMovementSubstrate g_cmSubstrate; // parse-once cache (see header note)

	void cmBuildSubstrate(CvMovementSubstrate& k)
	{
		k.aiTerrainMoveCost.assign(GC.getNumTerrainInfos(), -1);
		k.aiFeatureMoveCost.assign(GC.getNumFeatureInfos(), -1);
		k.aiRouteMoveCost.assign(GC.getNumRouteInfos(), -1);
		k.aiRouteFlatCost.assign(GC.getNumRouteInfos(), -1);

		for (int i = 0; i < GC.getNumTerrainInfos(); ++i)
		{
			std::string sJson; int iCost = 0;
			if (cmReadEntityJson(GC.getTerrainInfo((TerrainTypes)i).getType(), "terrains", sJson)
				&& cmIdentityInt(sJson, "movementCost", iCost))
			{ k.aiTerrainMoveCost[i] = iCost; ++k.iTerrainParsed; }
			else ++k.iMissing;
		}
		for (int i = 0; i < GC.getNumFeatureInfos(); ++i)
		{
			std::string sJson; int iCost = 0;
			if (cmReadEntityJson(GC.getFeatureInfo((FeatureTypes)i).getType(), "features", sJson)
				&& cmIdentityInt(sJson, "movementCost", iCost))
			{ k.aiFeatureMoveCost[i] = iCost; ++k.iFeatureParsed; }
			else ++k.iMissing;
		}
		for (int i = 0; i < GC.getNumRouteInfos(); ++i)
		{
			std::string sJson; int iCost = 0, iFlat = 0;
			if (cmReadEntityJson(GC.getRouteInfo((RouteTypes)i).getType(), "routes", sJson))
			{
				if (cmIdentityInt(sJson, "movementCost", iCost))     { k.aiRouteMoveCost[i] = iCost; ++k.iRouteParsed; }
				else ++k.iMissing;
				if (cmIdentityInt(sJson, "flatMovementCost", iFlat)) { k.aiRouteFlatCost[i] = iFlat; }
			}
			else ++k.iMissing;
		}
		k.bLoaded = true;
	}

	// Source a plot-substrate term from the cascade, falling back to the legacy getter when the cascade lacks the
	// datum (and flagging the miss so the shadow can separate "JSON diverges" from "JSON absent"). iLegacy is the
	// legacy value passed in so the fallback can't drift from the engine's own getter.
	int cmTerrain(const CvMovementSubstrate& k, TerrainTypes e, int iLegacy, bool& bMiss)
	{
		if (e == NO_TERRAIN) return 0;
		const int v = ((int)e < (int)k.aiTerrainMoveCost.size()) ? k.aiTerrainMoveCost[e] : -1;
		if (v < 0) { bMiss = true; return iLegacy; }
		return v;
	}
	int cmFeature(const CvMovementSubstrate& k, FeatureTypes e, int iLegacy, bool& bMiss)
	{
		if (e == NO_FEATURE) return 0;
		const int v = ((int)e < (int)k.aiFeatureMoveCost.size()) ? k.aiFeatureMoveCost[e] : -1;
		if (v < 0) { bMiss = true; return iLegacy; }
		return v;
	}
	int cmRoute(const CvMovementSubstrate& k, RouteTypes e, int iLegacy, bool& bMiss)
	{
		const int v = ((int)e < (int)k.aiRouteMoveCost.size()) ? k.aiRouteMoveCost[e] : -1;
		if (v < 0) { bMiss = true; return iLegacy; }
		return v;
	}
	int cmRouteFlat(const CvMovementSubstrate& k, RouteTypes e, int iLegacy, bool& bMiss)
	{
		const int v = ((int)e < (int)k.aiRouteFlatCost.size()) ? k.aiRouteFlatCost[e] : -1;
		if (v < 0) { bMiss = true; return iLegacy; }
		return v;
	}
} // namespace

const CvMovementSubstrate& cascadeMovementSubstrate()
{
	if (!g_cmSubstrate.bLoaded) cmBuildSubstrate(g_cmSubstrate);
	return g_cmSubstrate;
}

// A faithful re-decomposition of CvPlot::movementCost (Sources/Engine/CvPlot.cpp::movementCost, verified
// 2026-06-20 -- the same branch tree as CvHttpServer::decomposeMoveCost) with ONLY the plot-substrate terms
// (terrain / feature / route move-cost + route flat-cost) sourced from the cascade JSON. Every other term -- the
// early returns, the unit -cost discount, the double-move predicates, the hills/river/peak globals, the floor of
// 90 -- reads the engine/GC exactly as legacy, so a cascade-vs-legacy delta isolates to the migrated substrate.
void cascadeResolveMoveCost(const CvPlot* pTo, const CvUnit* pUnit, const CvPlot* pFrom, CvCascadeMoveCost& r)
{
	const CvMovementSubstrate& k = cascadeMovementSubstrate();
	const int iDenom = GC.getMOVE_DENOMINATOR();
	r = CvCascadeMoveCost();
	r.iDiscount = pUnit->getExtraMoveDiscount();

	// 1) flat-cost / air units: the ONLY branch that returns without the trailing max(1).
	if (pUnit->flatMovementCost() || pUnit->getDomainType() == DOMAIN_AIR)
	{
		r.bEarlyFlat = true; r.iFinal = iDenom; return;
	}
	// 2) human stepping into unrevealed, or invalid-domain-for-location: maxMoves(). 3) invalid-for-action: denom.
	if (pUnit->isHuman() && !pTo->isRevealed(pUnit->getTeam(), false)) { r.bEarlyFlat = true; r.iFinal = std::max(1, pUnit->maxMoves()); return; }
	if (!pFrom->isValidDomainForLocation(*pUnit))                      { r.bEarlyFlat = true; r.iFinal = std::max(1, pUnit->maxMoves()); return; }
	if (!pTo->isValidDomainForAction(*pUnit))                          { r.bEarlyFlat = true; r.iFinal = std::max(1, iDenom); return; }

	const bool bRiverCross = pFrom->isRiverCrossing(directionXY(pFrom, pTo));

	// 4) ROUTE OVERRIDE branch: cost = min(denom, min(routeCost, routeFlatCost)).
	if (pFrom->isValidRoute(pUnit) && pTo->isValidRoute(pUnit)
		&& (!bRiverCross || GET_TEAM(pUnit->getTeam()).isBridgeBuilding()))
	{
		r.bRouteBranch = true;
		const RouteTypes eFrom = pFrom->getRouteType();
		const RouteTypes eTo = pTo->getRouteType();
		const CvRouteInfo& kFrom = GC.getRouteInfo(eFrom);
		const CvRouteInfo& kTo = GC.getRouteInfo(eTo);
		// route move cost: cascade base + the per-tech route delta. The delta is now CASCADE-reconstructed
		// (cascadeTeamRouteChange = Σ the team's researched techs' movement.team.routes deposits == the engine's
		// CvTeam::getRouteChange) -- so the route cost is FULLY cascade-sourced and the edge shadow diffs the whole
		// thing vs the legacy decomposition. max of from/to when they differ.
		int iRoute = cmRoute(k, eFrom, kFrom.getMovementCost(), r.bSubstrateMiss)
			+ cascadeTeamRouteChange(pUnit->getTeam(), eFrom);
		if (eTo != eFrom)
		{
			const int iToCost = cmRoute(k, eTo, kTo.getMovementCost(), r.bSubstrateMiss)
				+ cascadeTeamRouteChange(pUnit->getTeam(), eTo);
			if (iToCost > iRoute) iRoute = iToCost;
		}
		r.iRouteCost = iRoute;
		r.iRouteFlatCost = std::max(cmRouteFlat(k, eFrom, kFrom.getFlatMovementCost(), r.bSubstrateMiss),
			cmRouteFlat(k, eTo, kTo.getFlatMovementCost(), r.bSubstrateMiss)) * pUnit->baseMoves();
		r.iFinal = std::max(1, std::min(iDenom, std::min(r.iRouteCost, r.iRouteFlatCost)));
		return;
	}

	// 5) REGULAR (terrain) branch: additive stack, -discount, *denom, double-move /2|/4, floor 90.
	int iRegular;
	bool bIgnore = pUnit->ignoreTerrainCost();
	if (bIgnore)
	{
		iRegular = 1;
	}
	else
	{
		r.iTerrain = cmTerrain(k, pTo->getTerrainType(),
			GC.getTerrainInfo(pTo->getTerrainType()).getMovementCost(), r.bSubstrateMiss);
		iRegular = r.iTerrain;
		if (pTo->getFeatureType() != NO_FEATURE)
		{
			r.iFeature = cmFeature(k, pTo->getFeatureType(),
				GC.getFeatureInfo(pTo->getFeatureType()).getMovementCost(), r.bSubstrateMiss);
			iRegular += r.iFeature;
		}
		if (pTo->isHills()) { r.iHills = GC.getHILLS_EXTRA_MOVEMENT(); iRegular += r.iHills; }
		if (bRiverCross)    { r.iRiver = GC.getRIVER_EXTRA_MOVEMENT(); iRegular += r.iRiver; }
		if (pTo->isAsPeak())
		{
			if (!GET_TEAM(pUnit->getTeam()).isMoveFastPeaks()) { r.iPeak = GC.getPEAK_EXTRA_MOVEMENT(); iRegular += r.iPeak; }
			r.iPeak += 3; iRegular += 3; // the literal "+3" the engine adds for peaks unconditionally
		}
	}
	if (iRegular > 0) iRegular = std::max(1, iRegular - r.iDiscount);
	if (iRegular <= 1) bIgnore = true; // discount drove it to the flat case
	r.bIgnoreTerrain = bIgnore;
	r.iRegularPreDenom = iRegular;
	iRegular *= iDenom;
	const bool bFeatDouble = ((pTo->getFeatureType() != NO_FEATURE && pUnit->isFeatureDoubleMove(pTo->getFeatureType()))
		|| (pTo->isHills() && pUnit->isHillsDoubleMove()));
	const bool bTerrDouble = pUnit->isTerrainDoubleMove(pTo->getTerrainType());
	if (!bIgnore && bFeatDouble)                      { iRegular /= 4; r.iDoubleDiv = 4; }
	else if (bTerrDouble || (bIgnore && bFeatDouble)) { iRegular /= 2; r.iDoubleDiv = 2; }
	r.iFinal = std::max(1, std::max(90, iRegular));
}

const char* cascadeMoveClassify(int iDelta, const CvCascadeMoveCost& cc, int& iCareOut)
{
	// Cut 1: only the plot-substrate differs from legacy, so a real delta (beyond int-rounding) is a migrated
	// terrain/feature/route moveCost that doesn't match legacy -- a wiring/data BUG to fix (or, once adjudicated,
	// an accepted correction). Provisional rung only; the owner sets the final care (shadow.md §4).
	if (iDelta == 0)            { iCareOut = CARE_FINE;     return "match"; }
	if (cc.bSubstrateMiss)      { iCareOut = CARE_WEIRD;    return "substrateMiss"; } // fell back to legacy for a term
	if (iDelta == 1 || iDelta == -1) { iCareOut = CARE_ROUNDING; return "rounding"; }
	if (cc.bRouteBranch)        { iCareOut = CARE_BUG;      return "routeSubstrate"; } // route moveCost/flat JSON
	iCareOut = CARE_BUG;        return "terrainSubstrate"; // terrain/feature moveCost JSON
}

// ===================== the UNIT-PLANE channel: parse + aggregate =====================
namespace
{
	// Read o[a][b]([c])["flat"] -- a 3-level (c==NULL) or 4-level path to a `flat` int. False if any hop is absent /
	// wrong-typed (the field is simply not present -> the caller leaves the term at 0).
	bool cmPathFlat(const picojson::object& o, const char* a, const char* b, const char* c, int& out)
	{
		picojson::object::const_iterator i = o.find(a);
		if (i == o.end() || !i->second.is<picojson::object>()) return false;
		const picojson::object& oa = i->second.get<picojson::object>();
		picojson::object::const_iterator j = oa.find(b);
		if (j == oa.end() || !j->second.is<picojson::object>()) return false;
		const picojson::object* leaf = &j->second.get<picojson::object>();
		if (c != 0)
		{
			picojson::object::const_iterator kk = leaf->find(c);
			if (kk == leaf->end() || !kk->second.is<picojson::object>()) return false;
			leaf = &kk->second.get<picojson::object>();
		}
		picojson::object::const_iterator f = leaf->find("flat");
		if (f == leaf->end() || !f->second.is<double>()) return false;
		out = (int)f->second.get<double>();
		return true;
	}

	// identity.base.moves (a bare double, not a `.flat` leaf).
	bool cmBaseMoves(const picojson::object& o, int& out)
	{
		picojson::object::const_iterator i = o.find("identity");
		if (i == o.end() || !i->second.is<picojson::object>()) return false;
		const picojson::object& id = i->second.get<picojson::object>();
		picojson::object::const_iterator b = id.find("base");
		if (b == id.end() || !b->second.is<picojson::object>()) return false;
		const picojson::object& base = b->second.get<picojson::object>();
		picojson::object::const_iterator m = base.find("moves");
		if (m == base.end() || !m->second.is<double>()) return false;
		out = (int)m->second.get<double>();
		return true;
	}

	bool cmCapBool(const picojson::object& caps, const char* key)
	{
		picojson::object::const_iterator i = caps.find(key);
		return (i != caps.end() && i->second.is<bool>() && i->second.get<bool>());
	}

	// capabilities.<key> as a keyed object {TYPE: true, ...} -> push the resolved engine Type indices.
	void cmCapKeys(const picojson::object& caps, const char* key, std::vector<int>& out)
	{
		picojson::object::const_iterator i = caps.find(key);
		if (i == caps.end() || !i->second.is<picojson::object>()) return;
		const picojson::object& m = i->second.get<picojson::object>();
		for (picojson::object::const_iterator it = m.begin(); it != m.end(); ++it)
		{
			if (it->second.is<bool>() && !it->second.get<bool>()) continue;
			const int ip = GC.getInfoTypeForString(it->first.c_str(), true);
			if (ip >= 0) out.push_back(ip);
		}
	}

	void cmParseSource(const std::string& sContent, bool bUnitType, CvMoveSourceProfile& p)
	{
		picojson::value root;
		if (!picojson::parse(root, sContent).empty() || !root.is<picojson::object>()) return;
		const picojson::object& o = root.get<picojson::object>();
		int v = 0;
		if (bUnitType && cmBaseMoves(o, v))                       { p.iMoves += v; }
		if (cmPathFlat(o, "movement", "unit", "moves", v))        { p.iMoves += v; }
		if (cmPathFlat(o, "movement", "unit", "moveDiscount", v)) { p.iMoveDiscount += v; }
		if (cmPathFlat(o, "range", "unit", 0, v))                 { p.iRange += v; } // unit base air range
		if (cmPathFlat(o, "air", "unit", "range", v))             { p.iRange += v; } // promo/combat air-range delta
		picojson::object::const_iterator ic = o.find("capabilities");
		if (ic != o.end() && ic->second.is<picojson::object>())
		{
			const picojson::object& caps = ic->second.get<picojson::object>();
			p.bIgnoreTerrain   = cmCapBool(caps, "ignoreTerrainCost");
			p.bFlatMoveCost    = cmCapBool(caps, "flatMovementCost");
			p.bHillsDoubleMove = cmCapBool(caps, "hillsDoubleMove");
			cmCapKeys(caps, "terrainDoubleMove", p.aiTerrainDM);
			cmCapKeys(caps, "featureDoubleMove", p.aiFeatureDM);
		}
		p.bAny = (p.iMoves != 0 || p.iMoveDiscount != 0 || p.iRange != 0 || p.bIgnoreTerrain || p.bFlatMoveCost
			|| p.bHillsDoubleMove || !p.aiTerrainDM.empty() || !p.aiFeatureDM.empty());
	}

	CvMovementUnitData g_cmUnitData; // parse-once cache (see header note)

	void cmBuildUnitData(CvMovementUnitData& k)
	{
		k.aUnit.assign(GC.getNumUnitInfos(), CvMoveSourceProfile());
		k.aPromo.assign(GC.getNumPromotionInfos(), CvMoveSourceProfile());
		k.aCombat.assign(GC.getNumUnitCombatInfos(), CvMoveSourceProfile());
		for (int i = 0; i < GC.getNumUnitInfos(); ++i)
		{
			std::string s;
			if (cmReadEntityJson(GC.getUnitInfo((UnitTypes)i).getType(), "units", s))
			{ cmParseSource(s, true, k.aUnit[i]); if (k.aUnit[i].bAny) ++k.iUnitParsed; }
		}
		for (int i = 0; i < GC.getNumPromotionInfos(); ++i)
		{
			std::string s;
			if (cmReadEntityJson(GC.getPromotionInfo((PromotionTypes)i).getType(), "promotions", s))
			{ cmParseSource(s, false, k.aPromo[i]); if (k.aPromo[i].bAny) { ++k.iPromoParsed; k.aMovePromoIdx.push_back(i); } }
		}
		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			std::string s;
			if (cmReadEntityJson(GC.getUnitCombatInfo((UnitCombatTypes)i).getType(), "unitcombats", s))
			{ cmParseSource(s, false, k.aCombat[i]); if (k.aCombat[i].bAny) { ++k.iCombatParsed; k.aMoveCombatIdx.push_back(i); } }
		}

		// TECH -> team route changes: movement.team.routes.{ROUTE}.flat (engine: CvTeam::getRouteChange, processTech).
		// (Domain extra moves -- tech.getDomainExtraMoves -> CvTeam::getExtraMoves -- are NOT emitted by curate_tech
		// today, so aTechDomain stays empty; the shadow surfaces any engine-nonzero domain moves as a divergence.)
		for (int i = 0; i < GC.getNumTechInfos(); ++i)
		{
			std::string s;
			if (!cmReadEntityJson(GC.getTechInfo((TechTypes)i).getType(), "techs", s)) continue;
			picojson::value root;
			if (!picojson::parse(root, s).empty() || !root.is<picojson::object>()) continue;
			const picojson::object& o = root.get<picojson::object>();
			picojson::object::const_iterator im = o.find("movement");
			if (im == o.end() || !im->second.is<picojson::object>()) continue;
			picojson::object::const_iterator it = im->second.get<picojson::object>().find("team");
			if (it == im->second.get<picojson::object>().end() || !it->second.is<picojson::object>()) continue;
			const picojson::object& team = it->second.get<picojson::object>();
			picojson::object::const_iterator ir = team.find("routes");
			if (ir != team.end() && ir->second.is<picojson::object>())
			{
				const picojson::object& routes = ir->second.get<picojson::object>();
				for (picojson::object::const_iterator r = routes.begin(); r != routes.end(); ++r)
				{
					const int iRoute = GC.getInfoTypeForString(r->first.c_str(), true);
					int iFlat = 0;
					if (iRoute >= 0 && r->second.is<picojson::object>())
					{
						picojson::object::const_iterator f = r->second.get<picojson::object>().find("flat");
						if (f != r->second.get<picojson::object>().end() && f->second.is<double>())
						{
							iFlat = (int)f->second.get<double>();
							CvTeamMoveDeposit d; d.iSource = i; d.iKey = iRoute; d.iFlat = iFlat;
							k.aTechRoute.push_back(d); ++k.iTechRouteParsed;
						}
					}
				}
			}
		}

		// TRAIT -> national missile/flight range: combat.empire.{missileRange|flightRange}.flat (engine:
		// CvPlayer::getNational{Missile|Flight}OperationRangeChange, processTrait). iKey: 1 = missile, 0 = flight.
		for (int i = 0; i < GC.getNumTraitInfos(); ++i)
		{
			std::string s;
			if (!cmReadEntityJson(GC.getTraitInfo((TraitTypes)i).getType(), "traits", s)) continue;
			picojson::value root;
			if (!picojson::parse(root, s).empty() || !root.is<picojson::object>()) continue;
			const picojson::object& o = root.get<picojson::object>();
			picojson::object::const_iterator ic = o.find("combat");
			if (ic == o.end() || !ic->second.is<picojson::object>()) continue;
			picojson::object::const_iterator ie = ic->second.get<picojson::object>().find("empire");
			if (ie == ic->second.get<picojson::object>().end() || !ie->second.is<picojson::object>()) continue;
			const picojson::object& emp = ie->second.get<picojson::object>();
			int v = 0;
			picojson::object::const_iterator mr = emp.find("missileRange");
			if (mr != emp.end() && mr->second.is<picojson::object>())
			{
				picojson::object::const_iterator f = mr->second.get<picojson::object>().find("flat");
				if (f != mr->second.get<picojson::object>().end() && f->second.is<double>())
				{ v = (int)f->second.get<double>(); CvTeamMoveDeposit d; d.iSource = i; d.iKey = 1; d.iFlat = v; k.aTraitRange.push_back(d); ++k.iTraitRangeParsed; }
			}
			picojson::object::const_iterator fr = emp.find("flightRange");
			if (fr != emp.end() && fr->second.is<picojson::object>())
			{
				picojson::object::const_iterator f = fr->second.get<picojson::object>().find("flat");
				if (f != fr->second.get<picojson::object>().end() && f->second.is<double>())
				{ v = (int)f->second.get<double>(); CvTeamMoveDeposit d; d.iSource = i; d.iKey = 0; d.iFlat = v; k.aTraitRange.push_back(d); ++k.iTraitRangeParsed; }
			}
		}
		k.bLoaded = true;
	}

	void cmMergeKeys(std::vector<int>& dst, const std::vector<int>& src)
	{
		for (size_t i = 0; i < src.size(); ++i)
		{
			bool bHas = false;
			for (size_t j = 0; j < dst.size(); ++j) if (dst[j] == src[i]) { bHas = true; break; }
			if (!bHas) dst.push_back(src[i]);
		}
	}

	void cmFold(CvUnitMoveAgg& out, const CvMoveSourceProfile& p, int iKind, int iType, bool bTypeCaps)
	{
		out.iMovesMigrated += p.iMoves;
		out.iMoveDiscount  += p.iMoveDiscount;
		out.iRangeMigrated += p.iRange;
		out.bHillsDoubleMove = out.bHillsDoubleMove || p.bHillsDoubleMove;
		cmMergeKeys(out.aiTerrainDM, p.aiTerrainDM);
		cmMergeKeys(out.aiFeatureDM, p.aiFeatureDM);
		if (bTypeCaps) { out.bIgnoreTerrain = p.bIgnoreTerrain; out.bFlatMoveCost = p.bFlatMoveCost; } // UnitInfo-only
		if (p.bAny) { CvMoveSourceRef ref; ref.iKind = iKind; ref.iType = iType; ref.pProfile = &p; out.sources.push_back(ref); }
	}
} // namespace

const CvMovementUnitData& cascadeMovementUnitData()
{
	if (!g_cmUnitData.bLoaded) cmBuildUnitData(g_cmUnitData);
	return g_cmUnitData;
}

int cascadeTeamRouteChange(int iTeam, int iRoute)
{
	if (iTeam < 0 || iTeam >= MAX_TEAMS) return 0;
	const CvMovementUnitData& k = cascadeMovementUnitData();
	const CvTeamAI& kTeam = GET_TEAM((TeamTypes)iTeam);
	int iSum = 0;
	for (size_t n = 0; n < k.aTechRoute.size(); ++n)
		if (k.aTechRoute[n].iKey == iRoute && kTeam.isHasTech((TechTypes)k.aTechRoute[n].iSource))
			iSum += k.aTechRoute[n].iFlat;
	return iSum;
}

int cascadeTeamExtraMoves(int iTeam, int iDomain)
{
	if (iTeam < 0 || iTeam >= MAX_TEAMS) return 0;
	const CvMovementUnitData& k = cascadeMovementUnitData();
	const CvTeamAI& kTeam = GET_TEAM((TeamTypes)iTeam);
	int iSum = 0; // aTechDomain is empty today (curate_tech gap) -> 0; structure ready for the curator fix
	for (size_t n = 0; n < k.aTechDomain.size(); ++n)
		if (k.aTechDomain[n].iKey == iDomain && kTeam.isHasTech((TechTypes)k.aTechDomain[n].iSource))
			iSum += k.aTechDomain[n].iFlat;
	return iSum;
}

int cascadePlayerNationalRange(int iPlayer, bool bMissile)
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return 0;
	const CvMovementUnitData& k = cascadeMovementUnitData();
	const CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
	const int iWant = bMissile ? 1 : 0;
	int iSum = 0;
	for (size_t n = 0; n < k.aTraitRange.size(); ++n)
		if (k.aTraitRange[n].iKey == iWant && kPlayer.hasTrait((TraitTypes)k.aTraitRange[n].iSource))
			iSum += k.aTraitRange[n].iFlat;
	return iSum;
}

void cascadeUnitMoveAgg(const CvUnit* pUnit, CvUnitMoveAgg& out)
{
	out = CvUnitMoveAgg();
	const CvMovementUnitData& k = cascadeMovementUnitData();

	const UnitTypes eUT = pUnit->getUnitType();
	if (eUT != NO_UNIT && (int)eUT < (int)k.aUnit.size())
		cmFold(out, k.aUnit[eUT], 0, eUT, true); // the unit TYPE: base + the UnitInfo-only ignore/flat caps

	for (size_t n = 0; n < k.aMovePromoIdx.size(); ++n)
	{
		const int i = k.aMovePromoIdx[n];
		if (pUnit->isHasPromotion((PromotionTypes)i)) cmFold(out, k.aPromo[i], 1, i, false);
	}
	for (size_t n = 0; n < k.aMoveCombatIdx.size(); ++n)
	{
		const int i = k.aMoveCombatIdx[n];
		if (pUnit->isHasUnitCombat((UnitCombatTypes)i)) cmFold(out, k.aCombat[i], 2, i, false);
	}

	// TEAM/EMPIRE scope, mirroring CvUnit::baseMoves()/airRange() exactly so the migrated diff spans the full value:
	//  - baseMoves adds team.getExtraMoves(domain) ONLY for non-air domains (the `domain != AIR` guard).
	//  - airRange adds team.getExtraMoves(AIR) + the national missile|flight range, for DOMAIN_AIR units, by branch.
	const int iTeam = pUnit->getTeam();
	const int iOwner = pUnit->getOwner();
	const DomainTypes eDom = pUnit->getDomainType();
	if (eDom != DOMAIN_AIR)
	{
		const int iTM = cascadeTeamExtraMoves(iTeam, eDom);
		out.iMovesMigrated += iTM;
		if (iTM != 0) { CvMoveSourceRef ref; ref.iKind = 3; ref.iType = -1; ref.iContribMoves = iTM; out.sources.push_back(ref); }
	}
	else
	{
		const int iTA = cascadeTeamExtraMoves(iTeam, DOMAIN_AIR); // folds into airRange, not baseMoves
		out.iRangeMigrated += iTA;
		if (iTA != 0) { CvMoveSourceRef ref; ref.iKind = 3; ref.iType = -1; ref.iContribRange = iTA; out.sources.push_back(ref); }
		// national range: airRange uses the missile term iff specialUnit == MISSILE; the flight term iff
		// (nukeRange() == -1 && specialUnit != MISSILE). Any other DOMAIN_AIR unit gets neither (the 2-term else).
		const bool bMissile = (pUnit->getSpecialUnitType() == GC.getSPECIALUNIT_MISSILE());
		bool bWantNat = false;
		if (bMissile) bWantNat = true;
		else if (pUnit->nukeRange() == -1) bWantNat = true;
		if (bWantNat)
		{
			const int iNR = cascadePlayerNationalRange(iOwner, bMissile);
			out.iRangeMigrated += iNR;
			if (iNR != 0) { CvMoveSourceRef ref; ref.iKind = 4; ref.iType = -1; ref.iContribRange = iNR; out.sources.push_back(ref); }
		}
	}
}
