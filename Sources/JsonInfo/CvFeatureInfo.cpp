//
//	CvFeatureInfo::mapFrom -- base core reading + availability, then the feature's LIVE real members from the
//	curator's real shapes: the plot yield/health/defense/culture/vision families and the `identity` placement fields.
//	HUMAN-native values (the cascade ×100s on its side). FK resolution via the kept type registry. See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvFeatureInfo.h"
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoFeature.h"   // complete CvArtInfoFeature -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)
#include "CvCascadePropertyBridge.h" // the shared grants.repeatable PROPERTY pulse -> manipulator walk
#include "AI/CvGameAI.h"          // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvFeatureInfo::CvFeatureInfo()
	: m_iMovementCost(0), m_iDefenseModifier(0), m_iHealthPercent(0), m_iCultureDistance(0),
	  m_iSeeThroughChange(0), m_iPopDestroys(-1), m_iAppearanceProbability(0), m_iDisappearanceProbability(0),
	  m_iGrowthProbability(0), m_iSpreadProbability(0), m_iAdvancedStartRemoveCost(0), m_iEffectProbability(0), m_iZobristValue(0),
	  m_bImpassable(false), m_bNoCity(false), m_bNoImprovement(false), m_bNoBonus(false), m_bCountsAsPeak(false),
	  m_bRequiresFlatlands(false), m_bRequiresRiver(false), m_bAddsFreshWater(false), m_bNukeImmune(false),
	  m_bNoCoast(false), m_bNoRiver(false), m_bNoAdjacent(false), m_bCoastalOnly(false), m_bVisibleAlways(false),
	  m_bIgnoreTerrainCulture(false), m_bCanGrowAnywhere(false), m_iWorldSoundscapeScriptId(-1)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) { m_aiYieldChange[i] = 0; m_aiRiverYieldChange[i] = 0; }
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvFeatureInfo ctor did (SourceArchive/Infos/CvFeatureInfo.cpp:54). CvPlot XORs it into m_movementCharacteristicsHash.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

// Reads entity[channel]["plot"]["flat"] -- the curator emits it as a bare number (unconditional yield) OR a mixed
// array of bare numbers and {value, enabled} objects (curate_feature: the base yield lands via apply_channel, then
// the RiverYieldChange HAS_RIVER-gated entries are appended in post_process via _inject). Splits the total:
// unconditional entries -> iBase (legacy YieldChange), HAS_RIVER-gated entries -> iRiver (legacy RiverYieldChange).
// HUMAN ints (feature yields are not x100), so read the numeric leaf directly.
static void readFeatureYield(const picojson::object& o, const char* channel, int& iBase, int& iRiver)
{
	iBase = 0; iRiver = 0;
	const picojson::object* fo = jsonChildObj(o, channel);  if (!fo) return;
	const picojson::object* so = jsonChildObj(*fo, "plot"); if (!so) return;
	picojson::object::const_iterator u = so->find("flat");
	if (u == so->end()) return;
	if (u->second.is<double>()) { iBase += (int)u->second.get<double>(); return; }
	if (!u->second.is<picojson::array>()) return;
	const picojson::array& a = u->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].is<double>()) { iBase += (int)a[i].get<double>(); continue; }
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		picojson::object::const_iterator val = e.find("value");
		if (val == e.end() || !val->second.is<double>()) continue;
		const int iVal = (int)val->second.get<double>();
		picojson::object::const_iterator en = e.find("enabled");
		const bool bRiver = (en != e.end() && en->second.is<std::string>() && en->second.get<std::string>() == "HAS_RIVER");
		if (bRiver) iRiver += iVal; else iBase += iVal;
	}
}

bool CvFeatureInfo::isOnlyBad() const
{
	// Byte-faithful to the archived CvFeatureInfo::isOnlyBad (BUG city-plot-status).
	if (getHealthPercent() > 0 || isAddsFreshWater()) return false;
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) if (getYieldChange(i) > 0) return false;
	return true;
}

bool CvFeatureInfo::isTerrain(int iTerrain) const
{
	for (size_t i = 0; i < m_aeValidTerrains.size(); ++i) if ((int)m_aeValidTerrains[i] == iTerrain) return true;
	return false;
}

void CvFeatureInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	m_aeValidTerrains.clear(); m_aeMapCategories.clear();
	CvInfo::mapFrom(entity);   // core reading + availability model
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: the feature's grants.repeatable property pulses (bamboo/jungle air-pollution
	// sinks, reef water-pollution sinks -- authored per json §5) feed the KEEP-legacy solver via the plot gather,
	// mirroring the legacy PLOT+NEAR shape. The ONE shared pulse walk (clear-and-refill inside).
	CascadePropertyBridge::bridgePulses(getGrants(), m_PropertyManipulators);

	// plot yield families -- split each channel's flat entries into the unconditional base (YieldChange) and the
	// HAS_RIVER-gated extra (RiverYieldChange). Handles both the scalar and the mixed-array authored shapes.
	readFeatureYield(o, "food",       m_aiYieldChange[YIELD_FOOD],       m_aiRiverYieldChange[YIELD_FOOD]);
	readFeatureYield(o, "production", m_aiYieldChange[YIELD_PRODUCTION], m_aiRiverYieldChange[YIELD_PRODUCTION]);
	readFeatureYield(o, "commerce",   m_aiYieldChange[YIELD_COMMERCE],   m_aiRiverYieldChange[YIELD_COMMERCE]);
	m_iCultureDistance = jsonFamVal(o, "cultureDistance", "plot", "flat");
	m_iHealthPercent   = jsonFamVal(o, "health", "plot", "percent");
	m_iDefenseModifier = jsonFamMemberVal(o, "defense", "plot", "amount", "percent");
	m_iSeeThroughChange = jsonFamMemberVal(o, "vision", "plot", "seeThrough", "flat");

	// identity: placement + relief fields
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iMovementCost      = jsonIdInt(*io, "movementCost");
		m_iPopDestroys       = jsonIdInt(*io, "popDestroys", -1);  // legacy load default -1 = never destroyed by city pop
		m_iAppearanceProbability    = jsonIdInt(*io, "appearance");
		m_iDisappearanceProbability = jsonIdInt(*io, "disappearance");
		m_iGrowthProbability        = jsonIdInt(*io, "growth");
		m_iSpreadProbability        = jsonIdInt(*io, "spread");
		m_bImpassable        = jsonIdBool(*io, "impassable");
		m_bNoCity            = jsonIdBool(*io, "noCity");
		m_bNoImprovement     = jsonIdBool(*io, "noImprovement");
		m_bNoBonus           = jsonIdBool(*io, "noBonus");
		m_bCountsAsPeak      = jsonIdBool(*io, "countsAsPeak");
		m_bRequiresFlatlands = jsonIdBool(*io, "requiresFlatlands");
		m_bRequiresRiver     = jsonIdBool(*io, "requiresRiver");
		m_bAddsFreshWater    = jsonIdBool(*io, "addsFreshWater");
		m_bNukeImmune        = jsonIdBool(*io, "nukeImmune");
		m_bNoCoast           = jsonIdBool(*io, "noCoast");
		m_bNoRiver           = jsonIdBool(*io, "noRiver");
		m_bNoAdjacent        = jsonIdBool(*io, "noAdjacent");
		m_bCoastalOnly       = jsonIdBool(*io, "coastalOnly");
		m_bVisibleAlways     = jsonIdBool(*io, "visibleAlways");
		m_bIgnoreTerrainCulture = jsonIdBool(*io, "ignoreTerrainCulture");
		m_bCanGrowAnywhere   = jsonIdBool(*io, "canGrowAnywhere");

		picojson::object::const_iterator vt = io->find("validTerrains");
		if (vt != io->end() && vt->second.is<picojson::array>())
		{
			const picojson::array& a = vt->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeValidTerrains.push_back((TerrainTypes)id); }
		}
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}
	// cost block: the advanced-start feature-removal cost (mapping routes iAdvancedStartRemoveCost -> cost, not identity)
	if (const picojson::object* co = jsonChildObj(o, "cost"))
		m_iAdvancedStartRemoveCost = jsonIdInt(*co, "advancedStartRemoveCost");

	// world.art: the ART_DEF_* icon tag + the on-map effect (EFFECT_BIRDSCATTER + its probability), via the shim's getArtInfo
	if (const picojson::object* art = jsonWorldArt(o))
	{
		jsonIdStr(*art, "define", m_szArtDefineTag);
		if (const picojson::object* eff = jsonChildObj(*art, "effect"))
		{
			jsonIdStr(*eff, "type", m_szEffectType);
			m_iEffectProbability = jsonIdInt(*eff, "probability");
		}
	}

	// sound block: the feature-growth 2D sound (a plain string tag) PLUS the on-map audio tags resolved to runtime
	// audio-manager indices at info-load -- EXACTLY the archived CvFeatureInfo::read mechanism (gDLL->getAudioTagIndex).
	// soundscape -> AUDIOTAG_SOUNDSCAPE; each footsteps entry keys a FOOTSTEP_AUDIO_* type (jsonResolveId == legacy
	// GetInfoClass) to its AS3D_* script tag (default iScriptType).
	if (const picojson::object* snd = jsonChildObj(o, "sound"))
	{
		jsonIdStr(*snd, "growth", m_szGrowthSound);

		picojson::object::const_iterator ss = snd->find("soundscape");
		if (ss != snd->end() && ss->second.is<std::string>() && ss->second.get<std::string>().length() > 0)
			m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex(ss->second.get<std::string>().c_str(), AUDIOTAG_SOUNDSCAPE);
		// else: m_iWorldSoundscapeScriptId stays -1 (the legacy absent-tag read default).

		picojson::object::const_iterator fs = snd->find("footsteps");
		if (fs != snd->end() && fs->second.is<picojson::array>())
		{
			const picojson::array& a = fs->second.get<picojson::array>();
			m_ai3DAudioScriptFootstepIndex.assign(GC.getNumFootstepAudioTypes(), -1);   // legacy InitList default -1
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& e = a[i].get<picojson::object>();
				for (picojson::object::const_iterator it = e.begin(); it != e.end(); ++it)
				{
					const int iType = jsonResolveId(it->first);   // FOOTSTEP_AUDIO_* -> footstep audio type index (legacy GetInfoClass)
					if (iType < 0 || iType >= (int)m_ai3DAudioScriptFootstepIndex.size()) continue;   // mirrors legacy's iIndexVal != -1 skip
					if (it->second.is<std::string>() && it->second.get<std::string>().length() > 0)
						m_ai3DAudioScriptFootstepIndex[iType] = gDLL->getAudioTagIndex(it->second.get<std::string>().c_str());
					// empty script tag -> slot stays -1 (legacy szTemp.GetLength() > 0 ? ... : -1)
				}
			}
		}
	}

	// grants block: the on-entry unit transform (module-only; no base feature authors it, but this is its curator address).
	if (const picojson::object* g = jsonChildObj(o, "grants")) jsonIdStr(*g, "onUnitChangeTo", m_szOnUnitChangeTo);

	// (m_iZobristValue is drawn in the ctor -- non-XML runtime value, see there.)
}

const CvArtInfoFeature* CvFeatureInfo::getArtInfo() const
{
	return ARTFILEMGR.getFeatureArtInfo(getArtDefineTag());
}
const char* CvFeatureInfo::getButton() const
{
	const CvArtInfoFeature* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
// Art-define tier: variety count + secondary-render test live in the art define (CvArtInfoFeature), not feature-curator
// output. Delegate to getArtInfo() exactly as the archived CvFeatureInfo did (getNumVarieties -> Python pedia
// CyInfoInterface2; canBeSecondary -> the historic secondary-feature render test).
int CvFeatureInfo::getNumVarieties() const
{
	const CvArtInfoFeature* p = getArtInfo();
	return p != NULL ? p->getNumVarieties() : 0;
}
bool CvFeatureInfo::canBeSecondary() const
{
	const CvArtInfoFeature* p = getArtInfo();
	return p != NULL && !(p->isRiverArt() || p->getTileArtType() != TILE_ART_TYPE_NONE);
}
