//
//	CvImprovementInfo::mapFrom -- base core reading + availability (the terrain/feature/irrigation VALIDITY
//	prereqs ride requires.build, store-inverted by the curator), then the improvement's real members from the
//	curator's family/identity/mapGeneration shapes. HUMAN-native values. FK resolution via the kept type registry.
//	Shapes nailed against curate_improvement.py (2026-07-07). See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvImprovementInfo.h"
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoImprovement.h"   // complete CvArtInfoImprovement -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)

CvImprovementInfo::CvImprovementInfo()
	: m_iDefenseModifier(0), m_iAirBombDefense(0), m_iHealthPercent(0), m_iHappiness(0), m_iCulture(0),
	  m_iPillageGold(0), m_iUniqueRange(0), m_iCultureRange(0), m_iFeatureGrowthProbability(0), m_iUpgradeTime(0),
	  m_eImprovementUpgrade(NO_IMPROVEMENT), m_eImprovementPillage(NO_IMPROVEMENT), m_eBonusChange(NO_BONUS),
	  m_bActsAsCity(false), m_bMilitaryStructure(false), m_bCarriesIrrigation(false),
	  m_bOutsideBorders(false), m_bBombardable(false), m_bZOCSource(false), m_bExtraterrestrial(false),
	  m_bUniversalBonusTrade(false), m_bGoody(false), m_bRequiresRiverSide(false),
	  m_iGoodyUniqueRange(0), m_iTilesPerGoody(0), m_iSeeFrom(0), m_iVisibilityChange(0), m_iAdvancedStartCost(0),
	  m_bUpgradeRequiresFortify(false),
	  m_bPlacesBonus(false), m_bPlacesFeature(false), m_bPlacesTerrain(false), m_bChangeRemove(false),
	  m_iWorldSoundscapeScriptId(-1)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i)
	{
		m_aiYieldChange[i] = 0;
		m_aiRiverSideYieldChange[i] = 0;
		m_aiIrrigatedYieldChange[i] = 0;
		m_aiPrereqNatureYield[i] = 0;
	}
}

void CvImprovementInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + availability (requires.build carries the placement/validity prereqs)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot yield families (the improvement's own tile output): base + condition-gated deposits, all folded into the
	// `<yield>.plot.flat` arrays by the curator. readConditionalYields sets m_aiYieldChange (base) AND the RiverSide/
	// Irrigated/Tech/Bonus buckets from one walk -- it REPLACES the old jsonFamVal reads, which returned 0 for the
	// array-shaped families (jsonFamVal reads a bare scalar only).
	readConditionalYields(entity);
	readPrereqNatureYield(entity);
	m_iDefenseModifier = jsonFamMemberVal(o, "defense", "plot", "amount", "percent");   // iDefenseModifier -> defense.plot.amount.percent
	m_iAirBombDefense  = jsonFamMemberVal(o, "defense", "plot", "air", "flat");         // iAirBombDefense -> defense.plot.air.flat
	m_iCulture         = jsonFamVal(o, "culture", "plot", "flat");                      // iCulture -> culture.plot.flat (Super Forts)
	m_iSeeFrom         = jsonFamMemberVal(o, "vision", "plot", "seeFrom", "flat");         // iSeeFrom -> vision.plot.seeFrom.flat
	m_iVisibilityChange = jsonFamMemberVal(o, "vision", "plot", "visibilityRange", "flat"); // iVisibilityChange -> vision.plot.visibilityRange.flat
	// m_iHealthPercent stays 0: iHealthPercent is a BALANCE-CUT source from improvements (curate_improvement.py
	//   EXTRA_DROP). The getter survives (live UI/CvCity callers) reading the cut-to-0 member.

	// mapGeneration
	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
	{
		m_iUniqueRange      = jsonIdInt(*mg, "uniqueRange");    // iUniqueRange -> mapGeneration.uniqueRange
		m_iGoodyUniqueRange = jsonIdInt(*mg, "goodyRange");     // iGoodyRange -> mapGeneration.goodyRange (id_rename: XML tag != member name)
		m_iTilesPerGoody    = jsonIdInt(*mg, "tilesPerGoody");  // iTilesPerGoody -> mapGeneration.tilesPerGoody
	}

	// identity: scalars, FKs, held-capability flags (the moved placement-domain flags are NOT here -- see below)
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iHappiness                = jsonIdInt(*io, "happiness");            // iHappiness -> identity (default path)
		m_iPillageGold              = jsonIdInt(*io, "pillageGold");          // iPillageGold -> pillageGold (id_rename)
		m_iCultureRange             = jsonIdInt(*io, "cultureRange");         // iCultureRange -> identity (owner: leave)
		m_iFeatureGrowthProbability = jsonIdInt(*io, "featureGrowth");        // iFeatureGrowth -> identity (owner: leave)
		m_iUpgradeTime              = jsonIdInt(*io, "upgradeTime");          // iUpgradeTime -> identity
		// upgradesTo/pillageTo are improvement->improvement self-FKs -- STASH the raw id, resolve POST-read (resolveDeferredFks):
		// SetGlobalClassInfo registers this improvement's id only AFTER read(), so a forward reference resolves to -1 here.
		jsonIdStr(*io, "upgradesTo", m_szUpgradeStr);
		jsonIdStr(*io, "pillageTo",  m_szPillageStr);
		m_eBonusChange              = (BonusTypes)jsonIdFk(*io, "bonusChange");         // BonusChange -> identity.bonusChange

		m_bActsAsCity            = jsonIdBool(*io, "actsAsCity");
		m_bMilitaryStructure     = jsonIdBool(*io, "militaryStructure");
		m_bCarriesIrrigation     = jsonIdBool(*io, "carriesIrrigation");     // KEPT in identity (propagation is live code)
		m_bOutsideBorders        = jsonIdBool(*io, "outsideBorders");
		m_bBombardable           = jsonIdBool(*io, "bombardable");
		m_bZOCSource             = jsonIdBool(*io, "zoneOfControl");         // bIsZOCSource -> zoneOfControl (id_rename)
		m_bExtraterrestrial      = jsonIdBool(*io, "extraterrestrial");      // bExtraterresial -> extraterrestrial (id_rename)
		m_bUniversalBonusTrade   = jsonIdBool(*io, "universalBonusTrade");   // bIsUniversalTradeBonusProvider (LIVE lynchpin)
		m_bUpgradeRequiresFortify = jsonIdBool(*io, "upgradeRequiresFortify");
		m_bPlacesBonus           = jsonIdBool(*io, "placesBonus");           // placement-transform outcome flags (KEPT on identity)
		m_bPlacesFeature         = jsonIdBool(*io, "placesFeature");
		m_bPlacesTerrain         = jsonIdBool(*io, "placesTerrain");
		m_bChangeRemove          = jsonIdBool(*io, "changeRemove");
		if (const picojson::object* as = jsonChildObj(*io, "advancedStart"))
			m_iAdvancedStartCost = jsonIdInt(*as, "cost");                   // iAdvancedStartCost -> identity.advancedStart.cost (to_identity)
		// NB waterImprovement / requiresIrrigation / peakImprovement / requiresFeature / requiresFlatlands / the
		//    MakesValid family are NOT cached into their OWN members here: the curator store-inverts them into
		//    requires.build (IS_WATER / HAS_IRRIGATION / HAS_PEAK / HAS_FEATURE / IS_FLATLANDS / HAS_TERRAIN /
		//    HAS_HILLS / ...), so their getters (below) walk the already-parsed m_requires.build tree on demand.

		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}

		picojson::object::const_iterator au = io->find("alternativeUpgrades");   // AlternativeImprovementUpgradeTypes (id_rename)
		if (au != io->end() && au->second.is<picojson::array>())
		{
			const picojson::array& a = au->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) m_altUpgradeStrs.push_back(a[i].get<std::string>());   // self-FKs -- resolve POST-read

		}

		picojson::object::const_iterator fc = io->find("featureChanges");   // FeatureChangeTypes (id_rename) -> FEATURE_ ids
		if (fc != io->end() && fc->second.is<picojson::array>())
		{
			const picojson::array& a = fc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aiFeatureChangeTypes.push_back(id); }
		}

		// identity.bonuses.{BONUS}: {trade, discoverRand, depletionRand} (BonusTypeStructs post_process nest).
		// BONUS_ loads before IMPROVEMENT_ (:802 < :807), so these cross-class FKs resolve at read() time.
		// `trade` -> the per-bonus tradeable set read by isImprovementBonusTrade (was dropped -- collapsed to the universal flag).
		if (const picojson::object* bonuses = jsonChildObj(*io, "bonuses"))
		{
			for (picojson::object::const_iterator b = bonuses->begin(); b != bonuses->end(); ++b)
			{
				if (!b->second.is<picojson::object>()) continue;
				const int id = jsonResolveId(b->first);
				if (id < 0) continue;
				const picojson::object& rb = b->second.get<picojson::object>();
				picojson::object::const_iterator tr = rb.find("trade");
				if (tr != rb.end() && tr->second.is<bool>() && tr->second.get<bool>()) m_bonusTradeIds.insert(id);
				picojson::object::const_iterator dr = rb.find("discoverRand");
				if (dr != rb.end() && dr->second.is<double>()) m_bonusDiscoverRand[id] = (int)dr->second.get<double>();
				picojson::object::const_iterator dp = rb.find("depletionRand");
				if (dp != rb.end() && dp->second.is<double>()) m_bonusDepletionRand[id] = (int)dp->second.get<double>();
			}
		}
	}

	// world.art.icon -- the ART_DEF_* tag (EXE map-gen art lookup, via the CvImprovementInfo shim's getArtInfo)
	if (const picojson::object* art = jsonWorldArt(o)) jsonIdStr(*art, "define", m_szArtDefineTag);

	// sound.soundscape -> runtime audio-manager index, resolved at info-load EXACTLY as the archived
	// CvImprovementInfo::read did (gDLL->getAudioTagIndex(tag, AUDIOTAG_SOUNDSCAPE)); absent/empty tag leaves the
	// legacy -1 default. (Only 2 improvements author a soundscape; the rest keep -1, matching legacy.)
	if (const picojson::object* snd = jsonChildObj(o, "sound"))
	{
		picojson::object::const_iterator ss = snd->find("soundscape");
		if (ss != snd->end() && ss->second.is<std::string>() && ss->second.get<std::string>().length() > 0)
			m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex(ss->second.get<std::string>().c_str(), AUDIOTAG_SOUNDSCAPE);
	}

	// mapGeneration.goody / .requiresRiverSide -- the EXE-bound isGoody() / isRequiresRiverSide()
	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
	{
		m_bGoody            = jsonIdBool(*mg, "goody");
		m_bRequiresRiverSide = jsonIdBool(*mg, "requiresRiverSide");
	}
}

//
//	Placement/validity condition-tree helpers -- read-only walks of `requires.build` (the CvJsonCondition tree the
//	base already parses via mutRequires()/CJK_REQUIRES; CvJsonConditionParse.cpp). curate_improvement.py's
//	requires_improvement() shapes:
//	  - a bare token (IS_WATER/HAS_PEAK/HAS_IRRIGATION/IS_FLATLANDS/HAS_FEATURE/IS_LAND/HAS_COAST/HAS_RIVER/
//	    HAS_HILLS/HAS_FRESHWATER) -> a PREDICATE node (id == -1, unparameterized).
//	  - {terrain|feature:[...]} membership sugar -> a GROUP node whose anyOf holds per-item HAS_TERRAIN/HAS_FEATURE
//	    PREDICATE nodes (id == the resolved TERRAIN_/FEATURE_ engine id).
//	  - {bonus:[...]} membership sugar -> a GROUP node whose anyOf holds per-item PRESENCE nodes (not predicates;
//	    id == the resolved BONUS_ engine id, `type` retains the original "BONUS_..." string).
//	  - the improvement's PrereqTech -> a PRESENCE node directly in `all` (`type` == "TECH_...").
//	A length-1 "MakesValid" OR-alternative (bHillsMakesValid/bFreshWaterMakesValid/bRiverSideMakesValid/
//	bPeakMakesValid -- requires_improvement's `anyset`) COLLAPSES to a bare token sitting DIRECTLY in `all` when it is
//	the improvement's only alternative -- indistinguishable BY SHAPE ALONE from the true AND-mandatory token of the
//	same name (bPeakImprovement's HAS_PEAK; bRequiresRiverSide's HAS_RIVER, already read separately into
//	m_bRequiresRiverSide from mapGeneration). Verified against the live CIV4ImprovementInfos.xml (the only two
//	collision-capable pairs in the whole file): every record with bPeakMakesValid=1 ALSO has bPeakImprovement=1
//	(mountain_mine/radio_tower/machu_picchu); the one bPeakImprovement-only record (early_mountain_mine) produces a
//	single, non-duplicated direct token. So counting DIRECT (an `all`-chain member) vs NESTED (reached via at least
//	one anyOf hop) occurrences recovers both booleans correctly for every real record: isPeakImprovement is "direct
//	count >= 1"; isPeakMakesValid is "nested, OR a SECOND direct occurrence" (the collapsed-and-duplicated case).
//	isRiverSideMakesValid uses the same count, netting out the one guaranteed direct hit m_bRequiresRiverSide
//	already contributes when true.
//

static bool condHasPredicate(const CvJsonCondition* c, CvCascPredKind k, int id)
{
	if (!c) return false;
	if (c->kind == CASC_COND_PREDICATE && c->predKind == k && c->id == id) return true;
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)   if (condHasPredicate(c->all[i], k, id))   return true;
		for (size_t i = 0; i < c->anyOf.size(); ++i) if (condHasPredicate(c->anyOf[i], k, id)) return true;
	}
	return false;
}

// The {bonus:[...]} membership atom is a PRESENCE node, not a predicate; filter on the retained `type` string
// (BONUS_ prefix) so a numerically-coincident id from another FK space (e.g. the PrereqTech PRESENCE) can't collide.
static bool condHasBonusPresence(const CvJsonCondition* c, int id)
{
	if (!c) return false;
	if (c->kind == CASC_COND_PRESENCE && c->id == id && c->type.compare(0, 6, "BONUS_") == 0) return true;
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)   if (condHasBonusPresence(c->all[i], id))   return true;
		for (size_t i = 0; i < c->anyOf.size(); ++i) if (condHasBonusPresence(c->anyOf[i], id)) return true;
	}
	return false;
}

// direct = occurrences of the bare predicate `k` reached WITHOUT ever crossing an anyOf hop; nested = occurrences
// reached through at least one anyOf hop (the "MakesValid" OR-alternative position). See the block comment above.
static void condCountPred(const CvJsonCondition* c, CvCascPredKind k, bool viaAny, int& direct, int& nested)
{
	if (!c) return;
	if (c->kind == CASC_COND_PREDICATE && c->predKind == k) { if (viaAny) ++nested; else ++direct; return; }
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)   condCountPred(c->all[i], k, viaAny, direct, nested);
		for (size_t i = 0; i < c->anyOf.size(); ++i) condCountPred(c->anyOf[i], k, true, direct, nested);
	}
}

TechTypes CvImprovementInfo::getPrereqTech() const
{
	if (m_requires.build)
		for (size_t i = 0; i < m_requires.build->all.size(); ++i)
		{
			const CvJsonCondition* c = m_requires.build->all[i];
			if (c && c->kind == CASC_COND_PRESENCE && c->type.compare(0, 5, "TECH_") == 0) return (TechTypes)c->id;
		}
	return NO_TECH;
}

bool CvImprovementInfo::isRequiresFeature() const    { return condHasPredicate(m_requires.build, CASC_PRED_HAS_FEATURE, -1); }
bool CvImprovementInfo::isRequiresFlatlands() const  { return condHasPredicate(m_requires.build, CASC_PRED_IS_FLATLANDS, -1); }
bool CvImprovementInfo::isRequiresIrrigation() const { return condHasPredicate(m_requires.build, CASC_PRED_HAS_IRRIGATION, -1); }
bool CvImprovementInfo::isWaterImprovement() const   { return condHasPredicate(m_requires.build, CASC_PRED_IS_WATER, -1); }

bool CvImprovementInfo::isCanMoveSeaUnits() const
{
	return condHasPredicate(m_requires.build, CASC_PRED_IS_LAND, -1) && condHasPredicate(m_requires.build, CASC_PRED_HAS_COAST, -1);
}

bool CvImprovementInfo::isPeakImprovement() const
{
	int d = 0, n = 0; condCountPred(m_requires.build, CASC_PRED_HAS_PEAK, false, d, n);
	return d >= 1;
}

bool CvImprovementInfo::isPeakMakesValid() const
{
	int d = 0, n = 0; condCountPred(m_requires.build, CASC_PRED_HAS_PEAK, false, d, n);
	return n > 0 || d >= 2;
}

bool CvImprovementInfo::isRiverSideMakesValid() const
{
	int d = 0, n = 0; condCountPred(m_requires.build, CASC_PRED_HAS_RIVER, false, d, n);
	return n > 0 || d > (m_bRequiresRiverSide ? 1 : 0);
}

bool CvImprovementInfo::isNoFreshWater() const
{
	if (!m_requires.build) return false;
	for (size_t i = 0; i < m_requires.build->noneOf.size(); ++i)
	{
		const CvJsonCondition* c = m_requires.build->noneOf[i];
		if (c && c->kind == CASC_COND_PREDICATE && c->predKind == CASC_PRED_HAS_FRESHWATER) return true;
	}
	return false;
}

bool CvImprovementInfo::isHillsMakesValid() const      { return condHasPredicate(m_requires.build, CASC_PRED_HAS_HILLS, -1); }
bool CvImprovementInfo::isFreshWaterMakesValid() const { return condHasPredicate(m_requires.build, CASC_PRED_HAS_FRESHWATER, -1); }
bool CvImprovementInfo::getTerrainMakesValid(int i) const { return condHasPredicate(m_requires.build, CASC_PRED_HAS_TERRAIN, i); }
bool CvImprovementInfo::getFeatureMakesValid(int i) const { return condHasPredicate(m_requires.build, CASC_PRED_HAS_FEATURE, i); }
bool CvImprovementInfo::isImprovementBonusMakesValid(int i) const { return condHasBonusPresence(m_requires.build, i); }

bool CvImprovementInfo::isAlternativeImprovementUpgradeType(int i) const
{
	for (size_t k = 0; k < m_aiAlternativeImprovementUpgradeTypes.size(); ++k)
		if (m_aiAlternativeImprovementUpgradeTypes[k] == i) return true;
	return false;
}

// POST-read self-FK resolution (cascadeLoadJson drives, after every improvement's id is registered). The strings were
// stashed at read() because SetGlobalClassInfo registers this improvement's id AFTER read()/mapFrom, so a same-class
// forward reference (a lower improvement upgrading to a higher one defined later in the XML) resolves to -1 at read()
// time. Resolve here against the COMPLETE registry. Idempotent (safe to re-run): the vector is cleared first.
void CvImprovementInfo::resolveDeferredFks()
{
	if (!m_szUpgradeStr.empty()) m_eImprovementUpgrade = (ImprovementTypes)jsonResolveId(m_szUpgradeStr);
	if (!m_szPillageStr.empty()) m_eImprovementPillage = (ImprovementTypes)jsonResolveId(m_szPillageStr);
	m_aiAlternativeImprovementUpgradeTypes.clear();
	for (size_t i = 0; i < m_altUpgradeStrs.size(); ++i)
	{
		const int id = jsonResolveId(m_altUpgradeStrs[i]);
		if (id >= 0) m_aiAlternativeImprovementUpgradeTypes.push_back(id);
	}
}

bool CvImprovementInfo::isFeatureChangeType(int i) const
{
	for (size_t k = 0; k < m_aiFeatureChangeTypes.size(); ++k)
		if (m_aiFeatureChangeTypes[k] == i) return true;
	return false;
}

// A NUM_YIELD_TYPES-sized, zero-initialised row for a keyed (tech/bonus) yield map, created on first touch.
static std::vector<int>& yieldBucket(std::map<int, std::vector<int> >& m, int key)
{
	std::vector<int>& r = m[key];
	if ((int)r.size() != NUM_YIELD_TYPES) r.assign(NUM_YIELD_TYPES, 0);
	return r;
}

// readConditionalYields -- walk food/production/commerce -> plot.flat, folding the curator's base + condition-gated
// deposits (curate_improvement.py post_process _inject) into the right member: a bare number is the base YieldChange;
// a {value,enabled} entry's gate routes to Irrigated/RiverSide/per-bonus/per-tech (see the header block comment).
void CvImprovementInfo::readConditionalYields(const picojson::value& entity)
{
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	const char* fam[3] = { "food", "production", "commerce" };
	const int   yidx[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
	for (int f = 0; f < 3; ++f)
	{
		const int iYield = yidx[f];
		const picojson::object* fo = jsonChildObj(o, fam[f]);      if (!fo) continue;
		const picojson::object* so = jsonChildObj(*fo, "plot");    if (!so) continue;
		picojson::object::const_iterator u = so->find("flat");     if (u == so->end()) continue;
		const picojson::value& flat = u->second;
		if (flat.is<double>()) { m_aiYieldChange[iYield] += (int)flat.get<double>(); continue; }   // bare scalar base
		if (!flat.is<picojson::array>()) continue;
		const picojson::array& a = flat.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			const picojson::value& e = a[i];
			if (e.is<double>()) { m_aiYieldChange[iYield] += (int)e.get<double>(); continue; }     // bare base element
			if (!e.is<picojson::object>()) continue;
			const picojson::object& eo = e.get<picojson::object>();
			picojson::object::const_iterator vv = eo.find("value");
			if (vv == eo.end() || !vv->second.is<double>()) continue;
			const int val = (int)vv->second.get<double>();
			picojson::object::const_iterator en = eo.find("enabled");
			if (en == eo.end()) { m_aiYieldChange[iYield] += val; continue; }                       // ungated -> base (defensive)
			const picojson::value& gate = en->second;
			if (gate.is<std::string>())
			{
				const std::string& s = gate.get<std::string>();
				if (s == "HAS_IRRIGATION")    m_aiIrrigatedYieldChange[iYield] += val;
				else if (s == "HAS_RIVER")    m_aiRiverSideYieldChange[iYield] += val;
				// any other bare-string gate has no legacy improvement getter -> not bucketed
			}
			else if (gate.is<picojson::object>())
			{
				const picojson::object& go = gate.get<picojson::object>();
				picojson::object::const_iterator hb = go.find("HAS_BONUS");
				if (hb != go.end() && hb->second.is<std::string>())
				{
					const int id = jsonResolveId(hb->second.get<std::string>());
					if (id >= 0) yieldBucket(m_bonusYieldChanges, id)[iYield] += val;
					continue;
				}
				picojson::object::const_iterator ty = go.find("type");
				if (ty != go.end() && ty->second.is<std::string>()
				    && ty->second.get<std::string>().compare(0, 5, "TECH_") == 0)
				{
					const int id = jsonResolveId(ty->second.get<std::string>());
					if (id >= 0) yieldBucket(m_techYieldChanges, id)[iYield] += val;
				}
			}
		}
	}
}

// readPrereqNatureYield -- the placement `{natureYield:{food:..}}` atom sits in requires.build.all; the shared
// condition parser has no natureYield case (drops it as CASC_PRED_UNKNOWN), so read the threshold straight off the
// raw JSON here (the only place the value survives).
void CvImprovementInfo::readPrereqNatureYield(const picojson::value& entity)
{
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	const picojson::object* rq = jsonChildObj(o, "requires");   if (!rq) return;
	const picojson::object* bd = jsonChildObj(*rq, "build");    if (!bd) return;
	picojson::object::const_iterator al = bd->find("all");
	if (al == bd->end() || !al->second.is<picojson::array>()) return;
	const picojson::array& a = al->second.get<picojson::array>();
	const char* fam[3] = { "food", "production", "commerce" };
	const int   yidx[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& eo = a[i].get<picojson::object>();
		picojson::object::const_iterator ny = eo.find("natureYield");
		if (ny == eo.end() || !ny->second.is<picojson::object>()) continue;
		const picojson::object& nyo = ny->second.get<picojson::object>();
		for (int f = 0; f < 3; ++f)
		{
			picojson::object::const_iterator v = nyo.find(fam[f]);
			if (v != nyo.end() && v->second.is<double>()) m_aiPrereqNatureYield[yidx[f]] = (int)v->second.get<double>();
		}
	}
}

int CvImprovementInfo::getImprovementBonusYield(int i, int j) const
{
	if (j < 0 || j >= NUM_YIELD_TYPES) return 0;
	std::map<int, std::vector<int> >::const_iterator it = m_bonusYieldChanges.find(i);
	return it != m_bonusYieldChanges.end() ? it->second[j] : 0;
}

int CvImprovementInfo::getTechYieldChanges(int i, int j) const
{
	if (j < 0 || j >= NUM_YIELD_TYPES) return 0;
	std::map<int, std::vector<int> >::const_iterator it = m_techYieldChanges.find(i);
	return it != m_techYieldChanges.end() ? it->second[j] : 0;
}

int* CvImprovementInfo::getTechYieldChangesArray(int i) const
{
	std::map<int, std::vector<int> >::const_iterator it = m_techYieldChanges.find(i);
	return it != m_techYieldChanges.end() ? const_cast<int*>(&it->second[0]) : NULL;   // legacy: NULL when the tech deposits none
}

const CvArtInfoImprovement* CvImprovementInfo::getArtInfo() const
{
	return ARTFILEMGR.getImprovementArtInfo(getArtDefineTag());
}
const char* CvImprovementInfo::getButton() const
{
	const CvArtInfoImprovement* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
