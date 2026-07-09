//
//	CvJsonPromotionInfo::mapFrom -- the base dispatch fills the composed units (modifiers / section-8 `skills` bool block /
//	entity-level gate / `grants`); this subclass then reads its own typed values straight off the raw entity, per
//	curate_promotion.py's family/member vocabulary (see the header). The strength/family scalars, the vs-keyed
//	percent maps, the identity availability gates, the vision LOS resolver, the ai weights and heal struct rows are
//	all subclass-parsed here.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson + GC
#include "CvJsonPromotionInfo.h"
#include "CvJsonPromotionLineInfo.h"  // GC.getPromotionLineInfo(...) -> the line's unitcombat-prereq / notOnUnitCombat getters
#include "CvJsonParse.h"            // jsonResolveId + jsonFamVal/jsonFamMemberVal + jsonIdInt/Bool/Fk/Str + jsonChildObj + jsonX100
#include "AI/CvGameAI.h"            // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvJsonPromotionInfo::CvJsonPromotionInfo()
	: m_iCombatPercent(0), m_iStrengthChange(0), m_iStrengthModifier(0), m_iAttackCombatModifierChange(0), m_iDefenseCombatModifierChange(0),
	  m_iVSBarbsChange(0), m_iReligiousCombatModifierChange(0), m_iStealthCombatModifierChange(0), m_iDamageModifierChange(0), m_iMaxHPChange(0),
	  m_iEnduranceChange(0), m_iTauntChange(0), m_iBreakdownChanceChange(0), m_iBreakdownDamageChange(0),
	  m_iUnnerveChange(0), m_iEncloseChange(0), m_iLungeChange(0), m_iDynamicDefenseChange(0),
	  m_iCombatModifierPerSizeMoreChange(0), m_iCombatModifierPerSizeLessChange(0), m_iCombatModifierPerVolumeMoreChange(0), m_iCombatModifierPerVolumeLessChange(0),
	  m_iCityAttackPercent(0), m_iCityDefensePercent(0), m_iHillsAttackPercent(0), m_iHillsDefensePercent(0),
	  m_iKamikazePercent(0), m_iCombatLimitChange(0), m_iStealthStrikesChange(0), m_iQualityChange(0), m_iGroupChange(0),
	  m_iWithdrawalChange(0), m_iFirstStrikesChange(0), m_iChanceFirstStrikesChange(0),
	  m_iBombardRateChange(0), m_iRBombardDamageChange(0), m_iRBombardDamageLimitChange(0), m_iRBombardDamageMaxUnitsChange(0), m_iDCMBombRangeChange(0), m_iDCMBombAccuracyChange(0),
	  m_iCollateralDamageChange(0), m_iCollateralDamageLimitChange(0), m_iCollateralDamageMaxUnitsChange(0), m_iCollateralDamageProtection(0),
	  m_iAirRangeChange(0), m_iInterceptChange(0), m_iEvasionChange(0), m_iAirCombatLimitChange(0),
	  m_iEnemyHealChange(0), m_iNeutralHealChange(0), m_iFriendlyHealChange(0), m_iSameTileHealChange(0), m_iAdjacentTileHealChange(0),
	  m_iSelfHealModifier(0), m_iNumHealSupport(0), m_iVictoryHeal(0), m_iVictoryAdjacentHeal(0), m_iVictoryStackHeal(0),
	  m_iMovesChange(0), m_iMoveDiscountChange(0), m_iExtraDropRange(0), m_iExperiencePercent(0),
	  m_iWorkRatePercent(0), m_iHillsWorkPercent(0), m_iPeaksWorkPercent(0),
	  m_iCargoChange(0), m_iSMCargoChange(0), m_iSMCargoVolumeChange(0), m_iSMCargoVolumeModifierChange(0),
	  m_iUpkeepModifier(0), m_iExtraUpkeep100(0), m_iUpgradeDiscount(0), m_iVisibilityChange(0),
	  m_iCaptureProbabilityModifierChange(0), m_iCaptureResistanceModifierChange(0), m_iPoisonProbabilityModifierChange(0),
	  m_iInsidiousnessChange(0), m_iInvestigationChange(0), m_iRevoltProtection(0), m_iPillageChange(0), m_iSurvivorChance(0),
	  m_iLayerAnimationPath(0), m_iMinEraType(0), m_iMaxEraType(0), m_iStateReligionPrereq(-1), m_iControlPoints(0), m_iCommandRange(0), m_iLevelPrereq(0), m_iLinePriority(0),
	  m_iCommandType(NO_COMMAND), m_ePromotionLine(NO_PROMOTIONLINE),
	  m_iReplacesUnitCombat(-1), m_iDomainCargoChange(-1), m_iSpecialCargoChange(-1), m_iSpecialCargoPrereq(-1), m_iSMNotSpecialCargoChange(-1), m_iSMNotSpecialCargoPrereq(-1),
	  m_bLeader(false), m_bStatus(false), m_bQuick(false), m_bStarsign(false), m_bZeroesXP(false), m_bForOffset(false), m_bCargoPrereq(false), m_bRBombardPrereq(false),
	  m_bSetOnHNCapture(false), m_bSetOnInvestigated(false), m_bPrereqNormInvisible(false), m_bPlotPrereqsKeepAfter(false), m_bRemoveAfterSet(false),
	  m_eTechPrereq(NO_TECH), m_eObsoleteTech(NO_TECH), m_iZobristValue(0)
{
	// Non-XML runtime state-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvPromotionInfo ctor did (SourceArchive/Infos/CvPromotionInfo.cpp:52). CvUnit XORs it into m_movementCharacteristicsHash.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

// --- shared local walkers ------------------------------------------------------------------------------------

// entity[family][scope][member][unit] as a DOUBLE (for the x100 re-scale of upkeep.extra). 0 if any hop missing.
static double famMemberDbl(const picojson::object& o, const char* family, const char* scope, const char* member, const char* unit)
{
	const picojson::object* fo = jsonChildObj(o, family);   if (!fo) return 0.0;
	const picojson::object* so = jsonChildObj(*fo, scope);  if (!so) return 0.0;
	const picojson::object* mo = jsonChildObj(*so, member); if (!mo) return 0.0;
	picojson::object::const_iterator u = mo->find(unit);
	return (u != mo->end() && u->second.is<double>()) ? u->second.get<double>() : 0.0;
}

// a keyed percent map under a scope: scope[kind].{TYPE}[.member].percent -> out[FK id] = (int)percent.
static void readKeyedPercent(const picojson::object& scope, const char* kind, const char* member, std::map<int, int>& out)
{
	const picojson::object* kd = jsonChildObj(scope, kind); if (!kd) return;
	for (picojson::object::const_iterator it = kd->begin(); it != kd->end(); ++it)
	{
		if (!it->second.is<picojson::object>()) continue;
		const picojson::object* leaf = &it->second.get<picojson::object>();
		if (member) { leaf = jsonChildObj(*leaf, member); if (!leaf) continue; }
		picojson::object::const_iterator p = leaf->find("percent");
		if (p == leaf->end() || !p->second.is<double>()) continue;
		const int id = jsonResolveId(it->first);
		if (id >= 0) out[id] = (int)p->second.get<double>();
	}
}

// a bare-int keyed map: obj.{TYPE} = int -> out[FK id] = int (vision intensity pair-lists).
static void readIntMap(const picojson::object& parent, const char* key, std::map<int, int>& out)
{
	const picojson::object* o = jsonChildObj(parent, key); if (!o) return;
	for (picojson::object::const_iterator it = o->begin(); it != o->end(); ++it)
		if (it->second.is<double>()) { const int id = jsonResolveId(it->first); if (id >= 0) out[id] = (int)it->second.get<double>(); }
}

// a string array -> FK id vector (identity prereq/notOn/negates lists + skills unitcombat lists).
static void readIdList(const picojson::object& parent, const char* key, std::vector<int>& out)
{
	picojson::object::const_iterator it = parent.find(key);
	if (it == parent.end() || !it->second.is<picojson::array>()) return;
	const picojson::array& a = it->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) out.push_back(id); }
}

// a {TYPE:true} keyed-bool object -> FK id set (skills.terrainDoubleMove / featureDoubleMove).
static void readIdSet(const picojson::object& parent, const char* key, std::set<int>& out)
{
	const picojson::object* o = jsonChildObj(parent, key); if (!o) return;
	for (picojson::object::const_iterator it = o->begin(); it != o->end(); ++it)
		if (it->second.is<bool>() && it->second.get<bool>()) { const int id = jsonResolveId(it->first); if (id >= 0) out.insert(id); }
}

// identity numeric-or-FK scalar: a plain number stays as-is; a string FK-resolves; absent -> 0.
static int idNumOrFk(const picojson::object& io, const char* key)
{
	picojson::object::const_iterator it = io.find(key);
	if (it == io.end()) return 0;
	if (it->second.is<double>()) return (int)it->second.get<double>();
	if (it->second.is<std::string>()) return jsonResolveId(it->second.get<std::string>());
	return 0;
}

// vision struct rows: [{invisible, <slotKey>, intensity}] -> parallel FK ids + intensity into the caller's fields.
static void readTerrainChanges(const picojson::object& vision, const char* name, std::vector<InvisibleTerrainChanges>& out)
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
static void readFeatureChanges(const picojson::object& vision, const char* name, std::vector<InvisibleFeatureChanges>& out)
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
static void readImprovementChanges(const picojson::object& vision, const char* name, std::vector<InvisibleImprovementChanges>& out)
{
	picojson::object::const_iterator ai = vision.find(name);
	if (ai == vision.end() || !ai->second.is<picojson::array>()) return;
	const picojson::array& a = ai->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		InvisibleImprovementChanges r;
		r.eInvisible    = static_cast<InvisibleTypes>(jsonIdFk(e, "invisible"));
		r.eImprovement  = static_cast<ImprovementTypes>(jsonIdFk(e, "improvement"));
		r.iIntensity    = jsonIdInt(e, "intensity");
		out.push_back(r);
	}
}

// flatten the entity-level gate condition into the flat GAMEOPTION id list the legacy On/NotOnGameOption getters
// expose. The curator authors a single option as a bare GAMEOPTION_ string (-> PRESENCE atom) and several as an
// {all}/{anyOf} tree (-> GROUP); recurse the group vectors and collect the resolved GAMEOPTION_ presence ids.
static void collectGameOptions(const CvJsonCondition* c, std::vector<int>& out)
{
	if (!c) return;
	if (c->kind == CASC_COND_PRESENCE)
	{
		if (c->id >= 0 && c->type.compare(0, 11, "GAMEOPTION_") == 0) out.push_back(c->id);
		return;
	}
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)    collectGameOptions(c->all[i], out);
		for (size_t i = 0; i < c->anyOf.size(); ++i)  collectGameOptions(c->anyOf[i], out);
		for (size_t i = 0; i < c->noneOf.size(); ++i) collectGameOptions(c->noneOf[i], out);
		collectGameOptions(c->enabled, out);
		collectGameOptions(c->disabled, out);
	}
}

void CvJsonPromotionInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + the section dispatch into the composed units (modifiers/skills/gate/grants)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// entity-level gate (populated by the base dispatch) -> the flat OnGameOptions/NotOnGameOptions lists
	collectGameOptions(m_gate.enabled, m_aiOnGameOptions);
	collectGameOptions(m_gate.disabled, m_aiNotOnGameOptions);

	// --- strength family (general % + named members + vs-keyed maps) ---
	m_iCombatPercent                    = jsonFamVal(o, "strength", "unit", "percent");
	m_iStrengthChange                   = jsonFamVal(o, "strength", "unit", "flat");
	m_iAttackCombatModifierChange       = jsonFamMemberVal(o, "strength", "unit", "attack", "percent");
	m_iDefenseCombatModifierChange      = jsonFamMemberVal(o, "strength", "unit", "defense", "percent");
	m_iVSBarbsChange                    = jsonFamMemberVal(o, "strength", "unit", "vsBarbs", "percent");
	m_iReligiousCombatModifierChange    = jsonFamMemberVal(o, "strength", "unit", "religious", "percent");
	m_iStealthCombatModifierChange      = jsonFamMemberVal(o, "strength", "unit", "stealth", "percent");
	m_iDamageModifierChange             = jsonFamMemberVal(o, "strength", "unit", "damageModifier", "percent");
	m_iEnduranceChange                  = jsonFamMemberVal(o, "strength", "unit", "endurance", "flat");
	m_iTauntChange                      = jsonFamMemberVal(o, "strength", "unit", "taunt", "flat");
	m_iBreakdownChanceChange            = jsonFamMemberVal(o, "strength", "unit", "breakdownChance", "flat");
	m_iBreakdownDamageChange            = jsonFamMemberVal(o, "strength", "unit", "breakdownDamage", "flat");
	m_iUnnerveChange                    = jsonFamMemberVal(o, "strength", "unit", "unnerve", "percent");
	m_iEncloseChange                    = jsonFamMemberVal(o, "strength", "unit", "enclose", "percent");
	m_iLungeChange                      = jsonFamMemberVal(o, "strength", "unit", "lunge", "percent");
	m_iDynamicDefenseChange             = jsonFamMemberVal(o, "strength", "unit", "dynamicDefense", "percent");
	m_iCityAttackPercent                = jsonFamMemberVal(o, "strength", "unit", "cityAttack", "percent");
	m_iCityDefensePercent               = jsonFamMemberVal(o, "strength", "unit", "cityDefense", "percent");
	m_iHillsAttackPercent               = jsonFamMemberVal(o, "strength", "unit", "hillsAttack", "percent");
	m_iHillsDefensePercent              = jsonFamMemberVal(o, "strength", "unit", "hillsDefense", "percent");
	m_iKamikazePercent                  = jsonFamMemberVal(o, "strength", "unit", "kamikaze", "percent");
	m_iCombatLimitChange                = jsonFamMemberVal(o, "strength", "unit", "combatLimit", "flat");
	m_iStealthStrikesChange             = jsonFamMemberVal(o, "strength", "unit", "stealthStrikes", "flat");
	// --- sizeMatters block (json.md §9): the SM deltas moved off the strength/cargo families ---
	if (const picojson::object* sm = jsonChildObj(o, "sizeMatters"))
	{
		m_iStrengthModifier = jsonIdInt(*sm, "sizeModifier");
		m_iMaxHPChange      = jsonIdInt(*sm, "maxHP");
		m_iQualityChange    = jsonIdInt(*sm, "quality");
		m_iGroupChange      = jsonIdInt(*sm, "group");
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
	if (const picojson::object* st = jsonChildObj(o, "strength"))
		if (const picojson::object* un = jsonChildObj(*st, "unit"))
		{
			readKeyedPercent(*un, "terrain",    "attack",  m_aiTerrainAttackPercent);
			readKeyedPercent(*un, "terrain",    "defense", m_aiTerrainDefensePercent);
			readKeyedPercent(*un, "feature",    "attack",  m_aiFeatureAttackPercent);
			readKeyedPercent(*un, "feature",    "defense", m_aiFeatureDefensePercent);
			readKeyedPercent(*un, "unitCombat", NULL,      m_aiUnitCombatModifierPercent);
			readKeyedPercent(*un, "domain",     NULL,      m_aiDomainModifierPercent);
			readKeyedPercent(*un, "flanking",   NULL,      m_aiFlanking);
		}

	// --- other own-families ---
	m_iWithdrawalChange           = jsonFamVal(o, "withdrawal", "unit", "percent");
	m_iFirstStrikesChange         = jsonFamMemberVal(o, "firstStrike", "unit", "strikes", "flat");
	m_iChanceFirstStrikesChange   = jsonFamMemberVal(o, "firstStrike", "unit", "chance", "flat");
	m_iBombardRateChange          = jsonFamMemberVal(o, "bombard", "unit", "rate", "percent");
	m_iRBombardDamageChange       = jsonFamMemberVal(o, "bombard", "unit", "rangedDamage", "flat");
	m_iRBombardDamageLimitChange  = jsonFamMemberVal(o, "bombard", "unit", "rangedDamageLimit", "flat");
	m_iRBombardDamageMaxUnitsChange = jsonFamMemberVal(o, "bombard", "unit", "rangedDamageMaxUnits", "flat");
	m_iDCMBombRangeChange         = jsonFamMemberVal(o, "bombard", "unit", "dcmRange", "flat");
	m_iDCMBombAccuracyChange      = jsonFamMemberVal(o, "bombard", "unit", "dcmAccuracy", "flat");
	m_iCollateralDamageChange     = jsonFamMemberVal(o, "collateral", "unit", "damage", "percent");
	m_iCollateralDamageLimitChange = jsonFamMemberVal(o, "collateral", "unit", "limit", "flat");
	m_iCollateralDamageMaxUnitsChange = jsonFamMemberVal(o, "collateral", "unit", "maxUnits", "flat");
	m_iCollateralDamageProtection = jsonFamMemberVal(o, "collateral", "unit", "protection", "percent");
	m_iAirRangeChange             = jsonFamMemberVal(o, "air", "unit", "range", "flat");
	m_iInterceptChange            = jsonFamMemberVal(o, "air", "unit", "intercept", "percent");
	m_iEvasionChange              = jsonFamMemberVal(o, "air", "unit", "evasion", "percent");
	m_iAirCombatLimitChange       = jsonFamMemberVal(o, "air", "unit", "combatLimit", "flat");
	m_iEnemyHealChange            = jsonFamMemberVal(o, "heal", "unit", "enemy", "flat");
	m_iNeutralHealChange          = jsonFamMemberVal(o, "heal", "unit", "neutral", "flat");
	m_iFriendlyHealChange         = jsonFamMemberVal(o, "heal", "unit", "friendly", "flat");
	m_iSameTileHealChange         = jsonFamMemberVal(o, "heal", "unit", "sameTile", "flat");
	m_iAdjacentTileHealChange     = jsonFamMemberVal(o, "heal", "unit", "adjacentTile", "flat");
	m_iSelfHealModifier           = jsonFamMemberVal(o, "heal", "unit", "selfModifier", "percent");
	m_iNumHealSupport             = jsonFamMemberVal(o, "heal", "unit", "support", "flat");
	m_iVictoryHeal                = jsonFamMemberVal(o, "heal", "unit", "victory", "flat");
	m_iVictoryAdjacentHeal        = jsonFamMemberVal(o, "heal", "unit", "victoryAdjacent", "flat");
	m_iVictoryStackHeal           = jsonFamMemberVal(o, "heal", "unit", "victoryStack", "flat");
	m_iMovesChange                = jsonFamMemberVal(o, "movement", "unit", "moves", "flat");
	m_iMoveDiscountChange         = jsonFamMemberVal(o, "movement", "unit", "moveDiscount", "flat");
	m_iExtraDropRange             = jsonFamMemberVal(o, "movement", "unit", "dropRange", "flat");
	m_iExperiencePercent          = jsonFamVal(o, "experience", "unit", "percent");
	m_iWorkRatePercent            = jsonFamMemberVal(o, "workRate", "unit", "rate", "percent");
	m_iHillsWorkPercent           = jsonFamMemberVal(o, "workRate", "unit", "hills", "percent");
	m_iPeaksWorkPercent           = jsonFamMemberVal(o, "workRate", "unit", "peaks", "percent");
	m_iCargoChange                = jsonFamMemberVal(o, "cargo", "unit", "space", "flat");   // SM cargo -> sizeMatters (above)
	m_iUpkeepModifier             = jsonFamMemberVal(o, "upkeep", "unit", "modifier", "percent");
	m_iExtraUpkeep100             = jsonX100(famMemberDbl(o, "upkeep", "unit", "extra", "flat"));   // legacy accessor is x100; JSON is de-scaled human
	m_iUpgradeDiscount            = jsonFamMemberVal(o, "upkeep", "unit", "upgradeDiscount", "percent");
	m_iVisibilityChange           = jsonFamVal(o, "vision", "range", "flat");
	m_iCaptureProbabilityModifierChange = jsonFamMemberVal(o, "capture", "unit", "probability", "flat");
	m_iCaptureResistanceModifierChange  = jsonFamMemberVal(o, "capture", "unit", "resistance", "flat");
	m_iPoisonProbabilityModifierChange  = jsonFamMemberVal(o, "poison", "unit", "probability", "flat");
	m_iInsidiousnessChange        = jsonFamMemberVal(o, "espionage", "unit", "insidiousness", "flat");
	m_iInvestigationChange        = jsonFamMemberVal(o, "espionage", "unit", "investigation", "flat");
	m_iRevoltProtection           = jsonFamVal(o, "revoltProtection", "unit", "percent");
	m_iPillageChange              = jsonFamVal(o, "pillage", "unit", "flat");
	m_iSurvivorChance             = jsonFamVal(o, "survivor", "unit", "percent");
	if (const picojson::object* wr = jsonChildObj(o, "workRate"))
		if (const picojson::object* un = jsonChildObj(*wr, "unit"))
		{
			readKeyedPercent(*un, "terrain", NULL, m_aiTerrainWorkPercent);
			readKeyedPercent(*un, "feature", NULL, m_aiFeatureWorkPercent);
			readKeyedPercent(*un, "build",   NULL, m_aiBuildWorkRate);
		}

	// heal.unit.unitCombat.{UC} = {heal, adjacentHeal} -> HealUnitCombat rows
	if (const picojson::object* hl = jsonChildObj(o, "heal"))
		if (const picojson::object* un = jsonChildObj(*hl, "unit"))
			if (const picojson::object* uc = jsonChildObj(*un, "unitCombat"))
				for (picojson::object::const_iterator it = uc->begin(); it != uc->end(); ++it)
					if (it->second.is<picojson::object>())
					{
						const int id = jsonResolveId(it->first);
						if (id < 0) continue;
						const picojson::object& e = it->second.get<picojson::object>();
						HealUnitCombat r;
						r.eUnitCombat  = static_cast<UnitCombatTypes>(id);
						r.iHeal        = jsonIdInt(e, "heal");
						r.iAdjacentHeal = jsonIdInt(e, "adjacentHeal");
						m_aHealUnitCombat.push_back(r);
					}

	// --- skills: raw tri-state bools + double-move keyed sets + unitcombat lists ---
	if (const picojson::object* sk = jsonChildObj(o, "skills"))
	{
		for (picojson::object::const_iterator it = sk->begin(); it != sk->end(); ++it)
			if (it->second.is<bool>()) m_skillTri[it->first] = it->second.get<bool>();
		readIdSet(*sk, "terrainDoubleMove", m_aiTerrainDoubleMove);
		readIdSet(*sk, "featureDoubleMove", m_aiFeatureDoubleMove);
		readIdList(*sk, "unitCombats", m_aiSubCombat);          // SubCombatChangeTypes
		readIdList(*sk, "removesUnitCombats", m_aiRemoves);     // RemovesUnitCombatTypes
	}

	// --- identity: flags, FKs, era/command scalars, availability lists ---
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_bLeader             = jsonIdBool(*io, "leader");
		m_bStatus             = jsonIdBool(*io, "status");
		m_bQuick              = jsonIdBool(*io, "quick");
		m_bStarsign           = jsonIdBool(*io, "starsign");
		m_bZeroesXP           = jsonIdBool(*io, "zeroesXP");
		m_bForOffset          = jsonIdBool(*io, "forOffset");
		m_bCargoPrereq        = jsonIdBool(*io, "cargoPrereq");
		m_bRBombardPrereq     = jsonIdBool(*io, "rBombardPrereq");
		m_bSetOnHNCapture     = jsonIdBool(*io, "setOnHNCapture");
		m_bSetOnInvestigated  = jsonIdBool(*io, "setOnInvestigated");
		m_bPrereqNormInvisible = jsonIdBool(*io, "prereqNormInvisible");
		m_bPlotPrereqsKeepAfter = jsonIdBool(*io, "plotPrereqsKeepAfter");
		m_bRemoveAfterSet     = jsonIdBool(*io, "removeAfterSet");

		m_iStateReligionPrereq = jsonIdFk(*io, "stateReligionPrereq");
		m_iControlPoints      = jsonIdInt(*io, "controlPoints");
		m_iCommandRange       = jsonIdInt(*io, "commandRange");
		m_iLevelPrereq        = jsonIdInt(*io, "levelPrereq");
		{ picojson::object::const_iterator ct = io->find("commandType");   // default NO_COMMAND unless authored (runtime setter may override)
		  if (ct != io->end() && ct->second.is<double>()) m_iCommandType = (int)ct->second.get<double>(); }
		m_iLayerAnimationPath = idNumOrFk(*io, "layerAnimationPath");
		m_iMinEraType         = idNumOrFk(*io, "minEra");
		m_iMaxEraType         = idNumOrFk(*io, "maxEra");
		m_iReplacesUnitCombat  = jsonIdFk(*io, "replacesUnitCombat");
		m_iDomainCargoChange   = jsonIdFk(*io, "domainCargoChange");
		m_iSpecialCargoChange  = jsonIdFk(*io, "specialCargoChange");
		m_iSpecialCargoPrereq  = jsonIdFk(*io, "specialCargoPrereq");
		m_iSMNotSpecialCargoChange = jsonIdFk(*io, "smNotSpecialCargoChange");
		m_iSMNotSpecialCargoPrereq = jsonIdFk(*io, "smNotSpecialCargoPrereq");

		std::string r; if (jsonIdStr(*io, "renamesUnitTo", r) && !r.empty()) m_szRenamesUnitTo = CvWString(r.c_str());

		readIdList(*io, "unitCombats", m_aeUnitCombat);
		readIdList(*io, "notOnUnitCombats", m_aiNotOnUnitCombats);
		readIdList(*io, "notOnDomains", m_aiNotOnDomains);
		readIdList(*io, "prereqTerrains", m_aiPrereqTerrains);
		readIdList(*io, "prereqFeatures", m_aiPrereqFeatures);
		readIdList(*io, "prereqImprovements", m_aiPrereqImprovements);
		readIdList(*io, "prereqPlotBonuses", m_aiPrereqPlotBonuses);
		readIdList(*io, "prereqLocalBuildings", m_aiPrereqLocalBuildings);
		readIdList(*io, "prereqBonuses", m_aiPrereqBonuses);
		readIdList(*io, "negatesInvisibility", m_aiNegatesInvisibility);
	}

	// --- promotionLine: top-level {LINE: rank} object (owner 2026-06-16) ---
	if (const picojson::object* pl = jsonChildObj(o, "promotionLine"))
		if (!pl->empty())
		{
			const int id = jsonResolveId(pl->begin()->first);
			if (id >= 0) m_ePromotionLine = static_cast<PromotionLineTypes>(id);
			if (pl->begin()->second.is<double>()) m_iLinePriority = (int)pl->begin()->second.get<double>();
		}

	// --- vision LOS resolver: intensity pair-maps + the invisible/visible struct rows ---
	if (const picojson::object* vs = jsonChildObj(o, "vision"))
	{
		readIntMap(*vs, "visibilityIntensity",      m_aiVisibilityIntensity);
		readIntMap(*vs, "invisibilityIntensity",    m_aiInvisibilityIntensity);
		readIntMap(*vs, "visibilityIntensityRange", m_aiVisibilityIntensityRange);
		readTerrainChanges(*vs, "invisibleTerrain",       m_aInvisibleTerrainChanges);
		readTerrainChanges(*vs, "visibleTerrain",         m_aVisibleTerrainChanges);
		readTerrainChanges(*vs, "visibleTerrainRange",    m_aVisibleTerrainRangeChanges);
		readFeatureChanges(*vs, "invisibleFeature",       m_aInvisibleFeatureChanges);
		readFeatureChanges(*vs, "visibleFeature",         m_aVisibleFeatureChanges);
		readFeatureChanges(*vs, "visibleFeatureRange",    m_aVisibleFeatureRangeChanges);
		readImprovementChanges(*vs, "invisibleImprovement",    m_aInvisibleImprovementChanges);
		readImprovementChanges(*vs, "visibleImprovement",      m_aVisibleImprovementChanges);
		readImprovementChanges(*vs, "visibleImprovementRange", m_aVisibleImprovementRangeChanges);
	}

	// --- ai.unitCombatWeights {UC:int} -> UnitCombatModifier rows ---
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
		if (const picojson::object* w = jsonChildObj(*ai, "unitCombatWeights"))
			for (picojson::object::const_iterator it = w->begin(); it != w->end(); ++it)
				if (it->second.is<double>())
				{
					const int id = jsonResolveId(it->first);
					if (id < 0) continue;
					UnitCombatModifier r;
					r.eUnitCombat = static_cast<UnitCombatTypes>(id);
					r.iModifier   = (int)it->second.get<double>();
					m_aAIWeight.push_back(r);
				}

	// --- sound.sound ---
	if (const picojson::object* so = jsonChildObj(o, "sound")) jsonIdStr(*so, "sound", m_szSound);
}

// remove the FIRST occurrence of `val` from `v` (the archived find+erase, without pulling in <algorithm>).
static void eraseValue(std::vector<int>& v, int val)
{
	for (std::vector<int>::iterator it = v.begin(); it != v.end(); ++it)
		if (*it == val) { v.erase(it); return; }
}

// Pedia caches, computed post-load -- a FAITHFUL transcription of the archived CvPromotionInfo doPostLoadCaching
// (SourceArchive/Infos/CvPromotionInfo.cpp): the qualified set = the unitcombats this promotion applies to
// (identity.unitCombats via getUnitCombat) UNION the promotion line's unitcombat prereqs; the disqualified set =
// this promotion's notOnUnitCombats UNION the line's notOnUnitCombats, each removed from the qualified set.
void CvJsonPromotionInfo::setQualifiedUnitCombatTypes()
{
	m_aiQualifiedUnitCombat.clear();
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (getUnitCombat(iI))
		{
			m_aiQualifiedUnitCombat.push_back(iI);
		}
	}
	const PromotionLineTypes ePromotionLine = getPromotionLine();
	if (ePromotionLine > -1)
	{
		const CvJsonPromotionLineInfo& kLine = GC.getPromotionLineInfo(ePromotionLine);
		for (int iI = 0; iI < kLine.getNumUnitCombatPrereqTypes(); iI++)
		{
			const int iUnitCombat = kLine.getUnitCombatPrereqType(iI);
			if (!isQualifiedUnitCombatType(iUnitCombat))
			{
				m_aiQualifiedUnitCombat.push_back(iUnitCombat);
			}
		}
	}
}

void CvJsonPromotionInfo::setDisqualifiedUnitCombatTypes()
{
	m_aiDisqualifiedUnitCombat.clear();
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (isNotOnUnitCombatType(iI))
		{
			if (isQualifiedUnitCombatType(iI))
			{
				eraseValue(m_aiQualifiedUnitCombat, iI);
			}
			m_aiDisqualifiedUnitCombat.push_back(iI);
		}
	}
	const PromotionLineTypes ePromotionLine = getPromotionLine();
	if (ePromotionLine > -1)
	{
		const CvJsonPromotionLineInfo& kLine = GC.getPromotionLineInfo(ePromotionLine);
		for (int iI = 0; iI < kLine.getNumNotOnUnitCombatTypes(); iI++)
		{
			const int iUnitCombat = kLine.getNotOnUnitCombatType(iI);
			if (!isNotOnUnitCombatType(iUnitCombat))
			{
				if (isQualifiedUnitCombatType(iUnitCombat))
				{
					eraseValue(m_aiQualifiedUnitCombat, iI);   // faithful to the archived: it searches iI (the loop index), not iUnitCombat
				}
				m_aiDisqualifiedUnitCombat.push_back(iUnitCombat);
			}
		}
	}
}
