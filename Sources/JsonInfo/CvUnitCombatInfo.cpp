//
//	CvUnitCombatInfo::mapFrom -- the base dispatch fills every composed unit (the §6 modifier families /
//	the §8 `skills` bool block / the entity-level gate); this subclass ALSO parses the values the surviving legacy
//	CvUnitCombatInfo consumers read as named getters: the identity scalars (base ranks / refs / AI tags), the
//	runtime zobrist, and every §6 modifier-family value (scalar + vs-keyed struct-vector + domain array). Every
//	address below is the curate_unitcombat.py output, confirmed against Assets/Data/unitcombats/*.json. The read
//	helpers return the raw human JSON value (no x100); the curator writes the raw XML value, so the round-trip
//	reproduces the archived getter -- EXCEPT getExtraUpkeep100, whose JSON is the curator-descaled human (x100 re-
//	applied here). See the header for the deferred-with-reason groups.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson + GC (getGame().getSorenRand())
#include "CvUnitCombatInfo.h"
#include "CvJsonParse.h"            // jsonChildObj / jsonFamVal / jsonFamMemberVal / jsonIdInt / jsonIdFk / jsonIdBool / jsonResolveId / jsonX100
#include "CvJsonCondition.h"        // CvJsonCondition + CASC_COND_* -- the entity-gate tree walk for the game-option lists
#include "AI/CvGameAI.h"            // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist seed)

// Collect a vs-keyed modifier object `{TYPE:{[member:]{unitKey:N}}}` -> (resolvedId, value) pairs (0 values skipped).
static void collectKeyed(const picojson::object* keyed, const char* member, const char* unitKey,
                         std::vector<std::pair<int, int> >& out)
{
	if (keyed == NULL) return;
	for (picojson::object::const_iterator it = keyed->begin(); it != keyed->end(); ++it)
	{
		const int id = jsonResolveId(it->first);
		if (id < 0 || !it->second.is<picojson::object>()) continue;
		const picojson::object& vo = it->second.get<picojson::object>();
		const picojson::object* leaf = &vo;
		if (member != NULL) { leaf = jsonChildObj(vo, member); if (leaf == NULL) continue; }
		picojson::object::const_iterator u = leaf->find(unitKey);
		if (u != leaf->end() && u->second.is<double>())
		{
			const int v = (int)u->second.get<double>();
			if (v != 0) out.push_back(std::make_pair(id, v));
		}
	}
}

// Flatten the entity-level gate condition into the flat GAMEOPTION id list the legacy On/NotOnGameOption getters
// expose (the pattern the Promotion poco uses): a single option is a bare GAMEOPTION_ string (-> PRESENCE atom),
// several are an {all}/{anyOf} tree (-> GROUP); recurse the group vectors and collect the resolved GAMEOPTION_ ids.
static void uc_collectGameOptions(const CvJsonCondition* c, std::vector<int>& out)
{
	if (c == NULL) return;
	if (c->kind == CASC_COND_PRESENCE)
	{
		if (c->id >= 0 && c->type.compare(0, 11, "GAMEOPTION_") == 0) out.push_back(c->id);
		return;
	}
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)    uc_collectGameOptions(c->all[i], out);
		for (size_t i = 0; i < c->anyOf.size(); ++i)  uc_collectGameOptions(c->anyOf[i], out);
		for (size_t i = 0; i < c->noneOf.size(); ++i) uc_collectGameOptions(c->noneOf[i], out);
		uc_collectGameOptions(c->enabled, out);
		uc_collectGameOptions(c->disabled, out);
	}
}

// a bare-int keyed map obj.{TYPE}:int -> out[FK id] = int (the §7 vision intensity pair-lists).
static void uc_readIntMap(const picojson::object& parent, const char* key, std::map<int, int>& out)
{
	const picojson::object* io = jsonChildObj(parent, key); if (io == NULL) return;
	for (picojson::object::const_iterator it = io->begin(); it != io->end(); ++it)
		if (it->second.is<double>()) { const int id = jsonResolveId(it->first); if (id >= 0) out[id] = (int)it->second.get<double>(); }
}

// a string array parent[key] = ["TYPE",...] -> FK id vector (identity ggPointsForUnits / defaultStatuses).
static void uc_readIdList(const picojson::object& parent, const char* key, std::vector<int>& out)
{
	picojson::object::const_iterator it = parent.find(key);
	if (it == parent.end() || !it->second.is<picojson::array>()) return;
	const picojson::array& a = it->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) out.push_back(id); }
}

// a {TYPE:true} keyed-bool object parent[key] -> FK id vector (skills.terrainDoubleMove / featureDoubleMove / trapImmunity).
static void uc_readKeyedBoolIdList(const picojson::object& parent, const char* key, std::vector<int>& out)
{
	const picojson::object* io = jsonChildObj(parent, key); if (io == NULL) return;
	for (picojson::object::const_iterator it = io->begin(); it != io->end(); ++it)
		if (it->second.is<bool>() && it->second.get<bool>()) { const int id = jsonResolveId(it->first); if (id >= 0) out.push_back(id); }
}

// vision struct rows: [{invisible, terrain|feature|improvement, intensity}] -> the typed InvisibleXChanges vectors.
static void uc_readTerrainChanges(const picojson::object& vision, const char* name, std::vector<InvisibleTerrainChanges>& out)
{
	picojson::object::const_iterator ai = vision.find(name);
	if (ai == vision.end() || !ai->second.is<picojson::array>()) return;
	const picojson::array& a = ai->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		InvisibleTerrainChanges r;
		r.eInvisible = static_cast<InvisibleTypes>(jsonIdFk(e, "invisible"));
		r.eTerrain   = static_cast<TerrainTypes>(jsonIdFk(e, "terrain"));
		r.iIntensity = jsonIdInt(e, "intensity");
		out.push_back(r);
	}
}
static void uc_readFeatureChanges(const picojson::object& vision, const char* name, std::vector<InvisibleFeatureChanges>& out)
{
	picojson::object::const_iterator ai = vision.find(name);
	if (ai == vision.end() || !ai->second.is<picojson::array>()) return;
	const picojson::array& a = ai->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		InvisibleFeatureChanges r;
		r.eInvisible = static_cast<InvisibleTypes>(jsonIdFk(e, "invisible"));
		r.eFeature   = static_cast<FeatureTypes>(jsonIdFk(e, "feature"));
		r.iIntensity = jsonIdInt(e, "intensity");
		out.push_back(r);
	}
}
static void uc_readImprovementChanges(const picojson::object& vision, const char* name, std::vector<InvisibleImprovementChanges>& out)
{
	picojson::object::const_iterator ai = vision.find(name);
	if (ai == vision.end() || !ai->second.is<picojson::array>()) return;
	const picojson::array& a = ai->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		InvisibleImprovementChanges r;
		r.eInvisible   = static_cast<InvisibleTypes>(jsonIdFk(e, "invisible"));
		r.eImprovement = static_cast<ImprovementTypes>(jsonIdFk(e, "improvement"));
		r.iIntensity   = jsonIdInt(e, "intensity");
		out.push_back(r);
	}
}

CvUnitCombatInfo::CvUnitCombatInfo()
	: m_eReligion(NO_RELIGION), m_eCulture(NO_BONUS), m_eEra(NO_ERA),
	  m_iQualityBase(-10), m_iGroupBase(-10), m_iSizeBase(-10),
	  m_iRBombardDamageBase(0), m_iRBombardDamageLimitBase(0), m_iRBombardDamageMaxUnitsBase(0),
	  m_iDCMBombRangeBase(0), m_iDCMBombAccuracyBase(0),
	  m_bForMilitary(false), m_bForNavalMilitary(false), m_zobristValue(0),
	  m_iCombatPercent(0), m_iStrengthChange(0), m_iStrengthModifier(0), m_iAttackCombatModifierChange(0), m_iDefenseCombatModifierChange(0),
	  m_iVSBarbsChange(0), m_iReligiousCombatModifierChange(0), m_iStealthCombatModifierChange(0), m_iDamageModifierChange(0),
	  m_iMaxHPChange(0), m_iEnduranceChange(0), m_iTauntChange(0), m_iBreakdownChanceChange(0), m_iBreakdownDamageChange(0),
	  m_iUnnerveChange(0), m_iEncloseChange(0), m_iLungeChange(0), m_iDynamicDefenseChange(0),
	  m_iCombatModifierPerSizeMoreChange(0), m_iCombatModifierPerSizeLessChange(0), m_iCombatModifierPerVolumeMoreChange(0), m_iCombatModifierPerVolumeLessChange(0),
	  m_iCityAttackPercent(0), m_iCityDefensePercent(0), m_iHillsAttackPercent(0), m_iHillsDefensePercent(0), m_iKamikazePercent(0), m_iCombatLimitChange(0), m_iStealthStrikesChange(0),
	  m_iWithdrawalChange(0), m_iFirstStrikesChange(0), m_iChanceFirstStrikesChange(0), m_iBombardRateChange(0),
	  m_iCollateralDamageChange(0), m_iCollateralDamageLimitChange(0), m_iCollateralDamageMaxUnitsChange(0), m_iCollateralDamageProtection(0),
	  m_iAirRangeChange(0), m_iInterceptChange(0), m_iEvasionChange(0), m_iAirCombatLimitChange(0),
	  m_iEnemyHealChange(0), m_iNeutralHealChange(0), m_iFriendlyHealChange(0), m_iSameTileHealChange(0), m_iAdjacentTileHealChange(0),
	  m_iSelfHealModifier(0), m_iNumHealSupport(0), m_iVictoryHeal(0), m_iVictoryAdjacentHeal(0), m_iVictoryStackHeal(0),
	  m_iMovesChange(0), m_iMoveDiscountChange(0), m_iExtraDropRange(0), m_iExperiencePercent(0),
	  m_iWorkRatePercent(0), m_iHillsWorkPercent(0), m_iPeaksWorkPercent(0),
	  m_iCargoChange(0), m_iSMCargoChange(0), m_iSMCargoVolumeChange(0), m_iSMCargoVolumeModifierChange(0),
	  m_iUpkeepModifier(0), m_iExtraUpkeep100(0), m_iUpgradeDiscount(0),
	  m_iVisibilityChange(0), m_iCaptureProbabilityModifierChange(0), m_iCaptureResistanceModifierChange(0), m_iPoisonProbabilityModifierChange(0),
	  m_iInsidiousnessChange(0), m_iInvestigationChange(0), m_iRevoltProtection(0), m_iPillageChange(0), m_iSurvivorChance(0),
	  m_bAnyDomainModifierPercent(false)
{
	for (int i = 0; i < NUM_DOMAIN_TYPES; ++i) m_aiDomainModifierPercent[i] = 0;
	// Runtime, non-XML: a per-type random hash contribution -- drawn ONCE at construction exactly as the archived
	// CvUnitCombatInfo ctor did (never in mapFrom: the full-registry pass re-runs mapFrom, and a re-run must not
	// redraw the synced RNG).
	m_zobristValue = GC.getGame().getSorenRand().getInt();
}

void CvUnitCombatInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every appending vector.
	m_aiGGptsforUnitTypes.clear(); m_aiDefaultStatusTypes.clear();
	m_terrainAttack.clear(); m_terrainDefense.clear(); m_terrainWork.clear();
	m_featureAttack.clear(); m_featureDefense.clear(); m_featureWork.clear();
	m_buildWork.clear(); m_unitCombatMod.clear(); m_flanking.clear(); m_trapAvoidance.clear();
	m_aiOnGameOptions.clear(); m_aiNotOnGameOptions.clear();
	m_aiTerrainDoubleMove.clear(); m_aiFeatureDoubleMove.clear(); m_aiTrapImmunity.clear();
	m_aInvisibleTerrainChanges.clear(); m_aVisibleTerrainChanges.clear(); m_aVisibleTerrainRangeChanges.clear();
	m_aInvisibleFeatureChanges.clear(); m_aVisibleFeatureChanges.clear(); m_aVisibleFeatureRangeChanges.clear();
	m_aInvisibleImprovementChanges.clear(); m_aVisibleImprovementChanges.clear(); m_aVisibleImprovementRangeChanges.clear();
	m_aiInvisibilityIntensity.clear(); m_aiVisibilityIntensity.clear();
	m_aiVisibilityIntensityRange.clear(); m_aiVisibilityIntensitySameTile.clear();

	CvInfo::mapFrom(entity);   // core reading + the section dispatch into the composed units

	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// ---- identity: base ranks / refs / AI tags ----
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		// identity.base.* -- the rangedBombard/dcm create-unit stats (the SM *Base ranks moved to sizeMatters below).
		if (const picojson::object* bo = jsonChildObj(*io, "base"))
		{
			m_iRBombardDamageBase         = jsonIdInt(*bo, "rangedBombardDamage");
			m_iRBombardDamageLimitBase    = jsonIdInt(*bo, "rangedBombardLimit");
			m_iRBombardDamageMaxUnitsBase = jsonIdInt(*bo, "rangedBombardMaxUnits");
			m_iDCMBombRangeBase           = jsonIdInt(*bo, "dcmRange");
			m_iDCMBombAccuracyBase        = jsonIdInt(*bo, "dcmAccuracy");
		}
		m_eReligion = (ReligionTypes)jsonIdFk(*io, "religion");
		m_eCulture  = (BonusTypes)jsonIdFk(*io, "culture");
		m_eEra      = (EraTypes)jsonIdFk(*io, "era");
		m_bForMilitary      = jsonIdBool(*io, "forMilitary");
		m_bForNavalMilitary = jsonIdBool(*io, "forNavalMilitary");
		// parked identity FK lists (great-general points-for-unit list / auto-applied default statuses)
		uc_readIdList(*io, "ggPointsForUnits", m_aiGGptsforUnitTypes);
		uc_readIdList(*io, "defaultStatuses", m_aiDefaultStatusTypes);
	}

	// ---- §6 modifier-family SCALARS (all at the `unit` self-accumulator scope) ----
	// strength.unit.* (member-less percent/flat + the named sub-members)
	m_iCombatPercent                     = jsonFamVal(o, "strength", "unit", "percent");
	m_iStrengthChange                    = jsonFamVal(o, "strength", "unit", "flat");
	m_iStrengthModifier                  = jsonFamMemberVal(o, "strength", "unit", "sizeModifier", "percent");
	m_iAttackCombatModifierChange        = jsonFamMemberVal(o, "strength", "unit", "attack", "percent");
	m_iDefenseCombatModifierChange       = jsonFamMemberVal(o, "strength", "unit", "defense", "percent");
	m_iVSBarbsChange                     = jsonFamMemberVal(o, "strength", "unit", "vsBarbs", "percent");
	m_iReligiousCombatModifierChange     = jsonFamMemberVal(o, "strength", "unit", "religious", "percent");
	m_iStealthCombatModifierChange       = jsonFamMemberVal(o, "strength", "unit", "stealth", "percent");
	m_iDamageModifierChange              = jsonFamMemberVal(o, "strength", "unit", "damageModifier", "percent");
	m_iEnduranceChange                   = jsonFamMemberVal(o, "strength", "unit", "endurance", "flat");
	m_iTauntChange                       = jsonFamMemberVal(o, "strength", "unit", "taunt", "flat");
	m_iBreakdownChanceChange             = jsonFamMemberVal(o, "strength", "unit", "breakdownChance", "flat");
	m_iBreakdownDamageChange             = jsonFamMemberVal(o, "strength", "unit", "breakdownDamage", "flat");
	m_iUnnerveChange                     = jsonFamMemberVal(o, "strength", "unit", "unnerve", "percent");
	m_iEncloseChange                     = jsonFamMemberVal(o, "strength", "unit", "enclose", "percent");
	m_iLungeChange                       = jsonFamMemberVal(o, "strength", "unit", "lunge", "percent");
	m_iDynamicDefenseChange              = jsonFamMemberVal(o, "strength", "unit", "dynamicDefense", "percent");
	m_iCityAttackPercent                 = jsonFamMemberVal(o, "strength", "unit", "cityAttack", "percent");
	m_iCityDefensePercent                = jsonFamMemberVal(o, "strength", "unit", "cityDefense", "percent");
	m_iHillsAttackPercent                = jsonFamMemberVal(o, "strength", "unit", "hillsAttack", "percent");
	m_iHillsDefensePercent               = jsonFamMemberVal(o, "strength", "unit", "hillsDefense", "percent");
	m_iKamikazePercent                   = jsonFamMemberVal(o, "strength", "unit", "kamikaze", "percent");
	m_iCombatLimitChange                 = jsonFamMemberVal(o, "strength", "unit", "combatLimit", "flat");
	m_iStealthStrikesChange              = jsonFamMemberVal(o, "strength", "unit", "stealthStrikes", "flat");
	// withdrawal / firstStrike / bombard / collateral / air
	m_iWithdrawalChange                  = jsonFamVal(o, "withdrawal", "unit", "percent");
	m_iFirstStrikesChange                = jsonFamMemberVal(o, "firstStrike", "unit", "strikes", "flat");
	m_iChanceFirstStrikesChange          = jsonFamMemberVal(o, "firstStrike", "unit", "chance", "flat");
	m_iBombardRateChange                 = jsonFamMemberVal(o, "bombard", "unit", "rate", "percent");
	m_iCollateralDamageChange            = jsonFamMemberVal(o, "collateral", "unit", "damage", "percent");
	m_iCollateralDamageLimitChange       = jsonFamMemberVal(o, "collateral", "unit", "limit", "flat");
	m_iCollateralDamageMaxUnitsChange    = jsonFamMemberVal(o, "collateral", "unit", "maxUnits", "flat");
	m_iCollateralDamageProtection        = jsonFamMemberVal(o, "collateral", "unit", "protection", "percent");
	m_iAirRangeChange                    = jsonFamMemberVal(o, "air", "unit", "range", "flat");
	m_iInterceptChange                   = jsonFamMemberVal(o, "air", "unit", "intercept", "percent");
	m_iEvasionChange                     = jsonFamMemberVal(o, "air", "unit", "evasion", "percent");
	m_iAirCombatLimitChange              = jsonFamMemberVal(o, "air", "unit", "combatLimit", "flat");
	// heal
	m_iEnemyHealChange                   = jsonFamMemberVal(o, "heal", "unit", "enemy", "flat");
	m_iNeutralHealChange                 = jsonFamMemberVal(o, "heal", "unit", "neutral", "flat");
	m_iFriendlyHealChange                = jsonFamMemberVal(o, "heal", "unit", "friendly", "flat");
	m_iSameTileHealChange                = jsonFamMemberVal(o, "heal", "unit", "sameTile", "flat");
	m_iAdjacentTileHealChange            = jsonFamMemberVal(o, "heal", "unit", "adjacentTile", "flat");
	m_iSelfHealModifier                  = jsonFamMemberVal(o, "heal", "unit", "selfModifier", "percent");
	m_iNumHealSupport                    = jsonFamMemberVal(o, "heal", "unit", "support", "flat");
	m_iVictoryHeal                       = jsonFamMemberVal(o, "heal", "unit", "victory", "flat");
	m_iVictoryAdjacentHeal               = jsonFamMemberVal(o, "heal", "unit", "victoryAdjacent", "flat");
	m_iVictoryStackHeal                  = jsonFamMemberVal(o, "heal", "unit", "victoryStack", "flat");
	// movement / experience / workRate / cargo
	m_iMovesChange                       = jsonFamMemberVal(o, "movement", "unit", "moves", "flat");
	m_iMoveDiscountChange                = jsonFamMemberVal(o, "movement", "unit", "moveDiscount", "flat");
	m_iExtraDropRange                    = jsonFamMemberVal(o, "movement", "unit", "dropRange", "flat");
	m_iExperiencePercent                 = jsonFamVal(o, "experience", "unit", "percent");
	m_iWorkRatePercent                   = jsonFamMemberVal(o, "workRate", "unit", "rate", "percent");
	m_iHillsWorkPercent                  = jsonFamMemberVal(o, "workRate", "unit", "hills", "percent");
	m_iPeaksWorkPercent                  = jsonFamMemberVal(o, "workRate", "unit", "peaks", "percent");
	m_iCargoChange                       = jsonFamMemberVal(o, "cargo", "unit", "space", "flat");
	// --- sizeMatters block (json.md §9): base ranks (-10 sentinel kept when absent) + maxHP + per-rank combat mods + SM cargo ---
	if (const picojson::object* sm = jsonChildObj(o, "sizeMatters"))
	{
		picojson::object::const_iterator it;
		if ((it = sm->find("qualityBase")) != sm->end() && it->second.is<double>()) m_iQualityBase = (int)it->second.get<double>();
		if ((it = sm->find("groupBase"))   != sm->end() && it->second.is<double>()) m_iGroupBase   = (int)it->second.get<double>();
		if ((it = sm->find("sizeBase"))    != sm->end() && it->second.is<double>()) m_iSizeBase    = (int)it->second.get<double>();
		m_iMaxHPChange = jsonIdInt(*sm, "maxHP");
		if (const picojson::object* cm = jsonChildObj(*sm, "combatModifier"))
		{
			m_iCombatModifierPerSizeMoreChange   = jsonIdInt(*cm, "perSizeMore");
			m_iCombatModifierPerSizeLessChange   = jsonIdInt(*cm, "perSizeLess");
			m_iCombatModifierPerVolumeMoreChange = jsonIdInt(*cm, "perVolumeMore");
			m_iCombatModifierPerVolumeLessChange = jsonIdInt(*cm, "perVolumeLess");
		}
		if (const picojson::object* cg = jsonChildObj(*sm, "cargo"))
		{
			m_iSMCargoChange               = jsonIdInt(*cg, "smSpace");
			m_iSMCargoVolumeChange         = jsonIdInt(*cg, "volume");
			m_iSMCargoVolumeModifierChange = jsonIdInt(*cg, "volumeModifier");
		}
	}
	// upkeep -- getExtraUpkeep100 re-applies the x100 (JSON holds the curator-descaled human)
	m_iUpkeepModifier                    = jsonFamMemberVal(o, "upkeep", "unit", "modifier", "percent");
	m_iUpgradeDiscount                   = jsonFamMemberVal(o, "upkeep", "unit", "upgradeDiscount", "percent");
	{
		const picojson::object* uf = jsonChildObj(o, "upkeep");
		const picojson::object* uu = uf ? jsonChildObj(*uf, "unit") : NULL;
		const picojson::object* ue = uu ? jsonChildObj(*uu, "extra") : NULL;
		if (ue != NULL)
		{
			picojson::object::const_iterator it = ue->find("flat");
			if (it != ue->end() && it->second.is<double>()) m_iExtraUpkeep100 = jsonX100(it->second.get<double>());
		}
	}
	// vision(range) / capture / poison / espionage / revoltProtection / pillage / survivor
	m_iVisibilityChange                  = jsonFamVal(o, "vision", "range", "flat");
	m_iCaptureProbabilityModifierChange  = jsonFamMemberVal(o, "capture", "unit", "probability", "flat");
	m_iCaptureResistanceModifierChange   = jsonFamMemberVal(o, "capture", "unit", "resistance", "flat");
	m_iPoisonProbabilityModifierChange   = jsonFamMemberVal(o, "poison", "unit", "probability", "flat");
	m_iInsidiousnessChange               = jsonFamMemberVal(o, "espionage", "unit", "insidiousness", "flat");
	m_iInvestigationChange               = jsonFamMemberVal(o, "espionage", "unit", "investigation", "flat");
	m_iRevoltProtection                  = jsonFamVal(o, "revoltProtection", "unit", "percent");
	m_iPillageChange                     = jsonFamVal(o, "pillage", "unit", "flat");
	m_iSurvivorChance                    = jsonFamVal(o, "survivor", "unit", "percent");

	// ---- §6 vs-keyed struct-vectors ----
	const picojson::object* su = NULL;   // strength.unit
	if (const picojson::object* s = jsonChildObj(o, "strength")) su = jsonChildObj(*s, "unit");
	const picojson::object* wu = NULL;   // workRate.unit
	if (const picojson::object* w = jsonChildObj(o, "workRate")) wu = jsonChildObj(*w, "unit");
	const picojson::object* tu = NULL;   // trap.unit
	if (const picojson::object* t = jsonChildObj(o, "trap")) tu = jsonChildObj(*t, "unit");

	std::vector<std::pair<int, int> > pr;
	// terrain: attack / defense (strength) + work (workRate)
	collectKeyed(su ? jsonChildObj(*su, "terrain") : NULL, "attack", "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { TerrainModifier m; m.eTerrain = (TerrainTypes)pr[i].first; m.iModifier = pr[i].second; m_terrainAttack.push_back(m); } pr.clear();
	collectKeyed(su ? jsonChildObj(*su, "terrain") : NULL, "defense", "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { TerrainModifier m; m.eTerrain = (TerrainTypes)pr[i].first; m.iModifier = pr[i].second; m_terrainDefense.push_back(m); } pr.clear();
	collectKeyed(wu ? jsonChildObj(*wu, "terrain") : NULL, NULL, "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { TerrainModifier m; m.eTerrain = (TerrainTypes)pr[i].first; m.iModifier = pr[i].second; m_terrainWork.push_back(m); } pr.clear();
	// feature: attack / defense (strength) + work (workRate)
	collectKeyed(su ? jsonChildObj(*su, "feature") : NULL, "attack", "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { FeatureModifier m; m.eFeature = (FeatureTypes)pr[i].first; m.iModifier = pr[i].second; m_featureAttack.push_back(m); } pr.clear();
	collectKeyed(su ? jsonChildObj(*su, "feature") : NULL, "defense", "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { FeatureModifier m; m.eFeature = (FeatureTypes)pr[i].first; m.iModifier = pr[i].second; m_featureDefense.push_back(m); } pr.clear();
	collectKeyed(wu ? jsonChildObj(*wu, "feature") : NULL, NULL, "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { FeatureModifier m; m.eFeature = (FeatureTypes)pr[i].first; m.iModifier = pr[i].second; m_featureWork.push_back(m); } pr.clear();
	// build work (workRate)
	collectKeyed(wu ? jsonChildObj(*wu, "build") : NULL, NULL, "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { BuildModifier m; m.eBuild = (BuildTypes)pr[i].first; m.iModifier = pr[i].second; m_buildWork.push_back(m); } pr.clear();
	// unitCombat change / flanking (strength) + trap avoidance (trap)
	collectKeyed(su ? jsonChildObj(*su, "unitCombat") : NULL, NULL, "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { UnitCombatModifier m; m.eUnitCombat = (UnitCombatTypes)pr[i].first; m.iModifier = pr[i].second; m_unitCombatMod.push_back(m); } pr.clear();
	collectKeyed(su ? jsonChildObj(*su, "flanking") : NULL, NULL, "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i) { UnitCombatModifier m; m.eUnitCombat = (UnitCombatTypes)pr[i].first; m.iModifier = pr[i].second; m_flanking.push_back(m); } pr.clear();
	collectKeyed(tu ? jsonChildObj(*tu, "avoidance") : NULL, NULL, "flat", pr);
	for (size_t i = 0; i < pr.size(); ++i) { UnitCombatModifier m; m.eUnitCombat = (UnitCombatTypes)pr[i].first; m.iModifier = pr[i].second; m_trapAvoidance.push_back(m); } pr.clear();

	// ---- §6 domain modifier array (strength.unit.domain.{DOMAIN}.percent) ----
	collectKeyed(su ? jsonChildObj(*su, "domain") : NULL, NULL, "percent", pr);
	for (size_t i = 0; i < pr.size(); ++i)
		if (pr[i].first >= 0 && pr[i].first < NUM_DOMAIN_TYPES) { m_aiDomainModifierPercent[pr[i].first] = pr[i].second; m_bAnyDomainModifierPercent = true; }

	// ---- entity-gate GAMEOPTION int-lists: walk the composed gate condition tree (populated by the base dispatch) ----
	uc_collectGameOptions(m_gate.enabled, m_aiOnGameOptions);
	uc_collectGameOptions(m_gate.disabled, m_aiNotOnGameOptions);

	// ---- §8 keyed-skill FK lists (skills.<name>.{TYPE}:true) ----
	if (const picojson::object* sk = jsonChildObj(o, "skills"))
	{
		uc_readKeyedBoolIdList(*sk, "terrainDoubleMove", m_aiTerrainDoubleMove);
		uc_readKeyedBoolIdList(*sk, "featureDoubleMove", m_aiFeatureDoubleMove);
		uc_readKeyedBoolIdList(*sk, "trapImmunity",      m_aiTrapImmunity);
	}

	// ---- §7 vision/LOS resolver: intensity pair-maps + the invisible/visible struct-row vectors ----
	if (const picojson::object* vs = jsonChildObj(o, "vision"))
	{
		uc_readIntMap(*vs, "visibilityIntensity",         m_aiVisibilityIntensity);
		uc_readIntMap(*vs, "invisibilityIntensity",       m_aiInvisibilityIntensity);
		uc_readIntMap(*vs, "visibilityIntensityRange",    m_aiVisibilityIntensityRange);
		uc_readIntMap(*vs, "visibilityIntensitySameTile", m_aiVisibilityIntensitySameTile);
		uc_readTerrainChanges(*vs, "invisibleTerrain",         m_aInvisibleTerrainChanges);
		uc_readTerrainChanges(*vs, "visibleTerrain",           m_aVisibleTerrainChanges);
		uc_readTerrainChanges(*vs, "visibleTerrainRange",      m_aVisibleTerrainRangeChanges);
		uc_readFeatureChanges(*vs, "invisibleFeature",         m_aInvisibleFeatureChanges);
		uc_readFeatureChanges(*vs, "visibleFeature",           m_aVisibleFeatureChanges);
		uc_readFeatureChanges(*vs, "visibleFeatureRange",      m_aVisibleFeatureRangeChanges);
		uc_readImprovementChanges(*vs, "invisibleImprovement",       m_aInvisibleImprovementChanges);
		uc_readImprovementChanges(*vs, "visibleImprovement",         m_aVisibleImprovementChanges);
		uc_readImprovementChanges(*vs, "visibleImprovementRange",    m_aVisibleImprovementRangeChanges);
	}
}

// ===================== game-option-gated getters (archive mirror -- SourceArchive/Infos/CvUnitCombatInfo.cpp
// :311-:560; owner ruling: IS_GAME_OPTION covers the combat-mod fields). Value = real curated data; the OPTION
// decides whether the consuming system is on, exactly as legacy gated it at the getter. =====
int CvUnitCombatInfo::getUnnerveChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iUnnerveChange : 0; }
int CvUnitCombatInfo::getEncloseChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iEncloseChange : 0; }
int CvUnitCombatInfo::getLungeChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iLungeChange : 0; }
int CvUnitCombatInfo::getDynamicDefenseChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iDynamicDefenseChange : 0; }
int CvUnitCombatInfo::getMaxHPChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iMaxHPChange : 0; }
int CvUnitCombatInfo::getCombatModifierPerSizeMoreChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerSizeMoreChange : 0; }
int CvUnitCombatInfo::getCombatModifierPerSizeLessChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerSizeLessChange : 0; }
int CvUnitCombatInfo::getCombatModifierPerVolumeMoreChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerVolumeMoreChange : 0; }
int CvUnitCombatInfo::getCombatModifierPerVolumeLessChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerVolumeLessChange : 0; }
int CvUnitCombatInfo::getStealthStrikesChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING) ? m_iStealthStrikesChange : 0; }
int CvUnitCombatInfo::getStealthCombatModifierChange() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING) ? m_iStealthCombatModifierChange : 0; }
int CvUnitCombatInfo::getStealthDefenseChange() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING)) return 0;
	static int s_clsId = -1;
	return m_skills.hasKey(s_clsId, CLSD_SKILL, "stealthDefense") ? 1 : 0;
}
