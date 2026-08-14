// unit.cpp


#include "Tools/FProfiler.h"
#include "Conditions/CvConditionEval.h"   // cascadeGateOk -- the entity-level enabled/disabled pair
#include "Conditions/CvConditionQuery.h"  // the ONE structural read over a parsed `requires` tree
#include "Enabler/CvEnablerKernel.h"      // everAvailable -- the CAN-I-EVER bar (the corp spread gate)
#include "Infos/CvClassificationIds.h"   // the generated SKILL_/TAG_/CAPABILITY_ id table

#include "CvGameCoreDLL.h"

#include "Data/CvInfoValuation.h"   // InfoValuation::collectHealByUnitCombat + HealByUnitCombat
#include "Engine/CvGameSpeedScale.h"
#include "AI/BetterBTSAI.h"
#include "CvArea.h"
#include "CvBuildingInfo.h"
#include "CvCity.h"
#include "UI/CvEventReporter.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvImprovementInfo.h"
#include "CvInfos.h"
#include "CvUnitCombatInfo.h"
#include "CvTraitInfo.h"
#include "UI/CvOutcomeList.h"
#include "CvMap.h"
#include "AI/CvPlayerAI.h"
#include "CvPlot.h"
#include "CvPopupInfo.h"
#include "Infrastructure/CvPython.h"
#include "CvSelectionGroup.h"
#include "Spine/CvEventSpine.h"   // emitUnitCreated / emitUnitEnteredCity / emitNameChange -- the DOMAIN emit surface
#include "AI/CvTeamAI.h"
#include "CvUnit.h"
#include "CvUnitSelectionCriteria.h"
#include "UI/CvViewport.h"
#include "Infrastructure/CvDLLEngineIFaceBase.h"
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"
#include "Infrastructure/CvDLLEntity.h"
#include "UI/CvGraphicsTrace.h"   // the [GFX] scene-occupancy trace
#include "Infrastructure/CvDLLEntityIFaceBase.h"
#include "Infrastructure/CvDLLFAStarIFaceBase.h"
#include "Infrastructure/CvDLLUtilityIFaceBase.h"
#include "Python/CyPlot.h"
#include "Python/CyUnit.h"
#include "Infrastructure/CvDLLButtonPopup.h"

static CvEntity* g_dummyEntity = NULL;
static CvUnit*	 g_dummyUnit = NULL;
static int		 g_numEntities = 0;
static int		 g_dummyUsage = 0;
static bool		 g_bUseDummyEntities = false;



//	static buffers allocated once and used during read and write only
int*	CvUnit::g_paiTempPromotionFreeCount = NULL;
int*	CvUnit::g_paiTempPromotionFromTraitCount = NULL;
bool*	CvUnit::g_pabTempValidBuildUp = NULL;
int*	CvUnit::g_paiTempExtraUnitCombatModifier = NULL;
bool*	CvUnit::g_pabTempHasPromotion = NULL;
bool*	CvUnit::g_pabTempHasUnitCombat = NULL;
int*	CvUnit::g_paiTempExtraFlankingStrengthbyUnitCombatType = NULL;
int*	CvUnit::g_paiTempHealUnitCombatTypeVolume = NULL;
int*	CvUnit::g_paiTempHealUnitCombatTypeAdjacentVolume = NULL;
int*	CvUnit::g_paiTempHealAsDamage = NULL;
bool	CvUnit::m_staticsInitialized = false;

bool CvUnit::isDummyEntity(const CvEntity* entity)
{
	return (entity == g_dummyEntity);
}

bool CvUnit::isRealEntity(const CvEntity* entity)
{
	return (entity != NULL && entity != g_dummyEntity);
}

// Public Functions...
#pragma warning( disable : 4355 )
CvUnit::CvUnit(bool bIsDummy) : m_GameObject(this),
m_Properties(this)
{
	m_aiExtraDomainModifier = new int[NUM_DOMAIN_TYPES];
	m_aiExtraVisibilityIntensity = new int[GC.getNumInvisibleInfos()];
	m_aiExtraInvisibilityIntensity = new int[GC.getNumInvisibleInfos()];
	m_aiExtraVisibilityIntensityRange = new int[GC.getNumInvisibleInfos()];
	m_aiExtraVisibilityIntensitySameTile = new int[GC.getNumInvisibleInfos()];
	m_aiNegatesInvisibleCount = new int[GC.getNumInvisibleInfos()];
	m_aExtraInvisibleTerrains.clear();
	m_aExtraInvisibleFeatures.clear();
	m_aExtraInvisibleImprovements.clear();
	m_aExtraVisibleTerrains.clear();
	m_aExtraVisibleFeatures.clear();
	m_aExtraVisibleImprovements.clear();
	m_aExtraVisibleTerrainRanges.clear();
	m_aExtraVisibleFeatureRanges.clear();
	m_aExtraVisibleImprovementRanges.clear();

	m_iMaxMoveCacheTurn = -1;

	if (g_dummyUnit == NULL && !bIsDummy)
	{
		g_dummyUnit = new CvUnitAI(true);

		if (GC.getENABLE_DYNAMIC_UNIT_ENTITIES())
		{
			g_bUseDummyEntities = true;
		}
	}

	if (!g_bUseDummyEntities)
	{
		CvDLLEntity::createUnitEntity(this); // create and attach entity to unit
	}
	else if (g_dummyEntity == NULL)
	{
		CvDLLEntity::createUnitEntity(this); // create and attach entity to unit

		g_dummyEntity = getEntity();
	}
	else
	{
		setEntity(g_dummyEntity);
		g_dummyUsage++;
	}

	bGraphicsSetup = false;

	reset(0, NO_UNIT, NO_PLAYER, true);

	if (!m_staticsInitialized)
	{
		//	Allocate static buffers to be used during read and write
		g_paiTempPromotionFreeCount = new int[GC.getNumPromotionInfos()];
		g_paiTempPromotionFromTraitCount = new int [GC.getNumPromotionInfos()];
		g_pabTempValidBuildUp = new bool[GC.getNumPromotionLineInfos()];
		g_paiTempExtraUnitCombatModifier = new int[GC.getNumUnitCombatInfos()];
		g_pabTempHasPromotion = new bool[GC.getNumPromotionInfos()];
		g_pabTempHasUnitCombat = new bool[GC.getNumUnitCombatInfos()];
		g_paiTempExtraFlankingStrengthbyUnitCombatType = new int[GC.getNumUnitCombatInfos()];
		g_paiTempHealUnitCombatTypeVolume = new int[GC.getNumUnitCombatInfos()]();
		g_paiTempHealUnitCombatTypeAdjacentVolume = new int[GC.getNumUnitCombatInfos()]();
		g_paiTempHealAsDamage = new int[GC.getNumUnitCombatInfos()];

		m_staticsInitialized = true;
	}
}


CvUnit::~CvUnit()
{
	if (!isUsingDummyEntities())
	{
		// Don't need to remove entity when the app is shutting down, or crash can occur
		if (!gDLL->GetDone() && GC.IsGraphicsInitialized())
		{
			gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
			CvDLLEntity::removeEntity(); // remove entity from engine
		}
		CvDLLEntity::destroyEntity(); // delete CvUnitEntity and detach from us
	}
	SAFE_DELETE_ARRAY(m_aiExtraDomainModifier);
	SAFE_DELETE_ARRAY(m_aiExtraVisibilityIntensity);
	SAFE_DELETE_ARRAY(m_aiExtraInvisibilityIntensity);
	SAFE_DELETE_ARRAY(m_aiExtraVisibilityIntensityRange);
	SAFE_DELETE_ARRAY(m_aiExtraVisibilityIntensitySameTile);
	SAFE_DELETE_ARRAY(m_aiNegatesInvisibleCount);
	SAFE_DELETE(m_commander);
	SAFE_DELETE(m_commodore);
	SAFE_DELETE(m_worker);
}


bool CvUnit::isUsingDummyEntities() const
{
	const CvEntity* entity = getEntity();

	return entity && g_dummyEntity == entity;
}

void CvUnit::destroyCurrentEntity()
{
	const CvEntity* pEntity = getEntity();

	if (pEntity == NULL)
	{
		return;
	}

	if (isRealEntity(pEntity))
	{
		// Destroy old entity, don't need to remove entity when the app is shutting down, or crash can occur
		if (!gDLL->GetDone() && GC.IsGraphicsInitialized())
		{
			gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
			CvDLLEntity::removeEntity(); // remove entity from engine
		}
		CvDLLEntity::destroyEntity(); // delete CvUnitEntity and detach from us

		//	Only the dummy-entity path ever incremented this, so only it may decrement — otherwise the count
		//	the [GFX] entity line reports drifts negative on a build that hands every unit a real entity.
		if (g_bUseDummyEntities)
		{
			g_numEntities--;
		}
	}
	else
	{
		g_dummyUsage--;
	}
	setEntity(NULL);
}

void CvUnit::rebuildEntityArt()
{
	//	The unit's MODEL changed — a warlord attaching swaps its art — which is the one reason a scene node
	//	genuinely has to be recreated. reloadEntity keeps a still-wanted one, so it cannot serve this.
	if (!IsSelected())
	{
		destroyCurrentEntity();
	}
	reloadEntity();
}

void CvUnit::reloadEntity(bool bForceLoad)
{
	const bool bNeedsRealEntity =
	(
		!g_bUseDummyEntities || bForceLoad
		||
		plot() && plot()->isActiveVisible(false)
		&&
		(plot()->getCenterUnit(false) == this || getOwner() == GC.getGame().getActivePlayer())
	);

	if (!IsSelected())
	{
		//	⛔ AN ENTITY THAT IS ALREADY THE KIND WE WANT IS KEPT. The engine's move queue lives ON the scene
		//	node — which this function's own RemoveUnitFromBattle call concedes — and CvSelectionGroup::groupMove
		//	destroys it between QueueMove and ExecuteMove. Recreating there executes the move on a node whose
		//	queue is empty and strands the walk animation on the tile the unit left.
		if (getEntity() != NULL && isRealEntity(getEntity()) != bNeedsRealEntity)
		{
			destroyCurrentEntity();
		}

		if (!getEntity())
		{
			if (g_bUseDummyEntities)
			{
				if (bNeedsRealEntity)
				{
					// Create and attach entity to unit
					CvDLLEntity::createUnitEntity(this);
					g_numEntities++;
					bGraphicsSetup = false;
				}
				else
				{
					setEntity(g_dummyEntity);
					g_dummyUsage++;
				}

				//	A REAL entity is a scene node, and a scene node is where the EXE's per-instance model/texture
				//	memory goes (memory-footprint.md) -- so this is the memory question asked at the moment it
				//	moves. It replaces an every-100 OutputDebugString, which put the one number that answers it on
				//	no readable surface at all and compiled to nothing in FinalRelease besides.
				//	bNeedsRealEntity's own terms, so the decision is readable rather than just its outcome -- it is
				//	partly circular (a unit needs a real entity because it IS the centre unit, and the centre unit is
				//	chosen from units that have one).
				gfxTraceEntity(this, bNeedsRealEntity, g_numEntities, g_dummyUsage,
					plot() != NULL && plot()->isActiveVisible(false),
					plot() != NULL && plot()->getCenterUnit(false) == this,
					getOwner() == GC.getGame().getActivePlayer());
			}
			else if (plot())
			{
				// Create and attach entity to unit
				CvDLLEntity::createUnitEntity(this);
				bGraphicsSetup = false;
			}
		}

		if (!bGraphicsSetup && bNeedsRealEntity && plot())
		{
			setupGraphical();
			bGraphicsSetup = true;
		}
	}
	else OutputDebugString("Reload of selected unit\n");
}

void CvUnit::changeIdentity(UnitTypes eUnit)
{
	reset(getID(), eUnit, getOwner(), false, true);
	//	Same id is now a differnt unit - make sure no old cached path info
	//	gets used for it
	CvPlot::NextCachePathEpoch();
}

void CvUnit::init(int iID, UnitTypes eUnit, UnitAITypes eUnitAI, PlayerTypes eOwner, int iX, int iY, DirectionTypes eFacingDirection, int iBirthmark)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);

	//	If the current viewport is not yet initialized center it on the first unit created for the active player
	if (GC.getGame().getActivePlayer() == eOwner
	&& GC.getCurrentViewport()->getState() == VIEWPORT_MODE_UNINITIALIZED
	&& UNIT_BIRTHMARK_TEMP_UNIT != iBirthmark)
	{
		GC.getCurrentViewport()->setOffsetToShow(iX, iY);
	}
	//--------------------------------
	// Init saved data
	reset(iID, eUnit, eOwner);

	// Koshling -  moved this earlier to get unitAI set up so that
	// constraint checking on the unitAI can work more uniformly
	AI_init(eUnitAI, iBirthmark);

	if (eFacingDirection == NO_DIRECTION)
		m_eFacingDirection = DIRECTION_SOUTH;
	else m_eFacingDirection = eFacingDirection;

	//--------------------------------
	// Init containers

	//--------------------------------
	// Init pre-setup() data
	//GC.getGame().logOOSSpecial(13, getID(), iX, iY);
	setXY(iX, iY, false, true, false, false, true);

	//TB OOS fix - POSSIBLE that this represents a fix but I consider it a longshot since they should really mean the same thing (-1)
	if (!getGroup())
	{
		::MessageBox(
			NULL, getGroupID() == FFreeList::INVALID_INDEX ?
				"Unit with NULL group ID after set position in init\n"
				:
				"Unit with no group after set position in init\n",
			"CvGameCoreDLL Diagnostics", MB_OK
		);
	}
	// ! TB

	if (iBirthmark != UNIT_BIRTHMARK_TEMP_UNIT)
	{
		if (plot()->getPlotCity())
		{
			setCityOfOrigin(plot()->getPlotCity());
		}

		if ((int)m_pUnitInfo->getBuilds().size() > 0)
		{
			m_worker = new UnitCompWorker();
		}
		const int iNumNames = static_cast<int>(m_pUnitInfo->getUniqueNames().size());

		if (GC.getGame().getUnitCreatedCount(getUnitType()) < iNumNames)
		{
			const int iOffset = GC.getGame().getSorenRandNum(iNumNames, "Unit name selection");

			for (int iI = 0; iI < iNumNames; iI++)
			{
				CvWString szName = gDLL->getText(m_pUnitInfo->getUniqueNames()[(iI + iOffset) % iNumNames].c_str());

				if (!GC.getGame().isGreatPersonBorn(szName))
				{
					setName(szName);
					GC.getGame().addGreatPersonBornName(szName);
					break;
				}
			}
		}
		setGameTurnCreated(GC.getGame().getGameTurn());
		calcUpkeep(); // This updates total upkeep on the player level too

		GC.getGame().incrementUnitCreatedCount(eUnit);
		GET_PLAYER(eOwner).changeUnitCount(eUnit, 1);

		if (eUnitAI == UNITAI_SUBDUED_ANIMAL)
		{
			GET_PLAYER(eOwner).NoteAnimalSubdued();
		}

		if (m_pUnitInfo->getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) > 0)
		{
			GET_PLAYER(eOwner).changeNumNukeUnits(1);
		}

		if (isMilitaryBranch())
		{
			GET_PLAYER(eOwner).changeNumMilitaryUnits(1);
		}

		doSetUnitCombats();
		doSetFreePromotions(true);
		// The unit's OWN grants.promotions are handed over by the GRANTS MACHINE off this emit. Its position is
		// pinned between two neighbours and is wrong on either side of them:
		//   AFTER doSetFreePromotions -- a promotion can grant UNIT-COMBATS, so the combat-class set is only
		//     settled once that pass has run. Emitting before it let the machine mutate the set mid-pass; the
		//     outcome lists are merged from unit + combat classes, which crashed as a wild CvTeam::isHasTech
		//     read out of CvOutcome::isPossibleSomewhere.
		//   BEFORE doSetDefaultStatuses -- that calls statusUpdate(), establishing the unit's status/animation
		//     state. Emitting after it left promotions landing on an already-computed visual state (units drawn
		//     only after re-selection; a fortified unit stuck in its run animation).
		emitUnitCreated((int)getUnitType(), getID(), (int)getOwner());
		doSetDefaultStatuses();

		// Cache initial healer values
		{
			std::vector<HealByUnitCombat> healRows;
			InfoValuation::collectHealByUnitCombat(m_pUnitInfo->getModifiers(), healRows);
			for (size_t iRow = 0; iRow < healRows.size(); ++iRow)
			{
				const HealByUnitCombat& kRow = healRows[iRow];
				//	the accumulators carry whole hit points; the deposits are ×100 amounts
				changeHealUnitCombatTypeVolume((UnitCombatTypes)kRow.iUnitCombat, kRow.iHeal / 100);
				changeHealUnitCombatTypeAdjacentVolume((UnitCombatTypes)kRow.iUnitCombat, kRow.iAdjacentHeal / 100);
			}
		}

		if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
		{
			setSMValues();

			// if unit doesn't have a group rank, it doesn't count as a SM unit at all
			if (groupRank() > 0)
			{
				GET_PLAYER(eOwner).changeUnitCountSM(eUnit, smGroupMultiplier(groupRank()));
			}
		}
		else
		{
			GET_PLAYER(eOwner).changeAssets(m_pUnitInfo->getWorth());
			GET_PLAYER(eOwner).changeUnitPower(m_pUnitInfo->getMilitaryWorth());
		}
		//--------------------------------
		// Init non-saved data
		setupGraphical();

		//--------------------------------
		// Init other game data
		plot()->updateCenterUnit();

		plot()->setFlagDirty(true);

		if (getDomainType() == DOMAIN_LAND && baseCombatStr() > 0
		&& (GC.getGame().getBestLandUnit() == NO_UNIT || baseCombatStrHuman() > GC.getGame().getBestLandUnitCombat()))
		{
			GC.getGame().setBestLandUnit(getUnitType());
		}

		if (eOwner == GC.getGame().getActivePlayer())
		{
			gDLL->getInterfaceIFace()->setDirty(GameData_DIRTY_BIT, true);
		}

		if (isWorldUnit(eUnit))
		{
			for (int iI = 0; iI < MAX_PLAYERS; iI++)
			{
				if (GET_PLAYER((PlayerTypes)iI).isAlive())
				{

					if (GET_TEAM(getTeam()).isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						AddDLLMessage(
							(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_SOMEONE_CREATED_UNIT", GET_PLAYER(eOwner).getNameKey(), getNameKey()),
							"AS2D_WONDER_UNIT_BUILD", MESSAGE_TYPE_MAJOR_EVENT, getButton(),
							GC.getCOLOR_UNIT_TEXT(), getX(), getY(), true, true
						);
					}
					else
					{
						AddDLLMessage(
							(PlayerTypes) iI, false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_UNKNOWN_CREATED_UNIT", getNameKey()),
							"AS2D_WONDER_UNIT_BUILD", MESSAGE_TYPE_MAJOR_EVENT, getButton(),
							GC.getCOLOR_UNIT_TEXT()
						);
					}
				}
			}
			GC.getGame().addReplayMessage(
				REPLAY_MESSAGE_MAJOR_EVENT, eOwner,
				gDLL->getText("TXT_KEY_MISC_SOMEONE_CREATED_UNIT", GET_PLAYER(eOwner).getNameKey(), getNameKey()),
				getX(), getY(), GC.getCOLOR_UNIT_TEXT()
			);
		}
		setGGExperienceEarnedTowardsType();

		if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
		{
			setHasAnyInvisibility();
		}
		establishBuildups();

		CvEventReporter::getInstance().unitCreated(this);
	}
}


// FUNCTION: reset()
// Initializes data members that are serialized.
void CvUnit::reset(int iID, UnitTypes eUnit, PlayerTypes eOwner, bool bConstructorCall, bool bIdentityChange)
{
	PROFILE_EXTRA_FUNC();
	clearCityOfOrigin();

	m_iHealUnitCombatCount = 0;


	m_iID = iID;
	if (!bIdentityChange)
	{
		m_iGroupID = FFreeList::INVALID_INDEX;
	}
	m_iHotKeyNumber = -1;
	m_iX = INVALID_PLOT_COORD;
	m_iY = INVALID_PLOT_COORD;
	m_iLastMoveTurn = 0;
	m_iReconX = INVALID_PLOT_COORD;
	m_iReconY = INVALID_PLOT_COORD;
	m_iGameTurnCreated = 0;
	m_iDamage = 0;
	m_iMoves = 0;
	m_iExperience = 0;
	m_iLevel = 1;
	m_iCargo = 0;
	m_iSMCargo = 0;
	m_iAttackPlotX = INVALID_PLOT_COORD;
	m_iAttackPlotY = INVALID_PLOT_COORD;
	m_iCombatTimer = 0;
	m_iCombatFirstStrikes = 0;
	m_iFortifyTurns = 0;
	m_iBuildUpTurns = 0;
	m_iBlitzCount = 0;
	m_iAmphibCount = 0;
	m_iRiverCount = 0;
	m_iEnemyRouteCount = 0;
	m_iAlwaysHealCount = 0;
	m_iHillsDoubleMoveCount = 0;
	m_iImmuneToFirstStrikesCount = 0;
	m_iAlwaysInvisibleCount = 0;

	m_iDefensiveVictoryMoveCount = 0;
	m_iFreeDropCount = 0;
	m_iOffensiveVictoryMoveCount = 0;

	m_iOneUpCount = 0;
	m_iPillageCultureCount = 0;
	m_iPillageEspionageCount = 0;
	m_iPillageMarauderCount = 0;
	m_iPillageOnMoveCount = 0;
	m_iPillageOnVictoryCount = 0;
	m_iPillageResearchCount = 0;
	m_iCelebrityHappy = 0;
	m_iCollateralDamageLimitChange = 0;
	m_iCollateralDamageMaxUnitsChange = 0;
	m_iCombatLimitChange = 0;
	m_iExtraDropRange = 0;

	m_iSurvivorChance = 0;

	m_iExtraMoves = 0;
	m_iUpkeep100 = 0;
	m_iExtraMoveDiscount = 0;
	//TB Combat Mods Begin
	m_iStampedeCount = 0;
	m_iAttackOnlyCitiesCount = 0;
	m_iIgnoreNoEntryLevelCount = 0;
	m_iIgnoreZoneofControlCount = 0;
	m_iFliesToMoveCount = 0;
	m_iSMStrength = 0;
	m_iOnslaughtCount = 0;
	m_iRetrainsAvailable = 0;
	m_iQualityBaseTotal = 0;
	m_iGroupBaseTotal = 0;
	m_iSizeBaseTotal = 0;
	m_iExtraQuality = 0;
	m_iExtraGroup = 0;
	m_iExtraSize = 0;
	m_iSMCargoVolume = 0;
	m_iSMExtraCargoVolume = 0;
	m_iSMCargoVolumeModifier = 0;
	m_iCannotMergeSplitCount = 0;


	m_iExtraBreakdownChance = 0;
	m_iExtraBreakdownDamage = 0;
	m_iExtraCombatModifierPerSizeMore = 0;
	m_iExtraCombatModifierPerSizeLess = 0;
	m_iExtraCombatModifierPerVolumeMore = 0;
	m_iExtraCombatModifierPerVolumeLess = 0;
	m_iExtraMaxHP = 0;
	m_iSMAssetValue = 0;
	m_iSMPowerValue = 0;
	m_iSMHPValue = 0;
	//TB Combat Mods End
	m_iExtraBombardRate = 0;
	m_iSMBombardRate = 0;
	m_iSMAirBombBaseRate = 0;
	m_iSMBaseWorkRate = 0;
	m_iSMRevoltProtection = 0;

	m_iRevoltProtection = 0;
	m_iCollateralDamageProtection = 0;
	m_iPillageChange = 0;
	m_iUpgradeDiscount = 0;
	m_iExperiencePercent = 0;
	m_iKamikazePercent = 0;
	m_eFacingDirection = DIRECTION_SOUTH;
	for (int iStatus = 0; iStatus < NUM_UNIT_STATUSES; ++iStatus)
	{
		m_aiStatusTurns[iStatus] = 0;
	}

	m_bCanRespawn = false; // Koshling - intentionally not saved - m_bCanrespawn should never persist in saves
	// as it is used only within a combat round and set upon unit death IF the unit has outstanding oneUpCount.
	// In some circumstances an autosave can save a state where the unit has just been respawned,
	// but m_bCanRespawn has not yet been reset at the start of the next turn, which leaves it bugged in the next turn's combat.

	m_bSurvivor = false;
	m_bMadeAttack = false;
	//TB Combat Mods (Att&DefCounters)
	m_iRoundCount = 0;
	m_iAttackCount = 0;
	m_iDefenseCount = 0;
	//TB Combat Mods end
	m_bMadeInterception = false;
	m_bPromotionReady = false;
	m_bDeathDelay = false;
	m_bInfoBarDirty = false;
	m_bBlockading = false;
	m_bAirCombat = false;
	m_bHasBuildUp = false;
	m_bInhibitMerge = false;
	m_bInhibitSplit = false;
	m_bIsBuildUp = false;
	m_bIsReligionLocked = false;

	m_iCanMovePeaksCount = 0;
	// Koshling - enhanced mountaineering mode to differentiate between ability to move through
	// mountains, and ability to lead a stack through mountains
	m_iCanLeadThroughPeaksCount = 0;

	if (eUnit != NO_UNIT)
	{
		m_movementCharacteristicsHash = GC.getUnitInfo(eUnit).getZobristValue();
	}

	m_iSleepTimer = 0;
	//@MOD Commanders/Commodores: reset parameters
	m_iCommanderID = -1;
	m_iCommodoreID = -1;
	m_iUsedCommanderID = -1;
	m_iUsedCommodoreID = -1;
	m_eOriginalOwner = eOwner;
	m_eNewDomainCargo = NO_DOMAIN;
	m_eNewSpecialCargo = NO_SPECIALUNIT;
	m_eNewSMNotSpecialCargo = NO_SPECIALUNIT;
	m_eSpecialUnit = NO_SPECIALUNIT;
	m_eSleepType = NO_MISSION;
	m_iZoneOfControlCount = 0;
	m_iExcileCount = 0;
	m_iPassageCount = 0;
	m_iNoNonOwnedCityEntryCount = 0;
	m_iBarbCoExistCount = 0;
	m_iBlendIntoCityCount = 0;
	m_iUpgradeAnywhereCount = 0;
	m_bAutoPromoting = false;
	m_bAutoUpgrading = false;
	m_iHiddenNationalityCount = 0;
	m_bHasAnyInvisibility = false;
	m_bRevealed = false;
	m_shadowUnit.reset();

	m_eOwner = eOwner;
	m_eCapturingPlayer = NO_PLAYER;
	m_eUnitType = eUnit;
	m_eReligionType = NO_RELIGION;
	m_pUnitInfo = (NO_UNIT != m_eUnitType) ? &GC.getUnitInfo(m_eUnitType) : NULL;
	// m_iBaseCombat100 is the unit's OWN base strength: seeded from the type here, then PER-UNIT STATE thereafter --
	// WorldBuilder edits it and the WBS scenario format persists it, so it is deliberately serialized and is NOT
	// re-derivable from the type alone (owner). The resolved plane carries the promotion/unit-combat DELTA only
	// and does not repeat this value (Cascade/CvUnitResolved.h).
	// ⚠ The ÷100 is a CLUSTER BOUNDARY, not a sanctioned shape: m_iBaseCombat100 is still a human whole number
	// here (it mixes with getExtraStrength and the percent stack below), so this reduces to meet it. It goes
	// when the combat cluster converts as a unit -- a scale conversion inside a calculation is the defect,
	// never the fix ([DEC-fixedpoint-x100]; fixed-point-and-scales.md § CONVERT BY ARITHMETIC CLUSTER).
	// ⚠ -1 (not 0) on the NO_UNIT path: CvUnit::read() calls reset() with NO_UNIT before draining the stream, so
	// this is also the pre-load value. 0 is a LEGITIMATE strength (non-combat units), so it cannot mark "unset";
	// read() re-seeds from the type when the save carried no value (see the m_iBaseCombat100 read).
	m_iBaseCombat100 = (NO_UNIT != m_eUnitType) ? m_pUnitInfo->getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) : -1;
	m_eLeaderUnitType = NO_UNIT;
	m_eGGExperienceEarnedTowardsType = NO_UNIT;
	m_iCargoCapacity = 0;
	m_iSMCargoCapacity = 0;
	m_iHealSupportUsed = 0;
	m_iExtraInsidiousness = 0;
	m_iExtraInvestigation = 0;
	m_iNoSelfHealCount = 0;
	m_iDebugCount = 0;
	m_iAssassinCount = 0;
	m_iStealthDefenseCount = 0;
	m_iOnlyDefensiveCount = 0;
	m_iNoInvisibilityCount = 0;
	m_iNoCaptureCount = 0;
	m_iExtraNoDefensiveBonusCount = 0;
	m_iExtraGatherHerdCount = 0;
	m_bIsArmed = false;
	// A CvUnit is RECYCLED out of an FFreeListTrashArray, so this must be cleared here or a new unit
	// inherits the previous occupant's suppression.
	m_bSuppressWithdrawal = false;
	m_eCurrentBuildUpType = NO_PROMOTIONLINE;

	m_eCapturingUnit.reset();
	m_combatUnit.reset();
	m_transportUnit.reset();

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		m_aiExtraDomainModifier[iI] = 0;
	}

	for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
	{
		m_aiExtraVisibilityIntensity[iI] = 0;
		m_aiExtraInvisibilityIntensity[iI] = 0;
		m_aiExtraVisibilityIntensityRange[iI] = 0;
		m_aiExtraVisibilityIntensitySameTile[iI] = 0;
		m_aiNegatesInvisibleCount[iI] = 0;
	}

	m_aExtraInvisibleTerrains.clear();
	m_aExtraInvisibleFeatures.clear();
	m_aExtraInvisibleImprovements.clear();
	m_aExtraVisibleTerrains.clear();
	m_aExtraVisibleFeatures.clear();
	m_aExtraVisibleImprovements.clear();
	m_aExtraVisibleTerrainRanges.clear();
	m_aExtraVisibleFeatureRanges.clear();
	m_aExtraVisibleImprovementRanges.clear();
	m_szName.clear();
	m_szScriptData = "";

	m_aExtraAidChanges.clear();

	if (!bConstructorCall)
	{
		FAssertMsg((0 < GC.getNumPromotionInfos()), "GC.getNumPromotionInfos() is not greater than zero but an array is being allocated in CvUnit::reset");

		m_promotionKeyedInfo.clear();
		m_promotionLineKeyedInfo.clear();
		m_terrainKeyedInfo.clear();
		m_featureKeyedInfo.clear();
		m_unitCombatKeyedInfo.clear();

		if (!bIdentityChange)
		{
			AI_reset(NO_UNITAI, true);
		}
	}
	m_pPlayerInvestigated = NO_PLAYER;
	m_Properties.clear();

	// Toffer - UnitComponents
	m_commander = NULL;
	m_commodore = NULL;
	m_worker = NULL;
}

CvUnit& CvUnit::operator=(const CvUnit& other)
{
	m_iHealUnitCombatCount = other.m_iHealUnitCombatCount;
	//m_iID = other.m_iID;
	//if (!bIdentityChange)
	//{
	//	m_iGroupID = other.m_iGroupID;
	//}
	m_iHotKeyNumber = other.m_iHotKeyNumber;
	//m_iX = other.m_iX;
	//m_iY = other.m_iY;
	m_iLastMoveTurn = other.m_iLastMoveTurn;
	m_iReconX = other.m_iReconX;
	m_iReconY = other.m_iReconY;
	m_iGameTurnCreated = other.m_iGameTurnCreated;
	m_iDamage = other.m_iDamage;
	m_iMoves = other.m_iMoves;
	m_iExperience = other.m_iExperience;
	m_iLevel = other.m_iLevel;
	m_iCargo = other.m_iCargo;
	m_iSMCargo = other.m_iSMCargo;
	m_iAttackPlotX = other.m_iAttackPlotX;
	m_iAttackPlotY = other.m_iAttackPlotY;
	m_iCombatTimer = other.m_iCombatTimer;
	m_iCombatFirstStrikes = other.m_iCombatFirstStrikes;
	m_iFortifyTurns = other.m_iFortifyTurns;
	m_iBuildUpTurns = other.m_iBuildUpTurns;
	m_iBlitzCount = other.m_iBlitzCount;
	m_iAmphibCount = other.m_iAmphibCount;
	m_iRiverCount = other.m_iRiverCount;
	m_iEnemyRouteCount = other.m_iEnemyRouteCount;
	m_iAlwaysHealCount = other.m_iAlwaysHealCount;
	m_iHillsDoubleMoveCount = other.m_iHillsDoubleMoveCount;
	m_iImmuneToFirstStrikesCount = other.m_iImmuneToFirstStrikesCount;
	m_iAlwaysInvisibleCount = other.m_iAlwaysInvisibleCount;
	m_iDefensiveVictoryMoveCount = other.m_iDefensiveVictoryMoveCount;
	m_iFreeDropCount = other.m_iFreeDropCount;
	m_iOffensiveVictoryMoveCount = other.m_iOffensiveVictoryMoveCount;
	m_iOneUpCount = other.m_iOneUpCount;
	m_iPillageCultureCount = other.m_iPillageCultureCount;
	m_iPillageEspionageCount = other.m_iPillageEspionageCount;
	m_iPillageMarauderCount = other.m_iPillageMarauderCount;
	m_iPillageOnMoveCount = other.m_iPillageOnMoveCount;
	m_iPillageOnVictoryCount = other.m_iPillageOnVictoryCount;
	m_iPillageResearchCount = other.m_iPillageResearchCount;
	m_iCelebrityHappy = other.m_iCelebrityHappy;
	m_iCollateralDamageLimitChange = other.m_iCollateralDamageLimitChange;
	m_iCollateralDamageMaxUnitsChange = other.m_iCollateralDamageMaxUnitsChange;
	m_iCombatLimitChange = other.m_iCombatLimitChange;
	m_iExtraDropRange = other.m_iExtraDropRange;
	m_iSurvivorChance = other.m_iSurvivorChance;
	m_iExtraMoves = other.m_iExtraMoves;
	m_iExtraMoveDiscount = other.m_iExtraMoveDiscount;
	m_iStampedeCount = other.m_iStampedeCount;
	m_iAttackOnlyCitiesCount = other.m_iAttackOnlyCitiesCount;
	m_iIgnoreNoEntryLevelCount = other.m_iIgnoreNoEntryLevelCount;
	m_iIgnoreZoneofControlCount = other.m_iIgnoreZoneofControlCount;
	m_iFliesToMoveCount = other.m_iFliesToMoveCount;
	m_iSMStrength = other.m_iSMStrength;
	m_iOnslaughtCount = other.m_iOnslaughtCount;
	m_iRetrainsAvailable = other.m_iRetrainsAvailable;
	m_iQualityBaseTotal = other.m_iQualityBaseTotal;
	m_iGroupBaseTotal = other.m_iGroupBaseTotal;
	m_iSizeBaseTotal = other.m_iSizeBaseTotal;
	m_iExtraQuality = other.m_iExtraQuality;
	m_iExtraGroup = other.m_iExtraGroup;
	m_iExtraSize = other.m_iExtraSize;
	m_iSMCargoVolume = other.m_iSMCargoVolume;
	m_iSMExtraCargoVolume = other.m_iSMExtraCargoVolume;
	m_iSMCargoVolumeModifier = other.m_iSMCargoVolumeModifier;
	m_iCannotMergeSplitCount = other.m_iCannotMergeSplitCount;
	m_iExtraBreakdownChance = other.m_iExtraBreakdownChance;
	m_iExtraBreakdownDamage = other.m_iExtraBreakdownDamage;
	m_iExtraCombatModifierPerSizeMore = other.m_iExtraCombatModifierPerSizeMore;
	m_iExtraCombatModifierPerSizeLess = other.m_iExtraCombatModifierPerSizeLess;
	m_iExtraCombatModifierPerVolumeMore = other.m_iExtraCombatModifierPerVolumeMore;
	m_iExtraCombatModifierPerVolumeLess = other.m_iExtraCombatModifierPerVolumeLess;
	m_iExtraMaxHP = other.m_iExtraMaxHP;
	m_iSMAssetValue = other.m_iSMAssetValue;
	m_iSMPowerValue = other.m_iSMPowerValue;
	m_iSMHPValue = other.m_iSMHPValue;
	m_iExtraBombardRate = other.m_iExtraBombardRate;
	m_iSMBombardRate = other.m_iSMBombardRate;
	m_iSMAirBombBaseRate = other.m_iSMAirBombBaseRate;
	m_iSMBaseWorkRate = other.m_iSMBaseWorkRate;
	m_iSMRevoltProtection = other.m_iSMRevoltProtection;
	m_iRevoltProtection = other.m_iRevoltProtection;
	m_iCollateralDamageProtection = other.m_iCollateralDamageProtection;
	m_iPillageChange = other.m_iPillageChange;
	m_iUpgradeDiscount = other.m_iUpgradeDiscount;
	m_iExperiencePercent = other.m_iExperiencePercent;
	m_iKamikazePercent = other.m_iKamikazePercent;
	m_eFacingDirection = other.m_eFacingDirection;
	for (int iStatus = 0; iStatus < NUM_UNIT_STATUSES; ++iStatus)
	{
		m_aiStatusTurns[iStatus] = other.m_aiStatusTurns[iStatus];
	}
	m_bCanRespawn = other.m_bCanRespawn;
	m_bSurvivor = other.m_bSurvivor;
	m_bMadeAttack = other.m_bMadeAttack;
	m_iRoundCount = other.m_iRoundCount;
	m_iAttackCount = other.m_iAttackCount;
	m_iDefenseCount = other.m_iDefenseCount;
	m_bMadeInterception = other.m_bMadeInterception;
	m_bPromotionReady = other.m_bPromotionReady;
	m_bDeathDelay = other.m_bDeathDelay;
	m_bInfoBarDirty = other.m_bInfoBarDirty;
	m_bBlockading = other.m_bBlockading;
	m_bAirCombat = other.m_bAirCombat;
	m_bHasBuildUp = other.m_bHasBuildUp;
	m_bInhibitMerge = other.m_bInhibitMerge;
	m_bInhibitSplit = other.m_bInhibitSplit;
	m_bIsBuildUp = other.m_bIsBuildUp;
	m_bIsReligionLocked = other.m_bIsReligionLocked;
	m_iCanMovePeaksCount = other.m_iCanMovePeaksCount;
	m_iCanLeadThroughPeaksCount = other.m_iCanLeadThroughPeaksCount;
	m_movementCharacteristicsHash = other.m_movementCharacteristicsHash;
	m_iSleepTimer = other.m_iSleepTimer;
	m_iCommanderID = other.m_iCommanderID;
	m_iCommodoreID = other.m_iCommodoreID;
	m_iUsedCommanderID = other.m_iUsedCommanderID;
	m_iUsedCommodoreID = other.m_iUsedCommodoreID;
	m_eOriginalOwner = other.m_eOriginalOwner;
	m_eNewDomainCargo = other.m_eNewDomainCargo;
	m_eNewSpecialCargo = other.m_eNewSpecialCargo;
	m_eNewSMNotSpecialCargo = other.m_eNewSMNotSpecialCargo;
	m_eSpecialUnit = other.m_eSpecialUnit;
	m_eSleepType = other.m_eSleepType;
	m_iZoneOfControlCount = other.m_iZoneOfControlCount;
	m_iExcileCount = other.m_iExcileCount;
	m_iPassageCount = other.m_iPassageCount;
	m_iNoNonOwnedCityEntryCount = other.m_iNoNonOwnedCityEntryCount;
	m_iBarbCoExistCount = other.m_iBarbCoExistCount;
	m_iBlendIntoCityCount = other.m_iBlendIntoCityCount;
	m_iUpgradeAnywhereCount = other.m_iUpgradeAnywhereCount;
	m_bAutoPromoting = other.m_bAutoPromoting;
	m_bAutoUpgrading = other.m_bAutoUpgrading;
	m_iHiddenNationalityCount = other.m_iHiddenNationalityCount;
	m_bHasAnyInvisibility = other.m_bHasAnyInvisibility;
	m_bRevealed = other.m_bRevealed;
	m_shadowUnit = other.m_shadowUnit;
	m_eOwner = other.m_eOwner;
	m_eCapturingPlayer = other.m_eCapturingPlayer;
	m_eUnitType = other.m_eUnitType;
	m_eReligionType = other.m_eReligionType;
	m_pUnitInfo = other.m_pUnitInfo;
	m_iBaseCombat100 = other.m_iBaseCombat100;
	m_eLeaderUnitType = other.m_eLeaderUnitType;
	m_eGGExperienceEarnedTowardsType = other.m_eGGExperienceEarnedTowardsType;
	m_iCargoCapacity = other.m_iCargoCapacity;
	m_iSMCargoCapacity = other.m_iSMCargoCapacity;
	m_iHealSupportUsed = other.m_iHealSupportUsed;
	m_iExtraInsidiousness = other.m_iExtraInsidiousness;
	m_iExtraInvestigation = other.m_iExtraInvestigation;
	m_iNoSelfHealCount = other.m_iNoSelfHealCount;
	m_iDebugCount = other.m_iDebugCount;
	m_iAssassinCount = other.m_iAssassinCount;
	m_iStealthDefenseCount = other.m_iStealthDefenseCount;
	m_iOnlyDefensiveCount = other.m_iOnlyDefensiveCount;
	m_iNoInvisibilityCount = other.m_iNoInvisibilityCount;
	m_iNoCaptureCount = other.m_iNoCaptureCount;
	m_iExtraNoDefensiveBonusCount = other.m_iExtraNoDefensiveBonusCount;
	m_iExtraGatherHerdCount = other.m_iExtraGatherHerdCount;
	m_bIsArmed = other.m_bIsArmed;
	m_bSuppressWithdrawal = false;   // within-frame state, never carried across units
	m_eCurrentBuildUpType = other.m_eCurrentBuildUpType;
	m_eCapturingUnit = other.m_eCapturingUnit;
	m_combatUnit = other.m_combatUnit;
	m_transportUnit = other.m_transportUnit;

	m_aExtraInvisibleTerrains = other.m_aExtraInvisibleTerrains;
	m_aExtraInvisibleFeatures = other.m_aExtraInvisibleFeatures;
	m_aExtraInvisibleImprovements = other.m_aExtraInvisibleImprovements;
	m_aExtraVisibleTerrains = other.m_aExtraVisibleTerrains;
	m_aExtraVisibleFeatures = other.m_aExtraVisibleFeatures;
	m_aExtraVisibleImprovements = other.m_aExtraVisibleImprovements;
	m_aExtraVisibleTerrainRanges = other.m_aExtraVisibleTerrainRanges;
	m_aExtraVisibleFeatureRanges = other.m_aExtraVisibleFeatureRanges;
	m_aExtraVisibleImprovementRanges = other.m_aExtraVisibleImprovementRanges;
	m_szName = other.m_szName;
	m_szScriptData = other.m_szScriptData;
	m_aExtraAidChanges = other.m_aExtraAidChanges;

	if (!other.m_promotionKeyedInfo.empty())
		m_promotionKeyedInfo = other.m_promotionKeyedInfo;

	if (!other.m_promotionLineKeyedInfo.empty())
		m_promotionLineKeyedInfo = other.m_promotionLineKeyedInfo;

	if (!other.m_terrainKeyedInfo.empty())
		m_terrainKeyedInfo = other.m_terrainKeyedInfo;

	if (!other.m_featureKeyedInfo.empty())
		m_featureKeyedInfo = other.m_featureKeyedInfo;

	if (!other.m_unitCombatKeyedInfo.empty())
		m_unitCombatKeyedInfo = other.m_unitCombatKeyedInfo;

	m_pPlayerInvestigated = other.m_pPlayerInvestigated;
	m_Properties = other.m_Properties;

	if (other.m_commander)
	{
		SAFE_DELETE(m_commander);
		m_commander = new UnitCompCommander(this, m_pUnitInfo);
		*m_commander = *other.m_commander;
	}
	if (other.m_commodore)
    {
    	SAFE_DELETE(m_commodore);
    	m_commodore = new UnitCompCommodore(this, m_pUnitInfo);
    	*m_commodore = *other.m_commodore;
    }
	if (other.m_worker)
	{
		SAFE_DELETE(m_worker);
		m_worker = new UnitCompWorker();
		*m_worker = *other.m_worker;
	}

	return *this;
}

//////////////////////////////////////
// graphical only setup
//////////////////////////////////////
void CvUnit::setupGraphical()
{
	PROFILE_FUNC();

	if (!GC.IsGraphicsInitialized() || !isInViewport())
	{
		return;
	}

	if (!isUsingDummyEntities())
	{
		CvDLLEntity::setup();
	}

	if (getGroup()->getActivityType() == ACTIVITY_INTERCEPT)
	{
		airCircle(true);
	}
	else
	{
		/* billw - This forces multi-unit graphics to update.
			If it isn't done then only 1 unit shows up, then the rest appear 10s or more later.
			I tried every other command on the CvDLLEntityIFaceBase to trigger update
			of these graphics (I didn't test every animation and mission type though),
			but only found this one that actually works.
		*/
		ExecuteMove(0, false);

		/* TEST CODE (billw 21/9/2019) >>>>>>
		// Anyone can remove this later if no problems show up with using ExecuteMode(0, false) above

		static int mode = 1;
		switch (mode)
		{
			case 0: ExecuteMove(0, false); break;
			case 1: SetPosition(plot()); break;
			case 2: {
				static AnimationTypes eAnim = NONE_ANIMATION;
				static float fSpeed = 1.0f;
				static bool bQueue = false;
				static int iLayer = 0;
				static float fStartPct = 0.0f;
				static float fEndPct = 1.0f;
				PlayAnimation(eAnim, fSpeed, bQueue, iLayer, fStartPct, fEndPct);
				break;
			};
			case 3: setVisible(true); break;
			case 4: setVisible(false); setVisible(true); break;
			case 5: gDLL->getEntityIFace()->updatePosition(getEntity()); break;
			case 6: MoveTo(plot()); break;
			case 7: QueueMove(plot()); break;
			case 8: {
				static MissionTypes eMission = NO_MISSION;
				NotifyEntity(eMission);
				break;
			}
			case 9: gDLL->getEntityIFace()->updateGraphicEra(getUnitEntity()); break;
			case 10: gDLL->getEntityIFace()->showPromotionGlow(getUnitEntity(), true); break;
			case 11: gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity()); break;
			case 12: gDLL->getEntityIFace()->updatePromotionLayers(getUnitEntity()); break;
			case 13: gDLL->getEntityIFace()->StopAnimation(getEntity()); break;
			default: break;
		};
		<<<<< TEST CODE */
	}
}


// Toffer - 04.04.20
// bKillOriginal is only used by worldbuilder at this time, when duplicating unit and changing unit owner.
// Reason is that delayed death does not happen before exiting worldbuilder, and
// it's messy to have a bunch of units on the map marked for death with no idea which ones that are marked.
// Also reduce the amount of code needed to process to duplicate a unit, as it doesn't have to call convert twice when keeping the original.
void CvUnit::convert(CvUnit* pUnit, const bool bKillOriginal)
{
	PROFILE_FUNC();

	setFortifyTurns(0);

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		int iTotalGroupOffset = 0;
		int iTotalQualityOffset = 0;

		for (int iI = GC.getNumPromotionInfos() - 1; iI > -1; iI--)
		{
			const PromotionTypes ePromoX = static_cast<PromotionTypes>(iI);
			const CvPromotionInfo& kPromo = GC.getPromotionInfo(ePromoX);
			PromotionLineTypes eLine = kPromo.getPromotionLine();
			bool bIsBuildup = false;
			if (eLine != NO_PROMOTIONLINE)
			{
				const CvPromotionLineInfo& kLine = GC.getPromotionLineInfo(eLine);
				bIsBuildup = kLine.isBuildUp();
			}
			if (pUnit->isHasPromotion(ePromoX) && !bIsBuildup)
			{
				if (GC.getPromotionInfo(ePromoX).getSizeMatters().group != 0)
				{
					iTotalGroupOffset += GC.getPromotionInfo(ePromoX).getSizeMatters().group;
				}
				else if (GC.getPromotionInfo(ePromoX).getSizeMatters().quality != 0)
				{
					iTotalQualityOffset += GC.getPromotionInfo(ePromoX).getSizeMatters().quality;
				}
				else if (!isHasPromotion(ePromoX)) //see note below on this situation with true for bDying
				{
					setHasPromotion(ePromoX, true, pUnit->isPromotionFree(ePromoX), true);
				}
			}
		}

		const bool bNormalizedGroup = CvUnit::normalizeUnitPromotions(this, iTotalGroupOffset,
			bind(&CvUnit::isGroupUpgradePromotion, this, _2),
			bind(&CvUnit::isGroupDowngradePromotion, this, _2)
		);
		FAssertMsg(bNormalizedGroup, "Could not apply required number of group promotions on converted unit");

		const bool bNormalizedQuality = CvUnit::normalizeUnitPromotions(this, iTotalQualityOffset,
			bind(&CvUnit::isQualityUpgradePromotion, this, _2),
			bind(&CvUnit::isQualityDowngradePromotion, this, _2)
		);
		FAssertMsg(bNormalizedQuality, "Could not apply required number of quality promotions on converted unit");
	}
	else
	{
		for (int iI = GC.getNumPromotionInfos() - 1; iI > -1; iI--)
		{
			const PromotionTypes ePromoX = static_cast<PromotionTypes>(iI);

			if (pUnit->isHasPromotion(ePromoX) && !isHasPromotion(ePromoX))
			{
				// TB - bDying is set to true to temporarily avoid obsoletion checks until AFTER all promos are assigned
				// as sometimes promos would be lost because prereqs simply weren't assigned yet due to the order in which they were established.
				setHasPromotion(ePromoX, true, pUnit->isPromotionFree(ePromoX), true);
			}
		}
	}
	//TB Combat Mod end

	if (pUnit->getCityOfOrigin() != NULL)
	{
		setCityOfOrigin(pUnit->getCityOfOrigin());
	}
	setGameTurnCreated(pUnit->getGameTurnCreated());

	const int iCurrentHPCap = pUnit->getMaxHP()-1;
	setDamage(std::min(iCurrentHPCap, pUnit->getDamage()));
	setMoves(pUnit->getMoves());
	for (int iStatus = 0; iStatus < NUM_UNIT_STATUSES; ++iStatus)
	{
		setStatus((UnitStatus)iStatus, pUnit->getStatus((UnitStatus)iStatus));
	}

	m_eOriginalOwner = pUnit->getOriginalOwner();
	m_eNewDomainCargo = pUnit->getDomainCargo();
	m_eNewSpecialCargo = pUnit->getSpecialCargo();
	m_eNewSMNotSpecialCargo = pUnit->getSMNotSpecialCargo();
	m_eSpecialUnit = pUnit->getSpecialUnitType();
	m_eSleepType = NO_MISSION;
	m_iHiddenNationalityCount = pUnit->getHiddenNationalityCount();
	setAutoPromoting(pUnit->isAutoPromoting());
	setAutoUpgrading(pUnit->isAutoUpgrading());
	m_eCurrentBuildUpType = NO_PROMOTIONLINE;

	setLevel(pUnit->getLevel());
	//	Carrying XP across owners rescales it by each side's own level modifier, so both empires are asked.
	int aiOldExperience[NUM_EXPERIENCE_KINDS];
	int aiOurExperience[NUM_EXPERIENCE_KINDS];
	GET_PLAYER(pUnit->getOwner()).getExperienceKinds(aiOldExperience);
	GET_PLAYER(getOwner()).getExperienceKinds(aiOurExperience);

	const int iOldModifier = std::max(1, 100 + aiOldExperience[EXPERIENCE_LEVEL_MODIFIER]);
	const int iOurModifier = std::max(1, 100 + aiOurExperience[EXPERIENCE_LEVEL_MODIFIER]);
	setExperience(std::max(0, pUnit->getExperience() * iOurModifier / iOldModifier));

	setName(pUnit->getNameNoDesc());

	if (pUnit->isDescInName() && getBugOptionBOOL("MiscHover__UpdateUnitNameOnUpgrade", true, "BUG_UPDATE_UNIT_NAME_ON_UPGRADE"))
	{
		CvWString szUnitType(pUnit->getDescription());

		m_szName.replace(m_szName.find(szUnitType), szUnitType.length(), getDescription());
	}

	if (pUnit->getLeaderUnitType() != NO_UNIT)
	{
		setLeaderUnitType(pUnit->getLeaderUnitType());
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		setHasAnyInvisibility();
	}

	CvUnit* pTransportUnit = pUnit->getTransportUnit();
	if (pTransportUnit != NULL)
	{
		pUnit->setTransportUnit(NULL);
		setTransportUnit(pTransportUnit);
	}

	std::vector<CvUnit*> aCargoUnits;
	pUnit->getCargoUnits(aCargoUnits);
	pUnit->validateCargoUnits();
	foreach_(CvUnit* pCargo, aCargoUnits)
	{
		// Check cargo types and capacity when upgrading transports
		if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
		{
			if (cargoSpaceAvailable(pCargo->getSpecialUnitType(), pCargo->getDomainType()) > pCargo->getCargoVolume())
			{
				pCargo->setTransportUnit(NULL);
				pCargo->setTransportUnit(this);
			}
			else
			{
				pCargo->setTransportUnit(NULL);
				pCargo->jumpToNearestValidPlot();
			}
		}
		else if (cargoSpaceAvailable(pCargo->getSpecialUnitType(), pCargo->getDomainType()) > 0)
		{
			pCargo->setTransportUnit(NULL);
			pCargo->setTransportUnit(this);
		}
		else
		{
			pCargo->setTransportUnit(NULL);
			pCargo->jumpToNearestValidPlot();
		}
	}
	validateCargoUnits();

	if (bKillOriginal)
	{
		pUnit->getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
		pUnit->kill(true);
	}
}


//	THE DISPATCHER every caller uses. It owns ONE decision -- the RECURSION BRAKE -- and delegates the rest.
//	The brake is load-bearing, not defensive: both survival outcomes call setDamage while the death is still
//	scheduled, and setDamage re-enters kill(true) whenever isDead() reports true (which isDelayedDeath() alone
//	makes true). Without the brake that is unbounded recursion.
void CvUnit::kill(bool bDelay, PlayerTypes ePlayer, bool bMessaged)
{
	if (bDelay && m_bDeathDelay)
	{
		return;
	}
	killUnconditional(bDelay, ePlayer, bMessaged);
}

//	Past the brake: run the pre-death sequence, then either hand the unit to the delayed-death pass or resolve
//	its fate now.
void CvUnit::killUnconditional(bool bDelay, PlayerTypes ePlayer, bool bMessaged)
{
	PROFILE_FUNC();

	if (scheduleDeath(bDelay, ePlayer, bMessaged))
	{
		return;
	}
	resolveScheduledDeath();
}

//	Every effect a death has on the world BEFORE its outcome is known -- deselection, worker-claim release,
//	the cargo's fate, the death report to Python and the players, the worker's city assignment. These run for
//	a unit that goes on to survive, because the engine treats them as consequences of the KILLING BLOW rather
//	than of the death.
//	NEVER kills. Returns true when the death has been DEFERRED to the delayed-death pass, false when the
//	caller must resolve it in this call.
bool CvUnit::scheduleDeath(bool bDelay, PlayerTypes ePlayer, bool bMessaged)
{
	deselect(!bDelay);

	/* Toffer - Evaluate if double messaging actually take place...
	if (m_combatResult.bDeathMessaged)
	{
		bMessaged = true;
	}
	*/
	const PlayerTypes eOwner = getOwner();
	CvPlayerAI& owner = GET_PLAYER(eOwner);

	CvPlot* pPlot = plot();

	if (pPlot)
	{
		if (hasCargo())
		{
			foreach_(CvUnit* unitX, pPlot->units())
			{
				if (unitX == this || unitX->getTransportUnit() != this)
				{
					continue;
				}
				if (unitX->isDelayedDeath())
				{
					// Should mean that we are on the second kill pass of transporting unit (this),
					// i.e. this cargo unit (unitX) already failed to survive on the first kill pass where bDelay=True.
					FAssertMsg(!bDelay, "bDelay should in theory always be false here... I think");
					unitX->kill(bDelay, NO_PLAYER, true);
					continue;
				}

				/* Toffer ToDo - revise
				if (getCaptureUnitType() != NO_UNIT && getCapturingPlayer() != NO_PLAYER
				&& unitX->getCaptureUnitType() != NO_UNIT && !GET_PLAYER(getCapturingPlayer()).isNPC()
				&& GC.getGame().getSorenRandNum(2, "50% prefer safe capture over deadly escape.") == 0)
				{
					unitX->setCapturingPlayer(getCapturingPlayer());
					unitX->setCapturingUnit(getCapturingUnit());
					unitX->kill(bDelay, ePlayer, bMessaged);
					continue;
				}
				*/
				if (GC.getGame().getSorenRandNum(pPlot->isWater() ? 5 : 2, "Unit Survives Drowning") == 0)
				{
					bool bSurvived = false;
					std::vector<const CvPlot*> validPlots;

					foreach_(const CvPlot* pAdjacentPlot, plot()->adjacent())
					{
						if (unitX->canMoveThrough(pAdjacentPlot, false))
						{
							validPlots.push_back(pAdjacentPlot);
							bSurvived = true;
						}
					}
					if (bSurvived)
					{
						const CvPlot* rescuePlot = validPlots[GC.getGame().getSorenRandNum(validPlots.size(), "Event pick plot")];

						FAssertMsg(rescuePlot, "rescuePlot is expected to be a valid plot!");
						unitX->setXY(rescuePlot->getX(), rescuePlot->getY());
						unitX->setDamage(GC.getGame().getSorenRandNum(std::min(unitX->getMaxHP() * 2/3, unitX->getHP()), "Survival Damage"), NO_PLAYER);
						AddDLLMessage(
							unitX->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_UNIT_SURVIVED_TRANSPORT_SINKING", unitX->getNameKey(), getNameKey()),
							NULL, MESSAGE_TYPE_MINOR_EVENT
						);
						continue;
					}
				}
				AddDLLMessage(
					eOwner, true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText("TXT_KEY_MISC_UNIT_DROWNED", unitX->getNameKey()),
					GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(),
					MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), getX(), getY()
				);
				unitX->kill(bDelay, ePlayer, true);
			}
		}

		if (ePlayer != NO_PLAYER)
		{
			CvEventReporter::getInstance().unitKilled(this, ePlayer);

			if (NO_UNIT != getLeaderUnitType() || GC.getUnitInfo(getUnitType()).getAllowed()->cap(ALLOWEDCAP_WORLD) == 1)
			{
				for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
				{
					if (GET_PLAYER((PlayerTypes)iI).isAlive())
					{
						AddDLLMessage(
							(PlayerTypes)iI, true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_GENERAL_KILLED", getNameKey()),
							GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(),
							MESSAGE_TYPE_MAJOR_EVENT, NULL, GC.getCOLOR_RED(), getX(), getY()
						);
					}
				}
			}
		}

		if (isWorker())
		{
			CvCity* city = owner.getCity(m_worker->getAssignedCity());
			if (city)
			{
				OutputDebugString(CvString::format("Worker at (%d,%d) killed with mission for city %S\n", getX(), getY(), city->getName().GetCString()).c_str());
				city->setWorkerHave(getID(), false);
			}
		}

		if (bDelay)
		{
			setDeathDelayInternal(true);
			return true;
		}
	}
	// An off-map unit is never scheduled. The delayed-death pass reaps units through the selection groups
	// standing on the map, so a deferral it cannot see would strand the unit alive forever.
	return false;
}

//	THE REAPER. Asks the survival question once and dispatches to exactly one outcome. It decides; it does not
//	perform -- each outcome below is a whole operation named for what it does to the unit.
void CvUnit::resolveScheduledDeath()
{
	// Both survival outcomes are asked only of a unit still on the map: an off-map unit has no position to
	// evacuate from, and the survivor roll belongs to a battle fought on a plot.
	if (plot() != NULL)
	{
		// Both outcomes sit under the capital: the evacuation needs it as a destination, and the survivor
		// roll shares that nesting, so a player without a capital has neither available.
		const CvCity* pCapitalCity = GET_PLAYER(getOwner()).getCapitalCity();
		if (pCapitalCity != NULL)
		{
			if (isCanRespawn() && pCapitalCity->plot() != plot())
			{
				evacuateToCapital(*pCapitalCity);
				return;
			}
			if (isSurvivor())
			{
				surviveLastStand();
				return;
			}
		}
	}
	die();
}

//	NOT a death: a relocation plus a damage set. The unit leaves the battlefield and reappears at the capital,
//	so nothing announces it dead.
void CvUnit::evacuateToCapital(const CvCity& kCapitalCity)
{
	//GC.getGame().logOOSSpecial(14, getID(), kCapitalCity.getX(), kCapitalCity.getY());
	setXY(kCapitalCity.getX(), kCapitalCity.getY(), false, false, false);
	// The death is still scheduled here, so isDead() is true and setDamage re-enters kill(true). The
	// dispatcher's brake is what absorbs that.
	setDamage(getMaxHP() * 9/10);
	changeOneUpCount(-1);
	AddDLLMessage(
		getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
		gDLL->getText("TXT_KEY_MISC_BATTLEFIELD_EVAC", getNameKey()),
		"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY()
	);
	setDeathDelayInternal(false);
}

//	NOT a death: a damage set. The unit is left one hit from dying and stays exactly where it stood.
void CvUnit::surviveLastStand()
{
	// As in evacuateToCapital, this setDamage re-enters kill(true) against a still-scheduled death.
	setDamage(getMaxHP() - std::max(1,(getSurvivorChance() / 1000)));
	AddDLLMessage(
		getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
		gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_IS_HARDCORE", getNameKey()),
		"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY()
	);
	setDeathDelayInternal(false);
	//	Only applies to THIS combat - it might be attacked again the same turn
	setSurvivor(false);
}

//	⛔ THE ONE TERMINAL -- the only function that ends a unit's life, and the only caller of emitUnitKilled. It
//	takes NO arguments and asks NO survival question: whatever reaches here dies. Two invariants make that
//	structural rather than a promise, and nothing may be added that breaks either:
//	  - the death fact is emitted on the FIRST line, so no outcome added later can slip in ahead of it;
//	  - owner.deleteUnit is the LAST line and is unconditional, so the unit is always removed.
//	A new outcome that leaves a unit alive belongs in resolveScheduledDeath, NEVER as an early return here.
//	The plot guards below govern what the unit is DETACHED FROM, never whether it dies: a unit holding no plot
//	has nothing to detach, and still dies.
void CvUnit::die()
{
	CvPlot* pDeathPlot = plot();
	const PlayerTypes eOwner = getOwner();
	CvPlayerAI& owner = GET_PLAYER(eOwner);

	emitUnitKilled((int)getUnitType(), getID(), (int)eOwner,
		(pDeathPlot != NULL) ? GC.getMap().plotNum(pDeathPlot->getX(), pDeathPlot->getY()) : -1);

	if (pDeathPlot != NULL)
	{
		if (isMadeAttack() && nukeRange() != -1)
		{
			CvPlot* pTarget = getAttackPlot();
			if (pTarget)
			{
				pTarget->nukeExplosion(nukeRange(), this);
				setAttackPlot(NULL, false);
			}
		}
		finishMoves();
		m_iDamage = getMaxHP(); // Toffer - Makes isDead() True

		// XXX this is NOT a hack, without it, the game crashes.
		if (!isUsingDummyEntities() && isInViewport())
		{
			gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
		}

		FAssertMsg(!isCombat(), "isCombat did not return false as expected");

		if (getTransportUnit())
		{
			setTransportUnit(NULL);
		}
		setReconPlot(NULL);
		setBlockading(false);
		/*
		if (isZoneOfControl())
		{
			foreach_(CvPlot* pAdjacentPlot, plot()->adjacent())
			{
				pAdjacentPlot->clearZoneOfControlCache();
			}
		}
		*/
		FAssertMsg(!getAttackPlot(), "The current unit instance's attack plot is expected to be NULL");
		FAssertMsg(!getCombatUnit(), "The current unit instance's combat unit is expected to be NULL");
	}

	// The owner's upkeep total is a Σ over its live units, so a death MARKS it rather than pushing a delta.
	owner.setUnitUpkeepDirty();

	owner.changeUnitCount(m_eUnitType, -1);
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS)
	// if unit doesn't have a group rank, it doesn't count as a SM unit at all
	&& groupRank() > 0)
	{
		owner.changeUnitCountSM(m_eUnitType, -smGroupMultiplier(groupRank()));
	}

	if (m_pUnitInfo->getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) > 0)
	{
		owner.changeNumNukeUnits(-1);
	}

	if (isMilitaryBranch())
	{
		owner.changeNumMilitaryUnits(-1);
	}

	owner.changeAssets(-assetValueTotal());
	owner.changeUnitPower(-getPowerValueTotal());

	if (pDeathPlot != NULL)
	{
		OutputDebugString(CvString::format("Unit %S of player %S killed\n", getName().GetCString(), owner.getCivilizationDescription(0)).c_str());

		owner.AI_changeNumAIUnits(AI_getUnitAIType(), -1);
		owner.AI_changeEffNumAIUnitsTimes100(AI_getUnitAIType(), -SMeffectiveCount());
		AI_killed(); // Update AI counts for this unit

		setCommander(false);
		setCommodore(false);
		// ⚠ From this statement until deleteUnit the unit is LIVE but off the map: it is gone from the plot's
		// unit list and plot() reports NULL, while CvPlayer::units() still yields it. Anything that re-enters
		// the death path on it inside that window takes die()'s off-map route, which deletes.
		setXY(INVALID_PLOT_COORD, INVALID_PLOT_COORD, true);
	}

	// Group membership is not a map position, so an off-map unit leaves its group the same way.
	joinGroup(NULL, false, false);

	if (pDeathPlot != NULL)
	{
		const PlayerTypes eCapturingPlayer = getCapturingPlayer();
		const UnitTypes eCaptureUnitType = getCaptureUnitType();

		if (eCapturingPlayer != NO_PLAYER && eCaptureUnitType != NO_UNIT && !GET_PLAYER(eCapturingPlayer).isNPC())
		{
			CvUnit* pkCapturedUnit = (
				GET_PLAYER(eCapturingPlayer).initUnit(
					eCaptureUnitType, pDeathPlot->getX(), pDeathPlot->getY(),
					NO_UNITAI, NO_DIRECTION,
					GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark")
				)
			);
			if (pkCapturedUnit)
			{
				CvEventReporter::getInstance().unitCaptured(eOwner, getUnitType(), pkCapturedUnit);

				AddDLLMessage(
					eCapturingPlayer, true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText("TXT_KEY_MISC_YOU_CAPTURED_UNIT", GC.getUnitInfo(eCaptureUnitType).getTextKeyWide()),
					"AS2D_UNITCAPTURE", MESSAGE_TYPE_INFO, pkCapturedUnit->getButton(), GC.getCOLOR_GREEN(), pDeathPlot->getX(), pDeathPlot->getY()
				);
				// Add a captured mission
				addMission(CvMissionDefinition(MISSION_CAPTURED, pDeathPlot, pkCapturedUnit));

				pkCapturedUnit->finishMoves();

				if (!GET_PLAYER(eCapturingPlayer).isHumanPlayer())
				{
					const CvPlot* pCapturedUnitPlot = pkCapturedUnit->plot();
					if (pCapturedUnitPlot && !pCapturedUnitPlot->isCity(false)
					&& GC.getDefineINT("AI_CAN_DISBAND_UNITS") && GET_PLAYER(eCapturingPlayer).AI_getPlotDanger(pCapturedUnitPlot))
					{
						pkCapturedUnit->kill(false, NO_PLAYER, true);
					}
				}
			}
		}
	}

	owner.deleteUnit(getID());
}


void CvUnit::NotifyEntity(MissionTypes eMission)
{
	if ( !isUsingDummyEntities() && isInViewport() )
	{
		gDLL->getEntityIFace()->NotifyEntity(getUnitEntity(), eMission);
	}
}


void CvUnit::doCityPassiveExperience()
{
    CvPlot* pPlot = plot();
    if (pPlot == NULL)
        return;

    CvCity* pCity = pPlot->getPlotCity();
    if (pCity == NULL)
        return;

    if (pCity->getOwner() != getOwner())
        return;

    for (int iPromotion = 0; iPromotion < GC.getNumPromotionInfos(); iPromotion++)
    {
        if (!isHasPromotion((PromotionTypes)iPromotion))
            continue;

        PromotionLineTypes eLine =
            GC.getPromotionInfo((PromotionTypes)iPromotion).getPromotionLine();

        if (eLine == GC.getInfoTypeForString("PROMOTIONLINE_BUILD_UP_TEACH"))
        {
            changeExperience100(5);
            return;
        }

        if (eLine == GC.getInfoTypeForString("PROMOTIONLINE_BUILD_UP_DISEASE_CONTROL"))
        {
            changeExperience100(5);
            return;
        }
    }
}


void CvUnit::doTurn()
{
	PROFILE("CvUnit::doTurn()");

	FAssertMsg(!isDead(), "isDead did not return false as expected");
	FAssertMsg(getGroup() != NULL, "getGroup() is not expected to be equal with NULL");

	if (isCommander())
	{
		m_commander->restoreControlPoints();
	}
    if (isCommodore())
	{
		m_commodore->restoreControlPoints();
	}
	gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);

	m_bRevealed = false;

	if (getInsidiousnessTotal(true) > 0)
	{
		if (plot()->isCity(false))
		{
			doInsidiousnessVSInvestigationCheck();
		}
		else if (m_pPlayerInvestigated != NO_PLAYER)
		{
			doRemoveInvestigatedPromotionCheck();
		}
	}


	// Apply 10% damage per turn for forced march or quick march status
    if (isHasPromotion((PromotionTypes)GC.getInfoTypeForString("PROMOTION_QUICK_MARCH_STATUS_HS_SM"))
    	|| isHasPromotion((PromotionTypes)GC.getInfoTypeForString("PROMOTION_QUICK_MARCH_STATUS"))
    	|| isHasPromotion((PromotionTypes)GC.getInfoTypeForString("PROMOTION_QUICK_MARCH_STATUS_HS")))
    {
    	changeDamagePercent(10, NO_PLAYER);
    }

	testPromotionReady();
	if (isBlockading())
	{
		collectBlockadeGold();
	}

	if (isSpy() && isIntruding() && !isCargo())
	{
		const TeamTypes eTeam = plot()->getTeam();

		if (NO_TEAM != eTeam)
		{
			if (GET_TEAM(getTeam()).isOpenBorders(eTeam))
			{
				testSpyIntercepted(plot()->getOwner(), GC.getDefineINT("ESPIONAGE_SPY_NO_INTRUDE_INTERCEPT_MOD"));
			}
			else
			{
				testSpyIntercepted(plot()->getOwner(), GC.getDefineINT("ESPIONAGE_SPY_INTERCEPT_MOD"));
			}
		}
	}

    // function that calculates passive xp in cities for units that provide buildups
	doCityPassiveExperience();

	const bool bHasMoved = hasMoved();
	const bool bHeal = ((bHasMoved && isAlwaysHeal()) || !bHasMoved);


	if (bHeal && isHurt())
	{
		doHeal();
	}

	if (!bHasMoved)
	{
		setFortifyTurns(getFortifyTurns() + 1);

		if (isBuildUp())
		{
			incrementBuildUp();
		}
	}

	if (isCanRespawn())
	{
		setCanRespawn(false);
	}

	if (isSurvivor())
	{
		setSurvivor(false);
	}

	if (isSpy() && m_iSleepTimer > 0 && getFortifyTurns() == GC.getMAX_FORTIFY_TURNS())
	{
		getGroup()->setActivityType(ACTIVITY_AWAKE);
		m_iSleepTimer = 0;
	}

	doStatusTurn();
	//TB Combat Mods (Att&DefCounter)
	if (getAttackCount()>0)
	{
		int AttackCountResetVal = -(getAttackCount());
		changeAttackCount(AttackCountResetVal);
	}
	if (getDefenseCount()>0)
	{
		int DefenseCountResetVal = -(getDefenseCount());
		changeDefenseCount(DefenseCountResetVal);
	}
	//TB Combat Mods (Att&DefCounter) end

	setMadeAttack(false);
	setMadeInterception(false);

	setReconPlot(NULL);

	if (isExcile() && (plot()->getOwner() == getOwner() || plot()->getOwner() == getOriginalOwner()))
	{
		jumpToNearestValidPlot(false);
	}
	setMoves(0);
}


void CvUnit::updateAirStrike(CvPlot* pPlot, bool bFinish)
{
	if (!bFinish)
	{
		if (isInBattle())
		{
			return;
		}

		if (canNuke())
		{
			kill(true, NO_PLAYER, true);
			return;
		}

		if (airStrike(pPlot) && pPlot->isVisibleToWatchingHuman())
		{
			setCombatTimer(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime());
			GC.getGame().incrementTurnTimer(getCombatTimer());

			addMission(CvAirMissionDefinition(MISSION_AIRSTRIKE, pPlot, this, NULL, getCombatTimer() * gDLL->getSecsPerTurn()));

			return;
		}
	}

	CvUnit *pDefender = getCombatUnit();
	if (pDefender)
	{
		pDefender->setCombatUnit(NULL);
	}
	setCombatUnit(NULL);
	setAttackPlot(NULL, false);

	if (isSuicide() && !isDead())
	{
		kill(true);
	}
}

void CvUnit::resolveAirCombat(CvUnit* pInterceptor, CvPlot* pPlot, CvAirMissionDefinition& kBattle)
{
	PROFILE_EXTRA_FUNC();
	CvWString szBuffer;

	int iTheirStrength = (DOMAIN_AIR == pInterceptor->getDomainType() ? pInterceptor->airCurrCombatStr(this) : pInterceptor->currCombatStr(NULL, NULL));
	int iOurStrength = (DOMAIN_AIR == getDomainType() ? airCurrCombatStr(pInterceptor) : currCombatStr(NULL, NULL));
	int iTotalStrength = iOurStrength + iTheirStrength;
	if (0 == iTotalStrength)
	{
		FErrorMsg("error");
		return;
	}

/********************************************************************************/
/* 	BETTER_BTS_AI_MOD						10/19/08	Roland J & jdog5000	*/
/* 																			*/
/* 	Combat mechanics														*/
/********************************************************************************/
	/*
	int iOurOdds = (100 * iOurStrength) / std::max(1, iTotalStrength);

	int iOurRoundDamage = (pInterceptor->currInterceptionProbability() * GC.getDefineINT("MAX_INTERCEPTION_DAMAGE")) / 100;
	int iTheirRoundDamage = (currInterceptionProbability() * GC.getDefineINT("MAX_INTERCEPTION_DAMAGE")) / 100;
	if (getDomainType() == DOMAIN_AIR)
	{
		iTheirRoundDamage = std::max(GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"), iTheirRoundDamage);
	}

	//original BTS code
	int iTheirDamage = 0;
	int iOurDamage = 0;

	for (int iRound = 0; iRound < GC.getDefineINT("INTERCEPTION_MAX_ROUNDS"); ++iRound)
	*/
	// For air v air, more rounds and factor in strength for per round damage
	int iOurOdds = (100 * iOurStrength) / std::max(1, iTotalStrength);
	int iMaxRounds = 0;
	int iOurRoundDamage = 0;
	int iTheirRoundDamage = 0;

	// Air v air is more like standard combat
	// Round damage in this case will now depend on strength and interception probability
	if( GC.getBBAI_AIR_COMBAT() && (DOMAIN_AIR == pInterceptor->getDomainType() && DOMAIN_AIR == getDomainType()) )
	{
		int iBaseDamage = GC.getDefineINT("AIR_COMBAT_DAMAGE");
		int iOurFirepower = ((airMaxCombatStr(pInterceptor) + iOurStrength + 1) / 2);
		int iTheirFirepower = ((pInterceptor->airMaxCombatStr(this) + iTheirStrength + 1) / 2);

		int iStrengthFactor = ((iOurFirepower + iTheirFirepower + 1) / 2);

		int iTheirInterception = std::max(pInterceptor->maxInterceptionProbability(),2*GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"));
		int iOurInterception = std::max(maxInterceptionProbability(),2*GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"));

		iOurRoundDamage = std::max(1, ((iBaseDamage * (iTheirFirepower + iStrengthFactor) * iTheirInterception) / ((iOurFirepower + iStrengthFactor) * 100)));
		iTheirRoundDamage = std::max(1, ((iBaseDamage * (iOurFirepower + iStrengthFactor) * iOurInterception) / ((iTheirFirepower + iStrengthFactor) * 100)));

		iMaxRounds = 2*GC.getDefineINT("INTERCEPTION_MAX_ROUNDS") - 1;
	}
	else
	{
		iOurRoundDamage = (pInterceptor->currInterceptionProbability() * GC.getDefineINT("MAX_INTERCEPTION_DAMAGE")) / 100;
		iTheirRoundDamage = (currInterceptionProbability() * GC.getDefineINT("MAX_INTERCEPTION_DAMAGE")) / 100;
		if (getDomainType() == DOMAIN_AIR)
		{
			iTheirRoundDamage = std::max(GC.getDefineINT("MIN_INTERCEPTION_DAMAGE"), iTheirRoundDamage);
		}

		iMaxRounds = GC.getDefineINT("INTERCEPTION_MAX_ROUNDS");
	}

	int iTheirDamage = 0;
	int iOurDamage = 0;

	for (int iRound = 0; iRound < iMaxRounds; ++iRound)
/********************************************************************************/
/* 	BETTER_BTS_AI_MOD						END								*/
/********************************************************************************/
	{
		if (GC.getGame().getSorenRandNum(100, "Air combat") < iOurOdds)
		{
			if (DOMAIN_AIR == pInterceptor->getDomainType())
			{
				iTheirDamage += iTheirRoundDamage;
				pInterceptor->changeDamage(iTheirRoundDamage, getOwner());
				if (pInterceptor->isDead())
				{
					break;
				}
			}
		}
		else
		{
			iOurDamage += iOurRoundDamage;
			changeDamage(iOurRoundDamage, pInterceptor->getOwner());
			if (isDead())
			{
				break;
			}
		}
	}

	if (isDead())
	{
		if (iTheirRoundDamage > 0)
		{
			int iExperience = pInterceptor->defenseXPValue();
			iExperience = (iExperience * iOurStrength) / std::max(1, iTheirStrength);
			iExperience = range(iExperience, GC.getMIN_EXPERIENCE_PER_COMBAT(), GC.getMAX_EXPERIENCE_PER_COMBAT());
			pInterceptor->changeExperience(iExperience, pInterceptor->maxXPValue(this), true, pPlot->getOwner() == pInterceptor->getOwner(), true);
		}
	}
	else if (pInterceptor->isDead())
	{
		int iExperience = attackXPValue();
		iExperience = (iExperience * iTheirStrength) / std::max(1, iOurStrength);
		iExperience = range(iExperience, GC.getMIN_EXPERIENCE_PER_COMBAT(), GC.getMAX_EXPERIENCE_PER_COMBAT());
		changeExperience(iExperience, maxXPValue(pInterceptor), true, pPlot->getOwner() == getOwner(), true);
	}
	else if (iOurDamage > 0)
	{
		if (iTheirRoundDamage > 0)
		{
			pInterceptor->changeExperience100(getExperiencefromWithdrawal(iOurOdds) * 10 / 100, 100 * pInterceptor->maxXPValue(this), true, pPlot->getOwner() == pInterceptor->getOwner(), true);
		}
	}
	else if (iTheirDamage > 0)
	{
		changeExperience100(getExperiencefromWithdrawal(iOurOdds) * 10 / 100, 100 * maxXPValue(pInterceptor), true, pPlot->getOwner() == getOwner(), true);
	}

	kBattle.setDamage(BATTLE_UNIT_ATTACKER, iOurDamage);
	kBattle.setDamage(BATTLE_UNIT_DEFENDER, iTheirDamage);
}


void CvUnit::updateAirCombat(bool bQuick)
{
	CvUnit* pInterceptor = NULL;
	bool bFinish = false;

	FAssert(getDomainType() == DOMAIN_AIR || getDropRange() > 0);

	if (getCombatTimer() > 0)
	{
		changeCombatTimer(-1);

		if (getCombatTimer() > 0)
		{
			return;
		}
		bFinish = true;
	}

	CvPlot* pPlot = getAttackPlot();
	if (pPlot == NULL)
	{
		return;
	}

	if (bFinish)
	{
		pInterceptor = getCombatUnit();
	}
	else
	{
		pInterceptor = bestInterceptor(pPlot);
	}


	if (!pInterceptor)
	{
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);
		return;
	}

	//if not finished and not fighting yet, set up combat damage and mission
	if (!bFinish)
	{
		if (!isInBattle())
		{
			if (plot()->isBattle() || pPlot->isBattle())
			{
				return;
			}

			setMadeAttack(true);

			setCombatUnit(pInterceptor, true, bQuick);
			pInterceptor->setCombatUnit(this, false, bQuick);
		}

		FAssertMsg(pInterceptor != NULL, "Defender is not assigned a valid value");

		FAssertMsg(plot()->isBattle(), "Current unit instance plot is not fighting as expected");
		FAssertMsg(pInterceptor->plot()->isBattle(), "pPlot is not fighting as expected");

		CvAirMissionDefinition kAirMission(getDomainType() == DOMAIN_AIR ? MISSION_AIRSTRIKE : MISSION_PARADROP, pPlot, this, pInterceptor, GC.getMissionInfo(MISSION_AIRSTRIKE).getTime() * gDLL->getSecsPerTurn());
		resolveAirCombat(pInterceptor, pPlot, kAirMission);

		if (!bQuick && isCombatVisible(pInterceptor))
		{
			setCombatTimer(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime());
			GC.getGame().incrementTurnTimer(getCombatTimer());
			addMission(kAirMission);
		}
		else bFinish = true;

		changeMoves(GC.getMOVE_DENOMINATOR());
		if (DOMAIN_AIR != pInterceptor->getDomainType())
		{
			pInterceptor->setMadeInterception(true);
		}

		if (isDead())
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SHOT_DOWN_ENEMY", pInterceptor->getNameKey(), getNameKey(), getVisualCivAdjective(pInterceptor->getTeam()));
			AddDLLMessage(pInterceptor->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_SHOT_DOWN", getNameKey(), pInterceptor->getNameKey());
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, pInterceptor->getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
		else if (kAirMission.getDamage(BATTLE_UNIT_ATTACKER) > 0)
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_HURT_ENEMY_AIR", pInterceptor->getNameKey(), getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_ATTACKER)), getVisualCivAdjective(pInterceptor->getTeam()));
			AddDLLMessage(pInterceptor->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_AIR_UNIT_HURT", getNameKey(), pInterceptor->getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_ATTACKER)));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, pInterceptor->getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}

		if (pInterceptor->isDead())
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SHOT_DOWN_ENEMY", getNameKey(), pInterceptor->getNameKey(), pInterceptor->getVisualCivAdjective(getTeam()));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pInterceptor->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_SHOT_DOWN", pInterceptor->getNameKey(), getNameKey());
			AddDLLMessage(pInterceptor->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
		else if (kAirMission.getDamage(BATTLE_UNIT_DEFENDER) > 0)
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_DAMAGED_ENEMY_AIR", getNameKey(), pInterceptor->getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_DEFENDER)), pInterceptor->getVisualCivAdjective(getTeam()));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pInterceptor->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_AIR_UNIT_DAMAGED", pInterceptor->getNameKey(), getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_DEFENDER)));
			AddDLLMessage(pInterceptor->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}

		if (0 == kAirMission.getDamage(BATTLE_UNIT_ATTACKER) + kAirMission.getDamage(BATTLE_UNIT_DEFENDER))
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ABORTED_ENEMY_AIR", pInterceptor->getNameKey(), getNameKey(), getVisualCivAdjective(getTeam()));
			AddDLLMessage(pInterceptor->getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pInterceptor->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_AIR_UNIT_ABORTED", getNameKey(), pInterceptor->getNameKey());
			AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
	}

	if (bFinish)
	{
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);
		pInterceptor->setCombatUnit(NULL);

		if (!isDead() && isSuicide())
		{
			kill(true);
		}
	}
}

namespace {
	bool unitsAtWar(const TeamTypes ourTeam, const CvUnit* theirUnit)
	{
		return GET_TEAM(theirUnit->getTeam()).isAtWar(ourTeam);
	}

	bool plotHasEnemy(const TeamTypes ourTeam, const CvPlot* ignorePlot, const CvPlot* plot)
	{
		return plot != ignorePlot && algo::any_of(plot->units(), bind(unitsAtWar, ourTeam, _1));
	}

	bool plotHasAdjacentEnemy(const TeamTypes ourTeam, const CvPlot* ignorePlot, const CvPlot* plot)
	{
		return algo::any_of(plot->adjacent(), bind(plotHasEnemy, ourTeam, ignorePlot, _1));
	}

	bool canWithdrawToPlot(const CvUnit* withdrawingUnit, const CvPlot* toPlot)
	{
		return withdrawingUnit->canEnterPlot(toPlot)
			&& !plotHasEnemy(withdrawingUnit->getTeam(), withdrawingUnit->plot(), toPlot)
			// && !plotHasAdjacentEnemy(withdrawingUnit.getTeam(), *withdrawingUnit.plot(), toPlot)
			;
	}

	bst::optional<CvPlot*> selectWithdrawPlot(bool bSamePlotCombat, const CvUnit* withdrawingUnit)
	{
		if (bSamePlotCombat)
		{
			return withdrawingUnit->plot();
		}

		return algo::find_if(withdrawingUnit->plot()->adjacent(), bind(canWithdrawToPlot, withdrawingUnit, _1));
	}
}

void CvUnit::resolveCombat(CvUnit* pDefender, CvPlot* pPlot, CvBattleDefinition& kBattle, bool bSamePlot)
{
	PROFILE_FUNC();

	CombatDetails cdAttackerDetails;
	CombatDetails cdDefenderDetails;

	AI_setPredictedHitPoints(-1);
	pDefender->AI_setPredictedHitPoints(-1);
	int iAttackerStrength = currCombatStr(NULL, NULL, &cdAttackerDetails);
	int iAttackerFirepower = currFirepower(NULL, NULL);
	int iDefenderStrength = 0;
	int iAttackerDamage = 0;
	int iDefenderDamage = 0;
	int iDefenderOdds = 0;

	bool bAttackerWithdrawn = false;
	//TB Combat Mods Begin
	m_combatResult.bAttackerStampedes = false;
	m_combatResult.bAttackerWithdraws = false;
	m_combatResult.bAttackerOnslaught = false;
	m_combatResult.bAttackerInjured = false;
	m_combatResult.bDefenderInjured = false;
	m_combatResult.bDeathMessaged = true;
	m_combatResult.bDefenderHitAttackerWithDistanceAttack = false;
	m_combatResult.bAttackerHitDefenderWithDistanceAttack = false;
	m_combatResult.bNeverMelee = true;
	bool bBreakdown = false;
	int iDefenderFirstStrikes = pDefender->getCombatFirstStrikes();
	int iAttackerFirstStrikes = getCombatFirstStrikes();
	//TB Combat Mods End
	bool bAttackerHasLostNoHP = true;
	int iAttackerInitialDamage = getDamage();
	int iDefenderInitialDamage = pDefender->getDamage();
	int iDefenderCombatRoll = 0;
	int iAttackerCombatRoll = 0;
	int WithdrawalRollResult = 0;
	int iAttackerHitModifier = 0;
	int iDefenderHitModifier = 0;
	int iAttackerOdds = 0;
	int	iDefenderHitChance = 0;
	int	iAttackerHitChance = 0;
	int iInitialDefXP = pDefender->getExperience100();
	int iInitialAttXP = getExperience100();
	int iInitialAttGGXP = GET_PLAYER(getOwner()).getCombatExperience();
	int iInitialDefGGXP = GET_PLAYER(pDefender->getOwner()).getCombatExperience();
	const bool bDynamicXP = GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP);

	getDefenderCombatValues(*pDefender, pPlot, iAttackerStrength, iAttackerFirepower, iDefenderOdds, iDefenderStrength, iAttackerDamage, iDefenderDamage, &cdDefenderDetails, pDefender);
	int iInitialAttackerStrength = iAttackerStrength;
	int iInitialDefenderStrength = iDefenderStrength;
	// Vanilla attacker withdrawal (TB pursuit/knockback/repel/early-withdraw escape layer removed).
	int iAttackerWithdraw = withdrawalProbability();
	int AdjustedAttWithdrawal = iAttackerWithdraw;
	int iAttackerKillOdds = iDefenderOdds * (100 - iAttackerWithdraw) / 100;

	if (isHuman() || pDefender->isHuman())
	{
		//Added ST
		CyArgsList pyArgsCD;
		pyArgsCD.add(gDLL->getPythonIFace()->makePythonObject(&cdAttackerDetails));
		pyArgsCD.add(gDLL->getPythonIFace()->makePythonObject(&cdDefenderDetails));
		pyArgsCD.add(getCombatOdds(this, pDefender));
		CvEventReporter::getInstance().genericEvent("combatLogCalc", pyArgsCD.makeFunctionArgs());
	}

	collateralCombat(pPlot, pDefender);

	int iWinningOdds = getCombatOdds(this, pDefender);
	m_combatResult.bDefenderWithdrawn = false;
	m_combatResult.pPlot = NULL;
	m_combatResult.iTurnCount++;
	bool bNoFurtherDamagetoDefender = false;

	bool bVanillaCombat = GC.getGame().isOption(GAMEOPTION_COMBAT_VANILLA_ENGINE);
	if (bVanillaCombat)
	{
		iAttackerStrength = currCombatStr(NULL, NULL, &cdAttackerDetails);
		iAttackerFirepower = currFirepower(NULL, NULL);
		getDefenderCombatValues(*pDefender, pPlot, iAttackerStrength, iAttackerFirepower, iDefenderOdds, iDefenderStrength, iAttackerDamage, iDefenderDamage, &cdDefenderDetails, pDefender);
		iAttackerOdds = std::max((GC.getCOMBAT_DIE_SIDES() - iDefenderOdds), 0);
		iDefenderHitChance = std::max(5, iDefenderOdds + ((iDefenderHitModifier * iDefenderOdds)/100));
		iAttackerHitChance = std::max(5, iAttackerOdds + ((iAttackerHitModifier * iAttackerOdds)/100));
	}

	while (true)
	{
		changeRoundCount(1);
		pDefender->changeRoundCount(1);
		if (!bVanillaCombat)
		{
			iAttackerStrength = currCombatStr(NULL, NULL, &cdAttackerDetails);
			iAttackerFirepower = currFirepower(NULL, NULL);
			getDefenderCombatValues(*pDefender, pPlot, iAttackerStrength, iAttackerFirepower, iDefenderOdds, iDefenderStrength, iAttackerDamage, iDefenderDamage, &cdDefenderDetails, pDefender);
			iAttackerOdds = std::max((GC.getCOMBAT_DIE_SIDES() - iDefenderOdds), 0);
			iDefenderHitChance = std::max(5, iDefenderOdds + ((iDefenderHitModifier * iDefenderOdds)/100));
			iAttackerHitChance = std::max(5, iAttackerOdds + ((iAttackerHitModifier * iAttackerOdds)/100));
		}

		//Check if this is a Breakdown Attack round and adjust the local bool so as to avoid reprocessing the Breakdown check multiple times per round.
		if (isBreakdownCombat(pPlot, bSamePlot) && /*getCombatFirstStrikes() == 0 &&*/ pDefender->getCombatFirstStrikes() == 0)
		{
			bBreakdown = true;
		}
		else
		{
			bBreakdown = false;
		}

		// dodge/precision hit-modifier removed; iAttackerHitChance/iDefenderHitChance
		// are computed above from the same-round odds (was lagging a round, which gave
		// the attacker a ~0.5% round-1 hit chance).
		iDefenderCombatRoll = GC.getGame().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "DefenderCombatRoll");
		iAttackerCombatRoll = GC.getGame().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "AttackerCombatRoll");
		WithdrawalRollResult = GC.getGame().getSorenRandNum(100, "Withdrawal");
		//Breakdown attack round?  If so we make the damage the defender would be dealt 0 and the chance of the attcker
		//hitting absolute so as to get through all normal checks to roll the chance for damaging the defenses while the
		//unit really does not engage in any counterattack against the defender.
		//TB Breakdown Adjustment: Finding this is probably not appropriate.  Better to allow actual combat to take place though
		//we may need to reduce the strengths on Rams some...  I'll probably end up making this more what I was looking for
		//when I get into the H2H/Distance mechanism.
		//if (bBreakdown)
		//{
		//	iDefenderDamage = 0;
		//	iAttackerHitChance = 10000;
		//}

		//TB Combat Mods (Breakdown) begin
		//Changes: No longer requires any particular combat result to make happen - previously attacker had to hit and since it had originally been setup to always hit so long as first strikes weren't taking place, the ram was rarely doing much damage.
		//I had made all rams immune to first strike though I'd prefer not to at this point... I can take that away now and allow the first strike rounds to take place as intended.
		if (bBreakdown)
		{
			resolveBreakdownAttack(pPlot);
			changeExperience100(10, MAX_INT, false, false, true);
		}
		bool bNeitherRanged = (!pDefender->isRanged() && !isRanged());
		bool bDefenderRangedbutOutofFS = (pDefender->isRanged() && pDefender->getCombatFirstStrikes() < 1);
		bool bDefenderNotRanged = (pDefender->isRanged());
		bool bAttackerRangedbutOutofFS = (pDefender->isRanged() && pDefender->getCombatFirstStrikes() < 1);
		bool bAttackerNotRanged = (isRanged());
		if (bNeitherRanged ||
			((bDefenderRangedbutOutofFS || bDefenderNotRanged) &&
			(bAttackerRangedbutOutofFS || bAttackerNotRanged)))
		{
			m_combatResult.bNeverMelee = false;
		}
		//Defender's attack round
		if (iDefenderCombatRoll < iDefenderHitChance)
		{
			if (getCombatFirstStrikes() == 0)
			{
				//	Attacker Attempts Withdrawal (vanilla)
				if ((getDamage() + iAttackerDamage) >= withdrawalHP(getMaxHP(), 0) && iAttackerWithdraw > 0)
				{
					if (WithdrawalRollResult < AdjustedAttWithdrawal)
					{
						flankingStrikeCombat(pPlot, iAttackerStrength, iAttackerFirepower, iAttackerKillOdds, iDefenderDamage, pDefender);
						bAttackerWithdrawn = true;

						if (!bDynamicXP)
						{
							changeExperience100(getExperiencefromWithdrawal(AdjustedAttWithdrawal) * 10 / 100, 100 * maxXPValue(pDefender), true, pPlot->getOwner() == getOwner(), true);

							int iExperience = 100 * pDefender->defenseXPValue() * iInitialAttackerStrength / iInitialDefenderStrength;
							iExperience = range(iExperience, 100 * GC.getMIN_EXPERIENCE_PER_COMBAT(), 100 * GC.getMAX_EXPERIENCE_PER_COMBAT());
							pDefender->changeExperience100(iExperience, 100 * pDefender->maxXPValue(this), true, pPlot->getOwner() == pDefender->getOwner(), true);
						}

// BUG - Combat Events - start
						CvEventReporter::getInstance().combatRetreat(this, pDefender);
						m_combatResult.bAttackerWithdraws = true;
						m_combatResult.bDeathMessaged = false;
// BUG - Combat Events - end
						break;
					}
				}
				//TB Combat Mod (Afflict) begin
				if (iAttackerDamage > 0)
				{
					m_combatResult.bAttackerInjured = true;
				}
				//TB Combat Mod (Afflict) end
				changeDamage(iAttackerDamage, pDefender->getOwner());
				//TB Combat Mod begin
				//TB Combat Mod end

				bAttackerHasLostNoHP = false;

				if (pDefender->getCombatFirstStrikes() > 0 && pDefender->isRanged())
				{
					kBattle.addFirstStrikes(BATTLE_UNIT_DEFENDER, 1);
					kBattle.addDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_RANGED, iAttackerDamage);
				}

				cdAttackerDetails.iCurrHitPoints = getHP();

				if (isHuman() || pDefender->isHuman())
				{
					CyArgsList pyArgs;
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdAttackerDetails));
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdDefenderDetails));
					pyArgs.add(1);
					pyArgs.add(iAttackerDamage);
					CvEventReporter::getInstance().genericEvent("combatLogHit", pyArgs.makeFunctionArgs());
				}
				if (pDefender->getCombatFirstStrikes() > 0 && pDefender->isRanged())
				{
					m_combatResult.bDefenderHitAttackerWithDistanceAttack = true;
				}
			}
		}
		//Attacker's attack round
		if ((bVanillaCombat && iDefenderCombatRoll >= iDefenderHitChance) || (iAttackerCombatRoll < iAttackerHitChance))
		{
			if (pDefender->getCombatFirstStrikes() == 0)
			{
				//Attacker reaches combat limit (cannot reduce defender below it) -> cap and end.
				if (std::min(pDefender->getMaxHP(), pDefender->getDamage() + iDefenderDamage) > combatLimit(pDefender))
				{
					if (!bBreakdown || getDamage() > combatLimit(this))
					{
						if (!bDynamicXP)
						{
							changeExperience100(getExperiencefromWithdrawal(100), 100 * maxXPValue(pDefender), true, pPlot->getOwner() == getOwner(), true);
							int iExperience = 100 * pDefender->defenseXPValue() * iInitialAttackerStrength / iInitialDefenderStrength;
							iExperience = range(iExperience, 100 * GC.getMIN_EXPERIENCE_PER_COMBAT(), 100 * GC.getMAX_EXPERIENCE_PER_COMBAT());
							pDefender->changeExperience100(iExperience, 100 * pDefender->maxXPValue(this), true, pPlot->getOwner() == pDefender->getOwner(), true);
						}
						m_combatResult.bDeathMessaged = false;
						pDefender->setDamage(combatLimit(pDefender), getOwner());
						break;
					}
					else
					{
						bNoFurtherDamagetoDefender = true;
					}
				}

				//TB Combat Mods (Afflict) begin
				if (iDefenderDamage > 0)
				{
					if (!bNoFurtherDamagetoDefender)
					{
						m_combatResult.bDefenderInjured = true;
						pDefender->changeDamage(iDefenderDamage, getOwner());
					}
				}
				//TB Combat Mods (Afflict) end
				//TB Combat Mods Begin
				if (!bBreakdown)
				{
				}
				//TB Combat Mods End

				if (getCombatFirstStrikes() > 0 && isRanged())
				{
					kBattle.addFirstStrikes(BATTLE_UNIT_ATTACKER, 1);
					kBattle.addDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, iDefenderDamage);
				}

				cdDefenderDetails.iCurrHitPoints=pDefender->getHP();

				if (isHuman() || pDefender->isHuman())
				{
					CyArgsList pyArgs;
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdAttackerDetails));
					pyArgs.add(gDLL->getPythonIFace()->makePythonObject(&cdDefenderDetails));
					pyArgs.add(0);
					pyArgs.add(iDefenderDamage);
					CvEventReporter::getInstance().genericEvent("combatLogHit", pyArgs.makeFunctionArgs());
				}

				if (!bBreakdown && !pDefender->isDead())
				{
					if (getCombatFirstStrikes() > 0 && isRanged())
					{
						m_combatResult.bAttackerHitDefenderWithDistanceAttack = true;
					}
				}
			}
		}

		if (getCombatFirstStrikes() > 0)
		{
			changeCombatFirstStrikes(-1);
		}

		if (pDefender->getCombatFirstStrikes() > 0)
		{
			pDefender->changeCombatFirstStrikes(-1);
		}

		if (isDead() || pDefender->isDead())
		{
			if (isDead())
			{
				if (!bDynamicXP)
				{
					int iExperience = pDefender->defenseXPValue() * iInitialAttackerStrength / iInitialDefenderStrength;
					iExperience = range(iExperience, GC.getMIN_EXPERIENCE_PER_COMBAT(), GC.getMAX_EXPERIENCE_PER_COMBAT());
					pDefender->changeExperience(iExperience, pDefender->maxXPValue(this), true, pPlot->getOwner() == pDefender->getOwner(), true);
				}
				// Koshling - add rolling history of combat results to allow the AI to adapt to what it sees happening
				pPlot->area()->recordCombatDeath(getOwner(), getUnitType(), pDefender->getUnitType());
			}
			else
			{
				//TB Note: Place again in the successful withdrawal segment if its not already there.  This may need debugging as well based on reports.
				flankingStrikeCombat(pPlot, iAttackerStrength, iAttackerFirepower, iAttackerKillOdds, iDefenderDamage, pDefender);

				if (!bDynamicXP)
				{
					int iExperience = attackXPValue() * iInitialDefenderStrength / std::max(1, iInitialAttackerStrength);
					iExperience = range(iExperience, GC.getMIN_EXPERIENCE_PER_COMBAT(), GC.getMAX_EXPERIENCE_PER_COMBAT());
					changeExperience(iExperience, maxXPValue(pDefender), true, pPlot->getOwner() == getOwner(), true);
				}
				// Koshling - add rolling history of combat results to allow the AI to adapt to what it sees happening
				pPlot->area()->recordCombatDeath(pDefender->getOwner(), pDefender->getUnitType(), getUnitType());
			}
			break;
		}
	}

	bool bPromotion = false;
	bool bDefPromotion = false;
	//TB Note: for both doBattleFieldPromotions and doDynamicXP, the iWinningOdds needs adjusted by YOUR ability to withdraw - if you have withdrawn at least.  Check the instance there.
	int iNonLethalAttackWinChance = std::max(0, AdjustedAttWithdrawal);
	int iNonLethalDefenseWinChance = 0; // no defender withdrawal/repel in the vanilla engine
	doBattleFieldPromotions(
		pDefender, cdDefenderDetails, pPlot,
		bAttackerHasLostNoHP, bAttackerWithdrawn,
		iAttackerInitialDamage, iWinningOdds,
		iInitialAttXP, iInitialAttGGXP, iDefenderInitialDamage,
		iInitialDefXP, iInitialDefGGXP, bPromotion, bDefPromotion,
		iNonLethalAttackWinChance, iNonLethalDefenseWinChance,
		iDefenderFirstStrikes, iAttackerFirstStrikes
	);
	if (bDynamicXP)
	{
		doDynamicXP(pDefender, pPlot, iAttackerInitialDamage, iWinningOdds, iDefenderInitialDamage, bPromotion, bDefPromotion);
	}
}


void CvUnit::updateCombat(CvUnit* pSelectedDefender, bool bSamePlot, bool bStealth, bool bNoCache)
{
	PROFILE_FUNC();

	/*GC.getGame().logOOSSpecial(6, getID(), getDamage());*/

	bool bFinish = false;

	if (getCombatTimer() > 0)
	{
		changeCombatTimer(-1);

		if (getCombatTimer() > 0)
		{
			/*GC.getGame().logOOSSpecial(7, getID(), getDamage());*/
			return;
		}
		bFinish = true;
	}

	CvPlot* pPlot = getAttackPlot();

	if (!pPlot)
	{
		/*GC.getGame().logOOSSpecial(8, getID(), getDamage());*/
		return;
	}

	if (getDomainType() == DOMAIN_AIR)
	{
		updateAirStrike(pPlot, bFinish);
		/*GC.getGame().logOOSSpecial(9, getID(), getDamage());*/
		return;
	}

	CvUnit* pDefender = NULL;

	if (bFinish)
	{
		pDefender = getCombatUnit();
	}
	else if (!pSelectedDefender)
	{
		pDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), this, true, false, false, false, bStealth || bNoCache);
	}
	else pDefender = pSelectedDefender;


	if (!pDefender)
	{
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);

		if (!bSamePlot)
		{
			getGroup()->groupMove(pPlot, true, (canAdvance(pPlot, 0) ? this : NULL));
		}
		/*GC.getGame().logOOSSpecial(10, getID(), getDamage());*/
		return;
	}
	//check if quick combat
	const bool bQuick = bSamePlot || !isCombatVisible(pDefender);

	const bool bHuman = isHuman();
	const bool bHumanDefender = pDefender->isHuman();

	const PlayerTypes eAttacker = getVisualOwner(pDefender->getTeam());
	const PlayerTypes eDefender = pDefender->getVisualOwner(getTeam());

	//if not finished and not fighting yet, set up combat damage and mission
	CvUnit* firstAttacker = NULL;
	if (!bFinish)
	{
		if (!isInBattle())
		{
			PROFILE("CvUnit::updateCombat.StartFight");

			//TB Combat Mods (Att&DefCounters)
			if (getRoundCount() > 0)
			{
				changeRoundCount(-getRoundCount());
			}
			if (pDefender->getRoundCount() > 0)
			{
				pDefender->changeRoundCount(-pDefender->getRoundCount());
			}
			changeAttackCount(1);
			//TB Combat Mods end
			if (!bSamePlot)
			{
				//rotate to face plot
				DirectionTypes newDirection = estimateDirection(this->plot(), pDefender->plot());
				if (newDirection != NO_DIRECTION)
				{
					setFacingDirection(newDirection);
				}

				//rotate enemy to face us
				newDirection = estimateDirection(pDefender->plot(), this->plot());
				if (newDirection != NO_DIRECTION)
				{
					pDefender->setFacingDirection(newDirection);
				}
			}

			const bool bStealthAttack = isInvisible(GET_PLAYER(pDefender->getOwner()).getTeam(), false, false) || pDefender->plot() == plot();
			if (bStealthAttack)
			{
				if (bHuman)
				{
					AddDLLMessage(
						getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_STEALTH_ATTACK_OWNER", getNameKey(), GET_PLAYER(eDefender).getNameKey(), pDefender->getNameKey()),
						"AS2D_EXPOSED", MESSAGE_TYPE_MINOR_EVENT, getButton(), GC.getCOLOR_UNIT_TEXT(), getX(), getY(), true, true
					);
				}
				if (bHumanDefender)
				{
					AddDLLMessage(
						pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_STEALTH_ATTACK", GET_PLAYER(eAttacker).getNameKey(), getNameKey(), pDefender->getNameKey()),
						"AS2D_EXPOSED", MESSAGE_TYPE_MINOR_EVENT, getButton(), GC.getCOLOR_UNIT_TEXT(), getX(), getY(), true, true
					);
				}
			}
			bool bStealthDefense = false;
			if (bStealthAttack || bStealth)
			{
				const bool bLieInWait = !isInvisible(GET_PLAYER(pDefender->getOwner()).getTeam(), false, false) && pDefender->plot() == plot();

				bStealthDefense = bStealth || bLieInWait;
				if (bStealthDefense && bLieInWait)
				{

					if (bHumanDefender)
					{
						AddDLLMessage(
							pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_STEALTH_LIE_IN_WAIT_OWNER", pDefender->getNameKey(), GET_PLAYER(getOwner()).getNameKey(), getNameKey()),
							"AS2D_EXPOSED", MESSAGE_TYPE_MINOR_EVENT, getButton(), GC.getCOLOR_UNIT_TEXT(), getX(), getY(), true, true
						);
					}
					if (bHuman)
					{
						AddDLLMessage(
							getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_STEALTH_LIE_IN_WAIT", GET_PLAYER(eAttacker).getNameKey(), getNameKey(), pDefender->getNameKey()),
							"AS2D_EXPOSED", MESSAGE_TYPE_MINOR_EVENT, getButton(), GC.getCOLOR_UNIT_TEXT(), getX(), getY(), true, true
						);
					}
				}
			}
			m_combatResult.bStealthDefense = bStealthDefense && pDefender->plot() != plot();//Note this information is transferred to bStealthDefense during bFinish routine to help define whether units should lose movement or not.
			//TBMaybeproblem : I'd like to elminate this from attackers in a stealth defense situation - they shouldn't be counted as having attacked for being ambushed.
			if (!canStampede() && !canOnslaught() && !bStealthDefense)
			{
				setMadeAttack(true);
			}
			if (getCombatUnit())
			{
				FErrorMsg("Not expected, though this code is a mess and need a full overhaul");
				getCombatUnit()->setCombatUnit(NULL);
			}
			setCombatUnit(pDefender, true, bQuick, bStealthAttack, bStealthDefense);

			firstAttacker = pDefender->getCombatUnit();

			pDefender->setCombatUnit(this, false, bQuick, bStealthAttack, bStealthDefense);
			//TB Combat Mods (Att&DefCounters)
			pDefender->changeDefenseCount(1);
			//TB Combat Mods end

			if (!firstAttacker)
			{
				pDefender->getGroup()->clearMissionQueue();
			}

			if (!bQuick
			&& !firstAttacker
			&& gDLL->getInterfaceIFace()->isCombatFocus()
			&& plot()->isInViewport()
			&& pDefender->isInViewport())
			{ // TBMaybeproblem - is it possible that all this should happen to setup the combat on a surprise defense?
				// It is not currently doing so, perhaps because of fear of the revealed unit not being visible yet?
				DirectionTypes directionType = directionXY(plot(), pPlot);
				//								N			NE				E				SE					S				SW					W				NW
				NiPoint2 directions[8] = {NiPoint2(0, 1), NiPoint2(1, 1), NiPoint2(1, 0), NiPoint2(1, -1), NiPoint2(0, -1), NiPoint2(-1, -1), NiPoint2(-1, 0), NiPoint2(-1, 1)};
				NiPoint3 attackDirection = NiPoint3(directions[directionType].x, directions[directionType].y, 0);
				float plotSize = GC.getPLOT_SIZE();
				NiPoint3 lookAtPoint(plot()->getPoint().x + plotSize / 2 * attackDirection.x, plot()->getPoint().y + plotSize / 2 * attackDirection.y, (plot()->getPoint().z + pPlot->getPoint().z) / 2);
				attackDirection.Unitize();
				gDLL->getInterfaceIFace()->lookAt(lookAtPoint, (((getOwner() != GC.getGame().getActivePlayer()) || gDLL->getGraphicOption(GRAPHICOPTION_NO_COMBAT_ZOOM)) ? CAMERALOOKAT_BATTLE : CAMERALOOKAT_BATTLE_ZOOM_IN), attackDirection);
			}
			else if (bHumanDefender)
			{
				if (BARBARIAN_PLAYER != eAttacker)
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_YOU_UNITS_UNDER_ATTACK", GET_PLAYER(getOwner()).getNameKey()),
						"AS2D_COMBAT", MESSAGE_TYPE_DISPLAY_ONLY, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true
					);
				}
				else
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_YOU_UNITS_UNDER_ATTACK_UNKNOWN"),
						"AS2D_COMBAT", MESSAGE_TYPE_DISPLAY_ONLY, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true
					);
				}
			}
		}
		FAssertMsg(pDefender, "Defender is not assigned a valid value");
		FAssertMsg(plot()->isBattle(), "Current unit instance plot is not fighting as expected");
		FAssertMsg(pPlot->isBattle(), "pPlot is not fighting as expected");

		if (!pDefender->canDefend())
		{
			if (!bQuick)
			{
				addMission(CvMissionDefinition(MISSION_SURRENDER, pPlot, this, pDefender, getCombatTimer() * gDLL->getSecsPerTurn()));

				// Surrender mission
				setCombatTimer(GC.getMissionInfo(MISSION_SURRENDER).getTime());

				GC.getGame().incrementTurnTimer(getCombatTimer());
			}
			else bFinish = true;

			// Kill them!
			pDefender->setDamage(pDefender->getMaxHP());
		}
		//	Need to check the attacker has not already died in the attempt (on building defenses)
		else if (!isDead())
		{
			PROFILE("CvUnit::updateCombat.CanDefend");

			//USE commanders here (so their command points will be decreased) for attacker and defender:
			this->tryUseCommander();
			pDefender->tryUseCommander();

            //USE commodore here (so their command points will be decreased) for attacker and defender:
			this->tryUseCommodore();
           	pDefender->tryUseCommodore();

			CvBattleDefinition kBattle(pPlot, this, pDefender);

			//	Koshling - save pre-combat helath so we can use health loss as
			//	a basis for more granular war weariness
			setupPreCombatDamage();
			pDefender->setupPreCombatDamage();

			resolveCombat(pDefender, pPlot, kBattle, bSamePlot);

			if (!bQuick)
			{
				kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_END, getDamage());
				kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_END, pDefender->getDamage());
				if (!bSamePlot)
				{
					kBattle.setAdvanceSquare(canAdvance(pPlot, pDefender->isDead() ? 0 : 1));
				}

				if (isRanged() && pDefender->isRanged())
				{
					kBattle.setDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_RANGED, kBattle.getDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_END));
					kBattle.setDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, kBattle.getDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_END));
				}
				else
				{
					kBattle.addDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_RANGED, kBattle.getDamage(BATTLE_UNIT_ATTACKER, BATTLE_TIME_BEGIN));
					kBattle.addDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_RANGED, kBattle.getDamage(BATTLE_UNIT_DEFENDER, BATTLE_TIME_BEGIN));
				}

				const int iTurns = planBattle(kBattle);
				kBattle.setMissionTime(iTurns * gDLL->getSecsPerTurn());

				setCombatTimer(firstAttacker ? std::max(firstAttacker->getCombatTimer() + 1, iTurns) : iTurns);

				GC.getGame().incrementTurnTimer(iTurns);

				//TB Debug: Without plot set, this routine ended up causing a crash at addMission below.

				if (pPlot->isActiveVisible(false) && !pDefender->isUsingDummyEntities())
				{
					//TB sameplot?
					ExecuteMove(0.5f, true);
					addMission(kBattle);
				}
			}
			else if (m_combatResult.bStealthDefense)
			{
				// Stealth defense is initialized in the bFinish section,
				//	so the previous fight should wrap up before this next combat does so.
				//	so add one to the timer for it to be queued for the next round of quick combat.
				setCombatTimer(1);
				GC.getGame().incrementTurnTimer(1);
			}
			else bFinish = true; // No need to update timers for quick combat
		}
		else bFinish = true; //Attacker died before it could even engage
	}

	if (bFinish)
	{
		PROFILE("CvUnit::updateCombat.Finish");

		const bool bStealthDefense = m_combatResult.bStealthDefense;


		//TB Combat Mod (Stampede/Onslaught)
		if (pDefender->isDead() || m_combatResult.bDefenderWithdrawn || m_combatResult.bAttackerWithdraws)
		{
			if (!bSamePlot && canStampede() && pPlot->getNumVisiblePotentialEnemyDefenders(this) > 1)
			{
				m_combatResult.bAttackerStampedes = true;
			}
			if (!bSamePlot && canOnslaught() && (getDamage() == 0) && pPlot->getNumVisiblePotentialEnemyDefenders(this) > 1)
			{
				m_combatResult.bAttackerOnslaught = true;
			}
		}
		//TB Combat Mods End
		//end the combat mission if this code executes first
		if (!isUsingDummyEntities() && isInViewport())
		{
			gDLL->getEntityIFace()->RemoveUnitFromBattle(this);
		}
		if (!pDefender->isUsingDummyEntities() && pDefender->isInViewport())
		{
			gDLL->getEntityIFace()->RemoveUnitFromBattle(pDefender);
		}
		setAttackPlot(NULL, false);
		setCombatUnit(NULL);
		pDefender->setCombatUnit(NULL);

		NotifyEntity(MISSION_DAMAGE);
		pDefender->NotifyEntity(MISSION_DAMAGE);

		if (isDead())
		{
			if (isNPC())
			{
				GET_PLAYER(pDefender->getOwner()).changeWinsVsBarbs(1);
			}

			if (!isHiddenNationality() && !pDefender->isHiddenNationality())
			{
				const int attackerWarWearinessChangeTimes100 = std::max(1, 100 * GC.getDefineINT("WW_UNIT_KILLED_ATTACKING") * (getMaxHP() - getPreCombatDamage()) / getMaxHP());
				GET_TEAM(getTeam()).changeWarWearinessTimes100(pDefender->getTeam(), *pPlot, attackerWarWearinessChangeTimes100);

				const int defenderWarWearinessChangeTimes100 = 100*GC.getDefineINT("WW_KILLED_UNIT_DEFENDING")*(pDefender->getDamage() - pDefender->getPreCombatDamage())/pDefender->getMaxHP();
				GET_TEAM(pDefender->getTeam()).changeWarWearinessTimes100(getTeam(), *pPlot, defenderWarWearinessChangeTimes100);

				GET_TEAM(pDefender->getTeam()).AI_changeWarSuccess(getTeam(), GC.getDefineINT("WAR_SUCCESS_DEFENDING"));
			}

			const int iInfluenceRatio = GC.isIDW_ENABLED() ? pDefender->doVictoryInfluence(this, false, false) : 0;

			if (bHuman)
			{
				CvWString szBuffer;

				szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DIED_ATTACKING", getNameKey(), pDefender->getNameKey());

				if (iInfluenceRatio > 0)
				{
					szBuffer = szBuffer + CvString::format(" %s: -%.1f%%", gDLL->getText("TXT_KEY_TILE_INFLUENCE").GetCString(), ((float)iInfluenceRatio)/10);
				}
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer,
					GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(),
					MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY()
				);
			}
			if (bHumanDefender)
			{
				CvWString szBuffer;

				if (BARBARIAN_PLAYER != eAttacker)
				{
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_KILLED_ENEMY_UNIT", pDefender->getNameKey(), getNameKey(), getVisualCivAdjective(pDefender->getTeam()));
				}
				else szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_KILLED_ENEMY_UNIT_HIDDEN", pDefender->getNameKey(), getNameKey());

				if (iInfluenceRatio > 0)
				{
					szBuffer = szBuffer + CvString::format(" %s: +%.1f%%", gDLL->getText("TXT_KEY_TILE_INFLUENCE").GetCString(), ((float)iInfluenceRatio)/10);
				}
				AddDLLMessage(
					pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer,
					GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitVictoryScript(),
					MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
				);
			}

			int iUnitsHealed = 0;
			if (pDefender->getVictoryAdjacentHeal() > 0
			&& GC.getGame().getSorenRandNum(100, "Field Hospital Die Roll") <= pDefender->getVictoryAdjacentHeal())
			{
				foreach_(const CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
				{
					foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
					{
						if (pLoopUnit->getTeam() == pDefender->getTeam() && pLoopUnit->isHurt())
						{
							iUnitsHealed++;
							pLoopUnit->doHeal();
						}
					}
				}
			}

			if (pDefender->getVictoryStackHeal() > 0
			&& GC.getGame().getSorenRandNum(100, "Field Surgeon Die Roll") <= pDefender->getVictoryStackHeal())
			{
				foreach_(CvUnit* pLoopUnit, pPlot->units())
				{
					if (pLoopUnit->getTeam() == pDefender->getTeam() && pLoopUnit->isHurt())
					{
						iUnitsHealed++;
						pLoopUnit->doHeal();
					}
				}
			}
			else if (pDefender->getVictoryHeal() > 0 && pDefender->isHurt()
			&& GC.getGame().getSorenRandNum(100, "Field Medic Die Roll") <= pDefender->getVictoryHeal())
			{
				pDefender->doHeal();
			}

			if (iUnitsHealed > 1)
			{
				if (bHuman)
				{
					if (BARBARIAN_PLAYER != eAttacker)
					{
						AddDLLMessage(
							getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_ENEMY_FIELD_MEDIC_DEFENDERS", GET_PLAYER(pDefender->getOwner()).getCivilizationAdjective()),
							"AS2D_COMBAT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefender->getX(), pDefender->getY()
						);
					}
					else
					{
						AddDLLMessage(
							getOwner(), true, GC.getEVENT_MESSAGE_TIME(), gDLL->getText("TXT_KEY_MISC_ENEMY_FIELD_MEDIC_DEFENDERS_HIDDEN"),
							"AS2D_COMBAT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefender->getX(), pDefender->getY()
						);
					}
				}
				if (bHumanDefender)
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_FIELD_MEDIC_DEFENDERS", pDefender->getNameKey(), iUnitsHealed),
						"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefender->getX(), pDefender->getY()
					);
				}
			}

			if (pDefender->getDefensiveVictoryMoveCount() > 0)
			{
				pDefender->changeMoves(-GC.getMOVE_DENOMINATOR());
			}

			if (getSurvivorChance() > 0
			&& GC.getGame().getSorenRandNum(100, "Too Badass Check") <= getSurvivorChance())
			{
				setSurvivor(true);
				jumpToNearestValidPlot();
			}
			else if (isOneUp())
			{
				setCanRespawn(true);
			}

			if (pDefender->isPillageOnVictory())
			{
				CvPlot* pDefenderPlot = pDefender->plot();
				int iPillageGold = getLevel() * getExperience();
				if (NO_UNIT != getLeaderUnitType())
				{
					iPillageGold *= getLevel();
				}

				if (iPillageGold > 0)
				{
					iPillageGold += (iPillageGold * pDefender->getPillageChange()) / 100;
					GET_PLAYER(pDefender->getOwner()).changeGold(iPillageGold);

					if (bHuman)
					{
						if (BARBARIAN_PLAYER != eDefender)
						{
							AddDLLMessage(
								getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
								gDLL->getText("TXT_KEY_MISC_GOLD_LOOTED_FROM_DEAD", getNameKey(), pDefender->getVisualCivAdjective(pDefender->getTeam())),
								"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY()
							);
						}
						else
						{
							AddDLLMessage(
								getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
								gDLL->getText("TXT_KEY_MISC_GOLD_LOOTED_FROM_DEAD_HIDDEN", getNameKey()),
								"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY()
							);
						}
					}

					for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
					{
						TechTypes ePillageTech = GET_PLAYER(pDefender->getOwner()).getCurrentResearch();
						CommerceTypes eCommerce = (CommerceTypes)iI;
						switch (eCommerce)
						{
						case COMMERCE_GOLD:
							if (pDefender->isPillageMarauder())
							{
								GET_PLAYER(pDefender->getOwner()).changeGold(iPillageGold);
								if (bHumanDefender)
								{
									AddDLLMessage(
										pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
										gDLL->getText("TXT_KEY_MISC_MARAUDERS_PLUNDERED_VICTIMS", iPillageGold, pDefender->getNameKey()),
										"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
									);
								}
								if (bHuman)
								{
									if (BARBARIAN_PLAYER != eDefender)
									{
										AddDLLMessage(
											getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_DEFENDED_BY_MARAUDERS", getNameKey(), getVisualCivAdjective(pDefender->getTeam())),
											"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY(), true, true
										);
									}
									else
									{
										AddDLLMessage(
											getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_DEFENDED_BY_MARAUDERS_HIDDEN", getNameKey()),
											"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY(), true, true
										);
									}
								}
							}
							break;
						case COMMERCE_RESEARCH:
							if (pDefender->isPillageResearch())
							{
								GET_TEAM(GET_PLAYER(pDefender->getOwner()).getTeam()).changeResearchProgress(ePillageTech, iPillageGold, pDefender->getOwner());
								if (bHumanDefender)
								{
									if (BARBARIAN_PLAYER != eDefender)
									{
										AddDLLMessage(
											pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText(
												"TXT_KEY_MISC_PLUNDERED_RESEARCH_FROM_VICTIMS",
												iPillageGold, pDefender->getNameKey(), getVisualCivAdjective(getTeam()), ePillageTech
											),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
									else
									{
										AddDLLMessage(
											pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_PLUNDERED_RESEARCH_FROM_VICTIMS_HIDDEN", iPillageGold, pDefender->getNameKey(), ePillageTech),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
								}
							}
							break;
						case COMMERCE_CULTURE:
							break;
						case COMMERCE_ESPIONAGE:
							if (pDefender->isPillageEspionage() && pDefenderPlot->getTeam() != NO_TEAM)
							{
								GET_TEAM(GET_PLAYER(pDefender->getOwner()).getTeam()).changeEspionagePointsAgainstTeam(getTeam(), iPillageGold);
								if (bHumanDefender)
								{
									if (BARBARIAN_PLAYER != eDefender)
									{
										AddDLLMessage(
											pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_PLUNDERED_ESPIONAGE_FROM_VICTIMS", iPillageGold, getVisualCivAdjective(getTeam()), pDefender->getNameKey()),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
									else
									{
										AddDLLMessage(
											pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_PLUNDERED_ESPIONAGE_FROM_VICTIMS_HIDDEN", iPillageGold, pDefender->getNameKey()),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
								}
							}
							break;
						}
					}
				}
			}

			// report event to Python, along with some other key state
			CvEventReporter::getInstance().combatResult(pDefender, this);

			CvOutcomeListMerged list;
			list.addOutcomeList(getUnitInfo().getKillOutcomeList());
			//getUnitInfo().getKillOutcomeList()->execute(*pDefender, getOwner(), getUnitType());
			for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
			{
				const UnitCombatTypes eCombat = (UnitCombatTypes)iI;
				if (isHasUnitCombat(eCombat))
				{
					list.addOutcomeList(GC.getUnitCombatInfo(eCombat).getKillOutcomeList());
					//pOutcomeList->execute(*pDefender, getOwner(), getUnitType());
				}
			}
			list.execute(*pDefender, getOwner(), getUnitType());

			return;
		}

		if (m_combatResult.bDefenderWithdrawn)
		{
			if (!m_combatResult.bAttackerOnslaught)
			{
				if (!m_combatResult.bAttackerStampedes)
				{
					if (bHuman)
					{
						AddDLLMessage(
							getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_WITHDRAW", pDefender->getNameKey(), getNameKey()),
							"AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_YELLOW(), pPlot->getX(), pPlot->getY()
						);
					}
					if (bHumanDefender)
					{
						AddDLLMessage(
							pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WITHDRAW", pDefender->getNameKey(), getNameKey()),
							"AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
						);
					}
				}
				else
				{
					if (bHuman)
					{
						AddDLLMessage(
							getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_WITHDRAW_STAMPEDE", pDefender->getNameKey(), getNameKey()),
							"AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_YELLOW(), pPlot->getX(), pPlot->getY()
						);
					}
					if (bHumanDefender)
					{
						AddDLLMessage(
							pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WITHDRAW_STAMPEDE", pDefender->getNameKey(), getNameKey()),
							"AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
						);
					}
				}
			}
			else
			{
				if (bHuman)
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_WITHDRAW_ONSLAUGHT", pDefender->getNameKey(), getNameKey()),
						"AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_YELLOW(), pPlot->getX(), pPlot->getY()
					);
				}
				if (bHumanDefender)
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WITHDRAW_ONSLAUGHT", pDefender->getNameKey(), getNameKey()),
						"AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
					);
				}
			}

			if (pPlot->isCity())
			{
				m_combatResult.pPlot = NULL;
				//TB Combat Mod (Stampede)
				if (m_combatResult.bAttackerStampedes || m_combatResult.bAttackerOnslaught)
				{
					attack(pPlot);
				}
				else
				{
					const bool bAdvance = !bSamePlot && canAdvance(pPlot, (pDefender->canDefend() && !pDefender->isDead() && pDefender->plot() == pPlot) ? 1 : 0);
					if (bAdvance)
					{
						if (getGroup() != NULL && pPlot->getNumVisiblePotentialEnemyDefenders(this) == 0)
						{
							PROFILE("CvUnit::updateCombat.Advance");
							getGroup()->groupMove(pPlot, true, bAdvance ? this : NULL);
						}
					}
					else if (!m_combatResult.bAttackerStampedes && !m_combatResult.bAttackerOnslaught)
					{
						changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
					}
				}
				//TB Combat Mod end
			}

			if (m_combatResult.pPlot && !bSamePlot)
			{
				FAssertMsg(m_combatResult.pPlot != plot(), "Can't escape back to attacker plot");
				FAssertMsg(m_combatResult.pPlot != pDefender->plot(), "Can't escape back to own plot");

				//defender escapes to a safe plot
				pDefender->move(m_combatResult.pPlot, true);
				const bool bAdvance = canAdvance(pPlot, (pDefender->canDefend() && !pDefender->isDead() && pDefender->plot() == pPlot) ? 1 : 0);

				//TB Combat Mod (Stampede) begin
				if (m_combatResult.bAttackerStampedes || m_combatResult.bAttackerOnslaught)
				{
					attack(pPlot);
				}
				else if (getGroup())
				{
					if (bAdvance && pPlot->getNumVisiblePotentialEnemyDefenders(this) == 0)
					{
						PROFILE("CvUnit::updateCombat.Advance");

						getGroup()->groupMove(pPlot, true, ((bAdvance) ? this : NULL));
					}
					else if (!bStealthDefense && !m_combatResult.bAttackerStampedes && !m_combatResult.bAttackerOnslaught)
					{
						changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
					}
				}
				else if (!bStealthDefense)
				{
					changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
				}
			}
			else if (!bStealthDefense)
			{
				changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
			}
		}
		else if (m_combatResult.bAttackerWithdraws)
		{
			if (!m_combatResult.bAttackerStampedes && !m_combatResult.bAttackerOnslaught)
			{
				if (bHuman)
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WITHDRAW", getNameKey(), pDefender->getNameKey()),
						"AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
					);
				}
				if (bHumanDefender)
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_WITHDRAW", getNameKey(), pDefender->getNameKey()),
						"AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_YELLOW(), pPlot->getX(), pPlot->getY()
					);
				}
				changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
				//GC.getGame().logOOSSpecial(53, getID(), getMoves(), getDamage());
			}
			//TB Combat Mod (Stampede) begin
			else if (m_combatResult.bAttackerStampedes)
			{
				if (bHuman)
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_YOU_UNIT_ATTACKER_WITHDRAW_STAMPEDE", getNameKey(), pDefender->getNameKey()),
						"AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
					);
				}
				if (bHumanDefender)
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_ATTACKER_WITHDRAW_STAMPEDE", getNameKey(), pDefender->getNameKey()),
						"AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_YELLOW(), pPlot->getX(), pPlot->getY()
					);
				}
				attack(pPlot);
			}
			else if (m_combatResult.bAttackerOnslaught)
			{
				if (bHuman)
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_YOU_UNIT_ATTACKER_WITHDRAW_ONSLAUGHT", getNameKey(), pDefender->getNameKey()),
						"AS2D_OUR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
					);
				}
				if (bHumanDefender)
				{
					AddDLLMessage(
						pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_ENEMY_UNIT_ATTACKER_WITHDRAW_ONSLAUGHT", getNameKey(), pDefender->getNameKey()),
						"AS2D_THEIR_WITHDRAWL", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_YELLOW(), pPlot->getX(), pPlot->getY()
					);
				}
				attack(pPlot);
			}
		}
		else if (pDefender->isDead())
		{
			if (pDefender->isNPC())
			{
				GET_PLAYER(getOwner()).changeWinsVsBarbs(1);
			}

			if (!isHiddenNationality() && !pDefender->isHiddenNationality())
			{
				const int defenderWarWearinessChangeTimes100 =
				(
					std::max(
						1,
						100 * GC.getDefineINT("WW_UNIT_KILLED_DEFENDING")
						* (pDefender->getMaxHP() - pDefender->getPreCombatDamage())
						/
						pDefender->getMaxHP()
					)
				);
				GET_TEAM(pDefender->getTeam()).changeWarWearinessTimes100(getTeam(), *pPlot, defenderWarWearinessChangeTimes100);

				const int attackerWarWearinessChangeTimes100 =
				(
					100 * GC.getDefineINT("WW_KILLED_UNIT_ATTACKING")
					* (getDamage() - getPreCombatDamage())
					/
					getMaxHP()
				);
				GET_TEAM(getTeam()).changeWarWearinessTimes100(pDefender->getTeam(), *pPlot, attackerWarWearinessChangeTimes100);

				GET_TEAM(getTeam()).AI_changeWarSuccess(pDefender->getTeam(), GC.getWAR_SUCCESS_ATTACKING());
			}

			const int iInfluenceRatio = GC.isIDW_ENABLED() ? doVictoryInfluence(pDefender, true, false) : 0;

			if (bHuman)
			{
				CvWString szBuffer;

				if (m_combatResult.bAttackerStampedes)
				{
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DESTROYED_ENEMY_STAMPEDE", pDefender->getNameKey(), getNameKey());
				}
				else if (m_combatResult.bAttackerOnslaught)
				{
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DESTROYED_ENEMY_ONSLAUGHT", pDefender->getNameKey(), getNameKey());
				}
				else szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_DESTROYED_ENEMY", getNameKey(), pDefender->getNameKey());

				if (iInfluenceRatio > 0)
				{
					szBuffer = szBuffer + CvString::format(" %s: +%.1f%%", gDLL->getText("TXT_KEY_TILE_INFLUENCE").GetCString(), ((float)iInfluenceRatio)/10);
				}
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer,
					GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitVictoryScript(),
					MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
				);
			}
			if (bHumanDefender)
			{
				CvWString szBuffer;

				if (m_combatResult.bAttackerStampedes)
				{
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WAS_DESTROYED_ENEMY_STAMPEDE", pDefender->getNameKey(), getNameKey());
				}
				else if (m_combatResult.bAttackerOnslaught)
				{
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WAS_DESTROYED_ENEMY_ONSLAUGHT", pDefender->getNameKey(), getNameKey());
				}
				else if (BARBARIAN_PLAYER == eAttacker)
				{
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WAS_DESTROYED_UNKNOWN", pDefender->getNameKey(), getNameKey());
				}
				else szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_WAS_DESTROYED", pDefender->getNameKey(), getNameKey(), getVisualCivAdjective(pDefender->getTeam()));

				if (iInfluenceRatio > 0)
				{
					szBuffer = szBuffer + CvString::format(" %s: -%.1f%%", gDLL->getText("TXT_KEY_TILE_INFLUENCE").GetCString(), ((float)iInfluenceRatio)/10);
				}
				AddDLLMessage(
					pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer,
					GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(),
					MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY()
				);
			}

			// report event to Python, along with some other key state
			int iUnitsHealed = 0;
			if (getVictoryAdjacentHeal() > 0
			&& GC.getGame().getSorenRandNum(100, "Field Hospital Die Roll") <= getVictoryAdjacentHeal())
			{
				foreach_(const CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
				{
					foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
					{
						if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->isHurt())
						{
							iUnitsHealed++;
							pLoopUnit->doHeal();
						}
					}
				}
			}

			if (getVictoryStackHeal() > 0
			&& GC.getGame().getSorenRandNum(100, "Field Surgeon Die Roll") <= getVictoryStackHeal())
			{
				foreach_(CvUnit* pLoopUnit, pPlot->units())
				{
					if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->isHurt())
					{
						iUnitsHealed++;
						pLoopUnit->doHeal();
					}
				}
			}
			else if (getVictoryHeal() > 0 && GC.getGame().getSorenRandNum(100, "Field Medic Die Roll") <= getVictoryHeal())
			{
				doHeal();
			}

			if (iUnitsHealed > 1)
			{
				if (bHumanDefender)
				{
					if (BARBARIAN_PLAYER != eAttacker)
					{
						AddDLLMessage(
							pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_ENEMY_FIELD_MEDIC_ATTACKERS", GET_PLAYER(getOwner()).getCivilizationAdjective()),
							"AS2D_COMBAT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), getX(), getY(), true, true
						);
					}
					else
					{
						AddDLLMessage(
							pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
							gDLL->getText("TXT_KEY_MISC_ENEMY_FIELD_MEDIC_ATTACKERS_HIDDEN"),
							"AS2D_COMBAT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), getX(), getY(), true, true
						);
					}
				}
				if (bHuman)
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_FIELD_MEDIC_ATTACKERS", getNameKey(), iUnitsHealed),
						"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY()
					);
				}
			}

			if (getOffensiveVictoryMoveCount() > 0)
			{
				changeMoves(-GC.getMOVE_DENOMINATOR());
			}

			if (pDefender->getSurvivorChance() > 0
			&& GC.getGame().getSorenRandNum(100, "Too Badass Check") <= pDefender->getSurvivorChance())
			{
				pDefender->setSurvivor(true);
				pDefender->jumpToNearestValidPlot();
			}
			else if (pDefender->isOneUp())
			{
				CvCity* pCapitalCity = GET_PLAYER(pDefender->getOwner()).getCapitalCity();
				if (pCapitalCity != NULL)
				{
					pDefender->setCanRespawn(true);
				}
			}

			if (isPillageOnVictory())
			{
				CvPlot* pDefenderPlot = (pDefender->plot());
				int iPillageGold = (pDefender->getLevel() * pDefender->getExperience());
				if (NO_UNIT != pDefender->getLeaderUnitType())
				{
					iPillageGold *= pDefender->getLevel();
				}
				if (iPillageGold > 0)
	            {
					iPillageGold += iPillageGold * getPillageChange() / 100;
					GET_PLAYER(getOwner()).changeGold(iPillageGold);

					if (bHuman)
					{
						if (BARBARIAN_PLAYER != eAttacker)
						{
							AddDLLMessage(
								getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
								gDLL->getText("TXT_KEY_MISC_GOLD_LOOTED_FROM_DEAD", pDefender->getNameKey(), getVisualCivAdjective(getTeam())),
								"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY()
							);
						}
						else
						{
							AddDLLMessage(
								getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
								gDLL->getText("TXT_KEY_MISC_GOLD_LOOTED_FROM_DEAD_HIDDEN", pDefender->getNameKey()),
								"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY()
							);
						}
					}

					for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
					{
						TechTypes ePillageTech = GET_PLAYER(getOwner()).getCurrentResearch();
						CommerceTypes eCommerce = (CommerceTypes)iI;
						switch (eCommerce)
						{
						case COMMERCE_GOLD:
							if (isPillageMarauder())
							{
								GET_PLAYER(getOwner()).changeGold(iPillageGold);
								if (bHuman)
								{
									AddDLLMessage(
										getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
										gDLL->getText("TXT_KEY_MISC_MARAUDERS_PLUNDERED_VICTIMS", iPillageGold, getNameKey()),
										"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
									);
								}
								if (bHumanDefender)
								{
									if (BARBARIAN_PLAYER != eAttacker)
									{
										AddDLLMessage(
											pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_ATTACKED_BY_MARAUDERS", pDefender->getNameKey(), getVisualCivAdjective(getTeam())),
											"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY(), true, true
										);
									}
									else
									{
										AddDLLMessage(
											pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_ATTACKED_BY_MARAUDERS_HIDDEN", pDefender->getNameKey()),
											"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), pDefenderPlot->getX(), pDefenderPlot->getY(), true, true
										);
									}
								}
							}
							break;
						case COMMERCE_RESEARCH:
							if (isPillageResearch())
							{
								GET_TEAM(GET_PLAYER(getOwner()).getTeam()).changeResearchProgress(ePillageTech, iPillageGold, getOwner());
								if (bHuman)
								{
									if (BARBARIAN_PLAYER != eDefender)
									{
										AddDLLMessage(
											getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText(
												"TXT_KEY_MISC_PLUNDERED_RESEARCH_FROM_IMP",
												iPillageGold, getNameKey(), getVisualCivAdjective(pDefender->getTeam()), ePillageTech
											),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
									else
									{
										AddDLLMessage(
											getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_PLUNDERED_RESEARCH_FROM_IMP_HIDDEN", iPillageGold, getNameKey(), ePillageTech),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
								}
							}
							break;
						case COMMERCE_CULTURE:
							break;
						case COMMERCE_ESPIONAGE:
							if (isPillageEspionage() && pDefenderPlot->getTeam() != NO_TEAM)
							{
								GET_TEAM(GET_PLAYER(getOwner()).getTeam()).changeEspionagePointsAgainstTeam(pDefenderPlot->getTeam(), iPillageGold);

								if (bHuman)
								{
									if (BARBARIAN_PLAYER != eDefender)
									{
										AddDLLMessage(
											getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText(
												"TXT_KEY_MISC_PLUNDERED_ESPIONAGE_FROM_IMP",
												iPillageGold, getNameKey(), getVisualCivAdjective(pDefender->getTeam()), ePillageTech
											),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
									else
									{
										AddDLLMessage(
											getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
											gDLL->getText("TXT_KEY_MISC_PLUNDERED_ESPIONAGE_FROM_IMP_HIDDEN", iPillageGold, getNameKey(), ePillageTech),
											"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pDefenderPlot->getX(), pDefenderPlot->getY()
										);
									}
								}
							}
							break;
						}
					}
				}
			}

			CvEventReporter::getInstance().combatResult(this, pDefender);
			PlayerTypes eDefenderUnitPlayer = pDefender->getOwner();
			UnitTypes eDefenderUnitType = pDefender->getUnitType();

			// generate the kill outcome list but don't execute it yet
			//std::vector<UnitCombatTypes> aDefenderCombats;
			CvOutcomeListMerged mergedList;
			mergedList.addOutcomeList(pDefender->getUnitInfo().getKillOutcomeList());
			for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
			{
				if(it->second.m_bHasUnitCombat)
				{
					mergedList.addOutcomeList(GC.getUnitCombatInfo((UnitCombatTypes)it->first).getKillOutcomeList());
					//aDefenderCombats.push_back((UnitCombatTypes)it->first);
				}
			}

			if (isSuicide())
			{
				kill(true);

				pDefender->kill(false, NO_PLAYER, true);
				pDefender = NULL;
			}
			else
			{
				if (pPlot == plot())
				{
					bSamePlot = true;
				}
				//TB debug attempt: added && pDefender->plot() == pPlot to try to allow units to move in when one defender on the plot withdrew.
				const bool bAdvance = !bSamePlot && canAdvance(pPlot, pDefender->canDefend() && !pDefender->isDead() && pDefender->plot() == pPlot);

				//TBMaybeproblem - should this come before the generation of the captive which takes place above at the add outcome step?
				if (bAdvance && !isNoCapture())
				{
					pDefender->setCapturingPlayer(getOwner());
					pDefender->setCapturingUnit(this);
				}

				pDefender->killUnconditional(false, NO_PLAYER, true);
				pDefender = NULL;
				//TB Combat Mod (Stampede) begin
				if (!bAdvance && !m_combatResult.bAttackerStampedes && !m_combatResult.bAttackerOnslaught && !bStealthDefense)
				{
					changeMoves(std::max(GC.getMOVE_DENOMINATOR(), pPlot->movementCost(this, plot())));
				}
				if (m_combatResult.bAttackerStampedes || m_combatResult.bAttackerOnslaught)
				{
					attack(pPlot);
				}
				if (bAdvance && (bQuick || !getGroup()->isCombat())) getGroup()->groupMove(pPlot, true, this);
			}
			//TB Combat Mods End

			mergedList.execute(*this, eDefenderUnitPlayer, eDefenderUnitType);
		}

		if (bQuick && IsSelected() && !canMove())
		{
			gDLL->getInterfaceIFace()->removeFromSelectionList(this);
		}
	}
}


bool CvUnit::isActionRecommended(int iAction) const
{
	if (getOwner() != GC.getGame().getActivePlayer()
	|| GET_PLAYER(getOwner()).isOption(PLAYEROPTION_NO_UNIT_RECOMMENDATIONS))
	{
		return false;
	}

	CvPlot* pPlot = gDLL->getInterfaceIFace()->getGotoPlot();

	if (pPlot == NULL && gDLL->shiftKey())
	{
		pPlot = getGroup()->lastMissionPlot();
	}
	if (pPlot == NULL)
	{
		pPlot = plot();
	}

	if (GC.getActionInfo(iAction).getMissionType() == MISSION_FORTIFY)
	{
		if (pPlot->isCity(true, getTeam()) && canDefend(pPlot) && pPlot->getNumDefenders(getOwner()) < (atPlot(pPlot) ? 2 : 1))
		{
			return true;
		}
	}
#ifdef _MOD_SENTRY
	else if (GC.getActionInfo(iAction).getMissionType() == MISSION_HEAL || GC.getActionInfo(iAction).getMissionType() == MISSION_SENTRY_WHILE_HEAL)
#else
	else if(GC.getActionInfo(iAction).getMissionType() == MISSION_HEAL || GC.getActionInfo(iAction).getMissionType() == MISSION_HEAL_BUILDUP)
#endif
	{
		if (isHurt() && !hasMoved() && (pPlot->getTeam() == getTeam() || healTurns(pPlot) > 0))
		{
			return true;
		}
	}
	else if (GC.getActionInfo(iAction).getCommandType() == COMMAND_PROMOTION)
	{
		return true;
	}
	else if (GC.getActionInfo(iAction).getMissionType() == MISSION_BUILD)
	{
		if (pPlot->getOwner() == getOwner())
		{
			const BuildTypes eBuild = (BuildTypes) GC.getActionInfo(iAction).getMissionData();

			FASSERT_BOUNDS(0, GC.getNumBuildInfos(), eBuild);

			if (canBuild(pPlot, eBuild))
			{
				const ImprovementTypes eImprovement = pPlot->getImprovementType();

				// Recommend build
				if (eImprovement == NO_IMPROVEMENT)
				{
					// If City AI wants it
					const CvCity* pWorkingCity = pPlot->getWorkingCity();

					if (pWorkingCity != NULL)
					{
						const int iIndex = pWorkingCity->getCityPlotIndex(pPlot);
						if (iIndex != -1 && pWorkingCity->AI_getBestBuild(iIndex) == eBuild)
						{
							return true;
						}
					}
					// Recommend improvement
					const ImprovementTypes eImprovementNew = GC.getBuildInfo(eBuild).getImprovement();

					if (eImprovementNew != NO_IMPROVEMENT)
					{
						const CvImprovementInfo& improvement = GC.getImprovementInfo(eImprovementNew);

						const BonusTypes eBonus = pPlot->getBonusType(getTeam());

						// If it provides bonus
						if (eBonus != NO_BONUS && improvement.isImprovementBonusTrade(eBonus))
						{
							return true;
						}
						// If it irrigates
						if (improvement.isCarriesIrrigation() && !pPlot->isIrrigated() && pPlot->isIrrigationAvailable(true))
						{
							return true;
						}
						// If it gives yields
						if (pWorkingCity != NULL)
						{
							if (improvement.getFlatYield(YIELD_COMMERCE, CASC_SCOPE_PLOT) > 0)
							{
								return true;
							}
							// Food is only interesting on flatland/water
							if (improvement.getFlatYield(YIELD_FOOD, CASC_SCOPE_PLOT) > 0 && !pPlot->isHills() && !pPlot->isAsPeak())
							{
								return true;
							}
							if (improvement.getFlatYield(YIELD_PRODUCTION, CASC_SCOPE_PLOT) > 0)
							{
								return true;
							}
						}
					}
				}
				// Recommend route
				const RouteTypes eRouteNew = (RouteTypes) GC.getBuildInfo(eBuild).getRoute();

				if (eRouteNew != NO_ROUTE)
				{
					// If bonus with no route
					if (!pPlot->isRoute() && pPlot->getBonusType(getTeam()) != NO_BONUS)
					{
						return true;
					}
					// If route improves yields from improvement
					if (eImprovement != NO_IMPROVEMENT
					&& (
						GC.getRouteInfo(eRouteNew).getImprovementYield(eImprovement, (YieldTypes)(YIELD_FOOD)) > 0
					||	GC.getRouteInfo(eRouteNew).getImprovementYield(eImprovement, (YieldTypes)(YIELD_PRODUCTION)) > 0
					||	GC.getRouteInfo(eRouteNew).getImprovementYield(eImprovement, (YieldTypes)(YIELD_COMMERCE)) > 0))
					{
						return true;
					}
				}
			}
		}
	}
	else if (GC.getActionInfo(iAction).getMissionType() == MISSION_FOUND)
	{
		if (canFound(pPlot) && pPlot->isBestAdjacentFound(getOwner()))
		{
			return true;
		}
	}
	return false;
}

int CvUnit::defenderValue(const CvUnit* pAttacker) const
{
	PROFILE_EXTRA_FUNC();
	if (pAttacker)
	{
		if (pAttacker->getDomainType() == DOMAIN_AIR)
		{
			// Does my current damage exceed the attackers damage limit?
			if (getDamage() >= pAttacker->airCombatLimit(this))
			{
				return 0;
			}
		}
		else if ((!isAnimal() && canCoexistWithAttacker(*pAttacker)) || !pAttacker->canAttack(*this))
		{
			return 0;
		}
	}
	if (!canDefend())
	{
		return 1;
	}
	int iValue = 2 + currCombatStr(plot(), pAttacker);

	if (isHurt() && ::isWorldUnit(getUnitType()))
	{
		iValue *= 3;
		iValue /= 4;
	}

	if (pAttacker)
	{
		if (!pAttacker->immuneToFirstStrikes())
		{
			iValue *= 100 + (firstStrikes() * 2 + chanceFirstStrikes()) * (GC.getCOMBAT_DAMAGE() * 2 / 5);
			iValue /= 100;
		}
		if (immuneToFirstStrikes())
		{
			iValue *= 100 + (pAttacker->firstStrikes() * 2 + pAttacker->chanceFirstStrikes()) * (GC.getCOMBAT_DAMAGE() * 2 / 5);
			iValue /= 100;
		}
	}
	else
	{
		if (collateralDamage() > 0)
		{
			iValue *= 100;
			iValue /= 100 + collateralDamage();
		}
		if (currInterceptionProbability() > 0)
		{
			iValue *= 100;
			iValue /= 100 + currInterceptionProbability();
		}
	}

	{
		const int iAssetValue = std::max(1, assetValueTotal()/100);
		int iCargoAssetValue = 0;
		std::vector<CvUnit*> aCargoUnits;
		getCargoUnits(aCargoUnits);
		foreach_(const CvUnit* pCargo, aCargoUnits)
		{
			iCargoAssetValue += std::max(1, pCargo->assetValueTotal()/100);
		}
		iValue = iValue * iAssetValue / std::max(1, iAssetValue + iCargoAssetValue);
	}

	if (NO_UNIT == getLeaderUnitType())
	{
		++iValue;
	}
	iValue += tauntTotal() * iValue / 100;

	// It should be greater than 0 as this target is at least valid as per the checks above
	if (pAttacker && isTargetOf(*pAttacker))
	{
		return std::max(1000000, std::min(MAX_INT, iValue + 1000000));
	}
	return std::max(1, iValue);
}

bool CvUnit::isBetterDefenderThan(const CvUnit* pDefender, const CvUnit* pAttacker) const
{
	PROFILE_EXTRA_FUNC();

	if (!pDefender) return true;

	const TeamTypes eAttackerTeam = pAttacker ? pAttacker->getTeam() : NO_TEAM;

	if (alwaysInvisible() || getTeam() == eAttackerTeam || !canDefend())
	{
		return false;
	}

	if (!pDefender->canDefend())
	{
		return true;
	}

	if (pAttacker)
	{
		if (isTargetOf(*pAttacker) && !pDefender->isTargetOf(*pAttacker))
		{
			return true;
		}

		if (!isTargetOf(*pAttacker) && pDefender->isTargetOf(*pAttacker))
		{
			return false;
		}

		if (pAttacker->canAttack(*pDefender) && !pAttacker->canAttack(*this))
		{
			return false;
		}

		if (pAttacker->canAttack(*this) && !pAttacker->canAttack(*pDefender))
		{
			return true;
		}
	}

	int iOurDefense = currCombatStr(plot(), pAttacker);
	if (::isWorldUnit(getUnitType()))
	{
		iOurDefense /= 2;
	}

	if (pAttacker)
	{
		if (!pAttacker->immuneToFirstStrikes())
		{
			iOurDefense *= 100 + (firstStrikes() * 2 + chanceFirstStrikes()) * GC.getCOMBAT_DAMAGE() * 2 / 5;
			iOurDefense /= 100;
		}

		if (immuneToFirstStrikes())
		{
			iOurDefense *= 100 + (pAttacker->firstStrikes() * 2 + pAttacker->chanceFirstStrikes()) * GC.getCOMBAT_DAMAGE() * 2 / 5;
			iOurDefense /= 100;
		}
	}
	else
	{
		if (pDefender->collateralDamage() > 0)
		{
			iOurDefense *= 100 + pDefender->collateralDamage();
			iOurDefense /= 100;
		}

		if (pDefender->currInterceptionProbability() > 0)
		{
			iOurDefense *= 100 + pDefender->currInterceptionProbability();
			iOurDefense /= 100;
		}
	}

	{
		const int iAssetValue = assetValueTotal() / 100;
		int iCargoAssetValue = 0;
		std::vector<CvUnit*> aCargoUnits;
		getCargoUnits(aCargoUnits);
		foreach_(const CvUnit* pCargoUnit, aCargoUnits)
		{
			iCargoAssetValue += pCargoUnit->assetValueTotal() / 100;
		}
		iOurDefense = iOurDefense * iAssetValue / std::max(1, iAssetValue + iCargoAssetValue);
	}

	int iTheirDefense = pDefender->currCombatStr(plot(), pAttacker);
	if (::isWorldUnit(pDefender->getUnitType()))
	{
		iTheirDefense /= 2;
	}

	if (NULL == pAttacker)
	{
		if (collateralDamage() > 0)
		{
			iTheirDefense *= (100 + collateralDamage());
			iTheirDefense /= 100;
		}

		if (currInterceptionProbability() > 0)
		{
			iTheirDefense *= (100 + currInterceptionProbability());
			iTheirDefense /= 100;
		}
	}
	else
	{
		if (!(pAttacker->immuneToFirstStrikes()))
		{
			iTheirDefense *= ((((pDefender->firstStrikes() * 2) + pDefender->chanceFirstStrikes()) * ((GC.getCOMBAT_DAMAGE() * 2) / 5)) + 100);
			iTheirDefense /= 100;
		}

		if (pDefender->immuneToFirstStrikes())
		{
			iTheirDefense *= ((((pAttacker->firstStrikes() * 2) + pAttacker->chanceFirstStrikes()) * ((GC.getCOMBAT_DAMAGE() * 2) / 5)) + 100);
			iTheirDefense /= 100;
		}
	}

	{
		const int iAssetValue = pDefender->assetValueTotal() / 100;
		int iCargoAssetValue = 0;
		std::vector<CvUnit*> aCargoUnits;
		pDefender->getCargoUnits(aCargoUnits);
		foreach_(const CvUnit* pCargoUnit, aCargoUnits)
		{
			iCargoAssetValue += pCargoUnit->assetValueTotal() / 100;
		}
		iTheirDefense = iTheirDefense * iAssetValue / std::max(1, iAssetValue + iCargoAssetValue);
	}

	if (iOurDefense == iTheirDefense)
	{
		if (NO_UNIT == getLeaderUnitType() && NO_UNIT != pDefender->getLeaderUnitType())
		{
			++iOurDefense;
		}
		else if (NO_UNIT != getLeaderUnitType() && NO_UNIT == pDefender->getLeaderUnitType())
		{
			++iTheirDefense;
		}
		else if (isBeforeUnitCycle(this, pDefender))
		{
			++iOurDefense;
		}
	}
	iOurDefense += tauntTotal() * iOurDefense / 100;
	iTheirDefense += pDefender->tauntTotal() * iTheirDefense / 100;

	return iOurDefense > iTheirDefense;
}


bool CvUnit::canDoCommand(CommandTypes eCommand, int iData1, int iData2, bool bTestVisible, bool bTestBusy) const
{
	CvUnit* pUnit;

	if (bTestBusy && getGroup()->isBusy())
	{
		return false;
	}

	switch (eCommand)
	{
	case COMMAND_PROMOTION:
		if (canPromote((PromotionTypes)iData1, iData2))
		{
			return true;
		}
		break;

	case COMMAND_UPGRADE:
		if (canUpgrade(((UnitTypes)iData1), bTestVisible))
		{
			return true;
		}
		break;

	case COMMAND_AUTOMATE:
		if (canAutomate((AutomateTypes)iData1))
		{
			return true;
		}
		break;

	case COMMAND_WAKE:
		if (!isAutomated() && isWaiting())
		{
			return true;
		}
		break;

	case COMMAND_CANCEL:
	case COMMAND_CANCEL_ALL:
		if (!isAutomated() && getGroup()->getLengthMissionQueue() > 0)
		{
			return true;
		}
		break;

	case COMMAND_STOP_AUTOMATION:
		if (isAutomated())
		{
			return true;
		}
		break;

	case COMMAND_DELETE:
		if (canScrap())
		{
			return true;
		}
		break;

	case COMMAND_GIFT:
		if (canGift(bTestVisible))
		{
			return true;
		}
		break;

	case COMMAND_LOAD:
		if (canLoad(plot()))
		{
			return true;
		}
		break;

	case COMMAND_LOAD_UNIT:
		pUnit = ::getUnit(IDInfo(((PlayerTypes)iData1), iData2));
		if (pUnit != NULL)
		{
			if (canLoadOntoUnit(pUnit, plot()))
			{
				return true;
			}
		}
		break;

	case COMMAND_UNLOAD:
		if (canUnload())
		{
			return true;
		}
		break;

	case COMMAND_UNLOAD_ALL:
		if (canUnloadAll())
		{
			return true;
		}
		break;

	case COMMAND_HOTKEY:
		if (isGroupHead())
		{
			return true;
		}
		break;

//TBSIZE
	case COMMAND_MERGE:

		if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
		{
			if (canMerge())
			{
				return true;
			}
		}
		break;

	case COMMAND_SPLIT:

		if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
		{
			if (canSplit())
			{
				return true;
			}
		}
		break;

	case COMMAND_STATUS:

		if ((PromotionTypes)iData1 != NO_PROMOTION)
		{//TBHERE
			bool bIsQuick = GC.getPromotionInfo((PromotionTypes)iData1).isQuick();
			if ((!hasMoved() || (canMove() && bIsQuick)) && canAcquirePromotion((PromotionTypes)iData1, PromotionRequirements::ForStatus))
			{
				return true;
			}
		}
		break;

	case COMMAND_ARREST:
		if (canArrest())
		{
			return true;
		}
		break;

	case COMMAND_AMBUSH:
		return false;
		break;

	default:
		FErrorMsg("error");
		break;
	}

	return false;
}


void CvUnit::doCommand(CommandTypes eCommand, int iData1, int iData2)
{
	//	The shared entry for EVERY unit command -- sleep, fortify, promote -- so one scope brackets the whole
	//	action and the nested scopes below it attribute where the time actually goes. `eCommand` is carried in
	//	the phase name rather than a payload field so the log separates the commands without a second reader.
	PERF_SCOPE("CvUnit::doCommand", getOwner());
	FAssert(getOwner() != NO_PLAYER);

	if (!canDoCommand(eCommand, iData1, iData2))
	{
		return;
	}
	switch (eCommand)
	{
		case COMMAND_PROMOTION:
		{
			promote((PromotionTypes)iData1, iData2);
			break;
		}
		case COMMAND_UPGRADE:
		{
			upgrade((UnitTypes)iData1);
			break;
		}
		case COMMAND_AUTOMATE:
		{
			automate((AutomateTypes)iData1);
			GC.getGame().updateSelectionListInternal();
			break;
		}
		case COMMAND_WAKE:
		{
			getGroup()->setActivityType(ACTIVITY_AWAKE);
			break;
		}
		case COMMAND_CANCEL:
		{
			getGroup()->popMission();
			break;
		}
		case COMMAND_CANCEL_ALL:
		{
			getGroup()->clearMissionQueue();
			break;
		}
		case COMMAND_STOP_AUTOMATION:
		{
			getGroup()->setAutomateType(NO_AUTOMATE);
			break;
		}
		case COMMAND_DELETE:
		{
			scrap();
			break;
		}
		case COMMAND_GIFT:
		{
			gift();
			break;
		}
		case COMMAND_LOAD:
		{
			load();
			break;
		}
		case COMMAND_LOAD_UNIT:
		{
			CvUnit* pUnit = ::getUnit(IDInfo(((PlayerTypes)iData1), iData2));

			if (pUnit != NULL)
			{
				loadOntoUnit(pUnit);
			}
			break;
		}
		case COMMAND_UNLOAD:
		{
			unload();
			break;
		}
		case COMMAND_UNLOAD_ALL:
		{
			unloadAll();
			break;
		}
		case COMMAND_HOTKEY:
		{
			setHotKeyNumber(iData1);
			break;
		}
		case COMMAND_MERGE:
		{
			doMerge();
			break;
		}
		case COMMAND_SPLIT:
		{
			doSplit();
			break;
		}
		case COMMAND_STATUS:
		{
			statusUpdate((PromotionTypes)iData1);
			if (!GC.getPromotionInfo((PromotionTypes)iData1).isQuick())
			{
				finishMoves();
			}
			break;
		}
		case COMMAND_ARREST:
		{
			doArrest();
			break;
		}
		case COMMAND_AMBUSH:
		{
			break;
		}
		default: FErrorMsg("error");
	}
	getGroup()->doDelayedDeath();
}


int CvUnit::getPathMovementRemaining() const
{
	return getGroup()->getPath().movementRemaining();
}


CvPlot* CvUnit::getPathEndTurnPlot() const
{
	return getGroup()->getPathEndTurnPlot();
}


bool CvUnit::generatePath(const CvPlot* pToPlot, int iFlags, bool bReuse, int* piPathTurns, int iMaxTurns, int iOptimizationLimit) const
{
	//TB OOS fix: This is just to assist me with my tracking
	//GC.getGame().logOOSSpecial(29, getGroup()->getID(), iMaxTurns);
	return getGroup()->generatePath(plot(), pToPlot, iFlags, bReuse, piPathTurns, iMaxTurns, iOptimizationLimit);
}


bool CvUnit::canEnterTerritory(TeamTypes eTeam, bool bIgnoreRightOfPassage) const
{
	if (eTeam == NO_TEAM)
	{
		return true;
	}

	if (GET_TEAM(getTeam()).isFriendlyTerritory(eTeam))
	{
		return true;
	}

	if (isEnemy(eTeam))
	{
		return true;
	}

	if (isRivalTerritory())
	{
		return true;
	}

	if (alwaysInvisible())
	{
		return true;
	}

	if (isHiddenNationality())
	{
		return true;
	}

	if (!bIgnoreRightOfPassage)
	{
		if (GET_TEAM(getTeam()).isOpenBorders(eTeam))
		{
			return true;
		}

		if (GET_TEAM(getTeam()).isLimitedBorders(eTeam) && (isOnlyDefensive() || !canFight() || isPassage()))
		{
			return true;
		}
	}
	if (!GET_TEAM(eTeam).isAlive())
	{
		return true;
	}

	return false;
}


bool CvUnit::canEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const
{
	return canEnterTerritory(eTeam, bIgnoreRightOfPassage);
}


// Returns the ID of the team to declare war against
TeamTypes CvUnit::getDeclareWarMove(const CvPlot* pPlot) const
{
	FAssert(GET_PLAYER(getOwner()).isHumanPlayer(true));

	if (getDomainType() != DOMAIN_AIR)
	{
		const TeamTypes eRevealedTeam = pPlot->getRevealedTeam(getTeam(), false);

		if (eRevealedTeam != NO_TEAM)
		{
			if ((!canEnterArea(eRevealedTeam, pPlot->area()) || getDomainType() == DOMAIN_SEA && !canCargoEnterArea(eRevealedTeam, pPlot->area(), false) && getGroup()->isAmphibPlot(pPlot))
			&& GET_TEAM(getTeam()).canDeclareWar(pPlot->getTeam()))
			{
				return eRevealedTeam;
			}
		}
		else if (pPlot->isActiveVisible(false) && canEnterPlot(pPlot, MoveCheck::Attack | MoveCheck::DeclareWar | MoveCheck::IgnoreLoad))
		{
			const CvUnit* pUnit = pPlot->plotCheck(PUF_canDeclareWar, getOwner(), isAlwaysHostile(pPlot), NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getOwner());

			if (pUnit != NULL)
			{
				return pUnit->getTeam();
			}
		}
	}
	return NO_TEAM;
}

bool CvUnit::willRevealByMove(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	const int iSight = sight(pPlot);
	const int iRange = iSight / VISION_OPEN_GROUND_COST + 1;

	foreach_(const CvPlot* plotX, pPlot->rect(iRange, iRange))
	{
		if (!plotX->isRevealed(getTeam(), false) && pPlot->canSeePlot(plotX, iSight))
		{
			return true;
		}
	}
	return false;
}

bool CvUnit::canEnterPlot(const CvPlot* pPlot, MoveCheck::flags flags /*= MoveCheck::None*/, CvUnit** ppDefender /*= NULL*/) const
{
	PROFILE_FUNC();

	if (!pPlot)
	{
		FErrorMsg("Plot is not assigned a valid value");
		return false;
	}
	// Wrong map category?
	if (m_pUnitInfo && !isMapCategory(*pPlot, *m_pUnitInfo)
	// Exiled?
	|| isExcile() && (pPlot->getOwner() == getOwner() || pPlot->getOwner() == getOriginalOwner())
	// Spies barred territorial entry by some condition
	|| isSpy()
	&& GC.getUSE_SPIES_NO_ENTER_BORDERS()
	&& NO_PLAYER != pPlot->getOwner()
	&& !GET_PLAYER(getOwner()).canSpiesEnterBorders(pPlot->getOwner()))
	{
		return false;
	}
	const bool bIgnoreLocation = flags & MoveCheck::IgnoreLocation;

	if (!bIgnoreLocation && atPlot(pPlot))
	{
		return false;
	}

	const bool bAttack = flags & MoveCheck::Attack;
	const bool bDeclareWar = flags & MoveCheck::DeclareWar;
	const bool bIgnoreLoad = flags & MoveCheck::IgnoreLoad;
	const bool bIgnoreTileLimit = flags & MoveCheck::IgnoreTileLimit;
	const bool bIgnoreAttack = flags & MoveCheck::IgnoreAttack;
	const bool bAssassinate = flags & MoveCheck::Assassinate;
	const bool bSuprise = flags & MoveCheck::Suprise;
	const bool bCheckForBest = flags & MoveCheck::CheckForBest;

	FAssertMsg(!bCheckForBest && !ppDefender || bCheckForBest && ppDefender, "MoveCheck::CheckForBest implies ppDefender is valid and vice-versa");

	const CvArea* pPlotArea = pPlot->area();
	TeamTypes ePlotTeam = pPlot->getTeam();

	if (canEnterArea(ePlotTeam, pPlotArea))
	{
		if (pPlot->getFeatureType() != NO_FEATURE)
		{
			if (m_pUnitInfo->isFeatureImpassable(pPlot->getFeatureType()))
			{
				const TechTypes eTech = (TechTypes)m_pUnitInfo->getFeaturePassableTech(pPlot->getFeatureType());
				if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
				{
					if (DOMAIN_SEA != getDomainType() || ePlotTeam != getTeam())  // sea units can enter impassable in own cultural borders
					{
						return false;
					}
				}
			}
		}

		if (pPlot->isAsPeak() && m_pUnitInfo->isTerrainImpassable(GC.getTERRAIN_PEAK())
		|| pPlot->isHills() && m_pUnitInfo->isTerrainImpassable(GC.getTERRAIN_HILL()))
		{
			const TechTypes eTech = (TechTypes)m_pUnitInfo->getTerrainPassableTech(pPlot->getTerrainType());
			if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
			{
				if (DOMAIN_SEA != getDomainType() || ePlotTeam != getTeam())  // sea units can enter impassable in own cultural borders
				{
					if (bIgnoreLoad || !canLoad(pPlot))
					{
						return false;
					}
				}
			}
		}
		if (m_pUnitInfo->isTerrainImpassable(pPlot->getTerrainType()))
		{
			const TechTypes eTech = (TechTypes)m_pUnitInfo->getTerrainPassableTech(pPlot->getTerrainType());
			if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
			{
				if (DOMAIN_SEA != getDomainType() || ePlotTeam != getTeam())  // sea units can enter impassable in own cultural borders
				{
					if (bIgnoreLoad || !canLoad(pPlot))
					{
						return false;
					}
				}
			}
		}
	}

	switch (getDomainType())
	{
		case DOMAIN_SEA:
		{
			if (!pPlot->isWater() && !canMoveAllTerrain() && !pPlot->isCanMoveSeaUnits()
			&& (!pPlot->isFriendlyCity(*this, true) || !pPlot->isCoastalLand()))
			{
				return false;
			}
			break;
		}
		case DOMAIN_AIR:
		{
			if (bAttack || bIgnoreAttack)
			{
				break;
			}

			bool bValid = pPlot->isFriendlyCity(*this, true);

			if (bValid && m_pUnitInfo->getAirUnitCap() > 0)
			{
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
				{
					if (pPlot->airUnitSpaceAvailable(getTeam()) < GC.getGame().getBaseAirUnitIncrementsbyCargoVolume())
					{
						bValid = false;
					}
				}
				else if (pPlot->airUnitSpaceAvailable(getTeam()) <= 0)
				{
					bValid = false;
				}
			}
			if (!bValid && (bIgnoreLoad || !canLoad(pPlot)))
			{
				return false;
			}

			// Afforess - 03/7/10 - Rebase Limit
			if (!GET_TEAM(getTeam()).isRebaseAnywhere() && GC.getGame().isModderGameOption(MODDERGAMEOPTION_AIRLIFT_RANGE)
			&& plotDistance(pPlot->getX(), pPlot->getY(), getX(), getY()) > GC.getGame().getModderGameOption(MODDERGAMEOPTION_AIRLIFT_RANGE))
			{
				return false;
			}
			break;
		}
		case DOMAIN_LAND:
		{
			if (pPlot->isWater() && !canMoveAllTerrain() && !pPlot->isSeaTunnel()
			&& (!pPlot->isCity() || pPlot->isCity() && 0 == GC.getLAND_UNITS_CAN_ATTACK_WATER_CITIES())
			&& (bIgnoreLoad || !isHuman() || plot()->isWater() || !canLoad(pPlot)))
			{
				return false;
			}
			if (isHominid() && plot()->getTeam() != ePlotTeam && pPlotArea->isBorderObstacle(ePlotTeam))
			{
				return false;
			}
			break;
		}
		case DOMAIN_IMMOBILE: return false;

		default: FErrorMsg("error");
	}

	//ls612: For units that can't enter non-Owned Cities
	if (getUnitInfo().hasSkill(CLS_SKILL_NO_NON_OWNED_CITY_ENTRY) && pPlot->isCity() && (pPlot->getOwner() != getOwner()))
	{
		return false;
	}

	if (isAnimal() && pPlot->isOwned())
	{
		if (!canAnimalIgnoresBorders())
		{
			return false;
		}

		if (pPlot->getImprovementType() != NO_IMPROVEMENT
		&& !canAnimalIgnoresImprovements())
		{
			return false;
		}

		if (pPlot->isCity(true) && !canAnimalIgnoresCities())
		{
			return false;
		}
	}

	bool bHasCheckedCityEntry = false;
	if (getDomainType() == DOMAIN_AIR)
	{
		if (bAttack && !bIgnoreAttack && !canAirStrike(pPlot))
		{
			return false;
		}
	}
	else
	{
		const bool bCanCoexist = canCoexistAlwaysOnPlot(*pPlot);
		const bool bVisibleEnemyDefender = !bCanCoexist && pPlot->isVisiblePotentialEnemyDefender(this);
		const bool bVisibleEnemyUnit = !bCanCoexist && (bVisibleEnemyDefender || pPlot->isVisiblePotentialEnemyDefenderless(this));

		bool bFailWithoutAttack = false;
		bool bFailWithAttack = false;

		// The following change makes it possible to capture defenseless units after having made a previous attack or paradrop
		if (bAttack && bVisibleEnemyUnit && !bSuprise && isMadeAttack() && !isBlitz())
		{
			if (!bIgnoreAttack)
			{
				return false;
			}
			bFailWithAttack = true;
		}

		if (canAttack())
		{
			if (pPlot->isVisible(getTeam(), false))
			{
				if (!bFailWithAttack && bVisibleEnemyDefender && bVisibleEnemyDefender && isMadeAttack() && !isBlitz())
				{
					return false;
				}

				if (bIgnoreAttack)
				{
					if (!bFailWithoutAttack && !bCanCoexist && bVisibleEnemyUnit
					&& (!bDeclareWar || pPlot->isVisibleOtherUnit(getOwner())))
					{
						if (bFailWithAttack)
						{
							return false;
						}
						bFailWithoutAttack = true;
					}
					if (!bFailWithAttack
					&& !bVisibleEnemyUnit
					&& (!bDeclareWar || !pPlot->isVisibleOtherUnit(getOwner()) && (!pPlot->getPlotCity() || isBlendIntoCity())))
					{
						if (bFailWithoutAttack)
						{
							return false;
						}
						bFailWithAttack = true;
					}
				}
				else if (!bCanCoexist && bVisibleEnemyDefender && !bAttack)
				{
					return false;
				}
			}

			if (bAttack && !bFailWithAttack)
			{
				//City Minimum Defense Level
				if (!bIgnoreLocation
				&& pPlot->getPlotCity()
				&& !isSpy()
				&& !isBlendIntoCity()
				&& (!isBarbCoExist() || !pPlot->isHominid())
				&& GET_TEAM(GET_PLAYER(getCombatOwner(ePlotTeam, pPlot)).getTeam()).isAtWar(ePlotTeam))
				{
					if (!pPlot->getPlotCity()->isDirectAttackable() && !canIgnoreNoEntryLevel())
					{
						return false;
					}
					bHasCheckedCityEntry = true;
				}
				const CvUnit* pDefender = pPlot->getFirstDefender(NO_PLAYER, getOwner(), this, true);
				if (pDefender)
				{
					if (!canAttack(*pDefender))
					{
						if (!bIgnoreAttack || bFailWithoutAttack)
						{
							return false;
						}
						bFailWithAttack = true;
					}
					if (bCheckForBest)
					{
						*ppDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), this, true, false, false, bAssassinate);
					}
				}
			}
		}
		else if (bAttack && !bFailWithAttack && !bIgnoreAttack
		|| !bCanCoexist && bVisibleEnemyUnit && pPlot->isVisible(getTeam(), false))
		{
			return false;
		}

		const TeamTypes eVisibleTeam = isHuman() ? pPlot->getRevealedTeam(getTeam(), false) : ePlotTeam;

		if (!canEnterArea(eVisibleTeam, pPlotArea))
		{
			FAssert(eVisibleTeam != NO_TEAM);

			if (!GET_TEAM(getTeam()).canDeclareWar(eVisibleTeam))
			{
				return false;
			}

			if (isHuman())
			{
				if (!bDeclareWar)
				{
					return false;
				}
			}
			else if (!GET_TEAM(getTeam()).AI_isSneakAttackReady(eVisibleTeam) || !getGroup()->AI_isDeclareWar(pPlot))
			{
				return false;
			}
		}
	}

	bool bValid = false;
	if (pPlot->isImpassable(getTeam()))
	{
		//Check our current tile
		if (plot()->isAsPeak())
		{
			//	Can this unit move through peaks regardless?
			if (isCanMovePeaks())
			{
				bValid = true;
			}
			else
			{
				//	If not we need a peak leader to be present
				bValid = plot()->getHasMountainLeader(getTeam());
			}
		}
		//Check the impassible tile
		if (!bValid)
		{
			if (pPlot->isAsPeak())
			{
				// Can this unit move through peaks regardless?
				if (isCanMovePeaks())
				{
					bValid = true;
				}
				else
				{
					// If not we need a peak leader to be present
					bValid = pPlot->getHasMountainLeader(getTeam());
				}
			}
			if (!bValid && !canMoveImpassable())
			{
				return false;
			}
		}
	}

	if (GC.getGame().getModderGameOption(MODDERGAMEOPTION_MAX_UNITS_PER_TILES) > 0)
	{
		if (!bIgnoreTileLimit)
		{
			if (!getUnitInfo().hasSkill(CLS_SKILL_ONLY_DEFENSIVE) && baseCombatStr() > 0)
			{
				if (getDomainType() == DOMAIN_LAND && !pPlot->isWater() || getDomainType() == DOMAIN_SEA && pPlot->isWater() || getDomainType() == DOMAIN_AIR)
				{
					int iCount = 0;

					//Check our current tile
					foreach_(const CvUnit* pLoopUnit, pPlot->units())
					{
						if (pLoopUnit->getTeam() == getTeam())
						{
							//Ignore workers, Missionaries, etc...
							if (!pLoopUnit->getUnitInfo().hasSkill(CLS_SKILL_ONLY_DEFENSIVE) && pLoopUnit->baseCombatStr() > 0)
							{
								//No counting cargo for ships, or harbors
								if (pLoopUnit->getDomainType() == getDomainType())
								{
									iCount++;
								}
							}
						}
					}//Unit is already on the tile, ignore it in the count
					if (bIgnoreLocation)
					{
						iCount--;
					}
					if (GC.getGame().getModderGameOption(MODDERGAMEOPTION_MAX_UNITS_PER_TILES) <= iCount)
					{
						return false;
					}
				}
			}
		}
	}

	if (!bIgnoreLocation && GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL))
	{
		//	ZoC don't apply into cities of the unit owner
		if (pPlot->getPlotCity() == NULL || pPlot->getPlotCity()->getTeam() != getTeam())
		{
			// Fort ZoC
			const PlayerTypes eDefender = plot()->controlsAdjacentZOCSource(getTeam());
			if (eDefender != NO_PLAYER)
			{
				const CvPlot* pZoneOfControl = plot()->isInFortControl(true, eDefender, getOwner());
				const CvPlot* pForwardZoneOfControl = pPlot->isInFortControl(true, eDefender, getOwner());
				if (pZoneOfControl != NULL && pForwardZoneOfControl != NULL && !canIgnoreZoneofControl()
				&& pZoneOfControl == pPlot->isInFortControl(true, eDefender, getOwner(), pZoneOfControl))
				{
					return false;
				}
			}
			// City ZoC
			if (plot()->isInCityZoneOfControl(getOwner()) && pPlot->isInCityZoneOfControl(getOwner()) && !canIgnoreZoneofControl())
			{
				return false;
			}
		}
		// Promotion ZoC
		if (GC.getGame().isAnyoneHasUnitZoneOfControl() && !canIgnoreZoneofControl()
		&& plot()->isInUnitZoneOfControl(getOwner()) && pPlot->isInUnitZoneOfControl(getOwner()))
		{
			return false;
		}
	}
	//City Minimum Defense Level
	if (!bHasCheckedCityEntry
	&& !bIgnoreLocation
	&& pPlot->getPlotCity()
	&& !isSpy()
	&& !isBlendIntoCity()
	&& (!isBarbCoExist() || !pPlot->isHominid())
	&& GET_TEAM(GET_PLAYER(getCombatOwner(ePlotTeam, pPlot)).getTeam()).isAtWar(ePlotTeam)
	&& !pPlot->getPlotCity()->isDirectAttackable()
	&& !canIgnoreNoEntryLevel())
	{
		return false;
	}
	return true;
}

bool CvUnit::canEnterOrAttackPlot(const CvPlot* pPlot, bool bDeclareWar) const
{
	const bool ignoreLocation = stepDistance(pPlot->getX(), pPlot->getY(), getX(), getY()) != 1;
	return canEnterPlot(pPlot,
		(bDeclareWar ? MoveCheck::DeclareWar : MoveCheck::None) |
		(ignoreLocation ? MoveCheck::IgnoreLocation : MoveCheck::None) |
		MoveCheck::IgnoreAttack
	);
}

bool CvUnit::canMoveThrough(const CvPlot* pPlot, bool bDeclareWar) const
{
	return canEnterPlot(pPlot, (bDeclareWar ? MoveCheck::DeclareWar : MoveCheck::None) | MoveCheck::IgnoreLoad);
}

void CvUnit::attack(CvPlot* pPlot, bool bStealth, bool bNoCache)
{
	PROFILE_FUNC();
	FAssert(plot() == pPlot || bStealth || bNoCache || canEnterPlot(pPlot, MoveCheck::Attack));
	FAssert(getCombatTimer() == 0);

	m_combatResult.iTurnCount = 0;

	//TB Combat Mods begin

	if (!isDead())
	{
		//TB Combat Mods end
		setAttackPlot(pPlot, false);

		FAssertMsg(pPlot != plot(), "We are passing in false for bSamePlot so why are we on the same plot? (This is here to confirm if the bSamePlot parameter actually means what it says or not, we might remove the parameter or rename it if the assert is hit)");
		updateCombat(NULL, false, bStealth, bNoCache);
	}
}

void CvUnit::fightInterceptor(const CvPlot* pPlot, bool bQuick)
{
	FAssert(getCombatTimer() == 0);

	setAttackPlot(pPlot, true);

	updateAirCombat(bQuick);
}


void CvUnit::move(CvPlot* pPlot, bool bShow)
{
	PROFILE_FUNC();

	FAssert(canEnterOrAttackPlot(pPlot) || isMadeAttack());

	CvPlot* pOldPlot = plot();

	changeMoves(pPlot->movementCost(this, pOldPlot));

	//GC.getGame().logOOSSpecial(16, getID(), pPlot->getX(), pPlot->getY());
	OutputDebugString(CvString::format("%S (%d) CvUnit::move (%d,%d)-->(%d,%d)\n", getDescription().c_str(), m_iID, m_iX, m_iY, pPlot->getX(), pPlot->getY()).c_str());

	setXY(pPlot->getX(), pPlot->getY(), true, true, bShow && pPlot->isVisibleToWatchingHuman(), bShow);

	//TBFIXHERE it's very possible for the unit to be dead from this point and there are further move aspects taking place that would make little sense if unit is dead.
	if (isDead())
	{
		// Toffer - Shouldn't this be handled when pLoopUnit actually dies in the above pLoopUnit->move(pPlot, true);
		//	rather than after it has died here below.
		joinGroup(NULL, true);
		finishMoves();
		return;
	}
	const FeatureTypes featureType = pPlot->getFeatureType();
	if (featureType != NO_FEATURE)
	{
		const CvString featureString(GC.getFeatureInfo(featureType).getOnUnitChangeTo());
		if (!featureString.IsEmpty())
		{
			pPlot->setFeatureType((FeatureTypes)GC.getInfoTypeForString(featureString));
		}
		//spawn birds if trees present - JW
		else if (!pPlot->isOwned() && getOwner() == GC.getGame().getActivePlayer()
		&& GC.getASyncRand().get(100) < GC.getFeatureInfo(featureType).getEffectProbability())
		{
			EffectTypes eEffect = (EffectTypes)GC.getInfoTypeForString(GC.getFeatureInfo(featureType).getEffectType());
			gDLL->getEngineIFace()->TriggerEffect(eEffect, pPlot->getPoint(), (float)(GC.getASyncRand().get(360)));
			gDLL->getInterfaceIFace()->playGeneralSound("AS3D_UN_BIRDS_SCATTER", pPlot->getPoint());
		}
	}
	if((pPlot->getOwner() != getOwner() || !pPlot->isOwned() ) && !(GET_PLAYER(getOwner()).isNPC()))
	{
		changeExperience100(10, 500);
		changeExperience100(1, 2000);
		if(isHasUnitCombat(GC.getUNITCOMBAT_RECON()))
		{
			changeExperience100(4, 10000);
		}
	}
}

// false if unit is killed
bool CvUnit::jumpToNearestValidPlot(bool bKill)
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(!isAttacking(), "isAttacking did not return false as expected");
	FAssertMsg(!isInBattle(), "isInBattle did not return false as expected");

	//	If the jump is due to being in an incorrect doamin it implies there WILL be an area change, so the relevant nearest
	//	city cannot possibly be in the same area, hence we need to search all
	CvCity* pNearestCity = GC.getMap().findCity(getX(), getY(), getOwner(), NO_TEAM, plot()->isValidDomainForAction(*this));

	int iBestValue = MAX_INT;
	CvPlot* pBestPlot = NULL;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);

		if (pLoopPlot->isValidDomainForLocation(*this))
		{
			if (canEnterPlot(pLoopPlot))
			{
				if (canEnterArea(pLoopPlot->getTeam(), pLoopPlot->area()) && !isEnemy(pLoopPlot->getTeam(), pLoopPlot))
				{
					FAssertMsg(!atPlot(pLoopPlot), "atPlot(pLoopPlot) did not return false as expected");

					if ((getDomainType() != DOMAIN_AIR) || pLoopPlot->isFriendlyCity(*this, true))
					{
						if (pLoopPlot->isRevealed(getTeam(), false))
						{
							int iValue = (plotDistance(getX(), getY(), pLoopPlot->getX(), pLoopPlot->getY()) * 2);

							if (pNearestCity != NULL)
							{
								iValue += plotDistance(pLoopPlot->getX(), pLoopPlot->getY(), pNearestCity->getX(), pNearestCity->getY());

								//	Try to at least favour the same landmass as the nearest city
								if (pLoopPlot->area() != pNearestCity->area())
								{
									iValue *= 3;
								}
							}

							if (getDomainType() == DOMAIN_SEA && !plot()->isWater())
							{
								if (!pLoopPlot->isWater() || !pLoopPlot->isAdjacentToArea(area()))
								{
									iValue *= 3;
								}
							}
							else
							{
								if (pLoopPlot->area() != area())
								{
									iValue *= 3;
								}
							}

							if (iValue < iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL)
	{
		//GC.getGame().logOOSSpecial(17, getID(), pBestPlot->getX(), pBestPlot->getY());
		setXY(pBestPlot->getX(), pBestPlot->getY());
		return true;
	}

	if (bKill)
	{
		kill(false);
	}
	return false;
}


bool CvUnit::canAutomate(AutomateTypes eAutomate) const
{
	if (eAutomate == NO_AUTOMATE)
	{
		return false;
	}

	if (!isGroupHead())
	{
		return false;
	}
	/************************************************************************************************/
	/* Afforess	                  Start		 02/14/10                                               */
	/*                                                                                              */
	/*  Clicking on the Automate button with an Inquisitor causes a CTD                             */
	/************************************************************************************************/
	if (getUnitInfo().hasSkill(CLS_SKILL_INQUISITOR))
	{
		return false;
	}
	/************************************************************************************************/
	/* Afforess	                     END                                                            */
	/************************************************************************************************/

	switch (eAutomate)
	{
	case AUTOMATE_BUILD:
		if ((AI_getUnitAIType() != UNITAI_WORKER) && (AI_getUnitAIType() != UNITAI_WORKER_SEA))
		{
			return false;
		}
		break;

	case AUTOMATE_NETWORK:
		if ((AI_getUnitAIType() != UNITAI_WORKER) || !canBuildRoute())
		{
			return false;
		}
		break;

	case AUTOMATE_CITY:
		if (AI_getUnitAIType() != UNITAI_WORKER)
		{
			return false;
		}
		break;

	case AUTOMATE_EXPLORE:
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD                      04/25/10                                jdog5000      */
		/*                                                                                              */
		/* Player Interface                                                                             */
		/************************************************************************************************/
		if (!canFight())
		{
			// Enable exploration for air units
			if (getDomainType() != DOMAIN_SEA && getDomainType() != DOMAIN_AIR)
			{
				if (!alwaysInvisible() || !isSpy())
				{
					return false;
				}
			}
		}

		if (getDomainType() == DOMAIN_IMMOBILE)
		{
			return false;
		}

		if (getDomainType() == DOMAIN_AIR && !canRecon())
		{
			return false;
		}

		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_EXPLORE))
		{
			return false;
		}
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD                       END                                                  */
		/************************************************************************************************/

		break;

	case AUTOMATE_RELIGION:
		if (AI_getUnitAIType() != UNITAI_MISSIONARY)
		{
			return false;
		}
		/************************************************************************************************/
		/* Afforess	                  Start		 09/16/10                                               */
		/*                                                                                              */
		/* Advanced Automations                                                                         */
		/************************************************************************************************/
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_SPREAD))
		{
			return false;
		}
		break;

	case AUTOMATE_PILLAGE:
		if (!getUnitInfo().hasSkill(CLS_SKILL_PILLAGE))
		{
			return false;
		}
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_PILLAGE))
		{
			return false;
		}
		break;
	case AUTOMATE_HUNT:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_HUNT))
		{
			return false;
		}
		if (!canAttack())
		{
			return false;
		}
		break;
	case AUTOMATE_CITY_DEFENSE:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_DEFENSE))
		{
			return false;
		}
		if (!canAttack())
		{
			return false;
		}
		break;
	case AUTOMATE_BORDER_PATROL:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_PATROL))
		{
			return false;
		}
		if (!canAttack())
		{
			return false;
		}
		break;
	case AUTOMATE_PIRATE:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_PIRATE))
		{
			return false;
		}
		if (getDomainType() != DOMAIN_SEA)
		{
			return false;
		}
		if (!canAttack())
		{
			return false;
		}
		if (!isHiddenNationality() || !getUnitInfo().hasSkill(CLS_SKILL_ALWAYS_HOSTILE))
		{
			return false;
		}
		break;
	case AUTOMATE_HURRY:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_CARAVAN))
		{
			return false;
		}
		if (m_pUnitInfo->getHurryBase() <= 0)
		{
			return false;
		}
		//Do not give ability to great people
		if (m_pUnitInfo->getProductionCost() < 0)
		{
			return false;
		}
		break;
	case AUTOMATE_AIRSTRIKE:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_AIR))
		{
			return false;
		}
		if (getDomainType() != DOMAIN_AIR)
		{
			return false;
		}
		if (!canAirAttack())
		{
			return false;
		}
		//Jets and Fighters can intercept, modders, if you have fighters with 0 interception, feel free to get rid of this check
		if (maxInterceptionProbability() <= 0)
		{
			return false;
		}
		break;
	case AUTOMATE_AIRBOMB:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_AIR))
		{
			return false;
		}
		if (getDomainType() != DOMAIN_AIR)
		{
			return false;
		}

		if (getAirBombBaseRate() == 0)
		{
			return false;
		}

		if (canAutomate(AUTOMATE_AIRSTRIKE))
		{
			return false;
		}
		break;
	case AUTOMATE_AIR_RECON:
		if (!canRecon())
		{
			return false;
		}
		break;
	case AUTOMATE_UPGRADING:
		if (m_pUnitInfo->getUpgradesTo().empty())
		{
			return false;
		}
		if (isAutoUpgrading())
		{
			return false;
		}
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_UPGRADE))
		{
			return false;
		}
		break;
	case AUTOMATE_CANCEL_UPGRADING:
		if (m_pUnitInfo->getUpgradesTo().empty())
		{
			return false;
		}
		if (!isAutoUpgrading())
		{
			return false;
		}
		break;
	case AUTOMATE_PROMOTIONS:
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_PROMOTE))
		{
			return false;
		}
		if (!canAcquirePromotionAny())
		{
			return false;
		}
		if (isAutoPromoting())
		{
			return false;
		}
		break;
	case AUTOMATE_CANCEL_PROMOTIONS:
		if (!canAcquirePromotionAny())
		{
			return false;
		}
		if (!isAutoPromoting())
		{
			return false;
		}
		break;
	case AUTOMATE_SHADOW:
		if (!canShadow())
		{
			return false;
		}
		if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_PROTECT))
		{
			return false;
		}
		break;
/************************************************************************************************/
/* Afforess	                     END                                                            */
/************************************************************************************************/
	case AUTOMATE_SPREAD:
		//	Auto-spread (#381): chase heritage/construct targets (tamed/subdued animals
		//	carrying their herd buildings to the cities that lack the bonus). Only offered
		//	to units that can actually construct something.
		if (m_pUnitInfo->getGrantedBuildings().empty() && m_pUnitInfo->getHeritage().empty())
		{
			return false;
		}
		break;
	default:
		FErrorMsg("error");
		break;
	}

	return true;
}


void CvUnit::automate(AutomateTypes eAutomate)
{
	PROFILE_EXTRA_FUNC();
	if (!canAutomate(eAutomate))
	{
		return;
	}

	if (eAutomate == AUTOMATE_UPGRADING || eAutomate == AUTOMATE_CANCEL_UPGRADING)
	{
		foreach_(CvUnit* pLoopUnit, getGroup()->units())
		{
			pLoopUnit->setAutoUpgrading((eAutomate == AUTOMATE_UPGRADING));
		}
		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
		}
		return;
	}
	if (eAutomate == AUTOMATE_PROMOTIONS || eAutomate == AUTOMATE_CANCEL_PROMOTIONS)
	{
		foreach_(CvUnit* pLoopUnit, getGroup()->units())
		{
			pLoopUnit->setAutoPromoting((eAutomate == AUTOMATE_PROMOTIONS));
		}
		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
		}
		return;
	}

	getGroup()->setAutomateType(eAutomate);
}


bool CvUnit::canScrap() const
{
	if (plot()->isBattle())
	{
		return false;
	}

	return true;
}


// No need to let return value exceed MAX_INT, shouldn't really happen unless one of the most expensive units is merged many times.
int CvUnit::calculateScrapValue() const
{
	int64_t iCost = getUnitInfo().getProductionCost() * CvGameSpeedScale::hammerCostPercent();

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		const int iGroupDiff = groupRank() - m_pUnitInfo->getBaseGroupRank();
		if (iGroupDiff != 0)
		{
			if (iGroupDiff > 0)
			{
				iCost *= intPow64(3, iGroupDiff);
			}
			else iCost /= intPow64(3, -iGroupDiff);
		}
	}
	iCost /= 100*GC.getUNIT_GOLD_DISBAND_DIVISOR();

	if (iCost > MAX_INT) return MAX_INT;
	// A minimum return of 1 will cause oddities in early game where a unit that can split only gives 1 gold,
	//	giving the player a reason to split before disbanding to earn a couple extra gold coins.
	if (iCost < 1) return 0;

	return static_cast<int>(iCost);
}

void CvUnit::scrap()
{
	if (!canScrap())
	{
		return;
	}

	if (GC.getGame().isOption(GAMEOPTION_UNIT_DOWNSIZING_IS_PROFITABLE) && plot()->getOwner() == getOwner())
	{
		GET_PLAYER(getOwner()).changeGold(calculateScrapValue());
	}

	getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	kill(true, NO_PLAYER, true);
}


bool CvUnit::canGift(bool bTestVisible, bool bTestTransport) const
{
	PROFILE_EXTRA_FUNC();
	const CvPlot* pPlot = plot();

	if (!pPlot->isOwned()
	||  pPlot->getOwner() == getOwner()
	||  pPlot->isVisibleEnemyUnit(this)
	||  pPlot->isVisibleEnemyUnit(pPlot->getOwner()))
	{
		return false;
	}

	{
		const CvUnit* pTransport = getTransportUnit();

		if (pTransport)
		{
			if (bTestTransport && pTransport->getTeam() != pPlot->getTeam())
			{
				return false;
			}
		}
		else if (!pPlot->isValidDomainForLocation(*this))
		{
			return false;
		}
	}

	for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
	{
		if (m_pUnitInfo->getCorporationSpreadStrength(iCorp) > 0)
		{
			return false;
		}
	}

	if (!bTestVisible)
	{
		if (GET_PLAYER(pPlot->getOwner()).isUnitMaxedOut(getUnitType(), GET_PLAYER(pPlot->getOwner()).getUnitMaking(getUnitType()))
		|| !GET_PLAYER(pPlot->getOwner()).AI_acceptUnit(this))
		{
			return false;
		}
	}
	return !atWar(pPlot->getTeam(), getTeam());
}


void CvUnit::gift(bool bTestTransport)
{
	if (!canGift(false, bTestTransport))
	{
		return;
	}
	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	if (!aCargoUnits.empty())
	{
		validateCargoUnits();
	}
	algo::for_each(aCargoUnits, bind(CvUnit::gift, _1, false));

	const PlayerTypes eNewOwner = plot()->getOwner();

	FAssertMsg(eNewOwner != NO_PLAYER, "plot()->getOwner() is not expected to be equal with NO_PLAYER");

	CvUnit* pGiftUnit = GET_PLAYER(eNewOwner).initUnit(getUnitType(), getX(), getY(), AI_getUnitAIType(), NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
	if (pGiftUnit == NULL)
	{
		FErrorMsg("GiftUnit is not assigned a valid value");
		return;
	}
	const PlayerTypes eOldOwner = getOwner();

	pGiftUnit->convert(this);

	if (pGiftUnit->isCombat())
	{
		GET_PLAYER(eNewOwner).AI_changePeacetimeGrantValue(eOldOwner, (pGiftUnit->getUnitInfo().getProductionCost() * 3 * GC.getGame().AI_combatValue(pGiftUnit->getUnitType()))/100);
	}
	else GET_PLAYER(eNewOwner).AI_changePeacetimeGrantValue(eOldOwner, (pGiftUnit->getUnitInfo().getProductionCost()));

	if (pGiftUnit->isHuman())
	{
		AddDLLMessage(
			eNewOwner, false, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText("TXT_KEY_MISC_GIFTED_UNIT_TO_YOU", GET_PLAYER(eOldOwner).getNameKey(), pGiftUnit->getNameKey()),
			"AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, pGiftUnit->getButton(), GC.getCOLOR_WHITE(), pGiftUnit->getX(), pGiftUnit->getY(), true, true
		);
	}
	// Python Event
	CvEventReporter::getInstance().unitGifted(pGiftUnit, eOldOwner, plot());
}


bool CvUnit::canLoadOntoUnit(const CvUnit* pUnit, const CvPlot* pPlot) const
{
	FAssert(pUnit != NULL);
	FAssert(pPlot != NULL);

	if (pUnit == this)
	{
		return false;
	}

	if (!pUnit->isCarrier())
	{
		return false;
	}


	if (pUnit->getUnitType() == getUnitType())
	{
		return false;
	}

	if (pUnit->getTeam() != getTeam())
	{
		return false;
	}

	if (getTransportUnit() == pUnit || pUnit->getTransportUnit() == this)
	{
		return false;
	}

	{
		const int iCargoSpace = pUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType());

		if (iCargoSpace < 1 || GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) && iCargoSpace < SMCargoVolume())
		{
			return false;
		}
	}

	//Not a good rule for C2C - would keep units carrying from being able to load
	//if (pUnit->isCargo())
	//{
	//	return false;
	//}

	if (!(pUnit->atPlot(pPlot)))
	{
		return false;
	}

	//Not a helpful rule for C2C
	//if (!m_pUnitInfo->hasSkill(CLS_SKILL_HIDDEN_NATIONALITY) && pUnit->getUnitInfo().hasSkill(CLS_SKILL_HIDDEN_NATIONALITY))
	//{
	//	return false;
	//}

	if (NO_SPECIALUNIT != getSpecialUnitType())
	{
		if (GC.getSpecialUnitInfo(getSpecialUnitType()).isCityLoad())
		{
			if (!pPlot->isCity(true, getTeam()))
			{
				return false;
			}
		}
	}

	return true;
}


void CvUnit::loadOntoUnit(CvUnit* pUnit)
{
	if (!canLoadOntoUnit(pUnit, plot()))
	{
		return;
	}

	setTransportUnit(pUnit);
}

bool CvUnit::shouldLoadOnMove(const CvPlot* pPlot) const
{
	if (isCargo())
	{
		return false;
	}

	switch (getDomainType())
	{
	case DOMAIN_LAND:
		if ((pPlot->isWater() && !canMoveAllTerrain()) && !pPlot->isSeaTunnel())
		{
			return true;
		}
		break;
	case DOMAIN_AIR:
		if (!pPlot->isFriendlyCity(*this, true))
		{
			return true;
		}

		if (m_pUnitInfo->getAirUnitCap() > 0)
		{
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				if (pPlot->airUnitSpaceAvailable(getTeam()) < getCargoVolume())
				{
					return true;
				}
			}
			else if (pPlot->airUnitSpaceAvailable(getTeam()) <= 0)
			{
				return true;
			}
		}
		break;
	default:
		break;
	}

	if (m_pUnitInfo->isTerrainImpassable(pPlot->getTerrainType()))
	{
		const TechTypes eTech = (TechTypes)m_pUnitInfo->getTerrainPassableTech(pPlot->getTerrainType());
		if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
		{
			return true;
		}
	}

	return false;
}


bool CvUnit::canLoad(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	foreach_(const CvUnit* pLoopUnit, pPlot->units())
	{
		if (canLoadOntoUnit(pLoopUnit, pPlot))
		{
			return true;
		}
	}

	return false;
}


void CvUnit::load()
{
	PROFILE_EXTRA_FUNC();
	if (!canLoad(plot()))
	{
		return;
	}

	const CvPlot* pPlot = plot();

	for (int iPass = 0; iPass < 2; iPass++)
	{
		foreach_(CvUnit* pLoopUnit, pPlot->units())
		{
			if (canLoadOntoUnit(pLoopUnit, pPlot))
			{
				if ((iPass == 0) ? (pLoopUnit->getOwner() == getOwner()) : (pLoopUnit->getTeam() == getTeam()))
				{
					setTransportUnit(pLoopUnit);
					break;
				}
			}
		}

		if (isCargo())
		{
			break;
		}
	}
}


bool CvUnit::canUnload() const
{
	if (getTransportUnit() == NULL)
	{
		return false;
	}

	const CvPlot& kPlot = *(plot());
	if (!kPlot.isValidDomainForLocation(*this))
	{
		return false;
	}


	if (m_pUnitInfo != NULL && !isMapCategory(kPlot, *m_pUnitInfo))
	{
		return false;
	}

	if (getDomainType() == DOMAIN_AIR)
	{
		if (kPlot.isFriendlyCity(*this, true))
		{
			const int iNumAirUnits = kPlot.countNumAirUnits(getTeam());
			const CvCity* pCity = kPlot.getPlotCity();
			if (NULL != pCity)
			{
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
				{
					const int iNumAirUnitCargoVolume = kPlot.countNumAirUnitCargoVolume(getTeam());
					if (iNumAirUnitCargoVolume >= pCity->getSMAirUnitCapacity(getTeam()))
					{
						return false;
					}
				}
				else if (iNumAirUnits >= pCity->getAirUnitCapacity(getTeam()))
				{
					return false;
				}
			}
			else
			{
				if (iNumAirUnits >= GC.getCITY_AIR_UNIT_CAPACITY())
				{
					return false;
				}
			}
		}
	}

	return true;
}


void CvUnit::unload()
{
	if (!canUnload())
	{
		return;
	}

	CvUnit* pUnit = getTransportUnit();
	setTransportUnit(NULL);

}


bool CvUnit::canUnloadAll() const
{
	return hasCargo();
}


void CvUnit::unloadAll()
{
	PROFILE_EXTRA_FUNC();
	if (!canUnloadAll())
	{
		return;
	}

	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	if (!aCargoUnits.empty())
	{
		validateCargoUnits();
	}
	foreach_(CvUnit* pCargo, aCargoUnits)
	{
		if (pCargo->canUnload())
		{
			pCargo->setTransportUnit(NULL);
		}
		else
		{
			FAssert(isHuman() || pCargo->getDomainType() == DOMAIN_AIR);
			pCargo->getGroup()->setActivityType(ACTIVITY_AWAKE);
		}
	}
}


bool CvUnit::canHold() const
{
	return true;
}


bool CvUnit::canSleep() const
{
	if (isFortifyable() || isWaiting())
	{
		return false;
	}
	return true;
}


bool CvUnit::canFortify() const
{
	if (!isFortifyable() || isWaiting())
	{
		return false;
	}
	return true;
}

bool CvUnit::canBuildUp() const
{
	if (!isBuildUpable() || isWaiting())
	{
		return false;
	}
	return true;
}

bool CvUnit::canAirPatrol(const CvPlot* pPlot) const
{
	if (getDomainType() != DOMAIN_AIR || !canAirDefend(pPlot) || isWaiting())
	{
		return false;
	}
	return true;
}


bool CvUnit::canSeaPatrol(const CvPlot* pPlot) const
{
	if (!pPlot->isWater())
	{
		return false;
	}

	if (getDomainType() != DOMAIN_SEA)
	{
		return false;
	}

	if (!canFight() || isOnlyDefensive())
	{
		return false;
	}

	if (isWaiting())
	{
		return false;
	}

	return true;
}


void CvUnit::airCircle(bool bStart)
{
	if (!GC.IsGraphicsInitialized())
	{
		return;
	}

	if (!isInViewport())
	{
		return;
	}

	if (getDomainType() != DOMAIN_AIR || maxInterceptionProbability() == 0)
	{
		return;
	}

	//cancel previos missions
	if ( !isUsingDummyEntities() && isInViewport() )
	{
		gDLL->getEntityIFace()->RemoveUnitFromBattle( this );

		if (bStart)
		{
			// patrol is indefinite - time is ignored
			addMission(CvAirMissionDefinition(MISSION_AIRPATROL, plot(), this, NULL, 1.f));
		}
	}
}


bool CvUnit::canHeal(const CvPlot* pPlot) const
{
	if (!isHurt() || isWaiting() || healTurns(pPlot) == 0)
	{
		return false;
	}
	return true;
}


bool CvUnit::canSentry(const CvPlot* pPlot) const
{
	if (!canDefend(pPlot) || isWaiting())
	{
		return false;
	}
	return true;
}


int CvUnit::healRate(const CvPlot* pPlot, bool bHealCheck) const
{
	PROFILE_FUNC();

    int iBattlefieldMedicine = GC.getInfoTypeForString("TECH_BATTLEFIELD_MEDICINE");

    if (!GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) && !isAnimal() && !isNPC())
    {
        // The self-recovery verdict is a BARE FETCH off the resolved HEAL block -- folded when the promotion
        // landed, never rediscovered by sweeping the promotion registry per call.
        const bool bCanHealOutside = healsOutsideFriendlyTerritory();

        if (!bCanHealOutside && !GET_TEAM(getTeam()).isHasTech((TechTypes)iBattlefieldMedicine))
            {
                // Check if a healer unit is present on same tile or adjacent
                bool bHealerPresent = false;

                foreach_(CvUnit* pLoopUnit, pPlot->units())
                {
                    if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining())
                    {
                        if ((pLoopUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) > 0)
                        {
                            bHealerPresent = true;
                            break;
                        }
                    }
                }

                if (!bHealerPresent)
                {
                    foreach_(const CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
                    {
                        foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
                        {
                            if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining())
                            {
                                if ((pLoopUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) > 0)
                                {
                                    bHealerPresent = true;
                                    break;
                                }
                            }
                        }
                        if (bHealerPresent) break;
                    }
                }

                if (!bHealerPresent)
                {
                    return 0;
                }
            }
    }

	//Find what will take the longest to heal and use that rate
	std::vector<UnitCombatTypes> kHealAsTypes;
	healAsUnitCombats(kHealAsTypes);
	if (!kHealAsTypes.empty())
	{
		int iWorstNumTurns = -1;
		int iBestHeal = MAX_INT;
		for (int iI = (int)kHealAsTypes.size() - 1; iI > -1; iI--)
		{
			const UnitCombatTypes eHealAsType = kHealAsTypes[iI];
			const int iHealAsDamage = getHealAsDamage(eHealAsType);
			if (iHealAsDamage > 0)
			{
				const int iHealAs = getHealRateAsType(pPlot, bHealCheck, eHealAsType);
				const int iNumTurns = iHealAs > 0 ? iHealAsDamage / iHealAs : MAX_INT;

				//Note we're actually looking for the slowest to heal here to use that for the # of rounds to heal total
				if (iNumTurns > iWorstNumTurns)
				{
					iBestHeal = iHealAs;
					iWorstNumTurns = iNumTurns;
					if (iNumTurns == MAX_INT)
					{
						break;
					}
				}
			}
		}
		if (iWorstNumTurns > -1)
		{
			if (!hasNoSelfHeal())
			{
				return std::max(1, std::min(iBestHeal, getDamage()));
			}
			return std::max(0, std::min(iBestHeal, getDamage()));
		}
	}

	int iTotalHeal = 0;

	if (!hasNoSelfHeal() || getSelfHealModifierTotal() < 0)
	{
		iTotalHeal += getSelfHealModifierTotal();
	}

	if (pPlot->isCity(true, getTeam()))
	{
		iTotalHeal += GC.getCITY_HEAL_RATE() + (GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) ? (resolvedValue(URS_HEAL_FRIENDLY) / 100) : (resolvedValue(URS_HEAL_NEUTRAL) / 100));

		const CvCity* pCity = pPlot->getPlotCity();

		if (pCity && !pCity->isOccupation())
		{
			int aiHeals[NUM_HEAL_KINDS];
			pCity->getHealKinds(aiHeals);
			iTotalHeal += aiHeals[HEAL_RATE] / 100;
		}
	}
	else if (!hasNoSelfHeal())
	{
		if (!GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()))
		{
			if (isEnemy(pPlot->getTeam(), pPlot))
			{
				iTotalHeal += (GC.getENEMY_HEAL_RATE() + (resolvedValue(URS_HEAL_ENEMY) / 100));
			}
			else
			{
				iTotalHeal += (GC.getNEUTRAL_HEAL_RATE() + (resolvedValue(URS_HEAL_NEUTRAL) / 100));
			}
		}
		else
		{
			iTotalHeal += (GC.getFRIENDLY_HEAL_RATE() + (resolvedValue(URS_HEAL_FRIENDLY) / 100));
		}
	}
	CvUnit* pHealUnit = NULL;

	// XXX optimize this (save it?)
	int iBestHeal = 0;

	foreach_(CvUnit* pLoopUnit, pPlot->units())
	{
		if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining()) // XXX what about alliances?
		{
			const int iHeal = (pLoopUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100);

			if (iHeal > iBestHeal)
			{
				iBestHeal = iHeal;
				pHealUnit = pLoopUnit;
			}
		}
	}

	foreach_(const CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
	{
		foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
		{
			if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining()) // XXX what about alliances?
			{
				const int iHeal = (pLoopUnit->resolvedValue(URS_HEAL_ADJACENT) / 100);

				if (iHeal > iBestHeal)
				{
					iBestHeal = iHeal;
					pHealUnit = pLoopUnit;
				}
			}
		}
	}
	iTotalHeal += iBestHeal;

	if (pHealUnit != NULL && bHealCheck)
	{
		pHealUnit->changeHealSupportUsed(1);
		pHealUnit->changeExperience100((10));
	}

	if (!hasNoSelfHeal())
	{
		return std::max(1, iTotalHeal);
	}
	return std::max(0, iTotalHeal);
}

int CvUnit::getHealRateAsType(const CvPlot* pPlot, bool bHealCheck, UnitCombatTypes eHealAsType) const
{
	PROFILE_FUNC();
	{
		bool bIsValid = false;
		std::vector<UnitCombatTypes> kValidHealAsTypes;
		healAsUnitCombats(kValidHealAsTypes);
		for (size_t iHealAs = 0; iHealAs < kValidHealAsTypes.size(); ++iHealAs)
		{
			if (kValidHealAsTypes[iHealAs] == eHealAsType)
			{
				bIsValid = true;
				break;
			}
		}
		if (!bIsValid)
		{
			return MAX_INT;
		}
	}
	int iTotalHeal = 0;

	if (!hasNoSelfHeal() || (getSelfHealModifierTotal() < 0))
	{
		iTotalHeal += getSelfHealModifierTotal();
	}

	if (pPlot->isCity(true, getTeam()))
	{
		iTotalHeal += GC.getCITY_HEAL_RATE() + (GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) ? (resolvedValue(URS_HEAL_FRIENDLY) / 100) : (resolvedValue(URS_HEAL_NEUTRAL) / 100));

		const CvCity* pCity = pPlot->getPlotCity();

		if (pCity && !pCity->isOccupation())
		{
			int aiHeals[NUM_HEAL_KINDS];
			pCity->getHealKinds(aiHeals);
			iTotalHeal += (aiHeals[HEAL_RATE] / 100) + pCity->getHealUnitCombatTypeTotal(eHealAsType);
		}
	}
	else if (!hasNoSelfHeal())
	{
		if (GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()))
		{
			iTotalHeal += GC.getFRIENDLY_HEAL_RATE() + (resolvedValue(URS_HEAL_FRIENDLY) / 100);
		}
		else if (isEnemy(pPlot->getTeam(), pPlot))
		{
			iTotalHeal += GC.getENEMY_HEAL_RATE() + (resolvedValue(URS_HEAL_ENEMY) / 100);
		}
		else
		{
			iTotalHeal += GC.getNEUTRAL_HEAL_RATE() + (resolvedValue(URS_HEAL_NEUTRAL) / 100);
		}
	}

	// XXX optimize this (save it?)
	int iBestHeal = 0;
	CvUnit* pHealUnit = NULL;

	foreach_(CvUnit* pLoopUnit, pPlot->units())
	{
		if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining()) // XXX what about alliances?
		{
			const int iHeal = (pLoopUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) + pLoopUnit->getHealUnitCombatTypeTotal(eHealAsType);

			//if ((pLoopUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) > 0 || pLoopUnit->getHealUnitCombatTypeTotal(eHealAsType) > 0)
			//{
			//	iHeal += pLoopUnit->establishModifier();
			//}

			if (iHeal > iBestHeal)
			{
				iBestHeal = iHeal;
				pHealUnit = pLoopUnit;
			}
		}
	}
	foreach_(const CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
	{
		foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
		{
			if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining()) // XXX what about alliances?
			{
				const int iHeal = (pLoopUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) + pLoopUnit->getHealUnitCombatTypeAdjacentTotal(eHealAsType);

				//if ((pLoopUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) > 0 || pLoopUnit->getHealUnitCombatTypeAdjacentTotal(eHealAsType) > 0)
				//{
				//	iHeal += pLoopUnit->establishModifier();
				//}

				if (iHeal > iBestHeal)
				{
					iBestHeal = iHeal;
					pHealUnit = pLoopUnit;
				}
			}
		}
	}
	if (pHealUnit != NULL && bHealCheck)
	{
		pHealUnit->changeHealSupportUsed(1);
		// The divisor is the HEALER's own heal-as count (this unit's rows), non-zero by the validity gate above.
		std::vector<UnitCombatTypes> kHealerHealAsTypes;
		healAsUnitCombats(kHealerHealAsTypes);
		pHealUnit->changeExperience100(10 / (int)kHealerHealAsTypes.size());
	}
	iTotalHeal += iBestHeal;

	if (hasNoSelfHeal())
	{
		return std::max(0, iTotalHeal);
	}
	return std::max(1, iTotalHeal);
}


int CvUnit::healTurns(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	if (!isHurt())
	{
		return 0;
	}

	int iBattlefieldMedicine = GC.getInfoTypeForString("TECH_BATTLEFIELD_MEDICINE");
    if (!GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) && !isAnimal() && !isNPC())
    {
        // The self-recovery verdict is a BARE FETCH off the resolved HEAL block -- see healRate() above.
        const bool bCanHealOutside = healsOutsideFriendlyTerritory();

        if (!bCanHealOutside && !GET_TEAM(getTeam()).isHasTech((TechTypes)iBattlefieldMedicine))
        {
            bool bHealerPresent = false;

            foreach_(CvUnit* pLoopUnit, pPlot->units())
            {
                if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining())
                {
                    if ((pLoopUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) > 0)
                    {
                        bHealerPresent = true;
                        break;
                    }
                }
            }

            if (!bHealerPresent)
            {
                foreach_(const CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
                {
                    foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
                    {
                        if (pLoopUnit->getTeam() == getTeam() && pLoopUnit->hasHealSupportRemaining())
                        {
                            if ((pLoopUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) > 0)
                            {
                                bHealerPresent = true;
                                break;
                            }
                        }
                    }
                    if (bHealerPresent) break;
                }
            }

            if (!bHealerPresent)
            {
                return 0;
            }
        }
    }

	std::vector<UnitCombatTypes> kHealAsTypes;
	healAsUnitCombats(kHealAsTypes);
	const int iNumHealAs = (int)kHealAsTypes.size();

	//Find what will take the longest to heal and use that rate
	if (iNumHealAs > 0)
	{
		bool bNeedsHealing = false;
		int iBestNumTurns = 0;

		for (int iI = 0; iI < iNumHealAs; iI++)
		{
			const UnitCombatTypes eHealAsType = kHealAsTypes[iI];
			const int iHealDamage = getHealAsDamage(eHealAsType);
			if (iHealDamage > 0)
			{
				bNeedsHealing = true;
				const int iHealRate = getHealRateAsType(pPlot, false, eHealAsType);

				if (iHealRate > 0 && iHealRate < MAX_INT)
				{
					int iNumTurns = iHealDamage / iHealRate;
					if ((iHealDamage % iHealRate) != 0)
					{
						iNumTurns++;
					}
					//Note we're actually looking for the slowest to heal here to use that for the # of rounds to heal total
					if (iNumTurns > iBestNumTurns)
					{
						iBestNumTurns = iNumTurns;
					}
				}
			}
		}
		if (bNeedsHealing)
		{
			return iBestNumTurns;
		}
	}

	const int iHeal = healRate(pPlot);

	if (iHeal > 0)
	{
		int iTurns = getDamage() / iHeal;

		if ((getDamage() % iHeal) != 0)
		{
			iTurns++;
		}
		return iTurns;
	}
	return MAX_INT;
}

int CvUnit::healTurnsAsType(const CvPlot* pPlot, UnitCombatTypes eHealAsType) const
{
	const int iHealDamage = getHealAsDamage(eHealAsType);
	if (iHealDamage < 1)
	{
		return MAX_INT;
	}
	const int iHealAs = getHealRateAsType(pPlot, false, eHealAsType);
	if (iHealAs < 1)
	{
		return MAX_INT;
	}
	int iNumTurns = iHealDamage / iHealAs;

	if ((iHealDamage % iHealAs) != 0)
	{
		iNumTurns++;
	}
	return iNumTurns;
}

void CvUnit::doHeal()
{
	PROFILE_EXTRA_FUNC();
	UnitCombatTypes eHealAsType = NO_UNITCOMBAT;

	std::vector<UnitCombatTypes> kHealAsTypes;
	healAsUnitCombats(kHealAsTypes);
	if (!kHealAsTypes.empty())
	{
		for (size_t iHealAs = 0; iHealAs < kHealAsTypes.size(); ++iHealAs)
		{
			eHealAsType = kHealAsTypes[iHealAs];
			if (!isHasUnitCombat(eHealAsType))
			{
				setHasUnitCombat(eHealAsType, true);
				setHealAsDamage(eHealAsType, getDamage());
			}
			if (getHealAsDamage(eHealAsType) > 0)
			{
				changeHealAsDamage(eHealAsType,-(getHealRateAsType(plot(), true, eHealAsType)));
			}
		}
	}
	else
	{
		changeDamage(-(healRate(plot(), true)));
	}
	//TB Combat Mod begin
	//Note: to be re-evaluated!!!
	//TB Combat Mod end
}


bool CvUnit::canAirlift(const CvPlot* pPlot) const
{
	if (getDomainType() != DOMAIN_LAND || hasMoved())
	{
		return false;
	}
	const CvCity* pCity = pPlot->getPlotCity();

	return pCity && pCity->getCurrAirlift() < pCity->getMaxAirlift() && pCity->getTeam() == getTeam();
}


bool CvUnit::canAirliftAt(const CvPlot* pPlot, int iX, int iY) const
{
	if (!canAirlift(pPlot))
	{
		return false;
	}
	const CvPlot* pTargetPlot = GC.getMap().plot(iX, iY);

	if (!canEnterPlot(pTargetPlot))
	{
		return false;
	}

	// Super Forts begin *airlift*
	if (pTargetPlot->getTeam() != NO_TEAM
	&& (pTargetPlot->getTeam() == getTeam() || GET_TEAM(pTargetPlot->getTeam()).isVassal(getTeam()))
	&&  pTargetPlot->getImprovementType() != NO_IMPROVEMENT
	&& GC.getImprovementInfo(pTargetPlot->getImprovementType()).hasCharacteristic(CLS_CHARACTERISTIC_ACTS_AS_CITY))
	{
		return true;
	}
	// Super Forts end
	{
		const CvCity* pTargetCity = pTargetPlot->getPlotCity();

		if (pTargetCity == NULL || pTargetCity->isAirliftTargeted())
		{
			return false;
		}

		if (pTargetCity->getTeam() != getTeam() && !GET_TEAM(pTargetCity->getTeam()).isVassal(getTeam()))
		{
			return false;
		}
	}
	if (!GET_TEAM(getTeam()).isRebaseAnywhere()
	&& GC.getGame().isModderGameOption(MODDERGAMEOPTION_AIRLIFT_RANGE)
	&& plotDistance(pPlot->getX(), pPlot->getY(), iX, iY) > GC.getGame().getModderGameOption(MODDERGAMEOPTION_AIRLIFT_RANGE))
	{
		return false;
	}
	return true;
}


bool CvUnit::airlift(int iX, int iY)
{
	if (!canAirliftAt(plot(), iX, iY))
	{
		return false;
	}
	CvCity* pCity = plot()->getPlotCity();

	CvPlot* pTargetPlot = GC.getMap().plot(iX, iY);

	FAssert(pCity != NULL && pTargetPlot != NULL);

	// Super Forts begin *airlift* - added if statement to allow airlifts to plots that aren't cities
	if (pTargetPlot->isCity())
	{
		CvCity* pTargetCity = pTargetPlot->getPlotCity();

		FAssert(pTargetCity != NULL && pCity != pTargetCity);

		if (pTargetCity->getMaxAirlift() == 0)
		{
			pTargetCity->setAirliftTargeted(true);
		}
	}
	pCity->changeCurrAirlift(1);
	// Super Forts end

	finishMoves();

	//GC.getGame().logOOSSpecial(18, getID(), pTargetPlot->getX(), pTargetPlot->getY());
	setXY(pTargetPlot->getX(), pTargetPlot->getY());

	return true;
}


void CvUnit::nukeDiplomacy(bool* nukedTeams)
{
	const PlayerTypes eMyOwner = getOwner();
	const TeamTypes eMyTeam = getTeam();
	CvTeam& myTeam = GET_TEAM(eMyTeam);

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (nukedTeams[iI])
		{
			const TeamTypes eNukedTeam = static_cast<TeamTypes>(iI);
			CvTeam& nukedTeam = GET_TEAM(eNukedTeam);

			if (!isEnemy(eNukedTeam))
			{
				myTeam.declareWar(eNukedTeam, false, WARPLAN_TOTAL);
			}
			nukedTeam.changeWarWeariness(eMyTeam, 100 * GC.getDefineINT("WW_HIT_BY_NUKE"));
			myTeam.changeWarWeariness(eNukedTeam, 100 * GC.getDefineINT("WW_ATTACKED_WITH_NUKE"));
			myTeam.AI_changeWarSuccess(eNukedTeam, GC.getDefineINT("WAR_SUCCESS_NUKE"));

			for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
			{
				CvPlayerAI& playerX = GET_PLAYER((PlayerTypes)iJ);

				if (playerX.isAliveAndTeam(eNukedTeam))
				{
					playerX.AI_changeMemoryCount(eMyOwner, MEMORY_NUKED_US, 1);
				}
			}
			for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
			{
				// If we are hit oureself we don't get further insulted if anyone else is too.
				if (!nukedTeams[iJ] && iJ != eMyTeam)
				{
					const TeamTypes eTeamX = static_cast<TeamTypes>(iJ);

					if (GET_TEAM(eTeamX).isAlive())
					{
						if (GET_TEAM(eTeamX).isHasMet(eNukedTeam)
						&&  GET_TEAM(eTeamX).AI_getAttitude(eNukedTeam) >= ATTITUDE_CAUTIOUS)
						{
							for (int iK = 0; iK < MAX_PC_PLAYERS; iK++)
							{
								CvPlayerAI& playerX = GET_PLAYER((PlayerTypes)iK);

								if (playerX.isAliveAndTeam(eTeamX))
								{
									playerX.AI_changeMemoryCount(eMyOwner, MEMORY_NUKED_FRIEND, 1);
								}
							}
						}
						else
						{
							for (int iK = 0; iK < MAX_PC_PLAYERS; iK++)
							{
								CvPlayerAI& playerX = GET_PLAYER((PlayerTypes)iK);

								if (playerX.isAliveAndTeam(eTeamX)
								&&  playerX.AI_getMemoryCount(eMyOwner, MEMORY_NUKED_US) == 0
								&&  playerX.AI_getMemoryCount(eMyOwner, MEMORY_NUKED_FRIEND) == 0)
								{
									playerX.AI_changeMemoryCount(eMyOwner, MEMORY_USED_NUKE, 1);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool CvUnit::isNukeVictim(const CvPlot* pPlot, const TeamTypes eTeam, const int iRange) const
{
	PROFILE_EXTRA_FUNC();
	if (!GET_TEAM(eTeam).isAlive() || eTeam == getTeam())
	{
		return false;
	}

	foreach_(const CvPlot* plotX, pPlot->rect(iRange, iRange))
	{
		if (plotX->getTeam() == eTeam || plotX->plotCheck(PUF_isCombatTeam, eTeam, getTeam()))
		{
			return true;
		}
	}
	return false;
}

bool CvUnit::canNuke() const
{
	return nukeRange() > -1;
}

bool CvUnit::canNukeAt(const CvPlot* pPlot, int iX, int iY) const
{
	PROFILE_EXTRA_FUNC();
	if (!canNuke())
	{
		return false;
	}
	const int iNukeRange = nukeRange();
	{
		const int iDistance = plotDistance(pPlot->getX(), pPlot->getY(), iX, iY);

		if (iDistance <= nukeRange() || airRange() > 0 && iDistance > airRange())
		{
			return false;
		}
	}
	const CvPlot* nukePlot = GC.getMap().plot(iX, iY);
	const CvTeam& team = GET_TEAM(getTeam());

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (!team.isAtWar(static_cast<TeamTypes>(iI))
		&&  !team.canDeclareWar(static_cast<TeamTypes>(iI))
		&&  isNukeVictim(nukePlot, static_cast<TeamTypes>(iI), nukeRange()))
		{
			return false;
		}
	}
	return true;
}


// The MISSION_NUKE launcher. The strike itself is not resolved here: this arms the unit
// (setMadeAttack + setAttackPlot) and CvUnit::kill fires pTarget->nukeExplosion(nukeRange(), this)
// once the unit dies, so the explosion is a consequence of the death rather than of the order.
bool CvUnit::nuke(int iX, int iY)
{
	PROFILE_EXTRA_FUNC();

	if (!canNukeAt(plot(), iX, iY))
	{
		return false;
	}
	CvPlot* nukePlot = GC.getMap().plot(iX, iY);

	if (airBaseCombatStr() != 0 && interceptTest(nukePlot))
	{
		return true;
	}
	const PlayerTypes eMyOwner = getOwner();
	CvPlayerAI& myOwner = GET_PLAYER(eMyOwner);

	bool nukedTeams[MAX_PC_TEAMS];

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		nukedTeams[iI] = isNukeVictim(nukePlot, (TeamTypes)iI, nukeRange());
	}

	if (airBaseCombatStr() != 0)
	{
		setReconPlot(nukePlot);
	}

	// NUKE INTERCEPTION does not roll here: `combat.nukeInterception` is one of the trigger-plane set that
	// carries no kind and attaches to its trigger's own `chance` ([triggers.md]), so neither the team's
	// interception odds nor the warhead's evasion has a source until the curator lands them there.

	if (nukePlot->isActiveVisible(false) && !isUsingDummyEntities() && isInViewport())
	{
		if (airBaseCombatStr() != 0)
		{
			addMission(CvAirMissionDefinition(MISSION_AIRSTRIKE, nukePlot, this));

			if (GC.getInfoTypeForString("EFFECT_JETFIGHTER_NUKE_EXPLODE") != -1)
			{
				gDLL->getEngineIFace()->TriggerEffect((EffectTypes)GC.getInfoTypeForString("EFFECT_JETFIGHTER_NUKE_EXPLODE"), nukePlot->getPoint(), 0);
				gDLL->getInterfaceIFace()->playGeneralSound("AS2D_NUKE_EXPLODES", nukePlot->getPoint());
			}
		}
		else // the non-intercepted entity mission -- no defender
		{
			addMission(CvMissionDefinition(MISSION_NUKE, nukePlot, this));
		}
	}

	setMadeAttack(true);
	setAttackPlot(nukePlot, false);

	nukeDiplomacy(nukedTeams);

	const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_NUKE_LAUNCHED", myOwner.getNameKey(), getNameKey());

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			AddDLLMessage(
				(PlayerTypes)iI, iI == eMyOwner, GC.getEVENT_MESSAGE_TIME(),
				szBuffer, "AS2D_NUKE_EXPLODES", MESSAGE_TYPE_MAJOR_EVENT,
				getButton(), GC.getCOLOR_RED(), nukePlot->getX(), nukePlot->getY(), true, true
			);
		}
	}

	if (isSuicide())
	{
		kill(true);
	}
	return true;
}



bool CvUnit::canRecon() const
{
	if (getDomainType() != DOMAIN_AIR)
	{
		return false;
	}

	if (airRange() == 0)
	{
		return false;
	}

	if (getUnitInfo().hasSkill(CLS_SKILL_SUICIDE))
	{
		return false;
	}

	return true;
}



bool CvUnit::canReconAt(const CvPlot* pPlot, int iX, int iY) const
{
	if (!canRecon())
	{
		return false;
	}

	const int iDistance = plotDistance(pPlot->getX(), pPlot->getY(), iX, iY);

	if (iDistance > airRange() || 0 == iDistance)
	{
		return false;
	}
	return true;
}


bool CvUnit::recon(int iX, int iY)
{
	if (!canReconAt(plot(), iX, iY))
	{
		return false;
	}

	CvPlot* pPlot = GC.getMap().plot(iX, iY);

	setReconPlot(pPlot);
	finishMoves();

	if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP))
	{
		 setExperience100(getExperience100() + 5);
	}
	addMission(CvAirMissionDefinition(MISSION_RECON, pPlot, this, NULL));

	return true;
}


bool CvUnit::canParadrop(const CvPlot* pPlot) const
{
	if (getDropRange() <= 0 || hasMoved() || !pPlot->isFriendlyCity(*this, true))
	{
		return false;
	}
	return true;
}


bool CvUnit::canParadropAt(const CvPlot* fromPlot, int toX, int toY) const
{
	if (!canParadrop(fromPlot))
	{
		return false;
	}

	CvPlot* pTargetPlot = GC.getMap().plot(toX, toY);
	if (NULL == pTargetPlot || pTargetPlot == fromPlot)
	{
		return false;
	}

	if (!pTargetPlot->isVisible(getTeam(), false))
	{
		return false;
	}

	if (!canEnterPlot(pTargetPlot, MoveCheck::IgnoreLoad))
	{
		return false;
	}

	if (plotDistance(fromPlot->getX(), fromPlot->getY(), toX, toY) > getDropRange())
	{
		return false;
	}

	if (!canCoexistAlwaysOnPlot(*pTargetPlot) && pTargetPlot->isEnemyCity(*this))
	{
		return false;
	}

	if (pTargetPlot->isWater() && getDomainType() == DOMAIN_LAND)
	{
		return false;
	}

	return true;
}


bool CvUnit::paradrop(int iX, int iY)
{
	if (!canParadropAt(plot(), iX, iY))
	{
		return false;
	}
	CvPlot* pPlot = GC.getMap().plot(iX, iY);

	if (!isFreeDrop())
	{
		changeMoves(GC.getMOVE_DENOMINATOR() / 2);
		setMadeAttack(true);
	}

	//GC.getGame().logOOSSpecial(19, getID(), pPlot->getX(), pPlot->getY());
	setXY(pPlot->getX(), pPlot->getY());

	//check if intercepted
	if (interceptTest(pPlot))
	{
		return true;
	}

	if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP))
	{
		 setExperience100(getExperience100() + 5);
	}

	//play paradrop animation by itself
	addMission(CvAirMissionDefinition(MISSION_PARADROP, pPlot, this));

	return true;
}


bool CvUnit::canAirBomb() const
{
	if (getDomainType() != DOMAIN_AIR)
	{
		return false;
	}

	if (getAirBombBaseRate() == 0)
	{
		return false;
	}

	if (isMadeAttack())
	{
		return false;
	}
	return true;
}


bool CvUnit::canAirBombAt(const CvPlot* pPlot, int iX, int iY) const
{
	PROFILE_EXTRA_FUNC();
	if (!canAirBomb())
	{
		return false;
	}

	CvPlot* pTargetPlot = GC.getMap().plot(iX, iY);

	if (plotDistance(pPlot->getX(), pPlot->getY(), pTargetPlot->getX(), pTargetPlot->getY()) > airRange())
	{
		return false;
	}

	if (pTargetPlot->isOwned() && !potentialWarAction(pTargetPlot))
	{
		return false;
	}

	CvCity* pCity = pTargetPlot->getPlotCity();

	// An improvement on the target plot short-circuits to allowed: the only test that lived here was the DCM
	// sea-unit one, so with DCM air bombing gone the branch has nothing left to ask.
	if (pTargetPlot->getImprovementType() == NO_IMPROVEMENT)
	{
		if (pCity != NULL)
		{
			if (!pCity->isBombardable(this))
			{
				return false;
			}
		}
		// Toffer - Something is wrong here, this else if can never be true as we already established that there's no improvement here if first if fails.
		else if (!pTargetPlot->isImprovementDestructible())
		{
			return false;
		}
	}
	return true;
}


bool CvUnit::airBomb(int iX, int iY)
{
	PROFILE_EXTRA_FUNC();
	if (!canAirBombAt(plot(), iX, iY))
	{
		return false;
	}
	CvPlot* pPlot = GC.getMap().plot(iX, iY);

	if (!isEnemy(pPlot->getTeam()))
	{
		getGroup()->groupDeclareWar(pPlot, true);
	}

	if (!isEnemy(pPlot->getTeam()))
	{
		return false;
	}

	if (interceptTest(pPlot))
	{
		return true;
	}

	CvCity* pCity = pPlot->getPlotCity();

	if (pPlot->getImprovementType() != NO_IMPROVEMENT)
	{
		
		if (GC.getGame().getSorenRandNum(getAirBombCurrRate(), "Air Bomb - Offense")
			>=
			GC.getGame().getSorenRandNum(GC.getImprovementInfo(pPlot->getImprovementType()).getDefense(DEFENSE_AIR, CASC_SCOPE_PLOT) / 100, "Air Bomb - Defense"))
		{
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText(
					"TXT_KEY_MISC_YOU_UNIT_DESTROYED_IMP",
					getNameKey(), GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()
				),
				"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
			);
			if (pPlot->isOwned())
			{
				if (BARBARIAN_PLAYER != getVisualOwner(getTeam()))
				{
					AddDLLMessage(
						pPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText(
							"TXT_KEY_MISC_YOU_IMP_WAS_DESTROYED",
							GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide(),
							getNameKey(), getVisualCivAdjective(pPlot->getTeam())
						),
						"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true, true
					);
				}
				else
				{
					AddDLLMessage(
						pPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText(
							"TXT_KEY_MISC_YOU_IMP_WAS_DESTROYED_HIDDEN",
							GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide(), getNameKey()
						),
						"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true, true
					);
				}
			}
			pPlot->setImprovementType(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage());
		}
		else
		{
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText(
					"TXT_KEY_MISC_YOU_UNIT_FAIL_DESTROY_IMP",
					getNameKey(), GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()
				),
				"AS2D_BOMB_FAILS", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY()
			);
		}
	}
	else if (pCity != NULL)
	{
		pCity->changeDefenseModifier(-getAirBombCurrRate());
		AddDLLMessage(
			pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText(
				"TXT_KEY_MISC_YOU_DEFENSES_REDUCED_TO",
				pCity->getNameKey(), pCity->getDefenseModifier(false), getNameKey()
			),
			"AS2D_BOMBARDED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY(), true, true
		);
		AddDLLMessage(
			getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText(
				"TXT_KEY_MISC_ENEMY_DEFENSES_REDUCED_TO",
				getNameKey(), pCity->getNameKey(), pCity->getDefenseModifier(false)
			),
			"AS2D_BOMBARD", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pCity->getX(), pCity->getY()
		);
	
	}
	setReconPlot(pPlot);
	setMadeAttack(true);
	changeMoves(GC.getMOVE_DENOMINATOR());
	addMission(CvAirMissionDefinition(MISSION_AIRBOMB, pPlot, this));

	if (isSuicide())
	{
		kill(true);
	}
	else if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP))
	{
		setExperience100(getExperience100() + 25 + GC.getGame().getSorenRandNum(26, "Random Min XP"));
	}
	return true;
}


CvCity* CvUnit::bombardTarget(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = MAX_INT;
	CvCity* pBestCity = NULL;

	foreach_(const CvPlot* pLoopPlot, pPlot->adjacent())
	{
		CvCity* pLoopCity = pLoopPlot->getPlotCity();

		if (pLoopCity != NULL && pLoopCity->isBombardable(this))
		{
			int iValue = pLoopCity->getDefenseDamage();

			// always prefer cities we are at war with
			if (isEnemy(pLoopCity->getTeam(), pPlot))
			{
				iValue *= 128;
			}

			if (iValue < iBestValue)
			{
				iBestValue = iValue;
				pBestCity = pLoopCity;
			}
		}
	}

	return pBestCity;
}


// Super Forts begin *bombard*
CvPlot* CvUnit::bombardImprovementTarget(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = MAX_INT;
	CvPlot* pBestPlot = NULL;

	foreach_(CvPlot* pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::isBombardable(this)))
	{
		int iValue = pLoopPlot->getDefenseDamage();

		// always prefer cities we are at war with
		if (isEnemy(pLoopPlot->getTeam(), pPlot))
		{
			iValue *= 128;
		}

		if (iValue < iBestValue)
		{
			iBestValue = iValue;
			pBestPlot = pLoopPlot;
		}
	}

	return pBestPlot;
}
// Super Forts end

bool CvUnit::canBombard(const CvPlot* pPlot, bool bIgnoreHasAttacked) const
{
	if (getBombardRate() <= 0)
	{
		return false;
	}

	if (!bIgnoreHasAttacked && isMadeAttack())
	{
		return false;
	}

	if (isCargo())
	{
		return false;
	}

	// Super Forts begin *bombard*
	if (bombardTarget(pPlot) == NULL && bombardImprovementTarget(pPlot) == NULL)
	//if (bombardTarget(pPlot) == NULL) - Original Code
	// Super Forts end
	{
		return false;
	}

	return true;
}


bool CvUnit::bombard()
{
	CvPlot* pPlot = plot();
	if (!canBombard(pPlot))
	{
		return false;
	}

	CvCity* pBombardCity = bombardTarget(pPlot);
	// Super Forts begin *bombard*
	//FAssertMsg(pBombardCity != NULL, "BombardCity is not assigned a valid value"); - Removed for Super Forts

	CvPlot* pTargetPlot;
	//CvPlot* pTargetPlot = pBombardCity->plot(); - Original Code
	if(pBombardCity != NULL)
	{
		pTargetPlot = pBombardCity->plot();
	}
	else
	{
		pTargetPlot = bombardImprovementTarget(pPlot);
	}
	// Super Forts end

	// Dale - RB: Bug Fix (RevolutionDCM - just checks for a null value)
	if (pTargetPlot != NULL)
	{
		if (!isEnemy(pTargetPlot->getTeam()))
		{
			getGroup()->groupDeclareWar(pTargetPlot, true);
		}

		if (!isEnemy(pTargetPlot->getTeam()))
		{
			return false;
		}

		int iBombardModifier = 0;
		// Super Forts begin *bombard* *text*
		if(pBombardCity != NULL)
		{

			if (!ignoreBuildingDefense())
			{
				iBombardModifier -= pBombardCity->getBombardDefense();
			}

			pBombardCity->changeDefenseModifier(-(getBombardRate() * std::max(0, 100 + iBombardModifier)) / 100);

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_DEFENSES_IN_CITY_REDUCED_TO", pBombardCity->getNameKey(), pBombardCity->getDefenseModifier(false), GET_PLAYER(getOwner()).getNameKey());
			AddDLLMessage(pBombardCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARDED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pBombardCity->getX(), pBombardCity->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_REDUCE_CITY_DEFENSES", getNameKey(), pBombardCity->getNameKey(), pBombardCity->getDefenseModifier(false));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARD", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pBombardCity->getX(), pBombardCity->getY());
		}
		else
		{

			pTargetPlot->changeDefenseDamage(getBombardRate());

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_DEFENSES_IN_CITY_REDUCED_TO", GC.getImprovementInfo(pTargetPlot->getImprovementType()).getText(),
				(GC.getImprovementInfo(pTargetPlot->getImprovementType()).getDefense(DEFENSE_AMOUNT, CASC_SCOPE_PLOT)-pTargetPlot->getDefenseDamage()), GET_PLAYER(getOwner()).getNameKey());
			AddDLLMessage(pTargetPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARDED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pTargetPlot->getX(), pTargetPlot->getY(), true, true);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_REDUCE_CITY_DEFENSES", getNameKey(), GC.getImprovementInfo(pTargetPlot->getImprovementType()).getText(),
				(GC.getImprovementInfo(pTargetPlot->getImprovementType()).getDefense(DEFENSE_AMOUNT, CASC_SCOPE_PLOT)-pTargetPlot->getDefenseDamage()));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARD", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pTargetPlot->getX(), pTargetPlot->getY());
		}
		// Super Forts end

		changeExperience100(100, -1, true);
		setMadeAttack(true);
		changeMoves(GC.getMOVE_DENOMINATOR());

		if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP))
		{
			 setExperience100(getExperience100() + 1 + GC.getGame().getSorenRandNum(26, "Random Min XP"));
		}

		if (pPlot->isActiveVisible(false))
		{
			CvUnit *pDefender = pTargetPlot->getBestDefender(NO_PLAYER, getOwner(), this, true);

			// Bombard entity mission
			addMission(CvMissionDefinition(MISSION_BOMBARD, pTargetPlot, this, pDefender));
		}
	}
	return true;
}

bool CvUnit::canPillage(const CvPlot* pPlot) const
{
	if (pPlot == NULL || !getUnitInfo().hasSkill(CLS_SKILL_PILLAGE))
	{
		return false;
	}

	if (isOnlyDefensive() && !isAnimal())
	{
		// Toffer - This is an odd one, probably specific to the locust swarm unit.
		return false;
	}

	if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_NO_FRIENDLY_PILLAGING) && pPlot->getTeam() == getTeam())
	{
		return false;
	}

	if (isCargo())
	{
		return false;
	}

	if (pPlot->isCity())
	{
		return false;
	}

	if (pPlot->getImprovementType() == NO_IMPROVEMENT)
	{
		if (!pPlot->isRoute())
		{
			return false;
		}
	}

	if (pPlot->isOwned() && !potentialWarAction(pPlot)
	&& (pPlot->getImprovementType() == NO_IMPROVEMENT || pPlot->getOwner() != getOwner()))
	{
		return false;
	}

	if (!pPlot->isValidDomainForAction(*this))
	{
		return false;
	}

	return true;
}


bool CvUnit::pillage(const bool bAutoPillage)
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pPlot = plot();

	if (!canPillage(pPlot))
	{
		return false;
	}

	const PlayerTypes ePlayerPillaged = pPlot->getOwner();
	if (ePlayerPillaged != NO_PLAYER
	// We should not be calling this without declaring war first, so do not declare war here
	&& !isEnemy(pPlot->getTeam(), pPlot)
	&& (pPlot->getImprovementType() == NO_IMPROVEMENT || ePlayerPillaged != getOwner()))
	{
		return false;
	}

	if (pPlot->isWater())
	{
		CvUnit* pInterceptor = bestSeaPillageInterceptor(this, GC.getCOMBAT_DIE_SIDES() / 2);

		if (NULL != pInterceptor)
		{
			setMadeAttack(false);

			// We are formally the attacker here but really the DEFENDER, so this exchange allows no
			// withdrawal. A within-frame flag says so for the duration of the attack -- never a write to
			// the resolved value, which is a derived cache and must not be snapshot-and-restored
			// (superseded-ideas #19).
			m_bSuppressWithdrawal = true;
			attack(pInterceptor->plot());
			m_bSuppressWithdrawal = false;

			return false;
		}
	}
	CvPlayer& player = GET_PLAYER(getOwner());
	int iPillageGold = 0;
	ImprovementTypes eTempImprovement = NO_IMPROVEMENT;
	RouteTypes eTempRoute = NO_ROUTE;

	if (pPlot->getImprovementType() != NO_IMPROVEMENT)
	{
		eTempImprovement = pPlot->getImprovementType();

		if (ePlayerPillaged != NO_PLAYER && pPlot->getTeam() != getTeam())
		{
			// Use python to determine pillage amounts...
			iPillageGold = Cy::call<int>(PYGameModule, "doPillageGold", Cy::Args() << pPlot << this);

			if (iPillageGold > 0)
			{
				const int iInfluenceRatio =
				(
					GC.isIDW_ENABLED() && GC.isIDW_PILLAGE_INFLUENCE_ENABLED() && atWar(pPlot->getTeam(), getTeam())
					?
					doPillageInfluence()
					:
					0
				);
				iPillageGold += iPillageGold * getPillageChange() / 100;
				player.changeGold(iPillageGold);

				if (isHuman())
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText(
							"TXT_KEY_MISC_PLUNDERED_GOLD_FROM_IMP",
							iPillageGold, GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()
						)
						, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(),
						pPlot->getX(), pPlot->getY()
					);
				}
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
				{
					CommerceTypes eCommerce = (CommerceTypes)iI;
					switch (eCommerce)
					{
						case COMMERCE_GOLD:
						{
							if (isPillageMarauder())
							{
								player.changeGold(iPillageGold);
								pPlot->setImprovementType(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage());
								if (isHuman())
								{
									AddDLLMessage(
										getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
										gDLL->getText(
											"TXT_KEY_MISC_MARAUDERS_PLUNDERED_IMP",
											iPillageGold, GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()
										),
										"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(),
										pPlot->getX(), pPlot->getY()
									);
								}
								if (GET_PLAYER(ePlayerPillaged).isHumanPlayer())
								{
									AddDLLMessage(
										ePlayerPillaged, false, GC.getEVENT_MESSAGE_TIME(),
										gDLL->getText(
											"TXT_KEY_MISC_IMP_DESTROYED_BY_MARAUDERS",
											GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide(),
											getNameKey(), getVisualCivAdjective(pPlot->getTeam())
										),
										"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(),
										pPlot->getX(), pPlot->getY(), true, true
									);
								}
							}
							break;
						}
						case COMMERCE_RESEARCH:
						{
							if (isPillageResearch())
							{
								GET_TEAM(player.getTeam()).changeResearchProgress(player.getCurrentResearch(), iPillageGold, getOwner());
								if (isHuman())
								{
									AddDLLMessage(
										getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
										gDLL->getText(
											"TXT_KEY_MISC_PLUNDERED_RESEARCH_FROM_IMP",
											iPillageGold, GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()
										),
										"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(),
										pPlot->getX(), pPlot->getY()
									);
								}
							}
							break;
						}
						case COMMERCE_CULTURE:
						{
							break;
						}
						case COMMERCE_ESPIONAGE:
						{
							if (isPillageEspionage() && pPlot->getTeam() != NO_TEAM)
							{
								GET_TEAM(player.getTeam()).changeEspionagePointsAgainstTeam(pPlot->getTeam(), iPillageGold);
								if (isHuman())
								{
									AddDLLMessage(
										getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
										gDLL->getText(
											"TXT_KEY_MISC_PLUNDERED_ESPIONAGE_FROM_IMP",
											iPillageGold, GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()
										),
										"AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(),
										pPlot->getX(), pPlot->getY()
									);
								}
							}
							break;
						}
					}
				}
				if (GET_PLAYER(ePlayerPillaged).isHumanPlayer())
				{
					CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_IMP_DESTROYED", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide(), getNameKey(), getVisualCivAdjective(pPlot->getTeam()));

					if (iInfluenceRatio > 0)
					{
						szBuffer = szBuffer + CvString::format(" %s: -%.1f%%", gDLL->getText("TXT_KEY_TILE_INFLUENCE").GetCString(), ((float)iInfluenceRatio)/10);
					}
					AddDLLMessage(
						ePlayerPillaged, false, GC.getEVENT_MESSAGE_TIME(), szBuffer,
						"AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true, true
					);
				}
				//	A pillage implies a source of danger even if we can't see it
				GET_PLAYER(ePlayerPillaged).addPlotDangerSource(pPlot, 100);
			}
		}
		pPlot->setImprovementType(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage());
	}
	else if (pPlot->isRoute() && !bAutoPillage)
	{
		eTempRoute = pPlot->getRouteType();
		pPlot->setRouteType(NO_ROUTE, true); // XXX downgrade rail???

		// Afforess - Alert Player of Pillaged Routes
		if (ePlayerPillaged != NO_PLAYER)
		{
			// A pillage implies a source of danger even if we can't see it
			GET_PLAYER(ePlayerPillaged).addPlotDangerSource(pPlot, 100);

			if (GET_PLAYER(ePlayerPillaged).isHumanPlayer())
			{
				AddDLLMessage(
					pPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_IMP_DESTROYED",
						GC.getRouteInfo(eTempRoute).getTextKeyWide(), getNameKey(),
						getVisualCivAdjective(pPlot->getTeam())
					)
					, "AS2D_PILLAGED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(),
					pPlot->getX(), pPlot->getY(), true, true
				);
			}
		}
	}
	changeMoves(GC.getMOVE_DENOMINATOR());

	if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP))
	{
		setExperience100(getExperience100() + 10 + GC.getGame().getSorenRandNum(21, "Random Min XP"));
	}
	else if (iPillageGold > 0 && pPlot->getOwner() != getOwner())
	{
		changeExperience100(iPillageGold);
	}
	addMission(CvMissionDefinition(MISSION_PILLAGE, pPlot, this));

	if (eTempImprovement != NO_IMPROVEMENT || eTempRoute != NO_ROUTE)
	{
		CvEventReporter::getInstance().unitPillage(this, eTempImprovement, eTempRoute, getOwner());
	}

	return true;
}


bool CvUnit::canPlunder(const CvPlot* pPlot, bool bTestVisible) const
{
	if (getDomainType() != DOMAIN_SEA)
	{
		return false;
	}

	if (!getUnitInfo().hasSkill(CLS_SKILL_PILLAGE))
	{
		return false;
	}

	if (!pPlot->isWater() || pPlot->isFreshWater())
	{
		return false;
	}

	if (!pPlot->isValidDomainForAction(*this))
	{
		return false;
	}

	if (!bTestVisible && pPlot->getTeam() == getTeam())
	{
		return false;
	}

	return true;
}


bool CvUnit::plunder()
{
	if (!canPlunder(plot()))
	{
		return false;
	}
	setBlockading(true);
	finishMoves();

	return true;
}


void CvUnit::updatePlunder(int iChange, bool bUpdatePlotGroups)
{
	PROFILE_FUNC();

	const int iBlockadeRange = GC.getSHIP_BLOCKADE_RANGE();
	bool bChanged = false;

	if (bUpdatePlotGroups)
	{
		CvPlot::setDeferredPlotGroupRecalculationMode(true);
	}

	foreach_(CvPlot* pLoopPlot, plot()->rect(iBlockadeRange, iBlockadeRange))
	{
		if (!pLoopPlot->isWater() || pLoopPlot->area() != area())
		{
			continue;
		}
		const int iPathDist = GC.getMap().calculatePathDistance(plot(),pLoopPlot);

		/* BBAI NOTES:
		// There are rare issues where the path finder will return incorrect results for unknown reasons.
		// Seems to find a suboptimal path sometimes in partially repeatable circumstances.
		// The fix below is a hack to address the permanent one or two tile blockades which
		// would appear randomly, it should cause extra blockade clearing only very rarely.
		if (iPathDist > iBlockadeRange)
		{
			continue; // No blockading on other side of an isthmus
		}
		*/
		if (iPathDist < 0 || iPathDist > iBlockadeRange + 2)
		{
			continue;
		}
		for (int iTeam = 0; iTeam < MAX_TEAMS; ++iTeam)
		{
			if (isEnemy((TeamTypes)iTeam)
			&& (iPathDist <= iBlockadeRange || iChange == -1 && pLoopPlot->getBlockadedCount((TeamTypes)iTeam) > 0))
			{
				const bool bOldTradeNet = pLoopPlot->isTradeNetwork((TeamTypes)iTeam);

				pLoopPlot->changeBlockadedCount((TeamTypes)iTeam, iChange);

				if (bOldTradeNet != pLoopPlot->isTradeNetwork((TeamTypes)iTeam))
				{
					bChanged = true;
					if (bUpdatePlotGroups)
					{
						pLoopPlot->updatePlotGroup();
					}
				}
			}
		}
	}

	if (bChanged)
	{
		gDLL->getInterfaceIFace()->setDirty(BlockadedPlots_DIRTY_BIT, true);

		if (bUpdatePlotGroups)
		{
			CvPlot::setDeferredPlotGroupRecalculationMode(false);
		}
	}
}


int CvUnit::sabotageProb(const CvPlot* pPlot, ProbabilityTypes eProbStyle) const
{
	PROFILE_EXTRA_FUNC();
	if (!pPlot->isOwned())
	{
		return 40 + 50 * (eProbStyle != PROBABILITY_LOW);
	}
	const int iProb = 40 / (pPlot->plotCount(PUF_canDefend, -1, -1, NULL, NO_PLAYER, pPlot->getTeam()) + 1);

	if (eProbStyle == PROBABILITY_LOW)
	{
		return iProb;
	}
	if (eProbStyle == PROBABILITY_HIGH)
	{
		return iProb + 50;
	}
	int iCounterSpyCount = pPlot->plotCount(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());

	foreach_(const CvPlot* pLoopPlot, pPlot->adjacent())
	{
		iCounterSpyCount += pLoopPlot->plotCount(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());
	}
	return iProb + 50 / (iCounterSpyCount + 1);
}


bool CvUnit::canSabotage(const CvPlot* pPlot, bool bTestVisible) const
{
	if (!getUnitInfo().hasSkill(CLS_SKILL_SABOTAGE))
	{
		return false;
	}

	if (pPlot->getTeam() == getTeam() || pPlot->isCity())
	{
		return false;
	}

	if (pPlot->getImprovementType() == NO_IMPROVEMENT)
	{
		return false;
	}

	if (!bTestVisible && GET_PLAYER(getOwner()).getGold() < GC.getBASE_SPY_SABOTAGE_COST())
	{
		return false;
	}

	return true;
}


bool CvUnit::sabotage()
{
	if (!canSabotage(plot()))
	{
		return false;
	}

	CvPlot* pPlot = plot();

	GET_PLAYER(getOwner()).changeGold(-GC.getBASE_SPY_SABOTAGE_COST());

	if (GC.getGame().getSorenRandNum(100, "Spy: Sabotage") <= sabotageProb(pPlot))
	{
		pPlot->setImprovementType(GC.getImprovementInfo(pPlot->getImprovementType()).getImprovementPillage());

		finishMoves();

		const CvCity* pNearestCity = GC.getMap().findCity(pPlot->getX(), pPlot->getY(), pPlot->getOwner(), NO_TEAM, false);

		if (pNearestCity != NULL)
		{
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_SPY_SABOTAGED", getNameKey(), pNearestCity->getNameKey()),
				"AS2D_SABOTAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY()
			);
			if (pPlot->isOwned())
			{
				AddDLLMessage(
					pPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText("TXT_KEY_MISC_SABOTAGE_NEAR", pNearestCity->getNameKey()),
					"AS2D_SABOTAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(),
					pPlot->getX(), pPlot->getY(), true, true
				);
			}
		}

		if (pPlot->isActiveVisible(false))
		{
			NotifyEntity(MISSION_SABOTAGE);
		}
	}
	else // Caught red handed
	{
		if (plot()->isActiveVisible(false))
		{
			NotifyEntity(MISSION_SURRENDER);
		}

		if (pPlot->isOwned())
		{
			if (!isEnemy(pPlot->getTeam(), pPlot))
			{
				GET_PLAYER(pPlot->getOwner()).AI_changeMemoryCount(getOwner(), MEMORY_SPY_CAUGHT, 1);
			}
			AddDLLMessage(
				pPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText(
					"TXT_KEY_MISC_SPY_CAUGHT_AND_KILLED",
					GET_PLAYER(getOwner()).getCivilizationAdjective(), getNameKey()
				),
				"AS2D_EXPOSE", MESSAGE_TYPE_INFO
			);
		}
		{
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_YOUR_SPY_CAUGHT", getNameKey()),
				"AS2D_EXPOSED", MESSAGE_TYPE_INFO
			);
		}

		kill(true, pPlot->getOwner(), true);
	}

	return true;
}


int CvUnit::destroyCost(const CvPlot* pPlot) const
{
	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return 0;
	}
	bool bLimited = false;

	if (pCity->isProductionUnit())
	{
		bLimited = isLimitedUnit(pCity->getProductionUnit());
	}
	else if (pCity->isProductionBuilding())
	{
		bLimited = isLimitedWonder(pCity->getProductionBuilding());
	}
	else if (pCity->isProductionProject())
	{
		bLimited = isLimitedProject(pCity->getProductionProject());
	}
	return
	(
		GC.getDefineINT("BASE_SPY_DESTROY_COST")
		+
		pCity->getProductionProgress()
		*
		(
			bLimited
			?
			GC.getDefineINT("SPY_DESTROY_COST_MULTIPLIER_LIMITED")
			:
			GC.getDefineINT("SPY_DESTROY_COST_MULTIPLIER")
		)
	);
}


int CvUnit::destroyProb(const CvPlot* pPlot, ProbabilityTypes eProbStyle) const
{
	PROFILE_EXTRA_FUNC();
	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return 0;
	}
	int iProb = 25 / (pPlot->plotCount(PUF_canDefend, -1, -1, NULL, NO_PLAYER, pPlot->getTeam()) + 1);

	if (eProbStyle != PROBABILITY_LOW)
	{
		if (eProbStyle != PROBABILITY_HIGH)
		{
			int iCounterSpyCount = pPlot->plotCount(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());

			foreach_(const CvPlot* pLoopPlot, pPlot->adjacent())
			{
				iCounterSpyCount += pLoopPlot->plotCount(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());
			}
			iProb += 50 / (iCounterSpyCount + 1);
		}
		else iProb += 50;
	}
	return iProb + std::min(25, pCity->getProductionTurnsLeft());
}


bool CvUnit::canDestroy(const CvPlot* pPlot, bool bTestVisible) const
{
	if (!getUnitInfo().hasSkill(CLS_SKILL_DESTROY))
	{
		return false;
	}

	if (pPlot->getTeam() == getTeam())
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL || pCity->getProductionProgress() == 0)
	{
		return false;
	}

	if (!bTestVisible && GET_PLAYER(getOwner()).getGold() < destroyCost(pPlot))
	{
		return false;
	}

	return true;
}


bool CvUnit::destroy()
{
	CvCity* pCity;
	CvWString szBuffer;
	bool bCaught;

	if (!canDestroy(plot()))
	{
		return false;
	}

	bCaught = (GC.getGame().getSorenRandNum(100, "Spy: Destroy") > destroyProb(plot()));

	pCity = plot()->getPlotCity();
	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	GET_PLAYER(getOwner()).changeGold(-(destroyCost(plot())));

	if (!bCaught)
	{
		pCity->setProductionProgress(pCity->getProductionProgress() / 2);

		finishMoves();

		{

			szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_DESTROYED_PRODUCTION", getNameKey(), pCity->getProductionNameKey(), pCity->getNameKey());
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_DESTROY", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pCity->getX(), pCity->getY());

			szBuffer = gDLL->getText("TXT_KEY_MISC_CITY_PRODUCTION_DESTROYED", pCity->getProductionNameKey(), pCity->getNameKey());
			AddDLLMessage(pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_DESTROY", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY(), true, true);
		}

		if (plot()->isActiveVisible(false))
		{
			NotifyEntity(MISSION_DESTROY);
		}
		if (!isSpy())
		{
			changeExperience100(100);
		}
	}
	else
	{
		if (isSpy())
		{

			szBuffer = gDLL->getText("TXT_KEY_MISC_SPY_CAUGHT_AND_KILLED", GET_PLAYER(getOwner()).getCivilizationAdjective(), getNameKey());
			AddDLLMessage(pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_SPY_CAUGHT", getNameKey());
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO);

			if (plot()->isActiveVisible(false))
			{
				NotifyEntity(MISSION_SURRENDER);
			}

			kill(true, pCity->getOwner(), true);
		}
		else
		{

			szBuffer = gDLL->getText("TXT_KEY_MISC_CRIMINAL_CAUGHT", GET_PLAYER(getOwner()).getCivilizationAdjective(), getNameKey());
			AddDLLMessage(pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO);

			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_CRIMINAL_CAUGHT", getNameKey());
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO);

			makeWanted(pCity);
		}


		if (!isEnemy(pCity->getTeam()))
		{
			GET_PLAYER(pCity->getOwner()).AI_changeMemoryCount(getOwner(), MEMORY_SPY_CAUGHT, 1);
		}
	}

	return true;
}


int CvUnit::stealPlansCost(const CvPlot* pPlot) const
{
	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return 0;
	}

	return (GC.getDefineINT("BASE_SPY_STEAL_PLANS_COST") + ((GET_TEAM(pCity->getTeam()).getTotalLand() + GET_TEAM(pCity->getTeam()).getTotalPopulation()) * GC.getDefineINT("SPY_STEAL_PLANS_COST_MULTIPLIER")));
}


// XXX compare with destroy prob...
int CvUnit::stealPlansProb(const CvPlot* pPlot, ProbabilityTypes eProbStyle) const
{
	PROFILE_EXTRA_FUNC();
	const CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL)
	{
		return 0;
	}

	int iProb = ((pCity->isGovernmentCenter()) ? 20 : 0); // XXX

	const int iDefenseCount = pPlot->plotCount(PUF_canDefend, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());

	int iCounterSpyCount = pPlot->plotCount(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());

	foreach_(const CvPlot* pLoopPlot, pPlot->adjacent())
	{
		iCounterSpyCount += pLoopPlot->plotCount(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, pPlot->getTeam());
	}

	if (eProbStyle == PROBABILITY_HIGH)
	{
		iCounterSpyCount = 0;
	}

	iProb += (20 / (iDefenseCount + 1)); // XXX

	if (eProbStyle != PROBABILITY_LOW)
	{
		iProb += (50 / (iCounterSpyCount + 1)); // XXX
	}

	return iProb;
}


bool CvUnit::canStealPlans(const CvPlot* pPlot, bool bTestVisible) const
{
	if (!(getUnitInfo().hasSkill(CLS_SKILL_STEAL_PLANS)))
	{
		return false;
	}

	if (pPlot->getTeam() == getTeam() || pPlot->isNPC())
	{
		return false;
	}

	if (isNPC())
	{
		return false;
	}

	if (pPlot->getPlotCity() == NULL)
	{
		return false;
	}

	if (!bTestVisible && GET_PLAYER(getOwner()).getGold() < stealPlansCost(pPlot))
	{
		return false;
	}

	return true;
}


bool CvUnit::stealPlans()
{
	if (!canStealPlans(plot()))
	{
		return false;
	}
	const CvCity* pCity = plot()->getPlotCity();
	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	GET_PLAYER(getOwner()).changeGold(-stealPlansCost(plot()));

	if (GC.getGame().getSorenRandNum(100, "Spy: Steal Plans") <= stealPlansProb(plot()))
	{
		GET_TEAM(getTeam()).changeStolenVisibilityTimer(pCity->getTeam(), 2);

		finishMoves();

		AddDLLMessage(
			getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText("TXT_KEY_MISC_SPY_STOLE_PLANS", getNameKey(), pCity->getNameKey()),
			"AS2D_STEALPLANS", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pCity->getX(), pCity->getY()
		);
		AddDLLMessage(
			pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText("TXT_KEY_MISC_PLANS_STOLEN", pCity->getNameKey()),
			"AS2D_STEALPLANS", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY(), true, true
		);

		if (plot()->isActiveVisible(false))
		{
			NotifyEntity(MISSION_STEAL_PLANS);
		}
		if (!isSpy())
		{
			changeExperience100(100);
		}

	}
	else
	{
		if (isSpy())
		{
			AddDLLMessage(
				pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_SPY_CAUGHT_AND_KILLED", GET_PLAYER(getOwner()).getCivilizationAdjective(), getNameKey()),
				"AS2D_EXPOSE", MESSAGE_TYPE_INFO
			);
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_YOUR_SPY_CAUGHT", getNameKey()),
				"AS2D_EXPOSED", MESSAGE_TYPE_INFO
			);

			if (plot()->isActiveVisible(false))
			{
				NotifyEntity(MISSION_SURRENDER);
			}
			kill(true, pCity->getOwner(), true);
		}
		else
		{
			AddDLLMessage(
				pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_CRIMINAL_CAUGHT", GET_PLAYER(getOwner()).getCivilizationAdjective(), getNameKey()),
				"AS2D_EXPOSE", MESSAGE_TYPE_INFO
			);
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText("TXT_KEY_MISC_YOUR_CRIMINAL_CAUGHT", getNameKey()),
				"AS2D_EXPOSED", MESSAGE_TYPE_INFO
			);
			makeWanted(pCity);
		}

		if (!isEnemy(pCity->getTeam()))
		{
			GET_PLAYER(pCity->getOwner()).AI_changeMemoryCount(getOwner(), MEMORY_SPY_CAUGHT, 1);
		}
	}

	return true;
}


bool CvUnit::canFound(const CvPlot* pPlot, bool bTestVisible) const
{
	return isFound() && GET_PLAYER(getOwner()).canFound(pPlot->getX(), pPlot->getY(), bTestVisible);
}


bool CvUnit::found()
{
	CvPlot* pPlot = plot();

	if (!pPlot || !canFound(pPlot))
	{
		return false;
	}

	if (GC.getGame().getActivePlayer() == getOwner())
	{
		GC.getCurrentViewport()->bringIntoView(getX(), getY());
	}

	GET_PLAYER(getOwner()).found(getX(), getY(), this);

	if (pPlot->isActiveVisible(false))
	{
		NotifyEntity(MISSION_FOUND);
	}

	// For the AI we need to run the turn for the new city to get production set
	if (!GET_PLAYER(getOwner()).isHumanPlayer())
	{
		pPlot->getPlotCity()->doTurn();
	}

	kill(true, NO_PLAYER, true);

	return true;
}


bool CvUnit::canSpread(const CvPlot* pPlot, ReligionTypes eReligion, bool bTestVisible) const
{
	PROFILE_FUNC();

	if (eReligion == NO_RELIGION || m_pUnitInfo->getReligionSpreadStrength(eReligion) <= 0)
	{
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL || pCity->isHasReligion(eReligion))
	{
		return false;
	}

	if (!canEnterArea(pPlot->getTeam(), pPlot->area()))
	{
		return false;
	}

	if (!bTestVisible && pCity->getTeam() != getTeam()
	&& GET_PLAYER(pCity->getOwner()).isNoNonStateReligionSpread()
	&& eReligion != GET_PLAYER(pCity->getOwner()).getStateReligion())
	{
		return false;
	}

	// TB Prophet Mod
	if (AI_getUnitAIType() != UNITAI_MISSIONARY)
	{
		if (!GC.getGame().isOption(GAMEOPTION_RELIGION_DIVINE_PROPHETS)
		|| GC.getGame().isOption(GAMEOPTION_RELIGION_LIMITED) && GET_PLAYER(getOwner()).hasHolyCity())
		{
			return false;
		}

		if (!GC.getGame().isOption(GAMEOPTION_RELIGION_PICK))
		{
			const TechTypes ePreqTech = GC.getReligionInfo(eReligion).getTechPrereq();

			if (!GC.getGame().isTechDiscovered(ePreqTech)
			|| !GET_TEAM(getTeam()).isHasTech(ePreqTech)
			&& GC.getGame().getGameTurn() <= GC.getGame().getTechGameTurnDiscovered(ePreqTech) + 1)
			{
				return false;
			}
		}
	}
	// ! TB Prophet Mod

	return true;
}


bool CvUnit::spread(ReligionTypes eReligion)
{
	if (!canSpread(plot(), eReligion))
	{
		return false;
	}
	CvCity* pCity = plot()->getPlotCity();

	if (pCity != NULL)
	{
		if (GC.getGame().isReligionFounded(eReligion))
		{
			int iSpreadProb = m_pUnitInfo->getReligionSpreadStrength(eReligion);

			int aiStateReligion[NUM_STATE_RELIGION_KINDS];
			GET_PLAYER(getOwner()).getStateReligionKinds(aiStateReligion);
			if ((ReligionTypes)GET_PLAYER(getOwner()).getStateReligion() == eReligion)
			{
				iSpreadProb += aiStateReligion[STATE_RELIGION_SPREAD_PROBABILITY];
			}
			else iSpreadProb += aiStateReligion[STATE_RELIGION_NON_STATE_SPREAD_PROBABILITY];

			if (pCity->getTeam() != getTeam())
			{
				iSpreadProb /= 2;
			}

			iSpreadProb += (GC.getNumReligionInfos() - pCity->getReligionCount()) * (100 - iSpreadProb) / GC.getNumReligionInfos();
			const bool bSuccess = GC.getGame().getSorenRandNum(100, "Unit Spread Religion") < iSpreadProb;

			// Python Event
			CvEventReporter::getInstance().unitSpreadReligionAttempt(this, eReligion, bSuccess);

			if (!bSuccess)
			{
				// Python event above may have spread the religion, it's fine if it did.
				if (!pCity->isHasReligion(eReligion))
				{
					AddDLLMessage(
						getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText(
							"TXT_KEY_MISC_RELIGION_FAILED_TO_SPREAD",
							getNameKey(),
							GC.getReligionInfo(eReligion).getChar(),
							pCity->getNameKey()
						),
						"AS2D_NOSPREAD", MESSAGE_TYPE_INFO, getButton(),
						GC.getCOLOR_RED(), pCity->getX(), pCity->getY()
					);
				}
			}
			else pCity->setHasReligion(eReligion, true, true, false);
		}
		else // Divine Prophet is founding religion here; always 100% chance.
		{
			GC.getGame().setHolyCity(eReligion, pCity, true);
			GC.getGame().setReligionSlotTaken(eReligion, true);
			pCity->setHasReligion(eReligion, true, true, false);
		}
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_SPREAD);
	}

	kill(true, NO_PLAYER, true);

	return true;
}


bool CvUnit::canSpreadCorporation(const CvPlot* pPlot, CorporationTypes eCorporation, bool bTestVisible) const
{
	PROFILE_EXTRA_FUNC();
	if (NO_CORPORATION == eCorporation)
	{
		return false;
	}

	if (!GET_PLAYER(getOwner()).isActiveCorporation(eCorporation))
	{
		return false;
	}

	if (m_pUnitInfo->getCorporationSpreadStrength(eCorporation) <= 0)
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();

	if (NULL == pCity)
	{
		return false;
	}

	if (pCity->isHasCorporation(eCorporation))
	{
		return false;
	}
/************************************************************************************************/
/* Afforess	                  Start		 01/17/10                                               */
/*                                                                                              */
/*   Blocks obsolete Corps from spreading                                                       */
/************************************************************************************************/
	if (GC.getCorporationInfo(eCorporation).getObsoleteTech() != NO_TECH)
	{
		if (GET_TEAM(GET_PLAYER(pCity->getOwner()).getTeam()).isHasTech(GC.getCorporationInfo(eCorporation).getObsoleteTech()))
		{
			return false;
		}
	}
	if (GC.getGame().isOption(GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS))
	{
		if (!GC.getGame().isModderGameOption(MODDERGAMEOPTION_NO_AUTO_CORPORATION_FOUNDING))
		{
			return false;
		}
	}
	if (!EnablerKernel::everAvailable(EDGEB_CORPORATIONS, (int)eCorporation))
	{
		return false;
	}
	if (!bTestVisible)
	{
		// The corp names the handful of buildings its spread needs; ask it for those rather than walking
		// every building id asking whether this is one of them.
		const std::map<int, int>& spreadNeeds = GC.getCorporationInfo(eCorporation).getSpreadBuildingCounts();
		for (std::map<int, int>::const_iterator needIt = spreadNeeds.begin(); needIt != spreadNeeds.end(); ++needIt)
		{
			if (GET_PLAYER(pCity->getOwner()).getBuildingCount((BuildingTypes)needIt->first) < needIt->second)
			{
				return false;
			}
		}
	}
/************************************************************************************************/
/* Afforess	                     END                                                            */
/************************************************************************************************/

	if (!canEnterArea(pPlot->getTeam(), pPlot->area()))
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (!GET_PLAYER(pCity->getOwner()).isActiveCorporation(eCorporation))
		{
			return false;
		}

		for (int iCorporation = 0; iCorporation < GC.getNumCorporationInfos(); ++iCorporation)
		{
			if (pCity->isHeadquarters((CorporationTypes)iCorporation))
			{
				if (GC.getGame().isCompetingCorporation((CorporationTypes)iCorporation, eCorporation))
				{
					return false;
				}
			}
		}
		// Afforess: Some corporations don't require any resources...
		bool bValid = false;
		bool bRequiresBonus = false;
		foreach_(const int iBonus, GC.getCorporationInfo(eCorporation).getConsumedBonuses())
		{
			bRequiresBonus = true;
			if (pCity->hasBonus((BonusTypes)iBonus))
			{
				bValid = true;
				break;
			}
		}
		if (!bValid && bRequiresBonus)
		{
			return false;
		}

		if (GET_PLAYER(getOwner()).getGold() < spreadCorporationCost(eCorporation, pCity))
		{
			return false;
		}
	}

	return true;
}

int CvUnit::spreadCorporationCost(CorporationTypes eCorporation, const CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	int iCost = std::max(0, GC.getCorporationInfo(eCorporation).getSpreadCost());

	if (pCity)
	{
		if (getTeam() != pCity->getTeam() && !GET_TEAM(pCity->getTeam()).isVassal(getTeam()))
		{
			iCost *= GC.getCORPORATION_FOREIGN_SPREAD_COST_PERCENT();
			iCost /= 100;
		}

		for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
		{
			if (iCorp != eCorporation && pCity->isActiveCorporation((CorporationTypes)iCorp)
			&& GC.getGame().isCompetingCorporation(eCorporation, (CorporationTypes)iCorp))
			{
				iCost *= 100 + GC.getCorporationInfo((CorporationTypes)iCorp).getSpreadFactor();
				iCost /= 100;
			}
		}
	}
	return iCost;
}

bool CvUnit::spreadCorporation(CorporationTypes eCorporation)
{
	int iSpreadProb;

	if (!canSpreadCorporation(plot(), eCorporation))
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (NULL != pCity)
	{
		GET_PLAYER(getOwner()).changeGold(-spreadCorporationCost(eCorporation, pCity));

		iSpreadProb = m_pUnitInfo->getCorporationSpreadStrength(eCorporation);

		if (pCity->getTeam() != getTeam())
		{
			iSpreadProb /= 2;
		}

		iSpreadProb += (((GC.getNumCorporationInfos() - pCity->getCorporationCount()) * (100 - iSpreadProb)) / GC.getNumCorporationInfos());

		if (GC.getGame().getSorenRandNum(100, "Unit Spread Corporation") < iSpreadProb)
		{
			pCity->setHasCorporation(eCorporation, true, true, false);
		}
		else
		{

			const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_CORPORATION_FAILED_TO_SPREAD", getNameKey(), GC.getCorporationInfo(eCorporation).getChar(), pCity->getNameKey());
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_NOSPREAD", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY());
		}
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_SPREAD_CORPORATION);
	}

	kill(true, NO_PLAYER, true);

	return true;
}


bool CvUnit::canJoin(const CvPlot* pPlot, SpecialistTypes eSpecialist) const
{
	if (eSpecialist == NO_SPECIALIST)
	{
		return false;
	}

	if (isCommander() || isDelayedDeath())
	{
		return false;
	}

    if (isCommodore() || isDelayedDeath())
	{
		return false;
	}

	if (!m_pUnitInfo->grantsGreatPerson(eSpecialist))
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL || pCity->getTeam() != getTeam())
	{
		return false;
	}

	return true;
}


bool CvUnit::join(SpecialistTypes eSpecialist)
{
	CvCity* pCity;

	if (!canJoin(plot(), eSpecialist))
	{
		return false;
	}

	pCity = plot()->getPlotCity();

	if (pCity != NULL)
	{
		pCity->changeFreeSpecialistCount(eSpecialist, 1, true);
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_JOIN);
	}

	getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	kill(true, NO_PLAYER, true);

	return true;
}


bool CvUnit::canConstruct(const CvPlot* pPlot, BuildingTypes eBuilding, bool bTestVisible) const
{
	if (eBuilding == NO_BUILDING || !m_pUnitInfo->grantsBuilding(eBuilding))
	{
		return false;
	}

	if (isDelayedDeath() || isCommander())
	{
		return false;
	}

	if (isDelayedDeath() || isCommodore())
	{
		return false;
	}

	if (!canPerformActionSM())
	{
		return false;
	}

	// "a SHRINE that cannot be built directly, and one already exists" -- the shrine relationship is the
	// building's own §9 `shrine` FK, and the cost sentinel moved: the legacy field used -1 for not-buildable
	// while the curated `cost.production` is simply ABSENT there (no building authors a negative cost).
	if (GC.getBuildingInfo(eBuilding).getShrineReligion() > NO_RELIGION
	&& GC.getBuildingInfo(eBuilding).getCost() <= 0
	&& GC.getGame().getBuildingCreatedCount(eBuilding) > 0)
	{
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();

	if (!pCity || getTeam() != pCity->getTeam())
	{
		return false;
	}

	if (pCity->hasBuilding(eBuilding))
	{
		return false;
	}

	if (pCity->getBuildingAvailability(eBuilding) < (bTestVisible ? EnablerDomain::STATE_GREYED : EnablerDomain::STATE_LISTED))
	{
		return false;
	}

	return true;
}


bool CvUnit::construct(BuildingTypes eBuilding)
{
	if (!canConstruct(plot(), eBuilding))
	{
		return false;
	}
	CvCity* pCity = plot()->getPlotCity();

	if (pCity)
	{
		pCity->changeHasBuilding(eBuilding, true);

		CvEventReporter::getInstance().buildingBuilt(pCity, eBuilding);
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_CONSTRUCT);
	}

	getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	kill(true, NO_PLAYER, true);
	return true;
}

bool CvUnit::canAddHeritage(const CvPlot* pPlot, const HeritageTypes eType, const bool bTestVisible) const
{
	if (eType == NO_HERITAGE || !vectorHas(m_pUnitInfo->getHeritage(), eType))
	{
		return false;
	}

	if (isDelayedDeath() || isCommander() || !canPerformActionSM())
	{
		return false;
	}

    if (isDelayedDeath() || isCommodore() || !canPerformActionSM())
	{
		return false;
	}

	if (!GET_PLAYER(getOwner()).canAddHeritage(eType, bTestVisible))
	{
		return false;
	}

	CvCity* pCity = pPlot->getPlotCity();

	if (!pCity || getTeam() != pCity->getTeam())
	{
		return false;
	}

	return true;
}

bool CvUnit::addHeritage(const HeritageTypes eType)
{
	if (!canAddHeritage(plot(), eType))
	{
		return false;
	}
	GET_PLAYER(getOwner()).setHeritage(eType, true);

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_HERITAGE);
	}
	getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	kill(true, NO_PLAYER, true);
	return true;
}


TechTypes CvUnit::getDiscoveryTech() const
{
	return ::getDiscoveryTech(getUnitType(), getOwner());
}


int CvUnit::getDiscoverResearch(const TechTypes eTech) const
{
	int iResearch = (
		m_pUnitInfo->getDiscoverBase() +
		m_pUnitInfo->getDiscoverMultiplier() * GET_TEAM(getTeam()).getTotalPopulation()
	);
	if (iResearch > 0)
	{
		iResearch *= CvGameSpeedScale::speedPercent();
		iResearch /= 100;

		if (eTech != NO_TECH)
		{
			return std::min(GET_TEAM(getTeam()).getResearchLeft(eTech), iResearch);
		}
		return iResearch;
	}
	return 0;
}


bool CvUnit::canDiscover() const
{
	if (isDelayedDeath())
	{
		return false;
	}
	if (getDiscoverResearch() == 0)
	{
		return false;
	}
	if (getDiscoveryTech() == NO_TECH)
	{
		return false;
	}
	return true;
}


bool CvUnit::discover(TechTypes eTech)
{
	if (!canDiscover())
	{
		return false;
	}
	if (eTech == NO_TECH)
	{
		eTech = getDiscoveryTech();
	}
	GET_TEAM(getTeam()).changeResearchProgress(eTech, getDiscoverResearch(eTech), getOwner());

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_DISCOVER);
	}
	getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	kill(true, NO_PLAYER, true);

	return true;
}


int CvUnit::getMaxHurryProduction(const CvCity* pCity) const
{
	int iProduction = (m_pUnitInfo->getHurryBase() + (m_pUnitInfo->getHurryMultiplier() * pCity->getPopulation()));

	iProduction *= CvGameSpeedScale::hammerCostPercent();
	iProduction /= 100;

	return std::max(0, iProduction);
}


int CvUnit::getHurryProduction(const CvPlot* pPlot) const
{
	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return 0;
	}

	int iProduction = std::min(pCity->productionLeft(), getMaxHurryProduction(pCity));

	return std::max(0, iProduction);
}


bool CvUnit::canHurry(const CvPlot* pPlot, bool bTestVisible) const
{
	if (isDelayedDeath() || getHurryProduction(pPlot) == 0)
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL || getTeam() != pCity->getTeam())
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (!(pCity->isProductionBuilding()))
		{
			return false;
		}
	}

	return true;
}


bool CvUnit::hurry()
{
	if (!canHurry(plot()))
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity != NULL)
	{
		pCity->changeProduction(getHurryProduction(plot()));
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_HURRY);
	}

	kill(true, NO_PLAYER, true);

	return true;
}


int CvUnit::getTradeGold(const CvPlot* pPlot) const
{
	CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL || pCity == getCityOfOrigin())
	{
		return 0;
	}
	CvCity* pCapitalCity = GET_PLAYER(pPlot->getOwner()).getCapitalCity();

	int iMult = m_pUnitInfo->getTradeMultiplier();

	// the route profit is ×100, so it reduces once here -- after the multiplier, not before it
	int iGold = m_pUnitInfo->getTradeBase() + ((pCapitalCity != NULL) ? iMult * pCity->calculateTradeProfit(pCapitalCity) / 100 : 0);

	iGold *= (pPlot->getOwner() != getOwner() ? iMult : 1);

	iGold *= pCity->getPopulation();
	iGold /= 10;

	CvPlot* cPlot = GC.getMap().plot(m_iXOrigin, m_iYOrigin);
	int iMaxDistance = GC.getMap().maxPlotDistance();
	if (cPlot != NULL)
	{
		iGold *= iMaxDistance + plotDistance(m_iXOrigin, m_iYOrigin, pPlot->getX(), pPlot->getY());
		iGold /= iMaxDistance;
	}

	// tradeMission is authored at EMPIRE scope, so the realized value is the owner's -- one roll-up read in
	// place of the team accumulator the tech push used to feed.
	const int iTradeMissionChannel = CascadeChannelRegistry::channelLookup(MODFAM_TRADE_MISSION, 0, -1);
	const int iTradeMissionMod = InfoValuation::realizedAtEmpire(GET_PLAYER(getOwner()), iTradeMissionChannel);
	iGold = getModifiedIntValue(iGold, GC.getTRADE_MISSION_END_TOTAL_PERCENT_ADJUSTMENT() + iTradeMissionMod);

	iGold *= CvGameSpeedScale::speedPercent();
	iGold /= 100;

	return std::max(0, iGold);
}


bool CvUnit::canTrade(const CvPlot* pPlot, bool bTestVisible) const
{
	if (isDelayedDeath())
	{
		return false;
	}

	if (!canPerformActionSM())
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return false;
	}

	if (getTradeGold(pPlot) == 0)
	{
		return false;
	}

	if (!canEnterArea(pPlot->getTeam(), pPlot->area()))
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (pCity->getTeam() == getTeam())
		{
			return false;
		}
	}

	return true;
}


bool CvUnit::trade()
{
	if (!canTrade(plot()))
	{
		return false;
	}


	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_TRADE);
	}

	//if is criminal then must pass an investigation check or fail and become wanted.  If passes the check, then send back to capital.
	CvCity* pCapital = getCityOfOrigin();

	if (isCriminal())
	{
		if (!criminalSuccessCheck())
		{
			finishMoves();
			return true;
		}
		changeExperience100(10);
		GET_PLAYER(getOwner()).changeGold(getTradeGold(plot()));
		finishMoves();
		if (pCapital != NULL)
		{
			setXY(pCapital->getX(), pCapital->getY(), false, true, true, false, false);
		}
		return true;
	}

	GET_PLAYER(getOwner()).changeGold(getTradeGold(plot()));
	//if is Great Person then kill

	//otherwise send back to capital
	if (!isGoldenAge() && pCapital != NULL)
	{
		changeExperience100(10);
		setXY(pCapital->getX(), pCapital->getY(), false, true, true, false, false);
	}
	else
	{
		kill(true, NO_PLAYER, true);
	}

	return true;
}


int CvUnit::getGreatWorkCulture() const
{
	int iCulture = m_pUnitInfo->getGreatWorkBase();

	iCulture *= CvGameSpeedScale::speedPercent();
	iCulture /= 100;

	return std::max(0, iCulture);
}


bool CvUnit::canGreatWork(const CvPlot* pPlot) const
{
	if (isDelayedDeath())
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return false;
	}

	if (pCity->getOwner() != getOwner())
	{
		return false;
	}

	if (getGreatWorkCulture() == 0)
	{
		return false;
	}

	return true;
}


bool CvUnit::greatWork()
{
	PROFILE_EXTRA_FUNC();
	if (!canGreatWork(plot()))
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity != NULL)
	{
		pCity->setCultureUpdateTimer(0);
		pCity->setOccupationTimer(0);

		const int iCultureToAdd = 100 * getGreatWorkCulture();
		const int iNumTurnsApplied = GC.getDefineINT("GREAT_WORKS_CULTURE_TURNS") * CvGameSpeedScale::speedPercent() / 100;

		for (int i = 0; i < iNumTurnsApplied; ++i)
		{
			pCity->changeCultureTimes100(getOwner(), iCultureToAdd / iNumTurnsApplied, true, true);
		}

		if (iNumTurnsApplied > 0)
		{
			pCity->changeCultureTimes100(getOwner(), iCultureToAdd % iNumTurnsApplied, false, true);
		}
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_GREAT_WORK);
	}

	kill(true, NO_PLAYER, true);

	return true;
}

bool CvUnit::doOutcomeMission(MissionTypes eMission)
{
	PROFILE_EXTRA_FUNC();
	const CvOutcomeMission* pOutcomeMission = getUnitInfo().getOutcomeMissionByMission(eMission);

	if (!pOutcomeMission)
	{
		// Outcome missions on unit combats
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			if (isHasUnitCombat((UnitCombatTypes)iI))
			{
				pOutcomeMission = GC.getUnitCombatInfo((UnitCombatTypes)iI).getOutcomeMissionByMission(eMission);
				if (pOutcomeMission)
				{
					break;
				}
			}
		}
	}

	if (!pOutcomeMission)
	{
		return false;
	}

	if (!pOutcomeMission->isPossible(this))
	{
		return false;
	}

	pOutcomeMission->execute(this);

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(eMission);
	}

	if (pOutcomeMission->isKill())
	{
		getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
		kill(true);
	}

	return true;
}


int CvUnit::getEspionagePoints() const
{
	int iEspionagePoints = m_pUnitInfo->getEspionagePoints();

	iEspionagePoints *= CvGameSpeedScale::speedPercent();
	iEspionagePoints /= 100;

	return std::max(0, iEspionagePoints);
}

bool CvUnit::canInfiltrate() const
{
	if (isDelayedDeath() || isNPC() || getEspionagePoints() == 0)
	{
		return false;
	}
	return true;
}

bool CvUnit::canInfiltrate(const CvPlot* pPlot, bool bTestVisible) const
{
	if (!canInfiltrate())
	{
		return false;
	}
	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL || pCity->isNPC())
	{
		return false;
	}
	if (!bTestVisible && pCity->getTeam() == getTeam())
	{
		return false;
	}
	if (!isInvisible(pCity->getTeam(), false, true))
	{
		return false;
	}
	return true;
}


bool CvUnit::infiltrate()
{
	if (!canInfiltrate(plot()))
	{
		return false;
	}

	int iPoints = getEspionagePoints();

	int iPointsAdj = (GC.getINFILTRATE_MISSION_END_TOTAL_PERCENT_ADJUSTMENT() * iPoints) / 100;

	iPoints += iPointsAdj;

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_INFILTRATE);
	}

	if (!isSpy())
	{
		if (criminalSuccessCheck())
		{
			changeExperience100(100);
			GET_TEAM(getTeam()).changeEspionagePointsAgainstTeam(GET_PLAYER(plot()->getOwner()).getTeam(), iPoints);
			GET_TEAM(getTeam()).changeEspionagePointsEver(iPoints);

			const CvCity* pCapital = GET_PLAYER(getOwner()).getCapitalCity();
			if (pCapital != NULL)
			{
				finishMoves();
				setXY(pCapital->getX(), pCapital->getY(), false, true, true, false, false);
			}
			else
			{
				kill(true, NO_PLAYER, true);
			}
		}
		else
		{
			finishMoves();
		}
	}
	else
	{
		GET_TEAM(getTeam()).changeEspionagePointsAgainstTeam(GET_PLAYER(plot()->getOwner()).getTeam(), iPoints);
		GET_TEAM(getTeam()).changeEspionagePointsEver(iPoints);
		kill(true, NO_PLAYER, true);
	}

	return true;
}


bool CvUnit::canEspionage(const CvPlot* pPlot, bool bTestVisible) const
{
	if (isDelayedDeath() || !isSpy())
	{
		return false;
	}

	const PlayerTypes ePlotOwner = pPlot->getOwner();
	if (NO_PLAYER == ePlotOwner)
	{
		return false;
	}

	const CvPlayer& kTarget = GET_PLAYER(ePlotOwner);

	if (kTarget.isNPC())
	{
		return false;
	}

	if (kTarget.getTeam() == getTeam())
	{
		return false;
	}

	if (GET_TEAM(getTeam()).isVassal(kTarget.getTeam()))
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (isMadeAttack())
		{
			return false;
		}

		if (hasMoved())
		{
			return false;
		}

		if (kTarget.getTeam() != getTeam() && !isInvisible(kTarget.getTeam(), false))
		{
			return false;
		}
	}

	return true;
}

//TSHEEP start
bool CvUnit::awardSpyExperience(TeamTypes eTargetTeam, int iModifier)
{
	if (GC.isSS_ENABLED())
	{
		const int iDifficulty = getSpyInterceptPercent(eTargetTeam) * (100 + iModifier) / 100;
		if (iDifficulty < 1)
			changeExperience(1);
		else if (iDifficulty < 10)
			changeExperience(2);
		else if (iDifficulty < 25)
			changeExperience(3);
		else if (iDifficulty < 50)
			changeExperience(4);
		else if (iDifficulty < 75)
			changeExperience(5);
		else
			changeExperience(6);
		return true;
	}
	return false;
}
//TSHEEP End


bool CvUnit::canAssassin(const CvPlot* pPlot, bool bTestVisible) const
{
	if (isDelayedDeath())
	{
		return false;
	}

	if (!isSpy())
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();
	if (NULL == pCity)
	{
		return false;
	}

	const int numGreatPeople = pCity->getNumGreatPeople();
	if (numGreatPeople <= 0)
	{
		return false;
	}

	const CvPlayer& kTarget = GET_PLAYER(pCity->getOwner());

	if (kTarget.getTeam() == getTeam())
	{
		return false;
	}

	if (kTarget.isNPC())
	{
		return false;
	}

	if (GET_TEAM(getTeam()).isVassal(kTarget.getTeam()))
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (isMadeAttack())
		{
			return false;
		}

		if (hasMoved())
		{
			return false;
		}

		if (kTarget.getTeam() != getTeam() && !isInvisible(kTarget.getTeam(), false))
		{
			return false;
		}
	}

	return true;
}

bool CvUnit::canBribe(const CvPlot* pPlot, bool bTestVisible) const
{
	if (isDelayedDeath())
	{
		return false;
	}

	if (!isSpy())
	{
		return false;
	}

	if(pPlot->plotCount(PUF_isOtherTeam, getOwner(), -1, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getOwner()) < 1)
	{
		return false;
	}

	if (pPlot->plotCount(PUF_isUnitAIType, UNITAI_WORKER, -1) < 1)
	{
		return false;
	}

	const CvUnit* pTargetUnit = pPlot->plotCheck(PUF_isOtherTeam, getOwner(), -1, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getOwner());
	const CvPlayer& kTarget = GET_PLAYER(pTargetUnit->getOwner());

	if (kTarget.getTeam() == getTeam())
	{
		return false;
	}

	if (kTarget.isNPC())
	{
		return false;
	}

	if (GET_TEAM(getTeam()).isVassal(kTarget.getTeam()))
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (isMadeAttack())
		{
			return false;
		}

		if (hasMoved())
		{
			return false;
		}

		if (kTarget.getTeam() != getTeam() && !isInvisible(kTarget.getTeam(), false))
		{
			return false;
		}
	}

	return true;
}


bool CvUnit::espionage(EspionageMissionTypes eMission, int iData)
{
	if (!canEspionage(plot()))
	{
		return false;
	}

	PlayerTypes eTargetPlayer = plot()->getOwner();

	if (NO_ESPIONAGEMISSION == eMission)
	{
		FAssert(GET_PLAYER(getOwner()).isHumanPlayer());
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_DOESPIONAGE);
		if (NULL != pInfo)
		{
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
		}
	}
	else if (GC.getEspionageMissionInfo(eMission).isTwoPhases() && -1 == iData)
	{
		FAssert(GET_PLAYER(getOwner()).isHumanPlayer());
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_DOESPIONAGE_TARGET);
		if (NULL != pInfo)
		{
			pInfo->setData1(eMission);
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
		}
	}
	else
	{
		if (testSpyIntercepted(eTargetPlayer, GC.getEspionageMissionInfo(eMission).getDifficultyMod()))
		{
			return false;
		}

		const bool bCaught = testSpyIntercepted(eTargetPlayer, GC.getDefineINT("ESPIONAGE_SPY_MISSION_ESCAPE_MOD"));

		if (GET_PLAYER(getOwner()).doEspionageMission(eMission, eTargetPlayer, plot(), iData, this, (bCaught && !isAlwaysHeal())))
		{
			// If it died in the mission (e.g. - nuke and blew itself up) then nothing else needs doing
			if (!isDelayedDeath())
			{
				if (plot()->isActiveVisible(false))
				{
					NotifyEntity(MISSION_ESPIONAGE);
				}

				if (!bCaught)
				{
					setFortifyTurns(0);
					setMadeAttack(true);
					finishMoves();

					// Afforess 07/12/10
					// Spy actions that aren't in a city don't cause the spy to be sent back
					if (plot()->isCity())
					{
						const CvCity* pCapital = GET_PLAYER(getOwner()).getCapitalCity();

						if (NULL != pCapital)
						{
							if (!pCapital->isInViewport())
							{
								GC.getCurrentViewport()->bringIntoView(pCapital->getX(), pCapital->getY(), NULL, true);
							}
							//GC.getGame().logOOSSpecial(20, getID(), pCapital->getX(), pCapital->getY());
							setXY(pCapital->getX(), pCapital->getY(), false, false, false);


							const CvWString szBuffer = gDLL->getText("TXT_KEY_ESPIONAGE_SPY_SUCCESS", getNameKey(), pCapital->getNameKey());
							AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO,
								getButton(), GC.getCOLOR_WHITE(), pCapital->getX(), pCapital->getY(), true, true);
						}
					}
					//TSHEEP Give spies xp for successful missions
					awardSpyExperience(GET_PLAYER(eTargetPlayer).getTeam(),GC.getEspionageMissionInfo(eMission).getDifficultyMod());
				}
			}
			return true;
		}
	}
	return false;
}

bool CvUnit::testSpyIntercepted(PlayerTypes eTargetPlayer, int iModifier)
{
	CvPlayer& kTargetPlayer = GET_PLAYER(eTargetPlayer);

	if (kTargetPlayer.isNPC())
	{
		return false;
	}

	if (GC.getGame().getSorenRandNum(10000, "Spy Interception") >= getSpyInterceptPercent(kTargetPlayer.getTeam()) * (100 + iModifier))
	{
		return false;
	}

	CvString szFormatNoReveal;
	CvString szFormatReveal;

	if (GET_TEAM(kTargetPlayer.getTeam()).getCounterespionageModAgainstTeam(getTeam()) > 0)
	{
		szFormatNoReveal = "TXT_KEY_SPY_INTERCEPTED_MISSION";
		szFormatReveal = "TXT_KEY_SPY_INTERCEPTED_MISSION_REVEAL";
	}
	else if (plot()->isEspionageCounterSpy(kTargetPlayer.getTeam()))
	{
		szFormatNoReveal = "TXT_KEY_SPY_INTERCEPTED_SPY";
		szFormatReveal = "TXT_KEY_SPY_INTERCEPTED_SPY_REVEAL";
	}
	else
	{
		szFormatNoReveal = "TXT_KEY_SPY_INTERCEPTED";
		szFormatReveal = "TXT_KEY_SPY_INTERCEPTED_REVEAL";
	}

	CvWString szCityName = kTargetPlayer.getCivilizationShortDescription();
	CvCity* pClosestCity = GC.getMap().findCity(getX(), getY(), eTargetPlayer, kTargetPlayer.getTeam(), true, false);
	if (pClosestCity != NULL)
	{
		szCityName = pClosestCity->getName();
	}

	CvWString szBuffer = gDLL->getText(szFormatReveal.GetCString(), GET_PLAYER(getOwner()).getCivilizationAdjectiveKey(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString());
	AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), getX(), getY(), true, true);

	//TSHEEP Enable Loyalty Promotion
	//if (GC.getGame().getSorenRandNum(100, "Spy Reveal identity") < GC.getDefineINT("ESPIONAGE_SPY_REVEAL_IDENTITY_PERCENT"))
	if (GC.getGame().getSorenRandNum(100, "Spy Reveal identity") < GC.getDefineINT("ESPIONAGE_SPY_REVEAL_IDENTITY_PERCENT") && !isAlwaysHeal())//TSHEEP End
	{
		if (!isEnemy(kTargetPlayer.getTeam()))
		{
			kTargetPlayer.AI_changeMemoryCount(getOwner(), MEMORY_SPY_CAUGHT, 1);
		}
		AddDLLMessage(eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_EXPOSE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY(), true, true);
	}
	else
	{
		AddDLLMessage(
			eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText(
				szFormatNoReveal.GetCString(), getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString()
			),
			"AS2D_EXPOSE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY(), true, true
		);
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_SURRENDER);
	}

	//TSHEEP - Give xp to spy who catches spy
	{
		CvUnit* pCounterUnit = plot()->plotCheck(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, kTargetPlayer.getTeam());

		if (NULL != pCounterUnit)
		{
			pCounterUnit->changeExperience(1);
		}
	}

	//TSHEEP Implement Escape Promotion
	if (GC.getGame().getSorenRandNum(100, "Spy Reveal identity") < withdrawalProbability())
	{
		setFortifyTurns(0);
		setMadeAttack(true);
		finishMoves();

		CvCity* pCapital = GET_PLAYER(getOwner()).getCapitalCity();

		if (NULL != pCapital)
		{
			//GC.getGame().logOOSSpecial(21, getID(), pCapital->getX(), pCapital->getY());
			setXY(pCapital->getX(), pCapital->getY(), false, false, false);
		}
		{

			szFormatReveal = "TXT_KEY_SPY_ESCAPED_REVEAL";
			szFormatNoReveal = "TXT_KEY_SPY_ESCAPED";
			AddDLLMessage(
				getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText(
					szFormatReveal.GetCString(), GET_PLAYER(getOwner()).getCivilizationAdjectiveKey(),
					getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString()
				),
				"AS2D_EXPOSED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY(), true, true
			);
			AddDLLMessage(
				eTargetPlayer, true, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText(
					szFormatNoReveal.GetCString(),
					getNameKey(), kTargetPlayer.getCivilizationAdjectiveKey(), szCityName.GetCString()
				),
				"AS2D_EXPOSE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), getX(), getY(), true, true
			);
		}
		changeExperience(1);

		return true;
	}
	//TSHEEP End

	kill(true, NO_PLAYER, true);

	return true;
}

int CvUnit::getSpyInterceptPercent(TeamTypes eTargetTeam) const
{
	FAssert(isSpy());
	FAssert(getTeam() != eTargetTeam);

	int iSuccess = 0;

	int iTargetPoints = GET_TEAM(eTargetTeam).getEspionagePointsEver();
	int iOurPoints = GET_TEAM(getTeam()).getEspionagePointsEver();
	iSuccess += (GC.getDefineINT("ESPIONAGE_INTERCEPT_SPENDING_MAX") * iTargetPoints) / std::max(1, iTargetPoints + iOurPoints);

	//TSHEEP - add evasion attribute to spy chances
	if (resolvedValue(URS_EVASION))
	{
		iSuccess -= resolvedValue(URS_EVASION);
	}

	if (plot()->isEspionageCounterSpy(eTargetTeam))
	{
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_COUNTERSPY");
		//TSHEEP - Add intercept attribute of any enemy spies present to chances
		if(plot()->plotCheck(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, eTargetTeam))
		{
			CvUnit* pCounterUnit = plot()->plotCheck(PUF_isCounterSpy, -1, -1, NULL, NO_PLAYER, eTargetTeam);
			if(pCounterUnit != NULL && pCounterUnit->resolvedValue(URS_INTERCEPT))
				iSuccess += pCounterUnit->resolvedValue(URS_INTERCEPT);
		}
	}

	if (GET_TEAM(eTargetTeam).getCounterespionageModAgainstTeam(getTeam()) > 0)
	{
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_COUNTERESPIONAGE_MISSION");
	}

	//TSHEEP - This check was always returning true since there is always at least one friendly spy in the tile
	//if (0 == getFortifyTurns() || plot()->plotCount(PUF_isSpy, -1, -1, NO_PLAYER, getTeam()) > 0)
	if (0 == getFortifyTurns() || plot()->plotCount(PUF_isSpy, -1, -1, NULL, NO_PLAYER, getTeam()) > 1)//TSHEEP - End
	{
		iSuccess += GC.getDefineINT("ESPIONAGE_INTERCEPT_RECENT_MISSION");
	}

	return std::min(100, std::max(0, iSuccess));
}

bool CvUnit::isIntruding() const
{
	TeamTypes eLocalTeam = plot()->getTeam();

	if (NO_TEAM == eLocalTeam || eLocalTeam == getTeam())
	{
		return false;
	}

	// UNOFFICIAL_PATCH Start
	// * Vassal's spies no longer caught in master's territory
	//if (GET_TEAM(eLocalTeam).isVassal(getTeam()))
	if (GET_TEAM(eLocalTeam).isVassal(getTeam()) || GET_TEAM(getTeam()).isVassal(eLocalTeam))
	// UNOFFICIAL_PATCH End
	{
		return false;
	}

	return true;
}

bool CvUnit::canGoldenAge(bool bTestVisible) const
{
	if (!isGoldenAge())
	{
		return false;
	}

	if (!bTestVisible)
	{
		if (GET_PLAYER(getOwner()).unitsRequiredForGoldenAge() > GET_PLAYER(getOwner()).unitsGoldenAgeReady())
		{
			return false;
		}
	}

	return true;
}


bool CvUnit::goldenAge()
{
	if (!canGoldenAge())
	{
		return false;
	}

	GET_PLAYER(getOwner()).killGoldenAgeUnits(this);

	GET_PLAYER(getOwner()).changeGoldenAgeTurns(GET_PLAYER(getOwner()).getGoldenAgeLength());
	GET_PLAYER(getOwner()).changeNumUnitGoldenAges(1);

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_GOLDEN_AGE);
	}

	kill(true, NO_PLAYER, true);

	return true;
}


bool CvUnit::canBuild(const CvPlot* pPlot, BuildTypes eBuild, bool bTestVisible) const
{
	if (!hasBuild(eBuild))
	{
		return false;
	}

	if (getGroup()->isAutomated())
	{
		if (!GET_PLAYER(getOwner()).isAutomatedCanBuild(eBuild))
		{
			return false;
		}
		if (plot()->getWorkingCity() != NULL && !plot()->getWorkingCity()->isAutomatedCanBuild(eBuild))
		{
			return false;
		}
	}

	if (!GET_PLAYER(getOwner()).canBuild(pPlot, eBuild, bTestVisible))
	{
		return false;
	}

	if (!pPlot->isValidDomainForAction(*this))
	{
		return false;
	}

	return true;
}

// Returns true if build finished...
bool CvUnit::build(BuildTypes eBuild)
{
	if (!canBuild(plot(), eBuild))
	{
		return false;
	}

	//TBNOTE: There were still some crashes in this so workers cannot merge or split anymore.
	//if (GC.getBuildInfo(eBuild).isKill())
	//{
	//	if (!canPerformActionSM())
	//	{
	//		getGroup()->clearMissionQueue();
	//		if (isHuman())
	//		{

	//			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_UNIT_CANT_FINISH_BUILD", getNameKey(), GC.getBuildInfo(eBuild).getTextKeyWide());
	//			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_YELLOW(), getX(), getY());
	//		}
	//		return false;
	//	}
	//}

	// Note: notify entity must come before changeBuildProgress - because once the unit is done building,
	// that function will notify the entity to stop building.
	NotifyEntity((MissionTypes)GC.getBuildInfo(eBuild).getMissionType());

	GET_PLAYER(getOwner()).changeGold(-(GET_PLAYER(getOwner()).getBuildCost(plot(), eBuild)));

	bool bFinished = plot()->changeBuildProgress(eBuild, workRate(false), getOwner());

	finishMoves(); // needs to be at bottom because movesLeft() can affect workRate()...
	deselect(true);

	if (bFinished)
	{
		const CvBuildInfo& kBuild = GC.getBuildInfo(eBuild);
		// Super Forts begin *culture*
		if (kBuild.getImprovement() != NO_IMPROVEMENT
		&& GC.getImprovementInfo(kBuild.getImprovement()).getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_PLOT) > 0)
		{
			if (plot()->getOwner() == NO_PLAYER)
			{
				plot()->setOwner(getOwner(),true,true);
			}

			// Special case for plot-grabbing improvements - to get the AI to behave sensibly
			// we nee to split the group here or else an escorted worker will now move away, taking its
			// escort with it, leaving the fort undefended until some other defensive unit can get there.
			// By splitting the group we let the escort occupy the fort at higher priority than
			// re-grouping with the worker, which then has to wait for another escort if it needs one
			if (!isHuman())
			{
				getGroup()->AI_makeForceSeparate();
			}
		}
		// Super Forts end

		if (kBuild.isConsumesUnit())
		{
			if (plot()->getWorkingCity() != NULL)
			{
				OutputDebugString(CvString::format("Worker at (%d,%d) consumed by build for city %S\n", getX(), getY(), plot()->getWorkingCity()->getName().GetCString()).c_str());
			}
			kill(true, NO_PLAYER, true);
		}
		else if (kBuild.getTime() > 0)
		{
			//ls612: Workers now get XP on finishing a build
			changeExperience100(kBuild.getTime() / std::max(1, workRate(true) / 50));
		}
	}

	// Python Event
	CvEventReporter::getInstance().unitBuildImprovement(this, eBuild, bFinished);

	return bFinished;
}


bool CvUnit::canPromote(PromotionTypes ePromotion, int iLeaderUnitId) const
{
	if (iLeaderUnitId >= 0)
	{
		if (iLeaderUnitId == getID())
		{
			return false;
		}

		// The command is always possible if it's coming from a Warlord unit that gives just experience points
		CvUnit* pWarlord = GET_PLAYER(getOwner()).getUnit(iLeaderUnitId);
		if (pWarlord &&
			NO_UNIT != pWarlord->getUnitType() &&
			pWarlord->getUnitInfo().getLeaderExperience() > 0 &&
			NO_PROMOTION == pWarlord->getUnitInfo().getLeaderPromotion() &&
			canAcquirePromotionAny())
		{
			return true;
		}
	}

	if (ePromotion == NO_PROMOTION)
	{
		return false;
	}

	//TB Debug Note: Apparently the promotions that indicate the unit will become a Leader type that are given to the unit during a Leader attaching
	//to the unit were passing through canAcquirePromotion with a true result though these should've been eliminated here as testPromotionReady would
	//thus find there was an existing promotion possible by only checking against canAcquirePromotion and not including the rest of these checks.
	//By making bForLeader a default false, this will only make it possible to pass true on that check on a leader promotion in canAcquirePromotion if the
	//check is specfically to see if the unit can in all other ways qualify for the leader promo.
	PromotionRequirements::flags promoFlags = PromotionRequirements::Promote;

	if (iLeaderUnitId >= 0)
	{
		promoFlags |= PromotionRequirements::ForLeader;
	}

	if (!canAcquirePromotion(ePromotion, promoFlags))
	{
		return false;
	}

	if (GC.getPromotionInfo(ePromotion).isLeader())
	{
		if (iLeaderUnitId >= 0)
		{
			const CvUnit* pWarlord = GET_PLAYER(getOwner()).getUnit(iLeaderUnitId);

			if (pWarlord && NO_UNIT != pWarlord->getUnitType())
			{
				return (pWarlord->getUnitInfo().getLeaderPromotion() == ePromotion);
			}
		}
		return false;
	}

	if (!isPromotionReady())
	{
		return false;
	}

	return true;
}

bool CvUnit::promote(PromotionTypes ePromotion, int iLeaderUnitId)
{
	if (!canPromote(ePromotion, iLeaderUnitId))
	{
		return false;
	}

	if (iLeaderUnitId >= 0)
	{
		CvUnit* pWarlord = GET_PLAYER(getOwner()).getUnit(iLeaderUnitId);
		if (pWarlord)
		{
			pWarlord->giveExperience();
			if (!pWarlord->getNameNoDesc().empty())
			{
				setName(pWarlord->getNameKey());
			}

			//update graphics models
			m_eLeaderUnitType = pWarlord->getUnitType();
			rebuildEntityArt();
		}
	}

	if (!GC.getPromotionInfo(ePromotion).isLeader())
	{
		if (getRetrainsAvailable() > 0)
		{
			changeRetrainsAvailable(-1);
		}
		else
		{
			changeLevel(1);
			changeDamage(-(getDamage() / 2));
		}
	}

	setHasPromotion(ePromotion, true, false, false, true);

	testPromotionReady();

	if (IsSelected())
	{
		gDLL->getInterfaceIFace()->playGeneralSound(GC.getPromotionInfo(ePromotion).getSound());

		gDLL->getInterfaceIFace()->setDirty(UnitInfo_DIRTY_BIT, true);

// BUG - Update Plot List - start
		gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
// BUG - Update Plot List - end
	}
	else
	{
		setInfoBarDirty(true);
	}

	CvEventReporter::getInstance().unitPromoted(this, ePromotion);

	return true;
}

bool CvUnit::lead(int iUnitId)
{
	if (!canLead(plot(), iUnitId))
	{
		return false;
	}

	PromotionTypes eLeaderPromotion = (PromotionTypes)m_pUnitInfo->getLeaderPromotion();

	if (-1 == iUnitId)
	{
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_LEADUNIT, eLeaderPromotion, getID());
		if (pInfo)
		{
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
		}
		return false;
	}
	else
	{
		CvUnit* pUnit = GET_PLAYER(getOwner()).getUnit(iUnitId);

		if (!pUnit || !pUnit->canPromote(eLeaderPromotion, getID()))
		{
			return false;
		}

		pUnit->joinGroup(NULL, true, true);

		pUnit->promote(eLeaderPromotion, getID());

		if (plot()->isActiveVisible(false))
		{
			NotifyEntity(MISSION_LEAD);
		}

		kill(true, NO_PLAYER, true);

		return true;
	}
}


int CvUnit::canLead(const CvPlot* pPlot, int iUnitId) const
{
	PROFILE_FUNC();

	if (isDelayedDeath())
	{
		return 0;
	}

	if (NO_UNIT == getUnitType())
	{
		return 0;
	}

	if (isCommander())
	{
		return 0;
	}

	if (isCommodore())
	{
		return 0;
	}


	int iNumUnits = 0;
	const CvUnitInfo& kUnitInfo = getUnitInfo();

	if (-1 == iUnitId)
	{
		foreach_(const CvUnit* pUnit, pPlot->units())
		{
			if (pUnit != this &&
				pUnit->getOwner() == getOwner() &&
				!pUnit->isCommander() &&
				!pUnit->isCommodore() &&
				pUnit->canPromote((PromotionTypes)kUnitInfo.getLeaderPromotion(), getID()))
			{
				++iNumUnits;
			}
		}
	}
	else
	{
		const CvUnit* pUnit = GET_PLAYER(getOwner()).getUnit(iUnitId);
		if (pUnit && pUnit != this &&
			pUnit->canPromote((PromotionTypes)kUnitInfo.getLeaderPromotion(), getID()))
		{
			iNumUnits = 1;
		}
	}
	return iNumUnits;
}


int CvUnit::canGiveExperience(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iNumUnits = 0;

	if (NO_UNIT != getUnitType() && m_pUnitInfo->getLeaderExperience() > 0)
	{
		foreach_(const CvUnit* pUnit, pPlot->units())
		{
			if (pUnit != this
			&& pUnit->getOwner() == getOwner()
			&& pUnit->canAcquirePromotionAny()
			&& !pUnit->getUnitInfo().hasSkill(CLS_SKILL_GREAT_GENERAL))
			{
				++iNumUnits;
			}
		}
	}
	return iNumUnits;
}

bool CvUnit::giveExperience()
{
	PROFILE_EXTRA_FUNC();
	const CvPlot* pPlot = plot();

	if (pPlot)
	{
		const int iNumUnits = canGiveExperience(pPlot);
		if (iNumUnits > 0)
		{
			const int iTotalExperience = getStackExperienceToGive(iNumUnits);
			const int iMinExperiencePerUnit = iTotalExperience / iNumUnits;

			int i = 0;
			foreach_(CvUnit* pUnit, pPlot->units())
			{
				if (pUnit != this && pUnit->getOwner() == getOwner()
				&& !pUnit->isCommander() && !pUnit->isCommodore() && pUnit->canAcquirePromotionAny())
				{
					pUnit->changeExperience(i < (iTotalExperience % iNumUnits) ? iMinExperiencePerUnit + 1 : iMinExperiencePerUnit);
				}
				i++;
			}
			return true;
		}
	}
	return false;
}

int CvUnit::getStackExperienceToGive(int iNumUnits) const
{
	return (m_pUnitInfo->getLeaderExperience() * (100 + std::min(50, (iNumUnits - 1) * GC.getDefineINT("WARLORD_EXTRA_EXPERIENCE_PER_UNIT_PERCENT")))) / 100;
}

int CvUnit::upgradePrice(UnitTypes eUnit) const
{
	if (isNPC())
	{
		return 0;
	}
	int64_t iPrice = (
		GC.getUNIT_UPGRADE_COST_PER_PRODUCTION()
		* (
			GET_PLAYER(getOwner()).getProductionNeeded(eUnit)
			-
			GET_PLAYER(getOwner()).getBaseUnitCost(getUnitType()) / 100
		)
	);
	if (iPrice < 1)
	{
		return 1;
	}
	{
		int aiCostKinds[NUM_COSTS_KINDS];
		GET_PLAYER(getOwner()).getCostKinds(aiCostKinds);
		int iMod = aiCostKinds[COSTS_UPGRADE];
		if (!isHuman())
		{
			iMod += (
				GC.getHandicapInfo(GC.getGame().getHandicapType()).getCostsModifier(COSTS_UPGRADE, CASC_SCOPE_EMPIRE, true) - 100
				+
				GC.getHandicapInfo(GC.getGame().getHandicapType()).getUnitUpkeepEraModifier() * GET_PLAYER(getOwner()).getCurrentEra()
			);
		}
		iPrice = getModifiedIntValue64(iPrice, iMod);
	}
	iPrice -= iPrice * std::min(100, getUpgradeDiscount()) / 100;

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		iPrice = applySMRank64(iPrice, getSizeMattersOffsetValue(), GC.getSIZE_MATTERS_MOST_MULTIPLIER(), false);
	}
	iPrice /= 100;

	if (iPrice >= MAX_INT)
	{
		return MAX_INT;
	}
	return std::max(1, static_cast<int>(iPrice));
}


bool CvUnit::upgradeAvailable(UnitTypes eFromUnit, UnitTypes eToUnit) const
{
	return GET_PLAYER(getOwner()).upgradeAvailable(eFromUnit, eToUnit);
}


bool CvUnit::canUpgrade(UnitTypes eUnit, bool bTestVisible) const
{
	if (eUnit == NO_UNIT || !isReadyForUpgrade())
	{
		return false;
	}

	if (!bTestVisible && GET_PLAYER(getOwner()).getGold() < upgradePrice(eUnit))
	{
		return false;
	}

	if (GET_PLAYER(getOwner()).getUpgradeRoundCount() == GC.getUPGRADE_ROUND_LIMIT())
	{
		return false;
	}

	if (isCargo())//Being able to upgrade a loaded unit can be problematic
	{
		return false;
	}

	//units in cities cannot upgrade unless the city is one of the owner's team.
	//easiest way to keep upgrades from capturing.  Enforces Rogues must move out of opponent cities to upgrade, which is a little risky which is something to keep things interesting for them.
	if (plot() != NULL && plot()->getPlotCity() != NULL && getTeam() != plot()->getTeam())
	{
		return false;
	}

	if (hasUpgrade(eUnit))
	{
		return true;
	}

	return false;
}

bool CvUnit::isReadyForUpgrade() const
{
	return canMove() && (plot()->getTeam() == getTeam() || isUpgradeAnywhere() || GET_PLAYER(getOwner()).isUpgradeAnywhere());
}

// has upgrade is used to determine if an upgrade is possible,
// it specifically does not check whether the unit can move, whether the current plot is owned, enough gold
// those are checked in canUpgrade()
// does not search all cities, only checks the closest one
bool CvUnit::hasUpgrade(bool bSearch) const
{
	return getUpgradeCity(bSearch) != NULL;
}

// has upgrade is used to determine if an upgrade is possible,
// it specifically does not check whether the unit can move, whether the current plot is owned, enough gold
// those are checked in canUpgrade()
// does not search all cities, only checks the closest one
bool CvUnit::hasUpgrade(UnitTypes eUnit, bool bSearch) const
{
	return getUpgradeCity(eUnit, bSearch) != NULL;
}

// finds the 'best' city which has a valid upgrade for the unit,
// it specifically does not check whether the unit can move, or if the player has enough gold to upgrade
// those are checked in canUpgrade()
// if bSearch is true, it will check every city, if not, it will only check the closest valid city
// NULL result means the upgrade is not possible
CvCity* CvUnit::getUpgradeCity(bool bSearch) const
{
	PROFILE_FUNC();

	CvPlayerAI& kPlayer = GET_PLAYER(getOwner());
	const UnitAITypes eUnitAI = AI_getUnitAIType();
	CvArea* pArea = area();

	const int iCurrentValue = kPlayer.AI_unitValue(getUnitType(), eUnitAI, pArea);

	int iBestSearchValue = MAX_INT;
	CvCity* pBestUpgradeCity = NULL;

	foreach_(int iUnitX, GC.getUnitInfo(m_eUnitType).getUpgradeChain())
	{
		const UnitTypes eUnitX = (UnitTypes)iUnitX;

		if (upgradeAvailable(m_eUnitType, eUnitX) && kPlayer.getUnitAvailabilityAnywhere(eUnitX) == EnablerDomain::STATE_LISTED
		&& kPlayer.AI_unitValue(eUnitX, eUnitAI, pArea) > iCurrentValue)
		{
			int iSearchValue;
			CvCity* pUpgradeCity = getUpgradeCity(eUnitX, bSearch, &iSearchValue);
			if (pUpgradeCity != NULL)
			{
				// if not searching or close enough, then this match will do
				if (!bSearch || iSearchValue < 16)
				{
					return pUpgradeCity;
				}

				if (iSearchValue < iBestSearchValue)
				{
					iBestSearchValue = iSearchValue;
					pBestUpgradeCity = pUpgradeCity;
				}
			}
		}
	}
	return pBestUpgradeCity;
}

// finds the 'best' city which has a valid upgrade for the unit, to eUnit type
// it specifically does not check whether the unit can move, or if the player has enough gold to upgrade
// those are checked in canUpgrade()
// if bSearch is true, it will check every city, if not, it will only check the closest valid city
// if iSearchValue non NULL, then on return it will be the city's proximity value, lower is better
// NULL result means the upgrade is not possible
CvCity* CvUnit::getUpgradeCity(UnitTypes eUnit, bool bSearch, int* iSearchValue) const
{
	PROFILE_FUNC();

	if (eUnit == NO_UNIT || !upgradeAvailable(m_eUnitType, eUnit) || GET_PLAYER(getOwner()).getUnitAvailabilityAnywhere(eUnit) != EnablerDomain::STATE_LISTED)
	{
		return NULL;
	}
	const CvUnitInfo& kUnitInfo = GC.getUnitInfo(eUnit);

	//The following checks to make sure that the upgrade won't make it impossible for a ship to hold
	//the cargo it already does.
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		if (kUnitInfo.getSizeMatters().cargoSmSpace < SMgetCargo())
		{
			return NULL;
		}
	}
	else if (kUnitInfo.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 < getCargo())
	{
		return NULL;
	}

	foreach_(const CvUnit* pLoopUnit, plot()->units())
	{
		if (pLoopUnit->getTransportUnit() == this)
		{
			if (kUnitInfo.getSpecialCargo() != NO_SPECIALUNIT)
			{
				return NULL;
			}
			// The DOMAIN restriction is the `unit:` predicate qualifier on the candidate's own cargo.space
			// entries ([modifier.md] par.6), so answering it needs the QUALIFIED entry read evaluated against
			// this cargo unit -- the point sum is the unqualified capacity plane by construction and cannot.
		}
	}

	// sea units must be built on the coast
	const bool bCoastalOnly = getDomainType() == DOMAIN_SEA;

	// results
	int iBestValue = MAX_INT;
	CvCity* pBestCity = NULL;

	// if search is true, check every city for our team
	if (bSearch)
	{
		// air units can travel any distance
		const bool bIgnoreDistance = getDomainType() == DOMAIN_AIR;
		const CvArea* pMyArea = (bCoastalOnly && !plot()->isWater()) ? plot()->waterArea() : area();

		const TeamTypes eTeam = getTeam();
		const int iX = getX();
		const int iY = getY();

		// check every player on our team's cities
		for (int iI = 0; iI < MAX_PLAYERS; iI++)
		{
			// is this player on our team?
			if (GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(eTeam))
			{
				foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
				{
					// if coastal only, then make sure we are coast
					CvArea* pCityArea = bCoastalOnly ? pLoopCity->waterArea() : pLoopCity->area();

					// Toffer, units should not be compelled to travel between areas just to get an upgrade.
					if ((bIgnoreDistance || pMyArea == pCityArea) && pLoopCity->getUnitAvailability(eUnit) == EnablerDomain::STATE_LISTED)
					{
						// if we do not care about distance, then the first match will do
						if (bIgnoreDistance)
						{
							// if we do not care about distance, then return 1 for value
							if (iSearchValue != NULL)
							{
								*iSearchValue = 1;
							}
							return pLoopCity;
						}
						int iValue = plotDistance(iX, iY, pLoopCity->getX(), pLoopCity->getY());

						// if we cannot path there, not as good (lower numbers are better)
						if (!generatePath(pLoopCity->plot(), 0, true))
						{
							iValue *= 16;
						}
						if (iValue < iBestValue)
						{
							iBestValue = iValue;
							pBestCity = pLoopCity;
						}
					}
				}
			}
		}
	}
	else
	{
		// Find the closest city
		CvCity* pClosestCity = GC.getMap().findCity(getX(), getY(), NO_PLAYER, getTeam(), true, bCoastalOnly);

		// If we can train, then return this city (otherwise it will return NULL)
		if (pClosestCity != NULL && pClosestCity->getUnitAvailability(eUnit) == EnablerDomain::STATE_LISTED)
		{
			// did not search, always return 1 for search value
			iBestValue = 1;
			pBestCity = pClosestCity;
		}
	}

	// return the best value, if non-NULL
	if (iSearchValue != NULL)
	{
		*iSearchValue = iBestValue;
	}
	return pBestCity;
}

bool CvUnit::upgrade(UnitTypes eUnit)
{
	if (!canUpgrade(eUnit))
	{
		return false;
	}

// BUG - Upgrade Unit Event - start
	const int iPrice = upgradePrice(eUnit);
	GET_PLAYER(getOwner()).changeGold(-iPrice);
	GET_PLAYER(getOwner()).changeUpgradeRoundCount(1);
// BUG - Upgrade Unit Event - end

	//	Preserve the AI type if that is possible
	UnitAITypes eUnitAI = AI_getUnitAIType();

	if ( !GC.getUnitInfo(eUnit).hasUnitAI(eUnitAI) )
	{
		eUnitAI = NO_UNITAI;	//	Will cause it to initialize with its default
	}

	//Set Group to rejoin
	GET_PLAYER(getOwner()).setSelectionRegroup(getGroupID());
	//Set Unit to reload onto
	CvUnit* pTransportUnit = getTransportUnit();

	CvUnit* pUpgradeUnit = GET_PLAYER(getOwner()).initUnit(eUnit, getX(), getY(), eUnitAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
	if (pUpgradeUnit == NULL)
	{
		FErrorMsg("UpgradeUnit is not assigned a valid value");
		return false;
	}
	pUpgradeUnit->joinGroup(getGroup());
	if (pTransportUnit)
	{
		pUpgradeUnit->setTransportUnit(pTransportUnit);
	}
	//Clear Group to rejoin
	GET_PLAYER(getOwner()).setSelectionRegroup(NULL);

	pUpgradeUnit->convert(this);

	pUpgradeUnit->finishMoves();

	if (pUpgradeUnit->getLeaderUnitType() == NO_UNIT && !GC.getGame().isOption(GAMEOPTION_UNIT_INFINITE_XP))
	{
		pUpgradeUnit->setExperience(pUpgradeUnit->getExperience() * 3 / 5);
	}

	CvEventReporter::getInstance().unitUpgraded(this, pUpgradeUnit, iPrice);

	return true;
}


HandicapTypes CvUnit::getHandicapType() const
{
	return GET_PLAYER(getOwner()).getHandicapType();
}


CivilizationTypes CvUnit::getCivilizationType() const
{
	return GET_PLAYER(getOwner()).getCivilizationType();
}

const wchar_t* CvUnit::getVisualCivAdjective(TeamTypes eForTeam) const
{
	if (getVisualOwner(eForTeam) == getOwner())
	{
		return GC.getCivilizationInfo(getCivilizationType()).getAdjectiveKey();
	}

	return L"";
}

SpecialUnitTypes CvUnit::getSpecialUnitType() const
{
	if (m_eSpecialUnit != NO_SPECIALUNIT)
	{
		return m_eSpecialUnit;
	}
	return ((SpecialUnitTypes)(m_pUnitInfo->getSpecialUnitType()));
}


UnitTypes CvUnit::getCaptureUnitType() const
{
	return (UnitTypes)m_pUnitInfo->getCaptures();
}


UnitCombatTypes CvUnit::getUnitCombatType() const
{
	return (UnitCombatTypes) m_pUnitInfo->getCombatClass();
}


DomainTypes CvUnit::getDomainType() const
{
        if (isCommodore())
        {
                return DOMAIN_LAND;
        }
        return m_pUnitInfo->getDomain();
}


InvisibleTypes CvUnit::getInvisibleType() const
{
	// THE CLASSIC METHOD -- the ONE method this unit hides by under the classic system, read from the info's
	// `hideAndSeek.method` (the legacy single `<Invisible>` tag's own datum). Most units carry none and answer
	// NO_INVISIBLE in one compare, which is what keeps this hot read cheap.
	// ⛔ NEVER derived from the unit's method-SKILL SET: the skills are the hide-and-seek CONTEST's membership
	// (a unit can contest by several methods at once), and deriving the classic method from their union made
	// every contest-only hider classically invisible for the first time ever -- the robber class authors no
	// classic method at all, and border patrols stopped killing criminals.
	const int iClassicSkill = getUnitInfo().getHideAndSeek().classicMethodSkill();
	if (iClassicSkill < 0)
	{
		return NO_INVISIBLE;
	}
	for (int iI = 0; iI < GC.getNumInvisibleInfos(); ++iI)
	{
		const InvisibleTypes eMethod = (InvisibleTypes)iI;

		//	A method whose cover has been STRIPPED is not a method this unit hides by — the WANTED line does
		//	exactly that. Without the suppression a marked unit still reported a hiding method, so the classic
		//	branch read it as invisible to every foreign team that had no spotter for it.
		if (GC.getMethodSkill(eMethod) == iClassicSkill && !isNegatesInvisible(eMethod))
		{
			return eMethod;
		}
	}
	return NO_INVISIBLE;
}


int CvUnit::flavorValue(FlavorTypes eFlavor) const
{
	return m_pUnitInfo->getFlavour((int)eFlavor);
}


bool CvUnit::isNPC() const
{
	return GET_PLAYER(getOwner()).isNPC();
}


bool CvUnit::isHominid() const
{
	return GET_PLAYER(getOwner()).isHominid();
}


bool CvUnit::isHuman() const
{
	return GET_PLAYER(getOwner()).isHumanPlayer();
}


int CvUnit::sight(const CvPlot* pPlot) const
{
	if (pPlot == NULL)
	{
		pPlot = plot();
	}
	// The unit's sight BUDGET (vision.md): its own STRENGTH -- base stat + combat classes + promotions, the
	// resolved unit plane -- plus the ELEVATION of wherever it happens to be standing. Elevation is the
	// ground's and POSITIONAL: a unit on a peak carries the peak's 3, and loses it the moment it steps off.
	int iSight = resolvedValue(URS_VISION);

	if (pPlot != NULL)
	{
		iSight += pPlot->visionElevation();
	}
	// The global define is authored in PLOTS, so it is lifted to the vision scale here rather than re-authored
	// -- anyone editing GlobalDefines still reads it as "eight plots".
	return std::min(GC.getMAX_UNIT_VISIBILITY_RANGE() * VISION_OPEN_GROUND_COST, iSight);
}


int CvUnit::baseMoves() const
{
	return (
		(m_pUnitInfo->getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100)
		+ getExtraMoves()
		+ (getDomainType() != DOMAIN_AIR ? GET_TEAM(getTeam()).getExtraMoves(getDomainType()) : 0)
	);
}

int CvUnit::maxMoves() const
{
	PROFILE_FUNC();

	if (m_iMaxMoveCacheTurn != GC.getGame().getGameTurn())
	{
		m_maxMoveCache = (baseMoves() * GC.getMOVE_DENOMINATOR());
		m_iMaxMoveCacheTurn = GC.getGame().getGameTurn();
	}
	return m_maxMoveCache;
}

int CvUnit::movesLeft() const
{
	return std::max(0, maxMoves() - getMoves());
}

bool CvUnit::canMove() const
{
	return !isDead() && getMoves() < maxMoves() && !hasStatus(STATUS_PARALYZED);
}

bool CvUnit::hasMoved()	const
{
	return getMoves() > 0;
}


int CvUnit::airRange() const
{
	// ⚖ A MISSILE'S RANGE IS THE SAME MECHANIC AS AN AIRPLANE'S BOMBARD RANGE (owner), so both answer through
	// the ONE `air.range` channel -- the empire leg reaches a missile exactly as it reaches a bomber. The two
	// branches this replaces differed only in that the missile one read a separate national missile-range
	// accumulator, and that accumulator was a dead end: its legacy tag lives in an XML schema and nowhere else,
	// no record ever authored it, and the address the curator mapped it to has no minted kind. So there was
	// never a second channel -- only a second store that summed nothing.
	// ⚠ A DOMAIN_AIR NUKE that is not a missile still answers its base alone, exactly as before; the nuke leg is
	// its own kind (AIR_NUKE_RANGE, read by nukeRange) and is deliberately not folded in here.
	// ⚠ AIR_RANGE is a FLAT slot, so it reduces at this point of use ([DEC-fixedpoint-x100]).
	const SpecialUnitTypes eMissile = GC.getSPECIALUNIT_MISSILE();
	if (getDomainType() == DOMAIN_AIR && (nukeRange() == -1 || getSpecialUnitType() == eMissile))
	{
		int aiAir[NUM_AIR_KINDS];
		GET_PLAYER(getOwner()).getAirKinds(aiAir);
		return (resolvedValue(URS_AIR_RANGE) / 100 + GET_TEAM(getTeam()).getExtraMoves(DOMAIN_AIR) + aiAir[AIR_RANGE] / 100);
	}
	return (resolvedValue(URS_AIR_RANGE) / 100);
}


int CvUnit::nukeRange() const
{
	// ⛔ The -1 SENTINEL IS PRESERVED HERE, deliberately. The cascade sums an unauthored slot to 0 while the
	// legacy field used -1 for "not a nuke", and that sentinel is load-bearing across this accessor's callers
	// (`>= 0`, `> -1`, `!= -1`) -- returning 0 would make every one of them true for EVERY unit. The slot is a
	// FLAT, so it reduces here ([DEC-fixedpoint-x100]); every authored range is >= 1, so 0 is unambiguous.
	const int iRange = m_pUnitInfo->getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) / 100;
	return iRange > 0 ? iRange : -1;
}

namespace CvUnitInternal
{
	//	The list is taken by TEMPLATE because the two build repertoires differ in element type: the unit info
	//	serves its own `builds` as FK ints, while the worker component holds typed BuildTypes.
	template <class BuildList>
	bool canBuildRoute(const BuildList& aBuilds, const CvTeam& team)
	{
		PROFILE_EXTRA_FUNC();
		for (size_t iBuild = 0; iBuild < aBuilds.size(); ++iBuild)
		{
			const CvBuildInfo& info = GC.getBuildInfo((BuildTypes)aBuilds[iBuild]);
			if (info.getRoute() > NO_ROUTE && team.isHasTech(info.getTechPrereq()))
			{
				return true;
			}
		}
		return false;
	}
}

bool CvUnit::canBuildRoute() const
{
	if (!isWorker()) return false;

	const CvTeam& team = GET_TEAM(getTeam());
	return CvUnitInternal::canBuildRoute(m_worker->getExtraBuilds(), team)
		|| CvUnitInternal::canBuildRoute(m_pUnitInfo->getBuilds(), team);
}

BuildTypes CvUnit::getBuildType() const
{
	BuildTypes eBuild;

	if (getGroup()->headMissionQueueNode() != NULL)
	{
		switch (getGroup()->headMissionQueueNode()->m_data.eMissionType)
		{
		case MISSION_MOVE_TO:
// BUG - Sentry Actions - start
#ifdef _MOD_SENTRY
		case MISSION_MOVE_TO_SENTRY:
#endif
// BUG - Sentry Actions - end
			break;

		case MISSION_ROUTE_TO:
			if (getGroup()->getBestBuildRoute(plot(), &eBuild) != NO_ROUTE)
			{
				return eBuild;
			}
			break;

		case MISSION_MOVE_TO_UNIT:
		case MISSION_SKIP:
		case MISSION_SLEEP:
		case MISSION_FORTIFY:
		case MISSION_BUILDUP:
		case MISSION_AUTO_BUILDUP:
		case MISSION_HEAL_BUILDUP:
		case MISSION_PLUNDER:
		case MISSION_AIRPATROL:
		case MISSION_SEAPATROL:
		case MISSION_HEAL:
		case MISSION_SENTRY:
// BUG - Sentry Actions - start
#ifdef _MOD_SENTRY
		case MISSION_SENTRY_WHILE_HEAL:
		case MISSION_SENTRY_NAVAL_UNITS:
		case MISSION_SENTRY_LAND_UNITS:
#endif
// BUG - Sentry Actions - end
		case MISSION_AIRLIFT:
		case MISSION_NUKE:

		case MISSION_RECON:
		case MISSION_PARADROP:
		case MISSION_AIRBOMB:
		case MISSION_BOMBARD:
		case MISSION_RANGE_ATTACK:
		case MISSION_PILLAGE:
		case MISSION_SABOTAGE:
		case MISSION_DESTROY:
		case MISSION_STEAL_PLANS:
		case MISSION_FOUND:
		case MISSION_SPREAD:
		case MISSION_SPREAD_CORPORATION:
		case MISSION_JOIN:
		case MISSION_CONSTRUCT:
		case MISSION_HERITAGE:
		case MISSION_DISCOVER:
		case MISSION_HURRY:
		case MISSION_TRADE:
		case MISSION_GREAT_WORK:
		case MISSION_INFILTRATE:
		case MISSION_GOLDEN_AGE:
		case MISSION_LEAD:
		case MISSION_ESPIONAGE:
		case MISSION_DIE_ANIMATION:
		// Dale - FE: Fighters
		case MISSION_FENGAGE:
		// ! Dale
		case MISSION_HURRY_FOOD:
		case MISSION_INQUISITION:
		case MISSION_CLAIM_TERRITORY:
		case MISSION_ESPIONAGE_SLEEP:
		case MISSION_GREAT_COMMANDER:
		case MISSION_GREAT_COMMODORE:
		case MISSION_SHADOW:
		case MISSION_AMBUSH:
		case MISSION_ASSASSINATE:
			break;

		case MISSION_BUILD:
			return (BuildTypes)getGroup()->headMissionQueueNode()->m_data.iData1;
			break;

		default:
			// AIAndy: Assumed to be an outcome mission
			// FErrorMsg("error");
			break;
		}
	}

	return NO_BUILD;
}

ImprovementTypes CvUnit::getBuildTypeImprovement() const
{
	const BuildTypes buildType = getBuildType();
	if (buildType == NO_BUILD) return NO_IMPROVEMENT;
	return GC.getBuildInfo(buildType).getImprovement();
}

bool CvUnit::isAnimal() const
{
	return GET_PLAYER(getOwner()).isAnimal();
}


bool CvUnit::isNoBadGoodies() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_NO_BAD_GOODIES);
}


bool CvUnit::isOnlyDefensive() const
{
	return m_iOnlyDefensiveCount + getUnitInfo().hasSkill(CLS_SKILL_ONLY_DEFENSIVE);
}

void CvUnit::changeOnlyDefensiveCount(int iChange)
{
	m_iOnlyDefensiveCount += iChange;
}


bool CvUnit::isNoCapture() const
{
	int iCount = getNoCaptureCount();
	if (!canAttack())
	{
		iCount++;
	}
	if (getUnitInfo().hasSkill(CLS_SKILL_NO_CAPTURE))
	{
		iCount++;
	}
	return (iCount > 0);
}


bool CvUnit::isRivalTerritory() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_RIVAL_TERRITORY);
}


bool CvUnit::isMilitaryHappiness() const
{
	return getUnitInfo().hasTag(CLS_TAG_MILITARY);
}

bool CvUnit::isMilitaryBranch() const
{
	return getUnitInfo().hasTag(CLS_TAG_MILITARY);
}


bool CvUnit::isInvestigate() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_INVESTIGATE);
}


bool CvUnit::isCounterSpy() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_COUNTER_SPY);
}


bool CvUnit::isSpy() const
{
	return getUnitInfo().hasTag(CLS_TAG_SPY);
}


bool CvUnit::isFound() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_FOUND);
}

bool CvUnit::isGoldenAge() const
{
	if (isDelayedDeath())
	{
		return false;
	}
	return getUnitInfo().hasSkill(CLS_SKILL_GOLDEN_AGE);
}

bool CvUnit::canCoexistAlwaysOnPlot(const CvPlot& onPlot) const
{
	return alwaysInvisible() || onPlot.isCity(true) && isBlendIntoCity();
}

bool CvUnit::canCoexistWithTeam(const TeamTypes withTeam) const
{
	return alwaysInvisible() || getTeam() == withTeam;
}

bool CvUnit::canCoexistWithTeamOnPlot(const TeamTypes withTeam, const CvPlot& onPlot) const
{
	return (
		   getTeam() == withTeam
		|| canCoexistAlwaysOnPlot(onPlot)
		// Invisible to team and on the same plot
		|| isInvisible(withTeam) && *plot() == onPlot
	);
}

namespace {
	// Will an attacker be always hostile to a defender on the defenders plot?
	bool alwaysHostile(const CvUnit& defender, const CvUnit& attacker)
	{
		return (
			(defender.isAlwaysHostile(defender.plot()) || attacker.isAlwaysHostile(defender.plot()))
			&&
			(!GC.getGame().isOption(GAMEOPTION_ANIMAL_PEACE_AMONG_NPCS) || !defender.isNPC() || !attacker.isNPC())
		);
	}
}
bool CvUnit::canCoexistWithAttacker(const CvUnit& attacker, bool bStealthDefend, bool bAssassinate) const
{
	const TeamTypes attackerTeam = GET_PLAYER(attacker.getOwner()).getTeam();

	return (
		// Same team
		getTeam() == attackerTeam
		// Always invisible
		|| alwaysInvisible() || attacker.alwaysInvisible()
		// Coexists due to blending into a city (nullified by assassination)
		|| !bAssassinate && plot()->isCity(false) && (isBlendIntoCity() || attacker.isBlendIntoCity())
		// Invisibility to the attacking team (nullified by stealthDefend)
		|| !bStealthDefend && isInvisible(attackerTeam, false)
		// War enemy, or just always hostile
		|| !isEnemy(attackerTeam) && !alwaysHostile(*this, attacker)
		// Checks for differing domains, transport status, amnesty game setting
		|| canUnitCoexistWithArrivingUnit(attacker)
	);
}

bool CvUnit::canUnitCoexistWithArrivingUnit(const CvUnit& enemyUnit) const
{
	if (enemyUnit.isDead())
	{
		return true;
	}

	if (plot()->isWater())
	{
		if (enemyUnit.canLoad(plot()))
		{
			return true;
		}

		if (plot()->isSeaTunnel())
		{
			const bool bIsAboveWater =
				getDomainType() != DOMAIN_LAND
				||
				canMoveAllTerrain();
			const bool bOtherIsAboveWater =
				enemyUnit.getDomainType() != DOMAIN_LAND
				||
				enemyUnit.canMoveAllTerrain();

			if (bIsAboveWater && !bOtherIsAboveWater)
			{
				return true;
			}
			if (!bIsAboveWater && bOtherIsAboveWater)
			{
				return true;
			}
		}
	}


	// if is loaded and is not attacking
	if (isCargo()
	&& !getGroup()->IsSelected()
	&&  plot()->isWater() == enemyUnit.plot()->isWater()
	||
		enemyUnit.isCargo()
	&& !enemyUnit.getGroup()->IsSelected()
	&&  plot()->isWater() == enemyUnit.plot()->isWater())
	{
		// May be oversimplified still
		//	If I move a transport and drop off units on land,
		//	the units aren't selected when they are checked to see if they can make this move without an attack.
		//	So this makes for a free overlapping beachhead maneuver somehow, if disembarking,
		//	it must still consider the unit incapable of automatically being able to share the space being moved to with enemies.
		//	Check the autodisembarking code for the right filter checks perhaps...
		return true;
	}
	return false;
}


/*DllExport*/ bool CvUnit::isFighting() const
{
#ifdef _DEBUG
	OutputDebugString("exe is asking if this unit is in battle\n");
#endif
	return getUnit(m_combatUnit) != NULL;
}

// Toffer - Same as isFighting.
bool CvUnit::isInBattle() const
{
	return getCombatUnit() != NULL;
}


bool CvUnit::isAttacking() const
{
	return getAttackPlot() && !isDelayedDeath();
}


bool CvUnit::isDefending() const
{
	return isInBattle() && !isAttacking();
}


bool CvUnit::isCombat() const
{
	return isInBattle() || isAttacking();
}

int CvUnit::withdrawalHP(int iMaxHitPoints, int iAttackerEarly) const
{
	return iMaxHitPoints * (100-iAttackerEarly) / 100;
}


/*DllExport*/ int CvUnit::maxHitPoints() const
{
#ifdef _DEBUG
	OutputDebugString("exe enquiring about unit max HP\n");
#endif
	return getMaxHP();
}


int CvUnit::getHP()	const
{
	return (AI_getPredictedHitPoints() == -1 ? getMaxHP() - getDamage() : AI_getPredictedHitPoints());
}


bool CvUnit::isHurt() const
{
	return (getDamage() > 0);
}


bool CvUnit::isDead() const
{
	return isDelayedDeath() || (getDamage() >= getMaxHP());
}


// The human WRITE boundary (WorldBuilder / the Cy binding edit in whole numbers), so it lifts to the ×100
// native scale here -- strength is ×100 everywhere inside the engine ([DEC-fixedpoint-x100]).
void CvUnit::setBaseCombatStr(int iCombat)
{
	m_iBaseCombat100 = 100 * iCombat;
}

int CvUnit::baseCombatStr() const
{
	return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? getSMStrength() : baseCombatStrPreCheck();
}

// THE human read boundary -- the ONE ÷100 on this cluster. Strength is ×100 everywhere inside the engine, so
// only a human-facing consumer (UI, a Cy binding, a comparison against a human config value) calls this.
// ⚠ Unconditional now: it used to reduce only under SIZE_MATTERS, because baseCombatStr() itself returned a
// different SCALE depending on that option.
int CvUnit::baseCombatStrHuman() const
{
	return baseCombatStr() / 100;
}

int CvUnit::airBaseCombatStr() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return getSMStrength();
	}
	return baseAirCombatStrPreCheck();
}

int CvUnit::baseCombatStrPreCheck() const
{
	// Both legs are ×100 and stay ×100 -- the base member and the resolved promotion/unit-combat delta are on the
	// same scale, so they add directly and nothing is reduced inside the calculation ([DEC-fixedpoint-x100]).
	int iStr = m_iBaseCombat100 + resolvedValue(URS_STRENGTH_FLAT);

	if (iStr < 0)
	{
		return 0;
	}
	if (getExtraStrengthModifier() != 0)
	{
		iStr *= 100 + getExtraStrengthModifier();
		iStr /= 100;
	}
	return iStr;
}

int CvUnit::baseAirCombatStrPreCheck() const
{
	// getAirCombat() is authored HUMAN (raw jsonIdInt, no ×100), so it lifts to meet the ×100 resolved delta --
	// the same shape CvUnitInfo uses for its own air strength.
	int iStr = 100 * m_pUnitInfo->getAirCombat() + resolvedValue(URS_STRENGTH_FLAT);

	if (iStr < 0)
	{
		return 0;
	}
	if (getExtraStrengthModifier() != 0)
	{
		iStr *= 100 + getExtraStrengthModifier();
		iStr /= 100;
	}
	return iStr;
}

int CvUnit::getSMStrength() const
{
	return m_iSMStrength;
}

void CvUnit::setSMStrength()
{
	const int iStrength = getDomainType() == DOMAIN_AIR? baseAirCombatStrPreCheck() : baseCombatStrPreCheck();
	m_iSMStrength = applySMRank(iStrength, getSizeMattersOffsetValue(), GC.getSIZE_MATTERS_MOST_MULTIPLIER());
	FASSERT_NOT_NEGATIVE(m_iSMStrength);
}

struct CombatStrCacheEntry
{
	int	iLRUIndex;
	int iResult;
	const CvPlot* pPlot;
	const CvPlot* pAttackedPlot;
	const CvUnit* pAttacker;
	const CvUnit* pForUnit;
};

#define	COMBATSTR_CACHE_SIZE	100
static CombatStrCacheEntry CombatStrCache[COMBATSTR_CACHE_SIZE];
static int CombatStrCacheInitializedTurn = -1;
static int iNextCombatCacheLRU = 1;

static void FlushCombatStrCache(CvUnit* pMovingUnit)
{
	PROFILE_EXTRA_FUNC();
	if ( pMovingUnit == NULL || pMovingUnit->isCommander() || pMovingUnit->isCommodore())
	{
		memset(CombatStrCache, 0, sizeof(CombatStrCache));

		CombatStrCacheInitializedTurn = GC.getGame().getGameTurn();
	}
	else
	{
		for(int iI = 0; iI < COMBATSTR_CACHE_SIZE; iI++)
		{
			CombatStrCacheEntry* pEntry = &CombatStrCache[iI];

			if ( pEntry->pAttacker == pMovingUnit ||
				 pEntry->pForUnit == pMovingUnit )
			{
				pEntry->pForUnit = NULL;
				pEntry->iLRUIndex = 1;
			}
		}
	}
}

// maxCombatStr can be called in four different configurations
//		pPlot == NULL, pAttacker == NULL for combat when this is the attacker
//		pPlot valid, pAttacker valid for combat when this is the defender
/*** Dexy - Surround and Destroy START ****/
//		pPlot == NULL, pAttacker valid for combat when this is the defender, attacker is just surrounding us (then defender gets no plot defensive bonuses)
/*** Dexy - Surround and Destroy  END  ****/
//		pPlot valid, pAttacker == NULL (new case), when this is the defender, attacker unknown
//		pPlot valid, pAttacker == this (new case), when the defender is unknown, but we want to calc approx str
//			note, in this last case, it is expected pCombatDetails == NULL, it does not have to be, but some
//			values may be unexpectedly reversed in this case (iModifierTotal will be the negative sum)
/*** Dexy - Surround and Destroy START ****/
int CvUnit::maxCombatStr(const CvPlot* pPlot, const CvUnit* pAttacker, CombatDetails* pCombatDetails, bool bSurroundedModifier) const
/*** Dexy - Surround and Destroy  END  ****/
{
	PROFILE_FUNC();

	FAssertMsg(pPlot == NULL || pPlot->getTerrainType() != NO_TERRAIN, "(pPlot == NULL) || (pPlot->getTerrainType() is not expected to be equal with NO_TERRAIN)");

	// Dexy - handle our new special case
	const CvPlot* pAttackedPlot = NULL; // Toffer - Hmm, feels so wrong to establish this as a const when it is changed below.
	bool bAttackingUnknownDefender = false;
	if (pAttacker == this)
	{
		bAttackingUnknownDefender = true;
		bSurroundedModifier = false;
		pAttackedPlot = pPlot;

		// Dexy - reset these values, we will fiddle with them below
		pPlot = NULL;
		pAttacker = NULL;
	}
	// Dexy - otherwise, attack plot is the plot of us (the defender)
	else if (pAttacker != NULL)
	{
		pAttackedPlot = plot();
	}

	CombatStrCacheEntry* pCacheEntry = NULL;
	const CvUnit* pOriginalAttacker = pAttacker;
	const int iBaseCombatStr = baseCombatStr();

	if (pCombatDetails != NULL)
	{
		pCombatDetails->iExtraCombatPercent = 0;
		pCombatDetails->iAnimalCombatModifierTA = 0;
		pCombatDetails->iAIAnimalCombatModifierTA = 0;
		pCombatDetails->iAnimalCombatModifierAA = 0;
		pCombatDetails->iAIAnimalCombatModifierAA = 0;
		pCombatDetails->iBarbarianCombatModifierTB = 0;
		pCombatDetails->iAIBarbarianCombatModifierTB = 0;
		pCombatDetails->iBarbarianCombatModifierAB = 0;
		pCombatDetails->iAIBarbarianCombatModifierAB = 0;
		pCombatDetails->iPlotDefenseModifier = 0;
		pCombatDetails->iFortifyModifier = 0;
		pCombatDetails->iCityDefenseModifier = 0;
		pCombatDetails->iHillsAttackModifier = 0;
		pCombatDetails->iHillsDefenseModifier = 0;
		pCombatDetails->iFeatureAttackModifier = 0;
		pCombatDetails->iFeatureDefenseModifier = 0;
		pCombatDetails->iTerrainAttackModifier = 0;
		pCombatDetails->iTerrainDefenseModifier = 0;
		pCombatDetails->iCityAttackModifier = 0;
		pCombatDetails->iDomainDefenseModifier = 0;
		pCombatDetails->iCityBarbarianDefenseModifier = 0;
		pCombatDetails->iDefenseModifier = 0;
		pCombatDetails->iAttackModifier = 0;
		pCombatDetails->iCombatModifierA = 0;
		pCombatDetails->iCombatModifierT = 0;
		pCombatDetails->iDomainModifierA = 0;
		pCombatDetails->iDomainModifierT = 0;
		pCombatDetails->iAnimalCombatModifierA = 0;
		pCombatDetails->iAnimalCombatModifierT = 0;
		pCombatDetails->iRiverAttackModifier = 0;
		pCombatDetails->iAmphibAttackModifier = 0;
		pCombatDetails->iKamikazeModifier = 0;
		pCombatDetails->iModifierTotal = 0;
		pCombatDetails->iBaseCombatStr = 0;
		pCombatDetails->iCombat = 0;
		pCombatDetails->iMaxCombatStr = 0;
		pCombatDetails->iCurrHitPoints = 0;
		pCombatDetails->iMaxHitPoints = 0;
		pCombatDetails->iCurrCombatStr = 0;
		pCombatDetails->eOwner = getOwner();
		pCombatDetails->eVisualOwner = getVisualOwner();
		pCombatDetails->sUnitName = getName().GetCString();
	}
	else if (iBaseCombatStr == 0)
	{
		return 0;
	}
	else if (bSurroundedModifier
	// And doesn't involve a human player
	&& !GET_PLAYER(getOwner()).isHumanPlayer()
	&& (pAttacker == NULL || !GET_PLAYER(pAttacker->getOwner()).isHumanPlayer()))
	{
		PROFILE("maxCombatStr.Cachable");

		if (CombatStrCacheInitializedTurn != GC.getGame().getGameTurn())
		{
			FlushCombatStrCache(NULL);
		}

		int	iBestLRU = MAX_INT;

		for (int iI = 0; iI < COMBATSTR_CACHE_SIZE; iI++)
		{
			CombatStrCacheEntry* pEntry = &CombatStrCache[iI];

			if (pEntry->iLRUIndex == 0)
			{
				pCacheEntry = pEntry;
				break;
			}
			if (pEntry->pPlot == pPlot && pEntry->pAttackedPlot == pAttackedPlot && pEntry->pAttacker == pOriginalAttacker && pEntry->pForUnit == this)
			{
				//OutputDebugString("maxCombatStr.CachHit\n");
				PROFILE("maxCombatStr.CachHit");
				pEntry->iLRUIndex = iNextCombatCacheLRU++;
				// The cache stores the pre-reduce iCombat, so it reduces exactly as the fresh path does -- cached and
				// fresh must agree or the two disagree by 100× (AGENTS.md drift detector 3).
				return std::max(1, pEntry->iResult / 100);
			}
			if (pEntry->iLRUIndex < iBestLRU)
			{
				iBestLRU = pEntry->iLRUIndex;
				pCacheEntry = pEntry;
			}
		}
	}

	int iExtraModifier = resolvedValue(URS_STRENGTH_PERCENT);
	int iModifier = iExtraModifier;
	if (pCombatDetails != NULL)
	{
		pCombatDetails->iExtraCombatPercent = iExtraModifier;
	}

	// Do modifiers for animals and barbarians (leaving these out for bAttackingUnknownDefender case)
	if (pAttacker != NULL)
	{
		if (isAnimal())
		{
			if (pAttacker->isHuman())
			{
				iExtraModifier = GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_ANIMAL, CASC_SCOPE_WORLD, false);
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAnimalCombatModifierTA = iExtraModifier;
				}
			}
			else
			{
				iExtraModifier = GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_ANIMAL, CASC_SCOPE_WORLD, true);
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAIAnimalCombatModifierTA = iExtraModifier;
				}
			}
		}

		if (pAttacker->isAnimal())
		{
			if (isHuman())
			{
				iExtraModifier = -GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_ANIMAL, CASC_SCOPE_WORLD, false);
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAnimalCombatModifierAA = iExtraModifier;
				}
			}
			else
			{
				iExtraModifier = -GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_ANIMAL, CASC_SCOPE_WORLD, true);
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAIAnimalCombatModifierAA = iExtraModifier;
				}
			}
		}

		if (isHominid())
		{
			//TB Combat Mods Begin
			if (pAttacker->isHuman())
			{
				iExtraModifier = GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_BARBARIAN, CASC_SCOPE_WORLD, false) - pAttacker->vsBarbsModifier();
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iBarbarianCombatModifierTB = iExtraModifier;
				}
			}
			else
			{
				iExtraModifier = GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_BARBARIAN, CASC_SCOPE_WORLD, true) - pAttacker->vsBarbsModifier();
				iModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAIBarbarianCombatModifierTB = iExtraModifier;
				}
			}
		}

		if (pAttacker->isHominid())
		{
			const int iBarbsMod = vsBarbsModifier();

			if (isHuman())
			{
				iExtraModifier = -GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_BARBARIAN, CASC_SCOPE_WORLD, false);
				iModifier += (iExtraModifier + iBarbsMod);
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iBarbarianCombatModifierAB = (iExtraModifier + iBarbsMod);
				}
			}
			else
			{
				iExtraModifier = -GC.getHandicapInfo(GC.getGame().getHandicapType()).getCombat(COMBAT_BARBARIAN, CASC_SCOPE_WORLD, true);
				iModifier += (iExtraModifier + iBarbsMod);
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAIBarbarianCombatModifierTB = (iExtraModifier + iBarbsMod);
				}
			}
		}
	}

	// add defensive bonuses (leaving these out for bAttackingUnknownDefender case)
	if (pPlot != NULL)
	{
		if (!noDefensiveBonus())
		{
			// When pAttacker is NULL but pPlot is not, this is a computation for this units defensive value
			// against an unknown attacker.  Always ignoring building defense in this case is a conservative estimate,
			// but causes AI to suicide against castle walls of low culture cities in early game.  Using this units
			// ignoreBuildingDefense does a little better ... in early game it corrects undervalue of castles.  One
			// downside is when medieval unit is defending a walled city against gunpowder.  Here, the over value
			// makes attacker a little more cautious, but with their tech lead it shouldn't matter too much.  Also
			// makes vulnerable units (ships, etc) feel safer in this case and potentially not leave, but ships
			// leave when ratio is pretty low anyway.
			iExtraModifier = pPlot->defenseModifier(getTeam(), (pAttacker != NULL) ? pAttacker->ignoreBuildingDefense() : ignoreBuildingDefense());

			iModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iPlotDefenseModifier = iExtraModifier;
			}
		}
		//TB Combat Mods (fortification)
		int iFort = fortifyModifier();
		int iFortModTotal = iFort;

		iExtraModifier = iFortModTotal;
		iModifier += iExtraModifier;
		if (pCombatDetails != NULL)
		{
			pCombatDetails->iFortifyModifier = iExtraModifier;
		}

		if (pPlot->isCity(true, getTeam()))
		{
			if (pAttacker && pAttacker->plot() != pPlot)
			{
				iExtraModifier = cityDefenseModifier() + cityDefenseVSOpponent(pAttacker);

				if (pPlot->isCity())
				{
					//TB SubCombat Mod Begin
					for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
					{
						if (it->second.m_bHasUnitCombat)
						{
							iExtraModifier += pPlot->getPlotCity()->getUnitCombatExtraStrength(it->first);
						}
					}
					//TB SubCombat Mod End
				}
			}

			iModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iCityDefenseModifier = iExtraModifier;
			}

		}

		if (pPlot->isHills())
		{
			iExtraModifier = hillsDefenseModifier();
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iHillsDefenseModifier = iExtraModifier;
			}
		}

		if (pPlot->getFeatureType() != NO_FEATURE)
		{
			iExtraModifier = featureDefenseModifier(pPlot->getFeatureType());
			iModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iFeatureDefenseModifier = iExtraModifier;
			}
		}

		iExtraModifier = terrainDefenseModifier(pPlot->getTerrainType());
		iModifier += iExtraModifier;
		if (pCombatDetails != NULL)
		{
			pCombatDetails->iTerrainDefenseModifier = iExtraModifier;
		}
	}

	// if we are attacking to an plot with an unknown defender, the calc the modifier in reverse
	if (bAttackingUnknownDefender)
	{
		pAttacker = this;
	}

	// calc attacker bonueses
	if (pAttacker != NULL && pAttackedPlot != NULL)
	{
		int iTempModifier = 0;

		if (pAttacker->plot() == pAttackedPlot || pAttacker->isInvisible(getTeam(), false, false) || (isRevealed() && getDefenseCount() <= 1))
		{
			iExtraModifier = -pAttacker->stealthCombatModifierTotal();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}

			if (!pAttacker->isInvisible(getTeam(), false, false))
			{
				iExtraModifier = stealthCombatModifierTotal();
				iTempModifier += iExtraModifier;
				{
					if (pCombatDetails != NULL)
					{
						pCombatDetails->iExtraCombatPercent += iExtraModifier;
					}
				}
			}
		}

		if (pAttackedPlot->isCity(true, getTeam()) && pAttacker->plot() != pPlot)
		{
			iExtraModifier = -pAttacker->cityAttackModifier();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iCityAttackModifier = iExtraModifier;
			}

			if (pAttacker->isHominid())
			{
				iExtraModifier = GC.getCITY_BARBARIAN_DEFENSE_MODIFIER();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iCityBarbarianDefenseModifier = iExtraModifier;
				}
			}
		}

		if (pAttackedPlot->isHills())
		{
			iExtraModifier = -pAttacker->hillsAttackModifier();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iHillsAttackModifier = iExtraModifier;
			}
		}

		if (pAttackedPlot->getFeatureType() != NO_FEATURE)
		{
			iExtraModifier = -pAttacker->featureAttackModifier(pAttackedPlot->getFeatureType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iFeatureAttackModifier = iExtraModifier;
			}
		}
		else
		{
			iExtraModifier = -pAttacker->terrainAttackModifier(pAttackedPlot->getTerrainType());
			/*** Dexy - Others' bug fixes START ****/
			iTempModifier += iExtraModifier;
			// OLD CODE
			// iModifier += iExtraModifier;
			/*** Dexy - Others' bug fixes  END  ****/
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iTerrainAttackModifier = iExtraModifier;
			}
		}

		// only compute comparisons if we are the defender with a known attacker
		if (!bAttackingUnknownDefender)
		{
			FAssertMsg(pAttacker != this, "pAttacker is not expected to be equal with this");

			iExtraModifier = unitDefenseModifier(pAttacker->getUnitType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iDefenseModifier = iExtraModifier;
			}

			iExtraModifier = -pAttacker->unitAttackModifier(getUnitType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iAttackModifier = iExtraModifier;
			}

			iExtraModifier = religiousCombatModifierTotal(pAttacker->getReligion());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}

			iExtraModifier = -pAttacker->religiousCombatModifierTotal(getReligion());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}

			iExtraModifier = 0;
			//TB SubCombat Mod Begin
			for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = pAttacker->m_unitCombatKeyedInfo.begin(), end = pAttacker->m_unitCombatKeyedInfo.end(); it != end; ++it)
			{
				if(it->second.m_bHasUnitCombat)
				{
					iExtraModifier += unitCombatModifier(it->first);
				}
			}

			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iCombatModifierA = iExtraModifier;
			}

			iExtraModifier = 0;
			//TB SubCombat Mod Begin
			for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
			{
				if(it->second.m_bHasUnitCombat)
				{
					iExtraModifier -= pAttacker->unitCombatModifier(it->first);
				}
			}
			//TB SubCombat Mod End
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iCombatModifierT = iExtraModifier;
			}

			iExtraModifier = domainModifier(pAttacker->getDomainType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iDomainModifierA = iExtraModifier;
			}

			iExtraModifier = -pAttacker->domainModifier(getDomainType());
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iDomainModifierT = iExtraModifier;
			}

			if (pAttacker->isAnimal())
			{
				iExtraModifier = animalCombatModifier();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAnimalCombatModifierA = iExtraModifier;
				}
			}

			if (isAnimal())
			{
				iExtraModifier = -pAttacker->animalCombatModifier();
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iAnimalCombatModifierT = iExtraModifier;
				}
			}
		}

		if (!(pAttacker->isRiver()))
		{
			if (pAttacker->plot()->isRiverCrossing(directionXY(pAttacker->plot(), pAttackedPlot)))
			{
				CvCity* pCity = pAttackedPlot->getPlotCity();

				if (pCity != NULL && pAttackedPlot->isCity(true, getTeam()))
				{
					iExtraModifier = std::min(0,(pCity->getExtraRiverDefensePenalty() - GC.getRIVER_ATTACK_MODIFIER()));
				}
				else
				{
					iExtraModifier = -GC.getRIVER_ATTACK_MODIFIER();
				}
				iTempModifier += iExtraModifier;
				if (pCombatDetails != NULL)
				{
					pCombatDetails->iRiverAttackModifier = iExtraModifier;
				}
			}
		}

		if (!pAttacker->isAmphib() && !pAttackedPlot->isWater() && pAttacker->plot()->isWater())
		{
			iExtraModifier = -GC.getAMPHIB_ATTACK_MODIFIER();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iAmphibAttackModifier = iExtraModifier;
			}
		}

		if (pAttacker->getKamikazePercent() != 0)
		{
			iExtraModifier = -pAttacker->getKamikazePercent();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iKamikazeModifier = iExtraModifier;
			}
		}

		if (bSurroundedModifier && pAttacker != this /* (pAttacker != this) != bAttackingUnknownDefender */)
		{
			// the stronger the surroundings -> decrease the iModifier more
			//TB Combat Mods (S&D promos) begin
			const int iSurround = pAttacker->surroundedDefenseModifier(pAttackedPlot, this);
			iTempModifier -= std::max(0, iSurround - iSurround * dynamicDefenseTotal() / 100);
			//TB Combat Mods (S&D promos) end
		}

		if (pAttacker->attackCombatModifierTotal() != 0)
		{
			iExtraModifier = -pAttacker->attackCombatModifierTotal();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}

		if (defenseCombatModifierTotal() != 0)
		{
			iExtraModifier = defenseCombatModifierTotal();
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}

		//Defender's bonus from size diff (more)
		int iOffset = 0;
		int iPerTotal = 0;
		iOffset = pAttacker->sizeRank() - sizeRank();
		if (combatModifierPerSizeMoreTotal() != 0 && iOffset > 0)
		{
			iPerTotal = combatModifierPerSizeMoreTotal() * iOffset;
			iExtraModifier = iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}
		//Attacker's bonus from size diff (more)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = sizeRank() - pAttacker->sizeRank();
		if (pAttacker->combatModifierPerSizeMoreTotal() != 0 && iOffset > 0)
		{
			iPerTotal = pAttacker->combatModifierPerSizeMoreTotal() * iOffset;
			iExtraModifier = -iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}

		//Defender's bonus from volume diff (more)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = pAttacker->groupRank() - groupRank();
		if (combatModifierPerVolumeMoreTotal() != 0 && iOffset > 0)
		{
			iPerTotal = combatModifierPerVolumeMoreTotal() * iOffset;
			iExtraModifier = iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}
		//Attacker's bonus from volume diff (more)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = groupRank() - pAttacker->groupRank();
		if (pAttacker->combatModifierPerVolumeMoreTotal() != 0 && iOffset > 0)
		{
			iPerTotal = pAttacker->combatModifierPerVolumeMoreTotal() * iOffset;
			iExtraModifier = -iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}

		//Defender's bonus from size diff (Less)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = pAttacker->sizeRank() - sizeRank();
		if (combatModifierPerSizeLessTotal() != 0 && iOffset < 0)
		{
			iPerTotal = combatModifierPerSizeLessTotal() * -iOffset;
			iExtraModifier = iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}
		//Attacker's bonus from size diff (Less)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = sizeRank() - pAttacker->sizeRank();
		if (pAttacker->combatModifierPerSizeLessTotal() != 0 && iOffset < 0)
		{
			iPerTotal = pAttacker->combatModifierPerSizeLessTotal() * -iOffset;
			iExtraModifier = -iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}

		//Defender's bonus from volume diff (Less)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = pAttacker->groupRank() - groupRank();
		if (combatModifierPerVolumeLessTotal() != 0 && iOffset < 0)
		{
			iPerTotal = combatModifierPerVolumeLessTotal() * -iOffset;
			iExtraModifier = iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}
		//Attacker's bonus from volume diff (Less)
		iOffset = 0;
		iPerTotal = 0;
		iOffset = groupRank() - pAttacker->groupRank();
		if (pAttacker->combatModifierPerVolumeLessTotal() != 0 && iOffset < 0)
		{
			iPerTotal = pAttacker->combatModifierPerVolumeLessTotal() * -iOffset;
			iExtraModifier = -iPerTotal;
			iTempModifier += iExtraModifier;
			if (pCombatDetails != NULL)
			{
				pCombatDetails->iExtraCombatPercent += iExtraModifier;
			}
		}
		// if we are attacking an unknown defender, then use the reverse of the modifier
		if (bAttackingUnknownDefender)
		{
			iModifier -= iTempModifier;
		}
		else
		{
			iModifier += iTempModifier;
		}
	}

	if (pCombatDetails != NULL)
	{
		pCombatDetails->iModifierTotal = iModifier;
		pCombatDetails->iBaseCombatStr = iBaseCombatStr;
	}
	const int iCombat = iBaseCombatStr * (iModifier > 0 ? iModifier + 100 : 10000 / (100 - iModifier));

	if (pCombatDetails != NULL)
	{
		// Published to Python (CvUtil.py divides by 100), so these carry the ×100 NATIVE scale -- not the
		// ×10000 intermediate iCombat is computed in.
		pCombatDetails->iCombat = iCombat / 100;
		pCombatDetails->iMaxCombatStr = std::max(1, iCombat / 100);
		pCombatDetails->iCurrHitPoints = getHP();
		pCombatDetails->iMaxHitPoints = getMaxHP();
		pCombatDetails->iCurrCombatStr = ((pCombatDetails->iMaxCombatStr * pCombatDetails->iCurrHitPoints) / pCombatDetails->iMaxHitPoints);
	}

	if (pCacheEntry != NULL)
	{
		pCacheEntry->iLRUIndex = iNextCombatCacheLRU++;
		pCacheEntry->iResult = std::max(1, iCombat);
		pCacheEntry->pPlot = pPlot;
		pCacheEntry->pAttackedPlot = pAttackedPlot;
		pCacheEntry->pAttacker = pOriginalAttacker;
		pCacheEntry->pForUnit = this;
	}
	// iCombat is base(×100) × a percent factor, so it is ×10000; one reduce brings it back to the ×100 native
	// scale every consumer expects. ⛔ UNCONDITIONAL -- this used to be SIZE_MATTERS-only and only cancelled
	// because baseCombatStr() was human without that option. Gating it again returns ×10000 in a normal game.
	return std::max(1, iCombat / 100);
}


int CvUnit::currCombatStr(const CvPlot* pPlot, const CvUnit* pAttacker, CombatDetails* pCombatDetails, bool bSurroundedModifier) const
{
	const int iMaxStr = maxCombatStr(pPlot, pAttacker, pCombatDetails, bSurroundedModifier);

	return iMaxStr * getHP() / getMaxHP();
}


int CvUnit::currFirepower(const CvPlot* pPlot, const CvUnit* pAttacker) const
{
	return (maxCombatStr(pPlot, pAttacker) + currCombatStr(pPlot, pAttacker) + 1) / 2;
}

// this normalizes str by firepower, useful for quick odds calcs
// the effect is that a damaged unit will have an effective str lowered by firepower/maxFirepower
// doing the algebra, this means we mulitply by 1/2(1 + currHP)/maxHP = (maxHP + currHP) / (2 * maxHP)
int CvUnit::currEffectiveStr(const CvPlot* pPlot, const CvUnit* pAttacker, CombatDetails* pCombatDetails) const
{
	int currStr = currCombatStr(pPlot, pAttacker, pCombatDetails);

	currStr *= getMaxHP() + getHP();
	currStr /= 2 * getMaxHP();

	return currStr;
}

float CvUnit::maxCombatStrFloat(const CvPlot* pPlot, const CvUnit* pAttacker) const
{
	return maxCombatStr(pPlot, pAttacker) / 100.0f;
}


float CvUnit::currCombatStrFloat(const CvPlot* pPlot, const CvUnit* pAttacker) const
{
	return currCombatStr(pPlot, pAttacker) / 100.0f;
}


bool CvUnit::canFight() const
{
	return m_iBaseCombat100 > 0; // Don't bother calculating modifiers for this call
}

bool CvUnit::canAttackNow() const
{
	return canAttack() && (!isMadeAttack() || isBlitz());
}

bool CvUnit::canAttack() const
{
	return canFight() && !isOnlyDefensive();
}

bool CvUnit::canAttack(const CvUnit& defender) const
{
	if (!canAttack() || getOwner() == defender.getOwner())
	{
		return false;
	}
	if (defender.canCoexistWithAttacker(*this))
	{
		return false;
	}
	// Combat limit reached; breakdown combat can proceed even at combat limit.
	if (defender.getDamage() >= combatLimit() * defender.getMaxHP() / 100 && breakdownChanceTotal() <= 0)
	{
		return false;
	}

	// Artillery can't amphibious attack
	if (plot()->isWater() && !defender.plot()->isWater() && combatLimit() < 100)
	{
		return false;
	}

	if (canAttackOnlyCities() && !defender.plot()->isCity())
	{
		return false;
	}

	//tunnel fixes
	if (defender.plot()->isWater() && defender.plot()->isSeaTunnel())
	{
		//Sea units and air units and hovering units can't be attacked by land units in tunnels (Unless the land unit is hovering)
		if ((defender.getDomainType() != DOMAIN_LAND || defender.canMoveAllTerrain()) && getDomainType() == DOMAIN_LAND && !canMoveAllTerrain())
		{
			return false;
		}

		//Non-Hovering Land units can't be attacked by sea or air units or transported units or hovering units in tunnels.
		if (defender.getDomainType() == DOMAIN_LAND && !defender.canMoveAllTerrain() && (getDomainType() != DOMAIN_LAND || isCargo() || canMoveAllTerrain()))
		{
			return false;
		}
	}

	if (defender.plot()->isCity(true) && isBlendIntoCity())
	{
		if (!isAssassin())
		{
			return false;
		}
		if (defender.plot() != plot())
		{
			return false;
		}
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_AMNESTY)
	&& defender.plot()->getOwner() == getOwner()
	&& isHiddenNationality()
	&& (GET_TEAM(getTeam()).isOpenBorders(defender.getTeam()) || GET_TEAM(getTeam()).isLimitedBorders(defender.getTeam()))
	&& (!defender.canAttack() || defender.isPassage()))
	{
		return false;
	}

	return true;
}

bool CvUnit::canAmbush(const CvUnit& defender, const bool bAssassinate) const
{
	if (!canAttack() || getOwner() == defender.getOwner())
	{
		return false;
	}
	// Combat limit reached; breakdown combat can proceed even at combat limit.
	if (defender.getDamage() >= combatLimit() * defender.getMaxHP() / 100 && breakdownChanceTotal() <= 0)
	{
		return false;
	}

	if (canAttackOnlyCities() && !defender.plot()->isCity())
	{
		return false;
	}

	if (defender.plot()->isCity(true) && isBlendIntoCity())
	{
		if (!isAssassin())
		{
			return false;
		}
		if (defender.plot() != plot())
		{
			return false;
		}
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_AMNESTY)
	&& defender.plot()->getOwner() == getOwner()
	&& isHiddenNationality()
	&& (GET_TEAM(getTeam()).isOpenBorders(defender.getTeam()) || GET_TEAM(getTeam()).isLimitedBorders(defender.getTeam()))
	&& (!defender.canAttack() || defender.isPassage()))
	{
		return false;
	}

	return true;
}


bool CvUnit::canDefend(const CvPlot* pPlot) const
{
	if (!pPlot) pPlot = plot();

	if (!pPlot->isValidDomainForAction(*this) && !GC.getLAND_UNITS_CAN_ATTACK_WATER_CITIES())
	{
		return false;
	}
	return true;
}

bool CvUnit::canStealthDefend(const CvUnit* victim) const
{
	return (
		   !isDead()
		&&  canFight()
		&&  hasStealthDefense()
		&& !isCargo()
		&& !hasStatus(STATUS_PARALYZED)
		&&  isInvisible(victim->getTeam(), false, false)
		&& !victim->isInvisible(getTeam(), false, false)
		&& !canCoexistWithAttacker(*victim, true)
	);
}


bool CvUnit::canSiege(TeamTypes eTeam) const
{
	if (!canDefend())
	{
		return false;
	}

	if (!isEnemy(eTeam))
	{
		return false;
	}

	if (isInvisible(eTeam, false))
	{
		return false;
	}

	if (getOwner() == PREY_PLAYER)
	{
		return false;
	}

	if (isCargo())
	{
		return false;
	}

	return true;
}


int CvUnit::airMaxCombatStr(const CvUnit* pOther) const
{
	PROFILE_EXTRA_FUNC();
	if (airBaseCombatStr() == 0)
	{
		return 0;
	}
	int iModifier = resolvedValue(URS_STRENGTH_PERCENT) + getKamikazePercent();

	if (NULL != pOther)
	{
		for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = pOther->m_unitCombatKeyedInfo.begin(), end = pOther->m_unitCombatKeyedInfo.end(); it != end; ++it)
		{
			if (it->second.m_bHasUnitCombat)
			{
				iModifier += unitCombatModifier(it->first);
			}
		}

		iModifier += domainModifier(pOther->getDomainType());

		if (pOther->isAnimal())
		{
			iModifier += animalCombatModifier();
		}

		if (pOther->isHominid())
		{
			iModifier += vsBarbsModifier();
		}
	}

	// airBaseCombatStr() is already ×100, so the old `100 *` manufactured the scale a second time (air read
	// ×10000 against land's ×100). getModifiedIntValue is scale-preserving.
	return std::max(1, getModifiedIntValue(airBaseCombatStr(), iModifier));
}


int CvUnit::airCurrCombatStr(const CvUnit* pOther) const
{
	return airMaxCombatStr(pOther) * getHP() / getMaxHP();
}


float CvUnit::airMaxCombatStrFloat(const CvUnit* pOther) const
{
	return (((float)(airMaxCombatStr(pOther))) / 100.0f);
}


float CvUnit::airCurrCombatStrFloat(const CvUnit* pOther) const
{
	return (((float)(airCurrCombatStr(pOther))) / 100.0f);
}


int CvUnit::combatLimit(const CvUnit* pOpponent) const
{
	int iTotal = (m_pUnitInfo->getCombatLimit() + getCombatLimitChange());
	if (pOpponent != NULL)
	{
		iTotal *= pOpponent->getMaxHP();
		iTotal /= 100;
	}
	return iTotal;
}


int CvUnit::airCombatLimit(const CvUnit* pOpponent) const
{
	int iTotal = m_pUnitInfo->getAirCombatLimit();

	if (pOpponent)
	{
		iTotal *= pOpponent->getMaxHP();
		iTotal /= 100;
	}
	return iTotal;
}


bool CvUnit::canAirAttack() const
{
	return !isMadeAttack() && !hasMoved() && airBaseCombatStr() > 0;
}


bool CvUnit::canAirDefend(const CvPlot* pPlot) const
{
	if (!pPlot)
	{
		pPlot = plot();
	}

	if (isSpy() || maxInterceptionProbability() == 0)
	{
		return false;
	}

	if (getDomainType() != DOMAIN_AIR)
	{
		// Land units which are cargo cannot intercept
		if (!pPlot->isValidDomainForLocation(*this) || isCargo())
		{
			return false;
		}
	}
	return true;
}


int CvUnit::airCombatDamage(const CvUnit* pDefender) const
{
	const CvPlot* pPlot = pDefender->plot();

	const int iTheirStrength = pDefender->maxCombatStr(pPlot, this);
	const int iOurStrength = airCurrCombatStr(pDefender);
	FAssertMsg(iOurStrength > 0, "Air combat strength is expected to be greater than zero");

	const int iStrengthFactor = (iOurStrength + iTheirStrength + 1) / 2;

	return std::max(1, GC.getDefineINT("AIR_COMBAT_DAMAGE") * (iOurStrength + iStrengthFactor) / (iTheirStrength + iStrengthFactor));
}


int CvUnit::rangeCombatDamage(const CvUnit* pDefender) const
{
	const CvPlot* pPlot = pDefender->plot();

	const int iOurStrength = airCurrCombatStr(pDefender);
	FAssertMsg(iOurStrength > 0, "Combat strength is expected to be greater than zero");
	const int iTheirStrength = pDefender->maxCombatStr(pPlot, this);

	const int iStrengthFactor = ((iOurStrength + iTheirStrength + 1) / 2);

	return std::max(1, ((GC.getDefineINT("RANGE_COMBAT_DAMAGE") * (iOurStrength + iStrengthFactor)) / (iTheirStrength + iStrengthFactor)));
}


CvUnit* CvUnit::bestInterceptor(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = 0;
	CvUnit* pBestUnit = NULL;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (!isInvisible(GET_PLAYER((PlayerTypes)iI).getTeam(), false, false))
			{
				foreach_(CvUnit* pLoopUnit, GET_PLAYER((PlayerTypes)iI).units())
				{
					if (pLoopUnit->canAirDefend())
					{
						if (!pLoopUnit->isMadeInterception())
						{
							if (isEnemy(GET_PLAYER((PlayerTypes)iI).getTeam(), NULL, pLoopUnit))
							{
								if ((pLoopUnit->getDomainType() != DOMAIN_AIR) || !(pLoopUnit->hasMoved()))
								{
									if ((pLoopUnit->getDomainType() != DOMAIN_AIR) || (pLoopUnit->getGroup()->getActivityType() == ACTIVITY_INTERCEPT))
									{
										if (plotDistance(pLoopUnit->getX(), pLoopUnit->getY(), pPlot->getX(), pPlot->getY()) <= pLoopUnit->airRange())
										{
											const int iValue = pLoopUnit->currInterceptionProbability();

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestUnit = pLoopUnit;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return pBestUnit;
}


CvUnit* CvUnit::bestSeaPillageInterceptor(const CvUnit* pPillager, int iMinOdds) const
{
	PROFILE_EXTRA_FUNC();
	CvUnit* pBestUnit = NULL;

	foreach_(const CvPlot* pLoopPlot, pPillager->plot()->rect(1, 1))
	{
		foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
		{
			if (pLoopUnit->area() == pPillager->plot()->area()
			&& !pLoopUnit->isInvisible(getTeam(), false)
			&& isEnemy(pLoopUnit->getTeam())
			&& DOMAIN_SEA == pLoopUnit->getDomainType()
			&& ACTIVITY_PATROL == pLoopUnit->getGroup()->getActivityType()
			&& (NULL == pBestUnit || pLoopUnit->isBetterDefenderThan(pBestUnit, this))
			&& getCombatOdds(pPillager, pLoopUnit) < iMinOdds)
			{
				pBestUnit = pLoopUnit;
			}
		}
	}
	return pBestUnit;
}


bool CvUnit::isAutomated() const
{
	return getGroup()->isAutomated();
}


bool CvUnit::isWaiting() const
{
	return getGroup()->isWaiting();
}


bool CvUnit::isFortifyable() const
{
	if (!canFight() || isAnimal() || noDefensiveBonus() || ((getDomainType() != DOMAIN_LAND) && (getDomainType() != DOMAIN_IMMOBILE)))
	{
		return false;
	}

	return true;
}

bool CvUnit::isBuildUpable() const
{
	if (isDead())
	{
		return false;
	}
	return m_bHasBuildUp;
}


int CvUnit::fortifyModifier() const
{
	if (!isFortifyable())
	{
		return 0;
	}
	//	⛔ BOTH defines read from the CACHED set. getDefineINT("...") is a string-keyed lookup through the
	//	variable system on EVERY call, and this sits under the pathfinder's per-node cost function
	//	(generatePath -> NewPathCostFunc -> AI_compareStacks -> AI_sumStrength -> currCombatStr -> maxCombatStr),
	//	which is exactly the per-step-gate cost class [DEC-materialize-at-mapfrom] names. Its neighbour on this
	//	same line was already cached; only this one was not.
	return range(getFortifyTurns(), 0, GC.getMAX_FORTIFY_TURNS()) * GC.getFORTIFY_MODIFIER_PER_TURN();
}

int CvUnit::experienceNeeded(int iLvlOffset) const
{
	int iExperienceNeeded = calcBaseExpNeeded(getLevel() + iLvlOffset, getOwner());

	if (isCommander())
	{
		iExperienceNeeded *= 3;
		iExperienceNeeded /= 2;
	}

    if (isCommodore())
	{
		iExperienceNeeded *= 3;
		iExperienceNeeded /= 2;
	}
	return iExperienceNeeded;
}


int CvUnit::attackXPValue() const
{
	return m_pUnitInfo->getXpValueAttack();
}


int CvUnit::defenseXPValue() const
{
	return m_pUnitInfo->getXpValueDefense();
}


int CvUnit::maxXPValue(const CvUnit* pVictim, bool bBarb) const
{
	if (GC.getGame().isOption(GAMEOPTION_UNIT_INFINITE_XP) || isNPC())
	{
		return -1;
	}
	int iMaxValue = -1;

	if (pVictim != NULL && pVictim->isAnimal())
	{
		if (!isHasUnitCombat(GC.getUNITCOMBAT_EXPLORER())
		&& !isHasPromotion(GC.getPROMOTION_ANIMAL_HUNTER()))
		{
			iMaxValue = GC.getANIMAL_MAX_XP_VALUE();
		}
	}
	else if (pVictim != NULL && pVictim->isHominid() || bBarb)
	{
		if (!isHasUnitCombat(GC.getUNITCOMBAT_RECON())
		&& !isHasPromotion(GC.getPROMOTION_BARBARIAN_HUNTER()))
		{
			iMaxValue = GC.getBARBARIAN_MAX_XP_VALUE();
		}
	}
	else if (pVictim != NULL && getUnitCombatType() == GC.getUNITCOMBAT_HUNTER())
	{
		if (!isHasPromotion(GC.getPROMOTION_BARBARIAN_HUNTER())
		&& pVictim->getUnitCombatType() != GC.getUNITCOMBAT_ANIMAL())
		{
			iMaxValue = GC.getBARBARIAN_MAX_XP_VALUE();
		}
	}
	if (iMaxValue > 0 && GC.getGame().isOption(GAMEOPTION_UNIT_MORE_XP_TO_LEVEL))
	{
		iMaxValue *= GC.getMORE_XP_TO_LEVEL_MODIFIER();
		iMaxValue /= 100;
	}
	return iMaxValue;
}


// ⚠ URS_FIRST_STRIKES is a FLAT slot (×100), and a first strike is a whole COMBAT ROUND, so it reduces at this
// point of use ([DEC-fixedpoint-x100]). Returning it raw grants a hundred-plus retaliation-free rounds per
// promotion and feeds getCombatOddsImpl's binomial loops at n in the hundreds.
int CvUnit::firstStrikes() const
{
	return std::max(0, resolvedValue(URS_FIRST_STRIKES) / 100);
}


// Same FLAT-slot reduction as firstStrikes above -- a whole-round count, not a magnitude.
int CvUnit::chanceFirstStrikes() const
{
	return std::max(0, resolvedValue(URS_FIRST_STRIKE_CHANCE) / 100);
}


int CvUnit::maxFirstStrikes() const
{
	return (firstStrikes() + chanceFirstStrikes());
}


bool CvUnit::isRanged() const
{
	PROFILE_EXTRA_FUNC();
	const int groupDefinitions = getGroupDefinitions();
	for (int  i = 0; i < groupDefinitions; i++)
	{
		if ( !getArtInfo(i, GET_PLAYER(getOwner()).getCurrentEra())->getActAsRanged() )
		{
			return false;
		}
	}
	return true;
}


bool CvUnit::alwaysInvisible() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_ALWAYS_INVISIBLE) || getAlwaysInvisibleCount() > 0;
}


bool CvUnit::immuneToFirstStrikes() const
{
	return ((getUnitInfo().hasSkill(CLS_SKILL_IMMUNE_TO_FIRST_STRIKES) || getUnitInfo().hasSkill(CLS_SKILL_FIRST_STRIKE_IMMUNE)) || (getImmuneToFirstStrikesCount() > 0));
}


bool CvUnit::noDefensiveBonus() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS) || getExtraNoDefensiveBonusCount() > 0;
}

int CvUnit::getExtraNoDefensiveBonusCount() const
{
	return m_iExtraNoDefensiveBonusCount;
}

void CvUnit::changeExtraNoDefensiveBonusCount(int iChange)
{
	m_iExtraNoDefensiveBonusCount += iChange;
}

bool CvUnit::ignoreBuildingDefense() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_IGNORE_BUILDING_DEFENSE);
}


bool CvUnit::canMoveImpassable() const
{
	return (getUnitInfo().hasSkill(CLS_SKILL_CAN_MOVE_IMPASSABLE) || canFliesToMove());
}

bool CvUnit::canMoveAllTerrain() const
{
	return (getUnitInfo().hasSkill(CLS_SKILL_CAN_MOVE_ALL_TERRAIN) || canFliesToMove());
}

bool CvUnit::flatMovementCost() const
{
	//soon as the pathing engine can handle it this should be uncommented.
	return (/*canFliesToMove() ||*/ getUnitInfo().hasSkill(CLS_SKILL_FLAT_MOVEMENT_COST));
}

bool CvUnit::ignoreTerrainCost() const
{
	return (getUnitInfo().hasSkill(CLS_SKILL_IGNORE_TERRAIN_COST) || canFliesToMove());
}

bool CvUnit::isNeverInvisible() const
{
	return !alwaysInvisible() && getInvisibleType() == NO_INVISIBLE && !hasAnyInvisibilityType();
}

int CvUnit::getNoInvisibilityCount() const
{
	return GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK) * m_iNoInvisibilityCount;
}

void CvUnit::changeNoInvisibilityCount(int iChange)
{
	m_iNoInvisibilityCount += iChange;
	setHasAnyInvisibility();
}


int CvUnit::concealment() const
{
	// How well this unit hides. ONE number -- the METHOD it hides by is a SKILL, and a seeker's detection
	// against THAT method is what this is weighed against (vision.md §4: one detection type counters one
	// concealment type).
	// A BARE FETCH. The fold over the unit's own info + held promotions + held unit-combat classes happened once,
	// at the promotion / combat-class fact that moved the held set ([state-repositories.md] THE UNIT PLANE).
	return m_resolvedValues.concealment();
}

int CvUnit::detectionAgainst(int iMethodSkill) const
{
	// A BARE FETCH, per the method asked -- see concealment() above for where the fold happens.
	// ⚠ The method is a SKILL id ([skills.md]), not the retired INVISIBLE_* axis: a promotion can grant a
	// hiding method (optical camouflage), which is what makes it a skill rather than a tag.
	return m_resolvedValues.detectionAgainst(iMethodSkill);
}

bool CvUnit::isInvisible(TeamTypes eTeam, bool bDebug, bool bCheckCargo) const
{
	PROFILE_EXTRA_FUNC();
	if (bDebug && GC.getGame().isDebugMode())
	{
		return false;
	}

	if (getTeam() == eTeam)
	{
		return false;
	}

	if (alwaysInvisible())
	{
		return true;
	}

	if (bCheckCargo && isCargo())
	{
		return true;
	}

	if (isNeverInvisible())
	{
		return false;
	}

	if (isRevealed())
	{
		return false;
	}

	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		return getInvisibleType() != NO_INVISIBLE && !plot()->isSpotterInSight(eTeam, getInvisibleType());
	}

	if (hasAnyInvisibilityType())
	{
		for (int iI = GC.getNumInvisibleInfos() - 1; iI > -1; iI--)
		{
			const InvisibleTypes eInvisible = static_cast<InvisibleTypes>(iI);

			if (hasInvisibilityType(eInvisible))
			{
				if (!plot()->isSpotterInSight(eTeam, eInvisible))
				{
					return true;
				}
				// THE CONTEST (vision.md §4): this unit's concealment against the best DETECTION any of that
				// team's seers has registered here for this method. A bare fetch of the resolved block.
				const int iConcealment = concealment();

				if ((iConcealment > 0 || GC.getInvisibleInfo(eInvisible).isIntrinsic())
				&& plot()->getHighestPlotTeamVisibilityIntensity(eInvisible, eTeam) < iConcealment)
				{
					return true;
				}
			}
		}
	}
	return false;
}


bool CvUnit::isNukeImmune() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_NUKE_IMMUNE);
}


bool CvUnit::isInquisitor() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_INQUISITOR);
}


int CvUnit::maxInterceptionProbability() const
{
	return std::min(GC.getDefineINT("MAX_INTERCEPTION_PROBABILITY"),std::max(0, resolvedValue(URS_INTERCEPT)));
}


int CvUnit::currInterceptionProbability() const
{
	if (getDomainType() != DOMAIN_AIR && !GC.getGame().isModderGameOption(MODDERGAMEOPTION_BETTER_INTERCETION))
	{
		return maxInterceptionProbability();
	}
	return maxInterceptionProbability() * getHP() / getMaxHP();
}

int CvUnit::evasionProbability() const
{
	return std::min(GC.getDefineINT("MAX_EVASION_PROBABILITY"),std::max(0, resolvedValue(URS_EVASION)));
}


int CvUnit::withdrawalProbability() const
{
	if (m_bSuppressWithdrawal)
	{
		return 0;
	}
	const int iProbability = m_pUnitInfo->getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) + resolvedValue(URS_WITHDRAWAL);

	if (shouldUseWithdrawalOddsCap())
	{
		return std::min(GC.getDefineINT("MAX_WITHDRAWAL_PROBABILITY"), std::max(0, iProbability));
	}
	return std::max(0, iProbability);
}
//TB Combat Mods Begin
int CvUnit::attackCombatModifierTotal() const
{
	return (resolvedValue(URS_COMBAT_ATTACK));
}

int CvUnit::defenseCombatModifierTotal() const
{
	if (noDefensiveBonus())
	{
		return 0;
	}

	return (resolvedValue(URS_COMBAT_DEFENSE));
}

int CvUnit::vsBarbsModifier() const
{
	return (resolvedValue(URS_VS_BARBS));
}

int CvUnit::religiousCombatModifierTotal(ReligionTypes eReligion, bool bDisplay) const
{
	if (bDisplay || (getReligion() != NO_RELIGION))
	{
		if (bDisplay || getReligion() != eReligion)
		{
			return (resolvedValue(URS_RELIGIOUS_COMBAT));
		}
		else if (getReligion() == eReligion)
		{
			return -(resolvedValue(URS_RELIGIOUS_COMBAT));
		}
	}
	return 0;
}

int CvUnit::damageModifierTotal() const
{
	return std::max(-95, (resolvedValue(URS_DAMAGE_MODIFIER)));
}

int CvUnit::costModifierTotal() const
{
	return 0;
}

bool CvUnit::canStampede() const
{
	return (m_pUnitInfo->hasSkill(CLS_SKILL_STAMPEDE) || mayStampede()) && !cannotStampede();
}

bool CvUnit::canAttackOnlyCities() const
{
	int iTrueCount = 0;

	if (m_pUnitInfo->hasSkill(CLS_SKILL_ATTACK_ONLY_CITIES))
	{
		iTrueCount++;
	}
	iTrueCount += getAttackOnlyCitiesCount();

	return iTrueCount > 0;
}

bool CvUnit::canIgnoreNoEntryLevel() const
{
	int iTrueCount = 0;

	if (m_pUnitInfo->hasSkill(CLS_SKILL_IGNORE_NO_ENTRY_LEVEL))
	{
		iTrueCount++;
	}
	iTrueCount += getIgnoreNoEntryLevelCount();

	return iTrueCount > 0;
}

bool CvUnit::canIgnoreZoneofControl() const
{
	int iTrueCount = 0;

	if (m_pUnitInfo->hasSkill(CLS_SKILL_IGNORE_ZONE_OF_CONTROL))
	{
		iTrueCount++;
	}
	iTrueCount += getIgnoreZoneofControlCount();

	return iTrueCount > 0;
}

bool CvUnit::canFliesToMove() const
{
	int iTrueCount = 0;

	if (m_pUnitInfo->hasSkill(CLS_SKILL_FLIES_TO_MOVE))
	{
		iTrueCount++;
	}
	iTrueCount += getFliesToMoveCount();

	return iTrueCount > 0;
}

int CvUnit::unnerveTotal() const
{
	return std::max(0, resolvedValue(URS_UNNERVE));
}

int CvUnit::encloseTotal() const
{
	return std::max(0, resolvedValue(URS_ENCLOSE));
}

int CvUnit::lungeTotal() const
{
	return std::max(0, resolvedValue(URS_LUNGE));
}

int CvUnit::dynamicDefenseTotal() const
{
	int iData = resolvedValue(URS_DYNAMIC_DEFENSE);

	if (plot()->isCity(false, getTeam()))
	{
		iData += plot()->getPlotCity()->getExtraLocalDynamicDefense();
	}
	return std::max(0, iData);
}

//	Where wild animals may go is decided by the GAME OPTIONS (owner) -- ANIMAL_STAY_OUT bars them from national
//	borders outright, ANIMAL_DANGEROUS lets them into borders and onto improved tiles. It is deliberately NOT a
//	per-unit tier any more: the tiers were rungs of PROMOTIONLINE_FERAL read through a stored count, and a skill
//	carries no value (skills.md), so FERAL2 and FERAL3 no longer differ on territory.
bool CvUnit::canAnimalIgnoresBorders() const
{
	if (GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT))
	{
		return false;
	}
	//	The unit's OWN authored skill is headroom: no unit type authors it today (only promotions and a
	//	unit-combat do, which this read cannot see), so this leg is inert until one does.
	return GC.getGame().isOption(GAMEOPTION_ANIMAL_DANGEROUS)
		|| getUnitInfo().hasSkill(CLS_SKILL_ANIMAL_IGNORES_BORDERS);
}

bool CvUnit::canAnimalIgnoresImprovements() const
{
	return !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT)
		&& GC.getGame().isOption(GAMEOPTION_ANIMAL_DANGEROUS);
}

bool CvUnit::canAnimalIgnoresCities() const
{
	return !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT)
		&& GC.getGame().isOption(GAMEOPTION_ANIMAL_DANGEROUS);
}

bool CvUnit::canOnslaught() const
{
	return m_pUnitInfo->hasSkill(CLS_SKILL_ONSLAUGHT) || mayOnslaught();
}



//TB Combat Mods End


int CvUnit::collateralDamage() const
{
	int iTotal = resolvedValue(URS_COLLATERAL);
	return std::max(0, iTotal);
}

int CvUnit::collateralDamageLimit() const
{
	return std::max(0, ((m_pUnitInfo->getFlatCollateral(COLLATERAL_LIMIT, CASC_SCOPE_UNIT) / 100) + getCollateralDamageLimitChange()) * GC.getMAX_HIT_POINTS() / 100);
}

int CvUnit::collateralDamageMaxUnits() const
{
	return std::max(0, ((m_pUnitInfo->getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100) + getCollateralDamageMaxUnitsChange()));
}


int CvUnit::cityAttackModifier() const
{
	return resolvedValue(URS_CITY_ATTACK);
}

int CvUnit::cityDefenseModifier() const
{
	if (noDefensiveBonus())
	{
		return 0;
	}
	return (resolvedValue(URS_CITY_DEFENSE));
}

int CvUnit::cityDefenseVSOpponent(const CvUnit* pOpponent) const
{
	PROFILE_EXTRA_FUNC();
	if (noDefensiveBonus())
	{
		return 0;
	}
	const CvCity* pCity = plot()->getPlotCity();
	int iValue = 0;

	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = pOpponent->m_unitCombatKeyedInfo.begin(), end = pOpponent->m_unitCombatKeyedInfo.end(); it != end; ++it)
	{
		if (it->second.m_bHasUnitCombat)
		{
			if (plot()->isCity(false, getTeam()))
			{
				iValue += pCity->getUnitCombatDefenseAgainstModifierTotal(it->first);
			}
		}
	}
	return iValue;
}


int CvUnit::animalCombatModifier() const
{
	return resolvedValue(URS_ANIMAL_COMBAT);
}


int CvUnit::hillsAttackModifier() const
{
	return (resolvedValue(URS_HILLS_ATTACK));
}


int CvUnit::hillsDefenseModifier() const
{
	if (noDefensiveBonus())
	{
		return 0;
	}
	return (resolvedValue(URS_HILLS_DEFENSE));
}


//	THE COMMANDER RIDES ON TOP OF A UNIT EXACTLY AS A UNIT RIDES ON TOP OF A CITY ([modifier.md] §2b, owner).
//	Whichever leader supports this unit for a combat -- the commander, else the commodore -- resolved ONCE at the
//	combat seam and folded live.
//
//	⛔ IT IS DELIBERATELY NOT PART OF THE UNIT'S OWN STATE. Attaching, detaching or moving a commander is neither
//	a promotion nor a combat-class change, so NO fact would ever move a cached copy and one would be permanently
//	stale the moment the commander moved. That is why the resolved plane is commander-free by construction and
//	why this fold lives here rather than inside the per-keyed getters, where it used to re-walk the player's
//	commander list on EVERY read of every terrain, feature, domain and unit-combat row.
const CvUnit* CvUnit::supportingLeader() const
{
	if (!isCommander())
	{
		const CvUnit* pCommander = getCommander();
		if (pCommander != NULL)
		{
			return pCommander;
		}
	}
	if (!isCommodore())
	{
		return getCommodore();
	}
	return NULL;
}

int CvUnit::terrainAttackModifier(TerrainTypes eTerrain) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eTerrain);
	const CvUnit* pLeader = supportingLeader();
	return (InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, eTerrain, COMBAT_ATTACK)
		+ getExtraTerrainAttackPercent(eTerrain)
		+ (pLeader != NULL ? pLeader->getExtraTerrainAttackPercent(eTerrain) : 0));
}


int CvUnit::terrainDefenseModifier(TerrainTypes eTerrain) const
{
	if (noDefensiveBonus())
	{
		return 0;
	}
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eTerrain);
	const CvUnit* pLeader = supportingLeader();
	return (InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, eTerrain, COMBAT_DEFENSE)
		+ getExtraTerrainDefensePercent(eTerrain)
		+ (pLeader != NULL ? pLeader->getExtraTerrainDefensePercent(eTerrain) : 0));
}


int CvUnit::featureAttackModifier(FeatureTypes eFeature) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eFeature);
	const CvUnit* pLeader = supportingLeader();
	return (InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, eFeature, COMBAT_ATTACK)
		+ getExtraFeatureAttackPercent(eFeature)
		+ (pLeader != NULL ? pLeader->getExtraFeatureAttackPercent(eFeature) : 0));
}

int CvUnit::featureDefenseModifier(FeatureTypes eFeature) const
{
	if (noDefensiveBonus())
	{
		return 0;
	}
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eFeature);
	const CvUnit* pLeader = supportingLeader();
	return (InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, eFeature, COMBAT_DEFENSE)
		+ getExtraFeatureDefensePercent(eFeature)
		+ (pLeader != NULL ? pLeader->getExtraFeatureDefensePercent(eFeature) : 0));
}

int CvUnit::unitAttackModifier(UnitTypes eUnit) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);
	return InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_UNIT, eUnit, COMBAT_ATTACK);
}


int CvUnit::unitDefenseModifier(UnitTypes eUnit) const
{
	if (noDefensiveBonus())
	{
		return 0;
	}
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);
	return InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_UNIT, eUnit, COMBAT_DEFENSE);
}


int CvUnit::unitCombatModifier(UnitCombatTypes eUnitCombat) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombat);
	return (InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT, eUnitCombat, COMBAT_AMOUNT) + getExtraUnitCombatModifier(eUnitCombat, isCommander(), isCommodore()));
}


int CvUnit::domainModifier(DomainTypes eDomain) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, eDomain);
	return (InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN, eDomain, COMBAT_AMOUNT) + getExtraDomainModifier(eDomain));
}


int CvUnit::cargoSpace() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		int iCargoCapacity = SMcargoSpaceFilter();

		//	The empire's own hold allowances, by carrier kind. FLAT slots, so each reduces at its point of use
		//	before the rank scaling ([DEC-fixedpoint-x100]).
		int aiCargo[NUM_CARGO_KINDS];
		GET_PLAYER(getOwner()).getCargoKinds(aiCargo);

		if (getDomainType() == DOMAIN_SEA)
		{
			iCargoCapacity += applySMRank(aiCargo[CARGO_NAVAL] / 100,
				getSizeMattersSpacialOffsetValue(),
				GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER());
		}
		const SpecialUnitTypes eMissile = (SpecialUnitTypes)GC.getInfoTypeForString("SPECIALUNIT_MISSILE");
		if (getSpecialCargo() == eMissile)
		{
			iCargoCapacity += applySMRank(aiCargo[CARGO_MISSILE] / 100,
				getSizeMattersSpacialOffsetValue(),
				GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER());
		}
		return iCargoCapacity;
	}
	int iCargoCapacity = m_pUnitInfo->getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 + m_iCargoCapacity;

	int aiCargo[NUM_CARGO_KINDS];
	GET_PLAYER(getOwner()).getCargoKinds(aiCargo);

	if (getDomainType() == DOMAIN_SEA)
	{
		iCargoCapacity += aiCargo[CARGO_NAVAL] / 100;
	}
	if (getSpecialCargo() == GC.getSPECIALUNIT_MISSILE())
	{
		iCargoCapacity += aiCargo[CARGO_MISSILE] / 100;
	}
	return std::max(0, iCargoCapacity);
}

void CvUnit::changeCargoSpace(int iChange)
{
	if (iChange != 0)
	{
		m_iCargoCapacity += iChange;
		setInfoBarDirty(true);
	}
}

bool CvUnit::isFull() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return SMgetCargo() >= cargoSpace();
	}
	return getCargo() >= cargoSpace();
}


int CvUnit::cargoSpaceAvailable(SpecialUnitTypes eSpecialCargo, DomainTypes eDomainCargo) const
{
	if (getSpecialCargo() != NO_SPECIALUNIT && getSpecialCargo() != eSpecialCargo)
	{
		return 0;
	}
	if (getDomainCargo() != NO_DOMAIN && getDomainCargo() != eDomainCargo)
	{
		return 0;
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		if  (eSpecialCargo != NO_SPECIALUNIT && getSMNotSpecialCargo() == eSpecialCargo)
		{
			return 0;
		}
		return std::max(0, cargoSpace() - SMgetCargo());
	}
	return std::max(0, cargoSpace() - getCargo());
}


bool CvUnit::hasCargo() const
{
	return SMgetCargo() > 0 || getCargo() > 0;
}


bool CvUnit::canCargoAllMove() const
{
	return algo::none_of(plot()->units(),
		CvUnit::fn::getTransportUnit() == this &&
		CvUnit::fn::getDomainType() == DOMAIN_LAND &&
		!CvUnit::fn::canMove()
	);
}

bool CvUnit::canCargoEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const
{
	return algo::none_of(plot()->units(),
		CvUnit::fn::getTransportUnit() == this &&
		!CvUnit::fn::canEnterArea(eTeam, pArea, bIgnoreRightOfPassage)
	);
}

int CvUnit::getUnitAICargo(UnitAITypes eUnitAI) const
{
	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	return algo::count_if(aCargoUnits, bind(CvUnit::AI_getUnitAIType, _1) == eUnitAI);
}


int CvUnit::getID() const
{
	return m_iID;
}


int CvUnit::getIndex() const
{
	return (getID() & FLTA_INDEX_MASK);
}


IDInfo CvUnit::getIDInfo() const
{
	IDInfo unit(getOwner(), getID());
	return unit;
}


void CvUnit::setID(int iID)
{
	m_iID = iID;
}


int CvUnit::getGroupID() const
{
	return m_iGroupID;
}


bool CvUnit::isInGroup() const
{
	return(getGroupID() != FFreeList::INVALID_INDEX);
}


bool CvUnit::isGroupHead() const
{
	return getGroup()->getHeadUnit() == this;
}


CvSelectionGroup* CvUnit::getGroup() const
{
	return GET_PLAYER(getOwner()).getSelectionGroup(getGroupID());
}

bool CvUnit::canJoinGroup(const CvPlot* pPlot, const CvSelectionGroup* pSelectionGroup) const
{
	// do not allow someone to join a group that is about to be split apart
	// this prevents a case of a never-ending turn
	if (pSelectionGroup->AI_isForceSeparate())
	{
		return false;
	}
	const CvUnit* pHeadUnit = pSelectionGroup->getHeadUnit();

	if (pSelectionGroup->getOwner() == NO_PLAYER)
	{
		if (pHeadUnit != NULL && pHeadUnit->getOwner() != getOwner())
		{
			return false;
		}
	}
	else if (pSelectionGroup->getOwner() != getOwner())
	{
		return false;
	}

	if (pSelectionGroup->getNumUnits() > 0)
	{
		if (pPlot != NULL && !pSelectionGroup->atPlot(pPlot))
		{
			return false;
		}

		//	Can't join a group that is loaded onto a transport as this
		//	would bypass the transport's record of what units it has on
		//	board
		if (pHeadUnit->isCargo())
		{
			if (pHeadUnit->isHuman() && pHeadUnit->getTransportUnit() == getTransportUnit())
			{
				return true;
			}
			return false;
		}
		//TB Note: Although it seems very unusual, checking for null pPlot allows this check to be safe for inter-domain upgrades.
		//this is important for immobile units to land or sea or air.  Could also enable some units to go from land or sea to air or vice versa.
		//if pPlot is NULL, the only known cause would be that the unit is, in fact, upgrading.
		if (pPlot != NULL && pSelectionGroup->getDomainType() != getDomainType())
		{
			return false;
		}
	}

	return true;
}


void CvUnit::joinGroup(CvSelectionGroup* pSelectionGroup, bool bRemoveSelected, bool bRejoin)
{
	PROFILE_FUNC();

	CvSelectionGroup* pOldSelectionGroup = GET_PLAYER(getOwner()).getSelectionGroup(getGroupID());
	CvSelectionGroup* pNewSelectionGroup = NULL;

	if (pSelectionGroup != pOldSelectionGroup && pSelectionGroup != NULL
	&& pSelectionGroup->getHeadUnit() != NULL
	&& pSelectionGroup->getHeadUnit()->isWaitingOnUnitAI((int)AI_getUnitAIType()))
	{
		pSelectionGroup->getHeadUnit()->setToWaitOnUnitAI(AI_getUnitAIType(), false);
	}

	if (pSelectionGroup != pOldSelectionGroup || pOldSelectionGroup == NULL)
	{
		CvPlot* pPlot = plot();

		if (pSelectionGroup != NULL)
		{
			pNewSelectionGroup = pSelectionGroup;
		}
		else if (bRejoin)
		{
			const int iSelectionRegroup = GET_PLAYER(getOwner()).getSelectionRegroup();

			if (iSelectionRegroup != NULL)
			{
				pNewSelectionGroup = GET_PLAYER(getOwner()).getSelectionGroup(iSelectionRegroup);

				if (!canJoinGroup(pPlot, pNewSelectionGroup))
				{
					pNewSelectionGroup = GET_PLAYER(getOwner()).addSelectionGroup();
					FAssert(pNewSelectionGroup != NULL);
					pNewSelectionGroup->init(pNewSelectionGroup->getID(), getOwner());
				}
			}
			else
			{
				pNewSelectionGroup = GET_PLAYER(getOwner()).addSelectionGroup();
				FAssert(pNewSelectionGroup != NULL);
				pNewSelectionGroup->init(pNewSelectionGroup->getID(), getOwner());
			}
		}

		if (pNewSelectionGroup == NULL || canJoinGroup(pPlot, pNewSelectionGroup))
		{
			if (pOldSelectionGroup != NULL)
			{
				const bool bWasHead = !isHuman()
					&& pOldSelectionGroup->getNumUnits() > 1
					&& pOldSelectionGroup->getHeadUnit() == this;

				pOldSelectionGroup->removeUnit(this);

				// if we were the head, if the head unitAI changed, then force the group to separate (non-humans)
				if (bWasHead)
				{
					if (isWaitingOnUnitAIAny())
					{
						for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
						{
							if (isWaitingOnUnitAI(iI))
							{
								setToWaitOnUnitAI((UnitAITypes)iI, false);
							}
						}
					}
					FAssert(pOldSelectionGroup->getHeadUnit() != NULL);

					if (!pOldSelectionGroup->isChoosingNewLeader()
					&& pOldSelectionGroup->getHeadUnit()->AI_getUnitAIType() != AI_getUnitAIType()
					// Special case to try to hold together city attacks that are breaking up but can still succeed
					// If we have lost the last city_attack AI unit see if we have a unit that COULD take over in the SAME role
					&& (UNITAI_ATTACK_CITY != AI_getUnitAIType() || !pOldSelectionGroup->findNewLeader(UNITAI_ATTACK_CITY)))
					{
						pOldSelectionGroup->AI_makeForceSeparate();
					}
				}
			}

			if (pNewSelectionGroup != NULL)
			{
				if (pNewSelectionGroup->getHeadUnit() != NULL && pNewSelectionGroup->getHeadUnit()->isWaitingOnUnitAI((int)AI_getUnitAIType()))
				{
					pNewSelectionGroup->getHeadUnit()->setToWaitOnUnitAI(AI_getUnitAIType(), false);
				}

				//	Normal rules apply when we join someone else's group unless
				//	the priority chnage was actually to DOWNgrade our priority
				if (AI_groupFirstVal() != LEADER_PRIORITY_MIN)
				{
					AI_setLeaderPriority(-1);
				}

				m_iGroupID = pNewSelectionGroup->getID();

				if (!pNewSelectionGroup->addUnit(this, false))
				{
					m_iGroupID = FFreeList::INVALID_INDEX;
				}
			}
			else
			{
				//	Normal rules apply when we are alone again
				AI_setLeaderPriority(-1);

				m_iGroupID = FFreeList::INVALID_INDEX;
			}

			if (getGroup() != NULL)
			{
				if (getGroup()->getNumUnits() > 1)
				{
					if (getGroup()->canAllMove())
					{
						getGroup()->setActivityType(ACTIVITY_AWAKE);
					}
				}
				else
				{
					GET_PLAYER(getOwner()).updateGroupCycle(this, false);
				}
			}

			if (pPlot != NULL && getTeam() == GC.getGame().getActiveTeam())
			{
				pPlot->setFlagDirty(true);
			}

			if (pPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
			{
				gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
			}
		}

		if (bRemoveSelected && IsSelected())
		{
			gDLL->getInterfaceIFace()->removeFromSelectionList(this);
		}
	}
}


int CvUnit::getHotKeyNumber()
{
	return m_iHotKeyNumber;
}


void CvUnit::getUnitRead(int (&unitRead)[NUM_UNIT_READS])
{
	unitRead[UNIT_READ_TYPE]                = (int)getUnitType();
	unitRead[UNIT_READ_DOMAIN]              = (int)getDomainType();
	unitRead[UNIT_READ_LEVEL]               = getLevel();
	unitRead[UNIT_READ_HP]                  = getHP();
	unitRead[UNIT_READ_MAX_HP]              = getMaxHP();
	unitRead[UNIT_READ_MOVES_LEFT]          = movesLeft();
	unitRead[UNIT_READ_BASE_MOVES]          = baseMoves();
	//	x100 native, like every amount -- the reader divides. The float-returning legacy accessor did that
	//	division inside the DLL, which is the presentation layer's arithmetic on the wrong side of the boundary.
	unitRead[UNIT_READ_EXPERIENCE]          = getExperience100();
	unitRead[UNIT_READ_EXPERIENCE_NEEDED]   = experienceNeeded();
	unitRead[UNIT_READ_HOTKEY_NUMBER]       = getHotKeyNumber();
	unitRead[UNIT_READ_BASE_COMBAT]         = baseCombatStr();
	unitRead[UNIT_READ_AIR_BASE_COMBAT]     = airBaseCombatStr();

	unitRead[UNIT_READ_CONTROL_POINTS]                = 0;
	unitRead[UNIT_READ_CONTROL_POINTS_LEFT]           = 0;
	unitRead[UNIT_READ_COMMODORE_CONTROL_POINTS]      = 0;
	unitRead[UNIT_READ_COMMODORE_CONTROL_POINTS_LEFT] = 0;
	if (getCommanderComp() != NULL)
	{
		unitRead[UNIT_READ_CONTROL_POINTS]      = getCommanderComp()->getControlPoints();
		unitRead[UNIT_READ_CONTROL_POINTS_LEFT] = getCommanderComp()->getControlPointsLeft();
	}
	if (getCommodoreComp() != NULL)
	{
		unitRead[UNIT_READ_COMMODORE_CONTROL_POINTS]      = getCommodoreComp()->getControlPoints();
		unitRead[UNIT_READ_COMMODORE_CONTROL_POINTS_LEFT] = getCommodoreComp()->getControlPointsLeft();
	}

	//	The ORDER state lives on the selection group, which is where Civ4 keeps it.
	unitRead[UNIT_READ_DAMAGE]             = getDamage();
	unitRead[UNIT_READ_FACING_DIRECTION]   = (int)getFacingDirection(false);
	unitRead[UNIT_READ_LEADER_UNIT_TYPE]   = (int)getLeaderUnitType();
	unitRead[UNIT_READ_UNIT_AI]            = (int)AI_getUnitAIType();
	unitRead[UNIT_READ_CAPTURE_UNIT_TYPE]  = (int)getCaptureUnitType();
	unitRead[UNIT_READ_CAPTURE_PROBABILITY]= captureProbabilityTotal();
	unitRead[UNIT_READ_CAPTURE_RESISTANCE] = captureResistanceTotal();
	unitRead[UNIT_READ_COMBAT_CLASS]       = (int)getUnitCombatType();
	unitRead[UNIT_READ_PILLAGE_CHANGE]     = getPillageChange();
	unitRead[UNIT_READ_NUKE_RANGE]         = nukeRange();
	unitRead[UNIT_READ_GROUP_ID]           = -1;
	unitRead[UNIT_READ_ACTIVITY]             = (int)NO_ACTIVITY;
	unitRead[UNIT_READ_AUTOMATE]             = (int)NO_AUTOMATE;
	unitRead[UNIT_READ_MISSION]              = (int)NO_MISSION;
	unitRead[UNIT_READ_MISSION_QUEUE_LENGTH] = 0;
	const CvSelectionGroup* pGroup = getGroup();
	if (pGroup != NULL)
	{
		unitRead[UNIT_READ_GROUP_ID]             = pGroup->getID();
		unitRead[UNIT_READ_ACTIVITY]             = (int)pGroup->getActivityType();
		unitRead[UNIT_READ_AUTOMATE]             = (int)pGroup->getAutomateType();
		unitRead[UNIT_READ_MISSION_QUEUE_LENGTH] = pGroup->getLengthMissionQueue();
		if (unitRead[UNIT_READ_MISSION_QUEUE_LENGTH] > 0)
		{
			unitRead[UNIT_READ_MISSION] = pGroup->getMissionType(0);
		}
	}
}


void CvUnit::getUnitFlags(int (&flags)[NUM_UNIT_FLAGS]) const
{
	flags[UNIT_FLAG_SELECTED]        = IsSelected() ? 1 : 0;
	flags[UNIT_FLAG_IN_BATTLE]       = isInBattle() ? 1 : 0;
	flags[UNIT_FLAG_WAITING]         = isWaiting() ? 1 : 0;
	flags[UNIT_FLAG_HURT]            = isHurt() ? 1 : 0;
	flags[UNIT_FLAG_FORTIFYABLE]     = isFortifyable() ? 1 : 0;
	flags[UNIT_FLAG_COMMANDER]       = isCommander() ? 1 : 0;
	flags[UNIT_FLAG_COMMODORE]       = isCommodore() ? 1 : 0;
	flags[UNIT_FLAG_PROMOTION_READY] = isPromotionReady() ? 1 : 0;
	flags[UNIT_FLAG_HAS_MOVED]       = hasMoved() ? 1 : 0;
	flags[UNIT_FLAG_CAN_MOVE]        = canMove() ? 1 : 0;
	flags[UNIT_FLAG_CAN_FIGHT]       = canFight() ? 1 : 0;
	flags[UNIT_FLAG_CAN_AIR_ATTACK]  = canAirAttack() ? 1 : 0;
	flags[UNIT_FLAG_MADE_ATTACK]     = isMadeAttack() ? 1 : 0;
	flags[UNIT_FLAG_DEAD]            = isDead() ? 1 : 0;
}


void CvUnit::setHotKeyNumber(int iNewValue)
{
	PROFILE_EXTRA_FUNC();
	FAssert(getOwner() != NO_PLAYER);

	if (getHotKeyNumber() != iNewValue)
	{
		if (iNewValue != -1)
		{
			foreach_(CvUnit* pLoopUnit, GET_PLAYER(getOwner()).units())
			{
				if (pLoopUnit->getHotKeyNumber() == iNewValue)
				{
					pLoopUnit->setHotKeyNumber(-1);
				}
			}
		}

		m_iHotKeyNumber = iNewValue;

		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}


int CvUnit::getViewportX() const
{
	const CvViewport* pCurrentViewPort = GC.getCurrentViewport();
	FAssert(pCurrentViewPort != NULL);

	return pCurrentViewPort->getViewportXFromMapX(m_iX);
}


int CvUnit::getViewportY() const
{
	const CvViewport* pCurrentViewPort = GC.getCurrentViewport();
	FAssert(pCurrentViewPort != NULL);

	return pCurrentViewPort->getViewportYFromMapY(m_iY);
}

bool CvUnit::isInViewport() const
{
	return GC.getCurrentViewport()->isInViewport(m_iX, m_iY);
}

bool CvUnit::isTempUnit() const
{
	return GET_PLAYER(getOwner()).isTempUnit(this);
}

void CvUnit::setXY(int iX, int iY, bool bGroup, bool bUpdate, bool bShow, bool bCheckPlotVisible, bool bInit)
{
	PROFILE_FUNC();
	/*GC.getGame().logOOSSpecial(1, getID(), iX, iY);*/

	{ // Toffer - Problems arise when units have illegal coordinates that are not specifically INVALID_PLOT_COORD.
		//	No Idea why it is set up so that plot() only returns NULL for INVALID_PLOT_COORD but not for other illegal coordinates.
		const int iMaxX = GC.getMap().getGridWidth();
		const int iMaxY = GC.getMap().getGridHeight();
		if (iX < 0 || iX >= iMaxX || iY < 0 || iY >= iMaxY)
		{
			if (iX != INVALID_PLOT_COORD || iY != INVALID_PLOT_COORD)
			{
				FErrorMsg("Illegal coordinates given to unit");
				iX = INVALID_PLOT_COORD;
				iY = INVALID_PLOT_COORD;
			}
		}
	}

	// Temp units do not really exist, and are just used to provide a data anchor for virtual pathing calculations.
	// As such they do not need to process their position into the wider game state and indeed should not without additional concurrency protection.
	if (isTempUnit())
	{
		m_iX = iX;
		m_iY = iY;

		if (!getGroup())
		{
			joinGroup(NULL);
		}
		/*GC.getGame().logOOSSpecial(2, getID(), iX, iY);*/
		return;
	}
	const PlayerTypes eMyPlayer = getOwner();
	CvPlayer& myPlayer = GET_PLAYER(eMyPlayer);

	// If a unit moves we need to flush any combat str cache entries relating to it
	FlushCombatStrCache(this);

	/*
	// OOS!! Temporary for Out-of-Sync madness debugging...
	if (GC.getLogging())
	{
		PROFILE("CvUnit::setXY.OOSLogging");

		char szOut[1024];
		sprintf(szOut, "Player %d Unit %d (%S's %S) moving from %d:%d to %d:%d\n", eMyPlayer, getID(), myPlayer.getNameKey(), getName().GetCString(), getX(), getY(), iX, iY);
		gDLL->messageControlLog(szOut);
	}
	*/

	if (isInBattle())
	{
		setCombatUnit(NULL);
	}
	if (at(iX, iY))
	{
		return;
	}
	FAssert(iX == INVALID_PLOT_COORD || GC.getMap().plot(iX, iY)->getX() == iX);
	FAssert(iY == INVALID_PLOT_COORD || GC.getMap().plot(iX, iY)->getY() == iY);

	// Activity before moving to the new plot
	const ActivityTypes eOldActivityType = getGroup() ? getGroup()->getActivityType() : NO_ACTIVITY;

	setBlockading(false);

	if (!bGroup || isCargo())
	{
		bShow = false;
	}

	CvPlot* pNewPlot = GC.getMap().plot(iX, iY);
	CvPlot* pOldPlot = plot();

	//	Koshling - Forcing the unit into a new group causes rapid cycling through the group id
	//	space, which is a scaling issue, so only do it when necessary
	//	Note - it used o do this unconditionally for cargo and changing that behavior
	//	might be dangerous, but it solves some scaling problems and I cannot think of a reason why
	//	it should be problematics, nor is it causing any issues in test cases I have tried
	if (!bGroup && (!getGroup() || getGroup()->getNumUnits() > 1))
	{
		// Need valid plot() for joinGroup() so set our position now
		if (bInit || pOldPlot == nullptr)
		{
			m_iX = iX;
			m_iY = iY;
		}
		joinGroup(NULL, true);
	}

	if (pNewPlot)
	{
		PROFILE("CvUnit::setXY.NewPlot");

		CvUnit* pTransportUnit = getTransportUnit();

		if (pTransportUnit && !pTransportUnit->atPlot(pNewPlot))
		{
			setTransportUnit(NULL); // Departed from transport
		}

		if (!bInit && pOldPlot && canFight() && !isCargo())
		{
			///TB: This next portion is to reset the plot list of the new plot before moving on after units may (probably were) have been destroyed in combat there.
			OutputDebugString(CvString::format("%S (%d) CvUnit::setXY (%d,%d)\n", getDescription().c_str(), m_iID, m_iX, m_iY).c_str());
			foreach_(CvUnit* unitX, pNewPlot->units_safe())
			{
				if (unitX->isDead())
					continue;

				if ((isEnemy(unitX->getTeam(), pNewPlot) || unitX->isEnemy(getTeam())) && !unitX->canCoexistWithAttacker(*this, true))
				{
					if (!unitX->canDefend(pNewPlot) && !unitX->isInvisible(getTeam(), false) && !unitX->isCargo())
					{
						//TB NOTE: This is where units that can't defend themselves are auto-captured IF the unit has a defined capture tag and cannot defend.
						if (!isNoCapture() && NO_UNIT != unitX->getUnitInfo().getCaptures())
						{
							if (isHiddenNationality() || unitX->isHiddenNationality())
							{
								GET_TEAM(unitX->getTeam()).changeWarWeariness(getTeam(), *pNewPlot, GC.getDefineINT("WW_UNIT_CAPTURED"));
								GET_TEAM(getTeam()).changeWarWeariness(unitX->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_UNIT"));
								GET_TEAM(getTeam()).AI_changeWarSuccess(unitX->getTeam(), GC.getDefineINT("WAR_SUCCESS_UNIT_CAPTURING"));
							}
							unitX->setCapturingPlayer(eMyPlayer);
							unitX->setCapturingUnit(this);
						}
						unitX->kill(true, eMyPlayer, true);
					}
				}
			}
		}

		if (pNewPlot->isGoody(getTeam()) && !isNPC())
		{
			myPlayer.doGoody(pNewPlot, this);
		}

		pNewPlot->area()->changeUnitsPerPlayer(eMyPlayer, 1);
		pNewPlot->area()->changePower(eMyPlayer, getPowerValueTotal());

		if (AI_getUnitAIType() != NO_UNITAI)
		{
			pNewPlot->area()->changeNumAIUnits(eMyPlayer, AI_getUnitAIType(), 1);
			pNewPlot->area()->changeEffNumAIUnitsTimes100(eMyPlayer, AI_getUnitAIType(), SMeffectiveCount());
		}

		if (isAnimal())
		{
			pNewPlot->area()->changeAnimalsPerPlayer(eMyPlayer, 1);
		}

		if (pNewPlot->getTeam() != getTeam() && (pNewPlot->getTeam() == NO_TEAM || !GET_TEAM(pNewPlot->getTeam()).isVassal(getTeam())))
		{
			myPlayer.changeNumOutsideUnits(1);
		}
	}

	if (pOldPlot)
	{
		PROFILE("CvUnit::setXY.OldPlot");

		pOldPlot->removeUnit(this, bUpdate && !hasCargo());

		setFortifyTurns(0);

		pOldPlot->changeAdjacentSight(getTeam(), sight(pOldPlot), false, this, true);
		changeDebugCount(-1);

		pOldPlot->area()->changeUnitsPerPlayer(eMyPlayer, -1);
		pOldPlot->area()->changePower(eMyPlayer, -getPowerValueTotal());

		if (AI_getUnitAIType() != NO_UNITAI)
		{
			pOldPlot->area()->changeNumAIUnits(eMyPlayer, AI_getUnitAIType(), -1);
			pOldPlot->area()->changeEffNumAIUnitsTimes100(eMyPlayer, AI_getUnitAIType(), -SMeffectiveCount());
		}

		if (isAnimal())
		{
			pOldPlot->area()->changeAnimalsPerPlayer(eMyPlayer, -1);
		}

		if (pOldPlot->getTeam() != getTeam() && (pOldPlot->getTeam() == NO_TEAM || !GET_TEAM(pOldPlot->getTeam()).isVassal(getTeam())))
		{
			myPlayer.changeNumOutsideUnits(-1);
		}

		setLastMoveTurn(GC.getGame().getTurnSlice());

		CvCity* pOldCity = pOldPlot->getPlotCity();

		if (pOldCity)
		{
			if (isMilitaryHappiness())
			{
				pOldCity->changeMilitaryHappinessUnits(-1);
			}
			pOldCity->noteUnitMoved(this);
			// The LEAVE twin of emitUnitEnteredCity. ⚠ Unconditional on the old city, while the entry is announced
			// only on the friendly branch (the enemy branch resolves into an acquisition instead of an entry), so
			// the two do not net to occupancy -- a consumer needing that reads the unit's live plot.
			emitUnitLeftCity((int)getUnitType(), getID(), (int)getOwner(), pOldCity->getID());
		}

		{
			CvCity* pWorkingCity = pOldPlot->getWorkingCity();

			if (pWorkingCity && canSiege(pWorkingCity->getTeam()))
			{
				pWorkingCity->AI_setAssignWorkDirty(true);
			}

			if (pOldPlot->isWater())
			{
				foreach_(const CvPlot* pLoopPlot, pOldPlot->adjacent() | filtered(CvPlot::fn::isWater()))
				{
					pWorkingCity = pLoopPlot->getWorkingCity();

					if (pWorkingCity && canSiege(pWorkingCity->getTeam()))
					{
						pWorkingCity->AI_setAssignWorkDirty(true);
					}
				}
			}
		}

		if (pOldPlot->isActiveVisible(true))
		{
			pOldPlot->updateMinimapColor();
		}

		if (pOldPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
		{
			gDLL->getInterfaceIFace()->verifyPlotListColumn();

			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}

	if (pNewPlot)
	{
		m_iX = pNewPlot->getX();
		m_iY = pNewPlot->getY();
	}
	else
	{
		m_iX = INVALID_PLOT_COORD;
		m_iY = INVALID_PLOT_COORD;
	}

	FAssertMsg(plot() == pNewPlot, "plot is expected to equal pNewPlot");

	if (pNewPlot)
	{
		PROFILE("CvUnit::setXY.NewPlot2");

		CvCity* pNewCity = pNewPlot->getPlotCity();

		if (pNewCity) //Again... is bUpdate only for when the unit is not cargo?
		{
			PROFILE("CvUnit::setXY.NewPlot2.NewCity");

			if (!myPlayer.isAnimal()
			&& !isBlendIntoCity()
			&& !isNoCapture()
			&& !isCargo()
			&& canFight()
			&& isEnemy(pNewCity->getTeam())
			&& (!isBarbCoExist() || !pNewPlot->isHominid())
			&& (!isHiddenNationality() || !pNewCity->isNPC())
			&& !canCoexistAlwaysOnPlot(*pNewPlot)
			&& !pNewPlot->hasDefender(false, NO_PLAYER, getOwner(), this, true, false, false, true))
			{
				GET_TEAM(getTeam()).changeWarWeariness(pNewCity->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_CITY"));

				// Double war success if capturing capital city, always a significant blow to enemy
				// pNewCity still points to old city here, hasn't been acquired yet
				GET_TEAM(getTeam()).AI_changeWarSuccess(pNewCity->getTeam(), (pNewCity->isCapital() ? 2 : 1)*GC.getWAR_SUCCESS_CITY_CAPTURING());


				const PlayerTypes eNewOwner =
				(
					isHiddenNationality()
					?
					BARBARIAN_PLAYER
					:
					myPlayer.pickConqueredCityOwner(*pNewCity)
				);
				if (NO_PLAYER != eNewOwner)
				{
					GET_PLAYER(eNewOwner).acquireCity(pNewCity, true, false, true); // will delete the pointer
					pNewCity = NULL;
				}
			}
			else
			{
				pNewCity->noteUnitMoved(this);
				// The unit entered a FRIENDLY city: the targeted trigger for what the city hands to units present
				// in it (building free promotions). Emitted here rather than at CvPlot::addUnit so the stream
				// carries a rare fact (a city entry) instead of every unit move.
				emitUnitEnteredCity((int)getUnitType(), getID(), (int)getOwner(), pNewCity->getID());
			}
		}

		// Koshling - modified a little to merge Super Forts logic
		const ImprovementTypes eImprovement = pNewPlot->getImprovementType();

		if (eImprovement != NO_IMPROVEMENT && GC.getImprovementInfo(eImprovement).hasCharacteristic(CLS_CHARACTERISTIC_ACTS_AS_CITY) && !isNoCapture()
		&& !isBlendIntoCity() && !isHiddenNationality() && !myPlayer.isAnimal() && !isCargo())
		{
			PROFILE("CvUnit::setXY.NewPlot2.ActAsCity");

			if (pNewPlot->getOwner() != NO_PLAYER)
			{
				const CvPlayer& pNewPlotOwner = GET_PLAYER(pNewPlot->getOwner());

				if ((isEnemy(pNewPlotOwner.getTeam()) || !pNewPlotOwner.isAlive())
				&& (!isBarbCoExist() || !pNewPlot->isHominid())
				&& !canCoexistWithTeamOnPlot(pNewPlotOwner.getTeam(), *pNewPlot) && canFight())
				{
					AddDLLMessage(
						pNewPlot->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
						gDLL->getText("TXT_KEY_MISC_CITY_CAPTURED_BY", GC.getImprovementInfo(eImprovement).getText(), myPlayer.getCivilizationDescriptionKey()),
						"AS2D_CITYCAPTURED", MESSAGE_TYPE_MAJOR_EVENT, GC.getImprovementInfo(eImprovement).getButton(), GC.getCOLOR_RED(), pNewPlot->getX(), pNewPlot->getY(), true, true
					);
					myPlayer.acquireFort(pNewPlot);
				}
			}
			else myPlayer.acquireFort(pNewPlot);
		}

		//update facing direction
		if (pOldPlot)
		{
			const DirectionTypes newDirection = estimateDirection(pOldPlot, pNewPlot);
			if (newDirection != NO_DIRECTION)
				m_eFacingDirection = newDirection;
		}

		//update cargo mission animations
		if (isCargo())
		{
			PROFILE("CvUnit::setXY.NewPlot2.Cargo");

			if (eOldActivityType != ACTIVITY_MISSION)
			{
				getGroup()->setActivityType(eOldActivityType);
			}
		}

		pNewPlot->changeAdjacentSight(getTeam(), sight(pNewPlot), true, this, true); // needs to be here so that the square is considered visible when we move into it...
		changeDebugCount(1);
		pNewPlot->addUnit(this, bUpdate && !hasCargo());

		if (!bInit && shouldLoadOnMove(pNewPlot))
		{
			PROFILE("CvUnit::setXY.NewPlot2.Load");

			load();
		}
		{
			PROFILE("CvUnit::setXY.NewPlot2.Meet");

			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (GET_TEAM((TeamTypes)iI).isAlive() && !isInvisible((TeamTypes)iI, false) && pNewPlot->isVisible((TeamTypes)iI, false))
				{
					GET_TEAM((TeamTypes)iI).meet(getTeam(), true);
				}
			}
		}

		{
			CvCity* pNewCity = pNewPlot->getPlotCity();

			if (pNewCity && isMilitaryHappiness())
			{
				pNewCity->changeMilitaryHappinessUnits(1);
			}
		}
		{
			CvCity* pWorkingCity = pNewPlot->getWorkingCity();

			if (pWorkingCity)
			{
				PROFILE("CvUnit::setXY.NewPlot2.WorkingCity");

				if (canSiege(pWorkingCity->getTeam()))
				{
					pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pNewPlot));
				}
			}

			if (pNewPlot->isWater())
			{
				PROFILE("CvUnit::setXY.NewPlot2.Water");

				foreach_(const CvPlot* pLoopPlot, pNewPlot->adjacent() | filtered(CvPlot::fn::isWater()))
				{
					pWorkingCity = pLoopPlot->getWorkingCity();

					if (pWorkingCity && canSiege(pWorkingCity->getTeam()))
					{
						pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pLoopPlot));
					}
				}
			}
		}

		if (pNewPlot->isActiveVisible(true))
		{
			pNewPlot->updateMinimapColor();
		}

		{
			PROFILE("CvUnit::setXY.NewPlot2.Visibility");

			//	⛔ THIS GATE DECIDES WHETHER THE MODEL FOLLOWS. With it false, NEITHER branch below runs and the
			//	scene node is never told the unit left — it keeps drawing on the tile it came from.
			const bool bGraphicsInitialized = GC.IsGraphicsInitialized();
			const bool bInViewport = isInViewport();
			const bool bWatched = pNewPlot->isVisibleToWatchingHuman();
			GfxMoveOutcome eOutcome = GFX_MOVE_SKIPPED;

			if (bGraphicsInitialized && bInViewport)
			{
				// Override bShow if check plot visible
				if (bShow || bCheckPlotVisible && bWatched)
				{
					QueueMove(pNewPlot);
					eOutcome = GFX_MOVE_QUEUED;
				}
				else
				{
					SetPosition(pNewPlot);
					eOutcome = GFX_MOVE_TELEPORTED;
				}
			}
			gfxTraceMove(pOldPlot ? pOldPlot->getX() : -1, pOldPlot ? pOldPlot->getY() : -1,
				pNewPlot->getX(), pNewPlot->getY(), eOutcome,
				bGraphicsInitialized, bInViewport, isRealEntity(getEntity()), bShow, bWatched);
		}

		if (pNewPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
		{
			gDLL->getInterfaceIFace()->verifyPlotListColumn();
			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}

		// Pillage on move: Only if unowned tile, or at war
		if (isPillageOnMove() && pNewPlot->getImprovementType() != NO_IMPROVEMENT)
		{
			if (!pNewPlot->isOwned() || atWar(getTeam(), GET_PLAYER(pNewPlot->getOwner()).getTeam()))
			{
				pillage(true);
			}
		}
	}

	if (pOldPlot && hasCargo())
	{
		PROFILE("CvUnit::setXY.OldPlot2");

		std::vector<CvUnit*> cargoUnits;

		foreach_(CvUnit* pLoopUnit, pOldPlot->units())
		{
			if (pLoopUnit->getTransportUnit() == this)
			{
				//GC.getGame().logOOSSpecial(22, pLoopUnit->getID(), iX, iY);
				cargoUnits.push_back(pLoopUnit);
			}
		}
		foreach_(CvUnit* pLoopUnit, cargoUnits)
		{
			pLoopUnit->setXY(iX, iY, bGroup, false);
		}
#ifdef _DEBUG
		foreach_(const CvUnit* pLoopUnit, pOldPlot->units())
		{
			if (pLoopUnit->getTransportUnit() == this)
			{
				pLoopUnit->getGroup()->validateLocations();
			}
		}
#endif
	}

	if (bUpdate)// && hasCargo())
	{
		PROFILE("CvUnit::setXY.updateCenter");

		if (pOldPlot)
		{
			pOldPlot->updateCenterUnit();
			pOldPlot->setFlagDirty(true);
		}
		if (pNewPlot)
		{
			pNewPlot->updateCenterUnit();
			pNewPlot->setFlagDirty(true);
		}
	}

	const bool bFarMove =
	(
		!pOldPlot || !pNewPlot
		||
		3 < stepDistance(pOldPlot->getX(), pOldPlot->getY(), pNewPlot->getX(), pNewPlot->getY())
	);

	FAssert(pOldPlot != pNewPlot);
	myPlayer.updateGroupCycle(this, bFarMove);

	if (pNewPlot)
	{
		setHasAnyInvisibility();

		if ((pOldPlot && pOldPlot->isInViewport()) != pNewPlot->isInViewport()
		|| g_bUseDummyEntities
		&& (pOldPlot && pOldPlot->isActiveVisible(false)) != pNewPlot->isActiveVisible(false))
		{
			reloadEntity();
		}
	}

	setInfoBarDirty(true);

	if (IsSelected())
	{
		if (isFound())
		{
			gDLL->getInterfaceIFace()->setDirty(GlobeLayer_DIRTY_BIT, true);

			if (!isUsingDummyEntities() && isInViewport())
			{
				gDLL->getEngineIFace()->updateFoundingBorder();
			}
		}
		gDLL->getInterfaceIFace()->setDirty(ColoredPlots_DIRTY_BIT, true);
	}

	//update glow
	if (pNewPlot && !isUsingDummyEntities() && isInViewport())
	{
		gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity());
	}
	/*GC.getGame().logOOSSpecial(5, getID(), iX, iY);*/
}


bool CvUnit::at(int iX, int iY) const
{
	return getX() == iX && getY() == iY;
}


bool CvUnit::atPlot(const CvPlot* pPlot) const
{
	return plot() == pPlot;
}


CvPlot* CvUnit::plot() const
{
	//FAssertMsg(isInViewport(), "Can't get plot of unit that is not in the viewport");
	//FAssertMsg(!isUsingDummyEntities(), "Can't get plot of unit that is using dummy entities");
	return GC.getMap().plotSorenINLINE(getX(), getY());
}


/*DllExport*/ CvPlot* CvUnit::plotExternal() const
{
#ifdef _DEBUG
	OutputDebugString("exe is asking for the plot of this unit\n");
#endif
	FAssertMsg(isInViewport(), "Can't get plot of unit that is not in the viewport");
	FAssertMsg(!isUsingDummyEntities(), "Can't get plot of unit that is using dummy entities");
	return GC.getMap().plotSorenINLINE(getX(), getY());
}


int CvUnit::getArea() const
{
	return plot()->getArea();
}


CvArea* CvUnit::area() const
{
	return plot()->area();
}


int CvUnit::getLastMoveTurn() const
{
	return m_iLastMoveTurn;
}

void CvUnit::setLastMoveTurn(int iNewValue)
{
	m_iLastMoveTurn = iNewValue;
	FASSERT_NOT_NEGATIVE(m_iLastMoveTurn);
}


CvPlot* CvUnit::getReconPlot() const
{
	return GC.getMap().plotSorenINLINE(m_iReconX, m_iReconY);
}


void CvUnit::setReconPlot(CvPlot* pNewPlot)
{
	CvPlot* pOldPlot = getReconPlot();

	if (pOldPlot != pNewPlot)
	{
		if (pOldPlot)
		{
			pOldPlot->changeAdjacentSight(getTeam(), GC.getRECON_VISIBILITY_RANGE() * VISION_OPEN_GROUND_COST, false, this, true);
			pOldPlot->changeReconCount(-1); // changeAdjacentSight() tests for getReconCount()
			changeDebugCount(-1);
		}

		if (pNewPlot)
		{
			m_iReconX = pNewPlot->getX();
			m_iReconY = pNewPlot->getY();

			pNewPlot->changeReconCount(1); // changeAdjacentSight() tests for getReconCount()
			pNewPlot->changeAdjacentSight(getTeam(), GC.getRECON_VISIBILITY_RANGE() * VISION_OPEN_GROUND_COST, true, this, true);
			changeDebugCount(1);
		}
		else
		{
			m_iReconX = INVALID_PLOT_COORD;
			m_iReconY = INVALID_PLOT_COORD;
		}
	}
}


int CvUnit::getGameTurnCreated() const
{
	return m_iGameTurnCreated;
}


void CvUnit::setGameTurnCreated(int iNewValue)
{
	FASSERT_NOT_NEGATIVE(iNewValue);

	m_iGameTurnCreated = iNewValue;
}


int CvUnit::getDamage() const
{
	return m_iDamage;
}

int CvUnit::getHealAsDamage(UnitCombatTypes eHealAsType) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eHealAsType);

	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eHealAsType);

	return info ? info->m_iHealAsDamage : 0;
}

void CvUnit::changeHealAsDamage(UnitCombatTypes eHealAsType, int iChange, PlayerTypes ePlayer)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eHealAsType);

	if (iChange != 0)
	{
		const UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo(eHealAsType);

		const int iNewValue = (info->m_iHealAsDamage + iChange);

		setHealAsDamage(eHealAsType, range(iNewValue, 0, getMaxHP()), ePlayer);

		FASSERT_NOT_NEGATIVE(info->m_iHealAsDamage);
	}
}

void CvUnit::setHealAsDamage(UnitCombatTypes eHealAsType, int iNewValue, PlayerTypes ePlayer, bool bNotifyEntity)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eHealAsType);

	UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo(eHealAsType);

	info->m_iHealAsDamage = range(iNewValue, 0, getMaxHP());

	int iHighestDamage = 0;
	std::vector<UnitCombatTypes> kHealAsTypes;
	healAsUnitCombats(kHealAsTypes);
	for (size_t iHealAs = 0; iHealAs < kHealAsTypes.size(); ++iHealAs)
	{
		const UnitCombatKeyedInfo* info2 = findOrCreateUnitCombatKeyedInfo(kHealAsTypes[iHealAs]);
		if (info2->m_iHealAsDamage > iHighestDamage)
		{
			iHighestDamage = info2->m_iHealAsDamage;
		}
	}
	if (iHighestDamage != getDamage())
	{
		setDamage(iHighestDamage, ePlayer, bNotifyEntity, NO_UNITCOMBAT, true);
	}
	FASSERT_NOT_NEGATIVE(info->m_iHealAsDamage);
}

int CvUnit::getDamagePercent() const
{
	return 100 * m_iDamage / getMaxHP();
}

void CvUnit::setupPreCombatDamage()
{
	m_iPreCombatDamage = m_iDamage;
}

int CvUnit::getPreCombatDamage() const
{
	return m_iPreCombatDamage;
}

void CvUnit::setDamage(int iNewValue, PlayerTypes ePlayer, bool bNotifyEntity, UnitCombatTypes eHealAsType, bool bSecondPass)
{
	PROFILE_EXTRA_FUNC();
	const int iOldValue = getDamage();

	std::vector<UnitCombatTypes> kHealAsTypes;
	healAsUnitCombats(kHealAsTypes);
	if (eHealAsType == NO_UNITCOMBAT && !bSecondPass && !kHealAsTypes.empty())
	{
		for (size_t iHealAs = 0; iHealAs < kHealAsTypes.size(); ++iHealAs)
		{
			setHealAsDamage(kHealAsTypes[iHealAs], iNewValue, ePlayer, bNotifyEntity);
		}
	}
	else if (eHealAsType != NO_UNITCOMBAT && !kHealAsTypes.empty())
	{
		setHealAsDamage(eHealAsType, iNewValue, ePlayer, bNotifyEntity);
	}
	else
	{
		m_iDamage = range(iNewValue, 0, getMaxHP());

		if (iOldValue != getDamage())
		{
			if (GC.getGame().isFinalInitialized() && bNotifyEntity)
			{
				NotifyEntity(MISSION_DAMAGE);
			}

			setInfoBarDirty(true);

			if (IsSelected())
			{
				gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
			}

			if (plot() == gDLL->getInterfaceIFace()->getSelectionPlot())
			{
				gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
			}
		}
	}
	FAssertMsg(getHP() >= 0, "getHP() is expected to be non-negative (invalid Index)");

	if (isDead())
	{
		kill(true, ePlayer);
	}
}


void CvUnit::healAsUnitCombats(std::vector<UnitCombatTypes>& healAsTypes) const
{
	healAsTypes.clear();
	std::vector<HealByUnitCombat> healRows;
	InfoValuation::collectHealByUnitCombat(m_pUnitInfo->getModifiers(), healRows);
	for (size_t iRow = 0; iRow < healRows.size(); ++iRow)
	{
		healAsTypes.push_back((UnitCombatTypes)healRows[iRow].iUnitCombat);
	}
}

void CvUnit::changeDamage(int iChange, PlayerTypes ePlayer, UnitCombatTypes eHealAsType)
{
	PROFILE_EXTRA_FUNC();
	std::vector<UnitCombatTypes> kHealAsTypes;
	healAsUnitCombats(kHealAsTypes);
	if (eHealAsType == NO_UNITCOMBAT && !kHealAsTypes.empty())
	{
		for (size_t iHealAs = 0; iHealAs < kHealAsTypes.size(); ++iHealAs)
		{
			changeHealAsDamage(kHealAsTypes[iHealAs], iChange, ePlayer);
		}
	}
	else if (!kHealAsTypes.empty())
	{
		changeHealAsDamage(eHealAsType, iChange, ePlayer);
	}
	else setDamage(getDamage() + iChange, ePlayer);
}

void CvUnit::changeDamagePercent(int iChange, PlayerTypes ePlayer)
{
	setDamage((getDamagePercent() + iChange) * getMaxHP() / 100, ePlayer);
}


int CvUnit::getMoves() const
{
	return m_iMoves;
}


void CvUnit::setMoves(int iNewValue)
{
	if (m_iMoves != iNewValue)
	{
		CvPlot* pPlot = plot();

		m_iMoves = iNewValue;

		FASSERT_NOT_NEGATIVE(m_iMoves);

		if (pPlot && getTeam() == GC.getGame().getActiveTeam())
		{
			pPlot->setFlagDirty(true);
		}

		if (IsSelected())
		{
			if (canMove())
			{
				gDLL->getFAStarIFace()->ForceReset(&GC.getInterfacePathFinder());
				gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
			}
			/* Toffer - make it a bug option - Unselect units upon expending all movement points.
			else if (getGroup()->canAnyMove())
			{
				gDLL->getInterfaceIFace()->removeFromSelectionList(this);
			}
			*/
		}

		if (pPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
		{
			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}
	}
}


void CvUnit::changeMoves(int iChange)
{
	setMoves(m_iMoves + iChange);
}


void CvUnit::finishMoves()
{
	setMoves(maxMoves());
}


int CvUnit::getExperience100() const
{
	return m_iExperience;
}

void CvUnit::setExperience100(int iNewValue)
{
	if (iNewValue < 0) return; // Integer overflow protection

	if (m_iExperience != iNewValue)
	{
		m_iExperience = iNewValue;

		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}

void CvUnit::changeExperience100(int iChange, int iMax, bool bFromCombat, bool bInBorders, bool bUpdateGlobal)
{
	if (bFromCombat)
	{
		if (iChange < 1) return; // Never lose xp from battle.

		CvPlayer& kPlayer = GET_PLAYER(getOwner());

		int aiScalars[NUM_INFO_SCALARS];
		kPlayer.getScalars(aiScalars);

		int iMod = getExperiencePercent();
		int iModGG = aiScalars[SCALAR_GREAT_GENERAL_RATE];

		if (bInBorders)
		{
			int aiExperience[NUM_EXPERIENCE_KINDS];
			kPlayer.getExperienceKinds(aiExperience);
			iMod += aiExperience[EXPERIENCE_IN_BORDER];
			iModGG += aiScalars[SCALAR_GREAT_GENERAL_RATE_DOMESTIC] + aiExperience[EXPERIENCE_IN_BORDER];
		}
		iChange = getModifiedIntValue(iChange, iMod);

		if (bUpdateGlobal)
		{
			kPlayer.changeFractionalCombatExperience(getModifiedIntValue(iChange, iModGG), getGGExperienceEarnedTowardsType());
		}

		if (getUsedCommander())
		{
			getUsedCommander()->changeExperience100(60, iMax); //0.6 xp every time, make global define?
			m_iUsedCommanderID = -1;
		}

		if (getUsedCommodore())
        {
        	getUsedCommodore()->changeExperience100(60, iMax); //0.6 xp every time, make global define?
        	m_iUsedCommodoreID = -1;
        }
	}
	if (iChange == 0)
	{
		return;
	}
	// Toffer - Maybe its redundant to support XP reductions with this function?
	//	However, I do like to be thorough, so here goes.
	if (iMax > -1)
	{
		if (iChange > 0)
		{
			if (getExperience100() >= iMax)
			{
				return;
			}
			if (getExperience100() + iChange >= iMax)
			{
				setExperience100(iMax);
				return;
			}
		}
		else // Reduction, and iMax is then considered a iMin value.
		{
			if (getExperience100() <= iMax)
			{
				return;
			}
			if (getExperience100() + iChange <= iMax)
			{
				setExperience100(iMax);
				return;
			}
		}
	}
	setExperience100(getExperience100() + iChange);
}

int CvUnit::getExperience() const
{
	return getExperience100() / 100;
}

void CvUnit::setExperience(int iNewValue)
{
	setExperience100(iNewValue * 100);
}

void CvUnit::changeExperience(int iChange, int iMax, bool bFromCombat, bool bInBorders, bool bUpdateGlobal)
{
	changeExperience100(iChange * 100, iMax * 100, bFromCombat, bInBorders, bUpdateGlobal);
}


int CvUnit::getLevel() const
{
	return m_iLevel;
}

void CvUnit::setLevel(int iNewValue)
{
	FAssert(iNewValue > 0);

	if (m_iLevel != iNewValue)
	{
		m_iLevel = iNewValue;

		if (iNewValue > GET_PLAYER(getOwner()).getHighestUnitLevel())
		{
			GET_PLAYER(getOwner()).setHighestUnitLevel(iNewValue);
		}

		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}
	}
}

void CvUnit::changeLevel(int iChange)
{
	setLevel(getLevel() + iChange);
}

int CvUnit::getCargo() const
{
	return m_iCargo;
}

int CvUnit::SMgetCargo() const
{
	return m_iSMCargo;
}

void CvUnit::changeCargo(int iChange)
{
	m_iCargo += iChange;
	FAssertRecalcMsg(getCargo() >= 0, "Transported units is less than 0");
}

void CvUnit::SMchangeCargo(int iChange)
{
	m_iSMCargo += iChange;
	FAssertOptionRecalcMsg(GAMEOPTION_COMBAT_SIZE_MATTERS, SMgetCargo() >= 0, "Transported cargo is less than 0");
}

void CvUnit::getCargoUnits(std::vector<CvUnit*>& aUnits) const
{
	PROFILE_EXTRA_FUNC();
	aUnits.clear();

#if FASSERT_ENABLE
	int iCheck = 0;
#endif
	if (hasCargo())
	{
		foreach_(CvUnit* pLoopUnit, plot()->units())
		{
			if (pLoopUnit->getTransportUnit() == this)
			{
				aUnits.push_back(pLoopUnit);
#if FASSERT_ENABLE
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
				{
					iCheck += pLoopUnit->SMCargoVolume();
				}
				else
				{
					iCheck++;
				}
#endif
			}
		}
	}

	FAssertOptionRecalcMsg(GAMEOPTION_COMBAT_SIZE_MATTERS, SMgetCargo() == iCheck, "Cargo size doesn't match expectations");
	FAssertRecalcMsg(getCargo() == aUnits.size(), "Number of cargo units found doesn't match cached number");
}

void CvUnit::validateCargoUnits()
{
	PROFILE_EXTRA_FUNC();
#if FASSERT_ENABLE
	int iCheck = 0;
	int iCount = 0;
	const CvPlot* pPlot = plot();

	if (hasCargo())
	{
		foreach_(const CvUnit* pLoopUnit, pPlot->units())
		{
			if (pLoopUnit->getTransportUnit() == this)
			{
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
				{
					iCheck += pLoopUnit->SMCargoVolume();
					iCount++;
				}
				else
				{
					iCheck++;
					iCount++;
				}
			}
		}
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		//TB: Backend cleanup - Assert is coming up and we have ghost loads so this is a quickfix only - still evaluating to see how this takes place
		if (SMgetCargo() != iCheck)
		{
			FErrorMsg("Load Volume is incorrect");
			m_iSMCargo = 0;
			foreach_(const CvUnit* pLoopUnit, pPlot->units())
			{
				if (pLoopUnit->getTransportUnit() == this)
				{
					SMchangeCargo(pLoopUnit->SMCargoVolume());
				}
			}
			//rerun check
			iCheck = 0;
			iCount = 0;
			foreach_(const CvUnit* pLoopUnit, pPlot->units())
			{
				if (pLoopUnit->getTransportUnit() == this)
				{
					if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
					{
						iCheck += pLoopUnit->SMCargoVolume();
						iCount++;
					}
					else
					{
						iCheck++;
						iCount++;
					}
				}
			}
			if (SMgetCargo() != iCheck)
			{
				FErrorMsg("Load Volume is incorrect");//If this persists, then the problem is due to having a unit that's not sharing the same plot being loaded onto the transport.
				std::vector<CvUnit*> aCargoUnits;
				getCargoUnits(aCargoUnits);
				foreach_(CvUnit* pCargo, aCargoUnits)
				{
					pCargo->setTransportUnit(NULL);
					if (pCargo->plot() == plot())
					{
						pCargo->setTransportUnit(this);
					}
				}
				m_iSMCargo = 0;
				iCount = 0;
				foreach_(const CvUnit* pCargo, aCargoUnits)
				{
					SMchangeCargo(pCargo->SMCargoVolume());
					iCount++;
				}
			}
		}
	}
	else
	{
		FAssert(iCheck == iCount);
	}
	FAssert(getCargo() == iCount);
#endif
}

CvPlot* CvUnit::getAttackPlot() const
{
	return GC.getMap().plotSorenINLINE(m_iAttackPlotX, m_iAttackPlotY);
}


void CvUnit::setAttackPlot(const CvPlot* pNewValue, bool bAirCombat)
{
	if (getAttackPlot() != pNewValue)
	{
		if (pNewValue)
		{
			m_iAttackPlotX = pNewValue->getX();
			m_iAttackPlotY = pNewValue->getY();
		}
		else
		{
			m_iAttackPlotX = INVALID_PLOT_COORD;
			m_iAttackPlotY = INVALID_PLOT_COORD;
		}
	}

	m_bAirCombat = bAirCombat;
}

bool CvUnit::isAirCombat() const
{
	return m_bAirCombat;
}

int CvUnit::getCombatTimer() const
{
	return m_iCombatTimer;
}

void CvUnit::setCombatTimer(int iNewValue)
{
	m_iCombatTimer = iNewValue;
	FASSERT_NOT_NEGATIVE(getCombatTimer());
}

void CvUnit::changeCombatTimer(int iChange)
{
	setCombatTimer(getCombatTimer() + iChange);
}

int CvUnit::getCombatFirstStrikes() const
{
	return m_iCombatFirstStrikes;
}

void CvUnit::setCombatFirstStrikes(int iNewValue)
{
	m_iCombatFirstStrikes = iNewValue;
	FASSERT_NOT_NEGATIVE(getCombatFirstStrikes());
}

void CvUnit::changeCombatFirstStrikes(int iChange)
{
	setCombatFirstStrikes(getCombatFirstStrikes() + iChange);
}

int CvUnit::getFortifyTurns() const
{
	return m_iFortifyTurns;
}

void CvUnit::setFortifyTurns(int iNewValue)
{
	const int iMaxFortify = GC.getMAX_FORTIFY_TURNS();

	iNewValue = range(iNewValue, 0, iMaxFortify);

	if (iNewValue != m_iFortifyTurns)
	{
		m_iFortifyTurns = iNewValue;
		setInfoBarDirty(true);

		if (iNewValue == 0 && isBuildUp())
		{
			clearBuildups();
		}
	}
}

int CvUnit::getBlitzCount() const
{
	return m_iBlitzCount;
}

bool CvUnit::isBlitz() const
{
	return m_iBlitzCount > 0;
}

void CvUnit::changeBlitzCount(int iChange)
{
	m_iBlitzCount += iChange;
	FASSERT_NOT_NEGATIVE(m_iBlitzCount);
}

int CvUnit::getAmphibCount() const
{
	return m_iAmphibCount;
}

bool CvUnit::isAmphib() const
{
	return (getAmphibCount() > 0 || canFliesToMove());
}

void CvUnit::changeAmphibCount(int iChange)
{
	m_iAmphibCount += iChange;
	FASSERT_NOT_NEGATIVE(getAmphibCount());
}

int CvUnit::getRiverCount() const
{
	return m_iRiverCount;
}

bool CvUnit::isRiver() const
{
	return (getRiverCount() > 0 || canFliesToMove());
}

void CvUnit::changeRiverCount(int iChange)
{
	m_iRiverCount += iChange;
	FASSERT_NOT_NEGATIVE(getRiverCount());
}

int CvUnit::getEnemyRouteCount() const
{
	return m_iEnemyRouteCount;
}

bool CvUnit::isEnemyRoute() const
{
	return getEnemyRouteCount() > 0;
}

void CvUnit::changeEnemyRouteCount(int iChange)
{
	m_iEnemyRouteCount += iChange;
	FASSERT_NOT_NEGATIVE(getEnemyRouteCount());
}

int CvUnit::getAlwaysHealCount() const
{
	return m_iAlwaysHealCount;
}

bool CvUnit::isAlwaysHeal() const
{
	return (getAlwaysHealCount() > 0);
}

void CvUnit::changeAlwaysHealCount(int iChange)
{
	m_iAlwaysHealCount += iChange;
	FASSERT_NOT_NEGATIVE(getAlwaysHealCount());
}

int CvUnit::getHillsDoubleMoveCount() const
{
	return m_iHillsDoubleMoveCount;
}

bool CvUnit::isHillsDoubleMove() const
{
	return (getHillsDoubleMoveCount() > 0);
}

void CvUnit::changeHillsDoubleMoveCount(int iChange)
{
	m_iHillsDoubleMoveCount += iChange;
	FASSERT_NOT_NEGATIVE(getHillsDoubleMoveCount());
}

int CvUnit::getImmuneToFirstStrikesCount() const
{
	return m_iImmuneToFirstStrikesCount;
}

void CvUnit::changeImmuneToFirstStrikesCount(int iChange)
{
	m_iImmuneToFirstStrikesCount += iChange;
	FASSERT_NOT_NEGATIVE(getImmuneToFirstStrikesCount());
}


int CvUnit::getAlwaysInvisibleCount() const
{
	return m_iAlwaysInvisibleCount;
}

void CvUnit::changeAlwaysInvisibleCount(int iChange)
{
	m_iAlwaysInvisibleCount += iChange;
	FASSERT_NOT_NEGATIVE(getAlwaysInvisibleCount());
}


int CvUnit::getDefensiveVictoryMoveCount() const
{
	return m_iDefensiveVictoryMoveCount;
}

bool CvUnit::isDefensiveVictoryMove() const
{
	return (getDefensiveVictoryMoveCount() > 0);
}

void CvUnit::changeDefensiveVictoryMoveCount(int iChange)
{
	m_iDefensiveVictoryMoveCount += iChange;
	FASSERT_NOT_NEGATIVE(getDefensiveVictoryMoveCount());
}


int CvUnit::getFreeDropCount() const
{
	return m_iFreeDropCount;
}

bool CvUnit::isFreeDrop() const
{
	return (getFreeDropCount() > 0);
}

void CvUnit::changeFreeDropCount(int iChange)
{
	m_iFreeDropCount += iChange;
	FASSERT_NOT_NEGATIVE(getFreeDropCount());
}


int CvUnit::getOffensiveVictoryMoveCount() const
{
	return m_iOffensiveVictoryMoveCount;
}

bool CvUnit::isOffensiveVictoryMove() const
{
	return (getOffensiveVictoryMoveCount() > 0);
}

void CvUnit::changeOffensiveVictoryMoveCount(int iChange)
{
	m_iOffensiveVictoryMoveCount += iChange;
	FASSERT_NOT_NEGATIVE(getOffensiveVictoryMoveCount());
}


int CvUnit::getOneUpCount() const
{
	return m_iOneUpCount;
}

bool CvUnit::isOneUp() const
{
	return getOneUpCount() > 0;
}

void CvUnit::changeOneUpCount(int iChange)
{
	m_iOneUpCount += iChange;
	FASSERT_NOT_NEGATIVE(getOneUpCount());
}

int CvUnit::getPillageEspionageCount() const
{
	return m_iPillageEspionageCount;
}

bool CvUnit::isPillageEspionage() const
{
	return getPillageEspionageCount() > 0;
}

void CvUnit::changePillageEspionageCount(int iChange)
{
	m_iPillageEspionageCount += iChange;
	FASSERT_NOT_NEGATIVE(getPillageEspionageCount());
}


int CvUnit::getPillageMarauderCount() const
{
	return m_iPillageMarauderCount;
}

bool CvUnit::isPillageMarauder() const
{
	return getPillageMarauderCount() > 0;
}

void CvUnit::changePillageMarauderCount(int iChange)
{
	m_iPillageMarauderCount += iChange;
	FASSERT_NOT_NEGATIVE(getPillageMarauderCount());
}


int CvUnit::getPillageOnMoveCount() const
{
	return m_iPillageOnMoveCount;
}

bool CvUnit::isPillageOnMove() const
{
	return getPillageOnMoveCount() > 0 && !isCargo();
}

void CvUnit::changePillageOnMoveCount(int iChange)
{
	m_iPillageOnMoveCount += iChange;
	FASSERT_NOT_NEGATIVE(getPillageOnMoveCount());
}


int CvUnit::getPillageOnVictoryCount() const
{
	return m_iPillageOnVictoryCount;
}

bool CvUnit::isPillageOnVictory() const
{
	return getPillageOnVictoryCount() > 0;
}

void CvUnit::changePillageOnVictoryCount(int iChange)
{
	m_iPillageOnVictoryCount += iChange;
	FASSERT_NOT_NEGATIVE(getPillageOnVictoryCount());
}


int CvUnit::getPillageResearchCount() const
{
	return m_iPillageResearchCount;
}

bool CvUnit::isPillageResearch() const
{
	return (getPillageResearchCount() > 0);
}

void CvUnit::changePillageResearchCount(int iChange)
{
	m_iPillageResearchCount += iChange;
	FASSERT_NOT_NEGATIVE(getPillageResearchCount());
}


int CvUnit::getCelebrityHappy() const
{
	return m_iCelebrityHappy;
}

void CvUnit::changeCelebrityHappy(int iChange)
{
	if (iChange != 0)
	{
		m_iCelebrityHappy += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getCollateralDamageLimitChange() const
{
	return m_iCollateralDamageLimitChange;
}

void CvUnit::changeCollateralDamageLimitChange(int iChange)
{
	if (iChange != 0)
	{
		m_iCollateralDamageLimitChange += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getCollateralDamageMaxUnitsChange() const
{
	return m_iCollateralDamageMaxUnitsChange;
}

void CvUnit::changeCollateralDamageMaxUnitsChange(int iChange)
{
	if (iChange != 0)
	{
		m_iCollateralDamageMaxUnitsChange += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getCombatLimitChange() const
{
	return m_iCombatLimitChange;
}

void CvUnit::changeCombatLimitChange(int iChange)
{
	if (iChange != 0)
	{
		m_iCombatLimitChange += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraDropRange() const
{
	return m_iExtraDropRange;
}

void CvUnit::changeExtraDropRange(int iChange)
{
	if (iChange != 0)
	{
		m_iExtraDropRange += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getSurvivorChance() const
{
	return m_iSurvivorChance;
}

void CvUnit::changeSurvivorChance(int iChange)
{
	if (iChange != 0)
	{
		m_iSurvivorChance += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getVictoryAdjacentHeal() const
{
	return resolvedValue(URS_HEAL_VICTORY_ADJACENT) / 100;
}

int CvUnit::getVictoryHeal() const
{
	// A RESOLVED SLOT like every other heal magnitude -- gathered over the unit's own info + held promotions +
	// held unit-combat classes when the promotion landed, never pushed in by a per-source changer.
	return resolvedValue(URS_HEAL_VICTORY) / 100;
}

int CvUnit::getVictoryStackHeal() const
{
	return resolvedValue(URS_HEAL_VICTORY_STACK) / 100;
}


int CvUnit::getExtraMoves() const
{
	return m_iExtraMoves;
}

void CvUnit::changeExtraMoves(int iChange)
{
	m_iExtraMoves += iChange;
	m_iMaxMoveCacheTurn--;

	FASSERT_NOT_NEGATIVE(m_iExtraMoves);
}

int CvUnit::getExtraMoveDiscount() const
{
	return m_iExtraMoveDiscount;
}

void CvUnit::changeExtraMoveDiscount(int iChange)
{
	m_iExtraMoveDiscount += iChange;
	FASSERT_NOT_NEGATIVE(m_iExtraMoveDiscount);
}


//TB Combat Mods Begin
// Toffer - Upkeep
void CvUnit::calcUpkeep()
{
	if (isNPC())
	{
		return;
	}
	// Flat only, by ruling: a unit costs its base upkeep plus any flat extra. The percentage stages that
	// used to multiply this (the promotion/unit-combat upkeep modifier and the Size-Matters rank
	// multiplier) are GONE -- see superseded-ideas.
	const int iCalc = 100 * m_pUnitInfo->getUpkeepCost() + resolvedValue(URS_UPKEEP_EXTRA);
	const int iOldUpkeep = m_iUpkeep100;
	m_iUpkeep100 = std::max(0, iCalc);
	if (m_iUpkeep100 != iOldUpkeep)
	{
		GET_PLAYER(getOwner()).setUnitUpkeepDirty();
	}
}

int CvUnit::getUpkeep() const
{
	return m_iUpkeep100;
}

// ! Upkeep


int CvUnit::getStampedeCount() const
{
	return m_iStampedeCount;
}

bool CvUnit::cannotStampede() const
{
	return getStampedeCount() < 0;
}

bool CvUnit::mayStampede() const
{
	return getStampedeCount() > 0;
}

void CvUnit::changeStampedeCount(int iChange)
{
	m_iStampedeCount += iChange;
}

int CvUnit::getAttackOnlyCitiesCount() const
{
	return m_iAttackOnlyCitiesCount;
}

void CvUnit::setAttackOnlyCitiesCount(int iChange)
{
	m_iAttackOnlyCitiesCount = iChange;
}

void CvUnit::changeAttackOnlyCitiesCount(int iChange)
{
	m_iAttackOnlyCitiesCount += iChange;
}

int CvUnit::getIgnoreNoEntryLevelCount() const
{
	return m_iIgnoreNoEntryLevelCount;
}

void CvUnit::setIgnoreNoEntryLevelCount(int iChange)
{
	m_iIgnoreNoEntryLevelCount = iChange;
}

void CvUnit::changeIgnoreNoEntryLevelCount(int iChange)
{
	m_iIgnoreNoEntryLevelCount += iChange;
}

int CvUnit::getIgnoreZoneofControlCount() const
{
	return m_iIgnoreZoneofControlCount;
}

void CvUnit::changeIgnoreZoneofControlCount(int iChange)
{
	m_iIgnoreZoneofControlCount += iChange;
}

int CvUnit::getFliesToMoveCount() const
{
	return m_iFliesToMoveCount;
}

void CvUnit::setFliesToMoveCount(int iChange)
{
	m_iFliesToMoveCount = iChange;
}

void CvUnit::changeFliesToMoveCount(int iChange)
{
	m_iFliesToMoveCount += iChange;
}

int CvUnit::getOnslaughtCount() const
{
	return m_iOnslaughtCount;
}

bool CvUnit::mayOnslaught() const
{
	return getOnslaughtCount() > 0;
}

void CvUnit::changeOnslaughtCount(int iChange)
{
	m_iOnslaughtCount += iChange;
	FASSERT_NOT_NEGATIVE(getOnslaughtCount());
}







//TB Combat Mods End

//WorkRateMod
int CvUnit::hillsWorkModifier() const
{
	return isWorker() ? m_worker->getHillsWorkModifier() : 0;
}

int CvUnit::peaksWorkModifier() const
{
	return isWorker() ? m_worker->getPeaksWorkModifier() : 0;
}

int CvUnit::getWorkModifier() const
{
	return isWorker() ? m_worker->getWorkModifier() : 0;
}

int CvUnit::getExtraWorkModForBuild(const BuildTypes eBuild) const
{
	return isWorker() ? m_worker->getExtraWorkModForBuild(eBuild) : 0;
}


int CvUnit::getCollateralDamageProtection() const
{
	return m_iCollateralDamageProtection;
}

void CvUnit::changeCollateralDamageProtection(int iChange)
{
	if (iChange != 0)
	{
		m_iCollateralDamageProtection += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getPillageChange() const
{
	return m_iPillageChange;
}

void CvUnit::changePillageChange(int iChange)
{
	if (iChange != 0)
	{
		m_iPillageChange += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getUpgradeDiscount() const
{
	return m_iUpgradeDiscount;
}

void CvUnit::changeUpgradeDiscount(int iChange)
{
	if (iChange != 0)
	{
		m_iUpgradeDiscount += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExperiencePercent() const
{

	if ((isCommander())||(isCommodore()))
	{
		return 0; // Afforess - Great Commanders/Commodores can not gain XP faster
	}
	const CvUnit* pCommander = getCommander();

	if (pCommander)
	{
		return m_iExperiencePercent + pCommander->getExperiencePercent();
	}

	const CvUnit* pCommodore = getCommodore();

		if (pCommodore)
    	{
    		return m_iExperiencePercent + pCommodore->getExperiencePercent();
    	}

	return m_iExperiencePercent;
}

void CvUnit::changeExperiencePercent(int iChange)
{
	if (iChange != 0)
	{
		m_iExperiencePercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getKamikazePercent() const
{
	return m_iKamikazePercent;
}

void CvUnit::changeKamikazePercent(int iChange)
{
	if (iChange != 0)
	{
		m_iKamikazePercent += iChange;

		setInfoBarDirty(true);
	}
}

DirectionTypes CvUnit::getFacingDirection(bool checkLineOfSightProperty) const
{
	// ⚑ NOTHING authors a line-of-sight restriction, so a caller asking for the restricted answer always gets
	// the unrestricted one. DllExport: the EXE is the only caller that passes true -- every in-tree call
	// passes false and reads the facing directly.
	if (checkLineOfSightProperty)
	{
		return NO_DIRECTION; //look in all directions
	}
	return m_eFacingDirection; //only look in facing direction
}

void CvUnit::setFacingDirection(DirectionTypes eFacingDirection)
{
	if (eFacingDirection != m_eFacingDirection)
	{
		m_eFacingDirection = eFacingDirection;
		NotifyEntity(NO_MISSION);
	}
}

void CvUnit::rotateFacingDirectionClockwise()
{
	//change direction
	DirectionTypes eNewDirection = (DirectionTypes) ((m_eFacingDirection + 1) % NUM_DIRECTION_TYPES);
	setFacingDirection(eNewDirection);
}

void CvUnit::rotateFacingDirectionCounterClockwise()
{
	//change direction
	DirectionTypes eNewDirection = (DirectionTypes) ((m_eFacingDirection + NUM_DIRECTION_TYPES - 1) % NUM_DIRECTION_TYPES);
	setFacingDirection(eNewDirection);
}

// --- UNIT STATUS (Engine/CvStatus.h) -- an applied counter that ticks down and is over at zero. ---

int CvUnit::getStatus(UnitStatus eStatus) const
{
	FASSERT_BOUNDS(0, NUM_UNIT_STATUSES, eStatus);
	return m_aiStatusTurns[eStatus];
}

// The gate IS the counter: held while there are turns left. There is no separate present/absent plane, so
// expiry needs no second fact -- reaching zero IS the status ending.
bool CvUnit::hasStatus(UnitStatus eStatus) const
{
	return getStatus(eStatus) > 0;
}

// The ONE write path for every unit status, so the HOLDS-crossing announces from exactly one place -- the tick
// below, the load, and every applier alike come through here.
void CvUnit::setStatus(UnitStatus eStatus, int iTurns)
{
	FASSERT_BOUNDS(0, NUM_UNIT_STATUSES, eStatus);
	const bool bWasHeld = m_aiStatusTurns[eStatus] > 0;
	m_aiStatusTurns[eStatus] = std::max(0, iTurns);
	// Only the 0-CROSSING is a fact: a status ticking 5 -> 4 moves nothing a consumer gates on, and the gate IS
	// `count > 0`. The general rule for every timer-backed fact.
	if (bWasHeld == (m_aiStatusTurns[eStatus] > 0))
	{
		return;
	}
	const int iPlotNum = (plot() != NULL) ? GC.getMap().plotNum(getX(), getY()) : -1;
	if (m_aiStatusTurns[eStatus] > 0)
	{
		emitUnitStatusAdded(getID(), (int)getOwner(), (int)eStatus, m_aiStatusTurns[eStatus], iPlotNum);
	}
	else
	{
		emitUnitStatusRemoved(getID(), (int)getOwner(), (int)eStatus, 0, iPlotNum);
	}
}

void CvUnit::changeStatus(UnitStatus eStatus, int iChange)
{
	setStatus(eStatus, getStatus(eStatus) + iChange);
}

// One tick per turn for every held status; a status reaching zero is simply no longer held.
void CvUnit::doStatusTurn()
{
	for (int iStatus = 0; iStatus < NUM_UNIT_STATUSES; ++iStatus)
	{
		if (m_aiStatusTurns[iStatus] > 0)
		{
			// Through setStatus, so the turn a status runs out announces its expiry like any other crossing.
			setStatus((UnitStatus)iStatus, m_aiStatusTurns[iStatus] - 1);
		}
	}
}


bool CvUnit::isCanRespawn() const
{
	return m_bCanRespawn;
}


void CvUnit::setCanRespawn(bool bNewValue)
{
	m_bCanRespawn = bNewValue;
}


bool CvUnit::isSurvivor() const
{
	return m_bSurvivor;
}


void CvUnit::setSurvivor(bool bNewValue)
{
	m_bSurvivor = bNewValue;
}


bool CvUnit::isMadeAttack() const
{
	return m_bMadeAttack;
}


void CvUnit::setMadeAttack(bool bNewValue)
{
	m_bMadeAttack = bNewValue;
}


//TB Combat Mods (Att&DefCounters)
int CvUnit::getRoundCount() const
{
	return m_iRoundCount;
}

void CvUnit::changeRoundCount(int iChange)
{
	if (iChange != 0)
	{
		m_iRoundCount += iChange;
	}
}

int CvUnit::getAttackCount() const
{
	return m_iAttackCount;
}

void CvUnit::changeAttackCount(int iChange)
{
	if (iChange != 0)
	{
		m_iAttackCount += iChange;
	}
}

int CvUnit::getDefenseCount() const
{
	return m_iDefenseCount;
}

void CvUnit::changeDefenseCount(int iChange)
{
	if (iChange != 0)
	{
		m_iDefenseCount += iChange;
	}
}
//TB Combat Mods End

bool CvUnit::isMadeInterception() const
{
	return m_bMadeInterception;
}


void CvUnit::setMadeInterception(bool bNewValue)
{
	m_bMadeInterception = bNewValue;
}


bool CvUnit::isPromotionReady() const
{
	return m_bPromotionReady;
}


void CvUnit::setPromotionReady(bool bNewValue)
{
	if (isPromotionReady() != bNewValue)
	{
		m_bPromotionReady = bNewValue;

/************************************************************************************************/
/* Afforess	                  Start		 09/16/10                                               */
/*                                                                                              */
/* Advanced Automations                                                                         */
/************************************************************************************************/
		if ( !isUsingDummyEntities() && isInViewport())
		{
			gDLL->getEntityIFace()->showPromotionGlow(getUnitEntity(), bNewValue);
		}

		if (m_bPromotionReady)
		{
			if (isAutoPromoting())
			{
				if(AI_promote())
				{
					setPromotionReady(false);
					testPromotionReady();
				}
				else
				{
					setPromotionReady(false);
					FErrorMsg("Couldn't apply promotion");
				}
			}
			else
			{
				MissionAITypes eMissionAI = getGroup()->AI_getMissionAIType();

				//	Don't interrupt units on their way to delivery or rally plots
				if ( (MISSIONAI_CONTRACT != eMissionAI && MISSIONAI_CONTRACT_UNIT != eMissionAI) ||
					 getGroup()->AI_getMissionAIPlot() == plot() )
				{
					getGroup()->setAutomateType(NO_AUTOMATE);
					getGroup()->clearMissionQueue();
					getGroup()->setActivityType(ACTIVITY_AWAKE);
				}
			}
/************************************************************************************************/
/* Afforess	                     END                                                            */
/************************************************************************************************/
		}

		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
		}
	}
}


void CvUnit::testPromotionReady()
{
	//TB Combat Mod
	bool bPromotionReady = false;
	if (getExperience() >= experienceNeeded() && canAcquirePromotionAny())
	{
		bPromotionReady = true;
	}
	if (getRetrainsAvailable() > 0 && canAcquirePromotionAny())
	{
		bPromotionReady = true;
	}

	setPromotionReady(bPromotionReady);
	//TB Combat Mod end
}


bool CvUnit::isDelayedDeath() const
{
	return m_bDeathDelay;
}


// Returns true if killed...
bool CvUnit::doDelayedDeath()
{
	// Koshling - added 'isDead' check to clean up units with 100% damage that have somehow been left behind
	if (isDead() && !isInBattle())
	{
		killUnconditional(false, NO_PLAYER, true);
		return true;
	}
	return false;
}


bool CvUnit::isInfoBarDirty() const
{
	return m_bInfoBarDirty;
}


void CvUnit::setInfoBarDirty(bool bNewValue)
{
	m_bInfoBarDirty = bNewValue;
}

bool CvUnit::isBlockading() const
{
	return m_bBlockading;
}

void CvUnit::setBlockading(bool bNewValue)
{
	if (bNewValue != isBlockading())
	{
		m_bBlockading = bNewValue;

		updatePlunder(isBlockading() ? 1 : -1, true);
	}
}

void CvUnit::collectBlockadeGold()
{
	PROFILE_EXTRA_FUNC();
	if (plot()->getTeam() == getTeam())
	{
		return;
	}

	const int iBlockadeRange = GC.getSHIP_BLOCKADE_RANGE();

	foreach_(const CvPlot* pLoopPlot, plot()->rect(iBlockadeRange, iBlockadeRange)
	| filtered(CvPlot::fn::isRevealed(getTeam(), false)))
	{
		CvCity* pCity = pLoopPlot->getPlotCity();

		if (NULL != pCity && !pCity->isPlundered() && isEnemy(pCity->getTeam()) && !atWar(pCity->getTeam(), getTeam()))
		{
			if (pCity->area() == area() || pCity->plot()->isAdjacentToArea(area()))
			{
				// ×100 through the multiply, reduced once after it
				int iGold = pCity->calculateTradeProfit(pCity) * pCity->getTradeRoutes() / 100;
				if (iGold > 0)
				{
					pCity->setPlundered(true);
					GET_PLAYER(getOwner()).changeGold(iGold);
					GET_PLAYER(pCity->getOwner()).changeGold(-iGold);


					const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_TRADE_ROUTE_PLUNDERED", getNameKey(), pCity->getNameKey(), iGold);
					AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BUILD_BANK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), getX(), getY());

					const CvWString szBuffer2 = gDLL->getText("TXT_KEY_MISC_TRADE_ROUTE_PLUNDER", getNameKey(), pCity->getNameKey(), iGold);
					AddDLLMessage(pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer2, "AS2D_BUILD_BANK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY());
				}
			}
		}
	}
}


PlayerTypes CvUnit::getVisualOwner(TeamTypes eForTeam) const
{
	if (NO_TEAM == eForTeam)
	{
		eForTeam = GC.getGame().getActiveTeam();
	}

	if (getTeam() != eForTeam && eForTeam != BARBARIAN_TEAM && isHiddenNationality() && !plot()->isCity(true, getTeam()))
	{
		return BARBARIAN_PLAYER;
	}
	return getOwner();
}


PlayerTypes CvUnit::getCombatOwner(TeamTypes eForTeam, const CvPlot* pPlot) const
{
	if (eForTeam != NO_TEAM && getTeam() != eForTeam && eForTeam != BARBARIAN_TEAM && isAlwaysHostile(pPlot))
	{
		return BARBARIAN_PLAYER;
	}
	return getOwner();
}

TeamTypes CvUnit::getTeam() const
{
	return GET_PLAYER(getOwner()).getTeam();
}


PlayerTypes CvUnit::getCapturingPlayer() const
{
	return m_eCapturingPlayer;
}


void CvUnit::setCapturingPlayer(PlayerTypes eNewValue)
{
	m_eCapturingPlayer = eNewValue;
}

CvUnit* CvUnit::getCapturingUnit() const
{
	return getUnit(m_eCapturingUnit);
}

void CvUnit::setCapturingUnit(const CvUnit* pCapturingUnit)
{
	m_eCapturingUnit = pCapturingUnit->getIDInfo();
}

const UnitTypes CvUnit::getUnitType() const
{
	return m_eUnitType;
}

const CvUnitInfo& CvUnit::getUnitInfo() const
{
	return *m_pUnitInfo;
}

const UnitTypes CvUnit::getLeaderUnitType() const
{
	return m_eLeaderUnitType;
}

void CvUnit::setLeaderUnitType(UnitTypes leaderUnitType)
{
	if (m_eLeaderUnitType != leaderUnitType)
	{
		m_eLeaderUnitType = leaderUnitType;
		rebuildEntityArt();
	}
}

CvUnit* CvUnit::getCombatUnit() const
{
	return getUnit(m_combatUnit);
}


void CvUnit::setCombatUnit(CvUnit* pCombatUnit, bool bAttacking, bool bQuick, bool bStealthAttack, bool bStealthDefense)
{
	if (pCombatUnit)
	{
		if (bAttacking)
		{
			if (GC.getLogging() && GC.getGame().isDebugMode())
			{
				// Log info about this combat...
				char szOut[1024];
				sprintf( szOut, "*** KOMBAT!\n     ATTACKER: Player %d Unit %d (%S's %S), CombatStrength=%d\n     DEFENDER: Player %d Unit %d (%S's %S), CombatStrength=%d\n",
					getOwner(), getID(), GET_PLAYER(getOwner()).getName(), getName().GetCString(), currCombatStr(NULL, NULL),
					pCombatUnit->getOwner(), pCombatUnit->getID(), GET_PLAYER(pCombatUnit->getOwner()).getName(), pCombatUnit->getName().GetCString(), pCombatUnit->currCombatStr(pCombatUnit->plot(), this));
				gDLL->messageControlLog(szOut);
			}

			if (showSeigeTower(pCombatUnit) && !isUsingDummyEntities()  && isInViewport())
			{
				CvDLLEntity::SetSiegeTower(true);
			}
			if (!bStealthAttack && !bStealthDefense)
			{
				setCombatFirstStrikes((pCombatUnit->immuneToFirstStrikes()) ? 0 : (firstStrikes() + GC.getGame().getSorenRandNum(chanceFirstStrikes() + 1, "First Strike")));
			}
			else if (bStealthAttack)
			{
				setCombatFirstStrikes(stealthStrikesTotal());
			}
		}
		else if (bStealthDefense)
		{
			setCombatFirstStrikes(stealthStrikesTotal());
		}
		else setCombatFirstStrikes((pCombatUnit->immuneToFirstStrikes()) ? 0 : (firstStrikes() + GC.getGame().getSorenRandNum(chanceFirstStrikes() + 1, "First Strike")));

		if (bAttacking
		&& !bQuick
		&& !gDLL->getInterfaceIFace()->isCombatFocus()
		&& !gDLL->getInterfaceIFace()->isFocusedWidget()
		&& (
			getOwner() == GC.getGame().getActivePlayer()
			||
			pCombatUnit->getOwner() == GC.getGame().getActivePlayer()
			&&
			!GC.getGame().isMPOption(MPOPTION_SIMULTANEOUS_TURNS)
			)
		)
		{
			gDLL->getInterfaceIFace()->setCombatFocus(true);
		}
		m_combatUnit = pCombatUnit->getIDInfo();

		if (!bStealthAttack)
		{
			setCombatFirstStrikes((pCombatUnit->immuneToFirstStrikes()) ? 0 : (firstStrikes() + GC.getGame().getSorenRandNum(chanceFirstStrikes() + 1, "First Strike")));
		}
		else setCombatFirstStrikes(stealthStrikesTotal());
	}
	else if (getCombatUnit())
	{
		m_combatUnit.reset();
		setCombatFirstStrikes(0);

		if (IsSelected())
		{
			gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
		}

		if (plot() == gDLL->getInterfaceIFace()->getSelectionPlot())
		{
			gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		}

		if (!isUsingDummyEntities() && isInViewport())
		{
			CvDLLEntity::SetSiegeTower(false);
		}
	}
	setInfoBarDirty(true);
}

// K-Mod. Return true if the combat animation should include a seige tower
bool CvUnit::showSeigeTower(const CvUnit* pDefender) const
{
	return getDomainType() == DOMAIN_LAND
		&& !getUnitInfo().hasSkill(CLS_SKILL_IGNORE_BUILDING_DEFENSE)
		&& pDefender->plot()->getPlotCity()
		&& pDefender->plot()->getPlotCity()->getBuildingDefense() > 0
		&& cityAttackModifier() >= GC.getDefineINT("MIN_CITY_ATTACK_MODIFIER_FOR_SIEGE_TOWER");
}


CvUnit* CvUnit::getTransportUnit() const
{
	return getUnit(m_transportUnit);
}


bool CvUnit::isCargo() const
{
	return getTransportUnit() != NULL;
}


void CvUnit::setTransportUnit(CvUnit* pTransportUnit)
{
	CvUnit* pOldTransportUnit = getTransportUnit();

	if (pOldTransportUnit != pTransportUnit)
	{
		if (pOldTransportUnit != NULL)
		{
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				pOldTransportUnit->SMchangeCargo(-SMCargoVolume());
				pOldTransportUnit->changeCargo(-1);
			}
			else
			{
				pOldTransportUnit->changeCargo(-1);
			}
		}

		if (pTransportUnit != NULL)
		{
			//Can Happen without it being a bug if the unit was forced reloaded by means of an adjustment when already loaded that allowed the unit to overload the transport.
			FAssertMsg(pTransportUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType()) > 0, "Cargo space is expected to be available");

			joinGroup(NULL, true); // Because what if a group of 3 tries to get in a transport which can hold 2...

			m_transportUnit = pTransportUnit->getIDInfo();
			setInhibitMerge(false);
			setInhibitSplit(false);

			if (getDomainType() != DOMAIN_AIR)
			{
				getGroup()->setActivityType(ACTIVITY_SLEEP);
			}

			if (GC.getGame().isFinalInitialized())
			{
				finishMoves();
			}

			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				pTransportUnit->SMchangeCargo(SMCargoVolume());
				pTransportUnit->changeCargo(1);
			}
			else
			{
				pTransportUnit->changeCargo(1);
			}
			pTransportUnit->getGroup()->setActivityType(ACTIVITY_AWAKE);
		}
		else
		{
			m_transportUnit.reset();

			getGroup()->setActivityType(ACTIVITY_AWAKE);

			// After a Barb Transport is done, set it to attack AI
			if (pOldTransportUnit != NULL && !pOldTransportUnit->hasCargo())
			{
				if (pOldTransportUnit->getDomainType() == DOMAIN_SEA && pOldTransportUnit->isHominid())
				{
					pOldTransportUnit->AI_setUnitAIType(UNITAI_ATTACK_SEA);
				}
			}

			// Koshling - have the AI prioritize regrouping with other units when unloaded
			getGroup()->AI_setMissionAI(MISSIONAI_REGROUP, NULL, NULL);
		}

#ifdef _DEBUG
		std::vector<CvUnit*> aCargoUnits;
		if (pOldTransportUnit != NULL)
		{
			pOldTransportUnit->getCargoUnits(aCargoUnits);
			if (aCargoUnits.size() > 0)
			{
				pOldTransportUnit->validateCargoUnits();
			}
		}
		if (pTransportUnit != NULL)
		{
			pTransportUnit->getCargoUnits(aCargoUnits);
			if (aCargoUnits.size() > 0)
			{
				pTransportUnit->validateCargoUnits();
			}
		}

		getGroup()->validateLocations();
#endif

	}
}


int CvUnit::getExtraDomainModifier(DomainTypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, eIndex);

	if (!isCommander())
	{
		const CvUnit* pCommander = getCommander();
		if (pCommander)
		{
			return m_aiExtraDomainModifier[eIndex] + pCommander->m_aiExtraDomainModifier[eIndex];
		}
	}
	if (!isCommodore())
    	{
    		const CvUnit* pCommodore = getCommodore();
    		if (pCommodore)
    		{
    			return m_aiExtraDomainModifier[eIndex] + pCommodore->m_aiExtraDomainModifier[eIndex];
    		}
    	}
	return m_aiExtraDomainModifier[eIndex];
}


void CvUnit::changeExtraDomainModifier(DomainTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, eIndex);
	m_aiExtraDomainModifier[eIndex] += iChange;
}


const CvWString CvUnit::getDescription(uint uiForm) const
{
	return m_pUnitInfo->getDescription(uiForm);
}

const CvWString CvUnit::getName(uint uiForm) const
{
	CvWString szBuffer;

	if (m_szName.empty())
	{
		return getDescription(uiForm);
	}

	if (isDescInName())
	{
		return m_szName;
	}

	szBuffer.Format(L"%s (%s)", m_szName.GetCString(), getDescription(uiForm).GetCString());

	return szBuffer;
}

bool CvUnit::isDescInName() const
{
	return (m_szName.find(getDescription()) != -1);
}


const wchar_t* CvUnit::getNameKey() const
{
	if (m_szName.empty())
	{
		return m_pUnitInfo->getTextKeyWide();
	}
	return m_szName.GetCString();
}


const CvWString& CvUnit::getNameNoDesc() const
{
	return m_szName;
}


void CvUnit::setName(CvWString szNewValue)
{
	gDLL->stripSpecialCharacters(szNewValue);

	m_szName = szNewValue;
	// #430 event spine: a set-name choke point -- the consumer resolves the NEW name live, so emit AFTER the assign.
	emitNameChange(NAMECHANGE_UNIT, getOwner(), getID());

	if (IsSelected())
	{
		gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	}
}


std::string CvUnit::getScriptData() const
{
	return m_szScriptData;
}


void CvUnit::setScriptData(std::string szNewValue)
{
	m_szScriptData = szNewValue;
}


int CvUnit::getTerrainDoubleMoveCount(TerrainTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	const TerrainKeyedInfo* info = findTerrainKeyedInfo(eIndex);

	return info == NULL ? 0 : info->m_iTerrainDoubleMoveCount;
}


bool CvUnit::isTerrainDoubleMove(TerrainTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);
	return getTerrainDoubleMoveCount(eIndex) > 0;
}


void CvUnit::changeTerrainDoubleMoveCount(TerrainTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	if (iChange != 0)
	{
		TerrainKeyedInfo* info = findOrCreateTerrainKeyedInfo(eIndex);

		info->m_iTerrainDoubleMoveCount += iChange;
		FASSERT_NOT_NEGATIVE(info->m_iTerrainDoubleMoveCount);
	}
}


int CvUnit::getFeatureDoubleMoveCount(FeatureTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	const FeatureKeyedInfo* info = findFeatureKeyedInfo(eIndex);

	return info == NULL ? 0 : info->m_iFeatureDoubleMoveCount;
}


bool CvUnit::isFeatureDoubleMove(FeatureTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);
	return (getFeatureDoubleMoveCount(eIndex) > 0);
}


void CvUnit::changeFeatureDoubleMoveCount(FeatureTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	if (iChange != 0)
	{
		FeatureKeyedInfo* info = findOrCreateFeatureKeyedInfo(eIndex);

		info->m_iFeatureDoubleMoveCount += iChange;
		FASSERT_NOT_NEGATIVE(info->m_iFeatureDoubleMoveCount);
	}
}

int CvUnit::getExtraTerrainWorkPercent(TerrainTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	const TerrainKeyedInfo* info = findTerrainKeyedInfo(eIndex);

	return info == NULL ? 0 : info->m_iExtraTerrainWorkPercent;
}

void CvUnit::changeExtraTerrainWorkPercent(TerrainTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	if (iChange != 0)
	{
		TerrainKeyedInfo* info = findOrCreateTerrainKeyedInfo(eIndex);

		info->m_iExtraTerrainWorkPercent += iChange;

		setInfoBarDirty(true);
	}
}


int CvUnit::getExtraFeatureWorkPercent(FeatureTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	const FeatureKeyedInfo* info = findFeatureKeyedInfo(eIndex);

	return info == NULL ? 0 : info->m_iExtraFeatureWorkPercent;
}

void CvUnit::changeExtraFeatureWorkPercent(FeatureTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	if (iChange != 0)
	{
		FeatureKeyedInfo* info = findOrCreateFeatureKeyedInfo(eIndex);

		info->m_iExtraFeatureWorkPercent += iChange;

		setInfoBarDirty(true);
	}
}

//get totals
int CvUnit::terrainWorkPercent(TerrainTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);
	return InfoValuation::keyedTarget(m_pUnitInfo->getModifiers(), MODFAM_WORK_RATE, -1,
		InfoValuation::keyedTargetSegment("terrain"), eIndex) + getExtraTerrainWorkPercent(eIndex);
}

int CvUnit::featureWorkPercent(FeatureTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);
	return InfoValuation::keyedTarget(m_pUnitInfo->getModifiers(), MODFAM_WORK_RATE, -1,
		InfoValuation::keyedTargetSegment("feature"), eIndex) + getExtraFeatureWorkPercent(eIndex);
}

int CvUnit::buildWorkPercent(BuildTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildInfos(), eIndex);
	return InfoValuation::keyedTarget(m_pUnitInfo->getModifiers(), MODFAM_WORK_RATE, -1,
		InfoValuation::keyedTargetSegment("builds"), eIndex) + getExtraWorkModForBuild(eIndex);
}


int CvUnit::getExtraTerrainAttackPercent(TerrainTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	const TerrainKeyedInfo* info = findTerrainKeyedInfo(eIndex);

	return info ? info->m_iExtraTerrainAttackPercent : 0;
}


void CvUnit::changeExtraTerrainAttackPercent(TerrainTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	if (iChange != 0)
	{
		findOrCreateTerrainKeyedInfo(eIndex)->m_iExtraTerrainAttackPercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraTerrainDefensePercent(TerrainTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	const TerrainKeyedInfo* info = findTerrainKeyedInfo(eIndex);

	return info ? info->m_iExtraTerrainDefensePercent : 0;
}


void CvUnit::changeExtraTerrainDefensePercent(TerrainTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumTerrainInfos(), eIndex);

	if (iChange != 0)
	{
		findOrCreateTerrainKeyedInfo(eIndex)->m_iExtraTerrainDefensePercent += iChange;

		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraFeatureAttackPercent(FeatureTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	const FeatureKeyedInfo* info = findFeatureKeyedInfo(eIndex);

	return info ? info->m_iExtraFeatureAttackPercent : 0;
}


void CvUnit::changeExtraFeatureAttackPercent(FeatureTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	if (iChange != 0)
	{
		findOrCreateFeatureKeyedInfo(eIndex)->m_iExtraFeatureAttackPercent += iChange;
		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraFeatureDefensePercent(FeatureTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	const FeatureKeyedInfo* info = findFeatureKeyedInfo(eIndex);

	return info ? info->m_iExtraFeatureDefensePercent : 0;
}


void CvUnit::changeExtraFeatureDefensePercent(FeatureTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumFeatureInfos(), eIndex);

	if (iChange != 0)
	{
		findOrCreateFeatureKeyedInfo(eIndex)->m_iExtraFeatureDefensePercent += iChange;
		setInfoBarDirty(true);
	}
}

int CvUnit::getExtraUnitCombatModifier(UnitCombatTypes eIndex, const bool bCommander, const bool bCommodore) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);

	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eIndex);

	if (!bCommander)
	{
		const CvUnit* pCommander = getCommander();
		if (pCommander)
		{
			return (info ? info->m_iExtraUnitCombatModifier : 0) + pCommander->getExtraUnitCombatModifier(eIndex);
		}
	}
	if (!bCommodore)
    	{
    		const CvUnit* pCommodore = getCommodore();
    		if (pCommodore)
    		{
    			return (info ? info->m_iExtraUnitCombatModifier : 0) + pCommodore->getExtraUnitCombatModifier(eIndex);
    		}
    	}
	return info ? info->m_iExtraUnitCombatModifier : 0;
}


void CvUnit::changeExtraUnitCombatModifier(UnitCombatTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);

	if (iChange != 0)
	{
		UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo(eIndex);

		info->m_iExtraUnitCombatModifier += iChange;
	}
}

bool CvUnit::canAcquirePromotion(PromotionTypes ePromotion, PromotionRequirements::flags requirements) const
{
	return canAcquirePromotion(ePromotion,
		requirements & PromotionRequirements::IgnoreHas,
		requirements & PromotionRequirements::Equip,
		requirements & PromotionRequirements::ForLeader,
		requirements & PromotionRequirements::ForOffset,
		requirements & PromotionRequirements::ForFree,
		requirements & PromotionRequirements::ForBuildUp,
		requirements & PromotionRequirements::ForStatus
	);
}


//	IS THIS PROMOTION EVEN IN THE UNIT'S TREE? -- a BARE FETCH of the maintained enabler state, asked BEFORE any
//	evaluation happens ([enabler.md] par.8: "every read is a BARE O(1) FETCH of the maintained tri-state -- no gate
//	runs, no calculator is called").
//
//	⚖ THE CANDIDATE SET HAS TWO SOURCES, AND TAKING EITHER ALONE IS WRONG:
//	  1. the PLAYER's unlocked-promotions domain -- what TECHS have unlocked;
//	  2. the LADDER SUCCESSORS of promotions THIS UNIT HOLDS -- per-unit state the player domain cannot carry.
//	⛔ Narrowing to (1) alone fails SILENTLY in the direction that looks like a filter bug: every ladder rung
//	reachable only from another promotion drops out, so the next rung stops being offered.
//	⚑ (2) is a FORWARD EDGE FETCH off each held promotion's own compiled `enables` -- O(held x fanout), never
//	O(registry).
bool CvUnit::enablerOffersPromotion(PromotionTypes ePromotion) const
{
	const CvPlayer& kOwner = GET_PLAYER(getOwner());
	if (kOwner.getPromotionUnlocked(ePromotion) >= EnablerDomain::STATE_GREYED)
	{
		return true;
	}
	const std::map<PromotionTypes, PromotionKeyedInfo>& kHeld = getPromotionKeyedInfo();
	for (std::map<PromotionTypes, PromotionKeyedInfo>::const_iterator it = kHeld.begin(); it != kHeld.end(); ++it)
	{
		if (!it->second.m_bHasPromotion)
		{
			continue;
		}
		const std::vector<int>* pSuccessors = GC.getPromotionInfo(it->first).edge(EDGEF_ENABLES, EDGEB_PROMOTIONS);
		if (pSuccessors == NULL)
		{
			continue;
		}
		for (size_t iEdge = 0; iEdge < pSuccessors->size(); ++iEdge)
		{
			if ((*pSuccessors)[iEdge] == (int)ePromotion)
			{
				return true;
			}
		}
	}
	return false;
}

bool CvUnit::canAcquirePromotion(PromotionTypes ePromotion, bool bIgnoreHas, bool bEquip, bool bForLeader, bool bForOffset, bool bForFree, bool bForBuildUp, bool bForStatus) const
{
	PROFILE_FUNC();

	FASSERT_BOUNDS(NO_PROMOTION, GC.getNumPromotionInfos(), ePromotion);

	if (ePromotion == NO_PROMOTION || !bIgnoreHas && isHasPromotion(ePromotion))
	{
		return false;
	}

	const CvPromotionInfo& promo = GC.getPromotionInfo(ePromotion);

	if (!bForStatus && promo.isStatus())
	{
		return false;
	}

	if (promo.getStateReligionPrereq() != NO_RELIGION && GET_PLAYER(getOwner()).getStateReligion() != promo.getStateReligionPrereq())
	{
		return false;
	}

	if (!isPromotionValid(ePromotion, bForFree))
	{
		return false;
	}



	//TB Debug Note: If the promotion being evaluated for is the sort you get from a leader as it attaches to the unit that then qualifies you for other
	//promotions, and the check being called here is not for that specific purpose, then return false for that promotion.
	if (!bForLeader && promo.isLeader())
	{
		return false;
	}

	if (!bForOffset && promo.isForOffset())
	{
		return false;
	}

	if (promo.getObsoleteTech() != NO_TECH && GET_TEAM(getTeam()).isHasTech(promo.getObsoleteTech()))
	{
		return false;
	}

	//Units without a primary unitcombat are unable to be assigned promos
	if (getUnitCombatType() == NO_UNITCOMBAT)
	{
		return false;
	}

	if (promo.getReplacesUnitCombat() != NO_UNITCOMBAT && !isHasUnitCombat((UnitCombatTypes)promo.getReplacesUnitCombat()))
	{
		return false;
	}

	if (getLevel() < promo.getLevelPrereq() && !bForOffset)
	{
		return false;
	}
	//TB Combat Mods Begin
	//	THE PROMOTION'S `requires.build`, through the ONE evaluator ([DEC-single-implementation]). This single
	//	gate replaces the whole hand-rolled prereq battery -- the AND prereq, the OR pair, and the plot-substrate
	//	prereqs below -- because the curator authors every one of them into `requires`: 508 promotions carry a
	//	requires.build, which is the LADDER (each rung naming the rung beneath it, json.md §9) plus two terrain
	//	clauses. The evaluator resolves a PROMOTION_ atom against the unit and a TERRAIN_/FEATURE_ atom against
	//	its plot, so nothing is lost by asking once rather than field by field.
	//	⚑ Promotions are the enabler's on-demand carve-out ([enabler.md §7.1]): there is no maintained per-unit
	//	set, so the gate is evaluated HERE, at the decision point, exactly as specced.
	if (!bForFree || bForBuildUp)
	{
		//	THE ENABLER ANSWERS FIRST -- a bare fetch of the maintained tree, before any evaluation runs.
		//	⚠ This is only correct on a ROOTED tree: TECH_GAME_START carries the start-available promotions,
		//	and a save predating that concept holds it nowhere until CvGame's load backfill grants it. Without
		//	that backfill this prune is right and the tree is empty, so every promotion reads as unavailable.
		if (!enablerOffersPromotion(ePromotion))
		{
			return false;
		}
		CvCascadeEvalCtx promoCtx;
		promoCtx.unit = this;
		promoCtx.empireContext = &GET_PLAYER(getOwner()).getEmpireContext();
		const CvPlot* pUnitPlot = plot();
		if (pUnitPlot != NULL)
		{
			promoCtx.plotContext = &pUnitPlot->getPlotContext();
			const CvCity* pPlotCity = pUnitPlot->getPlotCity();
			if (pPlotCity != NULL)
			{
				promoCtx.cityContext = &pPlotCity->getCityContext();
			}
		}
		static const CvCascadeEvalFlags kPromoFlags;
		if (!cascadeEvalCondition(promo.getRequires() != NULL ? promo.getRequires()->build : NULL,
			promoCtx, kPromoFlags))
		{
			return false;
		}
	}

	{
		// Only the MIN era band survives. No promotion authors a max (34 author identity.minEra, none a maxEra)
		// and the rebuilt info carries no member for one to be read from, so the upper leg is dead data rather
		// than a missing read. The band still gates on the unit's era-bearing combat class.
		const int iMinEraInt = promo.getMinEra();
		if (iMinEraInt > NO_ERA)
		{
			for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
			{
				if (isHasUnitCombat((UnitCombatTypes)iI) && GC.getUnitCombatInfo((UnitCombatTypes)iI).getEra() != NO_ERA)
				{
					if (iMinEraInt > (int)GC.getUnitCombatInfo((UnitCombatTypes)iI).getEra())
					{
						return false;
					}
					break;
				}
			}
		}
	}

	for (int iI = 0; iI < (int)promo.providesUnitCombats().size(); iI++)
	{
		//If we have the unitcombat the promotion will give us already
		if (isHasUnitCombat((UnitCombatTypes)promo.providesUnitCombats()[iI]))
		{
			return false;
		}
	}
	const PromotionLineTypes ePromotionLine = promo.getPromotionLine();

	if (bForBuildUp && (ePromotionLine == NO_PROMOTIONLINE || !GC.getPromotionLineInfo(ePromotionLine).isBuildUp())
	|| !bForBuildUp && ePromotionLine != NO_PROMOTIONLINE && GC.getPromotionLineInfo(ePromotionLine).isBuildUp())
	{
		return false;
	}

	if (
		promo.isNotOnDomain((int)getDomainType())
	||
		ePromotionLine != NO_PROMOTIONLINE
	&&	GC.getPromotionLineInfo(ePromotionLine).isNotOnDomain((int)getDomainType())
	) return false;
	//TB SubCombat Mod End

	// Must have the next less promotionline priority unless this is an affliction, equipment, or BuildUp or Status.
	if (ePromotionLine != NO_PROMOTIONLINE && !bEquip && !bForBuildUp && !bForStatus && promo.getLinePriority() > 1)
	{
		const CvPromotionLineInfo& promoLine = GC.getPromotionLineInfo(ePromotionLine);
		const int numPromotions = promoLine.getNumPromotions();
		for (int iJ = 0; iJ < numPromotions; iJ++)
		{
			const PromotionTypes ePrereq = (PromotionTypes)promoLine.getPromotion(iJ);
			if (GC.getPromotionInfo(ePrereq).getLinePriority() == promo.getLinePriority() - 1 && !isHasPromotion(ePrereq))
			{
				return false;
			}
		}
	}
	// For Statuses, you can only have one promotion in the line but iLinePriority is not necessarily a hierarchy, just an index in the line.
	//	However, you can set multiple promos with the same iLinePriority that cannot be swapped out for each other.
	if (bForStatus)
	{
		bool bPrereqFound = ePromotionLine == NO_PROMOTIONLINE || promo.getLinePriority() != 1;

		if (!bPrereqFound)
		{
			const int numPromotionInfos = GC.getNumPromotionInfos();
			for (int iI = 0; iI < numPromotionInfos; iI++)
			{
				const PromotionTypes ePrereq = (PromotionTypes)iI;
				if (isHasPromotion(ePrereq))
				{
					const CvPromotionInfo& kPrereqPromotion = GC.getPromotionInfo(ePrereq);
					if (kPrereqPromotion.getPromotionLine() == ePromotionLine)
					{
						if (kPrereqPromotion.getLinePriority() == promo.getLinePriority())
						{
							return false;
						}
						if (promo.getLinePriority() == 1)
						{
							// This establishes all Status Promos with an iLinePriority of 1 as being the status that erases any of the statuses.
							bPrereqFound = true;
							break;
						}
					}
				}
			}
		}
		if (!bPrereqFound)
		{
			return false;
		}
	}
	//TB Combat Mod end

	if	(promo.isCargoPrereq() && cargoSpace() < 1)
	{
		return false;
	}

	//	⛔ The special-cargo PREREQ gates are DEAD: no promotion authors one. ⚠ Do not confuse them with
	//	`specialCargoChange`, which 5 promotions DO author -- that is the cargo RESTRICTION a promotion grants
	//	(what the carrier may take), not a gate on acquiring it. The two read alike and dispose oppositely.

	if (!bForFree)
	{
		if (promo.getTechPrereq() != NO_TECH && !GET_TEAM(getTeam()).isHasTech(promo.getTechPrereq()))
		{
			return false;
		}
		if (ePromotionLine != NO_PROMOTIONLINE
		&&
			GC.getPromotionLineInfo(ePromotionLine).getPrereqTech() != NO_TECH
		&&
			!GET_TEAM(getTeam()).isHasTech(GC.getPromotionLineInfo(ePromotionLine).getPrereqTech()))
		{
			return false;
		}
	}

	//	⛔ THE PLOT-SUBSTRATE PREREQ BATTERY IS GONE, and it is two different dispositions in one block.
	//	TERRAIN prereqs are now part of the ONE `requires.build` gate above (the curator authors them there;
	//	two promotions carry one). FEATURE / IMPROVEMENT / LOCAL-BUILDING / PLOT-BONUS prereqs are DEAD: not
	//	one promotion in the curated data authors any of them, so every loop here ran zero times and the
	//	`bValid` scaffold around them decided nothing.
	//	⚑ Should a modder ever author one, it lands in `requires` like the terrain clauses and is evaluated
	//	by the same gate -- which is why nothing needs re-adding here for it to work.
	if (promo.isPrereqNormInvisible() && !hasInvisibleAbility())
	{
		return false;
	}

	if (!bForOffset && promo.getSizeMatters().quality > 0 && getRetrainsAvailable() > 0)
	{
		return false;
	}

	return true;
}

bool CvUnit::isPromotionValid(PromotionTypes ePromotion, bool bFree, bool bKeepCheck) const
{
	PROFILE_EXTRA_FUNC();
	const CvPromotionInfo& promo = GC.getPromotionInfo(ePromotion);

	if (!bKeepCheck) // If the unit got the promo then these checks have already passed.
	{
		if (m_pUnitInfo->hasTag(CLS_TAG_SPY) && !GC.isSS_ENABLED())
		{
			return false;
		}

		// The whole-entity game-option gate -- the promotion's own enabled/disabled pair, and its line's.
		// The legacy On/NotOnGameOption lists author here now and are served WHOLE by getGate()
		// ([DEC-entity-gate]); evaluating them through the ONE evaluator is what keeps the option test in a
		// single place rather than hand-rolled per site.
		CvCascadeEvalCtx ecGate;
		GET_PLAYER(getOwner()).getEmpireContext().fillEvalCtx(ecGate);
		CvCascadeEvalFlags gateFlags;
		if (!cascadeGateOk(promo.getGate(), ecGate, gateFlags))
		{
			return false;
		}
		if (promo.getPromotionLine() != NO_PROMOTIONLINE
		&& !cascadeGateOk(GC.getPromotionLineInfo(promo.getPromotionLine()).getGate(), ecGate, gateFlags))
		{
			return false;
		}
	}
	// Very few reasons to deny a unit promotions that are specifically set to be a free for it.
	if (m_pUnitInfo->grantsPromotion(ePromotion) || GET_PLAYER(getOwner()).isFreePromotion(getUnitType(), ePromotion))
	{
		return true;
	}

	if (m_pUnitInfo->getCombatClass() == NO_UNITCOMBAT)
	{
		return false;
	}
	if (promo.getObsoleteTech() != NO_TECH && GET_TEAM(getTeam()).isHasTech(promo.getObsoleteTech()))
	{
		return false;
	}
	if (!bFree)
	{
		if (promo.getTechPrereq() != NO_TECH && !GET_TEAM(getTeam()).isHasTech(promo.getTechPrereq()))
		{
			return false;
		}
		const PromotionLineTypes ePromotionLine = promo.getPromotionLine();

		if (ePromotionLine != NO_PROMOTIONLINE
		&& GC.getPromotionLineInfo(ePromotionLine).getPrereqTech() != NO_TECH
		&& !GET_TEAM(getTeam()).isHasTech(GC.getPromotionLineInfo(ePromotionLine).getPrereqTech()))
		{
			return false;
		}
	}

	// Toffer - Promotionline is factored in for the (dis)qualified caches.
	for (int iI = 0; iI < (int)promo.getDisqualifiedUnitCombats().size(); iI++)
	{
		if (isHasUnitCombat((UnitCombatTypes)promo.getDisqualifiedUnitCombats()[iI]))
		{
			return false;
		}
	}
	// TB SubCombat Mod Begin
	// The two solid ways to identify a Size Matters promotion that would not normally have a CC prereq.
	// Note: Apparently having no CC prereq is a clear way to isolate promotions to only being assigned directly by event or other special injection.
	// Thus it was necessary to pass the Size Matters promos despite having no particular CC prereq.
	if (!promo.isForOffset() && !promo.isZeroesXP())
	{
		bool bValid = bFree;

		for (int iI = (int)promo.getQualifiedUnitCombats().size() - 1; iI > -1; iI--)
		{
			bValid = false;
			if (isHasUnitCombat((UnitCombatTypes)promo.getQualifiedUnitCombats()[iI]))
			{
				bValid = true;
				break;
			}
		}
		if (!bValid)
		{
			return false;
		}
	}

	if (isSpy())
	{
		return true;
	}

	//Disable Looter Promos for units that cannot pillage
	if (promo.getScalar(SCALAR_PILLAGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) > 0 && !getUnitInfo().hasSkill(CLS_SKILL_PILLAGE))
	{
		return false;
	}

	if (isCommander() && (promo.getSizeMatters().group != 0 || promo.getSizeMatters().quality != 0))
	{
		return false;
	}

	if (isCommodore() && (promo.getSizeMatters().group != 0 || promo.getSizeMatters().quality != 0))
    	{
    		return false;
    	}

	if (isBlendIntoCity() && promo.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT) != 0)
	{
		return false;
	}

	if (!bKeepCheck)
	{
		if (promo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) + maxInterceptionProbability() > GC.getDefineINT("MAX_INTERCEPTION_PROBABILITY")
		||	promo.getAir(AIR_EVASION, CASC_SCOPE_UNIT) + evasionProbability() > GC.getDefineINT("MAX_EVASION_PROBABILITY")
		||	promo.getSizeMatters().quality > 0 && getExperience() >= experienceNeeded(1))
		{
			return false;
		}
	}

	return true;
}


//	Does this unit still have SOMETHING it can pick when taking a skill-based promotion? Asked so a level-up
//	does not offer a choice with nothing behind it.
//
//	⛔ IT ASKS THE ENABLER FIRST, THEN GATES -- the two-stage decision protocol ([patterns.md] THE DECISION
//	PROTOCOL: "first it should ask enabler what is possible, and then it asks cascade"). Sweeping the promotion
//	registry and running the full per-unit gate per id is the whole-database scan the maintained frontier exists
//	to delete ([enabler.md] §6: the AI's decisions iterate ONLY the frontier).
//
//	⚖ THE CANDIDATE SET HAS TWO SOURCES, AND TAKING EITHER ALONE IS WRONG:
//	  1. the PLAYER's unlocked-promotions domain -- what TECHS have unlocked (the §7.1 carve-out: one player-scope
//	     set, no per-unit maintained sets);
//	  2. the LADDER SUCCESSORS of the promotions THIS UNIT HOLDS -- a rung is unlocked by holding the rung
//	     beneath it, which is per-UNIT state the player-scope domain structurally cannot carry.
//	⛔ Narrowing to (1) alone is the trap, and it fails SILENTLY in the direction that looks like a filter bug:
//	every ladder rung reachable only from another promotion drops out, so the next rung stops being offered. That
//	is not a domain to "fix" -- the ladder is per-unit by construction, so the union is the answer.
//	⚑ (2) is a FORWARD EDGE FETCH off each held promotion's own compiled `enables` ([patterns.md] THE WHAT-IF
//	DRIVER: the entity's own edge list IS the answer; asking it backwards is the database scan). It costs
//	O(held x fanout), never O(registry).
bool CvUnit::canAcquirePromotionAny() const
{
	PROFILE_EXTRA_FUNC();

	std::vector<int> aCandidates;
	GET_PLAYER(getOwner()).getUnlockedPromotions(aCandidates);

	const std::map<PromotionTypes, PromotionKeyedInfo>& kHeldPromotions = getPromotionKeyedInfo();
	for (std::map<PromotionTypes, PromotionKeyedInfo>::const_iterator itPromotion = kHeldPromotions.begin();
		itPromotion != kHeldPromotions.end(); ++itPromotion)
	{
		if (!itPromotion->second.m_bHasPromotion)
		{
			continue;
		}
		const std::vector<int>* pSuccessors =
			GC.getPromotionInfo(itPromotion->first).edge(EDGEF_ENABLES, EDGEB_PROMOTIONS);
		if (pSuccessors != NULL)
		{
			aCandidates.insert(aCandidates.end(), pSuccessors->begin(), pSuccessors->end());
		}
	}

	// The per-unit gate decides each survivor -- level, unit-combat, the `requires.build` ladder atom, the line
	// rules. A duplicate between the two sources costs one extra gate call and cannot change the verdict.
	for (size_t iCandidate = 0; iCandidate < aCandidates.size(); ++iCandidate)
	{
		const PromotionTypes ePromotion = static_cast<PromotionTypes>(aCandidates[iCandidate]);

		PromotionRequirements::flags promoFlags = PromotionRequirements::Promote;

		if (GC.getPromotionInfo(ePromotion).isLeader())
		{
			promoFlags |= PromotionRequirements::ForLeader;
		}
		if (canAcquirePromotion(ePromotion, promoFlags))
		{
			return true;
		}
	}
	return false;
}

PromotionKeyedInfo*	CvUnit::findOrCreatePromotionKeyedInfo(PromotionTypes ePromotion, bool bCreate)
{
	PROFILE_FUNC();

	std::map<PromotionTypes, PromotionKeyedInfo>::iterator itr = m_promotionKeyedInfo.find(ePromotion);

	if (itr != m_promotionKeyedInfo.end())
	{
		return &(itr->second);
	}

	if (bCreate)
	{
		PromotionKeyedInfo newInfo;

		return &(m_promotionKeyedInfo.insert(std::make_pair(ePromotion, newInfo)).first->second);
	}

	return NULL;
}

const PromotionKeyedInfo* CvUnit::findPromotionKeyedInfo(PromotionTypes ePromotion) const
{
	std::map<PromotionTypes, PromotionKeyedInfo>::const_iterator itr = m_promotionKeyedInfo.find(ePromotion);

	if (itr == m_promotionKeyedInfo.end())
	{
		return NULL;
	}

	if (m_promotionKeyedInfo.size() > 32 && itr->second.Empty())
	{
		m_promotionKeyedInfo.erase(itr->first); // Alberts2 - Erase empty elemnts to save memory
		return NULL;
	}

	return &(itr->second);
}

const std::map<PromotionTypes, PromotionKeyedInfo>& CvUnit::getPromotionKeyedInfo() const
{
	return m_promotionKeyedInfo;
}

PromotionIterator CvUnit::getPromotionBegin()
{
	return m_promotionKeyedInfo.begin();
}

PromotionIterator CvUnit::getPromotionEnd()
{
	return m_promotionKeyedInfo.end();
}

PromotionLineKeyedInfo* CvUnit::findOrCreatePromotionLineKeyedInfo(PromotionLineTypes ePromotionLine, bool bCreate)
{
	std::map<PromotionLineTypes, PromotionLineKeyedInfo>::iterator itr = m_promotionLineKeyedInfo.find(ePromotionLine);

	if (itr != m_promotionLineKeyedInfo.end())
	{
		return &(itr->second);
	}

	if (bCreate)
	{
		PromotionLineKeyedInfo newInfo;

		return &(m_promotionLineKeyedInfo.insert(std::make_pair(ePromotionLine, newInfo)).first->second);
	}

	return NULL;
}

const PromotionLineKeyedInfo* CvUnit::findPromotionLineKeyedInfo(PromotionLineTypes ePromotionLine) const
{
	std::map<PromotionLineTypes, PromotionLineKeyedInfo>::const_iterator itr = m_promotionLineKeyedInfo.find(ePromotionLine);

	if (itr == m_promotionLineKeyedInfo.end())
	{
		return NULL;
	}

	if (m_promotionLineKeyedInfo.size() > 16 && itr->second.Empty())
	{
		m_promotionLineKeyedInfo.erase(itr->first); // Alberts2 - Erase empty elemnts to save memory
		return NULL;
	}

	return &(itr->second);
}

std::map<PromotionLineTypes, PromotionLineKeyedInfo>& CvUnit::getPromotionLineKeyedInfo() const
{
	return m_promotionLineKeyedInfo;
}

TerrainKeyedInfo* CvUnit::findOrCreateTerrainKeyedInfo(TerrainTypes eTerrain, bool bCreate)
{
	std::map<TerrainTypes, TerrainKeyedInfo>::iterator itr = m_terrainKeyedInfo.find(eTerrain);

	if (itr != m_terrainKeyedInfo.end())
	{
		return &(itr->second);
	}

	if (bCreate)
	{
		TerrainKeyedInfo newInfo;

		return &(m_terrainKeyedInfo.insert(std::make_pair(eTerrain, newInfo)).first->second);
	}

	return NULL;
}

const TerrainKeyedInfo*	CvUnit::findTerrainKeyedInfo(TerrainTypes eTerrain) const
{
	std::map<TerrainTypes, TerrainKeyedInfo>::const_iterator itr = m_terrainKeyedInfo.find(eTerrain);

	if (itr == m_terrainKeyedInfo.end())
	{
		return NULL;
	}
	if (m_terrainKeyedInfo.size() > 16 && itr->second.Empty())
	{
		m_terrainKeyedInfo.erase(itr->first); // Alberts2 - Erase empty elemnts to save memory
		return NULL;
	}
	return &(itr->second);
}

FeatureKeyedInfo* CvUnit::findOrCreateFeatureKeyedInfo(FeatureTypes eFeature, bool bCreate)
{
	std::map<FeatureTypes, FeatureKeyedInfo>::iterator itr = m_featureKeyedInfo.find(eFeature);

	if (itr != m_featureKeyedInfo.end())
	{
		return &(itr->second);
	}

	if (bCreate)
	{
		FeatureKeyedInfo newInfo;

		return &(m_featureKeyedInfo.insert(std::make_pair(eFeature, newInfo)).first->second);
	}

	return NULL;
}

const FeatureKeyedInfo* CvUnit::findFeatureKeyedInfo(FeatureTypes eFeature) const
{
	std::map<FeatureTypes, FeatureKeyedInfo>::const_iterator itr = m_featureKeyedInfo.find(eFeature);

	if (itr == m_featureKeyedInfo.end())
	{
		return NULL;
	}
	if (m_featureKeyedInfo.size() > 16 && itr->second.Empty())
	{
		m_featureKeyedInfo.erase(itr->first); // Alberts2 - Erase empty elemnts to save memory
		return NULL;
	}
	return &(itr->second);
}

UnitCombatKeyedInfo* CvUnit::findOrCreateUnitCombatKeyedInfo(UnitCombatTypes eUnitCombat, bool bCreate)
{
	std::map<UnitCombatTypes, UnitCombatKeyedInfo>::iterator itr = m_unitCombatKeyedInfo.find(eUnitCombat);

	if (itr != m_unitCombatKeyedInfo.end())
	{
		return &(itr->second);
	}

	if (bCreate)
	{
		UnitCombatKeyedInfo	newInfo;

		return &(m_unitCombatKeyedInfo.insert(std::make_pair(eUnitCombat, newInfo)).first->second);
	}

	return NULL;
}

const UnitCombatKeyedInfo* CvUnit::findUnitCombatKeyedInfo(UnitCombatTypes eUnitCombat) const
{
	std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator itr = m_unitCombatKeyedInfo.find(eUnitCombat);

	if (itr == m_unitCombatKeyedInfo.end())
	{
		return NULL;
	}
	if (m_unitCombatKeyedInfo.size() > 32 && itr->second.Empty())
	{
		m_unitCombatKeyedInfo.erase(itr->first); // Alberts2 - Erase empty elemnts to save memory
		return NULL;
	}
	return &(itr->second);
}

std::map<UnitCombatTypes, UnitCombatKeyedInfo>& CvUnit::getUnitCombatKeyedInfo() const
{
	return m_unitCombatKeyedInfo;
}

bool CvUnit::isHealsUnitCombat(UnitCombatTypes eIndex) const
{
	FASSERT_BOUNDS(NO_UNITCOMBAT, GC.getNumUnitCombatInfos(), eIndex);
	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eIndex);

	return (info != NULL && (info->m_iHealUnitCombatTypeVolume > 0));
}

bool CvUnit::isHasUnitCombat(UnitCombatTypes eIndex) const
{
	FASSERT_BOUNDS(NO_UNITCOMBAT, GC.getNumUnitCombatInfos(), eIndex);
	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eIndex);

	return (info != NULL && info->m_bHasUnitCombat);
}

// COMMIT the keyed flag, MAINTAIN the movement hash, ANNOUNCE the fact -- and nothing else. processUnitCombat
// below is the STAT APPLIER; the two are deliberately separable because the LOAD wants this half and not that
// one (every stat the applier adds is serialized on the unit in its own right, so running it on a load doubles
// them all). Splitting the announcement out is exactly what lets the read stop writing the flag by hand.
void CvUnit::setHasUnitCombatInternal(UnitCombatTypes eIndex, bool bNewValue)
{
	UnitCombatKeyedInfo* infoKeyed =
	(
		bNewValue
		?
		findOrCreateUnitCombatKeyedInfo(eIndex)
		:
		(UnitCombatKeyedInfo*)findUnitCombatKeyedInfo(eIndex)
	);
	if (infoKeyed == NULL || infoKeyed->m_bHasUnitCombat == bNewValue)
	{
		return;
	}
	infoKeyed->m_bHasUnitCombat = bNewValue;

	const CvUnitCombatInfo& kUnitCombat = GC.getUnitCombatInfo(eIndex);
	if (kUnitCombat.changesMoveThroughPlots())
	{
		m_movementCharacteristicsHash ^= kUnitCombat.getZobristValue();
		m_iMaxMoveCacheTurn = -1;
	}
	if (bNewValue)
	{
		emitUnitCombatAdded(getID(), (int)getOwner(), (int)eIndex);
	}
	else
	{
		emitUnitCombatRemoved(getID(), (int)getOwner(), (int)eIndex);
	}
}

void CvUnit::processUnitCombat(UnitCombatTypes eIndex, bool bAdding, bool bByPromo)
{
	PROFILE_EXTRA_FUNC();
	const CvUnitCombatInfo& kUnitCombat = GC.getUnitCombatInfo(eIndex);
	const int iChange = (bAdding ? 1 : -1);
	bool bSM = GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS);

	if (bSM)
	{
		if (!bByPromo)
		{
			// ONE group-rank derivation (json.md par.9 sizeMatters): the instance's base ranks ARE the info's
			// load-derived sums over its combat classes -- never re-derived from the single class in hand here.
			setQualityBaseTotal(m_pUnitInfo->getBaseQualityRank());
			setSizeBaseTotal(m_pUnitInfo->getBaseSizeRank());
			setGroupBaseTotal(m_pUnitInfo->getBaseGroupRank());
		}
	}

	changeExtraMoves(kUnitCombat.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100 * iChange);//no merge/split diff
	changeExtraMoveDiscount(kUnitCombat.getMovement(MOVEMENT_MOVE_DISCOUNT, CASC_SCOPE_UNIT) / 100 * iChange);//no merge/split diff
	changeCargoSpace(kUnitCombat.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 * iChange);//no merge/split diff (since this mechanism is either a base setter or is for non-SM or non-player on SM.

	// The SM cargo trio is the `sizeMatters` BLOCK (json.md par.9), not a modifier family -- plain authored
	// ints, so no de-scaling here.
	changeSMCargoSpace(kUnitCombat.getSizeMatters().cargoSmSpace * iChange);//merge/split volumetric
	changeCargoVolume(kUnitCombat.getSizeMatters().cargoVolume * iChange);//merge/split volumetric
	changeCargoVolumeModifier(kUnitCombat.getSizeMatters().cargoVolumeModifier * iChange);//merge/split volumetric

	changeExtraBombardRate(kUnitCombat.getFlatBombard(BOMBARD_RATE, CASC_SCOPE_UNIT) / 100 * iChange);//no merge/split (affect this volumetrically on the final value)
	// Assume only worker units can get the relevant unit combats, if not then we'll need a retroactive unitComp late init function.
	if (isWorker())
	{
		bool bChanged = false;
		// The work-rate scalars are PERCENTS, so they are not scaled ([DEC-fixedpoint-x100]).
		const int iHillsWork = kUnitCombat.getScalar(SCALAR_WORK_RATE_HILLS, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
		if (iHillsWork != 0)
		{
			m_worker->changeHillsWorkModifier(iHillsWork * iChange);
			bChanged = true;
		}
		const int iWorkRate = kUnitCombat.getScalar(SCALAR_WORK_RATE, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
		if (iWorkRate != 0)
		{
			m_worker->changeWorkModifier(iWorkRate * iChange);
			bChanged = true;
		}
		// The per-BUILD work rate: the entity's OWN compiled entries name the handful it authored, rather than
		// a container the info no longer holds ([modifier.md par.5]; the own-data inversion).
		std::vector<std::pair<int, int> > kBuildWork;
		InfoValuation::collectKeyedTarget(kUnitCombat.getModifiers(), MODFAM_WORK_RATE, 0,
			InfoValuation::keyedTargetSegment("builds"), kBuildWork);
		for (size_t iI = 0; iI < kBuildWork.size(); ++iI)
		{
			m_worker->changeExtraWorkModForBuild((BuildTypes)kBuildWork[iI].first, kBuildWork[iI].second * iChange);
			bChanged = true;
		}
		if (bChanged) setInfoBarDirty(true);
	}
	changeRevoltProtection(kUnitCombat.getScalar(SCALAR_REVOLT_PROTECTION, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) * iChange);// merge/split
	changeCollateralDamageProtection(kUnitCombat.getCollateralModifier(COLLATERAL_PROTECTION, CASC_SCOPE_UNIT) * iChange);//no merge/split
	changePillageChange(kUnitCombat.getScalar(SCALAR_PILLAGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100 * iChange);//no merge/split
	// ⚠ costs.upgrade is sign-NORMALIZED as a COST modifier, so a DISCOUNT authors NEGATIVE. This accumulator
	// holds the discount, so the sign is flipped at the read rather than at every use.
	changeUpgradeDiscount(-kUnitCombat.getCostsModifier(COSTS_UPGRADE, CASC_SCOPE_UNIT) * iChange);//no merge/split (modified but not multiplicative)
	changeExperiencePercent(kUnitCombat.getExperienceModifier(EXPERIENCE_AMOUNT, CASC_SCOPE_UNIT) * iChange);//no merge/split (modified but not multiplicative)
	changeKamikazePercent(kUnitCombat.getCombatModifier(COMBAT_KAMIKAZE, CASC_SCOPE_UNIT) * iChange);//no merge/split
	changeCelebrityHappy(((kUnitCombat.hasSkill(CLS_SKILL_CELEBRITY) ? 1 : 0)) * iChange);//no merge/split
	changeCollateralDamageLimitChange((kUnitCombat.getFlatCollateral(COLLATERAL_LIMIT, CASC_SCOPE_UNIT) / 100) * iChange);//no merge/split
	changeCollateralDamageMaxUnitsChange((kUnitCombat.getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100) * iChange);//no merge/split
	changeCombatLimitChange((kUnitCombat.getFlatCombat(COMBAT_LIMIT, CASC_SCOPE_UNIT) / 100) * iChange);//no merge/split
	changeExtraDropRange((kUnitCombat.getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT) / 100) * iChange);//no merge/split
	// Both are pure boolean ENABLERS, so they are SKILLS ([skills.md]) -- the count accumulates presence.
	changeExtraNoDefensiveBonusCount((kUnitCombat.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS) ? 1 : 0) * iChange);
	changeExtraGatherHerdCount((kUnitCombat.hasSkill(CLS_SKILL_GATHER_HERD) ? 1 : 0) * iChange);
	changeSurvivorChance((kUnitCombat.getScalar(SCALAR_SURVIVOR, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT)) * iChange);//no merge/split
	// The combat FLATS are ×100; the reader reduces at its point of use ([DEC-fixedpoint-x100]).
	changeExtraBreakdownChance(kUnitCombat.getFlatCombat(COMBAT_BREAKDOWN_CHANCE, CASC_SCOPE_UNIT) / 100 * iChange);//no merge/split (larger/smaller just more/less survivable)
	changeExtraBreakdownDamage(kUnitCombat.getFlatCombat(COMBAT_BREAKDOWN_DAMAGE, CASC_SCOPE_UNIT) / 100 * iChange);//no merge/split
	// The SM figures are the `sizeMatters` BLOCK (json.md par.9), plain authored ints -- never a family, so no
	// de-scaling.
	changeExtraMaxHP(kUnitCombat.getSizeMatters().maxHP * iChange);//merge/split

	changeExtraCombatModifierPerSizeMore(kUnitCombat.getSizeMatters().combatModifierPerSizeMore * iChange);//no merge/split
	changeExtraCombatModifierPerSizeLess(kUnitCombat.getSizeMatters().combatModifierPerSizeLess * iChange);//no merge/split
	changeExtraCombatModifierPerVolumeMore(kUnitCombat.getSizeMatters().combatModifierPerVolumeMore * iChange);//no merge/split
	changeExtraCombatModifierPerVolumeLess(kUnitCombat.getSizeMatters().combatModifierPerVolumeLess * iChange);//no merge/split
	//
	// Pure boolean ENABLERS, so all five are SKILLS ([skills.md]); the count accumulates presence.
	changeExcileCount((kUnitCombat.hasSkill(CLS_SKILL_EXCILE) ? 1 : 0) * iChange);
	changePassageCount((kUnitCombat.hasSkill(CLS_SKILL_PASSAGE) ? 1 : 0) * iChange);
	changeNoNonOwnedCityEntryCount((kUnitCombat.hasSkill(CLS_SKILL_NO_NON_OWNED_CITY_ENTRY) ? 1 : 0) * iChange);
	changeBarbCoExistCount((kUnitCombat.hasSkill(CLS_SKILL_BARB_CO_EXIST) ? 1 : 0) * iChange);
	changeBlendIntoCityCount((kUnitCombat.hasSkill(CLS_SKILL_BLEND_INTO_CITY) ? 1 : 0) * iChange);
	//
	//

	//booleans //no merge/split
	changeDefensiveVictoryMoveCount((kUnitCombat.hasSkill(CLS_SKILL_DEFENSIVE_VICTORY_MOVE)) ? iChange : 0);//no merge/split
	changeFreeDropCount((kUnitCombat.hasSkill(CLS_SKILL_FREE_DROP)) ? iChange : 0);//no merge/split
	changeOffensiveVictoryMoveCount((kUnitCombat.hasSkill(CLS_SKILL_OFFENSIVE_VICTORY_MOVE)) ? iChange : 0);//no merge/split
	changeOneUpCount((kUnitCombat.hasSkill(CLS_SKILL_ONE_UP)) ? iChange : 0);//no merge/split
	changePillageEspionageCount((kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_ESPIONAGE)) ? iChange : 0);//no merge/split
	changePillageMarauderCount((kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_MARAUDER)) ? iChange : 0);//no merge/split
	changePillageOnMoveCount((kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_ON_MOVE)) ? iChange : 0);//no merge/split
	changePillageOnVictoryCount((kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_ON_VICTORY)) ? iChange : 0);//no merge/split
	changePillageResearchCount((kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_RESEARCH)) ? iChange : 0);//no merge/split
	changeBlitzCount((kUnitCombat.hasSkill(CLS_SKILL_BLITZ)) ? iChange : 0);//no merge/split
	changeAmphibCount((kUnitCombat.hasSkill(CLS_SKILL_AMPHIB)) ? iChange : 0);//no merge/split
	changeRiverCount((kUnitCombat.hasSkill(CLS_SKILL_RIVER)) ? iChange : 0);//no merge/split
	changeEnemyRouteCount((kUnitCombat.hasSkill(CLS_SKILL_ENEMY_ROUTE)) ? iChange : 0);//no merge/split
	changeAlwaysHealCount((kUnitCombat.hasSkill(CLS_SKILL_ALWAYS_HEAL)) ? iChange : 0);
	changeHillsDoubleMoveCount((kUnitCombat.hasSkill(CLS_SKILL_HILLS_DOUBLE_MOVE)) ? iChange : 0);
	changeImmuneToFirstStrikesCount(((kUnitCombat.hasSkill(CLS_SKILL_IMMUNE_TO_FIRST_STRIKES) || kUnitCombat.hasSkill(CLS_SKILL_FIRST_STRIKE_IMMUNE))) ? iChange : 0);
	changeAlwaysInvisibleCount((kUnitCombat.hasSkill(CLS_SKILL_ALWAYS_INVISIBLE)) ? iChange : 0);
	//	⛔ SKILLS ARE GRANT-ONLY ([skills.md] par.4): the add/remove PAIRS collapse to the grant alone, so the
	//	`-iChange` revoke halves are gone rather than re-pointed. An ability is granted, never taken away by a
	//	`false` -- which is what removes the special case rather than carrying it forward.
	changeStampedeCount((kUnitCombat.hasSkill(CLS_SKILL_STAMPEDE)) ? iChange : 0);
	changeOnslaughtCount((kUnitCombat.hasSkill(CLS_SKILL_ONSLAUGHT)) ? iChange : 0);
	changeAttackOnlyCitiesCount((kUnitCombat.hasSkill(CLS_SKILL_ATTACK_ONLY_CITIES)) ? iChange : 0);
	changeIgnoreNoEntryLevelCount((kUnitCombat.hasSkill(CLS_SKILL_IGNORE_NO_ENTRY_LEVEL)) ? iChange : 0);
	changeIgnoreZoneofControlCount((kUnitCombat.hasSkill(CLS_SKILL_IGNORE_ZONE_OF_CONTROL)) ? iChange : 0);
	changeFliesToMoveCount((kUnitCombat.hasSkill(CLS_SKILL_FLIES_TO_MOVE)) ? iChange : 0);
	//	`canPassPeaks` is DUAL-PLANE under one name ([skills.md]): a promotion/combat class grants the UNIT
	//	skill, TECH_MOUNTAINEERING grants the empire CAPABILITY, and the effective check is the OR of the two.
	//	The legacy `bCanMovePeaks` was the unit half, so it resolves here.
	changeCanMovePeaksCount((kUnitCombat.hasSkill(CLS_SKILL_CAN_PASS_PEAKS)) ? iChange : 0);
	changeCanLeadThroughPeaksCount((kUnitCombat.hasSkill(CLS_SKILL_CAN_LEAD_THROUGH_PEAKS)) ? iChange : 0);
	changeZoneOfControlCount((kUnitCombat.hasSkill(CLS_SKILL_ZONE_OF_CONTROL)) ? iChange : 0);
	changeCannotMergeSplitCount((kUnitCombat.hasSkill(CLS_SKILL_CANNOT_MERGE_SPLIT)) ? iChange : 0);
	changeNoSelfHealCount((kUnitCombat.hasSkill(CLS_SKILL_NO_SELF_HEAL)) ? iChange : 0);
	//	⛔ insidiousness/investigation are UNDERWORLD, never espionage ([json.md §6]: espionage is what SPY
	//	units do; a criminal hiding from an investigator is neither). Mis-filed in three curators historically,
	//	which is precisely why the boundary is written down.
	changeExtraInsidiousness(kUnitCombat.getUnderworld(UNDERWORLD_INSIDIOUSNESS, CASC_SCOPE_UNIT) / 100 * iChange);
	changeExtraInvestigation(kUnitCombat.getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100 * iChange);
	changeStealthDefenseCount((kUnitCombat.hasSkill(CLS_SKILL_STEALTH_DEFENSE) ? 1 : 0) * iChange);
	changeOnlyDefensiveCount((kUnitCombat.hasSkill(CLS_SKILL_DEFENSE_ONLY) ? 1 : 0) * iChange);
	changeNoInvisibilityCount((kUnitCombat.hasSkill(CLS_SKILL_NO_INVISIBILITY) ? 1 : 0) * iChange);
	changeNoCaptureCount((kUnitCombat.hasSkill(CLS_SKILL_NO_CAPTURE) ? 1 : 0) * iChange);

	//	The DOMAIN-keyed combat percents: the entity's OWN compiled entries name the handful of domains it
	//	authored, rather than a sweep of every domain asking whether it deposits (the own-data inversion).
	std::vector<std::pair<int, int> > kDomainCombat;
	InfoValuation::collectKeyedCombat(kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN,
		COMBAT_AMOUNT, kDomainCombat);
	for (size_t iD = 0; iD < kDomainCombat.size(); ++iD)
	{
		changeExtraDomainModifier((DomainTypes)kDomainCombat[iD].first, kDomainCombat[iD].second * iChange);
	}

	//	doubleMove is HALF MOVEMENT COST on that ground ([skills.md] par.1) -- a keyed MOVEMENT modifier, never a
	//	skill. The FK lists are materialized once at mapFrom, so these are bare member reads.
	const std::vector<int>& kTerrainDoubleMove = kUnitCombat.getTerrainDoubleMoves();
	for (size_t iT = 0; iT < kTerrainDoubleMove.size(); ++iT)
	{
		changeTerrainDoubleMoveCount((TerrainTypes)kTerrainDoubleMove[iT], iChange);
	}

	const std::vector<int>& kFeatureDoubleMove = kUnitCombat.getFeatureDoubleMoves();
	for (size_t iF = 0; iF < kFeatureDoubleMove.size(); ++iF)
	{
		changeFeatureDoubleMoveCount((FeatureTypes)kFeatureDoubleMove[iF], iChange);
	}


	//	⛔ THE PER-TYPE AND PER-SUBSTRATE HIDE-AND-SEEK TABLES ARE RETIRED ([vision.md] §4). The 13 tables
	//	(visibility/invisibility intensity per INVISIBLE_*, their range and same-tile variants, and the
	//	terrain/feature/improvement conditional sets) collapsed onto TWO members: `hideAndSeek.concealment`
	//	-- how well this hides -- and `hideAndSeek.detection`, each entry naming the METHOD it answers as a
	//	SKILL. The method is a skill because a promotion can grant one, which a tag cannot hold.
	//	⚠ The marginal per-substrate loss is a DELIBERATE owner ruling, not an omission to restore: 270 of
	//	355 authoring entities named exactly ONE type, so the 14×13 surface served a quarter of its own data.
	//	The unit reads its folded answer through CvUnit::concealment() / detectionAgainst(skill).
	//	The TERRAIN / FEATURE / UNITCOMBAT keyed axes, read off the entity's OWN compiled entries -- the handful
	//	it authored, never a walk of a keyed container the info no longer holds. `combat` is what modifies
	//	`strength` (json.md §6), so attack/defense are combat kinds; the work rate is its own family.
	//	⚠ All of these are PERCENTS, so none is scaled ([DEC-fixedpoint-x100]).
	std::vector<std::pair<int, int> > kKeyed;
	InfoValuation::collectKeyedCombat(kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, COMBAT_ATTACK, kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraTerrainAttackPercent((TerrainTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	InfoValuation::collectKeyedCombat(kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, COMBAT_DEFENSE, kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraTerrainDefensePercent((TerrainTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	InfoValuation::collectKeyedTarget(kUnitCombat.getModifiers(), MODFAM_WORK_RATE, 0,
		InfoValuation::keyedTargetSegment("terrain"), kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraTerrainWorkPercent((TerrainTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	InfoValuation::collectKeyedCombat(kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, COMBAT_ATTACK, kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraFeatureAttackPercent((FeatureTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	InfoValuation::collectKeyedCombat(kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, COMBAT_DEFENSE, kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraFeatureDefensePercent((FeatureTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	InfoValuation::collectKeyedTarget(kUnitCombat.getModifiers(), MODFAM_WORK_RATE, 0,
		InfoValuation::keyedTargetSegment("feature"), kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraFeatureWorkPercent((FeatureTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	InfoValuation::collectKeyedCombat(kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT, COMBAT_AMOUNT, kKeyed);
	for (size_t iK = 0; iK < kKeyed.size(); ++iK)
		changeExtraUnitCombatModifier((UnitCombatTypes)kKeyed[iK].first, kKeyed[iK].second * iChange);

	{
		std::vector<std::pair<int, int> > flankRows;
		InfoValuation::collectKeyedTarget(kUnitCombat.getModifiers(), MODFAM_COMBAT, COMBAT_AMOUNT,
			InfoValuation::keyedTargetSegment("flanking"), flankRows, CASC_SCOPE_UNIT);
		for (size_t iRow = 0; iRow < flankRows.size(); ++iRow)
		{
			changeExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)flankRows[iRow].first, flankRows[iRow].second * iChange);
		}
	}


	if (bSM && bByPromo)
	{
		setSMValues();
	}

	if (kUnitCombat.getReligion() != NO_RELIGION)
	{
		defineReligion();
	}


	establishBuildups();

	if (bByPromo)
	{
		setGGExperienceEarnedTowardsType();
	}
}

void CvUnit::setHasUnitCombat(UnitCombatTypes eIndex, bool bNewValue, bool bByPromo)
{
	PROFILE_FUNC();

	if (isHasUnitCombat(eIndex) != bNewValue)
	{
		const CvUnitCombatInfo& info = GC.getUnitCombatInfo(eIndex);

		if (GC.getGame().isValidByGameOption(info)
		// Disable spy promotions mechanism, exempt commando promotion
		&& (!isSpy() || GC.isSS_ENABLED() || info.providesSkill(CLS_SKILL_ENEMY_ROUTE)))
		{
			// The commit, the hash and the fact; then the STATS this class carries, then the effects below.
			setHasUnitCombatInternal(eIndex, bNewValue);
			processUnitCombat(eIndex, bNewValue, bByPromo);

			AI_flushValueCache();

			//	Updates the grpahics last after everything is calculated
			//  Not entirely sure this will be necessary?  Koshling what say you?
			if (IsSelected())
			{
				gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
				gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
			}

			//update graphics
			if (!isUsingDummyEntities() && isInViewport())
			{
				gDLL->getEntityIFace()->updatePromotionLayers(getUnitEntity());
			}
		}
	}
}

bool CvUnit::isHasPromotion(PromotionTypes eIndex) const
{
	FASSERT_BOUNDS(NO_PROMOTION, GC.getNumPromotionInfos(), eIndex);
	const PromotionKeyedInfo* info = findPromotionKeyedInfo(eIndex);

	return (info != NULL && info->m_bHasPromotion);
}

// COMMIT the flag, ANNOUNCE the fact. Both transitions announce -- a one-way fact would leave a survivor
// permanently marked dying. The PLOT is passed in because the load knows the unit's id long before its
// coordinates deserialize, and the id is what a consumer keys on.
void CvUnit::setDeathDelayInternal(bool bNewValue)
{
	if (m_bDeathDelay == bNewValue)
	{
		return;
	}
	m_bDeathDelay = bNewValue;
	const int iPlotNum = (plot() != NULL) ? GC.getMap().plotNum(getX(), getY()) : -1;
	if (bNewValue)
	{
		emitUnitDeathScheduleAdded((int)getUnitType(), getID(), (int)getOwner(), iPlotNum);
	}
	else
	{
		emitUnitDeathScheduleRemoved((int)getUnitType(), getID(), (int)getOwner(), iPlotNum);
	}
}

// COMMIT the keyed flag, MAINTAIN the movement hash, ANNOUNCE the fact -- and nothing else. The twin of
// setHasUnitCombatInternal, and the same shape because the commit is the same: one keyed bool. processPromotion
// below keeps the STATS the promotion carries, which a load must not re-apply (they are serialized on the unit
// in their own right).
void CvUnit::setHasPromotionInternal(PromotionTypes eIndex, bool bNewValue)
{
	PromotionKeyedInfo* info =
	(
		bNewValue
		?
		findOrCreatePromotionKeyedInfo(eIndex)
		:
		(PromotionKeyedInfo*)findPromotionKeyedInfo(eIndex)
	);
	if (info == NULL || info->m_bHasPromotion == bNewValue)
	{
		return;
	}
	info->m_bHasPromotion = bNewValue;

	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(eIndex);
	if (kPromotion.changesMoveThroughPlots())
	{
		m_movementCharacteristicsHash ^= kPromotion.getZobristValue();
		m_iMaxMoveCacheTurn = -1;
	}
	if (bNewValue)
	{
		emitUnitPromotionAdded(getID(), (int)getOwner(), (int)eIndex);
	}
	else
	{
		emitUnitPromotionRemoved(getID(), (int)getOwner(), (int)eIndex);
	}
}

void CvUnit::processPromotion(PromotionTypes eIndex, bool bAdding, bool bInitial)
{
	PROFILE_EXTRA_FUNC();
	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(eIndex);
	const int iChange = (bAdding ? 1 : -1);
	int	iI;
	bool bSMrecalc = false;
	// #430 event spine: the unit plane's FIRST dirty trigger (state-repositories.md -- a unit's resolved values
	// dirty ONLY on a promotion or combat-class change). This is the ONE funnel: both setHasPromotion overloads
	// reach it, past the has-flag guard, with the flag already written.
	// ⚠ A unit's INITIAL free promotions run through here from CvUnit::init BEFORE emitUnitCreated (whose position
	// is pinned between doSetFreePromotions and doSetDefaultStatuses -- see init), so these facts can precede the
	// instance fact. That is sound rather than tolerated: spine events are FACTS, order-independent and
	// prerequisite-free (event-spine.md), so a consumer resolves the unit by id and never by arrival order.



	if (kPromotion.getDomainCargoChange() != NO_DOMAIN)
	{
		if (bAdding)
		{
			setNewDomainCargo((DomainTypes)kPromotion.getDomainCargoChange());
		}
		else
		{
			setNewDomainCargo(NO_DOMAIN);
		}
	}
	if (kPromotion.getSpecialCargoChange() != NO_SPECIALUNIT)
	{
		if (bAdding)
		{
			setNewSpecialCargo((SpecialUnitTypes)kPromotion.getSpecialCargoChange());
		}
		else
		{
			setNewSpecialCargo(NO_SPECIALUNIT);
		}
	}

	if (kPromotion.getSMNotSpecialCargoChange() != NO_SPECIALUNIT)
	{
		if (bAdding)
		{
			setNewSMNotSpecialCargo((SpecialUnitTypes)kPromotion.getSMNotSpecialCargoChange());
		}
		else
		{
			setNewSMNotSpecialCargo(NO_SPECIALUNIT);
		}
	}

	changeBlitzCount((kPromotion.providesSkill(CLS_SKILL_BLITZ)) ? iChange : 0);
	changeAmphibCount((kPromotion.providesSkill(CLS_SKILL_AMPHIB)) ? iChange : 0);
	changeRiverCount((kPromotion.providesSkill(CLS_SKILL_RIVER)) ? iChange : 0);
	changeEnemyRouteCount((kPromotion.providesSkill(CLS_SKILL_ENEMY_ROUTE)) ? iChange : 0);
	changeAlwaysHealCount((kPromotion.providesSkill(CLS_SKILL_ALWAYS_HEAL)) ? iChange : 0);
	changeHillsDoubleMoveCount((kPromotion.providesSkill(CLS_SKILL_HILLS_DOUBLE_MOVE)) ? iChange : 0);

	changeCanMovePeaksCount((kPromotion.providesSkill(CLS_SKILL_CAN_PASS_PEAKS)) ? iChange : 0);
	//	Koshling - enhanced mountaineering mode to differentiate between ability to move through
	//	mountains, and ability to lead a stack through mountains
	changeCanLeadThroughPeaksCount((kPromotion.providesSkill(CLS_SKILL_CAN_LEAD_THROUGH_PEAKS)) ? iChange : 0);

	// Toffer - Assume promotions with commander stats can only be gained by commanders.
	//	If assumption is wrong, we'll need setCommander(true) to go through all promotions the unit has,
	//	and apply commander specific stats at that point.
	if (isCommander())
	{
		m_commander->changeControlPoints(kPromotion.getControlPoints() * iChange);
		m_commander->changeCommandRange(kPromotion.getCommandRange() * iChange);
	}
	if (isCommodore())
    {
    	m_commodore->changeControlPoints(kPromotion.getControlPoints() * iChange);
    	m_commodore->changeCommandRange(kPromotion.getCommandRange() * iChange);
    }

	changeImmuneToFirstStrikesCount(((kPromotion.providesSkill(CLS_SKILL_IMMUNE_TO_FIRST_STRIKES) || kPromotion.providesSkill(CLS_SKILL_FIRST_STRIKE_IMMUNE))) ? iChange : 0);

	changeDefensiveVictoryMoveCount((kPromotion.providesSkill(CLS_SKILL_DEFENSIVE_VICTORY_MOVE)) ? iChange : 0);
	changeFreeDropCount((kPromotion.providesSkill(CLS_SKILL_FREE_DROP)) ? iChange : 0);
	changeOffensiveVictoryMoveCount((kPromotion.providesSkill(CLS_SKILL_OFFENSIVE_VICTORY_MOVE)) ? iChange : 0);

	changeOneUpCount((kPromotion.providesSkill(CLS_SKILL_ONE_UP)) ? iChange : 0);
	changePillageEspionageCount((kPromotion.providesSkill(CLS_SKILL_PILLAGE_ESPIONAGE)) ? iChange : 0);
	changePillageMarauderCount((kPromotion.providesSkill(CLS_SKILL_PILLAGE_MARAUDER)) ? iChange : 0);
	changePillageOnMoveCount((kPromotion.providesSkill(CLS_SKILL_PILLAGE_ON_MOVE)) ? iChange : 0);
	changePillageOnVictoryCount((kPromotion.providesSkill(CLS_SKILL_PILLAGE_ON_VICTORY)) ? iChange : 0);
	changePillageResearchCount((kPromotion.providesSkill(CLS_SKILL_PILLAGE_RESEARCH)) ? iChange : 0);
	changeCelebrityHappy(((kPromotion.providesSkill(CLS_SKILL_CELEBRITY) ? 1 : 0)) * iChange);
	changeCollateralDamageLimitChange((kPromotion.getFlatCollateral(COLLATERAL_LIMIT, CASC_SCOPE_UNIT) / 100) * iChange);
	changeCollateralDamageMaxUnitsChange((kPromotion.getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100) * iChange);
	changeCombatLimitChange((kPromotion.getFlatCombat(COMBAT_LIMIT, CASC_SCOPE_UNIT) / 100) * iChange);
	changeExtraDropRange((kPromotion.getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT) / 100) * iChange);
	changeExtraNoDefensiveBonusCount((kPromotion.providesSkill(CLS_SKILL_NO_DEFENSIVE_BONUS) ? 1 : 0) * iChange);
	changeExtraGatherHerdCount((kPromotion.providesSkill(CLS_SKILL_GATHER_HERD) ? 1 : 0) * iChange);

	changeSurvivorChance((kPromotion.getScalar(SCALAR_SURVIVOR, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT)) * iChange);
	//	the heal accumulators carry whole hit points; the deposits are ×100 flats ([DEC-fixedpoint-x100])

	changeExtraMoves(kPromotion.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100 * iChange);
	changeExtraMoveDiscount(kPromotion.getMovement(MOVEMENT_MOVE_DISCOUNT, CASC_SCOPE_UNIT) / 100 * iChange);
	//TB Combat Mods Begin


	//	skills are GRANT-ONLY ([skills.md] §4): the legacy add/remove pairs collapse to the grant, so the
	//	revoke half of each is gone rather than reading a `false` plane.
	changeStampedeCount((kPromotion.providesSkill(CLS_SKILL_STAMPEDE)) ? iChange : 0);
	changeAttackOnlyCitiesCount((kPromotion.providesSkill(CLS_SKILL_ATTACK_ONLY_CITIES)) ? iChange : 0);
	changeIgnoreNoEntryLevelCount((kPromotion.providesSkill(CLS_SKILL_IGNORE_NO_ENTRY_LEVEL)) ? iChange : 0);
	changeIgnoreZoneofControlCount((kPromotion.providesSkill(CLS_SKILL_IGNORE_ZONE_OF_CONTROL)) ? iChange : 0);
	changeFliesToMoveCount((kPromotion.providesSkill(CLS_SKILL_FLIES_TO_MOVE)) ? iChange : 0);
	if (kPromotion.getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT) != 0)
	{
		bSMrecalc = true;
	}
	changeOnslaughtCount((kPromotion.providesSkill(CLS_SKILL_ONSLAUGHT)) ? iChange : 0);

	//	the combat accumulators carry whole points; the deposits are ×100 flats ([DEC-fixedpoint-x100])
	changeExtraBreakdownChance(kPromotion.getFlatCombat(COMBAT_BREAKDOWN_CHANCE, CASC_SCOPE_UNIT) / 100 * iChange);
	changeExtraBreakdownDamage(kPromotion.getFlatCombat(COMBAT_BREAKDOWN_DAMAGE, CASC_SCOPE_UNIT) / 100 * iChange);
	changeExcileCount((kPromotion.providesSkill(CLS_SKILL_EXCILE) ? 1 : 0) * iChange);
	changePassageCount((kPromotion.providesSkill(CLS_SKILL_PASSAGE) ? 1 : 0) * iChange);
	changeNoNonOwnedCityEntryCount((kPromotion.providesSkill(CLS_SKILL_NO_NON_OWNED_CITY_ENTRY) ? 1 : 0) * iChange);
	changeBarbCoExistCount((kPromotion.providesSkill(CLS_SKILL_BARB_CO_EXIST) ? 1 : 0) * iChange);
	changeBlendIntoCityCount((kPromotion.providesSkill(CLS_SKILL_BLEND_INTO_CITY) ? 1 : 0) * iChange);
	changeUpgradeAnywhereCount((kPromotion.providesSkill(CLS_SKILL_UPGRADE_ANYWHERE) ? 1 : 0) * iChange);

	//	maxHP is applied from the sizeMatters section below, where json.md §9 homes the promotion's SM deltas.
	//	The value itself is gathered by the RESOLVED plane off the held set; only the size-matters recalc rider
	//	belongs at the apply site.
	if (kPromotion.getCombatModifier(COMBAT_AMOUNT, CASC_SCOPE_UNIT) != 0)
	{
		bSMrecalc = true;
	}
	//TB Combat Mods End
	if (kPromotion.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) != 0)
	{
		changeExtraBombardRate(kPromotion.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) * iChange);
		bSMrecalc = true;
	}
	// Assume only worker units can get the relevant promotions, if not then we'll need a retroactive unitComp late init function.
	if (isWorker())
	{
		bool bChanged = false;
		//	workRate percents are whole numbers and are NOT ×100 ([DEC-fixedpoint-x100]: a percent carries no
		//	decimals), so these re-point 1:1 with no scale step. The per-TERRAIN rows are applied by the
		//	terrain loop below, which is where a peak's rate now lives too — TERRAIN_PEAK is a terrain.
		const int iHillsWork = kPromotion.getScalar(SCALAR_WORK_RATE_HILLS, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
		if (iHillsWork != 0)
		{
			m_worker->changeHillsWorkModifier(iHillsWork * iChange);
			bChanged = true;
		}
		const int iWorkRate = kPromotion.getScalar(SCALAR_WORK_RATE, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
		if (iWorkRate != 0)
		{
			m_worker->changeWorkModifier(iWorkRate * iChange);
			bChanged = true;
		}
		//	the per-BUILD rows are the entity's OWN compiled entries, walked instead of asking every BUILD id
		//	whether this promotion deposits onto it (the own-data inversion, [modifier.md] §5)
		{
			std::vector<std::pair<int, int> > buildRates;
			InfoValuation::collectKeyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, -1,
				InfoValuation::keyedTargetSegment("builds"), buildRates, CASC_SCOPE_UNIT);
			for (size_t iRow = 0; iRow < buildRates.size(); ++iRow)
			{
				m_worker->changeExtraWorkModForBuild((BuildTypes)buildRates[iRow].first, buildRates[iRow].second * iChange);
				bChanged = true;
			}
		}
		if (bChanged) setInfoBarDirty(true);
	}
	//	percent-unit slots re-point 1:1 (a percent is never ×100); flat-unit slots reduce at this point of use.
	changeRevoltProtection(kPromotion.getScalar(SCALAR_REVOLT_PROTECTION, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) * iChange);
	changeCollateralDamageProtection(kPromotion.getCollateralModifier(COLLATERAL_PROTECTION, CASC_SCOPE_UNIT) * iChange);
	changePillageChange(kPromotion.getScalar(SCALAR_PILLAGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100 * iChange);
	changeUpgradeDiscount(kPromotion.getCostsModifier(COSTS_UPGRADE, CASC_SCOPE_UNIT) * iChange);
	changeExperiencePercent(kPromotion.getExperienceModifier(EXPERIENCE_AMOUNT, CASC_SCOPE_UNIT) * iChange);
	changeKamikazePercent(kPromotion.getCombatModifier(COMBAT_KAMIKAZE, CASC_SCOPE_UNIT) * iChange);
	if (kPromotion.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) != 0)
	{
		changeCargoSpace(kPromotion.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 * iChange);
		bSMrecalc = true;
	}
	//	the SM cargo trio are sizeMatters deltas ([json.md] §9), not the cargo family
	if (kPromotion.getSizeMatters().cargoSmSpace != 0)
	{
		changeSMCargoSpace(kPromotion.getSizeMatters().cargoSmSpace * iChange);
		bSMrecalc = true;
	}
	if (kPromotion.getSizeMatters().cargoVolume != 0)
	{
		changeExtraCargoVolume(kPromotion.getSizeMatters().cargoVolume * iChange);
		bSMrecalc = true;
	}
	if (kPromotion.getSizeMatters().cargoVolumeModifier != 0)
	{
		changeCargoVolumeModifier(kPromotion.getSizeMatters().cargoVolumeModifier * iChange);
		bSMrecalc = true;
	}

	changeExtraCombatModifierPerSizeMore(kPromotion.getSizeMatters().combatModifierPerSizeMore * iChange);//no merge/split
	changeExtraCombatModifierPerSizeLess(kPromotion.getSizeMatters().combatModifierPerSizeLess * iChange);//no merge/split
	changeExtraCombatModifierPerVolumeMore(kPromotion.getSizeMatters().combatModifierPerVolumeMore * iChange);//no merge/split
	changeExtraCombatModifierPerVolumeLess(kPromotion.getSizeMatters().combatModifierPerVolumeLess * iChange);//no merge/split

	changeNoSelfHealCount((kPromotion.providesSkill(CLS_SKILL_NO_SELF_HEAL)) ? iChange : 0);
	changeExtraInsidiousness(kPromotion.getUnderworld(UNDERWORLD_INSIDIOUSNESS, CASC_SCOPE_UNIT) / 100 * iChange);
	changeExtraInvestigation(kPromotion.getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100 * iChange);
	changeAssassinCount((kPromotion.providesSkill(CLS_SKILL_ASSASSIN) ? 1 : 0) * iChange);
	changeStealthDefenseCount((kPromotion.providesSkill(CLS_SKILL_STEALTH_DEFENSE) ? 1 : 0) * iChange);
	changeOnlyDefensiveCount((kPromotion.providesSkill(CLS_SKILL_DEFENSE_ONLY) ? 1 : 0) * iChange);
	changeNoInvisibilityCount((kPromotion.providesSkill(CLS_SKILL_NO_INVISIBILITY) ? 1 : 0) * iChange);
	changeHiddenNationalityCount((kPromotion.providesSkill(CLS_SKILL_HIDDEN_NATIONALITY) ? 1 : 0) * iChange);

	// the promotion's SM rank deltas live in its sizeMatters section (json.md par.9: Promotion carries the
	// runtime deltas), applied to the engine extra-accumulators on top of the info-derived base ranks
	if (kPromotion.getSizeMatters().quality != 0)
	{
		changeExtraQuality(kPromotion.getSizeMatters().quality * iChange);
		bSMrecalc = true;
	}
	if (kPromotion.getSizeMatters().group != 0)
	{
		changeExtraGroup(kPromotion.getSizeMatters().group * iChange);
		bSMrecalc = true;
	}
	// json.md par.9 names FOUR promotion delta scalars -- quality / group / sizeModifier / maxHP -- all "applied
	// as changes when the promotion is gained". The last two were parsed and then read by nothing, so a
	// promotion authoring them silently did nothing at all.
	if (kPromotion.getSizeMatters().sizeModifier != 0)
	{
		changeExtraSize(kPromotion.getSizeMatters().sizeModifier * iChange);
		bSMrecalc = true;
	}
	if (kPromotion.getSizeMatters().maxHP != 0)
	{
		changeExtraMaxHP(kPromotion.getSizeMatters().maxHP * iChange);
	}


	if (kPromotion.hasSkill(CLS_SKILL_ZONE_OF_CONTROL))
	{
		changeZoneOfControlCount(iChange > 0 ? 1 : -1);
	}

	for (iI = 0; iI < GC.getNumTerrainInfos(); iI++)
	{
		changeExtraTerrainAttackPercent(((TerrainTypes)iI), (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_ATTACK) * iChange));
		changeExtraTerrainDefensePercent(((TerrainTypes)iI), (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_DEFENSE) * iChange));
	}

	//	the per-terrain / per-feature workRate rows and the double-move lists are the promotion's OWN compiled
	//	entries, walked instead of asking every terrain/feature id whether it deposits ([modifier.md] §5)
	{
		std::vector<std::pair<int, int> > workRows;
		InfoValuation::collectKeyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, -1,
			InfoValuation::keyedTargetSegment("terrain"), workRows, CASC_SCOPE_UNIT);
		for (size_t iRow = 0; iRow < workRows.size(); ++iRow)
		{
			changeExtraTerrainWorkPercent((TerrainTypes)workRows[iRow].first, workRows[iRow].second * iChange);
		}
		const std::vector<int>& kTerrainDoubleMove = kPromotion.getTerrainDoubleMoves();
		for (size_t iRow = 0; iRow < kTerrainDoubleMove.size(); ++iRow)
		{
			changeTerrainDoubleMoveCount((TerrainTypes)kTerrainDoubleMove[iRow], iChange);
		}
	}

	for (iI = 0; iI < GC.getNumFeatureInfos(); iI++)
	{
		changeExtraFeatureAttackPercent(((FeatureTypes)iI), (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_ATTACK) * iChange));
		changeExtraFeatureDefensePercent(((FeatureTypes)iI), (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_DEFENSE) * iChange));
	}

	{
		std::vector<std::pair<int, int> > workRows;
		InfoValuation::collectKeyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, -1,
			InfoValuation::keyedTargetSegment("feature"), workRows, CASC_SCOPE_UNIT);
		for (size_t iRow = 0; iRow < workRows.size(); ++iRow)
		{
			changeExtraFeatureWorkPercent((FeatureTypes)workRows[iRow].first, workRows[iRow].second * iChange);
		}
		const std::vector<int>& kFeatureDoubleMove = kPromotion.getFeatureDoubleMoves();
		for (size_t iRow = 0; iRow < kFeatureDoubleMove.size(); ++iRow)
		{
			changeFeatureDoubleMoveCount((FeatureTypes)kFeatureDoubleMove[iRow], iChange);
		}
	}

	//	⛔ RETIRED with the unit-combat twin above ([vision.md] §4): the per-type and per-substrate
	//	hide-and-seek tables collapse onto `hideAndSeek.concealment` + `detection`, each detection entry
	//	naming the METHOD it answers as a SKILL -- which is exactly why the method had to be a skill and
	//	not a tag: 73 PROMOTIONS author one, and a tag is not promotion-grantable, so a tag reading would
	//	have dropped every one of them on the floor.

	const int numUnitCombatInfos = GC.getNumUnitCombatInfos();
	for (iI = 0; iI < numUnitCombatInfos; iI++)
	{
		changeExtraUnitCombatModifier(((UnitCombatTypes)iI), (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT, iI, COMBAT_AMOUNT) * iChange));
	}

	//	FLANKING is keyed by UNITCOMBAT ([json.md] §6: `combat.<scope>.flanking.{UNITCOMBAT_X}`), so the rows are
	//	the entity's OWN authored handful rather than a sweep of every combat class. The values are percents and
	//	are therefore not ×100 ([DEC-fixedpoint-x100]).
	{
		std::vector<std::pair<int, int> > flankRows;
		InfoValuation::collectKeyedTarget(kPromotion.getModifiers(), MODFAM_COMBAT, COMBAT_AMOUNT,
			InfoValuation::keyedTargetSegment("flanking"), flankRows, CASC_SCOPE_UNIT);
		for (size_t iRow = 0; iRow < flankRows.size(); ++iRow)
		{
			changeExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)flankRows[iRow].first, flankRows[iRow].second * iChange);
		}
	}

	for (iI = 0; iI < (int)kPromotion.providesUnitCombats().size(); iI++)
	{
		setHasUnitCombat(((UnitCombatTypes)kPromotion.providesUnitCombats()[iI]), bAdding, true);
	}

	for (iI = 0; iI < (int)kPromotion.removesUnitCombats().size(); iI++)
	{
		setHasUnitCombat(((UnitCombatTypes)kPromotion.removesUnitCombats()[iI]), bAdding ? false : true, true);
	}


	{
		std::vector<HealByUnitCombat> healRows;
		InfoValuation::collectHealByUnitCombat(kPromotion.getModifiers(), healRows);
		for (size_t iRow = 0; iRow < healRows.size(); ++iRow)
		{
			const HealByUnitCombat& kRow = healRows[iRow];
			const UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)kRow.iUnitCombat;
			//	the accumulators carry whole hit points; the deposits are ×100 amounts
			changeHealUnitCombatTypeVolume(eHealUnitCombat, kRow.iHeal / 100 * iChange);
			changeHealUnitCombatTypeAdjacentVolume(eHealUnitCombat, kRow.iAdjacentHeal / 100 * iChange);
		}
	}

	//TB Combat Mod End

	for (iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		changeExtraDomainModifier(((DomainTypes)iI), (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN, iI, COMBAT_AMOUNT) * iChange));
	}

	if (bAdding && bInitial && kPromotion.isZeroesXP())
	{
		setExperience100(0);
	}

	if (bSMrecalc && GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		setSMValues();
	}

	establishBuildups();
}

void CvUnit::setHasPromotion(PromotionTypes eIndex, bool bNewValue, PromotionApply::flags flags)
{
	setHasPromotion
	(
		eIndex, bNewValue,
		flags & PromotionApply::Free,
		flags & PromotionApply::Dying,
		flags & PromotionApply::Initial,
		flags & PromotionApply::FromTrait
	);
}

void CvUnit::setHasPromotion(PromotionTypes eIndex, bool bNewValue, bool bFree, bool bDying, bool bInitial, bool bFromTrait)
{
	PROFILE_FUNC();

	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), eIndex);

	if (eIndex == NO_PROMOTION)
	{
		FErrorMsg("Invalid promotion");
		return;
	}

	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(eIndex);
	// Disable spy promotions mechanism
	bool canPromote = !isSpy() || GC.isSS_ENABLED() || kPromotion.providesSkill(CLS_SKILL_ENEMY_ROUTE); //exempt commando promotion

	bool bAssignFree = false;
	if (bFree)
	{
		if (bNewValue)
		{
			// Check canKeep to ensure we're not wasting our time on free promos
			canPromote = canKeepPromotion(eIndex, true, false);
			// Following removes the need to count all those promos as free.
			if (canPromote)
			{
				bAssignFree = true;
			}
		}
		else // Remove free status
		{
			setPromotionFreeCount(eIndex, 0);

			if (bFromTrait)
			{
				setPromotionFromTrait(eIndex, 0);
			}
		}
	}

	if (isHasPromotion(eIndex) != bNewValue)
	{
		// If we check all normal promotions through this routine we'll accidentally disqualify a number of free promos on the basis of tech prereq.
		// Important this only runs a check for equps and afflicts.  All other means of getting here should
		// already be checked in their own way.  A better check for those would be canKeepPromotion() which they should
		// run up against regularly by default anyhow.  If we notice units getting free promos they can't keep, then
		// we'll have to find the source and check against canKeepPromotion before they qualify to get to setHasPromotion in the first place.


		if (canPromote)
		{
			// A VISION promotion moves the unit's sight strength IN PLACE, so the standing unit's seen region
			// is re-bracketed around the commit: the remove resolves against the OLD strength while it still
			// holds (the withdrawal rule, [state-repositories.md] § THE INVARIANT). Without it the visibility
			// counts skew on the unit's next move -- it removes a region it never added. The strength is
			// base + combat classes + promotions (vision.md), so a promotion moves it by its own authored
			// vision OR by a vision-authoring combat class it provides or removes.
			bool bMovesSight = kPromotion.getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) != 0;
			for (int iProvided = 0; !bMovesSight && iProvided < (int)kPromotion.providesUnitCombats().size(); ++iProvided)
			{
				bMovesSight = GC.getUnitCombatInfo((UnitCombatTypes)kPromotion.providesUnitCombats()[iProvided]).getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) != 0;
			}
			for (int iRemoved = 0; !bMovesSight && iRemoved < (int)kPromotion.removesUnitCombats().size(); ++iRemoved)
			{
				bMovesSight = GC.getUnitCombatInfo((UnitCombatTypes)kPromotion.removesUnitCombats()[iRemoved]).getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) != 0;
			}
			CvPlot* pSightPlot = bMovesSight ? plot() : NULL;
			if (pSightPlot != NULL)
			{
				pSightPlot->changeAdjacentSight(getTeam(), sight(pSightPlot), false, this, true);
			}

			// The commit, the hash and the fact; then the STATS this promotion carries, then the effects below.
			setHasPromotionInternal(eIndex, bNewValue);

			if (bAssignFree)
			{
				setPromotionFreeCount(eIndex, 1);
				if (bFromTrait)
				{
					setPromotionFromTrait(eIndex, 1);
				}
			}

			// Never Initial if Free or Removing
			if (bInitial && (!bNewValue || bFree))
			{
				bInitial = false;
			}

			processPromotion(eIndex, bNewValue, bInitial);

			if (pSightPlot != NULL)
			{
				pSightPlot->changeAdjacentSight(getTeam(), sight(pSightPlot), true, this, true);
			}

			AI_flushValueCache();

			// A unit can only have a single promotion in a promotion line for equipment or affliction promotions,
			//	if we're applying a higher priority one make sure any lower priority one from the same line that was present previously is removed
			// QUESTION FOR TB - should removing an afflication add in the affliction below it (priority wise)
			//	in its line??  (I assume not for equipments, but wasn't sure for afflictions)
			if (bNewValue)
			{
				if (kPromotion.isRemoveAfterSet())
				{
					setHasPromotion(eIndex, false, bFree, bDying, bInitial);
				}
			}

			// When promotions are being removed as part of killing a unit we dont want to add any more or invoke obsoletion checks,
			//	which results in lots of extra processing, and can also generate retrain messages for the dying units!
			if (!isDead() && !bDying)
			{
				checkPromotionObsoletion();
			}

			//	Updates the grpahics last after everything is calculated
			if (IsSelected())
			{
				gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
				gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
			}

			//update graphics
			if (!isUsingDummyEntities() && isInViewport())
			{
				gDLL->getEntityIFace()->updatePromotionLayers(getUnitEntity());
			}
		}
	}
}


bool CvUnit::applyUnitPromotions(const std::vector<CvUnit*>& units, int number, PromotionPredicateFn promotionPredicateFn)
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(number >= 0, "Number of promotions to apply cannot be negative");

	if (units.size() == 0)
		return true;

	while (number > 0)
	{
		PromotionTypes foundPromo = GC.findPromotion(promotionPredicateFn);
		if (foundPromo == NO_PROMOTION)
			break;
		foreach_(CvUnit* unit, units)
		{
			unit->setHasPromotion(foundPromo, true, PromotionApply::Free);
		}
		number--;
	}
	return number == 0;
}

bool CvUnit::applyUnitPromotions(CvUnit* unit, int number, PromotionPredicateFn promotionPredicateFn)
{
	std::vector<CvUnit*> units;
	units.push_back(unit);
	return applyUnitPromotions(units, number, promotionPredicateFn);
}

bool CvUnit::normalizeUnitPromotions(const std::vector<CvUnit*>& units, int offset, PromotionPredicateFn upgradePredicateFn, PromotionPredicateFn downgradePredicateFn)
{
	return offset == 0 ? true : applyUnitPromotions(units, std::abs(offset), offset > 0 ? upgradePredicateFn : downgradePredicateFn);
}

bool CvUnit::normalizeUnitPromotions(CvUnit* unit, int offset, PromotionPredicateFn upgradePredicateFn, PromotionPredicateFn downgradePredicateFn)
{
	return offset == 0 ? true : applyUnitPromotions(unit, std::abs(offset), offset > 0 ? upgradePredicateFn : downgradePredicateFn);
}

UnitCombatTypes CvUnit::getBestHealingType()
{
	return getBestHealingTypeConst();
}

//	The healer class this unit is strongest in. Only a class the unit carries a heal VOLUME for can win, and a
//	volume exists only where a keyed entry does (every feeder goes through changeHealUnitCombatTypeVolume, which
//	creates one) -- so the unit's OWN keyed map IS the candidate set. Asking the whole unitcombat registry was the
//	own-data inversion: it put a ~470-wide scan on an AI path to find the handful the unit already names.
//	⚠ The candidate set is deliberately NOT the info's authored heal rows: the volume is fed by the unit's info AND
//	by its promotions, so reading the info alone would silently drop every promotion-granted healer class.
UnitCombatTypes CvUnit::getBestHealingTypeConst() const
{
	PROFILE_EXTRA_FUNC();
	UnitCombatTypes eBestUnitCombat = NO_UNITCOMBAT;
	int iBestValue = 0;

	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
	{
		if (!GC.getUnitCombatInfo(it->first).hasSkill(CLS_SKILL_HEALS_AS))
		{
			continue;
		}
		const int iValue = getHealUnitCombatTypeTotal(it->first);
		if (iValue > iBestValue)
		{
			iBestValue = iValue;
			eBestUnitCombat = it->first;
		}
	}
	return eBestUnitCombat;
}

int CvUnit::getSubUnitCount() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) && groupRank() > 0)
	{
		return groupRank();
	}
	return m_pUnitInfo->getSizeMatters().groupSize;
}


int CvUnit::getSubUnitsAlive() const
{
	return getSubUnitsAlive(getDamage());
}


int CvUnit::getSubUnitsAlive(int iDamage) const
{
	const int iMaxHP = getMaxHP();
	if (iDamage >= iMaxHP)
	{
		return 0;
	}
	return std::max(1, (getSubUnitCount() * (iMaxHP - iDamage) + iMaxHP / (2*getSubUnitCount() + 1)) / iMaxHP);
}
// returns true if unit can initiate a war action with plot (possibly by declaring war)
bool CvUnit::potentialWarAction(const CvPlot* pPlot) const
{
	TeamTypes ePlotTeam = pPlot->getTeam();

	if (ePlotTeam == NO_TEAM)
	{
		return false;
	}

	if (isEnemy(ePlotTeam, pPlot))
	{
		return true;
	}

	if (getGroup()->AI_isDeclareWar(pPlot) && GET_TEAM(getTeam()).AI_getWarPlan(ePlotTeam) != NO_WARPLAN)
	{
		return true;
	}

	return false;
}

void CvUnit::read(FDataStreamBase* pStream)
{
	PROFILE_EXTRA_FUNC();
	// Init data before load
	reset();

	CvTaggedSaveFormatWrapper&	wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_READ_OBJECT_START(wrapper);

	WRAPPER_READ(wrapper, "CvUnit", &m_iID);
	WRAPPER_READ(wrapper, "CvUnit", &m_iGroupID);
	WRAPPER_READ(wrapper, "CvUnit", &m_iHotKeyNumber);
	WRAPPER_READ(wrapper, "CvUnit", &m_iX);
	WRAPPER_READ(wrapper, "CvUnit", &m_iY);
	WRAPPER_READ(wrapper, "CvUnit", &m_iLastMoveTurn);
	WRAPPER_READ(wrapper, "CvUnit", &m_iReconX);
	WRAPPER_READ(wrapper, "CvUnit", &m_iReconY);
	WRAPPER_READ(wrapper, "CvUnit", &m_iGameTurnCreated);
	WRAPPER_READ(wrapper, "CvUnit", &m_iDamage);
	WRAPPER_READ(wrapper, "CvUnit", &m_iMoves);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExperience);
	WRAPPER_READ(wrapper, "CvUnit", &m_iLevel);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCargo);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCargoCapacity);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAttackPlotX);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAttackPlotY);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCombatTimer);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCombatFirstStrikes);
	WRAPPER_READ(wrapper, "CvUnit", &m_iFortifyTurns);
	WRAPPER_READ(wrapper, "CvUnit", &m_iBlitzCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAmphibCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iRiverCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iEnemyRouteCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAlwaysHealCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iHillsDoubleMoveCount);

	WRAPPER_READ(wrapper, "CvUnit", &m_iCanMovePeaksCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCanLeadThroughPeaksCount);

	WRAPPER_READ(wrapper, "CvUnit", &m_iSleepTimer);

	WRAPPER_READ(wrapper, "CvUnit", &m_iCommanderID);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCommodoreID);

	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_eOriginalOwner);

	WRAPPER_READ(wrapper, "CvUnit", &m_bAutoPromoting);
	WRAPPER_READ(wrapper, "CvUnit", &m_bAutoUpgrading);

	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_shadowUnit.eOwner);
	WRAPPER_READ(wrapper, "CvUnit", &m_shadowUnit.iID);

	WRAPPER_READ(wrapper, "CvUnit", &m_iImmuneToFirstStrikesCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraMoves);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraMoveDiscount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraBombardRate);
	WRAPPER_READ(wrapper, "CvUnit", &m_iRevoltProtection);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCollateralDamageProtection);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageChange);
	WRAPPER_READ(wrapper, "CvUnit", &m_iUpgradeDiscount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExperiencePercent);
	WRAPPER_READ(wrapper, "CvUnit", &m_iKamikazePercent);
	// ⛔ NEW TAG: the member changed MEANING (human -> ×100), which the save format cannot express on the old
	// tag -- an old save would silently load 1/100th strength (save.md: the silent-wrong-load class). The old
	// tag is CUT in savemigration.txt; when it is absent the sentinel below re-seeds from the unit's type, so a
	// pre-existing save keeps correct strength and loses only a WorldBuilder per-unit override.
	WRAPPER_READ(wrapper, "CvUnit", &m_iBaseCombat100);
	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_eFacingDirection);

	WRAPPER_READ(wrapper, "CvUnit", &m_bMadeAttack);
	WRAPPER_READ(wrapper, "CvUnit", &m_bMadeInterception);
	WRAPPER_READ(wrapper, "CvUnit", &m_bPromotionReady);
	WRAPPER_READ(wrapper, "CvUnit", &m_bDeathDelay);
	WRAPPER_SKIP_ELEMENT(wrapper,"CvUnit", m_bCombatFocus, SAVE_VALUE_ANY);
	// m_bInfoBarDirty not saved...
	WRAPPER_READ(wrapper, "CvUnit", &m_bBlockading);
	WRAPPER_READ(wrapper, "CvUnit", &m_bAirCombat);

	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_eOwner);
	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_eCapturingPlayer);
	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_UNITS, (int*)&m_eUnitType);
	bool bKill = false;
	if (NO_UNIT == m_eUnitType)
	{
		// Assets must have removed this type (which will have been flagged in a queued error message).
		// Just give it a valid type and mark it to be killed.
		m_eUnitType = (UnitTypes)0;
		// Unit type 0 was never initialized, so we need to add its unit count before it dies.
		GET_PLAYER(getOwner()).changeUnitCount(m_eUnitType, 1);
		if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS)
		// if unit doesn't have a group rank, it doesn't count as a SM unit at all
		&& GC.getUnitInfo(m_eUnitType).getBaseGroupRank() > 0)
		{
			GET_PLAYER(getOwner()).changeUnitCountSM(m_eUnitType, smGroupMultiplier(GC.getUnitInfo(m_eUnitType).getBaseGroupRank()));
		}
		bKill = true;
	}
	m_pUnitInfo = &GC.getUnitInfo(m_eUnitType);
	// The save carried no base-strength value (a pre-×100 save): seed it from the type, exactly as reset() does
	// for a newly created unit. 0 is a legitimate strength, which is why the unset marker is -1.
	if (m_iBaseCombat100 < 0)
	{
		m_iBaseCombat100 = m_pUnitInfo->getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT);
	}
	m_movementCharacteristicsHash = m_pUnitInfo->getZobristValue();
	// THE RESEED EMIT (DEC-spine-reseed): the unit INSTANCE fact. A loaded unit never runs init(), so without this
	// the stream shows an empire whose units all predate the save. Emitted HERE, the first point m_iID / m_eOwner /
	// m_eUnitType have all deserialized. Result-producers suppress inside the load bracket, so this restores the
	// instance without re-granting anything -- the emitCityBuildingAdded(bFirst = false) contract.
	emitUnitCreated((int)m_eUnitType, m_iID, (int)m_eOwner);
	// The death SCHEDULE lands through its setter: a save can be taken with a kill already deferred. It
	// deserialized earlier, so it is taken off the member, cleared, and handed back -- a cleared schedule is
	// the reset() default and correctly announces nothing. The unit's coordinates arrive later, so the setter
	// resolves no plot here and the id is what a consumer keys on.
	const bool bLoadedDeathDelay = m_bDeathDelay;
	m_bDeathDelay = false;
	setDeathDelayInternal(bLoadedDeathDelay);

	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_UNITS, (int*)&m_eLeaderUnitType);

	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_combatUnit.eOwner);
	WRAPPER_READ(wrapper, "CvUnit", &m_combatUnit.iID);
	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_transportUnit.eOwner);
	WRAPPER_READ(wrapper, "CvUnit", &m_transportUnit.iID);

	WRAPPER_READ_ARRAY(wrapper, "CvUnit", NUM_DOMAIN_TYPES, m_aiExtraDomainModifier);
	// The statuses deserialize WHOLESALE, then LAND through setStatus: take the loaded turns, zero the slot, hand
	// it back to the one write path so the HOLDS-crossing announces. Written straight into the array they would
	// announce nothing and every consumer gating on one would read a unit that is not held.
	WRAPPER_READ_ARRAY(wrapper, "CvUnit", NUM_UNIT_STATUSES, m_aiStatusTurns);
	for (int iStatus = 0; iStatus < NUM_UNIT_STATUSES; ++iStatus)
	{
		if (m_aiStatusTurns[iStatus] > 0)
		{
			const int iLoadedStatusTurns = m_aiStatusTurns[iStatus];
			m_aiStatusTurns[iStatus] = 0;
			setStatus((UnitStatus)iStatus, iLoadedStatusTurns);
		}
	}

	WRAPPER_READ_STRING(wrapper, "CvUnit", m_szName);
	WRAPPER_READ_STRING(wrapper, "CvUnit", m_szScriptData);


	// Read compressed data format
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		g_pabTempHasPromotion[iI] = false;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasPromotion");
		if (iI != -1)
		{
			const int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_PROMOTIONS, iI, true);

			if (iNewIndex != NO_PROMOTION)
			{
				g_pabTempHasPromotion[iNewIndex] = true;
			}
		}
	} while(iI != -1);


	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if (g_pabTempHasPromotion[iI])
		{
			// Lands through the internal setter: the commit, the movement hash and the fact, from the one body
			// that owns them. ⛔ NOT processPromotion -- the stats it applies are serialized on this unit in
			// their own right (m_iExtraMoves, m_iBlitzCount, ... are read straight off the stream above), so
			// running it here would double every one.
			// ⚠ An isRemoveAfterSet promotion is the one the stream carries that is NOT restored as held: it
			// removes itself once applied, so its effect is already in the unit's serialized totals and the
			// flag correctly stays down.
			if (!GC.getPromotionInfo((PromotionTypes)iI).isRemoveAfterSet())
			{
				setHasPromotionInternal((PromotionTypes)iI, true);
			}
		}
	}

	do
	{
		iI = -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasTerrainInfo");
		if (iI != -1)
		{
			const int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_TERRAINS, iI, true);

			if (iNewIndex != NO_TERRAIN)
			{
				TerrainKeyedInfo* info = findOrCreateTerrainKeyedInfo((TerrainTypes)iNewIndex);

				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iTerrainDoubleMoveCount, "TerrainDoubleMove");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iExtraTerrainAttackPercent, "extraAttackPercent");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iExtraTerrainDefensePercent, "extraDefensePercent");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iExtraTerrainWorkPercent, "terrainExtraWorkPercent");
			}
		}
	} while(iI != -1);

	do
	{
		iI = -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasFeatureInfo");
		if (iI != -1)
		{
			const int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_FEATURES, iI, true);

			if (iNewIndex != NO_FEATURE)
			{
				FeatureKeyedInfo* info = findOrCreateFeatureKeyedInfo((FeatureTypes)iNewIndex);

				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iFeatureDoubleMoveCount, "FeatureDoubleMove");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iExtraFeatureAttackPercent, "extraAttackPercent");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iExtraFeatureDefensePercent, "extraDefensePercent");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &info->m_iExtraFeatureWorkPercent, "featureExtraWorkPercent");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		g_paiTempExtraUnitCombatModifier[iI] = 0;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasUnitCombatInfo");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_COMBATINFOS, iI, true);

			if ( iNewIndex != NO_UNITCOMBAT )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempExtraUnitCombatModifier[iNewIndex], "ExtraUnitCombatMod");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if ( g_paiTempExtraUnitCombatModifier[iI] != 0 )
		{
			UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo((UnitCombatTypes)iI);

			info->m_iExtraUnitCombatModifier = g_paiTempExtraUnitCombatModifier[iI];
		}
	}

	m_Properties.readWrapper(pStream);

	//TB Combat Mods Begin  TB SubCombat Mods Begin
	WRAPPER_READ(wrapper, "CvUnit", &m_iStampedeCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iOnslaughtCount);

	// Read compressed data format
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		g_paiTempPromotionFromTraitCount[iI] = 0;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasAfflicationInfo");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_PROMOTIONS, iI, true);

			if ( iNewIndex != NO_PROMOTION )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempPromotionFromTraitCount[iNewIndex], "promotionFromTraitCount");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if (0 != g_paiTempPromotionFromTraitCount[iI])
		{
			PromotionKeyedInfo* info = findOrCreatePromotionKeyedInfo((PromotionTypes)iI);

			info->m_iPromotionFromTraitCount = g_paiTempPromotionFromTraitCount[iI];
		}
	}

	WRAPPER_READ(wrapper, "CvUnit", &m_iRoundCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAttackCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iDefenseCount);


	// Read compressed data format
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		g_paiTempPromotionFreeCount[iI] = 0;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasFreePromotionCount");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_PROMOTIONS, iI, true);

			if ( iNewIndex != NO_PROMOTION )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempPromotionFreeCount[iNewIndex], "FreePromoCount");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if ( g_paiTempPromotionFreeCount[iI] != 0 )
		{
			PromotionKeyedInfo* info = findOrCreatePromotionKeyedInfo((PromotionTypes)iI);

			info->m_iPromotionFreeCount = g_paiTempPromotionFreeCount[iI];
		}
	}
	// Read compressed data format
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		g_paiTempExtraFlankingStrengthbyUnitCombatType[iI] = 0;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasUnitCombatInfo5");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_COMBATINFOS, iI, true);

			if ( iNewIndex != NO_UNITCOMBAT )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempExtraFlankingStrengthbyUnitCombatType[iNewIndex], "extraFlankingStrengthbyUnitCombatType");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if ( g_paiTempExtraFlankingStrengthbyUnitCombatType[iI] != 0 )
		{
			UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo((UnitCombatTypes)iI);

			info->m_iExtraFlankingStrengthbyUnitCombatType = g_paiTempExtraFlankingStrengthbyUnitCombatType[iI];
		}
	}


	// Read compressed data format
	for (int iI = 0; iI < GC.getNumPromotionLineInfos(); iI++)
	{
		g_pabTempValidBuildUp[iI] = false;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasAfflictOnAttackInfo");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_PROMOTIONLINES, iI, true);

			if ( iNewIndex != NO_PROMOTIONLINE )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_pabTempValidBuildUp[iNewIndex], "validBuildUp");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumPromotionLineInfos(); iI++)
	{
		bool	bNonDefaultValue =
			g_pabTempValidBuildUp[iI]
		;

		if ( bNonDefaultValue )
		{
			PromotionLineKeyedInfo* info = findOrCreatePromotionLineKeyedInfo((PromotionLineTypes)iI);

			info->m_bValidBuildUp = g_pabTempValidBuildUp[iI];
		}
	}
	WRAPPER_READ(wrapper, "CvUnit", &m_iRetrainsAvailable);
	//TB Combat Mods End  TB SubCombat Mods End

	WRAPPER_READ(wrapper, "CvUnit", &m_iDefensiveVictoryMoveCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iFreeDropCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iOffensiveVictoryMoveCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageCultureCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageEspionageCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageMarauderCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageOnMoveCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageOnVictoryCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPillageResearchCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCelebrityHappy);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCollateralDamageLimitChange);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCollateralDamageMaxUnitsChange);
	WRAPPER_READ(wrapper, "CvUnit", &m_iCombatLimitChange);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraDropRange);
	WRAPPER_READ(wrapper, "CvUnit", &m_iOneUpCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSurvivorChance);
	WRAPPER_READ(wrapper, "CvUnit", &m_bSurvivor);

	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraBreakdownChance);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraBreakdownDamage);

	// Read compressed data format
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		g_pabTempHasUnitCombat[iI] = false;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "hasUnitCombat");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_COMBATINFOS, iI, true);

			if ( iNewIndex != NO_UNITCOMBAT )
			{
				g_pabTempHasUnitCombat[iNewIndex] = true;
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (g_pabTempHasUnitCombat[iI])
		{
			// Lands through the internal setter: the commit, the movement hash and the fact, from the one body
			// that owns them. ⛔ NOT processUnitCombat -- the stats it applies are serialized on this unit in
			// their own right, so running it here would double every one of them.
			setHasUnitCombatInternal((UnitCombatTypes)iI, true);
		}
	}

	WRAPPER_READ(wrapper, "CvUnit", &m_iAttackOnlyCitiesCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iIgnoreNoEntryLevelCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iIgnoreZoneofControlCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraMaxHP);

	WRAPPER_READ(wrapper, "CvUnit", &m_iFliesToMoveCount);
	// The three SM base totals are DERIVED, so they are not read from the save ([DEC-derived-never-trusted]).
	// Each is a pure copy of this unit TYPE's load-derived rank (the info's Sigma over its combat classes,
	// json.md par.9) whose only writer is the assignment in processUnitCombat -- and that never runs on a load,
	// because the combat-class set is written straight into the keyed map above. Trusting the stored copy meant
	// a save pinned the ranks forever: re-curate a unitcombat *Base and every existing save kept the STALE
	// value, silently mis-dividing the SM unit count (smGroupMultiplier) for every unit of that type. Re-derived
	// here instead; the runtime deltas that ride ON TOP (m_iExtraQuality/Group/Size) stay serialized, being
	// genuine per-unit state.
	m_iQualityBaseTotal = m_pUnitInfo->getBaseQualityRank();
	m_iGroupBaseTotal = m_pUnitInfo->getBaseGroupRank();
	m_iSizeBaseTotal = m_pUnitInfo->getBaseSizeRank();
	WRAPPER_READ(wrapper, "CvUnit", &m_iCannotMergeSplitCount);

	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_UNITS, (int*)&m_eGGExperienceEarnedTowardsType);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMCargo);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMCargoCapacity);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMCargoVolume);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMCargoVolumeModifier);
	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_eNewDomainCargo);

	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_SPECIAL_UNITS, (int*)&m_eNewSpecialCargo);
	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_SPECIAL_UNITS, (int*)&m_eNewSMNotSpecialCargo);

	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraQuality);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraGroup);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraSize);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMStrength);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMAssetValue);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMPowerValue);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMHPValue);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMExtraCargoVolume);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMBombardRate);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMAirBombBaseRate);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMBaseWorkRate);
	WRAPPER_READ(wrapper, "CvUnit", &m_iSMRevoltProtection);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraCombatModifierPerSizeMore);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraCombatModifierPerSizeLess);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraCombatModifierPerVolumeMore);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraCombatModifierPerVolumeLess);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAlwaysInvisibleCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iHealUnitCombatCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iNoSelfHealCount);

	// Read compressed data format
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		g_paiTempHealAsDamage[iI] = 0;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "healAsDamageInfo");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_COMBATINFOS, iI, true);

			if ( iNewIndex != NO_UNITCOMBAT )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempHealAsDamage[iNewIndex], "healAsDamage");
			}
		}
	} while(iI != -1);

	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if ( g_paiTempHealAsDamage[iI] != 0 )
		{
			UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo((UnitCombatTypes)iI);

			info->m_iHealAsDamage = g_paiTempHealAsDamage[iI];
		}
	}

	WRAPPER_READ(wrapper, "CvUnit", &m_iHealSupportUsed);
	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_MISSIONS, (int*)&m_eSleepType);
	WRAPPER_READ(wrapper, "CvUnit", &m_bHasBuildUp);
	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_PROMOTIONLINES, (int*)&m_eCurrentBuildUpType);
	WRAPPER_READ(wrapper, "CvUnit", &m_iZoneOfControlCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_bInhibitMerge);
	WRAPPER_READ(wrapper, "CvUnit", &m_bInhibitSplit);
	WRAPPER_READ(wrapper, "CvUnit", &m_bIsBuildUp);
	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_SPECIAL_UNITS, (int*)&m_eSpecialUnit);
	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_eCapturingUnit.eOwner);
	WRAPPER_READ(wrapper, "CvUnit", &m_eCapturingUnit.iID);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExcileCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iPassageCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iNoNonOwnedCityEntryCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iBarbCoExistCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iBlendIntoCityCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iUpgradeAnywhereCount);

	// Read compressed data format
	for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
	{
		m_aiExtraVisibilityIntensity[iI] = 0;
		m_aiExtraInvisibilityIntensity[iI] = 0;
		m_aiExtraVisibilityIntensityRange[iI] = 0;
		m_aiNegatesInvisibleCount[iI] = 0;
		m_aiExtraVisibilityIntensitySameTile[iI] = 0;
	}
	do
	{
		iI= -1;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iI, "Visibilities");
		if ( iI != -1 )
		{
			int iNewIndex = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_INVISIBLES, iI, true);

			if ( iNewIndex != NO_UNITCOMBAT )
			{
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &m_aiExtraVisibilityIntensity[iNewIndex], "extraVisibilityIntensity");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &m_aiExtraInvisibilityIntensity[iNewIndex], "extraInvisibilityIntensity");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &m_aiExtraVisibilityIntensityRange[iNewIndex], "extraVisibilityIntensityRange");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &m_aiNegatesInvisibleCount[iNewIndex], "negatesInvisibleCount");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &m_aiExtraVisibilityIntensitySameTile[iNewIndex], "extraVisibilityIntensitySameTile");
			}
		}
	} while(iI != -1);

	int iType1 = 0;
	int iType2 = 0;
	int iType3 = 0;

	int iSize1 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize1, "m_aExtraInvisibleTerrains.size");
	m_aExtraInvisibleTerrains.resize(iSize1);
	if (iSize1 > 0)
	{
		for (int iI = 0; iI < iSize1; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraInvisibleTerrains[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraInvisibleTerrains[iI].eTerrain = (TerrainTypes)iType2;
			m_aExtraInvisibleTerrains[iI].iIntensity = iType3;
		}
	}

	int iSize2 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize2, "m_aExtraInvisibleFeatures.size");
	m_aExtraInvisibleFeatures.resize(iSize2);
	if (iSize2 > 0)
	{
		for (int iI = 0; iI < iSize2; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraInvisibleFeatures[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraInvisibleFeatures[iI].eFeature = (FeatureTypes)iType2;
			m_aExtraInvisibleFeatures[iI].iIntensity = iType3;
		}
	}

	int iSize3 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize3, "m_aExtraInvisibleImprovements.size");
	m_aExtraInvisibleImprovements.resize(iSize3);
	if (iSize3 > 0)
	{
		for (int iI = 0; iI < iSize3; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraInvisibleImprovements[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraInvisibleImprovements[iI].eImprovement = (ImprovementTypes)iType2;
			m_aExtraInvisibleImprovements[iI].iIntensity = iType3;
		}
	}

	int iSize4 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize4, "m_aExtraVisibleTerrains.size");
	m_aExtraVisibleTerrains.resize(iSize4);
	if (iSize4 > 0)
	{
		for (int iI = 0; iI < iSize4; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraVisibleTerrains[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraVisibleTerrains[iI].eTerrain = (TerrainTypes)iType2;
			m_aExtraVisibleTerrains[iI].iIntensity = iType3;
		}
	}

	int iSize5 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize5, "m_aExtraVisibleFeatures.size");
	m_aExtraVisibleFeatures.resize(iSize5);
	if (iSize5 > 0)
	{
		for (int iI = 0; iI < iSize5; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraVisibleFeatures[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraVisibleFeatures[iI].eFeature = (FeatureTypes)iType2;
			m_aExtraVisibleFeatures[iI].iIntensity = iType3;
		}
	}

	int iSize6 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize6, "m_aExtraVisibleImprovements.size");
	m_aExtraVisibleImprovements.resize(iSize6);
	if (iSize6 > 0)
	{
		for (int iI = 0; iI < iSize6; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraVisibleImprovements[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraVisibleImprovements[iI].eImprovement = (ImprovementTypes)iType2;
			m_aExtraVisibleImprovements[iI].iIntensity = iType3;
		}
	}

	int iSize7 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize7, "m_aExtraVisibleTerrainRanges.size");
	m_aExtraVisibleTerrainRanges.resize(iSize7);
	if (iSize7 > 0)
	{
		for (int iI = 0; iI < iSize7; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraVisibleTerrainRanges[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraVisibleTerrainRanges[iI].eTerrain = (TerrainTypes)iType2;
			m_aExtraVisibleTerrainRanges[iI].iIntensity = iType3;
		}
	}

	int iSize8 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize8, "m_aExtraVisibleFeatureRanges.size");
	m_aExtraVisibleFeatureRanges.resize(iSize8);
	if (iSize8 > 0)
	{
		for (int iI = 0; iI < iSize8; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraVisibleFeatureRanges[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraVisibleFeatureRanges[iI].eFeature = (FeatureTypes)iType2;
			m_aExtraVisibleFeatureRanges[iI].iIntensity = iType3;
		}
	}

	int iSize9 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize9, "m_aExtraVisibleImprovementRanges.size");
	m_aExtraVisibleImprovementRanges.resize(iSize9);
	if (iSize9 > 0)
	{
		for (int iI = 0; iI < iSize9; iI++)
		{
			iType1 = 0;
			iType2 = 0;
			iType3 = 0;
			WRAPPER_READ(wrapper, "CvUnit", &iType1);
			WRAPPER_READ(wrapper, "CvUnit", &iType2);
			WRAPPER_READ(wrapper, "CvUnit", &iType3);
			m_aExtraVisibleImprovementRanges[iI].eInvisible = (InvisibleTypes)iType1;
			m_aExtraVisibleImprovementRanges[iI].eImprovement = (ImprovementTypes)iType2;
			m_aExtraVisibleImprovementRanges[iI].iIntensity = iType3;
		}
	}
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraInsidiousness);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraInvestigation);
	WRAPPER_READ(wrapper, "CvUnit", (int*)&m_pPlayerInvestigated);
	WRAPPER_READ(wrapper, "CvUnit", &m_iAssassinCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iStealthDefenseCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_bRevealed);
	WRAPPER_READ(wrapper, "CvUnit", &m_iOnlyDefensiveCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iNoInvisibilityCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_bIsArmed);
	WRAPPER_READ(wrapper, "CvUnit", &m_iHiddenNationalityCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iNoCaptureCount);
	int iSize10 = 0;
	WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize10, "m_aExtraAidChanges.size");
	if (iSize10 > 0)
	{
		for (int iI = 0; iI < iSize10; iI++)
		{
			iType1 = -1;
			iType2 = 0;
			WRAPPER_READ_CLASS_ENUM_DECORATED_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_PROPERTIES, &iType1, "AidChange.eProperty");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iType2, "AidChange.iChange");

			if (iType1 != -1)
			{

				AidStruct AidChange;
				AidChange.eProperty = (PropertyTypes)iType1;
				AidChange.iChange = iType2;

				m_aExtraAidChanges.push_back(AidChange);
			}
		}
	}

	WRAPPER_READ(wrapper, "CvUnit", &m_iXOrigin);
	WRAPPER_READ(wrapper, "CvUnit", &m_iYOrigin);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraNoDefensiveBonusCount);
	WRAPPER_READ(wrapper, "CvUnit", &m_iExtraGatherHerdCount);
	WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_RELIGIONS, (int*)&m_eReligionType);
	WRAPPER_READ(wrapper, "CvUnit", &m_bIsReligionLocked);

	WRAPPER_READ(wrapper, "CvUnit", &m_iBuildUpTurns);

	for (int iI = GC.getNumUnitCombatInfos() - 1; iI > -1; iI--)
	{
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempHealUnitCombatTypeVolume[iI], "healUnitCombatTypeVolume");
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &g_paiTempHealUnitCombatTypeAdjacentVolume[iI], "healUnitCombatTypeAdjacentVolume");

		if (g_paiTempHealUnitCombatTypeVolume[iI] != 0
		||  g_paiTempHealUnitCombatTypeAdjacentVolume[iI] != 0)
		{
			UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo((UnitCombatTypes)iI);

			info->m_iHealUnitCombatTypeVolume = g_paiTempHealUnitCombatTypeVolume[iI];
			info->m_iHealUnitCombatTypeAdjacentVolume = g_paiTempHealUnitCombatTypeAdjacentVolume[iI];
		}
	}
	{
		bool bCommander = false;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &bCommander, "m_bCommander");

		if (bCommander)
		{
			int iExtraControlPoints = 0;
			int iExtraCommandRange = 0;
			short iControlPointsLeft = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iExtraControlPoints, "m_iExtraControlPoints");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iExtraCommandRange, "m_iExtraCommandRange");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iControlPointsLeft, "m_iControlPointsLeft");

			m_commander = (
				new UnitCompCommander(
					this,
					m_pUnitInfo->getControlPoints() + iExtraControlPoints,
					iControlPointsLeft,
					m_pUnitInfo->getCommandRange() + iExtraCommandRange
				)
			);
		}
	}
	{
		bool bCommodore = false;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &bCommodore, "m_bCommodore");

		if (bCommodore)
		{
			int iExtraControlPoints = 0;
			int iExtraCommandRange = 0;
			short iControlPointsLeft = 0;
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iExtraControlPoints, "m_iExtraControlPoints");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iExtraCommandRange, "m_iExtraCommandRange");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iControlPointsLeft, "m_iControlPointsLeft");

			m_commodore = (
				new UnitCompCommodore(
					this,
					m_pUnitInfo->getControlPoints() + iExtraControlPoints,
					iControlPointsLeft,
					m_pUnitInfo->getCommandRange() + iExtraCommandRange
				)
			);
		}
	}
	{
		bool bWorker = false;
		WRAPPER_READ_DECORATED(wrapper, "CvUnit", &bWorker, "m_worker");

		if (bWorker)
		{
			m_worker = new UnitCompWorker();
			short iExtraWorkPercent = 0;
			int iExtraHillsWorkPercent = 0;
			int iAssignedCity = -1;
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iExtraWorkPercent, "m_iExtraWorkPercent");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iExtraHillsWorkPercent, "m_iExtraHillsWorkPercent");
			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iAssignedCity, "m_iAssignedCity");

			m_worker->changeWorkModifier(iExtraWorkPercent);
			m_worker->changeHillsWorkModifier(iExtraHillsWorkPercent + m_pUnitInfo->getScalar(SCALAR_WORK_RATE_HILLS, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT));
			m_worker->setCityAssignment(iAssignedCity);

			short iSize = 0;
			int iBuild = NO_BUILD;

			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize, "ExtraBuildsSize");
			for (short i = 0; i < iSize; ++i)
			{
				WRAPPER_READ_CLASS_ENUM_DECORATED_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_BUILDS, &iBuild, "ExtraBuildType");

				if (iBuild != -1)
				{
					m_worker->setExtraBuild((BuildTypes)iBuild);
				}
			}

			WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iSize, "ExtraWorkModForBuildsSize");
			while (iSize-- > 0)
			{
				short iMod = 0;
				WRAPPER_READ_CLASS_ENUM_DECORATED_ALLOW_MISSING(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_BUILDS, &iBuild, "ExtraWorkModForBuildType");
				WRAPPER_READ_DECORATED(wrapper, "CvUnit", &iMod, "ExtraWorkModForBuild");

				if (iBuild != NO_BUILD)
				{
					m_worker->changeExtraWorkModForBuild((BuildTypes)iBuild, iMod);
				}
			}
		}
	}

	//Example of how to skip an outdated and unnecessary save element (at least for ints and bools)
	/*WRAPPER_SKIP_ELEMENT(wrapper,"CvUnit", m_bHiddenNationality, SAVE_VALUE_ANY);*/
	WRAPPER_READ_OBJECT_END(wrapper);


	// Post Process
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		setSMValues(true);
	}
	setHasAnyInvisibility();
	establishBuildups();
	if (bKill)
	{
		kill(false);
		FErrorMsg("Unit Asset removed, killing unit.");
	}
}


void CvUnit::write(FDataStreamBase* pStream)
{
	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper&	wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_WRITE_OBJECT_START(wrapper);


	WRAPPER_WRITE(wrapper, "CvUnit", m_iID);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iGroupID);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iHotKeyNumber);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iX);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iY);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iLastMoveTurn);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iReconX);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iReconY);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iGameTurnCreated);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iDamage);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iMoves);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExperience);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iLevel);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCargo);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCargoCapacity);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAttackPlotX);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAttackPlotY);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCombatTimer);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCombatFirstStrikes);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iFortifyTurns);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iBlitzCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAmphibCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iRiverCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iEnemyRouteCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAlwaysHealCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iHillsDoubleMoveCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCanMovePeaksCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCanLeadThroughPeaksCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSleepTimer);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCommanderID);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCommodoreID);

	WRAPPER_WRITE(wrapper, "CvUnit", m_eOriginalOwner);

	WRAPPER_WRITE(wrapper, "CvUnit", m_bAutoPromoting);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bAutoUpgrading);

	WRAPPER_WRITE(wrapper, "CvUnit", m_shadowUnit.eOwner);
	WRAPPER_WRITE(wrapper, "CvUnit", m_shadowUnit.iID);

	WRAPPER_WRITE(wrapper, "CvUnit", m_iImmuneToFirstStrikesCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraMoves);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraMoveDiscount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraBombardRate);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iRevoltProtection);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCollateralDamageProtection);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageChange);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iUpgradeDiscount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExperiencePercent);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iKamikazePercent);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iBaseCombat100);
	WRAPPER_WRITE(wrapper, "CvUnit", m_eFacingDirection);

	WRAPPER_WRITE(wrapper, "CvUnit", m_bMadeAttack);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bMadeInterception);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bPromotionReady);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bDeathDelay);
	// m_bInfoBarDirty not saved...
	WRAPPER_WRITE(wrapper, "CvUnit", m_bBlockading);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bAirCombat);

	WRAPPER_WRITE(wrapper, "CvUnit", m_eOwner);
	WRAPPER_WRITE(wrapper, "CvUnit", m_eCapturingPlayer);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_UNITS, m_eUnitType);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_UNITS, m_eLeaderUnitType);

	WRAPPER_WRITE(wrapper, "CvUnit", m_combatUnit.eOwner);
	WRAPPER_WRITE(wrapper, "CvUnit", m_combatUnit.iID);
	WRAPPER_WRITE(wrapper, "CvUnit", m_transportUnit.eOwner);
	WRAPPER_WRITE(wrapper, "CvUnit", m_transportUnit.iID);

	WRAPPER_WRITE_ARRAY(wrapper, "CvUnit", NUM_DOMAIN_TYPES, m_aiExtraDomainModifier);
	WRAPPER_WRITE_ARRAY(wrapper, "CvUnit", NUM_UNIT_STATUSES, m_aiStatusTurns);

	WRAPPER_WRITE_STRING(wrapper, "CvUnit", m_szName);
	WRAPPER_WRITE_STRING(wrapper, "CvUnit", m_szScriptData);

	//	Use condensed format now - only save non-default array elements
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if ( isHasPromotion((PromotionTypes)iI) )
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "hasPromotion");
		}
	}

	for (std::map<TerrainTypes, TerrainKeyedInfo>::iterator it = m_terrainKeyedInfo.begin(), itEnd = m_terrainKeyedInfo.end(); it != itEnd; ++it)
	{
		const TerrainKeyedInfo& info = it->second;
		if (!info.Empty())
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", it->first, "hasTerrainInfo");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iTerrainDoubleMoveCount, "TerrainDoubleMove");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iExtraTerrainAttackPercent, "extraAttackPercent");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iExtraTerrainDefensePercent, "extraDefensePercent");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iExtraTerrainWorkPercent, "terrainExtraWorkPercent");
		}
	}
	for (std::map<FeatureTypes, FeatureKeyedInfo>::iterator it = m_featureKeyedInfo.begin(), itEnd = m_featureKeyedInfo.end(); it != itEnd; ++it)
	{
		const FeatureKeyedInfo& info = it->second;
		if (!info.Empty())
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", it->first, "hasFeatureInfo");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iFeatureDoubleMoveCount, "FeatureDoubleMove");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iExtraFeatureAttackPercent, "extraAttackPercent");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iExtraFeatureDefensePercent, "extraDefensePercent");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info.m_iExtraFeatureWorkPercent, "featureExtraWorkPercent");
		}
	}
	for (iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (getExtraUnitCombatModifier((UnitCombatTypes)iI) != 0)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "hasUnitCombatInfo");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", getExtraUnitCombatModifier((UnitCombatTypes)iI), "ExtraUnitCombatMod");
		}
	}

	m_Properties.writeWrapper(pStream);

	//TB Combat Mods Begin
	WRAPPER_WRITE(wrapper, "CvUnit", m_iStampedeCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iOnslaughtCount);


	WRAPPER_WRITE(wrapper, "CvUnit", m_iRoundCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAttackCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iDefenseCount);




	//	Use condensed format now - only save non-default array elements
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if (getPromotionFreeCount((PromotionTypes)iI) != 0)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "hasFreePromotionCount");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", getPromotionFreeCount((PromotionTypes)iI), "FreePromoCount");
		}
	}

	//	Use condensed format now - only save non-default array elements
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (getExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)iI) != 0)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "hasUnitCombatInfo5");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", getExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)iI), "extraFlankingStrengthbyUnitCombatType");
		}
	}


	WRAPPER_WRITE(wrapper, "CvUnit", m_iRetrainsAvailable);
	//TB Combat Mods end

	WRAPPER_WRITE(wrapper, "CvUnit", m_iDefensiveVictoryMoveCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iFreeDropCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iOffensiveVictoryMoveCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageCultureCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageEspionageCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageMarauderCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageOnMoveCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageOnVictoryCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPillageResearchCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCelebrityHappy);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCollateralDamageLimitChange);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCollateralDamageMaxUnitsChange);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCombatLimitChange);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraDropRange);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iOneUpCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSurvivorChance);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bSurvivor);

	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraBreakdownChance);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraBreakdownDamage);

	//	Use condensed format now - only save non-default array elements
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if ( isHasUnitCombat((UnitCombatTypes)iI) )
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "hasUnitCombat");
		}
	}
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAttackOnlyCitiesCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iIgnoreNoEntryLevelCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iIgnoreZoneofControlCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraMaxHP);

	WRAPPER_WRITE(wrapper, "CvUnit", m_iFliesToMoveCount);
	// the three SM base totals are derived from the unit's info and re-derived at read -- never written
	WRAPPER_WRITE(wrapper, "CvUnit", m_iCannotMergeSplitCount);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_UNITS, m_eGGExperienceEarnedTowardsType);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMCargo);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMCargoCapacity);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMCargoVolume);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMCargoVolumeModifier);
	WRAPPER_WRITE(wrapper, "CvUnit", m_eNewDomainCargo);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_SPECIAL_UNITS, m_eNewSpecialCargo);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_SPECIAL_UNITS, m_eNewSMNotSpecialCargo);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraQuality);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraGroup);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraSize);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMStrength);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMAssetValue);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMPowerValue);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMHPValue);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMExtraCargoVolume);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMBombardRate);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMAirBombBaseRate);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMBaseWorkRate);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iSMRevoltProtection);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraCombatModifierPerSizeMore);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraCombatModifierPerSizeLess);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraCombatModifierPerVolumeMore);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraCombatModifierPerVolumeLess);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAlwaysInvisibleCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iHealUnitCombatCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iNoSelfHealCount);
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if ( getHealAsDamage((UnitCombatTypes)iI) != 0 )
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "healAsDamageInfo");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", getHealAsDamage((UnitCombatTypes)iI), "healAsDamage");
		}
	}

	WRAPPER_WRITE(wrapper, "CvUnit", m_iHealSupportUsed);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_MISSIONS, m_eSleepType);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bHasBuildUp);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_PROMOTIONLINES, m_eCurrentBuildUpType);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iZoneOfControlCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bInhibitMerge);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bInhibitSplit);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bIsBuildUp);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_SPECIAL_UNITS, m_eSpecialUnit);
	WRAPPER_WRITE(wrapper, "CvUnit", m_eCapturingUnit.eOwner);
	WRAPPER_WRITE(wrapper, "CvUnit", m_eCapturingUnit.iID);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExcileCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iPassageCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iNoNonOwnedCityEntryCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iBarbCoExistCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iBlendIntoCityCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iUpgradeAnywhereCount);
	//	Use condensed format now - only save non-default array elements
	for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
	{
		if ( m_aiExtraVisibilityIntensity[iI] != 0 ||
			m_aiExtraInvisibilityIntensity[iI] != 0 ||
			m_aiExtraVisibilityIntensityRange[iI] != 0 ||
			m_aiNegatesInvisibleCount[iI] != 0 ||
			m_aiExtraVisibilityIntensitySameTile[iI] != 0)
		{
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iI, "Visibilities");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_aiExtraVisibilityIntensity[iI], "extraVisibilityIntensity");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_aiExtraInvisibilityIntensity[iI], "extraInvisibilityIntensity");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_aiExtraVisibilityIntensityRange[iI], "extraVisibilityIntensityRange");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_aiNegatesInvisibleCount[iI], "negatesInvisibleCount");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_aiExtraVisibilityIntensitySameTile[iI], "extraVisibilityIntensitySameTile");
		}
	}

	int iType1 = 0;
	int iType2 = 0;
	int iType3 = 0;

	int iSize1 = (int)m_aExtraInvisibleTerrains.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize1, "m_aExtraInvisibleTerrains.size");
	if (iSize1 > 0)
	{
		for (iI = 0; iI < iSize1; iI++)
		{
			iType1 = (int)m_aExtraInvisibleTerrains[iI].eInvisible;
			iType2 = (int)m_aExtraInvisibleTerrains[iI].eTerrain;
			iType3 = m_aExtraInvisibleTerrains[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize2 = (int)m_aExtraInvisibleFeatures.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize2, "m_aExtraInvisibleFeatures.size");
	if (iSize2 > 0)
	{
		for (iI = 0; iI < iSize2; iI++)
		{
			iType1 = (int)m_aExtraInvisibleFeatures[iI].eInvisible;
			iType2 = (int)m_aExtraInvisibleFeatures[iI].eFeature;
			iType3 = m_aExtraInvisibleFeatures[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize3 = (int)m_aExtraInvisibleImprovements.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize3, "m_aExtraInvisibleImprovements.size");
	if (iSize3 > 0)
	{
		for (iI = 0; iI < iSize3; iI++)
		{
			iType1 = (int)m_aExtraInvisibleImprovements[iI].eInvisible;
			iType2 = (int)m_aExtraInvisibleImprovements[iI].eImprovement;
			iType3 = m_aExtraInvisibleImprovements[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize4 = (int)m_aExtraVisibleTerrains.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize4, "m_aExtraVisibleTerrains.size");
	if (iSize4 > 0)
	{
		for (iI = 0; iI < iSize4; iI++)
		{
			iType1 = (int)m_aExtraVisibleTerrains[iI].eInvisible;
			iType2 = (int)m_aExtraVisibleTerrains[iI].eTerrain;
			iType3 = m_aExtraVisibleTerrains[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize5 = (int)m_aExtraVisibleFeatures.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize5, "m_aExtraVisibleFeatures.size");
	if (iSize5 > 0)
	{
		for (iI = 0; iI < iSize5; iI++)
		{
			iType1 = (int)m_aExtraVisibleFeatures[iI].eInvisible;
			iType2 = (int)m_aExtraVisibleFeatures[iI].eFeature;
			iType3 = m_aExtraVisibleFeatures[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize6 = (int)m_aExtraVisibleImprovements.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize6, "m_aExtraVisibleImprovements.size");
	if (iSize6 > 0)
	{
		for (iI = 0; iI < iSize6; iI++)
		{
			iType1 = (int)m_aExtraVisibleImprovements[iI].eInvisible;
			iType2 = (int)m_aExtraVisibleImprovements[iI].eImprovement;
			iType3 = m_aExtraVisibleImprovements[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize7 = (int)m_aExtraVisibleTerrainRanges.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize7, "m_aExtraVisibleTerrainRanges.size");
	if (iSize7 > 0)
	{
		for (iI = 0; iI < iSize7; iI++)
		{
			iType1 = (int)m_aExtraVisibleTerrainRanges[iI].eInvisible;
			iType2 = (int)m_aExtraVisibleTerrainRanges[iI].eTerrain;
			iType3 = m_aExtraVisibleTerrainRanges[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize8 = (int)m_aExtraVisibleFeatureRanges.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize8, "m_aExtraVisibleFeatureRanges.size");
	if (iSize8 > 0)
	{
		for (iI = 0; iI < iSize8; iI++)
		{
			iType1 = (int)m_aExtraVisibleFeatureRanges[iI].eInvisible;
			iType2 = (int)m_aExtraVisibleFeatureRanges[iI].eFeature;
			iType3 = m_aExtraVisibleFeatureRanges[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	int iSize9 = (int)m_aExtraVisibleImprovementRanges.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize9, "m_aExtraVisibleImprovementRanges.size");
	if (iSize9 > 0)
	{
		for (iI = 0; iI < iSize9; iI++)
		{
			iType1 = (int)m_aExtraVisibleImprovementRanges[iI].eInvisible;
			iType2 = (int)m_aExtraVisibleImprovementRanges[iI].eImprovement;
			iType3 = m_aExtraVisibleImprovementRanges[iI].iIntensity;
			WRAPPER_WRITE(wrapper, "CvUnit", iType1);
			WRAPPER_WRITE(wrapper, "CvUnit", iType2);
			WRAPPER_WRITE(wrapper, "CvUnit", iType3);
		}
	}

	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraInsidiousness);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraInvestigation);
	WRAPPER_WRITE(wrapper, "CvUnit", (int)m_pPlayerInvestigated);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iAssassinCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iStealthDefenseCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bRevealed);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iOnlyDefensiveCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iNoInvisibilityCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bIsArmed);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iHiddenNationalityCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iNoCaptureCount);

	int iSize10 = (int)m_aExtraAidChanges.size();
	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iSize10, "m_aExtraAidChanges.size");
	if (iSize10 > 0)
	{
		for (iI = 0; iI < iSize10; iI++)
		{
			iType1 = m_aExtraAidChanges[iI].eProperty;
			iType2 = m_aExtraAidChanges[iI].iChange;
			WRAPPER_WRITE_CLASS_ENUM_DECORATED(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_PROPERTIES, iType1, "AidChange.eProperty");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", iType2, "AidChange.iChange");
		}
	}
	WRAPPER_WRITE(wrapper, "CvUnit", m_iXOrigin);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iYOrigin);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraNoDefensiveBonusCount);
	WRAPPER_WRITE(wrapper, "CvUnit", m_iExtraGatherHerdCount);
	WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_RELIGIONS, m_eReligionType);
	WRAPPER_WRITE(wrapper, "CvUnit", m_bIsReligionLocked);

	WRAPPER_WRITE(wrapper, "CvUnit", m_iBuildUpTurns);

	for (int iI = GC.getNumUnitCombatInfos() - 1; iI > -1; iI--)
	{
		const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(static_cast<UnitCombatTypes>(iI));

		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info ? info->m_iHealUnitCombatTypeVolume : 0, "healUnitCombatTypeVolume");
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", info ? info->m_iHealUnitCombatTypeAdjacentVolume : 0, "healUnitCombatTypeAdjacentVolume");
	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", isCommander(), "m_bCommander");
	if (m_commander)
	{
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_commander->getControlPoints() - m_pUnitInfo->getControlPoints(), "m_iExtraControlPoints");
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_commander->getCommandRange() - m_pUnitInfo->getCommandRange(), "m_iExtraCommandRange");
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_commander->getControlPointsLeft(), "m_iControlPointsLeft");
	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", isCommodore(), "m_bCommodore");
    	if (m_commodore)
    	{
    		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_commodore->getControlPoints() - m_pUnitInfo->getControlPoints(), "m_iExtraControlPoints");
    		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_commodore->getCommandRange() - m_pUnitInfo->getCommandRange(), "m_iExtraCommandRange");
    		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_commodore->getControlPointsLeft(), "m_iControlPointsLeft");
    	}

	WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", isWorker(), "m_worker");
	if (m_worker)
	{
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_worker->getWorkModifier(), "m_iExtraWorkPercent");
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_worker->getHillsWorkModifier() - m_pUnitInfo->getScalar(SCALAR_WORK_RATE_HILLS, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT), "m_iExtraHillsWorkPercent");
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", m_worker->getAssignedCity(), "m_iAssignedCity");

		const std::vector<BuildTypes>& extraBuilds = m_worker->getExtraBuilds();
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", (short)extraBuilds.size(), "ExtraBuildsSize");

		for (int i = extraBuilds.size() - 1; i > -1; i--)
		{
			WRAPPER_WRITE_CLASS_ENUM_DECORATED(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_BUILDS, extraBuilds[i], "ExtraBuildType");
		}

		std::map<BuildTypes, short> extraWorkModForBuilds = m_worker->getExtraWorkModForBuilds();
		WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", (short)extraWorkModForBuilds.size(), "ExtraWorkModForBuildsSize");

		for (std::map<BuildTypes, short>::const_iterator it = extraWorkModForBuilds.begin(), itEnd = extraWorkModForBuilds.end(); it != itEnd; ++it)
		{
			WRAPPER_WRITE_CLASS_ENUM_DECORATED(wrapper, "CvUnit", REMAPPED_CLASS_TYPE_BUILDS, it->first, "ExtraWorkModForBuildType");
			WRAPPER_WRITE_DECORATED(wrapper, "CvUnit", it->second, "ExtraWorkModForBuild");
		}
	}

	WRAPPER_WRITE_OBJECT_END(wrapper);
}

// Protected Functions...

bool CvUnit::canAdvance(const CvPlot* pPlot, int iThreshold) const
{
	FAssert(canFight());
	//TB Combat Mod next line
	FAssert(!isAnimal() || canAnimalIgnoresBorders() || !pPlot->isCity());
	FAssert(getDomainType() != DOMAIN_AIR);
	FAssert(getDomainType() != DOMAIN_IMMOBILE);

	if (pPlot->getNumVisiblePotentialEnemyDefenders(this) > iThreshold)
	{
		return false;
	}
	return true;
}


void CvUnit::collateralCombat(const CvPlot* pPlot, CvUnit* pSkipUnit)
{
	PROFILE_EXTRA_FUNC();
	const int iCollateralStrength = (getDomainType() == DOMAIN_AIR ? airBaseCombatStr() : baseCombatStr()) * collateralDamage() / 100;

	if (iCollateralStrength == 0)
	{
		return;
	}
	std::map<CvUnit*, int> mapUnitDamage;
	std::map<CvUnit*, int>::iterator it;

	const int iPossibleTargets = std::min((pPlot->getNumVisiblePotentialEnemyDefenders(this) - 1), collateralDamageMaxUnits());

	foreach_(CvUnit* pLoopUnit, pPlot->units())
	{
		if (pLoopUnit != pSkipUnit
		&& isEnemy(pLoopUnit->getTeam(), pPlot, pLoopUnit)
		&& !pLoopUnit->isInvisible(getTeam(), false)
		&& pLoopUnit->canDefend())
		{
			mapUnitDamage[pLoopUnit] = pLoopUnit->getHP() * (1 + GC.getGame().getSorenRandNum(10000, "Collateral Damage"));
		}
	}

	CvCity* pCity = NULL;
	if (getDomainType() == DOMAIN_AIR)
	{
		pCity = pPlot->getPlotCity();
	}
	int iDamageCount = 0;
	int iCount = 0;

	while (iCount < iPossibleTargets)
	{
		int iBestValue = 0;
		CvUnit* pBestUnit = NULL;

		for (it = mapUnitDamage.begin(); it != mapUnitDamage.end(); ++it)
		{
			if (it->second > iBestValue)
			{
				iBestValue = it->second;
				pBestUnit = it->first;
			}
		}

		if (pBestUnit == NULL)
		{
			break;
		}
		mapUnitDamage.erase(pBestUnit);
		//TB SubCombat Mod Begin
		//	collateral immunity is ONE boolean skill ([skills.md] §1: the legacy per-source keying -- siege /
		//	assault-mech / robot, all the siege variant -- collapses to a single enabler), so the per-class
		//	sweep it used to need is gone with the keying.
		const bool isCollateralImmune = pBestUnit->getUnitInfo().hasSkill(CLS_SKILL_COLLATERAL_IMMUNE);
		//TB SubCombat Mod End (with the exception of the following reference to 'isCollateralImmune'
		if (!isCollateralImmune)
		{
			const int iTheirStrength = pBestUnit->baseCombatStr();
			const int iStrengthFactor = (iCollateralStrength + iTheirStrength + 1) / 2;

			int iCollateralDamage = 100 * GC.getCOLLATERAL_COMBAT_DAMAGE();

			iCollateralDamage *= iStrengthFactor + iCollateralStrength;
			iCollateralDamage /= iStrengthFactor + iTheirStrength;

			iCollateralDamage -= std::min(100, std::max(0, pBestUnit->getCollateralDamageProtection())) * iCollateralDamage / 100;
			iCollateralDamage = std::max(0, iCollateralDamage);
			//TB Combat Mods end

			iCollateralDamage /= 100;

			iCollateralDamage = std::max(0, iCollateralDamage);

			const int iMaxDamage = std::min(collateralDamageLimit(), (collateralDamageLimit() * (iCollateralStrength + iStrengthFactor)) / (iTheirStrength + iStrengthFactor));
			const int iUnitDamage = std::max(pBestUnit->getDamage(), std::min(pBestUnit->getDamage() + iCollateralDamage, iMaxDamage));

			if (pBestUnit->getDamage() != iUnitDamage)
			{
// BUG - Combat Events - start
				int iDamageDone = iUnitDamage - pBestUnit->getDamage();
				pBestUnit->setDamage(iUnitDamage, getOwner());
				//TB Combat Mod begin
				//TB Combat Mod end
				CvEventReporter::getInstance().combatLogCollateral(this, pBestUnit, iDamageDone);
// BUG - Combat Events - end
				iDamageCount++;
			}
		}
		iCount++;
	}

	if (iDamageCount > 0)
	{
		AddDLLMessage(
			pSkipUnit->getOwner(), pSkipUnit->getDomainType() != DOMAIN_AIR, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText("TXT_KEY_MISC_YOU_SUFFER_COL_DMG", iDamageCount), "AS2D_COLLATERAL",
			MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pSkipUnit->getX(), pSkipUnit->getY(), true, true
		);
		AddDLLMessage(
			getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText("TXT_KEY_MISC_YOU_INFLICT_COL_DMG", getNameKey(), iDamageCount),
			"AS2D_COLLATERAL", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pSkipUnit->getX(), pSkipUnit->getY()
		);
	}
}


void CvUnit::flankingStrikeCombat(const CvPlot* pPlot, int iAttackerStrength, int iAttackerFirepower, int iDefenderOdds, int iDefenderDamage, CvUnit* pSkipUnit)
{
	PROFILE_EXTRA_FUNC();
	if (pSkipUnit && pPlot->isCity(true, pSkipUnit->getTeam()))
	{
		return;
	}

	std::vector< std::pair<CvUnit*, int> > listFlankedUnits;
	foreach_(CvUnit* pLoopUnit, pPlot->units())
	{
		if (pLoopUnit != pSkipUnit && !pLoopUnit->isDead() && isEnemy(pLoopUnit->getTeam(), pPlot, pLoopUnit)
		&& !pLoopUnit->isInvisible(getTeam(), false) && pLoopUnit->canDefend())
		{
			//	FLANKING is keyed by UNITCOMBAT, never by UNIT ([json.md] §6): the per-unit table encoded a
			//	balance theory that is rejected, and it left every unit nobody listed silently un-flankable.
			//	The class total below is the whole answer.
			int iFlankingStrength = 0;

			for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
			{
				if (pLoopUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					iFlankingStrength += flankingStrengthbyUnitCombatTotal((UnitCombatTypes)iI);
				}
			}

			if (iFlankingStrength > 0)
			{
				int iFlankedDefenderStrength;
				int iFlankedDefenderOdds;
				int iAttackerDamage;
				int iFlankedDefenderDamage;
				getDefenderCombatValues(*pLoopUnit, pPlot, iAttackerStrength, iAttackerFirepower, iFlankedDefenderOdds, iFlankedDefenderStrength, iAttackerDamage, iFlankedDefenderDamage, NULL, pLoopUnit);

				if (GC.getGame().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "Flanking Combat") >= iDefenderOdds)
				{
					const int iUnitDamage = std::max(pLoopUnit->getDamage(), pLoopUnit->getDamage() + iFlankingStrength * iDefenderDamage / 100);

					if (pLoopUnit->getDamage() != iUnitDamage)
					{
						listFlankedUnits.push_back(std::make_pair(pLoopUnit, iUnitDamage));
					}
				}
			}
		}
	}

	int iNumUnitsHit = std::min((int)listFlankedUnits.size(), collateralDamageMaxUnits());

	for (int i = 0; i < iNumUnitsHit; ++i)
	{
		int iIndexHit = GC.getGame().getSorenRandNum(listFlankedUnits.size(), "Pick Flanked Unit");
		CvUnit* pUnit = listFlankedUnits[iIndexHit].first;
		int iDamage = listFlankedUnits[iIndexHit].second;
// BUG - Combat Events - start
		int iDamageDone = iDamage - pUnit->getDamage();
// BUG - Combat Events - end
		pUnit->setDamage(iDamage, getOwner());
		//TB Combat Mod begin
		//TB Combat mod end
// BUG - Combat Events - start
		// The flanked unit is handed to Python BEFORE the kill below, which is an immediate kill and therefore
		// frees it. The damage is already applied at this point, so the event reports the same hit either way.
		CvEventReporter::getInstance().combatLogFlanking(this, pUnit, iDamageDone);
// BUG - Combat Events - end
		if (pUnit->isDead())
		{
			{

				CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_KILLED_UNIT_BY_FLANKING", getNameKey(), pUnit->getNameKey(), pUnit->getVisualCivAdjective(getTeam()));
				AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitVictoryScript(), MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY());
				szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_DIED_BY_FLANKING", pUnit->getNameKey(), getNameKey(), getVisualCivAdjective(pUnit->getTeam()));
				AddDLLMessage(pUnit->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
			}

			pUnit->kill(false, NO_PLAYER, true);
		}

		listFlankedUnits.erase(std::remove(listFlankedUnits.begin(), listFlankedUnits.end(), listFlankedUnits[iIndexHit]));
	}

	if (iNumUnitsHit > 0)
	{

		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_DAMAGED_UNITS_BY_FLANKING", getNameKey(), iNumUnitsHit);
		AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitVictoryScript(), MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY());

		if (NULL != pSkipUnit)
		{
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNITS_DAMAGED_BY_FLANKING", getNameKey(), iNumUnitsHit);
			AddDLLMessage(pSkipUnit->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
	}
}


// Returns true if we were intercepted...
bool CvUnit::interceptTest(const CvPlot* pPlot)
{
	if (GC.getGame().getSorenRandNum(100, "Evasion Rand") >= evasionProbability())
	{
		CvUnit* pInterceptor = bestInterceptor(pPlot);
		if (pInterceptor)
		{
			int iInterceptionOdds;
			if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_BETTER_INTERCETION))
			{
				iInterceptionOdds = interceptionChance(pPlot);
			}
			else
			{
				iInterceptionOdds = pInterceptor->currInterceptionProbability();
			}
			if (GC.getGame().getSorenRandNum(100, "Intercept Rand (Air)") < iInterceptionOdds)
			{
				fightInterceptor(pPlot, false);
				return true;
			}
		}
	}
	return false;
}


CvUnit* CvUnit::airStrikeTarget(const CvPlot* pPlot) const
{
	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), this, true);

	if (pDefender && !pDefender->isDead() && pDefender->canDefend())
	{
		return pDefender;
	}
	return NULL;
}


bool CvUnit::canAirStrike(const CvPlot* pPlot) const
{
	return (
		   getDomainType() == DOMAIN_AIR
		&& canAirAttack()
		&& pPlot != plot()
		&& pPlot->isVisible(getTeam(), false)
		&& plotDistance(getX(), getY(), pPlot->getX(), pPlot->getY()) <= airRange()
		&& airStrikeTarget(pPlot)
	);
}


bool CvUnit::airStrike(CvPlot* pPlot)//
{
	PROFILE_EXTRA_FUNC();
	if (!canAirStrike(pPlot))
	{
		return false;
	}

	if (interceptTest(pPlot))
	{
		return false;
	}

	CvUnit* pDefender = airStrikeTarget(pPlot);
	if (pDefender == NULL)
	{
		return false;
	}

	FAssert(pDefender != NULL);
	FAssert(pDefender->canDefend());

	setReconPlot(pPlot);

	CvCity* pCity = pPlot->getPlotCity();

	setMadeAttack(true);
	changeMoves(GC.getMOVE_DENOMINATOR());

	int iDamage = airCombatDamage(pDefender);

	int iUnitDamage = std::max(pDefender->getDamage(), std::min((pDefender->getDamage() + iDamage), airCombatLimit(pDefender)));

	{

		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ARE_ATTACKED_BY_AIR", pDefender->getNameKey(), getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->getMaxHP()));
		AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_AIR_ATTACK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true, true);

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ATTACK_BY_AIR", getNameKey(), pDefender->getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->getMaxHP()));
		AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_AIR_ATTACKED", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY());
	}

	collateralCombat(pPlot, pDefender);


	pDefender->setDamage(iUnitDamage, getOwner());
	//TB Combat Mod Begin
	//TB Combat mod end

	if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_IMPROVED_XP))
	{
		setExperience100(getExperience100() + 25);
	}

	return true;
}

bool CvUnit::canRangeStrike() const
{
	if (getDomainType() == DOMAIN_AIR)
	{
		return false;
	}

	if (airRange() <= 0)
	{
		return false;
	}

	if (airBaseCombatStr() <= 0)
	{
		return false;
	}

	if (!canFight())
	{
		return false;
	}

	if (isMadeAttack() && !isBlitz())
	{
		return false;
	}

	if (!canMove() && getMoves() > 0)
	{
		return false;
	}

	return true;
}

bool CvUnit::canRangeStrikeAt(const CvPlot* pPlot, int iX, int iY) const
{
	if (!canRangeStrike())
	{
		return false;
	}

	const CvPlot* pTargetPlot = GC.getMap().plot(iX, iY);

	if (NULL == pTargetPlot)
	{
		return false;
	}

	if (!pPlot->isVisible(getTeam(), false))
	{
		return false;
	}

	// Need to check target plot too
	//Fuyu: AI-controlled units can strike even when tile is invisible
	if (isHuman() && !isAutomated() && !pTargetPlot->isVisible(getTeam(), false))
	{
		return false;
	}

	if (plotDistance(pPlot->getX(), pPlot->getY(), pTargetPlot->getX(), pTargetPlot->getY()) > airRange())
	{
		return false;
	}

	const CvUnit* pDefender = airStrikeTarget(pTargetPlot);
	if (NULL == pDefender)
	{
		return false;
	}

	if (!pPlot->canSeePlot(pTargetPlot, getTeam()))
	{
		return false;
	}

	return true;
}


bool CvUnit::rangeStrike(int iX, int iY)
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pPlot = GC.getMap().plot(iX, iY);
	if (NULL == pPlot)
	{
		return false;
	}
	if (!canRangeStrikeAt(plot(), iX, iY))
	{
		return false;
	}

	CvUnit* pDefender = airStrikeTarget(pPlot);

	FAssert(pDefender != NULL);
	FAssert(pDefender->canDefend());

	if (GC.getDefineINT("RANGED_ATTACKS_USE_MOVES") == 0)
	{
		setMadeAttack(true);
	}
	changeMoves(GC.getMOVE_DENOMINATOR());

	const int iUnitDamage = std::max(
		pDefender->getDamage(),
		std::min(
			pDefender->getDamage() + rangeCombatDamage(pDefender),
			airCombatLimit(pDefender)
		)
	);

	{

		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ARE_ATTACKED_BY_AIR",
			pDefender->getNameKey(), getNameKey(), (iUnitDamage - pDefender->getDamage()) * -100 / pDefender->getMaxHP()
		);
		//red icon over attacking unit
		AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_COMBAT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), this->getX(), this->getY(), true, true);
		//white icon over defending unit
		AddDLLMessage(pDefender->getOwner(), false, 0, L"", "AS2D_COMBAT", MESSAGE_TYPE_DISPLAY_ONLY, pDefender->getButton(), GC.getCOLOR_WHITE(), pDefender->getX(), pDefender->getY(), true, true);

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ATTACK_BY_AIR", getNameKey(), pDefender->getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->getMaxHP()));
		AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_COMBAT", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY());
	}

	collateralCombat(pPlot, pDefender);

	//set damage but don't update entity damage visibility
	pDefender->setDamage(iUnitDamage, getOwner(), false);
	//TB Combat Mod begin
	//TB Combat Mod end

	// Range strike entity mission
	addMission(CvMissionDefinition(MISSION_RANGE_ATTACK, pDefender->plot(), this, pDefender));

	return true;
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::planBattle
//! \brief      Determines in general how a battle will progress.
//!
//!				Note that the outcome of the battle is not determined here. This function plans
//!				how many sub-units die and in which 'rounds' of battle.
//! \param      kBattleDefinition The battle definition, which receives the battle plan.
//! \retval     The number of game turns that the battle should be given.
//------------------------------------------------------------------------------------------------
int CvUnit::planBattle( CvBattleDefinition & kBattleDefinition ) const
{
PROFILE_EXTRA_FUNC();
#define BATTLE_TURNS_SETUP 4
/************************************************************************************************/
/* Afforess	                  Start		 6/20/11                                                */
/*                                                                                              */
/* Boost ending rounds to allow all unit animations to end                                      */
/************************************************************************************************/
/*
#define BATTLE_TURNS_ENDING 4
*/
#define BATTLE_TURNS_ENDING 6
/************************************************************************************************/
/* Afforess	                     END                                                            */
/************************************************************************************************/


#define BATTLE_TURNS_MELEE 6
#define BATTLE_TURNS_RANGED 6
#define BATTLE_TURN_RECHECK 4

	int								aiUnitsBegin[BATTLE_UNIT_COUNT];
	int								aiUnitsEnd[BATTLE_UNIT_COUNT];
	int								aiToKillMelee[BATTLE_UNIT_COUNT];
	int								aiToKillRanged[BATTLE_UNIT_COUNT];
	CvBattleRoundVector::iterator	iIterator;
	int								i, j;
	bool							bIsLoser;
	int								iRoundIndex;
	int								iTotalRounds = 0;
	int								iRoundCheck = BATTLE_TURN_RECHECK;

	// Initial conditions
	kBattleDefinition.setNumRangedRounds(0);
	kBattleDefinition.setNumMeleeRounds(0);

	int iFirstStrikesDelta = kBattleDefinition.getFirstStrikes(BATTLE_UNIT_ATTACKER) - kBattleDefinition.getFirstStrikes(BATTLE_UNIT_DEFENDER);
	if (iFirstStrikesDelta > 0) // Attacker first strikes
	{
		int iKills = computeUnitsToDie( kBattleDefinition, true, BATTLE_UNIT_DEFENDER );
		kBattleDefinition.setNumRangedRounds(std::max(iFirstStrikesDelta, iKills / iFirstStrikesDelta));
	}
	else if (iFirstStrikesDelta < 0) // Defender first strikes
	{
		int iKills = computeUnitsToDie( kBattleDefinition, true, BATTLE_UNIT_ATTACKER );
		iFirstStrikesDelta = -iFirstStrikesDelta;
		kBattleDefinition.setNumRangedRounds(std::max(iFirstStrikesDelta, iKills / iFirstStrikesDelta));
	}
	increaseBattleRounds( kBattleDefinition);


	// Keep randomizing until we get something valid
	do
	{
		iRoundCheck++;
		if ( iRoundCheck >= BATTLE_TURN_RECHECK )
		{
			increaseBattleRounds( kBattleDefinition);
			iTotalRounds = kBattleDefinition.getNumRangedRounds() + kBattleDefinition.getNumMeleeRounds();
			iRoundCheck = 0;
		}

		// Make sure to clear the battle plan, we may have to do this again if we can't find a plan that works.
		kBattleDefinition.clearBattleRounds();

		// Create the round list
		CvBattleRound kRound;
		kBattleDefinition.setBattleRound(iTotalRounds, kRound);

		// For the attacker and defender
		for ( i = 0; i < BATTLE_UNIT_COUNT; i++ )
		{
			// Gather some initial information
			BattleUnitTypes unitType = (BattleUnitTypes) i;
			aiUnitsBegin[unitType] = kBattleDefinition.getUnit(unitType)->getSubUnitsAlive(kBattleDefinition.getDamage(unitType, BATTLE_TIME_BEGIN));
			aiToKillRanged[unitType] = computeUnitsToDie( kBattleDefinition, true, unitType);
			aiToKillMelee[unitType] = computeUnitsToDie( kBattleDefinition, false, unitType);
			aiUnitsEnd[unitType] = aiUnitsBegin[unitType] - aiToKillMelee[unitType] - aiToKillRanged[unitType];

			// Make sure that if they aren't dead at the end, they have at least one unit left
			if ( aiUnitsEnd[unitType] == 0 && !kBattleDefinition.getUnit(unitType)->isDead() )
			{
				aiUnitsEnd[unitType]++;
				if ( aiToKillMelee[unitType] > 0 )
				{
					aiToKillMelee[unitType]--;
				}
				else
				{
					aiToKillRanged[unitType]--;
				}
			}

			// If one unit is the loser, make sure that at least one of their units dies in the last round
			if ( aiUnitsEnd[unitType] == 0 )
			{
				kBattleDefinition.getBattleRound(iTotalRounds - 1).addNumKilled(unitType, 1);
				if ( aiToKillMelee[unitType] > 0)
				{
					aiToKillMelee[unitType]--;
				}
				else
				{
					aiToKillRanged[unitType]--;
				}
			}

			// Randomize in which round each death occurs
			bIsLoser = aiUnitsEnd[unitType] == 0;

			// Randomize the ranged deaths
			for ( j = 0; j < aiToKillRanged[unitType]; j++ )
			{
				iRoundIndex = GC.getGame().getSorenRandNum( range( kBattleDefinition.getNumRangedRounds(), 0, kBattleDefinition.getNumRangedRounds()), "Ranged combat death");
				kBattleDefinition.getBattleRound(iRoundIndex).addNumKilled(unitType, 1);
			}

			// Randomize the melee deaths
			for ( j = 0; j < aiToKillMelee[unitType]; j++ )
			{
				iRoundIndex = GC.getGame().getSorenRandNum( range( kBattleDefinition.getNumMeleeRounds() - (bIsLoser ? 1 : 2 ), 0, kBattleDefinition.getNumMeleeRounds()), "Melee combat death");
				kBattleDefinition.getBattleRound(kBattleDefinition.getNumRangedRounds() + iRoundIndex).addNumKilled(unitType, 1);
			}

			// Compute alive sums
			int iNumberKilled = 0;
			for(int j=0;j<kBattleDefinition.getNumBattleRounds();j++)
			{
				CvBattleRound &round = kBattleDefinition.getBattleRound(j);
				round.setRangedRound(j < kBattleDefinition.getNumRangedRounds());
				iNumberKilled += round.getNumKilled(unitType);
				round.setNumAlive(unitType, aiUnitsBegin[unitType] - iNumberKilled);
			}
		}

		// Now compute wave sizes
		for(int i=0;i<kBattleDefinition.getNumBattleRounds();i++)
		{
			CvBattleRound &round = kBattleDefinition.getBattleRound(i);
			round.setWaveSize(computeWaveSize(round.isRangedRound(), round.getNumAlive(BATTLE_UNIT_ATTACKER) + round.getNumKilled(BATTLE_UNIT_ATTACKER), round.getNumAlive(BATTLE_UNIT_DEFENDER) + round.getNumKilled(BATTLE_UNIT_DEFENDER)));
		}

		if ( iTotalRounds > 400 )
		{
			kBattleDefinition.setNumMeleeRounds(1);
			kBattleDefinition.setNumRangedRounds(0);
			break;
		}
	}
	while ( !verifyRoundsValid( kBattleDefinition ));

	//add a little extra time for leader to surrender
	bool attackerLeader = false;
	bool defenderLeader = false;
	bool attackerDie = false;
	bool defenderDie = false;
	int lastRound = kBattleDefinition.getNumBattleRounds() - 1;
	if(kBattleDefinition.getUnit(BATTLE_UNIT_ATTACKER)->getLeaderUnitType() != NO_UNIT)
		attackerLeader = true;
	if(kBattleDefinition.getUnit(BATTLE_UNIT_DEFENDER)->getLeaderUnitType() != NO_UNIT)
		defenderLeader = true;
	if(kBattleDefinition.getBattleRound(lastRound).getNumAlive(BATTLE_UNIT_ATTACKER) == 0)
		attackerDie = true;
	if(kBattleDefinition.getBattleRound(lastRound).getNumAlive(BATTLE_UNIT_DEFENDER) == 0)
		defenderDie = true;

	int extraTime = 0;
	if((attackerLeader && attackerDie) || (defenderLeader && defenderDie))
		extraTime = BATTLE_TURNS_MELEE;

	if ( (!kBattleDefinition.getUnit(BATTLE_UNIT_ATTACKER)->isUsingDummyEntities() && kBattleDefinition.getUnit(BATTLE_UNIT_ATTACKER)->isInViewport() && showSeigeTower(kBattleDefinition.getUnit(BATTLE_UNIT_ATTACKER))) || //K-mod
		 (!kBattleDefinition.getUnit(BATTLE_UNIT_DEFENDER)->isUsingDummyEntities() && kBattleDefinition.getUnit(BATTLE_UNIT_DEFENDER)->isInViewport() && showSeigeTower(kBattleDefinition.getUnit(BATTLE_UNIT_DEFENDER))) )  //K-mod
	{
		extraTime = BATTLE_TURNS_MELEE;
	}

	return BATTLE_TURNS_SETUP + BATTLE_TURNS_ENDING + kBattleDefinition.getNumMeleeRounds() * BATTLE_TURNS_MELEE + kBattleDefinition.getNumRangedRounds() * BATTLE_TURNS_MELEE + extraTime;
}

//------------------------------------------------------------------------------------------------
// FUNCTION:	CvBattleManager::computeDeadUnits
//! \brief		Computes the number of units dead, for either the ranged or melee portion of combat.
//! \param		kDefinition The battle definition.
//! \param		bRanged true if computing the number of units that die during the ranged portion of combat,
//!					false if computing the number of units that die during the melee portion of combat.
//! \param		iUnit The index of the unit to compute (BATTLE_UNIT_ATTACKER or BATTLE_UNIT_DEFENDER).
//! \retval		The number of units that should die for the given unit in the given portion of combat
//------------------------------------------------------------------------------------------------
int CvUnit::computeUnitsToDie( const CvBattleDefinition & kDefinition, bool bRanged, BattleUnitTypes iUnit ) const
{
	FAssertMsg( iUnit == BATTLE_UNIT_ATTACKER || iUnit == BATTLE_UNIT_DEFENDER, "Invalid unit index");

	BattleTimeTypes iBeginIndex = bRanged ? BATTLE_TIME_BEGIN : BATTLE_TIME_RANGED;
	BattleTimeTypes iEndIndex = bRanged ? BATTLE_TIME_RANGED : BATTLE_TIME_END;
	return kDefinition.getUnit(iUnit)->getSubUnitsAlive(kDefinition.getDamage(iUnit, iBeginIndex)) -
		kDefinition.getUnit(iUnit)->getSubUnitsAlive( kDefinition.getDamage(iUnit, iEndIndex));
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::verifyRoundsValid
//! \brief      Verifies that all rounds in the battle plan are valid
//! \param      vctBattlePlan The battle plan
//! \retval     true if the battle plan (seems) valid, false otherwise
//------------------------------------------------------------------------------------------------
bool CvUnit::verifyRoundsValid( const CvBattleDefinition & battleDefinition ) const
{
	PROFILE_EXTRA_FUNC();
	for(int i=0;i<battleDefinition.getNumBattleRounds();i++)
	{
		if(!battleDefinition.getBattleRound(i).isValid())
			return false;
	}
	return true;
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::increaseBattleRounds
//! \brief      Increases the number of rounds in the battle.
//! \param      kBattleDefinition The definition of the battle
//------------------------------------------------------------------------------------------------
void CvUnit::increaseBattleRounds( CvBattleDefinition & kBattleDefinition ) const
{
	if ( kBattleDefinition.getUnit(BATTLE_UNIT_ATTACKER)->isRanged() && kBattleDefinition.getUnit(BATTLE_UNIT_DEFENDER)->isRanged())
	{
		kBattleDefinition.addNumRangedRounds(1);
	}
	else
	{
		kBattleDefinition.addNumMeleeRounds(1);
	}
}

//------------------------------------------------------------------------------------------------
// FUNCTION:    CvUnit::computeWaveSize
//! \brief      Computes the wave size for the round.
//! \param      bRangedRound true if the round is a ranged round
//! \param		iAttackerMax The maximum number of attackers that can participate in a wave (alive)
//! \param		iDefenderMax The maximum number of Defenders that can participate in a wave (alive)
//! \retval     The desired wave size for the given parameters
//------------------------------------------------------------------------------------------------
int CvUnit::computeWaveSize( bool bRangedRound, int iAttackerMax, int iDefenderMax ) const
{
	FAssertMsg( getCombatUnit() != NULL, "You must be fighting somebody!" );
	int aiDesiredSize[BATTLE_UNIT_COUNT];
	if ( bRangedRound )
	{
		//	the animation WAVE sizes go with the rest of the formation data (above); 0 is what the
		//	<= 0 fallbacks below already read as "use the maximum"
		aiDesiredSize[BATTLE_UNIT_ATTACKER] = 0;
		aiDesiredSize[BATTLE_UNIT_DEFENDER] = 0;
	}
	else
	{
		aiDesiredSize[BATTLE_UNIT_ATTACKER] = 0;
		aiDesiredSize[BATTLE_UNIT_DEFENDER] = 0;
	}

	aiDesiredSize[BATTLE_UNIT_DEFENDER] = aiDesiredSize[BATTLE_UNIT_DEFENDER] <= 0 ? iDefenderMax : aiDesiredSize[BATTLE_UNIT_DEFENDER];
	aiDesiredSize[BATTLE_UNIT_ATTACKER] = aiDesiredSize[BATTLE_UNIT_ATTACKER] <= 0 ? iDefenderMax : aiDesiredSize[BATTLE_UNIT_ATTACKER];
	return (
		std::min(
			std::min(aiDesiredSize[BATTLE_UNIT_ATTACKER], iAttackerMax),
			std::min(aiDesiredSize[BATTLE_UNIT_DEFENDER], iDefenderMax)
		)
	);
}

bool CvUnit::isTargetOf(const CvUnit& attacker) const
{
	PROFILE_EXTRA_FUNC();

	const CvUnitInfo& attackerInfo = attacker.getUnitInfo();

	//	the §8 targeting / immunity membership maps are keyed bool rows on the COMBAT family, read through the
	//	ONE keyed read rather than a per-key named getter ([json.md] §8, [modifier.md] §5)
	if (getUnitType() != NO_UNIT
	&& InfoValuation::keyedTarget(attackerInfo.getModifiers(), MODFAM_COMBAT, -1,
		InfoValuation::keyedTargetSegment("unitTargets"), getUnitType()) != 0)
	{
		return true;
	}

	const CvUnitInfo& ourInfo = getUnitInfo();

	if (attacker.getUnitType() != NO_UNIT && ourInfo.isDefendAgainstUnit(attacker.getUnitType()))
	{
		return true;
	}

	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
	{
		if (it->second.m_bHasUnitCombat
		&& InfoValuation::keyedTarget(attackerInfo.getModifiers(), MODFAM_COMBAT, -1,
				InfoValuation::keyedTargetSegment("targets"), it->first) != 0)
		{
			return true;
		}
	}
	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = attacker.m_unitCombatKeyedInfo.begin(), end = attacker.m_unitCombatKeyedInfo.end(); it != end; ++it)
	{
		if (it->second.m_bHasUnitCombat
		&& InfoValuation::keyedTarget(ourInfo.getModifiers(), MODFAM_COMBAT, -1,
			InfoValuation::keyedTargetSegment("defenders"), it->first) != 0)
		{
			return true;
		}
	}
	return false;
}

bool CvUnit::isEnemy(TeamTypes eTeam, const CvPlot* pPlot, const CvUnit* pUnit) const
{
	if (pUnit && (isBarbCoExist() && pUnit->isHominid() || pUnit->isBarbCoExist() && isHominid()))
	{
		return false;
	}
	return atWar(GET_PLAYER(getCombatOwner(eTeam, pPlot ? pPlot : plot())).getTeam(), eTeam);
}

bool CvUnit::isPotentialEnemy(TeamTypes eTeam, const CvPlot* pPlot, const CvUnit* pUnit) const
{
	if (pUnit && (isBarbCoExist() && pUnit->isHominid() || pUnit->isBarbCoExist() && isHominid()))
	{
		return false;
	}
	return ::isPotentialEnemy(GET_PLAYER(getCombatOwner(eTeam, pPlot ? pPlot : plot())).getTeam(), eTeam);
}

bool CvUnit::isSuicide() const
{
	return getUnitInfo().hasSkill(CLS_SKILL_SUICIDE) || getKamikazePercent() != 0;
}

int CvUnit::getDropRange() const
{
	return (m_pUnitInfo->getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT) / 100 + getExtraDropRange());
}

void CvUnit::getDefenderCombatValues(const CvUnit& kDefender, const CvPlot* pPlot, int iOurStrength, int iOurFirepower, int& iTheirOdds, int& iTheirStrength, int& iOurDamage, int& iTheirDamage, CombatDetails* pTheirDetails, const CvUnit* pDefender) const
{
	// Thin adapter over the shared Layer-1 RoundModel (CvCombatModel). This is the
	// canonical per-round formula; resolveCombat, flankingStrikeCombat, and the AI
	// (AI_attackOddsAtPlotInternal) all reach the engine through here.
	const RoundModel m = buildRoundModel(this, iOurStrength, iOurFirepower, &kDefender, pPlot, pTheirDetails);
	iTheirOdds = m.iDefenderOdds;
	iTheirStrength = m.iDefenderStrength;
	iOurDamage = m.iDamageToAttacker;
	iTheirDamage = m.iDamageToDefender;
}

int CvUnit::getTriggerValue(EventTriggerTypes eTrigger, const CvPlot* pPlot, bool bCheckPlot) const
{
	PROFILE_EXTRA_FUNC();
	const CvEventTriggerInfo& kTrigger = GC.getEventTriggerInfo(eTrigger);
	if (kTrigger.getNumUnits() <= 0)
	{
		return MIN_INT;
	}

	if (isDead())
	{
		return MIN_INT;
	}

	if (kTrigger.getNumUnitsRequired() > 0)
	{
		bool bFoundValid = false;
		for (int i = 0; i < kTrigger.getNumUnitsRequired(); ++i)
		{
			if (getUnitType() == kTrigger.getUnitRequired(i))
			{
				bFoundValid = true;
				break;
			}
		}

		if (!bFoundValid)
		{
			return MIN_INT;
		}
	}

	if (bCheckPlot && plot() && kTrigger.isUnitsOnPlot())
	{
		if (!plot()->canTrigger(eTrigger, getOwner()))
		{
			return MIN_INT;
		}
	}

	int iValue = 0;

	if (0 == getDamage() && kTrigger.getUnitDamagedWeight() > 0)
	{
		return MIN_INT;
	}

	//	Call out to Python last as its the most expensive part of the calcuation
	//	and we'll often have decided the trigger is inapplicable before this
	if (!CvString(kTrigger.getPythonCanDoUnit()).empty())
	{
		if (!Cy::call<bool>(PYRandomEventModule, kTrigger.getPythonCanDoUnit(), Cy::Args()
			<< eTrigger
			<< getOwner()
			<< getID()))
		{
			return MIN_INT;
		}
	}

	iValue += getDamage() * kTrigger.getUnitDamagedWeight();

	iValue += getExperience() * kTrigger.getUnitExperienceWeight();

	if (NULL != pPlot)
	{
		iValue += plotDistance(getX(), getY(), pPlot->getX(), pPlot->getY()) * kTrigger.getUnitDistanceWeight();
	}

	return iValue;
}

bool CvUnit::canApplyEvent(EventTypes eEvent) const
{
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (0 != kEvent.getUnitExperience())
	{
		if (!canAcquirePromotionAny())
		{
			return false;
		}
	}
	return true;
}

void CvUnit::applyEvent(EventTypes eEvent)
{
	if (!canApplyEvent(eEvent))
	{
		return;
	}

	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (0 != kEvent.getUnitExperience())
	{
		setDamage(0);
		changeExperience(kEvent.getUnitExperience());
	}


	if (kEvent.getUnitImmobileTurns() > 0)
	{
		changeStatus(STATUS_PARALYZED, kEvent.getUnitImmobileTurns());


		CvWString szText = gDLL->getText("TXT_KEY_EVENT_UNIT_IMMOBILE", getNameKey(), kEvent.getUnitImmobileTurns());
		AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szText, "AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, getButton(), (ColorTypes)GC.getInfoTypeForString("COLOR_UNIT_TEXT"), getX(), getY(), true, true);
	}

	CvWString szNameKey(kEvent.getUnitNameKey());

	if (!szNameKey.empty())
	{
		setName(gDLL->getText(kEvent.getUnitNameKey()));
	}

	if (kEvent.isDisbandUnit())
	{
		kill(false, NO_PLAYER, true);
	}
}

const CvArtInfoUnit* CvUnit::getArtInfo(int i, EraTypes eEra) const
{
	return m_pUnitInfo->getArtInfo(i, eEra, (UnitArtStyleTypes) GC.getCivilizationInfo(getCivilizationType()).getUnitArtStyleType());
}

const char* CvUnit::getButton() const
{
	const CvArtInfoUnit* pArtInfo = getArtInfo(0, GET_PLAYER(getOwner()).getCurrentEra());

	if (NULL != pArtInfo)
	{
		return pArtInfo->getButton();
	}

	return m_pUnitInfo->getButton();
}

int CvUnit::getGroupSize() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) && groupRank() > 0)
	{
		return groupRank();
	}
	return m_pUnitInfo->getGroupSize();
}

int CvUnit::getGroupDefinitions() const
{
	return m_pUnitInfo->getGroupDefinitions();
}

int CvUnit::getUnitGroupRequired(int i) const
{
	return m_pUnitInfo->getUnitGroupRequired(i);
}

//	`bRenderAlways` lives in the unit SCHEMA and in no unit record -- nothing has ever authored it -- so there is
//	no data to carry and false is the answer, not a gap. The symbol stays because the EXE calls it.
bool CvUnit::isRenderAlways() const
{
	return false;
}

float CvUnit::getAnimationMaxSpeed() const
{
	return m_pUnitInfo->getAnimationMaxSpeed();
}

float CvUnit::getAnimationPadTime() const
{
	return m_pUnitInfo->getAnimationPadTime();
}

const char* CvUnit::getFormationType() const
{
	return m_pUnitInfo->getFormationType();
}

bool CvUnit::isMechUnit() const
{
	return m_pUnitInfo->hasTag(CLS_TAG_MECHANIZED);
}

bool CvUnit::isRenderBelowWater() const
{
	return m_pUnitInfo->hasSkill(CLS_SKILL_RENDER_BELOW_WATER);
}

int CvUnit::getRenderPriority(UnitSubEntityTypes eUnitSubEntity, int iMeshGroupType, int UNIT_MAX_SUB_TYPES) const
{
	if (eUnitSubEntity == UNIT_SUB_ENTITY_SIEGE_TOWER)
	{
		return (getOwner() * (GC.getNumUnitInfos() + 2) * UNIT_MAX_SUB_TYPES) + iMeshGroupType;
	}
	else
	{
		return (getOwner() * (GC.getNumUnitInfos() + 2) * UNIT_MAX_SUB_TYPES) + m_eUnitType * UNIT_MAX_SUB_TYPES + iMeshGroupType;
	}
}

bool CvUnit::isAlwaysHostile(const CvPlot* pPlot) const
{
	if (!getUnitInfo().hasSkill(CLS_SKILL_ALWAYS_HOSTILE) && getHiddenNationalityCount() < 1)
	{
		return false;
	}

	if (pPlot && pPlot->isCity(true, getTeam()))
	{
		if (isBlendIntoCity())
		{
			return isAssassin() && pPlot == plot();
		}

		return pPlot->getOwner() != getOwner() && (!isBarbCoExist() || !pPlot->isHominid());
	}

	return true;
}

bool CvUnit::verifyStackValid()
{
	PROFILE_EXTRA_FUNC();
	if (isDead()) return true;

	const CvPlot* pPlot = plot();
	if (!canCoexistAlwaysOnPlot(*pPlot))
	{
		foreach_ (const CvUnit* unit, pPlot->units())
		{
			if (unit != this
			&& isEnemy(unit->getTeam(), NULL, unit)
			&& !isInvisible(unit->getTeam())
			&& !unit->canCoexistWithTeamOnPlot(getTeam(), *pPlot))
			{
				return jumpToNearestValidPlot();
			}
		}
	}
	return true;
}


//check if quick combat
bool CvUnit::isCombatVisible(const CvUnit* pDefender) const
{
	if (isHuman())
	{
		if (!GET_PLAYER(getOwner()).isOption(PLAYEROPTION_QUICK_ATTACK))
		{
			return true;
		}
	}
	else if (pDefender && pDefender->isHuman() && !GET_PLAYER(pDefender->getOwner()).isOption(PLAYEROPTION_QUICK_DEFENSE))
	{
		return true;
	}
	return false;
}

// used by the executable for the red glow and plot indicators
bool CvUnit::shouldShowEnemyGlow(TeamTypes eForTeam) const
{
	if (isDelayedDeath() || getDomainType() == DOMAIN_AIR || !canFight())
	{
		return false;
	}

	const CvPlot* pPlot = plot();
	if (pPlot == NULL)
	{
		return false;
	}

	const TeamTypes ePlotTeam = pPlot->getTeam();
	if (ePlotTeam != eForTeam || !isEnemy(ePlotTeam))
	{
		return false;
	}

	return true;
}

bool CvUnit::shouldShowFoundBorders() const
{
	return isFound();
}


void CvUnit::cheat(bool bCtrl, bool bAlt, bool bShift)
{
	if (bCtrl && (gDLL->getChtLvl() > 0 || GC.getGame().isDebugMode()))
	{
		setPromotionReady(true);
	}
}

float CvUnit::getHealthBarModifier() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		const int iWidthDivisor = (1 + (int)GET_PLAYER(getOwner()).getCurrentEra()) * 4;
		return ((GC.getDefineFLOAT("HEALTH_BAR_WIDTH") / iWidthDivisor) / (GC.getGame().getBestLandUnitCombat() * 2));
	}
	return (GC.getDefineFLOAT("HEALTH_BAR_WIDTH") / (GC.getGame().getBestLandUnitCombat() * 2));
}

void CvUnit::getLayerAnimationPaths(std::vector<AnimationPathTypes>& aAnimationPaths) const
{
	// EXE-BOUND (DllExport): the closed .exe asks for a unit's layered animation paths, so the signature is
	// fixed. It answers EMPTY because no promotion authors a layer animation path -- the curator carries the
	// mapping, the shipped data carries no key, so the walk it used to do could only ever find nothing.
	aAnimationPaths.clear();
}

int CvUnit::getSelectionSoundScript() const
{
	int iScriptId = getArtInfo(0, GET_PLAYER(getOwner()).getCurrentEra())->getSelectionSoundScriptId();
	if (iScriptId == -1)
	{
		iScriptId = GC.getCivilizationInfo(getCivilizationType()).getSelectionSoundScriptId();
	}
	return iScriptId;
}



// Dale - SA: Active Defense
void CvUnit::doActiveDefense()
{
	PROFILE_EXTRA_FUNC();
	int iDamage, iUnitDamage;
	CvUnit* pDefender = NULL;
	CvCity* pCity = NULL;
	bool bSuccess = false;
	CvWString szBuffer;
	if (!GC.isDCM_ACTIVE_DEFENSE())
	{
		return;
	}
	if (getGroup()->getActivityType() != ACTIVITY_INTERCEPT)
	{
		return;
	}
	foreach_(CvPlot* pLoopPlot, plot()->rect(2, 2))
	{
		if (pLoopPlot->getNumUnits() > 0)
		{
			pDefender = airStrikeTarget(pLoopPlot);
			if (pDefender != NULL)
			{
				iDamage = airCombatDamage(pDefender);
				iUnitDamage = std::max(pDefender->getDamage(), std::min((pDefender->getDamage() + iDamage), airCombatLimit(pDefender)));

				{

					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ARE_ATTACKED_BY_AIR", pDefender->getNameKey(), getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->getMaxHP()));
					AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_AIR_ATTACK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pLoopPlot->getX(), pLoopPlot->getY(), true, true);
					szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ATTACK_BY_AIR", getNameKey(), pDefender->getNameKey(), -(((iUnitDamage - pDefender->getDamage()) * 100) / pDefender->getMaxHP()));
					AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_AIR_ATTACKED", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_GREEN(), pLoopPlot->getX(), pLoopPlot->getY());
				}
				collateralCombat(pLoopPlot, pDefender);
				pDefender->setDamage(iUnitDamage, getOwner());
				bSuccess = true;
				//TB Combat Mod begin
				//TB Combat mod end
				if (pLoopPlot->isActiveVisible(false) && (!pDefender->isUsingDummyEntities() && pDefender->isInViewport()))
				{
					setCombatTimer(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime());
					GC.getGame().incrementTurnTimer(getCombatTimer());

					addMission(CvAirMissionDefinition(MISSION_AIRSTRIKE, pLoopPlot, this, pDefender));
				}
			}
		}
	}
}
// ! Dale - SA: Active Defense

// Dale - FE: Fighters
bool CvUnit::canFEngage() const
{
	if(!GC.isDCM_FIGHTER_ENGAGE())
	{
		return false;
	}
	if (!m_pUnitInfo->hasSkill(CLS_SKILL_DCM_FIGHTER_ENGAGE))
	{
		return false;
	}
	if (getDomainType() != DOMAIN_AIR)
	{
		return false;
	}
	if (isMadeAttack())
	{
		return false;
	}
//	if (isCargo())
//	{
//		return false;
//	}
	return true;
}

bool CvUnit::canFEngageAt(const CvPlot* pPlot, int iX, int iY) const
{
	PROFILE_EXTRA_FUNC();
	if (!canFEngage())
	{
		return false;
	}
	if (iX < 0 || iY < 0)
	{
		return false;
	}
	const CvPlot* pTargetPlot = GC.getMap().plot(iX, iY);

	if (plotDistance(pPlot->getX(), pPlot->getY(), pTargetPlot->getX(), pTargetPlot->getY()) > airRange())
	{
		return false;
	}
	if (pTargetPlot->isOwned() && pTargetPlot->getTeam() != getTeam()
	&& !atWar(pTargetPlot->getTeam(), getTeam()))
	{
		return false;
	}
	for (int iI = 0; iI < MAX_PLAYERS; ++iI)
	{
		if (atWar(GET_PLAYER((PlayerTypes)iI).getTeam(), getTeam())
		&& algo::any_of(GET_PLAYER((PlayerTypes)iI).units(), CvUnit::fn::plot() == pTargetPlot && CvUnit::fn::getDomainType() == DOMAIN_AIR))
		{
			return true;
		}
	}
	return false;
}

bool CvUnit::fighterEngage(int iX, int iY)
{
	PROFILE_EXTRA_FUNC();
	if (!canFEngageAt(plot(), iX, iY))
	{
		return false;
	}
	CvPlot* pPlot = GC.getMap().plot(iX, iY);

	if (interceptTest(pPlot))
	{
		return true;
	}
	int iCount = algo::count_if(pPlot->units(), CvUnit::fn::getDomainType() == DOMAIN_AIR);
	iCount = 1 + GC.getGame().getSorenRandNum(iCount, "Choose plane");
	CvUnit* pDefender = NULL;
	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
	while (iCount > 0)
	{
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		if (pLoopUnit->getDomainType() == DOMAIN_AIR)
		{
			iCount--;
			pDefender = pLoopUnit;
		}
		pUnitNode = pPlot->nextUnitNode(pUnitNode);
	}
	if (pDefender != NULL)
	{
		CvAirMissionDefinition kAirMission(MISSION_AIRSTRIKE, pPlot, this, pDefender);
		resolveAirCombat(pDefender, pPlot, kAirMission);
		if (kAirMission.isValid())
		{
			setCombatTimer(GC.getMissionInfo(MISSION_AIRSTRIKE).getTime());
			GC.getGame().incrementTurnTimer(getCombatTimer());
			addMission(kAirMission);
		}

		if (isDead())
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SHOT_DOWN_ENEMY", pDefender->getNameKey(), getNameKey(), getVisualCivAdjective(pDefender->getTeam()));
			AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_SHOT_DOWN", getNameKey(), pDefender->getNameKey());
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
		else if (kAirMission.getDamage(BATTLE_UNIT_ATTACKER) > 0)
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_HURT_ENEMY_AIR", pDefender->getNameKey(), getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_ATTACKER)), getVisualCivAdjective(pDefender->getTeam()));
			AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_AIR_UNIT_HURT", getNameKey(), pDefender->getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_ATTACKER)));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
		if (pDefender->isDead())
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_SHOT_DOWN_ENEMY", getNameKey(), pDefender->getNameKey(), pDefender->getVisualCivAdjective(getTeam()));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_UNIT_SHOT_DOWN", pDefender->getNameKey(), getNameKey());
			AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
		else if (kAirMission.getDamage(BATTLE_UNIT_DEFENDER) > 0)
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_DAMAGED_ENEMY_AIR", getNameKey(), pDefender->getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_DEFENDER)), pDefender->getVisualCivAdjective(getTeam()));
			AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_AIR_UNIT_DAMAGED", pDefender->getNameKey(), getNameKey(), -(kAirMission.getDamage(BATTLE_UNIT_DEFENDER)));
			AddDLLMessage(pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
		if (0 == kAirMission.getDamage(BATTLE_UNIT_ATTACKER) + kAirMission.getDamage(BATTLE_UNIT_DEFENDER))
		{

			CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_ABORTED_ENEMY_AIR", pDefender->getNameKey(), getNameKey(), getVisualCivAdjective(getTeam()));
			AddDLLMessage(pDefender->getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPT", MESSAGE_TYPE_INFO, pDefender->getButton(), GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY(), true, true);
			szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_AIR_UNIT_ABORTED", getNameKey(), pDefender->getNameKey());
			AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_INTERCEPTED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
		}
	}
	setMadeAttack(true);
	changeMoves(GC.getMOVE_DENOMINATOR());
	return true;
}
// ! Dale - FE: Fighters


// IDW start
// unit influences combat area after victory
// returns influence 10x int % in defended plot
// also handles emergency drafting???
int CvUnit::doVictoryInfluence(CvUnit* pLoserUnit, bool bAttacking, bool bWithdrawal)
{
	PROFILE_FUNC();
	if (pLoserUnit == NULL)
	{
		FErrorMsg("This can maybe occur, investigate when time allows");
		return 0; // this is not ideal, but if unit is deleted before this calculation we dont want the ctd
	}

	if (!pLoserUnit->canDefend() || isAnimal() || pLoserUnit->isAnimal())
		return 0; // no influence from worker capture or animal kill

	if (isAlwaysHostile(plot()) || pLoserUnit->isAlwaysHostile(pLoserUnit->plot()))
		return 0;

	if (GC.isIDW_NO_BARBARIAN_INFLUENCE() && (isHominid() || pLoserUnit->isHominid()))
		return 0;

	if (GC.isIDW_NO_NAVAL_INFLUENCE() && DOMAIN_SEA == getDomainType())
		return 0;

	const PlayerTypes pLoserPlayer = pLoserUnit->getOwner();

	if (pLoserPlayer < 0 || pLoserPlayer > MAX_PLAYERS)
	{
		FErrorMsg("This can maybe occur, investigate when time allows");
		return 0; // Bad unit owner TODO find out why gets passed in
	}

	CvPlot* pWinnerPlot = plot();
	CvPlot* pLoserPlot = pLoserUnit->plot();

	const CvPlot* pDefenderPlot = bAttacking ? pLoserPlot : pWinnerPlot;
	bool bFieldCombat = true;
	const int64_t iWinnerCultureBefore = pDefenderPlot->getCulture(getOwner()); //used later for influence %

	// Multipliers are percents, stored as *100. E.g., 1 = 0.1%, 100 = 1%, 10,000 = 100%
	int iWinnerPlotMultiplier = 100 * GC.getIDW_WINNER_PLOT_MULTIPLIER();
	int iLoserPlotMultiplier = 100 * GC.getIDW_LOSER_PLOT_MULTIPLIER();
	// Unused currently
	if (bWithdrawal)
	{
		iWinnerPlotMultiplier /= 3;
		iLoserPlotMultiplier /= 3;
	}
	if (pLoserPlot->isEnemyCity(*this, true)) // city combat
	{
		const CvCity* pLoserCity = pLoserPlot->getPlotCity();
		int iDefenders = pLoserPlot->getNumVisibleEnemyDefenders(this);

		// Couldn't figure out how to count leading zeroes for ghetto log2(city pop)
		// int iEmergencyDefenderLimit = 15 - countl_zero((uint16_t)(pLoserPlot->getPlotCity()->getPopulation()));
		// This is effectively same though; thresholds at 4, 16, 36, 64, vs 4, 8, 16, 32, 64, etc
		int iEmergencyDefenderLimit = 3 + intSqrt((uint)(pLoserCity->getPopulation())) / 2;

		if (GC.isIDW_EMERGENCY_DRAFT_ENABLED() && iDefenders < iEmergencyDefenderLimit)
		{
			const int iAttackerCulturePercent = pLoserPlot->calculateCulturePercent(getOwner(), 1);

			// if attacker culture has not yet surpassed threshold & defender can still draft,
			// city is not captured yet but emergency militia is drafted
			if (iAttackerCulturePercent / 10 < GC.getIDW_EMERGENCY_DRAFT_CULTURE_THRESHOLD()
				&& pLoserCity->getPopulation() >= GC.getIDW_EMERGENCY_DRAFT_MIN_POPULATION())
			{
				pLoserPlot->getPlotCity()->emergencyConscript();
				iDefenders++;

				// Draft twice if under 1/2 defender threshold
				if (iDefenders * 2 <= iEmergencyDefenderLimit
					&& pLoserCity->getPopulation() >= GC.getIDW_EMERGENCY_DRAFT_MIN_POPULATION())
				{
					pLoserPlot->getPlotCity()->emergencyConscript();
					iDefenders++;
				}

				CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_CITY_MILITIA_EMERGED",
					GET_PLAYER(getOwner()).getCivilizationAdjective(), iAttackerCulturePercent/10,
					iAttackerCulturePercent % 10, GC.getIDW_EMERGENCY_DRAFT_CULTURE_THRESHOLD());
				AddDLLMessage(pLoserPlayer, false, GC.getEVENT_MESSAGE_TIME(),
					szBuffer, "AS2D_UNIT_BUILD_UNIT", MESSAGE_TYPE_INFO, getButton(),
					GC.getCOLOR_GREEN(), pLoserPlot->getX(), pLoserPlot->getY(), true, true);
				AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					szBuffer, "AS2D_UNIT_BUILD_UNIT", MESSAGE_TYPE_INFO, getButton(),
					GC.getCOLOR_RED(), pLoserPlot->getX(), pLoserPlot->getY());
			}
		}
		if (iDefenders == 0) // City Capture
		{
			const int iNoCityDefenderMultiplier = GC.getIDW_NO_CITY_DEFENDER_MULTIPLIER();

			influencePlots(pLoserPlot, pLoserPlayer, iLoserPlotMultiplier * iNoCityDefenderMultiplier / 100);
			influencePlots(pWinnerPlot, pLoserPlayer, iWinnerPlotMultiplier * iNoCityDefenderMultiplier / 100);
			bFieldCombat = false;
		}
	}
	else if (pLoserPlot->isActsAsCity() && pLoserPlot->getNumVisibleEnemyDefenders(this) == 0)
	{
		// Fort captured
		const int iFortCaptureMultiplier = GC.getIDW_FORT_CAPTURE_MULTIPLIER();

		influencePlots(pLoserPlot, pLoserPlayer, iLoserPlotMultiplier * iFortCaptureMultiplier / 100);
		influencePlots(pWinnerPlot, pLoserPlayer, iWinnerPlotMultiplier * iFortCaptureMultiplier / 100);
		bFieldCombat = false;
	}
	if (bFieldCombat)
	{
		influencePlots(pLoserPlot, pLoserPlayer, iLoserPlotMultiplier);
		influencePlots(pWinnerPlot, pLoserPlayer, iWinnerPlotMultiplier);
	}

	// calculate influence % in defended plot (to be displayed in game log)
	const int64_t iTotalCulture = pDefenderPlot->countTotalCulture();

	if (iTotalCulture > 0)
	{
		// A per-mille INFLUENCE ratio, bounded by construction -- the culture it derives from is not, so the
		// whole expression resolves in 64 bits and reduces here, at the point of use.
		return static_cast<int>((pDefenderPlot->getCulture(getOwner()) - iWinnerCultureBefore) * 1000 / iTotalCulture);
	}
    return 0;
}

// unit influences given plot and surounding area i.e. transfers culture from target civ to unit's owner
void CvUnit::influencePlots(CvPlot* pCentralPlot, const PlayerTypes eTargetPlayer, const int iLocationMultiplier)
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, MAX_PLAYERS, eTargetPlayer);

	// get influence radius
	const int iInfluenceRadius = GC.getIDW_INFLUENCE_RADIUS();
	if (iInfluenceRadius < 0) return;

	// calculate base multiplier used for all plots
	int iMultiplier = GC.getIDW_BASE_COMBAT_INFLUENCE() * iLocationMultiplier / 100;
	if (iMultiplier < 1) return;

	if (NO_UNIT != getLeaderUnitType()) // if led
	{
		iMultiplier *= GC.getIDW_LEADER_MULTIPLIER();
		iMultiplier /= 100;
	}
	if (iMultiplier < 1) return;

	const int iCityPlotMultiplier = GC.isIDW_EMERGENCY_DRAFT_ENABLED() ? 100 : GC.getIDW_CITY_TILE_MULTIPLIER();

	for (int iDX = -iInfluenceRadius; iDX <= iInfluenceRadius; iDX++)
	{
		for (int iDY = -iInfluenceRadius; iDY <= iInfluenceRadius; iDY++)
		{
			const int iDistance = plotDistance(0, 0, iDX, iDY);

			if (iDistance <= iInfluenceRadius)
			{
				CvPlot* pLoopPlot = plotXY(pCentralPlot->getX(), pCentralPlot->getY(), iDX, iDY);

				if (pLoopPlot != NULL)
				{
					const int64_t iTargetCulture = pLoopPlot->getCulture(eTargetPlayer);
					if (iTargetCulture < 1) continue;

					int iMult = iMultiplier;
					// calculate distance multiplier for current plot
					int iDistanceMultiplier = 100 / intPow((iDistance + 1), 2);
					if (iDistanceMultiplier < 1) continue;

					// Cities gain reduced culture transfer if not emergency draft,
					// also halved if city has protected culture
					if (pLoopPlot->isCity())
					{
						iMult *= iCityPlotMultiplier;
						iMult /= 100;
						if (pLoopPlot->getPlotCity()->isProtectedCulture())
							iMult /= 2;
					}
					// and non cities avoid culture transfer, if they are protected
					else if (pLoopPlot->getWorkingCity() != NULL && pLoopPlot->getWorkingCity()->isProtectedCulture())
						continue;

					if (iMult < 1) continue;

					// Removing a total of 1e6
					int64_t iCultureTransfer = iMult * iDistanceMultiplier / 100 * iTargetCulture / 10000;

					// Catch potential unlikely overflows?
					if (iCultureTransfer < 0) iCultureTransfer = 0;

					if (iTargetCulture < iCultureTransfer)
					{
						// cannot transfer more culture than remaining target culure
						iCultureTransfer = iTargetCulture;
					}
					if (iCultureTransfer == iTargetCulture
					&& pLoopPlot->isActsAsCity()) // fort, must not lose all culture when it may still be garrisoned)
					{
						iCultureTransfer--;
					}

					if (iCultureTransfer > 0)
					{
						// target player's culture in plot is lowered
						pLoopPlot->changeCulture(eTargetPlayer, -iCultureTransfer, false);
						// owners's culture in plot is raised
					}
						pLoopPlot->changeCulture(getOwner(), iCultureTransfer, true);
				}
			}
		}
	}
}


// unit influences current tile via pillaging
// returns influence 10x int % in pillaged plot
int CvUnit::doPillageInfluence()
{
	if (isAnimal() || isHiddenNationality())
	{
		return 0;
	}
	if (isHominid() && GC.isIDW_NO_BARBARIAN_INFLUENCE())
	{
		return 0;
	}
	if (DOMAIN_SEA == getDomainType() && GC.isIDW_NO_NAVAL_INFLUENCE())
	{
		return 0;
	}

	CvPlot* pPlot = plot();
	if (pPlot == NULL)
	{
		FErrorMsg("pPlot == NULL; should not happen");
		return 0;
	}
	if (pPlot->getWorkingCity() != NULL && pPlot->getWorkingCity()->isProtectedCulture())
	{
		return 0;
	}

	const PlayerTypes eTargetPlayer = pPlot->getOwner();
	const int64_t iTargetCulture = pPlot->getCulture(eTargetPlayer);
	if (iTargetCulture < 1)
	{
		FErrorMsg("iTargetCulture < 1; should not happen");
		return 0;
	}
	int64_t iCultureTransfer = GC.getIDW_BASE_PILLAGE_INFLUENCE() * iTargetCulture / 100;
	if (iCultureTransfer < 1)
		iCultureTransfer = 1;
	// cannot transfer more culture than remaining target culure
	if (iTargetCulture <= iCultureTransfer)
	{
		iCultureTransfer = iTargetCulture;
	}

	if (iCultureTransfer > 0)
	{
		const int64_t iOurCultureBefore = pPlot->getCulture(getOwner()); //used later for influence %

		pPlot->changeCulture(eTargetPlayer, -iCultureTransfer, false);
		pPlot->changeCulture(getOwner(), iCultureTransfer, true);

		// calculate 10x influence % in pillaged plot (to be displayed in game log)
		// A per-mille INFLUENCE ratio, bounded by construction (see doVictoryInfluence).
		return static_cast<int>((pPlot->getCulture(getOwner()) - iOurCultureBefore) * 1000 / pPlot->countTotalCulture());
	}
	return 0;
}
// ------ END InfluenceDrivenWar ---------------------------------


bool CvUnit::canPerformInquisition(const CvPlot* pPlot) const
{
	if (!getUnitInfo().hasSkill(CLS_SKILL_INQUISITOR))
	{
		return false;
	}
	const CvCity* pCity = pPlot->getPlotCity();

	if (pCity == NULL)
	{
		return false;
	}
	if (GET_PLAYER(getOwner()).getStateReligion() == NO_RELIGION)
	{
		return false;
	}
	if (!pCity->isHasReligion(GET_PLAYER(getOwner()).getStateReligion()))
	{
		return false;
	}
	// Allow inquisitions in vassals
	if (pCity->getTeam() != getTeam() && !GET_TEAM(pCity->getTeam()).isVassal(getTeam()))
	{
		return false;
	}
	if (!pCity->isInquisitionConditions() || !GET_PLAYER(getOwner()).isInquisitionConditions())
	{
		return false;
	}
	if (GET_PLAYER(getOwner()).getStateReligion() != GET_PLAYER(pCity->getOwner()).getStateReligion())
	{
		return false;
	}
	return true;
}


bool CvUnit::performInquisition()
{
	PROFILE_EXTRA_FUNC();
	const CvPlot* pPlot = plot();

	if (!canPerformInquisition(pPlot))
	{
		return false;
	}
	CvCity* pCity = pPlot->getPlotCity();

	if (pCity != NULL)
	{
		CvWString szBuffer;

		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kLoopPlayer.isAlive()
			&& (pPlot->isVisible(kLoopPlayer.getTeam(), true) || pPlot->isRevealed(kLoopPlayer.getTeam(), true)))
			{
				gDLL->getInterfaceIFace()->playGeneralSound("AS3D_UN_CHRIST_MISSIONARY_ACTIVATE", pPlot->getPoint());
			}
		}

		int iHolyCityVal = 0;
		int iReligionCount = 0;
		for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
		{
			if ((ReligionTypes)iI != GET_PLAYER(getOwner()).getStateReligion() && pCity->isHasReligion((ReligionTypes)iI))
			{
				iReligionCount++;
				if (pCity->isHolyCity((ReligionTypes)iI))
				{
					iHolyCityVal = 50;
				}
			}
		}
		int iCompensationGold = 0;

		if (GC.getGame().getSorenRandNum(100, "Inquisition Persection Chance") < std::max(25, (95 - iHolyCityVal - (5 * iReligionCount))))
		{
			// Change memory if we are removing a religion that is another player's state religion
			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
				if (kLoopPlayer.isAlive()
				&& (pPlot->isVisible(kLoopPlayer.getTeam(), false) || pPlot->isRevealed(kLoopPlayer.getTeam(), false))
				&& GET_TEAM(kLoopPlayer.getTeam()).isHasMet(GET_PLAYER(getOwner()).getTeam()))
				{
					for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
					{
						if (GET_PLAYER(getOwner()).getStateReligion() != (ReligionTypes)iJ
						// if the player has the holy city, or has the religion as a state religion.
						&& (kLoopPlayer.hasHolyCity((ReligionTypes)iJ) || pCity->isHasReligion((ReligionTypes)iJ) && kLoopPlayer.getStateReligion() == (ReligionTypes)iJ))
						{
							kLoopPlayer.AI_changeMemoryCount(getOwner(), MEMORY_INQUISITION, 1);
							break;
						}
					}
				}
			}
			//Remove temples, monasteries, etc...
			{
				std::vector<BuildingTypes> temp;

				//	A building's religion PREREQ is an ordinary `requires` atom now (the curator emits it as the
				//	reversible MEANS it always was -- a religion can leave via exactly this inquisition), so the
				//	building's own tree names the religions it depends on. Asking every religion in the registry
				//	whether THIS building named it was the own-data inversion.
				//	⚠ The structural walk reports what a tree MENTIONS, not what it REQUIRES, so a religion named
				//	under a `noneOf` would read as a dependency here. Nothing authors one, and a gate VERDICT is
				//	the enabler's, never this walk's ([CvConditionQuery.h]).
				const ReligionTypes eStateReligion = GET_PLAYER(getOwner()).getStateReligion();

				foreach_(const BuildingTypes eType, pCity->getHasBuildings())
				{
					const CvBuildingInfo& buildingX = GC.getBuildingInfo(eType);
					const CvRequires* pRequires = buildingX.getRequires();
					if (pRequires == NULL)
					{
						continue;
					}
					std::vector<int> requiredReligions;
					CvConditionQuery::collectIds(pRequires->build, EDGEB_RELIGIONS, requiredReligions);
					CvConditionQuery::collectIds(pRequires->operate, EDGEB_RELIGIONS, requiredReligions);
					if (requiredReligions.empty())
					{
						continue;
					}
					//	A building the STATE religion satisfies survives the purge, however many faiths it names.
					bool bKeepsStateReligion = false;
					for (size_t iReligion = 0; iReligion < requiredReligions.size(); ++iReligion)
					{
						if (requiredReligions[iReligion] == (int)eStateReligion)
						{
							bKeepsStateReligion = true;
							break;
						}
					}
					if (bKeepsStateReligion)
					{
						continue;
					}
					temp.push_back(eType);
					iCompensationGold += buildingX.getCost() * CvGameSpeedScale::hammerCostPercent() / std::max(1, GC.getDefineINT("INQUISITION_BUILDING_GOLD_DIVISOR"));
				}
				foreach_(const BuildingTypes eType, temp)
				{
					pCity->changeHasBuilding(eType, false);
				}
			}
			//Remove the Religion & Holy Cities
			for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
			{
				if (GET_PLAYER(getOwner()).getStateReligion() != (ReligionTypes)iI)
				{
					if (pCity->isHolyCity((ReligionTypes)iI))
					{
						//TODO: This value needs to be set from python
						if (GC.getDefineINT("OC_RESPAWN_HOLY_CITIES"))
						{
							GC.getGame().setHolyCity((ReligionTypes)iI, NULL , false);
							iCompensationGold += GC.getDefineINT("HOLYCITY_REMOVAL_GOLD");
							pCity->setHasReligion((ReligionTypes)iI, false, false, false);

							//Find the best place to replace the holy city
							PlayerTypes eBestPlayer = NO_PLAYER;
							int iBestCount = 0;
							for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
							{
								const CvPlayerAI& kLoopPlayer = GET_PLAYER((PlayerTypes)iJ);

								if (kLoopPlayer.isAlive())
								{
									const int iCount = kLoopPlayer.getHasReligionCount((ReligionTypes)iI);
									if (iCount > iBestCount)
									{
										iBestCount = iCount;
										eBestPlayer = (PlayerTypes)iJ;
									}
								}
							}
							//Relocate the holy city
							if (eBestPlayer != NO_PLAYER)
							{
								const CvPlayerAI& kPlayer = GET_PLAYER(eBestPlayer);
								foreach_(const CvCity* pLoopCity, kPlayer.cities())
								{
									if (pLoopCity->isHasReligion((ReligionTypes)iI))
									{
										GC.getGame().setHolyCity((ReligionTypes)iI, pLoopCity, true);
										//TODO: Create a text entry: "A Holy City Religion has been Respawned"
										{

											szBuffer = gDLL->getText("TXT_KEY_MSG_HOLY_CITY_RESPAWNED");
											AddDLLMessage(
												GC.getGame().getActivePlayer(), true, GC.getEVENT_MESSAGE_TIME(),
												szBuffer, "AS2D_DISCOVERBONUS", MESSAGE_TYPE_MAJOR_EVENT,
												"Art/Interface/Buttons/TerrainFeatures/Forest.dds",
												ColorTypes(8), getX(), getY(), false, false
											);
										}
										break;
									}
								}
							}
						}
					}
					else if (pCity->isHasReligion((ReligionTypes)iI))
					{
						pCity->setHasReligion((ReligionTypes)iI, false, false, false);
						iCompensationGold += GC.getDefineINT("RELIGION_REMOVAL_GOLD");
					}
				}
			}

			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
				if (kLoopPlayer.isAlive()
				&& (pPlot->isVisible(kLoopPlayer.getTeam(), true) || pPlot->isRevealed(kLoopPlayer.getTeam(), true)))
				{
					szBuffer = gDLL->getText("TXT_KEY_MSG_INQUISITION", pCity->getNameKey());
					AddDLLMessage(((PlayerTypes)iI), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PLAGUE", MESSAGE_TYPE_MAJOR_EVENT, getButton() , ColorTypes(8), getX(), getY(), true, true);
				}
			}

			//Increase Temp Anger
			pCity->changeHurryAngerTimer(pCity->flatHurryAngerLength());
			if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
			{
				//Avoid setting the Rev Index below 0...
				pCity->changeLocalRevIndex(-std::min(pCity->getRevolutionIndex(), iCompensationGold));
			}
			else GET_PLAYER(getOwner()).changeGold(iCompensationGold);
		}
		//Inquisition Fails...
		else
		{
			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				const CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
				if (kLoopPlayer.isAlive()
				&& (pPlot->isVisible(kLoopPlayer.getTeam(), true) || pPlot->isRevealed(kLoopPlayer.getTeam(), true)))
				{
					szBuffer = gDLL->getText("TXT_KEY_MSG_INQUISITION_FAIL", pCity->getNameKey());
					AddDLLMessage(((PlayerTypes)iI), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_SABOTAGE", MESSAGE_TYPE_MAJOR_EVENT, getButton() , ColorTypes(8), getX(), getY(), true, true);
				}
			}
			pCity->changeHurryAngerTimer(pCity->flatHurryAngerLength());

			if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
			{
				pCity->changeLocalRevIndex(iCompensationGold / 2);
			}
		}
		if (pPlot->isActiveVisible(false))
		{
			NotifyEntity(MISSION_INQUISITION);
		}
	}
	kill(true, NO_PLAYER, true);
	return true;
}


bool CvUnit::canTradeUnit(PlayerTypes eReceivingPlayer) const
{
	PROFILE_EXTRA_FUNC();
	if (eReceivingPlayer == NO_PLAYER || eReceivingPlayer > MAX_PLAYERS)
	{
		return false;
	}
	if (hasCargo())
	{
		return false;
	}
	if (getDomainType() == DOMAIN_SEA)
	{
		CvArea* pWaterArea;
		bool bCoast = false;
		foreach_(const CvCity* pLoopCity, GET_PLAYER(eReceivingPlayer).cities())
		{
			if ((pWaterArea = pLoopCity->waterArea()) != NULL && !pWaterArea->isLake())
			{
				bCoast = true;
				break;
			}
		}
		if (!bCoast)
		{
			return false;
		}
	}
	return true;
}

void CvUnit::tradeUnit(PlayerTypes eReceivingPlayer)
{
	if (eReceivingPlayer == NO_PLAYER)
	{
		return;
	}
	CvPlayerAI& receiver = GET_PLAYER(eReceivingPlayer);
	CvCity* pBestCity = NULL;

	if (getDomainType() == DOMAIN_SEA)
	{
		pBestCity = receiver.findBestCoastalCity();
	}
	else pBestCity = receiver.getCapitalCity();

	CvUnit* pTradeUnit = receiver.initUnit(
		getUnitType(), pBestCity->getX(), pBestCity->getY(), AI_getUnitAIType(), NO_DIRECTION,
		GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark")
	);
	if (pTradeUnit != NULL)
	{
		pTradeUnit->convert(this);
		if (receiver.isHumanPlayer())
		{
			AddDLLMessage(
				eReceivingPlayer, false, GC.getEVENT_MESSAGE_TIME(),
				gDLL->getText(
					"TXT_KEY_MISC_TRADED_UNIT_TO_YOU",
					GET_PLAYER(getOwner()).getNameKey(), pTradeUnit->getNameKey()
				),
				"AS2D_UNITGIFTED", MESSAGE_TYPE_INFO, pTradeUnit->getButton(),
				GC.getCOLOR_WHITE(), pTradeUnit->getX(), pTradeUnit->getY(), true, true
			);
		}
	}
}

void CvUnit::spyNuke(int iX, int iY, bool bCaught)
{
	PROFILE_EXTRA_FUNC();

	CvWString szBuffer;
	CvPlot* pPlot = GC.getMap().plot(iX, iY);
	bool nukedTeams[MAX_PC_TEAMS];

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		nukedTeams[iI] = isNukeVictim(pPlot, (TeamTypes)iI, 0);
	}

	if (bCaught)
	{
		nukeDiplomacy(nukedTeams);
		szBuffer = gDLL->getText("TXT_KEY_MISC_NUKE_ENEMY_SPY", GET_PLAYER(getOwner()).getNameKey(), GET_PLAYER(pPlot->getOwner()).getNameKey());
	}
	else szBuffer = gDLL->getText("TXT_KEY_MISC_NUKE_UNKNOWN", GET_PLAYER(pPlot->getOwner()).getNameKey());


	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			AddDLLMessage(
				(PlayerTypes)iI, iI == getOwner(), GC.getEVENT_MESSAGE_TIME(),
				szBuffer, "AS2D_NUKE_EXPLODES", MESSAGE_TYPE_MAJOR_EVENT, getButton(),
				GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY(), true, true
			);
		}
	}

	//This is just so the espionage mission makes the cool explosion effect.
	if (GC.getInfoTypeForString("EFFECT_ICBM_NUCLEAR_EXPLOSION") != -1)
	{
		gDLL->getEngineIFace()->TriggerEffect((EffectTypes)GC.getInfoTypeForString("EFFECT_ICBM_NUCLEAR_EXPLOSION"), pPlot->getPoint(), 0);
		gDLL->getInterfaceIFace()->playGeneralSound("AS2D_NUKE_EXPLODES", pPlot->getPoint());
	}
	pPlot->nukeExplosion(0);
}


bool CvUnit::canClaimTerritory(const CvPlot* pPlot) const
{
	if (!GET_PLAYER(getOwner()).hasFixedBorders())
	{
		return false;
	}

	if (isNPC() || getUnitInfo().hasSkill(CLS_SKILL_ALWAYS_HOSTILE) || isHiddenNationality() || !getUnitInfo().hasSkill(CLS_SKILL_PILLAGE))
	{
		return false;
	}

	if (pPlot != NULL)
	{
		if (getOwner() == pPlot->getOwner())
		{
			return false;
		}
		// if we or someone else (a friend) already claimed this plot in this turn
		if (pPlot->getClaimingOwner() != NO_PLAYER)
		{
			return false;
		}

		if (!pPlot->isPotentialCityWork())
		{
			return false;
		}

		if (pPlot->isCity() || pPlot->isCity(true) && !GET_TEAM(getTeam()).isAtWar(pPlot->getTeam()))
		{
			return false;
		}

		if (GC.getGame().getModderGameOption(MODDERGAMEOPTION_CANNOT_CLAIM_OCEAN) && pPlot->isWater())
		{
			return false;
		}

		/* cannot claim plots adjacent to someone else's city */
		if (pPlot->getAdjacentCity(pPlot->getOwner()) != NULL)
		{
			return false;
		}

		if (!pPlot->isOwned() || potentialWarAction(pPlot))
		{
			return true;
		}
		return false;
	}
	return true;
}

bool CvUnit::claimTerritory()
{
	//logging::logMsg("C2C.log", "%S claims territory from %S at (%d, %d)", GET_PLAYER(getOwner()).getCivilizationShortDescription(), GET_PLAYER(plot()->getOwner()).getCivilizationShortDescription(), getX(), getY());
	CvPlot* pPlot = plot();

	if (!canClaimTerritory(pPlot))
	{
		return false;
	}
	PlayerTypes pPlayerThatLostTerritory = NO_PLAYER;

	if (pPlot->isOwned())
	{
		pPlayerThatLostTerritory = pPlot->getOwner();
		TeamTypes tTeamThatLostTerritory = GET_PLAYER(pPlayerThatLostTerritory).getTeam();

		GET_TEAM(tTeamThatLostTerritory).changeWarWeariness(getTeam(), *pPlot, GC.getDefineINT("WW_PLOT_CAPTURED"));
		GET_TEAM(getTeam()).changeWarWeariness(tTeamThatLostTerritory, *pPlot, GC.getDefineINT("WW_CAPTURED_PLOT"));
		GET_TEAM(getTeam()).AI_changeWarSuccess(tTeamThatLostTerritory, GC.getDefineINT("WAR_SUCCESS_PLOT_CAPTURING"));
	}
	pPlot->setClaimingOwner(getOwner());

	if (pPlayerThatLostTerritory != NO_PLAYER)
	{
		CvWString szBuffer;
		CvCity *pNearestCity = GC.getMap().findCity(pPlot->getX(), pPlot->getY(), pPlayerThatLostTerritory, NO_TEAM, false);

		if (pNearestCity != NULL)
		{
			szBuffer = gDLL->getText("TXT_KEY_MISSION_CLAIM_TERRITORY_CIV_SUCCESS_NEAR", GET_PLAYER(getOwner()).getCivilizationAdjectiveKey(), pNearestCity->getName().GetCString());
		}
		else
		{
			szBuffer = gDLL->getText("TXT_KEY_MISSION_CLAIM_TERRITORY_CIV_SUCCESS", GET_PLAYER(getOwner()).getCivilizationAdjectiveKey());
		}
		AddDLLMessage(pPlayerThatLostTerritory, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, GC.getEraInfo(GC.getGame().getCurrentEra()).getAudioUnitDefeatScript(), MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), pPlot->getX(), pPlot->getY());
	}
	finishMoves();

	return true;
}

int CvUnit::surroundedDefenseModifier(const CvPlot *pPlot, const CvUnit *pDefender) const
{
	PROFILE_EXTRA_FUNC();
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY))
	{
		return 0;
	}
	const DirectionTypes dtDirectionAttacker = directionXY(pPlot, plot());
	int iExtraModifier = 0;
	//TB Combat Mods Begin (Enclose)
	int iEnclose = 0;
	//TB Combat Mods End
	const bool bAttackerHN = isHiddenNationality();
	const bool bActAsHominid = isHominid() || isBarbCoExist();
	const bool bSeaCombat = pPlot->isWater();

	for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		if (iI != dtDirectionAttacker)
		{
			const CvPlot* plotX = plotDirection(pPlot, static_cast<DirectionTypes>(iI));

			if (plotX != NULL && bSeaCombat == plotX->isWater())
			{
				const CvUnit* pBestUnit = NULL;
				int iLowestCurrCombatStr = INT_MAX;

				foreach_(const CvUnit* unitX, plotX->units())
				{
					if (unitX->getTeam() == getTeam()
					&& bAttackerHN == unitX->isHiddenNationality()
					&& unitX->canAttack(*pDefender)
					&& unitX->canEnterPlot(pPlot, MoveCheck::IgnoreAttack))
					{
						iEnclose += unitX->encloseTotal();

						const int iTmpCurrCombatStrOnly = pDefender->currCombatStr(NULL, unitX, NULL, false);
						const int iTmpCurrCombatStr = std::max(1, iTmpCurrCombatStrOnly - iTmpCurrCombatStrOnly * unitX->unnerveTotal() / 100);

						if (iTmpCurrCombatStr < iLowestCurrCombatStr)
						{
							iLowestCurrCombatStr = iTmpCurrCombatStr;
							pBestUnit = unitX;
						}
					}
				}

				if (pBestUnit != NULL)
				{
					const double fAttDeffFactor = static_cast<double>(pBestUnit->currCombatStr(pPlot, pBestUnit, NULL, false)) / iLowestCurrCombatStr;

					if (fAttDeffFactor != 0)
					{
						/* surrounding distance = 1, 2, 3 or 4; the bigger the better */
						int iSurroundingDistanceFactor;

						switch (abs(std::min(abs(iI - dtDirectionAttacker), abs(abs(iI - dtDirectionAttacker) - 8))))
						{
							case 1:
								iSurroundingDistanceFactor = GC.getSAD_FACTOR_1();
								break;
							case 2:
								iSurroundingDistanceFactor = GC.getSAD_FACTOR_2();
								break;
							case 3:
								iSurroundingDistanceFactor = GC.getSAD_FACTOR_3();
								break;
							case 4:
								iSurroundingDistanceFactor = GC.getSAD_FACTOR_4();
								break;
							default:
								iSurroundingDistanceFactor = GC.getSAD_FACTOR_1();
								break;
						}
						if (fAttDeffFactor == 1)
						{
							iExtraModifier += iSurroundingDistanceFactor;
						}
						else
						{
							iExtraModifier += int(iSurroundingDistanceFactor * ((fAttDeffFactor - 1.0) * pow(abs(fAttDeffFactor - 1.0), -0.75) + 1.0));
						}
					}
				}
			}
		}
	}
	//TB Combat Mods Begin (SAD mods)
	const int iSurroundModifier = std::min(GC.getSAD_MAX_MODIFIER() + iEnclose, iExtraModifier);

	return iSurroundModifier + iSurroundModifier * lungeTotal() / 100;
	//TB Combat Mods End (SAD mods)
}


bool CvUnit::isCanMovePeaks() const
{
	return m_iCanMovePeaksCount > 0;
}

void CvUnit::changeCanMovePeaksCount(int iChange)
{
	m_iCanMovePeaksCount += iChange;
	FASSERT_NOT_NEGATIVE(m_iCanMovePeaksCount);
}

// Koshling - enhanced mountaineering mode to differentiate between ability to move through
//	mountains, and ability to lead a stack through mountains
bool CvUnit::isCanLeadThroughPeaks() const
{
	return m_iCanLeadThroughPeaksCount > 0;
}

void CvUnit::changeCanLeadThroughPeaksCount(int iChange)
{
	m_iCanLeadThroughPeaksCount += iChange;
	FASSERT_NOT_NEGATIVE(m_iCanLeadThroughPeaksCount);
}


int CvUnit::getMaxHurryFood() const
{
	return std::max(0, m_pUnitInfo->getFoodBase() * CvGameSpeedScale::hammerCostPercent() / 100);
}

int CvUnit::getHurryFood(const CvPlot* pPlot) const
{
	const CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL) return 0;

	return std::max(0, std::min(pCity->growthThreshold() - pCity->getFood(), getMaxHurryFood()));
}

bool CvUnit::canHurryFood(const CvPlot* pPlot) const
{
	if (isDelayedDeath() || getHurryFood(pPlot) == 0)
	{
		return false;
	}

	const CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL || pCity->getOwner() != getOwner() || pCity->getFoodTurnsLeft() == 1)
	{
		return false;
	}
	return true;
}


bool CvUnit::hurryFood()
{
	if (!canHurryFood(plot()))
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity != NULL)
	{
		pCity->changeFood(getHurryFood(plot()));
	}

	if (plot()->isActiveVisible(false))
	{
		NotifyEntity(MISSION_HURRY_FOOD);
	}

	kill(true, NO_PLAYER, true);

	return true;
}

bool CvUnit::sleepForEspionage()
{
	if (!canSleep() || !canEspionage(plot(), true) || getFortifyTurns() == GC.getMAX_FORTIFY_TURNS())
	{
		return false;
	}
	m_iSleepTimer = 1;

	return true;
}


UnitCompCommander* CvUnit::getCommanderComp() const
{
	return m_commander;
}

bool CvUnit::isCommander() const
{
	return m_commander != NULL;
}

bool CvUnit::isCommanderReady() const
{
	return m_commander ? m_commander->isReady() : false;
}

void CvUnit::setCommander(bool bNewVal)
{
	PROFILE_EXTRA_FUNC();
	if (isCommander() == bNewVal || getDomainType() == DOMAIN_SEA) return;

	if (bNewVal)
	{
		m_commander = new UnitCompCommander(this, m_pUnitInfo);

		foreach_(const int iSubCombat, m_pUnitInfo->getCombatClasses())
		{
			const UnitCombatTypes eSubCombat = (UnitCombatTypes)iSubCombat;
			if (GC.getUnitCombatInfo(eSubCombat).getSizeMatters().qualityBase > -10)
			{
				setHasUnitCombat(eSubCombat, false);
			}
		}
		plot()->countCommander(true, this);
	}
	else
	{
		if (m_commander->isReady())
		{
			plot()->countCommander(false, this);
		}
		delete m_commander;
		m_commander = NULL;
	}
	GET_PLAYER(getOwner()).listCommander(bNewVal, this);
}


CvUnit* CvUnit::getCommander() const
{
	PROFILE_FUNC();

	const CvPlot* pPlot = plot();
	if (pPlot == NULL || !pPlot->inCommandField(getOwner()) || getDomainType() == DOMAIN_SEA)
	{
		return NULL;
	}

	CvUnit* pBestCommander = getLastCommander();
	if (pBestCommander)
	{
		const int cachedDistance = plotDistance(pBestCommander->getX(), pBestCommander->getY(), getX(), getY());
		if (cachedDistance <= pBestCommander->getCommanderComp()->getCommandRange())
		{
			return pBestCommander;
		}
		pBestCommander = NULL;
	}

	int iBestCommanderDistance = std::numeric_limits<int>::max();
	int iBestCommanderXP = -1;

	const CvPlayer& player = GET_PLAYER(getOwner());
	const std::vector<CvUnit*>& commanders = player.getCommanders();

	for (std::vector<CvUnit*>::const_iterator it = commanders.begin(); it != commanders.end(); ++it)
	{
		CvUnit* com = *it;
		UnitCompCommander* comComp = com->getCommanderComp();
		if (comComp == NULL)
			continue;  // s�curit� si jamais �a renvoie NULL
		if (comComp->getControlPointsLeft() <= 0)
			continue;

		const CvPlot* comPlot = com->plot();
		FAssertMsg(comPlot != NULL, "Unexpected... CTD incoming");

		const int iDistance = plotDistance(comPlot->getX(), comPlot->getY(), getX(), getY());
		if (iDistance > comComp->getCommandRange())
			continue;

		const int iXP = com->getExperience();
		if (
			pBestCommander == NULL ||
			iDistance < iBestCommanderDistance ||
			(iDistance == iBestCommanderDistance && iXP > iBestCommanderXP)
		)
		{
			pBestCommander = com;
			iBestCommanderDistance = iDistance;
			iBestCommanderXP = iXP;
			if (iDistance == 0) break; // Early exit: best possible
		}
	}
	m_iCommanderID = pBestCommander ? pBestCommander->getID() : -1;
	return pBestCommander;
}

void CvUnit::tryUseCommander()
{
	CvUnit* pCommander = getCommander();

	if (pCommander) //commander is used when any unit under his command fights in combat
	{
		m_iUsedCommanderID = pCommander->getID();

		pCommander->m_commander->changeControlPointsLeft(-1);

		if (!pCommander->m_commander->isReady())
		{
			FlushCombatStrCache(NULL);
			nullLastCommander();
		}
	}
}

void CvUnit::nullLastCommander()
{
	m_iCommanderID = -1;
}

// This only exist during combat with the purpose of remembering what commander should get exp.
CvUnit* CvUnit::getUsedCommander() const
{
	return (m_iUsedCommanderID == -1 ? NULL : GET_PLAYER(getOwner()).getUnit(m_iUsedCommanderID));
}

// This ties a commander to this unit for as long as said commander is valid;
//	it cease to be valid mid combat when it expends its last CP.
CvUnit* CvUnit::getLastCommander() const
{
	return (m_iCommanderID == -1 ? NULL : GET_PLAYER(getOwner()).getUnit(m_iCommanderID));
}


UnitCompCommodore* CvUnit::getCommodoreComp() const
{
	return m_commodore;
}

bool CvUnit::isCommodore() const
{
	return m_commodore != NULL;
}

bool CvUnit::isCommodoreReady() const
{
	return m_commodore ? m_commodore->isReady() : false;
}

void CvUnit::setCommodore(bool bNewVal)
{
	PROFILE_EXTRA_FUNC();
	if (isCommodore() == bNewVal) return;

	if (bNewVal)
	{
		m_commodore = new UnitCompCommodore(this, m_pUnitInfo);

		foreach_(const int iSubCombat, m_pUnitInfo->getCombatClasses())
		{
			const UnitCombatTypes eSubCombat = (UnitCombatTypes)iSubCombat;
			if (GC.getUnitCombatInfo(eSubCombat).getSizeMatters().qualityBase > -10)
			{
				setHasUnitCombat(eSubCombat, false);
			}
		}
		plot()->countCommodore(true, this);
	}
	else
	{
		if (m_commodore->isReady())
		{
			plot()->countCommodore(false, this);
		}
		delete m_commodore;
		m_commodore = NULL;
	}
	GET_PLAYER(getOwner()).listCommodore(bNewVal, this);
}

CvUnit* CvUnit::getCommodore() const
{
	PROFILE_FUNC();

	const CvPlot* pPlot = plot();
	if (pPlot == NULL || !pPlot->inCommandCommodoreField(getOwner()) || getDomainType() == DOMAIN_LAND)
	{
		return NULL;
	}

	CvUnit* pBestCommodore = getLastCommodore();
	if (pBestCommodore)
	{
		const int cachedDistance = plotDistance(pBestCommodore->getX(), pBestCommodore->getY(), getX(), getY());
		if (cachedDistance <= pBestCommodore->getCommodoreComp()->getCommandRange())
		{
			return pBestCommodore;
		}
		// The one we used would have been the cached one so will have to search again
		pBestCommodore = NULL;
	}

	int iBestCommodoreDistance = std::numeric_limits<int>::max();
	int iBestCommodoreXP = -1;

	const CvPlayer& player = GET_PLAYER(getOwner());
	const std::vector<CvUnit*>& commodores = player.getCommodores();

	for (std::vector<CvUnit*>::const_iterator it = commodores.begin(); it != commodores.end(); ++it)
	{
		CvUnit* com = *it;
		UnitCompCommodore* comComp = com->getCommodoreComp();
		if (comComp == NULL)
			continue;  // s�curit� si jamais �a renvoie NULL
		if (comComp->getControlPointsLeft() <= 0)
			continue;

		const CvPlot* comPlot = com->plot();
		FAssertMsg(comPlot != NULL, "Unexpected... CTD incoming");

		const int iDistance = plotDistance(comPlot->getX(), comPlot->getY(), getX(), getY());
		if (iDistance > comComp->getCommandRange())
			continue;

		const int iXP = com->getExperience();
		if (
			pBestCommodore == NULL ||
			iDistance < iBestCommodoreDistance ||
			(iDistance == iBestCommodoreDistance && iXP > iBestCommodoreXP)
		)
		{
			pBestCommodore = com;
			iBestCommodoreDistance = iDistance;
			iBestCommodoreXP = iXP;
			if (iDistance == 0) break; // Early exit: best possible
		}
	}
	m_iCommodoreID = pBestCommodore ? pBestCommodore->getID() : -1;
	return pBestCommodore;

}

void CvUnit::tryUseCommodore()
{
	CvUnit* pCommodore = getCommodore();

	if (pCommodore) //commodore is used when any unit under his command fights in combat
	{
		m_iUsedCommodoreID = pCommodore->getID();

		pCommodore->m_commodore->changeControlPointsLeft(-1);

		if (!pCommodore->m_commodore->isReady())
		{
			FlushCombatStrCache(NULL);
			nullLastCommodore();
		}
	}
}

void CvUnit::nullLastCommodore()
{
	m_iCommodoreID = -1;
}

// This only exist during combat with the purpose of remembering what commodore should get exp.
CvUnit* CvUnit::getUsedCommodore() const
{
	return (m_iUsedCommodoreID == -1 ? NULL : GET_PLAYER(getOwner()).getUnit(m_iUsedCommodoreID));
}

// This ties a commodore to this unit for as long as said commodore is valid;
//	it cease to be valid mid combat when it expends its last CP.
CvUnit* CvUnit::getLastCommodore() const
{
	return (m_iCommodoreID == -1 ? NULL : GET_PLAYER(getOwner()).getUnit(m_iCommodoreID));
}


int CvUnit::interceptionChance(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iNoInterceptionChanceTimes100 = 10000;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && !isInvisible(GET_PLAYER((PlayerTypes)iI).getTeam(), false, false))
		{
			foreach_(const CvUnit* pLoopUnit, GET_PLAYER((PlayerTypes)iI).units())
			{
				if (pLoopUnit->canAirDefend() && !pLoopUnit->isMadeInterception() && isEnemy(pLoopUnit->getTeam(), NULL, pLoopUnit)
				&& (pLoopUnit->getDomainType() != DOMAIN_AIR || !pLoopUnit->hasMoved() && pLoopUnit->getGroup()->getActivityType() == ACTIVITY_INTERCEPT)
				&& plotDistance(pLoopUnit->getX(), pLoopUnit->getY(), pPlot->getX(), pPlot->getY()) <= pLoopUnit->airRange())
				{
					const int iValue = pLoopUnit->currInterceptionProbability();

					if (iValue > 0)
					{
						if (iValue > 99) return 100;

						iNoInterceptionChanceTimes100 *= 100 - iValue;
						iNoInterceptionChanceTimes100 /= 100;
						if (iNoInterceptionChanceTimes100 < 100)
						{
							return 100;
						}
					}
				}
			}
		}
	}
	return 100 - iNoInterceptionChanceTimes100 / 100;
}

PlayerTypes CvUnit::getOriginalOwner() const
{
	return m_eOriginalOwner;
}

void CvUnit::doBattleFieldPromotions(CvUnit* pDefender, const CombatDetails& cdDefenderDetails, const CvPlot* pPlot, bool bAttackerHasLostNoHP, bool bAttackerWithdrawn, int iAttackerInitialDamage, int iWinningOdds, int iInitialAttXP, int iInitialAttGGXP, int iDefenderInitialDamage, int iInitialDefXP, int iInitialDefGGXP, bool& bAttackerPromoted, bool& bDefenderPromoted, int iNonLethalAttackWinChance, int iNonLethalDefenseWinChance, int iDefenderFirstStrikes, int iAttackerFirstStrikes)
{
	PROFILE_EXTRA_FUNC();
	if (!GC.getGame().getModderGameOption(MODDERGAMEOPTION_BATTLEFIELD_PROMOTIONS) ||
		getUnitCombatType() == NO_UNITCOMBAT || pDefender->getUnitCombatType() == NO_UNITCOMBAT)
	{
		return;
	}

	bool bNoDefBon = noDefensiveBonus();
	std::vector<PromotionTypes> aAttackerAvailablePromotions;
	std::vector<PromotionTypes> aDefenderAvailablePromotions;
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		const PromotionTypes promotionType = static_cast<PromotionTypes>(iI);
		const CvPromotionInfo& kPromotion = GC.getPromotionInfo(promotionType);
		/* Block These Promotions */
		if (kPromotion.getCombatModifier(COMBAT_KAMIKAZE, CASC_SCOPE_UNIT) > 0 ||
			kPromotion.isLeader() ||
			kPromotion.hasNegativeEffects())
		{
			continue;
		}

		//TB Combat Mods Begin
		if (pDefender->isDead())
		{
			if (!canAcquirePromotion(promotionType)) //attacker can not acquire this promotion
			{
				continue;
			}
			//* attacker was attacking with S&D bonus
			if (kPromotion.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT) > 0 && surroundedDefenseModifier(pPlot, pDefender) != 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//TB Combat Mods End
			//* attacker was crossing river
			if (kPromotion.providesSkill(CLS_SKILL_RIVER) && cdDefenderDetails.iRiverAttackModifier != 0)	//this bonus is being applied to defender
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* attack from water
			if (kPromotion.providesSkill(CLS_SKILL_AMPHIB) && cdDefenderDetails.iAmphibAttackModifier != 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* attack terrain
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, (int)pPlot->getTerrainType(), COMBAT_ATTACK) > 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* attack feature
			if (pPlot->getFeatureType() != NO_FEATURE &&
				InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, (int)pPlot->getFeatureType(), COMBAT_ATTACK) > 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* attack hills
			if (kPromotion.getCombatModifier(COMBAT_HILLS_ATTACK, CASC_SCOPE_UNIT) > 0 && pPlot->isHills())
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* attack city
			if (kPromotion.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT) > 0 && pPlot->isCity(true))	//count forts too
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* first strikes/chanses promotions
			if ((kPromotion.getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100 > 0 ||
				kPromotion.getScalar(SCALAR_FIRST_STRIKE_CHANCES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) > 0) && (firstStrikes() > 0 || chanceFirstStrikes() > 0))
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* unit combat mod
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT, (int)pDefender->getUnitCombatType(), COMBAT_AMOUNT) > 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//TB Combat Mods Begin * anti-barbarian combat mod
			if (kPromotion.getCombatModifier(COMBAT_VS_BARBS, CASC_SCOPE_UNIT) > 0 && (pDefender->isHominid()))
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			if (kPromotion.getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT) > 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			if (kPromotion.getCombatModifier(COMBAT_ATTACK, CASC_SCOPE_UNIT) > 0)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//TB Combat Mods End
			//* combat strength promotions
			if (kPromotion.getCombatModifier(COMBAT_AMOUNT, CASC_SCOPE_UNIT) > 0 && !kPromotion.providesSkill(CLS_SKILL_AMPHIB))
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* domain mod
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN, (int)pDefender->getDomainType(), COMBAT_AMOUNT))
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
			//* blitz
			if (kPromotion.providesSkill(CLS_SKILL_BLITZ) && bAttackerHasLostNoHP)
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
		}	//if defender is dead or withdrawn
		//* defender withdrawn, give him withdrawal promo
		else if (m_combatResult.bDefenderWithdrawn)
		{
			if (kPromotion.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) > 0 &&
				pDefender->canAcquirePromotion(promotionType))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
		}
		//* attacker withdrawn
		else if (bAttackerWithdrawn)
		{
			if (kPromotion.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) > 0 &&
				canAcquirePromotion(promotionType))
			{
				aAttackerAvailablePromotions.push_back(promotionType);
			}
		}
		//* attacker is presumably dead
		else
		{
			FAssertMsg(isDead(), "Attacker is expected to be dead");
			if (!pDefender->canAcquirePromotion(promotionType))
			{
				continue;
			}
			//TB Combat Mods Begin
			//* Defender Suffered Surround and Destroy Modifier
			if (kPromotion.getCombatModifier(COMBAT_DYNAMIC_DEFENSE, CASC_SCOPE_UNIT) > 0 && surroundedDefenseModifier(pPlot, pDefender) != 0)
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			if (kPromotion.getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT) > 0)
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			if (kPromotion.providesSkill(CLS_SKILL_ANIMAL_IGNORES_BORDERS) && pDefender->isAnimal() && !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//TB Combat Mods End
			//* defend terrain
			if (!noDefensiveBonus() && (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, (int)pPlot->getTerrainType(), COMBAT_DEFENSE) > 0))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//* defend feature
			if (!noDefensiveBonus() && (pPlot->getFeatureType() != NO_FEATURE &&
				InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, (int)pPlot->getFeatureType(), COMBAT_DEFENSE) > 0))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//* defend hills
			if (!noDefensiveBonus() && (kPromotion.getCombatModifier(COMBAT_HILLS_DEFENSE, CASC_SCOPE_UNIT) > 0 && pPlot->isHills()))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//* defend city
			if (!noDefensiveBonus() && kPromotion.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT) > 0 && pPlot->isCity(true))	//count forts too
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//* first strikes/chanses promotions
			if ((kPromotion.getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100 > 0 ||
				kPromotion.getScalar(SCALAR_FIRST_STRIKE_CHANCES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) > 0) &&
				(pDefender->firstStrikes() > 0 || pDefender->chanceFirstStrikes() > 0))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//* unit combat mod vs attacker unit type
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT, (int)getUnitCombatType(), COMBAT_AMOUNT) > 0)
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//TB Combat Mods Begin * anti-barbarian combat mod
			if (kPromotion.getCombatModifier(COMBAT_VS_BARBS, CASC_SCOPE_UNIT) > 0 && isHominid())
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}

			if (!noDefensiveBonus() && kPromotion.getCombatModifier(COMBAT_DEFENSE, CASC_SCOPE_UNIT) > 0)
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//TB Combat Mods End
			//* combat strength promotions
			if (kPromotion.getCombatModifier(COMBAT_AMOUNT, CASC_SCOPE_UNIT) > 0)
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
			//* domain mod
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN, (int)getDomainType(), COMBAT_AMOUNT))
			{
				aDefenderAvailablePromotions.push_back(promotionType);
			}
		}	//if attacker withdrawn
	}	//end promotion types cycle

	//promote attacker:
	if (!isDead() && aAttackerAvailablePromotions.size() > 0)
	{
		FAssertMsg(getMaxHP() - iAttackerInitialDamage > 0, "Attacker is Dead!");
		int iHealthPercent = (getMaxHP() - getDamage()) * 100 / std::max(1, getMaxHP() - iAttackerInitialDamage);
		iNonLethalAttackWinChance *= 10;
		int iOdds = std::max(iWinningOdds, iNonLethalAttackWinChance);
		int iPromotionChance = (GC.getCOMBAT_DIE_SIDES() - iOdds)/* * (100 + iPromotionChanceModifier) / 100*/;

		int iFirstStrikes = 1 + iAttackerFirstStrikes;
		iFirstStrikes = std::max(1, iFirstStrikes);

		iPromotionChance /= iFirstStrikes;
		if (GC.getGame().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "Occasional Promotion") < iPromotionChance)
		{
			//select random promotion from available
			PromotionTypes ptPromotion = aAttackerAvailablePromotions[
				GC.getGame().getSorenRandNum(aAttackerAvailablePromotions.size(), "Select Promotion Type")];
			//promote
			//TB Combat Mod next line
			setHasPromotion(ptPromotion, true, false);
			bAttackerPromoted = true;

			setExperience100(iInitialAttXP);
			GET_PLAYER(getOwner()).setCombatExperience(iInitialAttGGXP, getGGExperienceEarnedTowardsType());

			//show message
			const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_PROMOTED_IN_BATTLE", getNameKey(), GC.getPromotionInfo(ptPromotion).getText());
			AddDLLMessage(
				getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer,
				GC.getPromotionInfo((PromotionTypes)0).getSound(), MESSAGE_TYPE_INFO, NULL,
				GC.getCOLOR_GREEN(), getX(), getY()
			);
		}
	}

	//promote defender:
	if (!pDefender->isDead() && aDefenderAvailablePromotions.size() > 0)
	{
		FAssertMsg(pDefender->getMaxHP() - iDefenderInitialDamage > 0, "Defender is Dead!");
		int iHealthPercent = (pDefender->getMaxHP() - pDefender->getDamage()) * 100 / std::max(1, pDefender->getMaxHP() - iDefenderInitialDamage);
		iNonLethalDefenseWinChance *= 10;
		iNonLethalDefenseWinChance = std::max(0, (GC.getCOMBAT_DIE_SIDES() - iNonLethalDefenseWinChance));
		int iOdds = std::min(iWinningOdds, iNonLethalDefenseWinChance);
		int iPromotionChance = iOdds/* * (100 + iPromotionChanceModifier) / 100*/;
		int iFirstStrikes = 1 + iDefenderFirstStrikes;
		//change to stealth strikes if stealth combat
		iFirstStrikes = std::max(1, iFirstStrikes);
		iPromotionChance /= iFirstStrikes;
		if (GC.getGame().getSorenRandNum(GC.getCOMBAT_DIE_SIDES(), "Occasional Promotion") < iPromotionChance)
		{
			//select random promotion from available
			PromotionTypes ptPromotion = aDefenderAvailablePromotions[
				GC.getGame().getSorenRandNum(aDefenderAvailablePromotions.size(), "Select Promotion Type")];
			//promote
			//TB Combat Mod next line
			pDefender->setHasPromotion(ptPromotion, true, false);

			pDefender->setExperience100(iInitialDefXP);
			GET_PLAYER(pDefender->getOwner()).setCombatExperience(iInitialDefGGXP, pDefender->getGGExperienceEarnedTowardsType());
			bDefenderPromoted = true;

			//show message
			{

				CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_YOUR_UNIT_PROMOTED_IN_BATTLE", pDefender->getNameKey(),
					GC.getPromotionInfo(ptPromotion).getText());
				AddDLLMessage(
					pDefender->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer,
					GC.getPromotionInfo((PromotionTypes)0).getSound(), MESSAGE_TYPE_INFO, NULL,
					GC.getCOLOR_GREEN(), pPlot->getX(), pPlot->getY());
			}
		}
	}
}


void CvUnit::doDynamicXP(CvUnit* pDefender, const CvPlot* pPlot, int iAttackerInitialDamage, int iAttackerWinOdds, int iDefenderInitialDamage, bool bPromotion, bool bDefPromotion)
{
	if (!isDead() && !pDefender->isDead())
	{
		// Combat aborted before conclusion, withdrawals and the like.
		if (!bPromotion && attackXPValue() > 0)
		{
			applyDynamicXP(
				getEngagementDynamicXP(
					pDefender, GC.getCOMBAT_DIE_SIDES() - iAttackerWinOdds,
					iDefenderInitialDamage,
					iAttackerInitialDamage,
					50 * defenseXPValue()
				),
				pPlot->getOwner() == getOwner(),
				100 * maxXPValue(this)
			);
		}
		if (!bDefPromotion && pDefender->defenseXPValue() > 0)
		{
			pDefender->applyDynamicXP(
				pDefender->getEngagementDynamicXP(
					this, iAttackerWinOdds,
					iAttackerInitialDamage,
					iDefenderInitialDamage,
					50 * pDefender->defenseXPValue()
				),
				pPlot->getOwner() == pDefender->getOwner(),
				100 * pDefender->maxXPValue(this)
			);
		}
	}
	else if (!bPromotion && !isDead() && attackXPValue() > 0)
	{
		applyDynamicXP(
			getVanquishDynamicXP(
				GC.getCOMBAT_DIE_SIDES() - iAttackerWinOdds, iAttackerInitialDamage, 100 * attackXPValue()
			),
			pPlot->getOwner() == getOwner(),
			100 * maxXPValue(pDefender)
		);
	}
	else if (!bDefPromotion && !pDefender->isDead() && pDefender->defenseXPValue() > 0)
	{
		pDefender->applyDynamicXP(
			pDefender->getVanquishDynamicXP(
				iAttackerWinOdds, iDefenderInitialDamage, 100 * pDefender->defenseXPValue()
			),
			pPlot->getOwner() == pDefender->getOwner(),
			100 * pDefender->maxXPValue(this)
		);
	}
}

int CvUnit::getEngagementDynamicXP(const CvUnit* enemy, const int iLoseOdds, const int iInitialDamageEnemy, const int iInitialDamage, const int iMaxXP) const
{
	const int iHealthPercentLost = 1000 - (getMaxHP() - getDamage()) * 1000 / (getMaxHP() - iInitialDamage);
	const int iHealthPercentLostEnemy = 1000 - (enemy->getMaxHP() - enemy->getDamage()) * 1000 / (enemy->getMaxHP() - iInitialDamageEnemy);
	const int iMod = iHealthPercentLost + 3*iHealthPercentLostEnemy + 6*iLoseOdds;

	// Damage done, modified by chance to lose and damage received.
	return range((iHealthPercentLostEnemy + 1) * (iLoseOdds + 1) * (1000 + iMod) / 1000000, 0, iMaxXP);
}

int CvUnit::getVanquishDynamicXP(const int iLoseOdds, const int iInitialDamage, const int iMaxXP) const
{
	const int iMinXP = 1 + std::min(GC.getGame().getSorenRandNum(15, ""), iMaxXP);
	const int iMaxHP = getMaxHP();
	const int iHealthPercentLost = 1000 - (iMaxHP - getDamage()) * 1000 / (iMaxHP - iInitialDamage);

	FAssertMsg(iMaxHP - iInitialDamage > 0, "Applying exp to a dead unit!");

	// First factor of 10 means that if the unit lose 99.9% of its max life it will get 10 times more XP.
	const int iMod = 10 * iHealthPercentLost * iMaxHP / (iMaxHP + 10 * iInitialDamage);

	// Chance of losing, modified by hardship of winning,
	//	where an outworn unit doesn't learn as much from hardship as a combat ready unit.
	return range(iLoseOdds * (1000 + iMod) / 1000, iMinXP, iMaxXP);
}

void CvUnit::applyDynamicXP(const int iExperience, const bool bHomeTerritory, int iMaxTotalXP)
{
	// Toffer - Allow a little xp against animals and barbs if above the max with dynamic XP.
	//	The diminshing return of dynamic XP makes it somewhat of a natural xp limiter against weak foes even before reaching the limit.
	if (iMaxTotalXP > -1 && getExperience100() + iExperience > iMaxTotalXP)
	{
		iMaxTotalXP = std::max(iMaxTotalXP, getExperience100() + iExperience / 10);
	}
	changeExperience100(iExperience, iMaxTotalXP, true, bHomeTerritory, true);
}



int CvUnit::getZoneOfControlCount() const
{
	return m_iZoneOfControlCount;
}

bool CvUnit::isZoneOfControl() const
{
	return (getZoneOfControlCount() > 0);
}

void CvUnit::changeZoneOfControlCount(int iChange)
{
	m_iZoneOfControlCount += iChange;
	if (isZoneOfControl())
	{
		GC.getGame().toggleAnyoneHasUnitZoneOfControl();
	}
	//TB Combat Mod Debug
	FASSERT_NOT_NEGATIVE(getZoneOfControlCount());
}

bool CvUnit::isAutoPromoting() const
{
	return m_bAutoPromoting;
}
void CvUnit::setAutoPromoting(bool bNewValue)
{
	m_bAutoPromoting = bNewValue;
	if (bNewValue)
	{
		//Force recalculation
		setPromotionReady(false);
		testPromotionReady();
	}
}

bool CvUnit::isAutoUpgrading() const
{
	return m_bAutoUpgrading;
}

void CvUnit::setAutoUpgrading(bool bNewValue)
{
	m_bAutoUpgrading = bNewValue;
}


bool CvUnit::canShadow() const
{
	if (!canAttack())
	{
		return false;
	}

	if (GET_PLAYER(getOwner()).isModderOption(MODDEROPTION_HIDE_AUTO_PROTECT))
	{
		return false;
	}

	return true;
}

bool CvUnit::canShadowAt(const CvPlot* pShadowPlot, CvUnit* pShadowUnit) const
{
	if (!canShadow() || !pShadowPlot)
	{
		return false;
	}

	if (!pShadowUnit)
	{
		pShadowUnit = pShadowPlot->getCenterUnit(false);
	}
	if (!pShadowUnit
	|| pShadowUnit == this
	|| pShadowUnit->getTeam() != getTeam()
	|| pShadowUnit->baseMoves() > baseMoves())
	{
		return false;
	}

	int iPathTurns;
	if (!generatePath(pShadowPlot, 0, true, &iPathTurns))
	{
		return false;
	}
	return true;
}

CvUnit* CvUnit::getShadowUnit() const
{
	return getUnit(m_shadowUnit);
}


void CvUnit::setShadowUnit(const CvUnit* pUnit)
{
	if (pUnit != NULL)
	{
		m_shadowUnit = pUnit->getIDInfo();
	}
	else
	{
		m_shadowUnit.reset();
	}
}


CvProperties* CvUnit::getProperties()
{
	return &m_Properties;
}

const CvProperties* CvUnit::getPropertiesConst() const
{
	return &m_Properties;
}

void CvUnit::addMission(const CvMissionDefinition& mission)
{
	if (mission.isValid())
	{
		gDLL->getEntityIFace()->AddMission(&mission);
	}
}

bool CvUnit::isArcher() const
{
	return isHasUnitCombat(GC.getUNITCOMBAT_ARCHER());
}

//TB Combat Mods begin
//	Is this promotion SUPERSEDED by a higher rung of its own line that the unit also holds?
//
//	⛔ A BARE FETCH of the resolved set, folded when the promotion landed. It used to sweep the WHOLE promotion
//	registry per ask -- and the unit panel asks it once per promotion the unit carries, on every redraw, so the
//	cost was `held x REGISTRY` and grew QUADRATICALLY with promotions held while an unpromoted unit paid nothing.
//	That is the own-data inversion ([DEC-one-reverse-view]): the answer needs only what the unit HOLDS, and the
//	unit already enumerates that.
bool CvUnit::isPromotionOverriden(PromotionTypes ePromotionType) const
{
	return m_resolvedValues.isPromotionOverridden((int)ePromotionType);
}




void CvUnit::checkPromotionObsoletion()
{
	PROFILE_FUNC();

	if ((isCommander())||(isCommodore()))
	{
		for (int iI = GC.getNumUnitCombatInfos() - 1; iI > -1; iI--)
		{
			const UnitCombatTypes eUnitCombatX = static_cast<UnitCombatTypes>(iI);
			if (
				isHasUnitCombat(eUnitCombatX)
			&&	(
					GC.getUnitCombatInfo(eUnitCombatX).getSizeMatters().groupBase > -10
					||
					GC.getUnitCombatInfo(eUnitCombatX).getSizeMatters().qualityBase > -10
				)
			)
			{
				setHasUnitCombat(eUnitCombatX, false);
			}
		}
	}

	while (true)
	{
		bool bRemovalMade = false;
		for (int iI = GC.getNumPromotionInfos() - 1; iI > -1; iI--)
		{
			const PromotionTypes ePromotion = static_cast<PromotionTypes>(iI);
			const CvPromotionInfo& promotionInfo = GC.getPromotionInfo(ePromotion);
			bool bPromo = true;
			bool bPromotionFree = isPromotionFree(ePromotion);

			if (isHasPromotion(ePromotion) && !canKeepPromotion(ePromotion, bPromotionFree, true))
			{
				if (bPromotionFree)
				{
					setHasPromotion(ePromotion, false, true);
					bRemovalMade = true;
				}
				else if (bPromo)
				{
					//	The retrain mechanism relies on knowing if a promotion was free or not, but in
					//	saves from older versions we don't have that information, and many promotions that
					//	actually were free will not be flagged as such.  In such cases you'll get to retrain
					//	things you really shouldn't, but we don't allow more total retrains than your level

					changeRetrainsAvailable(1);
					setHasPromotion(ePromotion, false, false);
					bRemovalMade = true;
				}
			}
		}
		if (!bRemovalMade)
		{
			break;
		}
	}
}



bool CvUnit::canKeepPromotion(PromotionTypes ePromotion, bool bAssertFree, bool bMessageOnFalse) const
{
	PROFILE_FUNC();

	if (ePromotion == NO_PROMOTION)
	{
		FErrorMsg("Invalid promotion");
		return false;
	}
	bool bPromo = false;

	const CvPromotionInfo& promo = GC.getPromotionInfo(ePromotion);

	bPromo = true;

	const bool bIsFreePromotion = (
		bAssertFree
		||
		isPromotionFree(ePromotion)
		||
		GET_PLAYER(getOwner()).isFreePromotion(getUnitCombatType(), ePromotion)
		||
		promo.isZeroesXP()
		||
		promo.isForOffset()
	);

	if (promo.isCargoPrereq() && !isCarrier())
	{
		if (bMessageOnFalse)
		{
			if (bPromo && !bIsFreePromotion)
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_CARRIER_CAN_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), getX(), getY()
				);
			}
			else
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_CARRIER_NO_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), getX(), getY()
				);
			}
		}
		return false;
	}

	if (promo.getObsoleteTech() != NO_TECH && GET_TEAM(getTeam()).isHasTech(promo.getObsoleteTech()))
	{
		if (bMessageOnFalse)
		{
			if (bPromo && !bIsFreePromotion)
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_TECH_CAN_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), getX(), getY()
				);
			}
			else
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_TECH_NO_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), getX(), getY()
				);
			}
		}
		return false;
	}

	if (promo.isNotOnDomain(getDomainType())
	||  promo.getPromotionLine() != NO_PROMOTIONLINE
	&&  GC.getPromotionLineInfo(promo.getPromotionLine()).isNotOnDomain(getDomainType()))
	{
		if (bMessageOnFalse)
		{
			if (bPromo && !bIsFreePromotion)
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_DOMAIN_CAN_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), getX(), getY()
				);
			}
			else
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_DOMAIN_NO_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), getX(), getY()
				);
			}
		}
		return false;
	}

	//	THE PROMOTION'S `requires.build`, through the ONE evaluator ([DEC-single-implementation]) -- the same
	//	single gate the ACQUIRE half asks. It replaces the whole hand-rolled prereq battery this function used to
	//	walk field by field (the plot-substrate axes -- terrain / feature / plot bonus / improvement / local
	//	building -- and the promotion AND/OR trio beneath them), because the curator authors every one of them
	//	into `requires`: the evaluator resolves a PROMOTION_ atom against the unit and a TERRAIN_/FEATURE_ atom
	//	against its plot, so nothing is lost by asking once rather than axis by axis.
	//	⚑ Promotions are the enabler's on-demand carve-out ([enabler.md §7.1]): there is no maintained per-unit
	//	set, so the KEEP verdict is evaluated HERE, at the decision point, exactly as the ACQUIRE verdict is.
	//	⚠ BEHAVIOUR, and it is a REAL LOSS rather than a tidy-up: the legacy walk emitted a CAN_RETRAIN /
	//	NO_RETRAIN message PER FAILING AXIS -- six pairs, all twelve authored and rendering in
	//	`Global_CIV4GameText.xml`. A single evaluator call returns a bool and cannot attribute WHICH clause
	//	failed, so the player now learns that a promotion lapsed without being told why.
	//	⚑ The gate is still the evaluator's -- the axes are what the curator authors into `requires`, and
	//	re-creating a per-axis walk purely to caption the failure would rebuild the legacy battery for text.
	//	Player-facing text is END-STAGE and demand-driven ([patterns.md] THE DIVISION OF LABOUR: a line removed
	//	by a cut is not a regression to restore), so the notices come back -- if wanted -- on the ALERT
	//	consumer, off the fact, never re-inlined here ([event-spine.md] PLAYER ALERTS). Owed list: todo.md.
	//	⛔ It is NOT answerable from the structural `requires` walk: that reports what a tree NAMES, so an `any`
	//	pair would read as an AND and a `noneOf` as a requirement. A gate verdict is the evaluator's.
	if (!bIsFreePromotion)
	{
		CvCascadeEvalCtx keepCtx;
		keepCtx.unit = this;
		keepCtx.empireContext = &GET_PLAYER(getOwner()).getEmpireContext();
		const CvPlot* pUnitPlot = plot();
		if (pUnitPlot != NULL)
		{
			keepCtx.plotContext = &pUnitPlot->getPlotContext();
			const CvCity* pPlotCity = pUnitPlot->getPlotCity();
			if (pPlotCity != NULL)
			{
				keepCtx.cityContext = &pPlotCity->getCityContext();
			}
		}
		static const CvCascadeEvalFlags kKeepFlags;
		if (!cascadeEvalCondition(promo.getRequires() != NULL ? promo.getRequires()->build : NULL,
			keepCtx, kKeepFlags))
		{
			return false;
		}
	}

	if (!isPromotionValid(ePromotion, bIsFreePromotion, true))
	{
		if (bMessageOnFalse)
		{
			if (bPromo && !bIsFreePromotion)
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_INVALIDATE_CAN_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_GREEN(), getX(), getY()
				);
			}
			else
			{
				AddDLLMessage(
					getOwner(), true, GC.getEVENT_MESSAGE_TIME(),
					gDLL->getText(
						"TXT_KEY_MISC_OBSOLETED_PROMOTION_INVALIDATE_NO_RETRAIN",
						getNameKey(), promo.getDescription()
					),
					"AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, NULL, GC.getCOLOR_RED(), getX(), getY()
				);
			}
		}
		return false;
	}
	return true;
}

int CvUnit::getPromotionFreeCount(PromotionTypes ePromotion) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), ePromotion);

	const PromotionKeyedInfo* info = findPromotionKeyedInfo(ePromotion);

	return info == NULL ? 0 : info->m_iPromotionFreeCount;
}

bool CvUnit::isPromotionFree(PromotionTypes ePromotion) const
{
	return getPromotionFreeCount(ePromotion) > 0;
}

void CvUnit::setPromotionFreeCount(PromotionTypes ePromotion, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), ePromotion);

	PromotionKeyedInfo* info = findOrCreatePromotionKeyedInfo(ePromotion, iChange != 0);

	if (info != NULL)
	{
		info->m_iPromotionFreeCount = iChange;
	}
}

//TB Combat Mods end

bool CvUnit::meetsUnitSelectionCriteria(const CvUnitSelectionCriteria* criteria) const
{
	if (criteria != NULL)
	{
		if (criteria->m_eUnitAI != NO_UNITAI && AI_getUnitAIType() != criteria->m_eUnitAI)
		{
			return false;
		}

		if ((criteria->m_bNoNegativeProperties || criteria->m_bPropertyBeneficial)
		&& AI_beneficialPropertyValueToCity(NULL, NO_PROPERTY) < 0)
		{
			return false;
		}

		if (criteria->m_eProperty != NO_PROPERTY)
		{
			const int iPropertyDelta = AI_beneficialPropertyValueToCity(NULL, criteria->m_eProperty);

			if (iPropertyDelta == 0)
			{
				return false;
			}
			if (iPropertyDelta > 0)
			{
				if (!criteria->m_bPropertyBeneficial)
				{
					return false;
				}
			}
			else if (criteria->m_bPropertyBeneficial)
			{
				return false;
			}
		}

		if (criteria->m_bIsHealer)
		{
			if (criteria->m_eHealUnitCombat == NO_UNITCOMBAT && ( (resolvedValue(URS_HEAL_SAME_TILE) / 100) == 0 && (resolvedValue(URS_HEAL_ADJACENT) / 100) == 0 ))
			{
				return false;
			}
			if (getBestHealingTypeConst() != criteria->m_eHealUnitCombat || getNumHealSupportTotal() < 1)
			{
				return false;
			}
		}

		if (criteria->m_bIsCommander && !isCommander())
		{
			return false;
		}

		if (criteria->m_bIsCommodore && !isCommodore())
        {
        	return false;
        }
	}

	return true;
}

bool CvUnit::shouldUseWithdrawalOddsCap() const
{
	return false;
}

void CvUnit::statusUpdate(PromotionTypes eStatus)
{
	PROFILE_EXTRA_FUNC();
	bool bReplaced = false;

	for (int iI = 0; iI < GC.getNumPromotionInfos() && !bReplaced; iI++)
	{
		if (GC.getPromotionInfo((PromotionTypes)iI).isStatus())
		{
			const PromotionTypes ePromoToReplace = ((PromotionTypes)iI);
			if (isHasPromotion(ePromoToReplace) && GC.getPromotionInfo(ePromoToReplace).getPromotionLine() == GC.getPromotionInfo(eStatus).getPromotionLine())
			{
				if (GC.getPromotionInfo(ePromoToReplace).getLinePriority() != GC.getPromotionInfo(eStatus).getLinePriority())
				{
					setHasPromotion(ePromoToReplace, false, true);
					bReplaced = true;
				}
			}
		}
	}
	setHasPromotion(eStatus, true, true);
	//	Koshling - testing promotion readiness here is uneccessary since CvUnit::doTurn
	//	will do it.  It is alo now dangerous to do it here (or indeed anywhere but controlled
	//	places) becaue it is not thread-safe and needs to run strictly on the main thread
	//testPromotionReady();

	if (IsSelected())
	{
		gDLL->getInterfaceIFace()->playGeneralSound(GC.getPromotionInfo(eStatus).getSound());

		gDLL->getInterfaceIFace()->setDirty(UnitInfo_DIRTY_BIT, true);

// BUG - Update Plot List - start
		gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
// BUG - Update Plot List - end
	}
	else
	{
		setInfoBarDirty(true);
	}

	//CvEventReporter::getInstance().unitPromoted(this, eStatus);
}

int CvUnit::flankingStrengthbyUnitCombatTotal(UnitCombatTypes eCombatType) const
{
	// The unit's own authored share is the KEYED entry `combat.unit.flanking.{UNITCOMBAT_X}` -- flanking is keyed
	// by combat CLASS, never by unit ([json.md] par.6) -- read off its own compiled entries, never by walking the
	// UnitCombat registry. A memberless address compiles to kind 0, and a percent is not scaled.
	return (
		std::max(
			0,
			InfoValuation::keyedCombat(m_pUnitInfo->getModifiers(), InfoValuation::COMBAT_TARGET_FLANKING,
				(int)eCombatType, COMBAT_AMOUNT)
			+ getExtraFlankingStrengthbyUnitCombatType(eCombatType, isCommander(), isCommodore())
		)
	);
}

int CvUnit::getExtraFlankingStrengthbyUnitCombatType(UnitCombatTypes eIndex, const bool bCommander, const bool bCommodore) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);

	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eIndex);

	if (!bCommander)
	{
		const CvUnit* pCommander = getCommander();
		if (pCommander)
		{
			return (info ? info->m_iExtraFlankingStrengthbyUnitCombatType : 0) + pCommander->getExtraFlankingStrengthbyUnitCombatType(eIndex);
		}
	}
	if (!bCommodore)
    	{
    		const CvUnit* pCommodore = getCommodore();
    		if (pCommodore)
    		{
    			return (info ? info->m_iExtraFlankingStrengthbyUnitCombatType : 0) + pCommodore->getExtraFlankingStrengthbyUnitCombatType(eIndex);
    		}
    	}
	return info ? info->m_iExtraFlankingStrengthbyUnitCombatType : 0;
}


void CvUnit::changeExtraFlankingStrengthbyUnitCombatType(UnitCombatTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eIndex);

	if (iChange != 0)
	{
		findOrCreateUnitCombatKeyedInfo(eIndex)->m_iExtraFlankingStrengthbyUnitCombatType += iChange;
	}
}


int CvUnit::getHealUnitCombatCount() const
{
	return m_iHealUnitCombatCount;
}

int CvUnit::getHealUnitCombatTypeTotal(UnitCombatTypes eUnitCombatType) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombatType);

	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eUnitCombatType);

	return std::max(0, info ? info->m_iHealUnitCombatTypeVolume : 0);
}

void CvUnit::changeHealUnitCombatTypeVolume(UnitCombatTypes eUnitCombatType, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombatType);

	if (iChange != 0)
	{
		UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo(eUnitCombatType);

		if (info->m_iHealUnitCombatTypeVolume > 0)
		{
			m_iHealUnitCombatCount -= info->m_iHealUnitCombatTypeVolume;
		}
		info->m_iHealUnitCombatTypeVolume += iChange;

		if (info->m_iHealUnitCombatTypeVolume > 0)
		{
			m_iHealUnitCombatCount += info->m_iHealUnitCombatTypeVolume;
		}
	}
}

int CvUnit::getHealUnitCombatTypeAdjacentTotal(UnitCombatTypes eUnitCombatType) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombatType);

	const UnitCombatKeyedInfo* info = findUnitCombatKeyedInfo(eUnitCombatType);

	return std::max(0, info ? info->m_iHealUnitCombatTypeAdjacentVolume : 0);
}

void CvUnit::changeHealUnitCombatTypeAdjacentVolume(UnitCombatTypes eUnitCombatType, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), eUnitCombatType);

	if (iChange != 0)
	{
		UnitCombatKeyedInfo* info = findOrCreateUnitCombatKeyedInfo(eUnitCombatType);

		if (info->m_iHealUnitCombatTypeAdjacentVolume > 0)
		{
			m_iHealUnitCombatCount -= info->m_iHealUnitCombatTypeAdjacentVolume;
		}
		info->m_iHealUnitCombatTypeAdjacentVolume += iChange;

		if (info->m_iHealUnitCombatTypeAdjacentVolume > 0)
		{
			m_iHealUnitCombatCount += info->m_iHealUnitCombatTypeAdjacentVolume;
		}
	}
}

void CvUnit::doSetUnitCombats()
{
	PROFILE_EXTRA_FUNC();
	if (getUnitCombatType() != NO_UNITCOMBAT)
	{
		setHasUnitCombat(getUnitCombatType(), true);
	}
	foreach_(const int iSubCombat, m_pUnitInfo->getCombatClasses())
	{
		setHasUnitCombat((UnitCombatTypes)iSubCombat, true);
	}
	// The unit's era stamp comes from the tech that UNLOCKS it, off its own ENABLED_BY reverse family
	// ([DEC-one-reverse-view]) -- a tech names the units it unlocks in `enables.units`, so the unit has no
	// forward prereq of its own to read.
	const TechTypes eEnablingTech = m_pUnitInfo->getEnablingTech();
	const EraTypes eEra =
	(
		eEnablingTech != NO_TECH
		?
		(EraTypes)GC.getTechInfo(eEnablingTech).getEra()
		:
		GC.getGame().getCurrentEra()
	);
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (GC.getUnitCombatInfo((UnitCombatTypes)iI).getEra() == eEra)
		{
			setHasUnitCombat((UnitCombatTypes)iI, true, false);
			break;
		}
	}
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		setSMValues();
	}
}

void CvUnit::setFreePromotion(PromotionTypes ePromotion, bool bAdding, TraitTypes eTrait)
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& pPlayer = GET_PLAYER(getOwner());

	if (bAdding && !isHasPromotion(ePromotion))
	{
		if (m_pUnitInfo->grantsPromotion((int)ePromotion)
		|| (NO_UNIT != getUnitType() && pPlayer.isFreePromotion(getUnitType(), ePromotion)))
		{
			setHasPromotion(ePromotion, true, true);
			return;
		}

		for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
		{
			if (it->second.m_bHasUnitCombat)
			{
				// The player free-promotion registry STAYS: it is written only by applyEvent, i.e. genuine
				// one-shot event state, which is out of the payload plane's scope entirely
				// (legacy-grant-apply-sites.md par.4 -- the three legs of this function have three lifetimes).
				if (pPlayer.isFreePromotion(it->first, ePromotion))
				{
					setHasPromotion(ePromotion, true, true);
					return;
				}
			}
		}
	}
	// The TRAIT legs are gone from both halves: a free promotion is a TRIGGER/GRANT payload (owner), so the
	// curated traits author `onUnitEnteredCity` -> `action.promote`, each entry carrying its own
	// `enabled: "IS_<TAG>"` predicate for the classes it arms -- the same shape the BUILDING leg uses.
	// ⛔ Do NOT answer a dangling trait promotion by restoring a trait-side promotion x unitcombat map -- that is
	// the legacy mechanism whose data moved, and it swept the whole trait registry per promotion to do it.
	// Until the trigger engine's promote pass consults HELD TRAITS (it walks the city's operating buildings
	// only), trait-granted promotions reach no unit; that hole is tracked, and is the correct exposed state
	// rather than a legacy path kept breathing.
}

void CvUnit::doSetFreePromotions(bool bAdding, TraitTypes eTrait)
{
	PROFILE_EXTRA_FUNC();
	for (int iI = GC.getNumPromotionInfos() - 1; iI > -1; iI--)
	{
		setFreePromotion(static_cast<PromotionTypes>(iI), bAdding, eTrait);
	}
	if (GC.getGame().getModderGameOption(MODDERGAMEOPTION_STARSIGNS))
	{
		doStarsign();
	}
}

int CvUnit::getRetrainsAvailable() const
{
	return m_iRetrainsAvailable;
}

void CvUnit::setRetrainsAvailable(int iNewValue)
{
	m_iRetrainsAvailable = iNewValue;
	FASSERT_NOT_NEGATIVE(getRetrainsAvailable());
}

void CvUnit::changeRetrainsAvailable(int iChange)
{
	setRetrainsAvailable(getRetrainsAvailable() + iChange);
}

int CvUnit::getExperiencefromWithdrawal(const int iWithdrawalProbability) const
{
	return std::max(1, GC.getEXPERIENCE_FROM_WITHDRAWL() * (100 - iWithdrawalProbability));
}


int CvUnit::captureProbabilityTotal() const
{
	int iData = resolvedValue(URS_CAPTURE_PROBABILITY);

	int aiCapture[NUM_CAPTURE_KINDS];
	GET_PLAYER(getOwner()).getCaptureKinds(aiCapture);
	iData += aiCapture[CAPTURE_PROBABILITY];

	return std::max(0, iData);
}


int CvUnit::captureResistanceTotal() const
{
	int iData = resolvedValue(URS_CAPTURE_RESISTANCE);

	int aiCapture[NUM_CAPTURE_KINDS];
	GET_PLAYER(getOwner()).getCaptureKinds(aiCapture);
	iData += aiCapture[CAPTURE_RESISTANCE];

	return std::max(0, iData);
}


void CvUnit::changeExtraBreakdownChance(int iChange)
{
	m_iExtraBreakdownChance += iChange;
}

int CvUnit::breakdownChanceTotal() const
{
	int iData = m_pUnitInfo->getFlatCombat(COMBAT_BREAKDOWN_CHANCE, CASC_SCOPE_UNIT) / 100 + m_iExtraBreakdownChance;

	return std::max(0, iData);
}


void CvUnit::changeExtraBreakdownDamage(int iChange)
{
	m_iExtraBreakdownDamage +=iChange;
}

int CvUnit::breakdownDamageTotal() const
{
	int iData = m_pUnitInfo->getFlatCombat(COMBAT_BREAKDOWN_DAMAGE, CASC_SCOPE_UNIT) / 100 + m_iExtraBreakdownDamage;
	return std::max(0, iData);
}

// ⚠ URS_TAUNT is a FLAT slot (×100) consumed as a human percent (`iValue += tauntTotal() * iValue / 100`), so
// it reduces here ([DEC-fixedpoint-x100]) -- raw, a +50% taunt applies as roughly x51.
int CvUnit::tauntTotal() const
{
	return std::max(0, resolvedValue(URS_TAUNT) / 100);
}

int CvUnit::getExtraCombatModifierPerSizeMore() const
{
	return std::max(0, m_iExtraCombatModifierPerSizeMore);
}

void CvUnit::changeExtraCombatModifierPerSizeMore(int iChange)
{
	m_iExtraCombatModifierPerSizeMore += iChange;
}

void CvUnit::setExtraCombatModifierPerSizeMore(int iChange)
{
	m_iExtraCombatModifierPerSizeMore = iChange;
}

int CvUnit::combatModifierPerSizeMoreTotal() const
{
	int iData = m_pUnitInfo->getSizeMatters().combatModifierPerSizeMore;
	iData += getExtraCombatModifierPerSizeMore();
	return iData;
}

int CvUnit::getExtraCombatModifierPerSizeLess() const
{
	return std::max(0, m_iExtraCombatModifierPerSizeLess);
}

void CvUnit::changeExtraCombatModifierPerSizeLess(int iChange)
{
	m_iExtraCombatModifierPerSizeLess += iChange;
}

void CvUnit::setExtraCombatModifierPerSizeLess(int iChange)
{
	m_iExtraCombatModifierPerSizeLess = iChange;
}

int CvUnit::combatModifierPerSizeLessTotal() const
{
	int iData = m_pUnitInfo->getSizeMatters().combatModifierPerSizeLess;
	iData += getExtraCombatModifierPerSizeLess();
	return iData;
}

int CvUnit::getExtraCombatModifierPerVolumeMore() const
{
	return std::max(0, m_iExtraCombatModifierPerVolumeMore);
}

void CvUnit::changeExtraCombatModifierPerVolumeMore(int iChange)
{
	m_iExtraCombatModifierPerVolumeMore += iChange;
}

void CvUnit::setExtraCombatModifierPerVolumeMore(int iChange)
{
	m_iExtraCombatModifierPerVolumeMore = iChange;
}

int CvUnit::combatModifierPerVolumeMoreTotal() const
{
	int iData = m_pUnitInfo->getSizeMatters().combatModifierPerVolumeMore;
	iData += getExtraCombatModifierPerVolumeMore();
	return iData;
}

int CvUnit::getExtraCombatModifierPerVolumeLess() const
{
	return std::max(0, m_iExtraCombatModifierPerVolumeLess);
}

void CvUnit::changeExtraCombatModifierPerVolumeLess(int iChange)
{
	m_iExtraCombatModifierPerVolumeLess += iChange;
}

void CvUnit::setExtraCombatModifierPerVolumeLess(int iChange)
{
	m_iExtraCombatModifierPerVolumeLess = iChange;
}

int CvUnit::combatModifierPerVolumeLessTotal() const
{
	int iData = m_pUnitInfo->getSizeMatters().combatModifierPerVolumeLess;
	iData += getExtraCombatModifierPerVolumeLess();
	return iData;
}

//	`strength` is the BASE and `combat` is what MODIFIES it (json.md par.6), so the strength MODIFIER is the
//	combat percent the RESOLVED plane already gathers from the unit's held promotions + combat classes.
//	⚠ A PERCENT is not scaled ([DEC-fixedpoint-x100]), so this reduces by nothing -- the callers stack it as
//	`100 + value`, which is the unscaled form.
int CvUnit::getExtraStrengthModifier() const
{
	return resolvedValue(URS_STRENGTH_PERCENT);
}

bool CvUnit::isBreakdownCombat(const CvPlot* pPlot, bool bSamePlot) const
{
	//only if attacking unit has the ability and is not currently engaged in a distance counterattack as it approaches.
	if (breakdownChanceTotal() > 0 && breakdownDamageTotal() > 0 && !bSamePlot)
	{
		if (pPlot->isCity(false))
		{
			const CvCity* pCity = pPlot->getPlotCity();
			//Only if the city still has some defenses left to damage.
			if (pCity->isBombardable(this))
			{
				return true;
			}
		}
	}
	return false;
}

void CvUnit::resolveBreakdownAttack(const CvPlot* pPlot)
{
	if (!pPlot->isCity(false))
	{
		return;
	}

	CvCity* pCity = pPlot->getPlotCity();

	FAssertMsg(pCity != NULL, "Breakdown Target City is not assigned a valid value");

	const int iNormalDamage = breakdownDamageTotal();
	int iTrueDamage = iNormalDamage + iNormalDamage * std::max(0, 100 - pCity->getBombardDefense()) / 100;

	if (iNormalDamage > 0)
	{
		iTrueDamage = std::max(1, iTrueDamage);
	}

	if (std::max(5, breakdownChanceTotal()) > GC.getGame().getSorenRandNum(100, "BreakdownAttackRoll"))
	{
		pCity->changeDefenseModifier(-iTrueDamage);

		CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_DEFENSES_IN_CITY_REDUCED_TO", pCity->getNameKey(), pCity->getDefenseModifier(false), GET_PLAYER(getOwner()).getNameKey());
		AddDLLMessage(pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARDED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY(), true, true);

		szBuffer = gDLL->getText("TXT_KEY_MISC_YOU_REDUCE_CITY_DEFENSES", getNameKey(), pCity->getNameKey(), pCity->getDefenseModifier(false));
		AddDLLMessage(getOwner(), true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_BOMBARD", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_GREEN(), pCity->getX(), pCity->getY());
	}
}

int CvUnit::getDiminishingReturn(int i) const
{
	PROFILE_EXTRA_FUNC();
	if (i < 51)
	{
		return i;
	}
	int iA = 0;
	int iB = 100;

	for (int iC = i; iC > 0; iC /= 2)
	{
		iB /= 2;
		iA += iB;
		i -= iB;
		i /= 2;
		if (i < iB/2 + 1)
		{
			i += iA;
			return i;
		}
	}
	return 0;
}

bool CvUnit::hasCannotMergeSplit() const
{
	return getCannotMergeSplitCount() > 0;
}

int CvUnit::getCannotMergeSplitCount() const
{
	return m_iCannotMergeSplitCount;
}

void CvUnit::changeCannotMergeSplitCount(int iNewValue)
{
	m_iCannotMergeSplitCount += iNewValue;
}

int CvUnit::getQualityBaseTotal() const
{
	return m_iQualityBaseTotal;
}

void CvUnit::setQualityBaseTotal(int iNewValue)
{
	m_iQualityBaseTotal = iNewValue;
}

int CvUnit::getGroupBaseTotal() const
{
	return m_iGroupBaseTotal;
}

void CvUnit::setGroupBaseTotal(int iNewValue)
{
	m_iGroupBaseTotal = iNewValue;
}

int CvUnit::getSizeBaseTotal() const
{
	return m_iSizeBaseTotal;
}

void CvUnit::setSizeBaseTotal(int iNewValue)
{
	m_iSizeBaseTotal = iNewValue;
}

int CvUnit::getExtraQuality() const
{
	return m_iExtraQuality;
}

void CvUnit::changeExtraQuality(int iChange)
{
	m_iExtraQuality += iChange;
}

int CvUnit::getExtraGroup() const
{
	return m_iExtraGroup;
}

void CvUnit::changeExtraGroup(int iChange)
{
	const int iOldEffCount = SMeffectiveCount();

	GET_PLAYER(getOwner()).changeUnitCountSM(m_eUnitType, -smGroupMultiplier(groupRank()));
	m_iExtraGroup += iChange;
	GET_PLAYER(getOwner()).changeUnitCountSM(m_eUnitType, smGroupMultiplier(groupRank()));

	// Keep the strength-weighted force ledgers in step with rank changes (#395). Dead
	// units were already removed from the ledgers by the kill path.
	if (!isDead())
	{
		const int iEffChange = SMeffectiveCount() - iOldEffCount;

		if (iEffChange != 0 && AI_getUnitAIType() != NO_UNITAI)
		{
			GET_PLAYER(getOwner()).AI_changeEffNumAIUnitsTimes100(AI_getUnitAIType(), iEffChange);

			if (plot() != NULL)
			{
				plot()->area()->changeEffNumAIUnitsTimes100(getOwner(), AI_getUnitAIType(), iEffChange);
			}
		}
	}
}

int CvUnit::getExtraSize() const
{
	return m_iExtraSize;
}

void CvUnit::changeExtraSize(int iChange)
{
	m_iExtraSize += iChange;
}

int CvUnit::qualityRank() const
{
	FASSERT_NOT_NEGATIVE(getQualityBaseTotal());
	return (getQualityBaseTotal() + getExtraQuality());
}

int CvUnit::groupRank() const
{
	FASSERT_NOT_NEGATIVE(getGroupBaseTotal());
	return (getGroupBaseTotal() + getExtraGroup());
}

int CvUnit::sizeRank() const
{
	FASSERT_NOT_NEGATIVE(getSizeBaseTotal());
	return (getSizeBaseTotal() + getExtraSize());
}

// Strength-weighted body equivalent for AI force accounting, times-100 fixed point (#395).
// A unit at its type's base group rank counts as 100; each group rank above (merges and
// group promotions alike) multiplies by SIZE_MATTERS_MOST_MULTIPLIER -- the same x1.5 the
// unit's strength actually scales by -- and each rank below divides. Owner ruling: a merged
// unit must never be credited as its constituent count (x3 would tell the AI it has force
// it does not have). Quality/size ranks are deliberately excluded: they exist without
// merging, and the pre-SM demand constants never weighted promotions either.
int CvUnit::SMeffectiveCount() const
{
	if (getExtraGroup() == 0 || !GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return 100;
	}
	return applySMRank(100, getExtraGroup(), GC.getSIZE_MATTERS_MOST_MULTIPLIER());
}

// (bAutocheck = true) check will be ordering a 4th potentially mergable unit to
// split instead during it's check processing.
// The thinking behind this method is that when we merge 3 units we want a 4th one that
// was capable of it to be present and to split so that the unit count remains the same
// and for the alternative strategy of splitting to be equally expressed.
// For defense this means you create fodder flak to hold off minimalist unit count
// armies, buying time, and a strong lead defender to make a tough stand
// For attack you have a strong lead attacker to bust through stiff opposition and some
// smaller units to wipe up defenders weakened by collateral (or splitting strategies
// to delay the capture of the city or position.)
// After a few round of such merging among particular types in the same location will
// create a nice gradient of unit group sizes.  Should be interesting to see its effect in play.
// TBSPLIT
bool CvUnit::canMerge(bool bAutocheck) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(plot(), "canMerge expects unit plot to be valid");


	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return false;
	}

	// Size Matters: a human selection group can bulk-merge its own units that share the
	// same unit type and size (group rank and quality rank), in triples, via one click on
	// the Merge command. This mirrors how Split already operates on a whole group.
	if (isHuman() && getGroup()->getNumUnits() > 1)
	{
		// Enable the Merge command if the group holds at least one complete triple of
		// eligible units sharing the same unit type and size (group rank and quality rank).
		// The actual bulk merge (doMergeAllInGroup) scans the whole group, so this does not
		// depend on the head unit itself being eligible.
		foreach_(const CvUnit* pBase, getGroup()->units())
		{
			if (!pBase->isMergeEligible())
			{
				continue;
			}
			int iValidUnitCount = 1; // includes pBase
			foreach_(const CvUnit* pLoopUnit, getGroup()->units())
			{
				if (pLoopUnit->getID() != pBase->getID()
					&& pLoopUnit->getUnitType() == pBase->getUnitType()
					&& pLoopUnit->groupRank() == pBase->groupRank()
					&& pLoopUnit->qualityRank() == pBase->qualityRank()
					&& pLoopUnit->isMergeEligible())
				{
					if (++iValidUnitCount >= 3)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	if (isHurt()
		|| isDead()
		|| isInBattle()
		|| isCargo()
		|| hasCargo()
		|| isSpy()
		|| hasMoved()
		|| groupRank() >= eraGroupMergeLimit()
		|| isInhibitMerge())
	{
		return false;
	}

	if (hasCannotMergeSplit())
	{
		return false;
	}

	if (baseWorkRate() > 0)
	{
		return false;
	}

	CvPlot* pPlot = plot();
	int iValidUnitCount = 0;
	foreach_(const CvUnit* pLoopUnit, pPlot->units())
	{
		if (pLoopUnit->getOwner() == getOwner() && pLoopUnit->getID() != getID()

			&& pLoopUnit->getUnitType() == getUnitType()
			&& pLoopUnit->groupRank() == groupRank()
			&& pLoopUnit->qualityRank() == qualityRank()

			&& !pLoopUnit->isHurt()
			&& !pLoopUnit->isDead()
			&& !pLoopUnit->isInBattle()
			&& !pLoopUnit->isCargo()
			&& !pLoopUnit->hasCargo()
			&& !pLoopUnit->isSpy()
			&& !pLoopUnit->hasMoved()
			&& pLoopUnit->baseWorkRate() < 1
			&& pLoopUnit->groupRank() < pLoopUnit->eraGroupMergeLimit()

			&& !pLoopUnit->hasCannotMergeSplit()
			)
		{
			if (!bAutocheck)
			{
				iValidUnitCount++;
			}
			else if (pLoopUnit->AI_getUnitAIType() == AI_getUnitAIType())
			{
				iValidUnitCount++;
			}
			if (bAutocheck && iValidUnitCount == 3)
			{
				return true;
			}
		}
	}
	if (iValidUnitCount >= 2)
	{
		return true;
	}

	return false;
}


bool CvUnit::canSplit() const
{

	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return false;
	}

	if (isHurt() || isDead() || isInBattle() || isCargo() || hasCargo() || isSpy() || hasMoved() || isInhibitSplit() )
	{
		return false;
	}

	if (hasCannotMergeSplit())
	{
		return false;
	}

	if (baseWorkRate() > 0)
	{
		return false;
	}

	if (groupRank() <= eraGroupSplitLimit())
	{
		return false;
	}
	return true;
}

// Helpers
bool CvUnit::isGroupUpgradePromotion(PromotionTypes promotion) const
{
	return GC.getPromotionInfo(promotion).getSizeMatters().group > 0 &&
		(canAcquirePromotion(promotion, PromotionRequirements::Promote | PromotionRequirements::ForOffset) || canAcquirePromotion(promotion));
}

bool CvUnit::isGroupDowngradePromotion(PromotionTypes promotion) const
{
	return GC.getPromotionInfo(promotion).getSizeMatters().group < 0 &&
		(canAcquirePromotion(promotion, PromotionRequirements::Promote | PromotionRequirements::ForOffset) || canAcquirePromotion(promotion));
}

bool CvUnit::isQualityUpgradePromotion(PromotionTypes promotion) const
{
	return GC.getPromotionInfo(promotion).getSizeMatters().quality > 0 &&
		(canAcquirePromotion(promotion, PromotionRequirements::Promote | PromotionRequirements::ForOffset) || canAcquirePromotion(promotion));
}

bool CvUnit::isQualityDowngradePromotion(PromotionTypes promotion) const
{
	return GC.getPromotionInfo(promotion).getSizeMatters().quality < 0 &&
		(canAcquirePromotion(promotion, PromotionRequirements::Promote | PromotionRequirements::ForOffset) || canAcquirePromotion(promotion));
}


void CvUnit::doMerge()
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(plot() != NULL, "doMerge requires CvUnit plot to be valid");
	GET_PLAYER(getOwner()).setBaseMergeSelectionUnit(getID());
	if (isHuman())
	{
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_CHOOSE_MERGE_UNIT);
		pInfo->setData1(getID());
		pInfo->setData2(getX());
		pInfo->setData3(getY());
		gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
	}
	else
	{
		CvPlot* pPlot = plot();
		CvUnit* pUnit1 = GET_PLAYER(getOwner()).getUnit(GET_PLAYER(getOwner()).getBaseMergeSelectionUnit());
		CvSelectionGroup* pMergingGroup = pUnit1->getGroup();

		foreach_(const CvUnit* pLoopUnit, pPlot->units())
		{
			if (pLoopUnit->getOwner() == getOwner()
				&& pLoopUnit->getID() != pUnit1->getID()

				&& pLoopUnit->getUnitType() == pUnit1->getUnitType()
				&& pLoopUnit->groupRank() == pUnit1->groupRank()
				&& pLoopUnit->qualityRank() == pUnit1->qualityRank()
				&& pLoopUnit->AI_getUnitAIType() == pUnit1->AI_getUnitAIType()

				&& !pLoopUnit->isHurt()
				&& !pLoopUnit->isDead()
				&& !pLoopUnit->isInBattle()
				&& !pLoopUnit->isCargo()
				&& !pLoopUnit->hasCargo()
				&& !pLoopUnit->isSpy()
				&& !pLoopUnit->hasMoved()
				)
			{
				if (GET_PLAYER(getOwner()).getFirstMergeSelectionUnit() == FFreeList::INVALID_INDEX)
				{
					GET_PLAYER(getOwner()).setFirstMergeSelectionUnit(pLoopUnit->getID());
				}
				else if (GET_PLAYER(getOwner()).getSecondMergeSelectionUnit() == FFreeList::INVALID_INDEX)
				{
					GET_PLAYER(getOwner()).setSecondMergeSelectionUnit(pLoopUnit->getID());
					break;
				}
			}
		}

		CvUnit* pUnit2 = GET_PLAYER(getOwner()).getUnit(GET_PLAYER(getOwner()).getFirstMergeSelectionUnit());
		CvUnit* pUnit3 = GET_PLAYER(getOwner()).getUnit(GET_PLAYER(getOwner()).getSecondMergeSelectionUnit());

		CvUnit::mergeUnits(pUnit1, pUnit2, pUnit3, pMergingGroup);

		GET_PLAYER(getOwner()).setBaseMergeSelectionUnit(FFreeList::INVALID_INDEX);
		GET_PLAYER(getOwner()).setFirstMergeSelectionUnit(FFreeList::INVALID_INDEX);
		GET_PLAYER(getOwner()).setSecondMergeSelectionUnit(FFreeList::INVALID_INDEX);
	}
}

// Size Matters: per-unit eligibility for a merge (independent of any partner). The unit
// type and size (group rank and quality rank) of candidate partners are compared at the
// call site; this only gates a single unit's own state.
bool CvUnit::isMergeEligible() const
{
	return !isHurt()
		&& !isDead()
		&& !isInBattle()
		&& !isCargo()
		&& !hasCargo()
		&& !isSpy()
		&& !hasMoved()
		&& !isInhibitMerge()
		&& !hasCannotMergeSplit()
		&& baseWorkRate() < 1
		&& groupRank() < eraGroupMergeLimit();
}

// Size Matters: combine three same-type/size units into a single unit one size larger,
// merging their promotions, experience, leader status and AI type. The merged unit is
// placed on pUnit1's plot and, if pJoinGroup is non-NULL, added to that selection group.
// The three source units are killed. Returns the merged unit.
CvUnit* CvUnit::mergeUnits(CvUnit* pUnit1, CvUnit* pUnit2, CvUnit* pUnit3, CvSelectionGroup* pJoinGroup)
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(pUnit1 != NULL && pUnit2 != NULL && pUnit3 != NULL, "mergeUnits requires three valid units");

	const PlayerTypes eOwner = pUnit1->getOwner();
	CvPlot* pPlot = pUnit1->plot();
	const UnitTypes eUnitType = pUnit1->getUnitType();

	CvUnit* pkMergedUnit = GET_PLAYER(eOwner).initUnit(eUnitType, pPlot->getX(), pPlot->getY(), NO_UNITAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));

	// Oscillation guard (scoped to THIS merge operation): forbid splitting while the merge is in
	// progress, so a split fired re-entrantly mid-merge can't tear the half-formed unit apart. It is
	// RELEASED the moment the merge completes (below) -- it is NOT a permanent lock. Leaving it set was
	// the #440 bug: group-merged units could never split, which also broke the SM city-defense fallback
	// that splits a merged defender. The merge<->split CYCLE stays guarded the other way by inhibitMerge
	// (set on split units, checked in canMerge), so releasing this does not enable thrash.
	pkMergedUnit->setInhibitSplit(true);

	pUnit1->setFortifyTurns(0);
	pUnit2->setFortifyTurns(0);
	pUnit3->setFortifyTurns(0);

	int iTotalGroupOffset = 1;
	int iTotalQualityOffset = 0;
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		PromotionTypes ePromotion = ((PromotionTypes)iI);
		if (GC.getPromotionInfo(ePromotion).getSizeMatters().group == 0 && GC.getPromotionInfo(ePromotion).getSizeMatters().quality == 0)
		{
			if (pUnit1->isHasPromotion(ePromotion) || pUnit2->isHasPromotion(ePromotion) || pUnit3->isHasPromotion(ePromotion))
			{
				if (GC.getPromotionInfo(ePromotion).isLeader())
				{
					pkMergedUnit->setHasPromotion(ePromotion, true, true);
				}
				else if (pUnit1->isPromotionFree(ePromotion) || pUnit2->isPromotionFree((PromotionTypes)iI) || pUnit3->isPromotionFree((PromotionTypes)iI))
				{
					pkMergedUnit->setHasPromotion(ePromotion, true, true);
				}
				else if (pUnit1->isHasPromotion(ePromotion) && pUnit2->isHasPromotion(ePromotion) && pUnit3->isHasPromotion(ePromotion))
				{
					pkMergedUnit->setHasPromotion(ePromotion, true, false);
					pkMergedUnit->changeLevel(1);
				}
			}
		}
		else if (GC.getPromotionInfo(ePromotion).getSizeMatters().quality != 0)
		{
			if (pUnit1->isHasPromotion(ePromotion) || pUnit2->isHasPromotion(ePromotion) || pUnit3->isHasPromotion(ePromotion))
			{
				iTotalQualityOffset += GC.getPromotionInfo((PromotionTypes)iI).getSizeMatters().quality;
			}
		}
		else if (GC.getPromotionInfo(ePromotion).getSizeMatters().group != 0)
		{
			if (pUnit1->isHasPromotion(ePromotion) || pUnit2->isHasPromotion(ePromotion) || pUnit3->isHasPromotion(ePromotion))
			{
				iTotalGroupOffset += GC.getPromotionInfo((PromotionTypes)iI).getSizeMatters().group;
			}
		}
	}

	bool bNormalizedGroup = CvUnit::normalizeUnitPromotions(pkMergedUnit, iTotalGroupOffset,
		bind(&CvUnit::isGroupUpgradePromotion, pkMergedUnit, _2),
		bind(&CvUnit::isGroupDowngradePromotion, pkMergedUnit, _2)
	);
	FAssertMsg(bNormalizedGroup, "Could not apply required number of group promotions on merged units");

	bool bNormalizedQuality = CvUnit::normalizeUnitPromotions(pkMergedUnit, iTotalQualityOffset,
		bind(&CvUnit::isQualityUpgradePromotion, pkMergedUnit, _2),
		bind(&CvUnit::isQualityDowngradePromotion, pkMergedUnit, _2)
	);
	FAssertMsg(bNormalizedQuality, "Could not apply required number of quality promotions on merged units");

	//Set New Experience
	int iXP1 = pUnit1->getExperience100();
	int iXP2 = pUnit2->getExperience100();
	int iXP3 = pUnit3->getExperience100();
	int iXP = iXP1 + iXP2 + iXP3;
	if (iXP != 0)
	{
		iXP /= 3;
	}
	pkMergedUnit->setExperience100(iXP);

	pkMergedUnit->setGameTurnCreated(pUnit1->getGameTurnCreated());
	pkMergedUnit->m_eOriginalOwner = pUnit1->getOriginalOwner();
	pkMergedUnit->setAutoPromoting(pUnit1->isAutoPromoting());
	pkMergedUnit->testPromotionReady();
	pkMergedUnit->setName(pUnit1->getNameNoDesc());

	pkMergedUnit->AI_setUnitAIType(pUnit1->AI_getUnitAIType());
	if (pUnit2->AI_getUnitAIType() == pUnit3->AI_getUnitAIType() && pkMergedUnit->AI_getUnitAIType() != pUnit2->AI_getUnitAIType())
	{
		pkMergedUnit->AI_setUnitAIType(pUnit2->AI_getUnitAIType());
	}

	if (pUnit1->getLeaderUnitType() != NO_UNIT)
	{
		pkMergedUnit->setLeaderUnitType(pUnit1->getLeaderUnitType());
	}
	if (pUnit2->getLeaderUnitType() != NO_UNIT && pkMergedUnit->getLeaderUnitType() == NO_UNIT)
	{
		pkMergedUnit->setLeaderUnitType(pUnit2->getLeaderUnitType());
	}
	if (pUnit3->getLeaderUnitType() != NO_UNIT && pkMergedUnit->getLeaderUnitType() == NO_UNIT)
	{
		pkMergedUnit->setLeaderUnitType(pUnit3->getLeaderUnitType());
	}
	if (pJoinGroup != NULL)
	{
		pkMergedUnit->joinGroup(pJoinGroup);
	}

	// [UNT/merge] -- Size Matters: three units became one. Merges deflate raw unit counts
	// under count-based demand targets (docs/plans/unit-ai-valuation.md A5), so every merge
	// is auditable in UnitAI.log.
	logUnitAI(1, "[UNT/merge] owner=%d type=%d ai=%d at=(%d,%d) ids=(%d,%d,%d)->%d rank=%d quality=%d",
		(int)eOwner, (int)eUnitType, (int)pkMergedUnit->AI_getUnitAIType(), pPlot->getX(), pPlot->getY(),
		pUnit1->getID(), pUnit2->getID(), pUnit3->getID(), pkMergedUnit->getID(),
		pkMergedUnit->groupRank(), pkMergedUnit->qualityRank());

	pUnit1->joinGroup(NULL);
	pUnit2->joinGroup(NULL);
	pUnit3->joinGroup(NULL);

	pUnit1->getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	pUnit1->kill(true, NO_PLAYER, true);
	pUnit2->getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	pUnit2->kill(true, NO_PLAYER, true);
	pUnit3->getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
	pUnit3->kill(true, NO_PLAYER, true);

	// Merge complete -- release the operation-scoped guard. The merged unit is now freely splittable
	// (the human Split button and the SM city-defense fallback both depend on this). Strategic
	// merge<->split oscillation is NOT prevented here; that belongs in the AI decision tree (a
	// per-turn, per-unit merge/split cap), not in a permanent flag on the unit.
	pkMergedUnit->setInhibitSplit(false);

	return pkMergedUnit;
}

// Size Matters: bulk-merge every eligible unit in this unit's selection group that shares
// the same unit type and size (group rank and quality rank), in triples, keeping the
// resulting units in the group. Leftover units that cannot form a complete triple of a
// given type/size are left untouched. Runs deterministically on all clients (driven by the
// GAMEMESSAGE_MERGE_ALL network message), so no popup is used.
void CvUnit::doMergeAllInGroup()
{
	PROFILE_EXTRA_FUNC();

	CvSelectionGroup* pGroup = getGroup();
	if (pGroup == NULL)
	{
		return;
	}
	const PlayerTypes eOwner = getOwner();

	// Snapshot the eligible units up front (by ID); merging mutates the group, and the
	// merged units that join the group are one size larger so they never re-bucket here.
	std::vector<int> aiEligible;
	foreach_(const CvUnit* pLoopUnit, pGroup->units())
	{
		if (pLoopUnit->isMergeEligible())
		{
			aiEligible.push_back(pLoopUnit->getID());
		}
	}

	// Bucket the snapshot by (unit type, group rank, quality rank) and merge each bucket in
	// triples. Units are re-fetched and re-validated by ID since earlier merges kill units.
	for (size_t i = 0; i < aiEligible.size(); i++)
	{
		const CvUnit* pBase = GET_PLAYER(eOwner).getUnit(aiEligible[i]);
		if (pBase == NULL || !pBase->isMergeEligible())
		{
			continue;
		}
		const UnitTypes eUnitType = pBase->getUnitType();
		const int iGroupRank = pBase->groupRank();
		const int iQualityRank = pBase->qualityRank();

		std::vector<CvUnit*> apTriple;
		apTriple.push_back(GET_PLAYER(eOwner).getUnit(aiEligible[i]));

		for (size_t j = i + 1; j < aiEligible.size() && apTriple.size() < 3; j++)
		{
			CvUnit* pPartner = GET_PLAYER(eOwner).getUnit(aiEligible[j]);
			if (pPartner != NULL
				&& pPartner->isMergeEligible()
				&& pPartner->getUnitType() == eUnitType
				&& pPartner->groupRank() == iGroupRank
				&& pPartner->qualityRank() == iQualityRank)
			{
				apTriple.push_back(pPartner);
				aiEligible[j] = FFreeList::INVALID_INDEX; // claim this partner
			}
		}

		if (apTriple.size() == 3)
		{
			aiEligible[i] = FFreeList::INVALID_INDEX; // claim the base
			CvUnit::mergeUnits(apTriple[0], apTriple[1], apTriple[2], pGroup);
		}
	}
}

void CvUnit::doSplit()
{
	GET_PLAYER(getOwner()).setSplittingUnit(getID());
	if (isHuman())
	{
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_CONFIRM_SPLIT_UNIT);
		pInfo->setData1(getID());
		pInfo->setData2(getX());
		pInfo->setData3(getY());
		gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
	}
	else
	{
		CvUnit* pUnit0 = GET_PLAYER(getOwner()).getUnit(GET_PLAYER(getOwner()).getSplittingUnit());
		const UnitTypes eUnitType = pUnit0->getUnitType();
		CvUnit* pUnit1 = GET_PLAYER(getOwner()).initUnit(eUnitType, pUnit0->getX(), pUnit0->getY(), NO_UNITAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
		CvUnit* pUnit2 = GET_PLAYER(getOwner()).initUnit(eUnitType, pUnit0->getX(), pUnit0->getY(), NO_UNITAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
		CvUnit* pUnit3 = GET_PLAYER(getOwner()).initUnit(eUnitType, pUnit0->getX(), pUnit0->getY(), NO_UNITAI, NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));

		PROFILE_FUNC();

		// Toffer - Remove any potential buildup promotions
		pUnit0->setFortifyTurns(0);

		int iTotalGroupOffset = -1;
		int iTotalQualityOffset = 0;

		for (int iI = GC.getNumPromotionInfos() -1; iI > -1; iI--)
		{
			const PromotionTypes ePromoX = static_cast<PromotionTypes>(iI);

			if (!pUnit0->isHasPromotion(ePromoX) || pUnit1->isHasPromotion(ePromoX))
			{
				// Toffer - If pUnit1 has it then pUnit2 and 3 should also have it at this point.
				continue;
			}

			if (GC.getPromotionInfo(ePromoX).getSizeMatters().quality != 0)
			{
				iTotalQualityOffset += GC.getPromotionInfo((PromotionTypes)iI).getSizeMatters().quality;
			}
			else if (GC.getPromotionInfo(ePromoX).getSizeMatters().group != 0)
			{
				iTotalGroupOffset += GC.getPromotionInfo((PromotionTypes)iI).getSizeMatters().group;
			}
			else if (GC.getPromotionInfo(ePromoX).isLeader())
			{
				pUnit1->setHasPromotion(ePromoX, true, true);
			}
			else if (pUnit0->isPromotionFree(ePromoX))
			{
				pUnit1->setHasPromotion(ePromoX, true, true);
			}
			else
			{
				pUnit1->setHasPromotion(ePromoX, true, false);
				pUnit2->setHasPromotion(ePromoX, true, false);
				pUnit3->setHasPromotion(ePromoX, true, false);
			}
		}

		std::vector<CvUnit*> newUnits;
		newUnits.push_back(pUnit1);
		newUnits.push_back(pUnit2);
		newUnits.push_back(pUnit3);

		const bool bNormalizedGroup = CvUnit::normalizeUnitPromotions(newUnits, iTotalGroupOffset,
			bind(isGroupUpgradePromotion, pUnit1, _2),
			bind(isGroupDowngradePromotion, pUnit1, _2)
		);
		FAssertMsg(bNormalizedGroup, "Could not apply required number of group promotions on split units");

		const bool bNormalizedQuality = CvUnit::normalizeUnitPromotions(newUnits, iTotalQualityOffset,
			bind(isQualityUpgradePromotion, pUnit1, _2),
			bind(isQualityDowngradePromotion, pUnit1, _2)
		);
		FAssertMsg(bNormalizedQuality, "Could not apply required number of quality promotions on split units");

		if (pUnit0->getLeaderUnitType() != NO_UNIT)
		{
			pUnit1->setLeaderUnitType(pUnit0->getLeaderUnitType());
		}
		CvSelectionGroup* pSplittingGroup = pUnit0->getGroup();

		foreach_(CvUnit* unit, newUnits)
		{
			unit->setInhibitMerge(true);
			//Set New Experience
			unit->setExperience100(pUnit0->getExperience100());
			unit->setLevel(pUnit0->getLevel());
			unit->setGameTurnCreated(pUnit0->getGameTurnCreated());
			unit->m_eOriginalOwner = pUnit0->getOriginalOwner();
			unit->setAutoPromoting(pUnit0->isAutoPromoting());
			unit->setName(pUnit0->getNameNoDesc());
			unit->joinGroup(pSplittingGroup);
		}

		GET_PLAYER(getOwner()).setSplittingUnit(FFreeList::INVALID_INDEX);

		// [UNT/split] -- Size Matters: one unit became three (the inverse count distortion
		// of [UNT/merge]; same auditability rationale).
		logUnitAI(1, "[UNT/split] owner=%d type=%d ai=%d at=(%d,%d) id=%d->(%d,%d,%d) rank=%d quality=%d",
			(int)getOwner(), (int)eUnitType, (int)pUnit0->AI_getUnitAIType(), pUnit0->getX(), pUnit0->getY(),
			pUnit0->getID(), pUnit1->getID(), pUnit2->getID(), pUnit3->getID(),
			pUnit1->groupRank(), pUnit1->qualityRank());

		pUnit0->kill(true, NO_PLAYER, true);
	}
}

void CvUnit::setGGExperienceEarnedTowardsType()
{
	PROFILE_EXTRA_FUNC();
	//TB notes: This has been setup to allow the UnitCombat tag to take multiple entries but has not been setup to manage this in CvUnit.
	//It paves the way for future potential but leaves the design where it is for now where only the first definition the unit finds will be established.
	//(No examples of setting up a singular text reference on a delayed resolution was part of the motivation here)
	//But it could also lead to a deepening of this system later.
	//ONLY the Primary Category UnitCombats should define what type of general pts are generated by that unit.
	//OR Alternatively, subselections of a given Primary Category (for example: UNITCOMBAT_CIVILIAN) could give its sub-selections designations instead (like UNITCOMBAT_LAW_ENFORCEMENT and UNITCOMBAT_HEALER)
	//In this case, we'd probably want a Great Citizen to give pts to UNITCOMBAT_CIVILIAN when settled in the city BUT such a unit would come more from the GP mechanism instead.

	//	The unit's OWN combat classes are the candidate set -- asking the whole registry which ones it has was the
	//	own-data inversion. ⚠ The keyed map is a SUPERSET (an entry is created by any keyed write, e.g. a
	//	promotion-fed heal volume), so the has-test stays; and the map sorts ASCENDING by id, so it is walked in
	//	REVERSE to keep the highest-id-class-wins selection the registry countdown had.
	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_reverse_iterator it = m_unitCombatKeyedInfo.rbegin(), end = m_unitCombatKeyedInfo.rend(); it != end; ++it)
	{
		if (!isHasUnitCombat(it->first))
		{
			continue;
		}
		const std::vector<int>& kGGUnits = GC.getUnitCombatInfo(it->first).getGGPointsForUnits();
		for (int iEntry = (int)kGGUnits.size() - 1; iEntry > -1; iEntry--)
		{
			if (kGGUnits[iEntry] > -1)
			{
				m_eGGExperienceEarnedTowardsType = static_cast<UnitTypes>(kGGUnits[iEntry]);
				return;
			}
		}
	}
	m_eGGExperienceEarnedTowardsType = GC.getUNIT_GREAT_GENERAL();
}

UnitTypes CvUnit::getGGExperienceEarnedTowardsType() const
{
	return m_eGGExperienceEarnedTowardsType;
}

int CvUnit::eraGroupMergeLimit() const
{
	return m_pUnitInfo->getBaseGroupRank() + GET_PLAYER(getOwner()).getCurrentEra() + 1;
}

int CvUnit::eraGroupSplitLimit() const
{
	return std::max(1, m_pUnitInfo->getBaseGroupRank() - GET_PLAYER(getOwner()).getCurrentEra() - 1);
}

DomainTypes CvUnit::getDomainCargo() const
{
	// The promotion-set override is the whole of it: the AUTHORED restriction is the `unit:` predicate
	// qualifier on the unit's cargo.space entries ([modifier.md] par.6), never an info scalar.
	return m_eNewDomainCargo;
}

void CvUnit::setNewDomainCargo(DomainTypes eDomain)
{
	m_eNewDomainCargo = eDomain;
}

SpecialUnitTypes CvUnit::getSpecialCargo() const
{
	if (m_eNewSpecialCargo != NO_SPECIALUNIT)
	{
		return m_eNewSpecialCargo;
	}
	return (SpecialUnitTypes)m_pUnitInfo->getSpecialCargo();
}

void CvUnit::setNewSpecialCargo(SpecialUnitTypes eSpecialUnit)
{
	m_eNewSpecialCargo = eSpecialUnit;
}

SpecialUnitTypes CvUnit::getSMNotSpecialCargo() const
{
	if (m_eNewSMNotSpecialCargo != NO_SPECIALUNIT)
	{
		return m_eNewSMNotSpecialCargo;
	}
	return (SpecialUnitTypes)m_pUnitInfo->getSMNotSpecialCargo();
}

void CvUnit::setNewSMNotSpecialCargo(SpecialUnitTypes eSpecialUnit)
{
	m_eNewSMNotSpecialCargo = eSpecialUnit;
}
//
void CvUnit::changeSMCargoSpace(int iChange)
{
	if (iChange != 0)
	{
		m_iSMCargoCapacity += iChange;
		FASSERT_NOT_NEGATIVE(m_iSMCargoCapacity);
		setInfoBarDirty(true);
	}
}

int CvUnit::SMcargoSpaceFilter() const
{
	if (getSMCargoCapacity() == 0)
	{
		return SMcargoCapacityPreCheck();
	}
	return getSMCargoCapacity();
}

int CvUnit::SMcargoCapacityPreCheck() const
{
	if (isCarrier())
	{
		return std::max(1, 100 + getCargoCapacitybyType(100));
	}
	return 0;
}

int CvUnit::getSMCargoCapacity() const
{
	return m_iSMCargoCapacity;
}

void CvUnit::setSMCargoCapacity()
{
	m_iSMCargoCapacity = applySMRank(
		SMcargoCapacityPreCheck(), getSizeMattersSpacialOffsetValue(), GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER()
		);
	FASSERT_NOT_NEGATIVE(m_iSMCargoCapacity);
}

int CvUnit::getExtraMaxHP() const
{
	return m_iExtraMaxHP;
}

void CvUnit::changeExtraMaxHP(int iChange)
{
	m_iExtraMaxHP += iChange;
}

int CvUnit::getMaxHP() const
{
	int iMaxHP = 0;
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) || getSMHPValue() == 0)
	{
		iMaxHP = HPValueTotalPreCheck();
	}
	else
	{
		iMaxHP = getSMHPValue();
	}
	return std::max(1, iMaxHP);
}

int CvUnit::HPValueTotalPreCheck() const
{
	return std::max(1, m_pUnitInfo->getSizeMatters().maxHP + getExtraMaxHP());
}

int CvUnit::getSMHPValue() const
{
	return m_iSMHPValue;
}

void CvUnit::setSMHPValue()
{
	m_iSMHPValue =
	(
		applySMRank(
			HPValueTotalPreCheck(),
			getSizeMattersOffsetValue(),
			GC.getSIZE_MATTERS_MOST_MULTIPLIER()
		)
	);
	FASSERT_NOT_NEGATIVE(m_iSMHPValue);
}

int CvUnit::getPowerValueTotal() const
{
	return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iSMPowerValue : m_pUnitInfo->getMilitaryWorth();
}

void CvUnit::setSMPowerValue(bool bForLoad)
{
	const int oldSMPowerValue = m_iSMPowerValue;
	m_iSMPowerValue = applySMRank(m_pUnitInfo->getMilitaryWorth(), getSizeMattersOffsetValue(), GC.getSIZE_MATTERS_MOST_MULTIPLIER());
	FASSERT_NOT_NEGATIVE(m_iSMPowerValue);
	if (!bForLoad)
	{
		const int iChange = m_iSMPowerValue - oldSMPowerValue;
		GET_PLAYER(getOwner()).changeUnitPower(iChange);
	}
}

int CvUnit::assetValueTotal() const
{
	return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iSMAssetValue : m_pUnitInfo->getWorth();
}

void CvUnit::setSMAssetValue(bool bForLoad)
{
	const int offsetValue = getSizeMattersOffsetValue();
	if (offsetValue != -15) // Special Case for size cat undefined units
	{
		const int oldSMAssetValue = m_iSMAssetValue;
		m_iSMAssetValue = applySMRank(m_pUnitInfo->getWorth(), offsetValue, GC.getSIZE_MATTERS_MOST_MULTIPLIER());
		if (!bForLoad)
		{
			const int iChange = m_iSMAssetValue - oldSMAssetValue;
			GET_PLAYER(getOwner()).changeAssets(iChange);
		}
		FAssertOptionRecalcMsg(GAMEOPTION_COMBAT_SIZE_MATTERS, m_iSMAssetValue >= 0, "Asset value fell below 0");
	}
}

int CvUnit::getCargoVolumeModifier() const
{
	return m_iSMCargoVolumeModifier;
}

void CvUnit::setCargoVolumeModifier(int iNewValue)
{
	m_iSMCargoVolumeModifier = iNewValue;
}

void CvUnit::changeCargoVolumeModifier(int iChange)
{
	setCargoVolumeModifier(getCargoVolumeModifier() + iChange);
}

int CvUnit::getCargoVolume() const
{
	return m_iSMCargoVolume;
}

void CvUnit::setCargoVolume(int iNewValue)
{
	m_iSMCargoVolume = iNewValue;
}

void CvUnit::changeCargoVolume(int iChange)
{
	setCargoVolume(getCargoVolume() + iChange);
}

int CvUnit::getExtraCargoVolume() const
{
	return m_iSMExtraCargoVolume;
}

void CvUnit::setExtraCargoVolume(int iNewValue)
{
	m_iSMExtraCargoVolume = iNewValue;
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		setSMCargoVolume();
	}
}

void CvUnit::changeExtraCargoVolume(int iChange)
{
	setExtraCargoVolume(getExtraCargoVolume() + iChange);
}

int CvUnit::getSMCargoVolumeBase() const
{
	return std::max(0, 100 + getExtraCargoVolume());
}

int CvUnit::SMCargoVolume() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
		return 0;

	return std::max(1, getCargoVolume() == 0 ? getSMCargoVolumeBase() : getCargoVolume()) + isCarrier() * SMgetCargo();
}

void CvUnit::setSMCargoVolume()
{
	m_iSMCargoVolume =
	(
		std::max(
			1,
			applySMRank(
				getSMCargoVolumeBase(),
				getSizeMattersSpacialOffsetValue(),
				GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER()
			)
		)
	);
}

int CvUnit::getSizeMattersOffsetValue() const
{
	return qualityRank() + groupRank() + sizeRank() - 15;
}

int CvUnit::getSizeMattersSpacialOffsetValue() const
{
	return groupRank() + sizeRank() - 10;
}

int CvUnit::getCargoCapacitybyType(int iValue) const
{
	const SpecialUnitTypes eSpecialUnitDefined = getSpecialCargo();
	const SpecialUnitTypes eSpecialUnitDefinedNot = getSMNotSpecialCargo();

	int rankChange = 0;
	if (eSpecialUnitDefined == GC.getSPECIALUNIT_PEOPLE()
	|| eSpecialUnitDefined == GC.getSPECIALUNIT_MISSILE())
	{
		rankChange = -3;
	}
	else if (eSpecialUnitDefined == GC.getSPECIALUNIT_FIGHTER()
		|| eSpecialUnitDefined == GC.getSPECIALUNIT_SEAPLANE()
		|| eSpecialUnitDefinedNot == GC.getSPECIALUNIT_MISSILE())
	{
		rankChange = -1;
	}
	return applySMRank(iValue, rankChange, GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER());
}

bool CvUnit::isCarrier() const
{
	return getSpecialCargo() != NO_SPECIALUNIT || getDomainCargo() != NO_DOMAIN;
}

bool CvUnit::isUnitAtBaseGroup() const
{
	return m_pUnitInfo->getBaseGroupRank() == groupRank();
}

bool CvUnit::isUnitAboveBaseGroup() const
{
	return groupRank() > m_pUnitInfo->getBaseGroupRank();
}

bool CvUnit::isUnitBelowBaseGroup() const
{
	return groupRank() < m_pUnitInfo->getBaseGroupRank();
}

//Model of how to use Size Matters Most Multiplicative plug in.
//optional - if there is a flat +/- modifier it plugs in here in the Extra functions
//Confusingly, this is often already in place and is sometimes not well named but is convenient to keep as was.
//Often needs to be moved next to those functions below.
int CvUnit::getExtraBombardRate() const
{
	return m_iExtraBombardRate;
}

void CvUnit::changeExtraBombardRate(int iChange)
{
	m_iExtraBombardRate += iChange;
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		setSMBombardRate();
	}
	FASSERT_NOT_NEGATIVE(m_iExtraBombardRate);
}

// The call that plugs into the rest of the code (final value) - this can be plugged into the existing final - or even be renamed to the existing final (though experience has shown me this causes me tremendous confusion!)
int CvUnit::getBombardRate() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{

		return m_iSMBombardRate;
	}
	return std::max(0, m_pUnitInfo->getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) + getExtraBombardRate());
}


////The active call to establish the current proper adjusted value.
////This is the core multiplicative method being utilized.
void CvUnit::setSMBombardRate()
{
	m_iSMBombardRate = applySMRank(std::max(0, m_pUnitInfo->getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) + m_iExtraBombardRate), getSizeMattersOffsetValue(), GC.getSIZE_MATTERS_MOST_MULTIPLIER());

	// optional but most of these should be above or equal to 0.
	FASSERT_NOT_NEGATIVE(m_iSMBombardRate);
}


int CvUnit::getAirBombCurrRate() const
{
	return getAirBombBaseRate() * getHP() / getMaxHP();
}

int CvUnit::getAirBombBaseRate() const//The call that plugs into the rest of the code (final value) - this can be plugged into the existing final - or even be renamed to the existing final (though experience has shown me this causes me tremendous confusion!)
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS)
	//if the current final result of the SMM multiplicative mechanism is nothing but an empty shell
	//then this is the first time it's being run so we take from the base value to start.
	//Either that or the base is 0 anyhow.
	|| getSMAirBombBaseRate() == 0)
	{
		return getSMAirBombBaseRateTotalBase();
	}
	return getSMAirBombBaseRate();
}

int CvUnit::getSMAirBombBaseRateTotalBase() const//The total before the Size Matters multiplicative method adjusts for the final value.
{
	return m_pUnitInfo->getFlatBombard(BOMBARD_AIR_BOMB_RATE, CASC_SCOPE_UNIT) / 100;//Unit base.
}

int CvUnit::getSMAirBombBaseRate() const//The final result of the Multiplicative adjustment
{
	return m_iSMAirBombBaseRate;//A separate (likely new) data storage to track the multiplicated value.
}

//The active call to establish the current proper adjusted value.
//This is the core multiplicative method being utilized.
void CvUnit::setSMAirBombBaseRate()
{
	m_iSMAirBombBaseRate =
	(
		applySMRank
		(
			getSMAirBombBaseRateTotalBase(),
			getSizeMattersOffsetValue(),
			GC.getSIZE_MATTERS_MOST_MULTIPLIER()
		)
	);
	//optional but most of these should be above or equal to 0.
	FASSERT_NOT_NEGATIVE(m_iSMAirBombBaseRate);
	m_iSMAirBombBaseRate = std::max(0, m_iSMAirBombBaseRate);
}

int CvUnit::workRate(bool bMax) const
{
	PROFILE_EXTRA_FUNC();
	if (!bMax && !canMove())
	{
		return 0;
	}
	int iRate = baseWorkRate();

	if (iRate == 0)
	{
		return 0;
	}
	int aiScalars[NUM_INFO_SCALARS];
	GET_PLAYER(getOwner()).getScalars(aiScalars);
	int iWorkMod = getWorkModifier() + aiScalars[SCALAR_WORK_RATE];

	const CvPlot* pPlot = plot();
	for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
	{
		if (pPlot->getFeatureType() == (FeatureTypes)iI)
		{
			iWorkMod += featureWorkPercent((FeatureTypes)iI);
		}
	}
	iWorkMod += terrainWorkPercent(pPlot->getTerrainType());
	{
		const BuildTypes eBuild = getBuildType();

		if (eBuild != NO_BUILD)
		{
			iWorkMod += buildWorkPercent(eBuild);
		}
	}

	if (pPlot->isHills())
	{
		iWorkMod += hillsWorkModifier();
	}
	else if (pPlot->isAsPeak())
	{
		iWorkMod += peaksWorkModifier();
	}

	if (GET_PLAYER(getOwner()).isNormalAI())
	{
		iWorkMod += (
			GC.getHandicapInfo(GC.getGame().getHandicapType()).getScalarModifier(SCALAR_WORK_RATE, CASC_SCOPE_EMPIRE, true)
			-
			GC.getHandicapInfo(GC.getGame().getHandicapType()).getUnitUpkeepEraModifier() * GET_PLAYER(getOwner()).getCurrentEra()
		);
	}
	return getModifiedIntValue(iRate, iWorkMod);
}

// The call that plugs into the rest of the code (final value)
// This can be plugged into the existing final, or even be renamed to the existing final (though experience has shown me this causes me tremendous confusion!)
int CvUnit::baseWorkRate() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS)
	//if the current final result of the SMM multiplicative mechanism is nothing but an empty shell
	//then this is the first time it's being run so we take from the base value to start.
	//Either that or the base is 0 anyhow.
	|| getSMBaseWorkRate() == 0)
	{
		return std::max(0, baseWorkRatePreCheck());
	}
	return std::max(0, getSMBaseWorkRate());
}

int CvUnit::baseWorkRatePreCheck() const//The total before the Size Matters multiplicative method adjusts for the final value.
{
	return std::max(0, m_pUnitInfo->getWorkRate());
}

int CvUnit::getSMBaseWorkRate() const//The final result of the Multiplicative adjustment
{
	return m_iSMBaseWorkRate;//A separate (likely new) data storage to track the multiplicated value.
}

//The active call to establish the current proper adjusted value.
//This is the core multiplicative method being utilized.
void CvUnit::setSMBaseWorkRate()
{
	m_iSMBaseWorkRate =
	(
		applySMRank
		(
			baseWorkRatePreCheck(),
			getSizeMattersSpacialOffsetValue(),
			GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER()
		)
	);
	//optional but most of these should be above or equal to 0.
	FASSERT_NOT_NEGATIVE(m_iSMBaseWorkRate);
	m_iSMBaseWorkRate = std::max(0, m_iSMBaseWorkRate);
}

int CvUnit::getRevoltProtection() const
{
	// The authored share is `culture.unit.garrison`, which mints no getter until its vocabulary + curator call
	// land, so only the runtime accumulator answers meanwhile ([todo.md]: curate culture.unit.garrison).
	return m_iRevoltProtection;
}

void CvUnit::changeRevoltProtection(int iChange)
{
	if (iChange != 0)
	{
		m_iRevoltProtection += iChange;
		/*
		if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
		{
			setSMRevoltProtection();
		}
		*/
		setInfoBarDirty(true);
	}
}

//need to change references to getRevoltProtection to the following:
int CvUnit::revoltProtectionTotal() const
{
	return getRevoltProtection();
}
/* flabbert - disabling size matters revolt protection, it seems to give some weird results
	// The call that plugs into the rest of the code (final value).
	// This can be plugged into the existing final, or even be renamed to the existing final (though experience has shown me this causes me tremendous confusion!).

	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS)
	// if the current final result of the SMM multiplicative mechanism is nothing but an empty shell
	// then this is the first time it's being run so we take from the base value to start.
	// Either that or the base is 0 anyhow.
	|| getSMRevoltProtection() == 0)
	{
		return getRevoltProtection();
	}
	 return std::max(0, getSMRevoltProtection());
}

int CvUnit::getSMRevoltProtection() const//The final result of the Multiplicative adjustment
{
	return m_iSMRevoltProtection;//A separate (likely new) data storage to track the multiplicated value.
}

//The active call to establish the current proper adjusted value.
//This is the core multiplicative method being utilized.
void CvUnit::setSMRevoltProtection()
{
	m_iSMRevoltProtection =
	(
		applySMRank
		(
			getRevoltProtection(),
			getSizeMattersOffsetValue(),
			GC.getSIZE_MATTERS_MOST_MULTIPLIER()
		)
	);
	// optional but most of these should be above or equal to 0.
	FASSERT_NOT_NEGATIVE(m_iSMRevoltProtection);
	m_iSMRevoltProtection = std::max(0, m_iSMRevoltProtection);
}
*/

bool CvUnit::canPerformActionSM() const
{
	return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? isUnitAtBaseGroup() : true;
}

void CvUnit::setSMValues(bool bForLoad)
{
	CvUnit* pTransportUnit = NULL;

	if (!bForLoad && isCargo())
	{
		pTransportUnit = getTransportUnit();
		setTransportUnit(NULL);
	}
	setSMStrength();
	setSMHPValue();
	setSMAssetValue(bForLoad);
	setSMPowerValue(bForLoad);
	setSMCargoCapacity();
	setSMCargoVolume();
	setSMBombardRate();
	setSMAirBombBaseRate();
	setSMBaseWorkRate();
	//setSMRevoltProtection();
	//many missions may require the unit be at base unit defined group size.
		//construct or force a building - code adjusted.
		//
	//property modifiers - huge issues here since they don't compile and become a part of the unit data
		//perhaps if we changed the way that worked they could be modified at the actual unit level.
		//for now property modifying unit types as defined by their general CC categories will have to remain unmergable/unsplittable.
	if (!bForLoad && pTransportUnit != NULL)
	{
		setTransportUnit(pTransportUnit);
	}
}

int CvUnit::getNoSelfHealCount() const
{
	return m_iNoSelfHealCount;
}

bool CvUnit::hasNoSelfHeal() const
{
	return getNoSelfHealCount() + m_pUnitInfo->hasSkill(CLS_SKILL_NO_SELF_HEAL) > 0;
}

void CvUnit::changeNoSelfHealCount(int iChange)
{
	m_iNoSelfHealCount += iChange;
	FASSERT_NOT_NEGATIVE(getNoSelfHealCount());
}

int CvUnit::getSelfHealModifierTotal() const
{
	// A PERCENT, so unscaled; the gather already includes the unit's own type.
	return resolvedValue(URS_HEAL_SELF_MODIFIER);
}

int CvUnit::getNumHealSupportTotal() const
{
	// The gather ALREADY includes the unit's own type, so the type is not added a second time here -- that
	// separate add is exactly what the resolved plane replaces.
	return std::max(0, resolvedValue(URS_HEAL_SUPPORT) / 100);
}

int CvUnit::getHealSupportUsedTotal() const
{
	return m_iHealSupportUsed;
}

void CvUnit::changeHealSupportUsed(int iChange)
{
	m_iHealSupportUsed += iChange;
}

void CvUnit::setHealSupportUsed(int iChange)
{
	m_iHealSupportUsed = iChange;
}

int CvUnit::getHealSupportRemaining() const
{
	return std::max(0, getNumHealSupportTotal() - getHealSupportUsedTotal());
}

bool CvUnit::hasHealSupportRemaining() const
{
	return getHealSupportRemaining() > 0;
}

MissionTypes CvUnit::getSleepType() const
{
	return m_eSleepType;
}

void CvUnit::setSleepType(MissionTypes eSleepType)
{
	m_eSleepType = eSleepType;
}

void CvUnit::establishBuildups()
{
	PROFILE_EXTRA_FUNC();
	m_bHasBuildUp = false;

	for (std::map<PromotionLineTypes, PromotionLineKeyedInfo>::iterator it = m_promotionLineKeyedInfo.begin(), end = m_promotionLineKeyedInfo.end(); it != end; ++it)
	{
		it->second.m_bValidBuildUp = false;
	}

	for (int iI = GC.getNumPromotionLineInfos() - 1; iI > -1; iI--)
	{
		const PromotionLineTypes ePromotionLine = static_cast<PromotionLineTypes>(iI);
		const CvPromotionLineInfo& kPromotionLine = GC.getPromotionLineInfo(ePromotionLine);
		if (kPromotionLine.isBuildUp())
		{
			for (int iJ = 0; iJ < kPromotionLine.getNumPromotions(); iJ++)
			{
				const PromotionTypes ePromotion = (PromotionTypes)kPromotionLine.getPromotion(iJ);
				if (GC.getPromotionInfo(ePromotion).getLinePriority() == 1
				&& canAcquirePromotion(ePromotion, PromotionRequirements::IgnoreHas | PromotionRequirements::ForFree | PromotionRequirements::ForBuildUp))
				{
					PromotionLineKeyedInfo* info = findOrCreatePromotionLineKeyedInfo(ePromotionLine);

					info->m_bValidBuildUp = true;
					m_bHasBuildUp = true;
					break;
				}
			}
		}
	}
}

PromotionLineTypes CvUnit::getBuildUpType() const
{
	return m_eCurrentBuildUpType;
}

void CvUnit::setBuildUpType(PromotionLineTypes ePromotionLine, MissionTypes eSleepType)
{
	PROFILE_EXTRA_FUNC();
	if (isHuman())
	{
		// Buildup chosen
		if (ePromotionLine != NO_PROMOTIONLINE)
		{
			if (m_eCurrentBuildUpType != ePromotionLine)
			{
				if (m_iBuildUpTurns > 0)
				{
					clearBuildups();
				}
				m_bIsBuildUp = true;
				m_eCurrentBuildUpType = ePromotionLine;
			}
			GC.getGame().updateSelectionListInternal();
			return;
		}
		// Choose buildup popup
		if (eSleepType != MISSION_AUTO_BUILDUP && eSleepType != MISSION_HEAL_BUILDUP)
		{
			CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_CHOOSE_BUILDUP);
			pInfo->setData1(getID());
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner());
			return;
		}
	}

	// AI buildup evaluation
	const bool bCanHeal = getHealUnitCombatCount() > 0 || (resolvedValue(URS_HEAL_SAME_TILE) / 100) > 0 || (resolvedValue(URS_HEAL_ADJACENT) / 100) > 0;
	const bool bMustHeal = getDamage() > 0;
	int iBestValue = 0;

	if (isHuman() && eSleepType == MISSION_HEAL_BUILDUP && (bMustHeal || bCanHeal))
	{
		PromotionLineTypes eAssignPromotionLine = NO_PROMOTIONLINE;

		for (std::map<PromotionLineTypes, PromotionLineKeyedInfo>::const_iterator it = m_promotionLineKeyedInfo.begin(), end = m_promotionLineKeyedInfo.end(); it != end; ++it)
		{
			if (it->second.m_bValidBuildUp)
			{
				const PromotionLineTypes ePotentialPromotionLine = it->first;
				const CvPromotionLineInfo& kPotentialPromotionLine = GC.getPromotionLineInfo(ePotentialPromotionLine);

				for (int iI = 0; iI < kPotentialPromotionLine.getNumPromotions(); iI++)
				{
					const PromotionTypes ePromotion = (PromotionTypes)kPotentialPromotionLine.getPromotion(iI);
					const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);

					if (kPromotion.getLinePriority() == 1
					&& canAcquirePromotion(ePromotion, PromotionRequirements::IgnoreHas | PromotionRequirements::ForFree | PromotionRequirements::ForBuildUp))
					{
						int iValue = 0;
						if (bCanHeal)
						{
							std::vector<HealByUnitCombat> healRows;
							InfoValuation::collectHealByUnitCombat(kPromotion.getModifiers(), healRows);
							for (size_t iRow = 0; iRow < healRows.size(); ++iRow)
							{
								const HealByUnitCombat& kRow = healRows[iRow];
								const UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)kRow.iUnitCombat;
								iValue += kRow.iHeal / 100 * getHealUnitCombatTypeTotal(eHealUnitCombat);
								iValue += kRow.iAdjacentHeal / 100 * getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
							}
							iValue += kPromotion.getFlatHeal(HEAL_SAME_TILE, CASC_SCOPE_UNIT) / 100 * 100;
							iValue += kPromotion.getFlatHeal(HEAL_ADJACENT_TILE, CASC_SCOPE_UNIT) / 100 * 10;
						}
						if (bMustHeal)
						{
							iValue += kPromotion.getHealModifier(HEAL_SELF_MODIFIER, CASC_SCOPE_UNIT) * 100;
						}
						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							eAssignPromotionLine = ePotentialPromotionLine;
						}
					}
				}
			}
		}
		if (eAssignPromotionLine == NO_PROMOTIONLINE && isBuildUpable())
		{
			FErrorMsg("This shouldn't happen");
			// Try again
			establishBuildups();

			if (isBuildUpable())
			{
				setBuildUpType(NO_PROMOTIONLINE, MISSION_AUTO_BUILDUP);
			}
			return;
		}

		if (eAssignPromotionLine != m_eCurrentBuildUpType)
		{
			if (m_iBuildUpTurns > 0)
			{
				clearBuildups();
			}
			m_eCurrentBuildUpType = eAssignPromotionLine;

			if (m_eCurrentBuildUpType != NO_PROMOTIONLINE)
			{
				m_bIsBuildUp = true;
			}
		}
		return;
	}
	// Here is were we can implement some AI selection methodology.
	PromotionLineTypes eAssignPromotionLine = NO_PROMOTIONLINE;

	for (std::map<PromotionLineTypes, PromotionLineKeyedInfo>::const_iterator it = m_promotionLineKeyedInfo.begin(), end = m_promotionLineKeyedInfo.end(); it != end; ++it)
	{
		if (it->second.m_bValidBuildUp)
		{
			const PromotionLineTypes ePotentialPromotionLine = it->first;
			const CvPromotionLineInfo& kPotentialPromotionLine = GC.getPromotionLineInfo(ePotentialPromotionLine);
			for (int iI = 0; iI < kPotentialPromotionLine.getNumPromotions(); iI++)
			{
				const PromotionTypes ePromotion = (PromotionTypes)kPotentialPromotionLine.getPromotion(iI);
				if (GC.getPromotionInfo(ePromotion).getLinePriority() == 1
				&& canAcquirePromotion(ePromotion, PromotionRequirements::IgnoreHas | PromotionRequirements::ForFree | PromotionRequirements::ForBuildUp))
				{
					const int iValue = std::max(1, GET_PLAYER(getOwner()).AI_promotionValue(ePromotion, getUnitType(), this, AI_getUnitAIType(), true));

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						eAssignPromotionLine = ePotentialPromotionLine;
					}
				}
			}
		}
	}
	if (eAssignPromotionLine == NO_PROMOTIONLINE && isBuildUpable())
	{
		FErrorMsg("This shouldn't happen");
		// Try again
		establishBuildups();

		if (isBuildUpable())
		{
			setBuildUpType(NO_PROMOTIONLINE, MISSION_AUTO_BUILDUP);
		}
		return;
	}

	if (eAssignPromotionLine != m_eCurrentBuildUpType)
	{
		if (m_iBuildUpTurns > 0)
		{
			clearBuildups();
		}
		m_eCurrentBuildUpType = eAssignPromotionLine;

		if (m_eCurrentBuildUpType != NO_PROMOTIONLINE)
		{
			m_bIsBuildUp = true;
		}
	}
}

void CvUnit::clearBuildups()
{
	PROFILE_EXTRA_FUNC();
	for (int iJ = 0; iJ < GC.getNumPromotionLineInfos(); iJ++)
	{
		if (GC.getPromotionLineInfo((PromotionLineTypes)iJ).isBuildUp())
		{
			const PromotionLineTypes ePromotionLine = ((PromotionLineTypes)iJ);
			for (int iI = 0; iI < GC.getPromotionLineInfo(ePromotionLine).getNumPromotions(); iI++)
			{
				const PromotionTypes ePromotion = (PromotionTypes)GC.getPromotionLineInfo(ePromotionLine).getPromotion(iI);
				if (isHasPromotion(ePromotion))
				{
					setHasPromotion(ePromotion, false, true, false, false);
				}
			}
		}
	}
	m_iBuildUpTurns = 0;
	m_eCurrentBuildUpType = NO_PROMOTIONLINE;
	setSleepType(NO_MISSION);
	m_bIsBuildUp = false;
	setInfoBarDirty(true);
}

void CvUnit::incrementBuildUp()
{
	PROFILE_EXTRA_FUNC();
	if (getBuildUpType() == NO_PROMOTIONLINE)
	{
		FErrorMsg("Units build up status corrupted")
		clearBuildups();
		getGroup()->setActivityType(ACTIVITY_AWAKE);
		return;
	}
	m_iBuildUpTurns++;

	const PromotionLineTypes ePromotionLine = getBuildUpType();

	// AI units will reconsider its buildup on regular intervals.
	if (!isHuman() && 0 == (m_iBuildUpTurns % 11))
	{
		for (int iI = 0; iI < GC.getPromotionLineInfo(ePromotionLine).getNumPromotions(); iI++)
		{
			const PromotionTypes ePromotion = (PromotionTypes)GC.getPromotionLineInfo(ePromotionLine).getPromotion(iI);

			if (GC.getPromotionInfo(ePromotion).getLinePriority() == 1
			&& GET_PLAYER(getOwner()).AI_promotionValue(ePromotion, getUnitType(), this, AI_getUnitAIType(), true) < 10)
			{
				clearBuildups();
				getGroup()->setActivityType(ACTIVITY_AWAKE);
				return;
			}
		}
	}

	for (int iI = 0; iI < GC.getPromotionLineInfo(ePromotionLine).getNumPromotions(); iI++)
	{
		const PromotionTypes ePromotion = (PromotionTypes)GC.getPromotionLineInfo(ePromotionLine).getPromotion(iI);

		if (!isHasPromotion(ePromotion)
		&& GC.getPromotionInfo(ePromotion).getLinePriority() <= m_iBuildUpTurns
		&& canAcquirePromotion(ePromotion, PromotionRequirements::ForFree | PromotionRequirements::ForBuildUp))
		{
			setHasPromotion(ePromotion, true, true, false, false);
		}
	}
}

bool CvUnit::isInhibitMerge() const
{
	return m_bInhibitMerge;
}

void CvUnit::setInhibitMerge(bool bNewValue)
{
	m_bInhibitMerge = bNewValue;
}

bool CvUnit::isInhibitSplit() const
{
	return m_bInhibitSplit;
}

void CvUnit::setInhibitSplit(bool bNewValue)
{
	m_bInhibitSplit = bNewValue;
}

bool CvUnit::isBuildUp() const
{
	return m_bIsBuildUp;
}

void CvUnit::setSpecialUnit(bool bChange, SpecialUnitTypes eSpecialUnit)
{
	m_eSpecialUnit = bChange ? eSpecialUnit : (SpecialUnitTypes)m_pUnitInfo->getSpecialUnitType();
}

bool CvUnit::isHiddenNationality() const
{
	return 0 < getHiddenNationalityCount() + m_pUnitInfo->hasSkill(CLS_SKILL_HIDDEN_NATIONALITY);
}

bool CvUnit::hasBuild(BuildTypes eBuild) const
{
	return isWorker() && (m_pUnitInfo->hasBuild(eBuild) || m_worker->hasExtraBuild(eBuild));
}

void CvUnit::changeExtraBuildType(bool bChange, BuildTypes eBuild)
{
	if (eBuild != NO_BUILD)
	{
		if (bChange)
		{
			if (!isWorker())
			{
				m_worker = new UnitCompWorker();
			}
			m_worker->setExtraBuild(eBuild);
		}
		else if (isWorker())
		{
			m_worker->setExtraBuild(eBuild, false);

			if ((int)m_pUnitInfo->getBuilds().size() == 0 && m_worker->getExtraBuilds().size() == 0)
			{
				CvCity* city = GET_PLAYER(getOwner()).getCity(m_worker->getAssignedCity());
				if (city)
				{
					OutputDebugString(CvString::format("Worker at (%d,%d) stopped being a worker with mission for city %S\n", getX(), getY(), city->getName().GetCString()).c_str());
					city->setWorkerHave(getID(), false);
				}
				delete m_worker;
				m_worker = NULL;
			}
		}
	}
}

bool CvUnit::isExcile() const
{
	int iCount = m_iExcileCount;
	if (m_pUnitInfo->hasSkill(CLS_SKILL_EXCILE))
	{
		iCount++;
	}
	return (iCount > 0);
}

void CvUnit::changeExcileCount(int iChange)
{
	m_iExcileCount += iChange;
}

bool CvUnit::isPassage() const
{
	int iCount = m_iPassageCount;
	if (m_pUnitInfo->hasSkill(CLS_SKILL_PASSAGE))
	{
		iCount++;
	}
	return (iCount > 0);
}

void CvUnit::changePassageCount(int iChange)
{
	m_iPassageCount += iChange;
}

bool CvUnit::isNoNonOwnedCityEntry() const
{
	int iCount = m_iNoNonOwnedCityEntryCount;
	if (m_pUnitInfo->hasSkill(CLS_SKILL_NO_NON_OWNED_CITY_ENTRY))
	{
		iCount++;
	}
	return (iCount > 0);
}

void CvUnit::changeNoNonOwnedCityEntryCount(int iChange)
{
	m_iNoNonOwnedCityEntryCount += iChange;
}

bool CvUnit::isBarbCoExist() const
{
	return m_iBarbCoExistCount + m_pUnitInfo->hasSkill(CLS_SKILL_BARB_CO_EXIST) > 0;
}

void CvUnit::changeBarbCoExistCount(int iChange)
{
	m_iBarbCoExistCount += iChange;
}

bool CvUnit::isBlendIntoCity() const
{
	int iCount = m_iBlendIntoCityCount;
	if (m_pUnitInfo->hasSkill(CLS_SKILL_BLEND_INTO_CITY))
	{
		iCount++;
	}
	return (iCount > 0 || (isAnimal() && canAnimalIgnoresCities()));
}

void CvUnit::changeBlendIntoCityCount(int iChange)
{
	m_iBlendIntoCityCount += iChange;
}

bool CvUnit::isUpgradeAnywhere() const
{
	int iCount = m_iUpgradeAnywhereCount;
	if (m_pUnitInfo->hasSkill(CLS_SKILL_UPGRADE_ANYWHERE))
	{
		iCount++;
	}
	return (iCount > 0);
}

void CvUnit::changeUpgradeAnywhereCount(int iChange)
{
	m_iUpgradeAnywhereCount += iChange;
}

// Only used when unit spot stats change, not when the sight area of this unit change.
void CvUnit::updateSpotIntensity(const InvisibleTypes eInvisibleType, const bool bSameTile)
{
	PROFILE_EXTRA_FUNC();
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		return;
	}
	std::vector<InvisibleTypes> aSeeInvisibleTypes;

	if (eInvisibleType == NO_INVISIBLE)
	{
		for (int iI = GC.getNumInvisibleInfos() - 1; iI > -1; iI--)
		{
			aSeeInvisibleTypes.push_back(static_cast<InvisibleTypes>(iI));
		}
	}
	else aSeeInvisibleTypes.push_back(eInvisibleType);

	const bool bAerial = getDomainType() == DOMAIN_AIR;

	const int iRange = bSameTile ? 0 : sight(plot());

	for (int i = aSeeInvisibleTypes.size() - 1; i > -1; i--)
	{
		const InvisibleTypes eInvisible = aSeeInvisibleTypes[i];

		for (int dx = -iRange; dx <= iRange; dx++)
		{
			for (int dy = -iRange; dy <= iRange; dy++)
			{
				CvPlot* pPlot = plotXY(getX(), getY(), dx, dy);

				if (NULL != pPlot && (bAerial || plot()->canSeePlot(pPlot, sight())))
				{
					// The same registration as CvPlot::changeAdjacentSight, and the same rule: DETECTION with
					// no distance attenuation and no spot range, because reach is vision's (vision.md §4).
					pPlot->setSpotIntensity(getTeam(), eInvisible, getID(), detectionAgainst(GC.getMethodSkill(eInvisible)));
				}
			}
		}
	}
}

int CvUnit::getExtraVisibilityIntensityType(InvisibleTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);

	if (!isCommander())
	{
		const CvUnit* pCommander = getCommander();
		if (pCommander)
		{
			return m_aiExtraVisibilityIntensity[eIndex] + pCommander->m_aiExtraVisibilityIntensity[eIndex];
		}
	}
	if (!isCommodore())
    	{
    		const CvUnit* pCommodore = getCommodore();
    		if (pCommodore)
    		{
    			return m_aiExtraVisibilityIntensity[eIndex] + pCommodore->m_aiExtraVisibilityIntensity[eIndex];
    		}
    	}
	return m_aiExtraVisibilityIntensity[eIndex];
}


void CvUnit::changeExtraVisibilityIntensityType(InvisibleTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);
	if (iChange != 0)
	{
		m_aiExtraVisibilityIntensity[eIndex] += iChange;
		updateSpotIntensity(eIndex);
	}
}

bool CvUnit::hasInvisibilityType(InvisibleTypes eInvisibleType) const
{
	//	⛔ MEMBERSHIP FIRST — a unit is hidden only by a method it actually HIDES BY. This test was a pure
	//	negation filter, so it answered TRUE for all 14 methods on any unit that negated none of them; and
	//	isInvisible's first clause returns INVISIBLE for a method no seer has registered against, which is
	//	nearly all of them. The method is a SKILL (vision.md §4), so holding it is the membership question.
	const int iMethodSkill = GC.getMethodSkill(eInvisibleType);

	if (iMethodSkill < 0 || !getUnitInfo().hasSkill(iMethodSkill))
	{
		return false;
	}
	return !isNegatesInvisible(eInvisibleType) && !m_pUnitInfo->hasSkill(CLS_SKILL_NO_INVISIBILITY) && getNoInvisibilityCount() < 1;
}

int CvUnit::getExtraInvisibilityIntensityType(InvisibleTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);

	if (!isCommander())
	{
		const CvUnit* pCommander = getCommander();
		if (pCommander)
		{
			return m_aiExtraInvisibilityIntensity[eIndex] + pCommander->m_aiExtraInvisibilityIntensity[eIndex];
		}
	}
	if (!isCommodore())
    	{
    		const CvUnit* pCommodore = getCommodore();
    		if (pCommodore)
    		{
    			return m_aiExtraInvisibilityIntensity[eIndex] + pCommodore->m_aiExtraInvisibilityIntensity[eIndex];
    		}
    	}
	return m_aiExtraInvisibilityIntensity[eIndex];
}


void CvUnit::changeExtraInvisibilityIntensityType(InvisibleTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);
	m_aiExtraInvisibilityIntensity[eIndex] += iChange;
}

bool CvUnit::hasAnyInvisibilityType() const
{
	return m_bHasAnyInvisibility;
}

void CvUnit::setHasAnyInvisibility()
{
	PROFILE_EXTRA_FUNC();
	if (m_pUnitInfo->hasSkill(CLS_SKILL_NO_INVISIBILITY) || getNoInvisibilityCount() > 0)
	{
		m_bHasAnyInvisibility = false;
		return;
	}
	for (int iI = GC.getNumInvisibleInfos() - 1; iI > -1; iI--)
	{
		//	The same membership question hasInvisibilityType now asks, so the cached "does this unit hide at
		//	all" answer cannot disagree with the per-method one it gates.
		if (hasInvisibilityType((InvisibleTypes)iI))
		{
			m_bHasAnyInvisibility = true;
			return;
		}
	}
	m_bHasAnyInvisibility = false;
}

int CvUnit::getExtraVisibilityIntensityRangeType(InvisibleTypes eIndex) const
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);
	return m_aiExtraVisibilityIntensityRange[eIndex];
}


void CvUnit::changeExtraVisibilityIntensityRangeType(InvisibleTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);
	if (iChange != 0)
	{
		m_aiExtraVisibilityIntensityRange[eIndex] += iChange;
		updateSpotIntensity(eIndex);
	}
}

void CvUnit::changeExtraVisibilityIntensitySameTileType(InvisibleTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eIndex);
	if (iChange != 0)
	{
		m_aiExtraVisibilityIntensitySameTile[eIndex] += iChange;
		updateSpotIntensity(eIndex, true);
	}
}

int CvUnit::getNumExtraInvisibleTerrains() const
{
	return (int)m_aExtraInvisibleTerrains.size();
}

InvisibleTerrainChanges& CvUnit::getExtraInvisibleTerrain(int iIndex)
{
	return m_aExtraInvisibleTerrains[iIndex];
}

void CvUnit::changeExtraInvisibleTerrain(InvisibleTypes eInvisible, TerrainTypes eTerrain, int iChange)
{
	PROFILE_EXTRA_FUNC();
	bool bFound = false;
	int iSize = getNumExtraInvisibleTerrains();
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraInvisibleTerrains[iI].eInvisible == eInvisible && m_aExtraInvisibleTerrains[iI].eTerrain == eTerrain)
		{
			m_aExtraInvisibleTerrains[iI].iIntensity += iChange;
			if (m_aExtraInvisibleTerrains[iI].iIntensity == 0)
			{
				m_aExtraInvisibleTerrains.erase(m_aExtraInvisibleTerrains.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		const int iISize = iSize;
		iSize++;
		m_aExtraInvisibleTerrains.resize(iSize);
		m_aExtraInvisibleTerrains[iISize].eInvisible = eInvisible;
		m_aExtraInvisibleTerrains[iISize].eTerrain = eTerrain;
		m_aExtraInvisibleTerrains[iISize].iIntensity = iChange;
	}
}

int CvUnit::extraInvisibleTerrain(InvisibleTypes eInvisible, TerrainTypes eTerrain) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraInvisibleTerrains(); iI++)
	{
		if (m_aExtraInvisibleTerrains[iI].eInvisible == eInvisible && m_aExtraInvisibleTerrains[iI].eTerrain == eTerrain)
		{
			return m_aExtraInvisibleTerrains[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraInvisibleFeatures() const
{
	return (int)m_aExtraInvisibleFeatures.size();
}

InvisibleFeatureChanges& CvUnit::getExtraInvisibleFeature(int iIndex)
{
	return m_aExtraInvisibleFeatures[iIndex];
}

void CvUnit::changeExtraInvisibleFeature(InvisibleTypes eInvisible, FeatureTypes eFeature, int iChange)
{
	PROFILE_EXTRA_FUNC();
	bool bFound = false;
	int iSize = getNumExtraInvisibleFeatures();
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraInvisibleFeatures[iI].eInvisible == eInvisible && m_aExtraInvisibleFeatures[iI].eFeature == eFeature)
		{
			m_aExtraInvisibleFeatures[iI].iIntensity += iChange;
			if (m_aExtraInvisibleFeatures[iI].iIntensity == 0)
			{
				m_aExtraInvisibleFeatures.erase(m_aExtraInvisibleFeatures.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		const int iISize = iSize;
		iSize++;
		m_aExtraInvisibleFeatures.resize(iSize);
		m_aExtraInvisibleFeatures[iISize].eInvisible = eInvisible;
		m_aExtraInvisibleFeatures[iISize].eFeature = eFeature;
		m_aExtraInvisibleFeatures[iISize].iIntensity = iChange;
	}
}

int CvUnit::extraInvisibleFeature(InvisibleTypes eInvisible, FeatureTypes eFeature) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraInvisibleFeatures(); iI++)
	{
		if (m_aExtraInvisibleFeatures[iI].eInvisible == eInvisible && m_aExtraInvisibleFeatures[iI].eFeature == eFeature)
		{
			return m_aExtraInvisibleFeatures[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraInvisibleImprovements() const
{
	return (int)m_aExtraInvisibleImprovements.size();
}

InvisibleImprovementChanges& CvUnit::getExtraInvisibleImprovement(int iIndex)
{
	return m_aExtraInvisibleImprovements[iIndex];
}

void CvUnit::changeExtraInvisibleImprovement(InvisibleTypes eInvisible, ImprovementTypes eImprovement, int iChange)
{
	PROFILE_EXTRA_FUNC();
	bool bFound = false;
	int iSize = getNumExtraInvisibleImprovements();
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraInvisibleImprovements[iI].eInvisible == eInvisible && m_aExtraInvisibleImprovements[iI].eImprovement == eImprovement)
		{
			m_aExtraInvisibleImprovements[iI].iIntensity += iChange;
			if (m_aExtraInvisibleImprovements[iI].iIntensity == 0)
			{
				m_aExtraInvisibleImprovements.erase(m_aExtraInvisibleImprovements.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		const int iISize = iSize;
		iSize++;
		m_aExtraInvisibleImprovements.resize(iSize);
		m_aExtraInvisibleImprovements[iISize].eInvisible = eInvisible;
		m_aExtraInvisibleImprovements[iISize].eImprovement = eImprovement;
		m_aExtraInvisibleImprovements[iISize].iIntensity = iChange;
	}
}

int CvUnit::extraInvisibleImprovement(InvisibleTypes eInvisible, ImprovementTypes eImprovement) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraInvisibleImprovements(); iI++)
	{
		if (m_aExtraInvisibleImprovements[iI].eInvisible == eInvisible && m_aExtraInvisibleImprovements[iI].eImprovement == eImprovement)
		{
			return m_aExtraInvisibleImprovements[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraVisibleTerrains() const
{
	return (int)m_aExtraVisibleTerrains.size();
}

InvisibleTerrainChanges& CvUnit::getExtraVisibleTerrain(int iIndex)
{
	return m_aExtraVisibleTerrains[iIndex];
}

void CvUnit::changeExtraVisibleTerrain(InvisibleTypes eInvisible, TerrainTypes eTerrain, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	bool bFound = false;
	const int iSize = getNumExtraVisibleTerrains();
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraVisibleTerrains[iI].eInvisible == eInvisible && m_aExtraVisibleTerrains[iI].eTerrain == eTerrain)
		{
			m_aExtraVisibleTerrains[iI].iIntensity += iChange;
			if (m_aExtraVisibleTerrains[iI].iIntensity == 0)
			{
				m_aExtraVisibleTerrains.erase(m_aExtraVisibleTerrains.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		m_aExtraVisibleTerrains.resize(iSize + 1);
		m_aExtraVisibleTerrains[iSize].eInvisible = eInvisible;
		m_aExtraVisibleTerrains[iSize].eTerrain = eTerrain;
		m_aExtraVisibleTerrains[iSize].iIntensity = iChange;
	}
	updateSpotIntensity(eInvisible);
}

int CvUnit::extraVisibleTerrain(InvisibleTypes eInvisible, TerrainTypes eTerrain) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraVisibleTerrains(); iI++)
	{
		if (m_aExtraVisibleTerrains[iI].eInvisible == eInvisible && m_aExtraVisibleTerrains[iI].eTerrain == eTerrain)
		{
			return m_aExtraVisibleTerrains[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraVisibleFeatures() const
{
	return (int)m_aExtraVisibleFeatures.size();
}

InvisibleFeatureChanges& CvUnit::getExtraVisibleFeature(int iIndex)
{
	return m_aExtraVisibleFeatures[iIndex];
}

void CvUnit::changeExtraVisibleFeature(InvisibleTypes eInvisible, FeatureTypes eFeature, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	const int iSize = getNumExtraVisibleFeatures();
	bool bFound = false;
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraVisibleFeatures[iI].eInvisible == eInvisible && m_aExtraVisibleFeatures[iI].eFeature == eFeature)
		{
			m_aExtraVisibleFeatures[iI].iIntensity += iChange;
			if (m_aExtraVisibleFeatures[iI].iIntensity == 0)
			{
				m_aExtraVisibleFeatures.erase(m_aExtraVisibleFeatures.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		m_aExtraVisibleFeatures.resize(iSize + 1);
		m_aExtraVisibleFeatures[iSize].eInvisible = eInvisible;
		m_aExtraVisibleFeatures[iSize].eFeature = eFeature;
		m_aExtraVisibleFeatures[iSize].iIntensity = iChange;
	}
	updateSpotIntensity(eInvisible);
}

int CvUnit::extraVisibleFeature(InvisibleTypes eInvisible, FeatureTypes eFeature) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraVisibleFeatures(); iI++)
	{
		if (m_aExtraVisibleFeatures[iI].eInvisible == eInvisible && m_aExtraVisibleFeatures[iI].eFeature == eFeature)
		{
			return m_aExtraVisibleFeatures[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraVisibleImprovements() const
{
	return (int)m_aExtraVisibleImprovements.size();
}

InvisibleImprovementChanges& CvUnit::getExtraVisibleImprovement(int iIndex)
{
	return m_aExtraVisibleImprovements[iIndex];
}

void CvUnit::changeExtraVisibleImprovement(InvisibleTypes eInvisible, ImprovementTypes eImprovement, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	const int iSize = getNumExtraVisibleImprovements();
	bool bFound = false;
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraVisibleImprovements[iI].eInvisible == eInvisible && m_aExtraVisibleImprovements[iI].eImprovement == eImprovement)
		{
			m_aExtraVisibleImprovements[iI].iIntensity += iChange;
			if (m_aExtraVisibleImprovements[iI].iIntensity == 0)
			{
				m_aExtraVisibleImprovements.erase(m_aExtraVisibleImprovements.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		m_aExtraVisibleImprovements.resize(iSize + 1);
		m_aExtraVisibleImprovements[iSize].eInvisible = eInvisible;
		m_aExtraVisibleImprovements[iSize].eImprovement = eImprovement;
		m_aExtraVisibleImprovements[iSize].iIntensity = iChange;
	}
	updateSpotIntensity(eInvisible);
}

int CvUnit::extraVisibleImprovement(InvisibleTypes eInvisible, ImprovementTypes eImprovement) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraVisibleImprovements(); iI++)
	{
		if (m_aExtraVisibleImprovements[iI].eInvisible == eInvisible && m_aExtraVisibleImprovements[iI].eImprovement == eImprovement)
		{
			return m_aExtraVisibleImprovements[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraVisibleTerrainRanges() const
{
	return (int)m_aExtraVisibleTerrainRanges.size();
}

InvisibleTerrainChanges& CvUnit::getExtraVisibleTerrainRange(int iIndex)
{
	return m_aExtraVisibleTerrainRanges[iIndex];
}

void CvUnit::changeExtraVisibleTerrainRange(InvisibleTypes eInvisible, TerrainTypes eTerrain, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	const int iSize = getNumExtraVisibleTerrainRanges();
	bool bFound = false;
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraVisibleTerrainRanges[iI].eInvisible == eInvisible && m_aExtraVisibleTerrainRanges[iI].eTerrain == eTerrain)
		{
			m_aExtraVisibleTerrainRanges[iI].iIntensity += iChange;
			if (m_aExtraVisibleTerrainRanges[iI].iIntensity == 0)
			{
				m_aExtraVisibleTerrainRanges.erase(m_aExtraVisibleTerrainRanges.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		m_aExtraVisibleTerrainRanges.resize(iSize + 1);
		m_aExtraVisibleTerrainRanges[iSize].eInvisible = eInvisible;
		m_aExtraVisibleTerrainRanges[iSize].eTerrain = eTerrain;
		m_aExtraVisibleTerrainRanges[iSize].iIntensity = iChange;
	}
	updateSpotIntensity(eInvisible);
}

int CvUnit::extraVisibleTerrainRange(InvisibleTypes eInvisible, TerrainTypes eTerrain) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraVisibleTerrainRanges(); iI++)
	{
		if (m_aExtraVisibleTerrainRanges[iI].eInvisible == eInvisible && m_aExtraVisibleTerrainRanges[iI].eTerrain == eTerrain)
		{
			return m_aExtraVisibleTerrainRanges[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraVisibleFeatureRanges() const
{
	return (int)m_aExtraVisibleFeatureRanges.size();
}

InvisibleFeatureChanges& CvUnit::getExtraVisibleFeatureRange(int iIndex)
{
	return m_aExtraVisibleFeatureRanges[iIndex];
}

void CvUnit::changeExtraVisibleFeatureRange(InvisibleTypes eInvisible, FeatureTypes eFeature, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	const int iSize = getNumExtraVisibleFeatureRanges();
	bool bFound = false;
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraVisibleFeatureRanges[iI].eInvisible == eInvisible && m_aExtraVisibleFeatureRanges[iI].eFeature == eFeature)
		{
			m_aExtraVisibleFeatureRanges[iI].iIntensity += iChange;
			if (m_aExtraVisibleFeatureRanges[iI].iIntensity == 0)
			{
				m_aExtraVisibleFeatureRanges.erase(m_aExtraVisibleFeatureRanges.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		m_aExtraVisibleFeatureRanges.resize(iSize + 1);
		m_aExtraVisibleFeatureRanges[iSize].eInvisible = eInvisible;
		m_aExtraVisibleFeatureRanges[iSize].eFeature = eFeature;
		m_aExtraVisibleFeatureRanges[iSize].iIntensity = iChange;
	}
	updateSpotIntensity(eInvisible);
}

int CvUnit::extraVisibleFeatureRange(InvisibleTypes eInvisible, FeatureTypes eFeature) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraVisibleFeatureRanges(); iI++)
	{
		if (m_aExtraVisibleFeatureRanges[iI].eInvisible == eInvisible && m_aExtraVisibleFeatureRanges[iI].eFeature == eFeature)
		{
			return m_aExtraVisibleFeatureRanges[iI].iIntensity;
		}
	}
	return 0;
}

int CvUnit::getNumExtraVisibleImprovementRanges() const
{
	return (int)m_aExtraVisibleImprovementRanges.size();
}

InvisibleImprovementChanges& CvUnit::getExtraVisibleImprovementRange(int iIndex)
{
	return m_aExtraVisibleImprovementRanges[iIndex];
}

void CvUnit::changeExtraVisibleImprovementRange(InvisibleTypes eInvisible, ImprovementTypes eImprovement, int iChange)
{
	PROFILE_EXTRA_FUNC();
	if (iChange == 0)
	{
		return;
	}
	const int iSize = getNumExtraVisibleImprovementRanges();
	bool bFound = false;
	for (int iI = 0; iI < iSize; iI++)
	{
		if (m_aExtraVisibleImprovementRanges[iI].eInvisible == eInvisible && m_aExtraVisibleImprovementRanges[iI].eImprovement == eImprovement)
		{
			m_aExtraVisibleImprovementRanges[iI].iIntensity += iChange;
			if (m_aExtraVisibleImprovementRanges[iI].iIntensity == 0)
			{
				m_aExtraVisibleImprovementRanges.erase(m_aExtraVisibleImprovementRanges.begin()+iI);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		m_aExtraVisibleImprovementRanges.resize(iSize + 1);
		m_aExtraVisibleImprovementRanges[iSize].eInvisible = eInvisible;
		m_aExtraVisibleImprovementRanges[iSize].eImprovement = eImprovement;
		m_aExtraVisibleImprovementRanges[iSize].iIntensity = iChange;
	}
	updateSpotIntensity(eInvisible);
}

int CvUnit::extraVisibleImprovementRange(InvisibleTypes eInvisible, ImprovementTypes eImprovement) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < getNumExtraVisibleImprovementRanges(); iI++)
	{
		if (m_aExtraVisibleImprovementRanges[iI].eInvisible == eInvisible && m_aExtraVisibleImprovementRanges[iI].eImprovement == eImprovement)
		{
			return m_aExtraVisibleImprovementRanges[iI].iIntensity;
		}
	}
	return 0;
}


bool CvUnit::isNegatesInvisible(InvisibleTypes eInvisible) const
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eInvisible);

	if (GC.getInvisibleInfo(eInvisible).isIntrinsic() && !GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		return true;
	}
	return (getNegatesInvisibleCount(eInvisible) > 0 || isRevealed());
}
int CvUnit::getNegatesInvisibleCount(InvisibleTypes eInvisible) const
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eInvisible);
	return m_aiNegatesInvisibleCount[eInvisible];
}
void CvUnit::changeNegatesInvisibleCount(InvisibleTypes eInvisible, int iChange)
{
	FASSERT_BOUNDS(0, GC.getNumInvisibleInfos(), eInvisible);
	m_aiNegatesInvisibleCount[eInvisible] += iChange;
	setHasAnyInvisibility();
}

bool CvUnit::hasInvisibleAbility() const
{
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		return hasAnyInvisibilityType();
	}

	if (getInvisibleType() != NO_INVISIBLE)
	{
		return true;
	}

	return false;
}

bool CvUnit::isCriminal() const
{
	return getInsidiousnessTotal(true) > 0;
}

int CvUnit::getInsidiousnessTotal(bool bCriminalCheck) const
{
	int iTotal = m_pUnitInfo->getUnderworld(UNDERWORLD_INSIDIOUSNESS, CASC_SCOPE_UNIT) / 100;
	iTotal += m_iExtraInsidiousness;
	if (!bCriminalCheck && iTotal > 0)
	{
		if (plot() != NULL)
		{
			const CvCity* pCity = plot()->getPlotCity();
			if (pCity != NULL)
			{
				iTotal += pCity->getExtraInsidiousness();
			}
		}
	}
	return iTotal;
}

void CvUnit::changeExtraInsidiousness(int iChange)
{
	m_iExtraInsidiousness += iChange;
}

int CvUnit::getInvestigationTotal() const
{
	int iTotal = m_pUnitInfo->getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100;
	iTotal += m_iExtraInvestigation;
	return iTotal;
}

void CvUnit::changeExtraInvestigation(int iChange)
{
	m_iExtraInvestigation += iChange;
}

bool CvUnit::criminalSuccessCheck()
{
	int iDice = 1000;
	int iInsidious = getInsidiousnessTotal();
	int iInvestigate = 0;
	int iChance = 0;
	int iRoll = 0;

	if (plot() == NULL)
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();
	bool bSuccess = true;
	if (pCity != NULL)
	{
		iInvestigate = pCity->getInvestigationTotal(true);
		iChance = std::max(0, iInvestigate - iInsidious);
		iRoll = GC.getGame().getSorenRandNum(iDice, "InvestigationRoll");
		if (iRoll < iChance)
		{
			makeWanted(pCity);
			bSuccess = false;
		}
		else
		{
			//Avoided being Investigated
			changeExperience100(5);
		}
	}
	return bSuccess;
}

void CvUnit::doInsidiousnessVSInvestigationCheck()
{
	int iDice = 1000;
	int iInsidious = getInsidiousnessTotal();
	int iInvestigate = 0;
	int iChance = 0;
	int iRoll = 0;

	if (plot() == NULL)
	{
		return;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity != NULL)
	{
		iInvestigate = pCity->getInvestigationTotal(true);
		iChance = std::max(0, iInvestigate - iInsidious);
		iRoll = GC.getGame().getSorenRandNum(iDice, "InvestigationRoll");
		if (iRoll < iChance)
		{
			makeWanted(pCity);
		}
		else
		{
			//Avoided being Investigated
			changeExperience100(5);
		}
	}
}

void CvUnit::doRemoveInvestigatedPromotionCheck()
{
	PROFILE_EXTRA_FUNC();
	if (plot() != NULL &&
		(!plot()->isVisible(GET_PLAYER(m_pPlayerInvestigated).getTeam(), false) ||
		(plot()->getOwner() != m_pPlayerInvestigated && (isInvisible(GET_PLAYER(m_pPlayerInvestigated).getTeam(), false, false) || m_pPlayerInvestigated == getOwner()))))
	{
		int iI = 0;
		for (iI = 0; iI < GC.getNumPromotionInfos(); iI++)
		{
			if (GC.getPromotionInfo((PromotionTypes)iI).isSetOnInvestigated() && isHasPromotion((PromotionTypes)iI) )
			{
				PromotionTypes ePromotion = ((PromotionTypes)iI);
				setHasPromotion(ePromotion, false, true, false, false);
				m_pPlayerInvestigated = NO_PLAYER;
			}
		}
	}
}

bool CvUnit::isWantedbyPlayer(PlayerTypes ePlayer) const
{
	return (m_pPlayerInvestigated == ePlayer);
}

bool CvUnit::isWanted() const
{
	return (m_pPlayerInvestigated != NO_PLAYER);
}


void CvUnit::attackSamePlotSpecifiedUnit(CvUnit* pSelectedDefender)
{
	PROFILE_FUNC();

	FAssert(getCombatTimer() == 0);
	m_combatResult.iTurnCount = 0;
	//TB Note: No Strength in numbers possible on such a same plot attack.
	CvPlot* pPlot = plot();
	setAttackPlot(pPlot, false);

	updateCombat(pSelectedDefender, true);
}

bool CvUnit::canArrest() const
{
	//is Law Enforcement? - does not check city modifiers, only base
	if (getInvestigationTotal() > 0)
	{
		if (isCargo())
		{
			return false;
		}
		const CvPlot* pPlot = plot();
		if (canMove() && canAttack() && !isDead() && !isInBattle() && !isCargo()) // && getGroup()->getNumUnits() == 1)
		{
			if (pPlot != NULL)
			{
				if (!(pPlot->isValidDomainForAction(*this)))
				{
					return false;
				}
				if (pPlot->getNumVisibleWantedCriminals(getOwner()) > 0)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CvUnit::doArrest()
{
	PROFILE_EXTRA_FUNC();

	if (isHuman())
	{
		CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_CHOOSE_ARREST_UNIT);
		pInfo->setData1(getID());
		pInfo->setData2(getX());
		pInfo->setData3(getY());
		pInfo->setFlags(getOwner());
		gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
		return;
	}
	CvUnit* pBestUnit = NULL;
	{
		int iBestOdds = 0;
		foreach_(CvUnit* unitX, plot()->units())
		{
			if (unitX->isWanted()
			&&  unitX->getID() != getID()
			&& !unitX->isInvisible(GET_PLAYER(getOwner()).getTeam(), false)
			&& !unitX->isDead()
			&& !unitX->isInBattle()
			&& !unitX->isSpy())
			{
				const int iOdds = getCombatOdds(this, unitX);
				if (iOdds > 50 && iOdds > iBestOdds)
				{
					iBestOdds = iOdds;
					pBestUnit = unitX;
				}
			}
		}
	}
	if (pBestUnit)
	{
		attackSamePlotSpecifiedUnit(pBestUnit);
		setMadeAttack(true);
		changeMoves(GC.getMOVE_DENOMINATOR());
	}
}

bool CvUnit::canAmbush(const CvPlot* pPlot, bool bAssassinate) const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return false;
	}

	if (pPlot == NULL)
	{
		return false;
	}

	if (!canAttack())
	{
		return false;
	}

	if (isCargo())
	{
		return false;
	}

	if (!(pPlot->isValidDomainForAction(*this)))
	{
		return false;
	}

	if (bAssassinate && !isAssassin())
	{
		return false;
	}

	if (!bAssassinate && pPlot->isCity(false)) //true->false Calvitix (to be able to attack animals in fort)
	{
		return false;
	}

	// Inside a proper city, assassins cannot engage criminals — wanted or not.
    // Non-wanted criminals are civilians and are protected.
    // Wanted criminals must be handled by law enforcement (arrest), not assassination.
    // Forts are excluded so assassins can still hunt criminals hiding in the wilderness.
    if (bAssassinate && pPlot->isCity(true))
    {
    	bool bHasNonCriminalTarget = false;
    	foreach_(const CvUnit* pLoopUnit, pPlot->units())
    	{
    		if (pLoopUnit->getTeam() == getTeam()) continue;
    		if (pLoopUnit->isDead() || pLoopUnit->isInBattle()) continue;
    		if (pLoopUnit->isInvisible(getTeam(), false)) continue;

    		// Criminals in cities are off-limits to assassins regardless of wanted status
    		if (pLoopUnit->isCriminal() && !pLoopUnit->isAnimal())
    		{
    			continue;
    		}

    		bHasNonCriminalTarget = true;
    		break;
    	}
    	if (!bHasNonCriminalTarget)
    	{
    		return false;
    	}
    }

	if (isBlitz() || !isMadeAttack())
	{
		const CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), this, true, true, false, bAssassinate);
		if (pDefender != NULL)
		{
			if (!pDefender->isInvisible(getTeam(), false))
			{
				return true;
			}
		}
		if (pPlot->isVisiblePotentialEnemyDefender(this) || pPlot->isVisiblePotentialEnemyDefenderless(this))
		{
			foreach_(CvUnit* pLoopUnit, pPlot->units())
			{
				if (bAssassinate && !pLoopUnit->isTargetOf(*this))
				{
					continue;
				}
				if (canAttack(*pLoopUnit))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool CvUnit::doAmbush(bool bAssassinate)
{

	if (!canAmbush(plot(), bAssassinate))
	{
		return false;
	}
	if (bAssassinate && !isAssassin())
	{
		return false;
	}
	if (bAssassinate && plot()->isCity(false))
	{
		doInsidiousnessVSInvestigationCheck();
	}
	if (GET_PLAYER(getOwner()).getAmbushingUnit() == FFreeList::INVALID_INDEX)
	{
		//Get best attacker from selected and send it to be the one selected to attack by setting it as the ambushing unit.
		if (isHuman())
		{
			GET_PLAYER(getOwner()).setAmbushingUnit(getID(), bAssassinate);
			CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_CONFIRM_AMBUSH); // BUTTONPOPUP_CONFIRM_AMBUSH);
			pInfo->setData1(getID());
			pInfo->setData2(getX());
			pInfo->setData3(getY());
			pInfo->setFlags(AMBUSH_FLAG);
			//pInfo->setPythonModule("AmbushPopup");
			//pInfo->setOnClickedPythonCallback("onAmbushPopup");
			gDLL->getInterfaceIFace()->addPopup(pInfo, getOwner(), true);
		}
		else
		{
			GET_PLAYER(getOwner()).setAmbushingUnit(getID());
			CvPlot* pPlot = plot();
			if (pPlot != NULL)
			{
				CvUnit* pDefender;
				if (bAssassinate && GC.getGame().isModderGameOption(MODDERGAMEOPTION_ASSASSINATE_CHOICE))
				{
					pDefender = pPlot->getWorstDefender(NO_PLAYER, getOwner(), this, true, true, false, bAssassinate);
				}
				else
				{
					pDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), this, true, true, false, bAssassinate);
				}

// 				// Safety net: if the picked defender is a criminal in a city, refuse the ambush.
//                 // The canAmbush check above should already block this case, but guard here
//                 // in case getBestDefender picks a criminal when mixed with other valid targets.
//                 if (pDefender != NULL
//                 &&  bAssassinate
//                 &&  pPlot->isCity(true)
//                 &&  pDefender->isCriminal())
//                 {
//                 	GET_PLAYER(getOwner()).setAmbushingUnit(FFreeList::INVALID_INDEX);
//                 	return false;
//                 }

				if (pDefender != NULL)
				{
					attackSamePlotSpecifiedUnit(pDefender);
					setMadeAttack(true); //Calvitix (if ambush succes, cannot attack anymore)
					changeMoves(GC.getMOVE_DENOMINATOR());
				}
			}
			GET_PLAYER(getOwner()).setAmbushingUnit(FFreeList::INVALID_INDEX);
		}
	}
	return true;
}

void CvUnit::enactAmbush(bool bAssassinate, CvUnit * pSelectedDefender)
{
	CvPlot* pPlot = plot();
	CvUnit* pDefender = NULL;
	if (!pSelectedDefender)
	{
		pDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), this, !gDLL->altKey(), NO_TEAM == getDeclareWarMove(pPlot), false, bAssassinate);
	}
	else
	{
		pDefender = pSelectedDefender;
	}

// 	if (pDefender != NULL
//     &&  bAssassinate
//     &&  pPlot->isCity(true)
//     &&  pDefender->isCriminal())
//     {
//     	return;
//     }

	if (pDefender != NULL)
	{
		attackSamePlotSpecifiedUnit(pDefender);
		setMadeAttack(true); //Calvitix (if ambush succes, cannot attack anymore)
		changeMoves(GC.getMOVE_DENOMINATOR());
	}
}

void CvUnit::changeDebugCount(int iChange)
{
	//TB: disabled while recalculating vision every round.
	m_iDebugCount += iChange;
	//assert disabled until I need to run a new test
	/*FAssert(m_iDebugCount >= 0 && m_iDebugCount <= 1);*/
}

void CvUnit::setDebugCount(int iValue)
{
	//TB: disabled while recalculating vision every round.
	m_iDebugCount = iValue;
	FAssert(m_iDebugCount >= 0 && m_iDebugCount <= 1);
}

bool CvUnit::isAssassin() const
{
	return m_iAssassinCount + m_pUnitInfo->hasSkill(CLS_SKILL_ASSASSIN) > 0;
}

int CvUnit::getAssassinCount() const
{
	return m_iAssassinCount;
}

void CvUnit::changeAssassinCount(int iChange)
{
	m_iAssassinCount += iChange;
}

int CvUnit::stealthStrikesTotal() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	// URS_STEALTH_STRIKES is a FLAT slot (×100) consumed as a whole strike count, so it reduces here
	// ([DEC-fixedpoint-x100] -- the reader ÷100s at the point of use). The slot already gathers the unit's OWN
	// type alongside its promotions and combat classes, so this is the whole value in one read.
	return std::max(0, resolvedValue(URS_STEALTH_STRIKES) / 100);
}

int CvUnit::stealthCombatModifierTotal() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return 0;
	}
	int iAnswer = resolvedValue(URS_STEALTH);

	return iAnswer;
}

bool CvUnit::hasStealthDefense() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
	{
		return false;
	}
	int iCount = getStealthDefenseCount();
	if (m_pUnitInfo->hasSkill(CLS_SKILL_STEALTH_DEFENSE))
	{
		iCount++;
	}
	return (iCount > 0);
}

int CvUnit::getStealthDefenseCount() const
{
	return m_iStealthDefenseCount;
}

void CvUnit::changeStealthDefenseCount(int iChange)
{
	m_iStealthDefenseCount += iChange;
}

void CvUnit::reveal()
{
	m_bRevealed = true;
}

bool CvUnit::isRevealed() const
{
	return m_bRevealed;
}

void CvUnit::doSetDefaultStatuses()
{
	PROFILE_EXTRA_FUNC();
	std::vector<int> m_iDefaultStatusTypes;
	m_iDefaultStatusTypes.clear();
	//Step 1: Assign all statuses from defaults into a local vector to create a singular list
	//	The unit's own combat classes are the candidate set. The keyed map iterates ASCENDING by id, exactly as the
	//	registry countdown did, so the list order step 2 compares and erases against is unchanged.
	for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
	{
		if (!isHasUnitCombat(it->first))
		{
			continue;
		}
		const std::vector<int>& kDefaultStatuses = GC.getUnitCombatInfo(it->first).getDefaultStatuses();
		m_iDefaultStatusTypes.insert(m_iDefaultStatusTypes.end(), kDefaultStatuses.begin(), kDefaultStatuses.end());
	}
	//Step 2: Compare all statuses in the list to all other statuses in the list to check for the same promotionline.
	//If one of them has the iLinePriority 1 promo in that set, this is supposed to indicate NO status from this group.
	//Let it take default by erasing out of the list any other default statuses in this status group (promotionline).
	for (int iI = 0; iI < (int)m_iDefaultStatusTypes.size(); iI++)
	{
		const PromotionTypes ePromotion = (PromotionTypes)m_iDefaultStatusTypes[iI];
		const PromotionLineTypes ePromotionLine = GC.getPromotionInfo(ePromotion).getPromotionLine();
		for (int iJ = 0; iJ < (int)m_iDefaultStatusTypes.size(); iJ++)
		{
			const PromotionTypes pPromotion = (PromotionTypes)m_iDefaultStatusTypes[iJ];
			const PromotionLineTypes pPromotionLine = GC.getPromotionInfo(pPromotion).getPromotionLine();
			if (pPromotionLine == ePromotionLine && ePromotion != pPromotion)
			{
				if (GC.getPromotionInfo(ePromotion).getLinePriority() == 1)
				{
					m_iDefaultStatusTypes.erase(m_iDefaultStatusTypes.begin()+iJ);
				}
				else if (GC.getPromotionInfo(pPromotion).getLinePriority() == 1)
				{
					m_iDefaultStatusTypes.erase(m_iDefaultStatusTypes.begin()+iI);
				}
			}
		}
	}
	//Step 3: Assign default statuses.
	//The order now doesn't matter so if there are differing statuses in the same group then the last to be set takes precedence.
	for (int iI = 0; iI < (int)m_iDefaultStatusTypes.size(); iI++)
	{
		const PromotionTypes ePromotion = (PromotionTypes)m_iDefaultStatusTypes[iI];
		if (canAcquirePromotion(ePromotion, PromotionRequirements::ForStatus))
		{
			statusUpdate(ePromotion);
		}
	}
}

















//








//












bool CvUnit::isArmed() const
{
	return m_bIsArmed || hasStatus(STATUS_PARALYZED);
}




void CvUnit::changeHiddenNationalityCount(int iValue)
{
	m_iHiddenNationalityCount += iValue;
}

int CvUnit::getHiddenNationalityCount() const
{
	return m_iHiddenNationalityCount;
}

int CvUnit::getNoCaptureCount() const
{
	return m_iNoCaptureCount;
}

void CvUnit::changeNoCaptureCount(int iChange)
{
	m_iNoCaptureCount += iChange;
}

void CvUnit::makeWanted(const CvCity* pCity)
{
	PROFILE_EXTRA_FUNC();
	if (pCity == NULL)
	{
		return;
	}
	//Is now Wanted
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		const PromotionTypes ePromotion = (PromotionTypes)iI;

		if (GC.getPromotionInfo(ePromotion).isSetOnInvestigated()
		&& canAcquirePromotion(ePromotion, PromotionRequirements::ForFree))
		{
			setHasPromotion(ePromotion, true, true, false, false);
			m_pPlayerInvestigated = pCity->getOwner();
			// This is something it has to manage on its own
			if (getGroup()->getNumUnits() > 1)
			{
				joinGroup(NULL);
			}
			//do message
			const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_INVESTIGATED_WANTED_RESULT", pCity->getNameKey());
			AddDLLMessage(pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_POSITIVE_DINK", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_HIGHLIGHT_TEXT(), pCity->getX(), pCity->getY(), true, true);

			const CvWString szBuffer2 = gDLL->getText("TXT_KEY_MISC_INVESTIGATED_BECOME_WANTED", getNameKey(), pCity->getNameKey());
			AddDLLMessage(getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer2, "AS2D_EXPOSED", MESSAGE_TYPE_INFO, getButton(), GC.getCOLOR_RED(), pCity->getX(), pCity->getY(), true, true);
			return;
		}
	}
}


void CvUnit::setCityOfOrigin(CvCity* pCity)
{
	m_iXOrigin = pCity->getX();
	m_iYOrigin = pCity->getY();

}

void CvUnit::clearCityOfOrigin()
{
	m_iXOrigin = INVALID_PLOT_COORD;
	m_iYOrigin = INVALID_PLOT_COORD;
}

CvCity* CvUnit::getCityOfOrigin() const
{
	const CvPlot* pPlot = GC.getMap().plotSorenINLINE(m_iXOrigin, m_iYOrigin);
	CvCity* pCity;
	if (pPlot != NULL)
	{
		pCity = pPlot->getPlotCity();
		if (pCity != NULL)
		{
			return pCity;
		}
	}
	return NULL;
}

bool CvUnit::isPromotionFromTrait(PromotionTypes ePromotion) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), ePromotion);

	const PromotionKeyedInfo* info = findPromotionKeyedInfo(ePromotion);

	return info == NULL ? false : info->m_iPromotionFromTraitCount > 0;
}

void CvUnit::setPromotionFromTrait(PromotionTypes ePromotion, bool iChange)
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), ePromotion);

	PromotionKeyedInfo* info = findOrCreatePromotionKeyedInfo(ePromotion, iChange != 0);

	if (info != NULL)
	{
		info->m_iPromotionFromTraitCount = iChange;
	}
}

bool CvUnit::isGatherHerd() const
{
	return (getGatherHerdCount() > 0);
}
int CvUnit::getGatherHerdCount() const
{
	int iTotal = getExtraGatherHerdCount();
	if (m_pUnitInfo->hasSkill(CLS_SKILL_GATHER_HERD))
	{
		iTotal++;
	}
	return iTotal;
}
int CvUnit::getExtraGatherHerdCount() const
{
	return m_iExtraGatherHerdCount;
}
void CvUnit::changeExtraGatherHerdCount(int iChange)
{
	m_iExtraGatherHerdCount += iChange;
}

void CvUnit::defineReligion()
{PROFILE_EXTRA_FUNC();
	//call this when a unitcombat that has a religion is processed in and for all units when the state religion is changed.
	//Check for dedicated faith by unit type, assign it and let it not be changeable unless the unit type changes
	if (!m_bIsReligionLocked)//purely meaning the unit has an overriding religious unitcombat in its base definition (like a missionary, crusader or hellsmouth dog would)
	{
		if (m_eReligionType == NO_RELIGION)
		{
			// A unit's combat classes are its PRIMARY plus its subs ([json.md] par.8) -- the loop's -1
			// sentinel stood for the primary, which is simply the `combatClass` read.
			std::vector<int> aeCombatClasses;
			if (m_pUnitInfo->getCombatClass() != NO_UNITCOMBAT)
			{
				aeCombatClasses.push_back(m_pUnitInfo->getCombatClass());
			}
			const std::vector<int>& aeSubCombatClasses = m_pUnitInfo->getCombatClasses();
			aeCombatClasses.insert(aeCombatClasses.end(), aeSubCombatClasses.begin(), aeSubCombatClasses.end());

			for (size_t iClass = 0; iClass < aeCombatClasses.size(); ++iClass)
			{
				const UnitCombatTypes eUnitCombat = (UnitCombatTypes)aeCombatClasses[iClass];
				const ReligionTypes eOriginalCombatReligion = (ReligionTypes)GC.getUnitCombatInfo(eUnitCombat).getReligion();

				if (eOriginalCombatReligion != NO_RELIGION)
				{
					m_eReligionType = eOriginalCombatReligion;
					m_bIsReligionLocked = true;
					break;
				}
			}
		}
		if (m_eReligionType == NO_RELIGION)
		{
			//if not locked by innate type, after changes in unitcombat process function we'll call this function IF the unitcombat has a religion.
			//This function is also called if the state religion changes so if we find a unit combat has defined m_eReligionType then we'll not bother with switching to the state religion so check here first
			bool bFound = false;
			for (std::map<UnitCombatTypes, UnitCombatKeyedInfo>::const_iterator it = m_unitCombatKeyedInfo.begin(), end = m_unitCombatKeyedInfo.end(); it != end; ++it)
			{
				if (isHasUnitCombat(it->first))
				{
					const ReligionTypes eUnitCombatReligion = (ReligionTypes)GC.getUnitCombatInfo(it->first).getReligion();
					if (eUnitCombatReligion != NO_RELIGION)
					{
						m_eReligionType = eUnitCombatReligion; //Let's assume there's only going to be one of these on a unit ever - it only ever comes up if the unit isn't locked with a pre-defined one anyhow
						//and unitcombats that assign a religion should be rare to assign unless we are more advanced into the Ideas project where the city will assign its religion type to all units that it produces.
						//There could be promos that assign overriding religious types but we'll cross that bridge when we get there.
						bFound = true;
						break;//thus we stop at the first one we find
					}
				}
			}
			if (!bFound)
			{
				m_eReligionType = GET_PLAYER(getOwner()).getStateReligion();//NO_RELIGION is a perfectly satisfactory answer here.
			}
		}
	}
	//else do nothing - if the religion is locked we're done here.
}

ReligionTypes CvUnit::getReligion() const
{
	return m_eReligionType;
}

bool CvUnit::isWorker() const
{
	return m_worker != NULL;
}

UnitCompWorker* CvUnit::getWorkerComponent() const
{
	return m_worker;
}

void CvUnit::deselect(const bool bQuick)
{
	if (IsSelected())
	{
		if (gDLL->getInterfaceIFace()->getLengthSelectionList() > 1)
		{
			gDLL->getInterfaceIFace()->removeFromSelectionList(this);
		}
		else if (bQuick || GET_PLAYER(GC.getGame().getActivePlayer()).isOption(PLAYEROPTION_QUICK_MOVES))
		{
			GC.getGame().updateSelectionListInternal();
		}
	}
}

void CvUnit::forceInvalidCoordinates()
{
	m_iX = INVALID_PLOT_COORD;
	m_iY = INVALID_PLOT_COORD;
}

void CvUnit::doStarsign()
{
	// Do not give starsigns to units created from a unit split/merge action.
	if (GET_PLAYER(getOwner()).getSplittingUnit() != FFreeList::INVALID_INDEX
	|| GET_PLAYER(getOwner()).getBaseMergeSelectionUnit() != FFreeList::INVALID_INDEX
	|| GC.getGame().getSorenRandNum(49, "Seventh son of seventh son") > 0)
	{
		return;
	}
	const CvTeam& team = GET_TEAM(getTeam());
	if (team.isHasTech((TechTypes)GC.getDefineINT("STARSIGN_TECH_END"))
	|| !team.isHasTech((TechTypes)GC.getDefineINT("STARSIGN_TECH_START")))
	{
		return;
	}
	if (team.isHasTech((TechTypes)GC.getDefineINT("STARSIGN_TECH_NERF")))
	{
		if (GC.getGame().getSorenRandNum(4, "3/4 probability after Astronomy") == 0)
		{
			return;
		}
	}
	else if (!team.isHasTech((TechTypes)GC.getDefineINT("STARSIGN_TECH_BOOST")))
	{
		if (GC.getGame().getSorenRandNum(2, "1/2 probability before Astrology") > 0)
		{
			return;
		}
	}
	std::vector<PromotionTypes> starsigns;
	int iCount = 0;
	for (int iI = GC.getNumStarsigns() - 1; iI > -1; iI--)
	{
		const PromotionTypes ePromo = GC.getStarsign(iI);
		if (canKeepPromotion(ePromo, true))
		{
			starsigns.push_back(ePromo);
			iCount++;
		}
	}
	if (iCount == 0)
	{
		return;
	}
	const PromotionTypes ePromo = starsigns[GC.getGame().getSorenRandNum(iCount, "random pick")];

	setHasPromotion(ePromo, true, true);

	if (isHuman())
	{
		CvWString szBuffer;

		if (plot()->getPlotCity())
			szBuffer = gDLL->getText("TXT_KEY_MSG_STARSIGN_BUILD", plot()->getPlotCity()->getNameKey());
		else
			szBuffer = gDLL->getText("TXT_KEY_MSG_STARSIGN_CREATE");

		AddDLLMessage(
			getOwner(), false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_POSITIVE_DINK",
			MESSAGE_TYPE_INFO, GC.getPromotionInfo(ePromo).getButton(),
			GC.getCOLOR_WHITE(), getX(), getY(), true, true
		);
	}
}




