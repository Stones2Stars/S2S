#include "CvGameCoreDLL.h"
#include "CyPyList.h"
#include "Engine/CvUnit.h"
#include "CyArea.h"
#include "CyPlot.h"
#include "CySelectionGroup.h"
#include "Infos/CvUnitInfo.h"   // canUpgradeToAny -- the type's own upgrade chain
#include "AI/CvPlayerAI.h"   // GET_PLAYER -- convert resolves the SOURCE unit
#include <boost/python/class.hpp>
#include "CyUnit.h"
#include "Engine/CvMap.h"   // a mission target is addressed by position
#include "Engine/CvSelectionGroup.h"   // the order plane -- activity, missions, ready-to-move
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"   // selectGroup -- the engine action this relays

//	The last Cy read attempted, reported in the crash line (CvGlobals.cpp).
extern const char* g_szLastCyRead;

//
// Python wrapper class for CvUnit
//

CyUnit::CyUnit(CvUnit* pUnit) : m_pUnit(pUnit)
{
	FAssert(m_pUnit != NULL);
}

void CyUnit::NotifyEntity(int /*MissionTypes*/ eEvent)
{
	m_pUnit->NotifyEntity((MissionTypes)eEvent);
}

bool CyUnit::canEnterPlot(const CyPlot& kPlot, bool bAttack, bool bDeclareWar, bool bIgnoreLoad) const
{
	return m_pUnit->canEnterPlot(kPlot.getPlot(),
		(bAttack ? MoveCheck::Attack : MoveCheck::None) |
		(bDeclareWar ? MoveCheck::DeclareWar : MoveCheck::None) |
		(bIgnoreLoad ? MoveCheck::IgnoreLoad : MoveCheck::None)
	);
}

int /*TechTypes*/ CyUnit::getDiscoveryTech() const
{
	return m_pUnit->getDiscoveryTech();
}

int CyUnit::getDiscoverResearch(int /*TechTypes*/ eTech) const
{
	return m_pUnit->getDiscoverResearch((TechTypes) eTech);
}

int CyUnit::getHurryProduction(const CyPlot& kPlot) const
{
	return m_pUnit->getHurryProduction(kPlot.getPlot());
}

bool CyUnit::canTrade(const CyPlot& kPlot, bool bTestVisible) const
{
	return m_pUnit->canTrade(kPlot.getPlot(), bTestVisible);
}

int CyUnit::getGreatWorkCulture(const CyPlot& kPlot) const
{
	return m_pUnit->getGreatWorkCulture();
}

int /*HandicapTypes*/ CyUnit::getHandicapType() const
{
	return m_pUnit->getHandicapType();
}

int /*CivilizationTypes*/ CyUnit::getCivilizationType() const
{
	return m_pUnit->getCivilizationType();
}

int /*SpecialUnitTypes*/ CyUnit::getSpecialUnitType() const
{
	return m_pUnit->getSpecialUnitType();
}

int /*UnitCombatTypes*/ CyUnit::getUnitCombatType() const
{
	return m_pUnit->getUnitCombatType();
}

DomainTypes CyUnit::getDomainType() const
{
	return m_pUnit->getDomainType();
}

bool CyUnit::isNPC() const
{
	return m_pUnit->isNPC();
}

bool CyUnit::isHuman() const
{
	return m_pUnit->isHuman();
}

int CyUnit::baseMoves() const
{
	return m_pUnit->baseMoves();
}

int CyUnit::movesLeft() const
{
	return m_pUnit->movesLeft();
}

bool CyUnit::isOnlyDefensive() const
{
	return m_pUnit->isOnlyDefensive();
}

bool CyUnit::isFound() const
{
	return m_pUnit->isFound();
}

int CyUnit::getMaxHP() const
{
	return m_pUnit->getMaxHP();
}

int CyUnit::getHP() const
{
	return m_pUnit->getHP();
}

bool CyUnit::isHurt() const
{
	return m_pUnit->isHurt();
}

void CyUnit::setBaseCombatStr(int iCombat)
{
	m_pUnit->setBaseCombatStr(iCombat);
}

// The Python boundary reads HUMAN -- strength is ×100 inside the engine (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
int CyUnit::baseCombatStr() const
{
	return m_pUnit->baseCombatStrHuman();
}

bool CyUnit::canFight() const
{
	return m_pUnit->canFight();
}

// Human at the boundary, as above.
int CyUnit::getAirBaseCombatStr() const
{
	return m_pUnit->airBaseCombatStr() / 100;
}

bool CyUnit::isAutoPromoting() const
{
	return m_pUnit->isAutoPromoting();
}

bool CyUnit::isAutoUpgrading() const
{
	return m_pUnit->isAutoUpgrading();
}

bool CyUnit::isWaiting() const
{
	return m_pUnit->isWaiting();
}

bool CyUnit::isFortifyable() const
{
	return m_pUnit->isFortifyable();
}

int CyUnit::bombardRate() const
{
	return m_pUnit->getBombardRate();
}

int /*SpecialUnitTypes*/ CyUnit::getSpecialCargo() const
{
	return m_pUnit->getSpecialCargo();
}

int /*DomainTypes*/ CyUnit::getDomainCargo() const
{
	return m_pUnit->getDomainCargo();
}

int CyUnit::cargoSpace() const
{
	return m_pUnit->cargoSpace();
}

void CyUnit::changeCargoSpace(int iChange)
{
	m_pUnit->changeCargoSpace(iChange);
}

bool CyUnit::isFull() const
{
	return m_pUnit->isFull();
}

int CyUnit::getID() const
{
	return m_pUnit->getID();
}

int CyUnit::getGroupID() const
{
	return m_pUnit->getGroupID();
}

CySelectionGroup* CyUnit::getGroup() const
{
	return new CySelectionGroup(m_pUnit->getGroup());
}

int CyUnit::getX() const
{
	return m_pUnit->getX();
}

int CyUnit::getY() const
{
	return m_pUnit->getY();
}

void CyUnit::setXY(int iX, int iY, bool bGroup, bool bUpdate, bool bShow)
{
	m_pUnit->setXY(iX, iY, bGroup, bUpdate, bShow);
}

int CyUnit::getDamage() const
{
	return m_pUnit->getDamage();
}

void CyUnit::changeDamage(int iChange, int /*PlayerTypes*/ ePlayer)
{
	m_pUnit->changeDamage(iChange, (PlayerTypes)ePlayer);
}

int CyUnit::getMoves() const
{
	return m_pUnit->getMoves();
}

void CyUnit::changeMoves(int iChange)
{
	m_pUnit->changeMoves(iChange);
}

int CyUnit::getExperience() const
{
	return m_pUnit->getExperience();
}

int CyUnit::getLevel() const
{
	return m_pUnit->getLevel();
}

void CyUnit::setLevel(int iNewLevel)
{
	m_pUnit->setLevel(iNewLevel);
}

int CyUnit::getFacingDirection() const
{
	return m_pUnit->getFacingDirection(false);
}

void CyUnit::rotateFacingDirectionClockwise()
{
	m_pUnit->rotateFacingDirectionClockwise();
}

void CyUnit::rotateFacingDirectionCounterClockwise()
{
	m_pUnit->rotateFacingDirectionCounterClockwise();
}

int CyUnit::getCargo() const
{
	return m_pUnit->getCargo();
}

int CyUnit::getFortifyTurns() const
{
	return m_pUnit->getFortifyTurns();
}

void CyUnit::setFortifyTurns(int iNewValue)
{
	m_pUnit->setFortifyTurns(iNewValue);
}

bool CyUnit::isRiver() const
{
	return m_pUnit->isRiver();
}

bool CyUnit::isMadeAttack() const
{
	return m_pUnit->isMadeAttack();
}

void CyUnit::setMadeAttack(bool bNewValue)
{
	m_pUnit->setMadeAttack(bNewValue);
}

bool CyUnit::isMadeInterception() const
{
	return m_pUnit->isMadeInterception();
}

void CyUnit::setMadeInterception(bool bNewValue)
{
	m_pUnit->setMadeInterception(bNewValue);
}

bool CyUnit::isPromotionReady() const
{
	return m_pUnit->isPromotionReady();
}

void CyUnit::setPromotionReady(bool bNewValue)
{
	m_pUnit->setPromotionReady(bNewValue);
}

int CyUnit::getOwner() const
{
	return m_pUnit->getOwner();
}

int CyUnit::getTeam() const
{
	return m_pUnit->getTeam();
}

int /*UnitTypes*/ CyUnit::getUnitType() const
{
	return m_pUnit->getUnitType();
}

int /*UnitTypes*/ CyUnit::getLeaderUnitType() const
{
	return m_pUnit->getLeaderUnitType();
}

CyUnit* CyUnit::getTransportUnit() const
{
	CvUnit* unit = m_pUnit->getTransportUnit();
	return unit ? new CyUnit(unit) : NULL;
}

bool CyUnit::isCargo() const
{
	return m_pUnit->isCargo();
}

void CyUnit::setTransportUnit(const CyUnit& kTransportUnit, const bool bLoad)
{
	m_pUnit->setTransportUnit(bLoad ? kTransportUnit.getUnit() : NULL);
}

std::wstring CyUnit::getName() const
{
	return m_pUnit->getName();
}

std::wstring CyUnit::getNameKey() const
{
	return m_pUnit->getNameKey();
}

bool CyUnit::isHasPromotion(int /*PromotionTypes*/eIndex) const
{
	return m_pUnit->isHasPromotion((PromotionTypes)eIndex);
}

bool CyUnit::isHasUnitCombat(int /*UnitCombatTypes*/eIndex) const
{
	return m_pUnit->isHasUnitCombat((UnitCombatTypes)eIndex);
}

void CyUnit::setHasPromotion(int /*PromotionTypes*/ eIndex, bool bNewValue)
{
	m_pUnit->setHasPromotion((PromotionTypes) eIndex, bNewValue);
}

int /*UnitAITypes*/ CyUnit::getUnitAIType() const
{
	return m_pUnit->AI_getUnitAIType();
}

void CyUnit::setAIType(int /*UnitAITypes*/ iNewValue)
{
	m_pUnit->AI_setUnitAIType((UnitAITypes)iNewValue);
}

std::string CyUnit::getButton() const
{
	return m_pUnit->getButton();
}

void CyUnit::setCommander(bool bNewValue)
{
	m_pUnit->setCommander(bNewValue);
}

void CyUnit::setCommodore(bool bNewValue)
{
	m_pUnit->setCommodore(bNewValue);
}

//	⚖ THE IDENTITY SET (CyUnit.h) plus the unit's OWN data, read from the unit's OWN accessor
//	(docs/architecture/patterns.md §THE PYTHON READ BOUNDARY, accessor homing). A unit's hit points and
//	whether it can fight are the unit's, so they are asked of the unit -- a flat `getUnitFlags(owner, id)` is
//	the mishomed shape that ruling names, and its tell is the NOUN in the method name: an accessor that owns
//	its subject needs none.
//	⚖ The WRITES live here too (`setDamage`, `kill`), each routing through the engine's own setter so the
//	DOMAIN fact still fires -- what was cut was the legacy per-field contract, never the mutation itself.
//	⚑ Added ON DEMAND, for call sites that want them ([patterns.md] § THE PYTHON READ BOUNDARY: endpoint COUNT is
//	not the axis, findable homing is) -- never a pre-emptive re-publication of the legacy per-field contract.
bool CyUnit::canAcquirePromotion(int iPromotion) const
{
	g_szLastCyRead = "CyUnit::canAcquirePromotion";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->canAcquirePromotion((PromotionTypes)iPromotion);
}
bool CyUnit::canUpgrade(int iToUnit, bool bTestVisible) const
{
	g_szLastCyRead = "CyUnit::canUpgrade";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iToUnit < 0 || iToUnit >= GC.getNumUnitInfos())
	{
		return false;
	}
	return pUnit->canUpgrade((UnitTypes)iToUnit, bTestVisible);
}
bool CyUnit::canUpgradeToAny() const
{
	PERF_SCOPE("CyUnit::canUpgradeToAny", -1);
	g_szLastCyRead = "CyUnit::canUpgradeToAny";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	//	⛔ ASK THE UNIT WHAT IT UPGRADES TO -- do NOT scan the registry asking every type "can I become you?".
	const std::vector<int>& upgrades = pUnit->getUnitInfo().getUpgradesTo();
	for (size_t i = 0; i < upgrades.size(); ++i)
	{
		if (pUnit->canUpgrade((UnitTypes)upgrades[i], true)) return true;
	}
	return false;
}
int CyUnit::getNumVisiblePotentialEnemyDefenders(int iX, int iY) const
{
	g_szLastCyRead = "CyUnit::getNumVisiblePotentialEnemyDefenders";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return 0;
	const CvPlot* pPlot = GC.getMap().plot(iX, iY);
	return pPlot ? pPlot->getNumVisiblePotentialEnemyDefenders(pUnit) : 0;
}
int CyUnit::getBaseCombatStr() const
{
	g_szLastCyRead = "CyUnit::getBaseCombatStr";
	const CvUnit* pUnit = m_pUnit;
	return pUnit ? pUnit->baseCombatStrHuman() : 0;
}
python::list CyUnit::getFlags() const
{
	PERF_SCOPE("CyUnit::getFlags", -1);
	g_szLastCyRead = "CyUnit::getFlags";
	int values[NUM_UNIT_FLAGS] = { 0 };
	const CvUnit* pUnit = m_pUnit;
	if (pUnit) pUnit->getUnitFlags(values);
	return cyToList(values);
}
std::wstring CyUnit::getNameNoDesc() const
{
	g_szLastCyRead = "CyUnit::getNameNoDesc";
	const CvUnit* pUnit = m_pUnit;
	return pUnit ? std::wstring(pUnit->getNameNoDesc()) : std::wstring();
}
python::list CyUnit::getPosition() const
{
	int values[2] = { -1, -1 };   // -1,-1 = no such unit, or the unit is OFF-MAP (a real state)
	const CvUnit* pUnit = m_pUnit;
	if (pUnit)
	{
		//	⛔ THE ON-MAP TEST IS THE COORDINATE RANGE, never `plot() != NULL` alone: plot() answers NULL for
		//	exactly ONE pair (INVALID_PLOT_COORD) and resolves any OTHER out-of-range value to a WRONG plot
		//	([unit-lifecycle.md]). A unit carrying a stored -1 would otherwise hand back a real but wrong tile,
		//	which is worse than answering "off map" -- and saves do contain such units.
		const int iX = pUnit->getX();
		const int iY = pUnit->getY();
		if (iX >= 0 && iY >= 0 && iX < GC.getMap().getGridWidth() && iY < GC.getMap().getGridHeight())
		{
			values[0] = iX;
			values[1] = iY;
		}
	}
	return cyToList(values);
}
python::list CyUnit::getPromotions() const
{
	PERF_SCOPE("CyUnit::getPromotions", -1);
	g_szLastCyRead = "CyUnit::getPromotions";
	python::list ids = python::list();
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return ids;
	//	⛔ WALK WHAT THE UNIT HOLDS -- do NOT sweep the promotion registry asking "do you have this one?". The
	//	unit keys only the promotions it actually carries, and isHasPromotion is a keyed LOOKUP, so a registry
	//	sweep is ~1500 map searches per unit per redraw to rediscover a list the unit already has.
	const std::map<PromotionTypes, PromotionKeyedInfo>& held = pUnit->getPromotionKeyedInfo();
	for (std::map<PromotionTypes, PromotionKeyedInfo>::const_iterator it = held.begin(); it != held.end(); ++it)
	{
		if (it->second.m_bHasPromotion && !pUnit->isPromotionOverriden(it->first))
		{
			ids.append((int)it->first);
		}
	}
	return ids;
}
python::list CyUnit::getMissionQueue() const
{
	PERF_SCOPE("CyUnit::getMissionQueue", -1);
	g_szLastCyRead = "CyUnit::getMissionQueue";
	//	One crossing for the WHOLE queue, as a list of [missionType, data1] pairs. getRead() carries the HEAD
	//	mission only, which is all a one-line summary needs; the interface's queue panel renders every entry,
	//	so it needs the list. ⛔ The answer is NOT to publish CySelectionGroup: that type's registration is the
	//	bare identity the marshaller needs and it publishes no methods at all, so a group handle in Python is a
	//	dead end by design -- a unit answers for its own orders.
	python::list queue = python::list();
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return queue;
	const CvSelectionGroup* pGroup = pUnit->getGroup();
	if (pGroup == NULL) return queue;
	const int iLength = pGroup->getLengthMissionQueue();
	for (int iNode = 0; iNode < iLength; ++iNode)
	{
		python::list entry = python::list();
		entry.append(pGroup->getMissionType(iNode));
		entry.append(pGroup->getMissionData1(iNode));
		queue.append(entry);
	}
	return queue;
}

python::list CyUnit::getRead() const
{
	PERF_SCOPE("CyUnit::getRead", -1);
	g_szLastCyRead = "CyUnit::getRead";
	int values[NUM_UNIT_READS] = { 0 };
	values[UNIT_READ_TYPE]     = -1;
	values[UNIT_READ_ACTIVITY] = (int)NO_ACTIVITY;
	values[UNIT_READ_AUTOMATE] = (int)NO_AUTOMATE;
	values[UNIT_READ_MISSION]  = (int)NO_MISSION;
	CvUnit* pUnit = m_pUnit;
	if (pUnit) pUnit->getUnitRead(values);
	return cyToList(values);
}
std::string CyUnit::getScriptData() const
{
	g_szLastCyRead = "CyUnit::getScriptData";
	const CvUnit* pUnit = m_pUnit;
	return pUnit ? pUnit->getScriptData() : std::string();
}
int CyUnit::getVisualOwner() const
{
	g_szLastCyRead = "CyUnit::getVisualOwner";
	const CvUnit* pUnit = m_pUnit;
	return pUnit ? (int)pUnit->getVisualOwner() : -1;
}
bool CyUnit::hasCombat(int iUnitCombat) const
{
	g_szLastCyRead = "CyUnit::hasCombat";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iUnitCombat < 0 || iUnitCombat >= GC.getNumUnitCombatInfos())
	{
		return false;
	}
	return pUnit->isHasUnitCombat((UnitCombatTypes)iUnitCombat);
}
bool CyUnit::hasPromotion(int iPromotion) const
{
	g_szLastCyRead = "CyUnit::hasPromotion";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isHasPromotion((PromotionTypes)iPromotion);
}
bool CyUnit::isActionRecommended(int iAction) const
{
	PERF_SCOPE("CyUnit::isActionRecommended", -1);
	g_szLastCyRead = "CyUnit::isActionRecommended";
	const CvUnit* pUnit = m_pUnit;
	//	BOTH bounds: the action id indexes the action registry, so an unchecked upper bound is an out-of-bounds
	//	read rather than a wrong answer -- and FASSERT_BOUNDS is compiled out of Release, which is where it runs.
	if (pUnit == NULL || iAction < 0 || iAction >= GC.getNumActionInfos())
	{
		return false;
	}
	return pUnit->isActionRecommended(iAction);
}
bool CyUnit::isDead() const
{
	g_szLastCyRead = "CyUnit::isDead";
	const CvUnit* pUnit = m_pUnit;
	return pUnit ? pUnit->isDead() : true;
}
bool CyUnit::isHiddenNationality() const
{
	g_szLastCyRead = "CyUnit::isHiddenNationality";
	const CvUnit* pUnit = m_pUnit;
	return pUnit ? pUnit->isHiddenNationality() : false;
}
bool CyUnit::isInvisible(int iTeam) const
{
	PERF_SCOPE("CyUnit::isInvisible", -1);
	g_szLastCyRead = "CyUnit::isInvisible";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iTeam < 0 || iTeam >= MAX_TEAMS)
	{
		return false;
	}
	return pUnit->isInvisible((TeamTypes)iTeam, false);
}
bool CyUnit::isPromotionOverridden(int iPromotion) const
{
	g_szLastCyRead = "CyUnit::isPromotionOverridden";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isPromotionOverriden((PromotionTypes)iPromotion);
}
bool CyUnit::isPromotionValid(int iPromotion) const
{
	g_szLastCyRead = "CyUnit::isPromotionValid";
	const CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL || iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos())
	{
		return false;
	}
	return pUnit->isPromotionValid((PromotionTypes)iPromotion);
}
bool CyUnit::changeExperience(int iChange, int iMax,
							  bool bFromCombat, bool bInBorders, bool bUpdateGlobal) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->changeExperience(iChange, iMax, bFromCombat, bInBorders, bUpdateGlobal);
	return true;
}
bool CyUnit::doCommand(int iCommand, int iData1, int iData2) const
{
	if (iCommand < 0 || iCommand >= NUM_COMMAND_TYPES) return false;
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->doCommand((CommandTypes)iCommand, iData1, iData2);
	return true;
}
bool CyUnit::finishMoves() const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->finishMoves();
	return true;
}
bool CyUnit::kill(bool bDelay, int iByPlayer) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->kill(bDelay, (iByPlayer >= 0 && iByPlayer < MAX_PLAYERS) ? (PlayerTypes)iByPlayer : NO_PLAYER);
	return true;
}
bool CyUnit::setDamage(int iDamage, int iByPlayer) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	//	⚠ setDamage can KILL the unit (it ends in `if (isDead()) kill(...)`, [unit-lifecycle.md]), so the caller
	//	must not assume the unit survives this call -- exactly as an engine-side caller must not.
	pUnit->setDamage(iDamage, (iByPlayer >= 0 && iByPlayer < MAX_PLAYERS) ? (PlayerTypes)iByPlayer : NO_PLAYER);
	return true;
}
bool CyUnit::setExperience(int iExperience) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->setExperience(iExperience);
	return true;
}
bool CyUnit::setLeaderUnitType(int iLeaderUnitType) const
{
	if (iLeaderUnitType >= GC.getNumUnitInfos()) return false;
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	//	-1 CLEARS the attachment, which is a real call (the beastmaster link is dropped when the unit dies), so
	//	a negative id is passed through rather than refused.
	pUnit->setLeaderUnitType((UnitTypes)iLeaderUnitType);
	return true;
}
bool CyUnit::setMoves(int iMoves) const
{
	//	Moves SPENT, in move points -- the partial-moves sibling of finishUnitMoves (an event spawn that
	//	leaves its unit a point rather than the whole allowance or none).
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->setMoves(iMoves);
	return true;
}
bool CyUnit::setName(std::wstring szName) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->setName(CvWString(szName));
	return true;
}
bool CyUnit::setPromotion(int iPromotion, bool bNewValue) const
{
	if (iPromotion < 0 || iPromotion >= GC.getNumPromotionInfos()) return false;
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->setHasPromotion((PromotionTypes)iPromotion, bNewValue);
	return true;
}
bool CyUnit::setScriptData(std::string szData) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	pUnit->setScriptData(szData);
	return true;
}
bool CyUnit::setStatus(int iStatus, int iTurns) const
{
	if (iStatus < 0 || iStatus >= (int)NUM_UNIT_STATUSES) return false;
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	//	The ONE write path, so the 0-crossing announces and the load lands through it too ([state.md]).
	pUnit->setStatus((UnitStatus)iStatus, iTurns);
	return true;
}

bool CyUnit::selectGroup(bool bShift, bool bCtrl, bool bAlt) const
{
	CvUnit* pUnit = m_pUnit;
	if (pUnit == NULL) return false;
	gDLL->getInterfaceIFace()->selectGroup(pUnit, bShift, bCtrl, bAlt);
	return true;
}

bool CyUnit::convert(int iFromPlayer, int iFromUnit, bool bKillOriginal) const
{
	CvUnit* pUnit = m_pUnit;
	if (iFromPlayer < 0 || iFromPlayer >= MAX_PLAYERS) return false;
	CvUnit* pFrom = GET_PLAYER((PlayerTypes)iFromPlayer).getUnit(iFromUnit);
	if (pUnit == NULL || pFrom == NULL) return false;
	pUnit->convert(pFrom, bKillOriginal);
	return true;
}

bool CyUnit::setActivity(int iActivityType)
{
	if (m_pUnit == NULL || m_pUnit->getGroup() == NULL) return false;
	m_pUnit->getGroup()->setActivityType((ActivityTypes)iActivityType);
	return true;
}

bool CyUnit::isReadyToMove(bool bAny)
{
	if (m_pUnit == NULL || m_pUnit->getGroup() == NULL) return false;
	return m_pUnit->getGroup()->readyToMove(bAny);
}

bool CyUnit::canStartMission(int iMission, int iData1, int iData2, int iX, int iY, bool bTestVisible) const
{
	if (m_pUnit == NULL || m_pUnit->getGroup() == NULL) return false;
	CvPlot* pPlot = GC.getMap().plot(iX, iY);
	return m_pUnit->getGroup()->canStartMission(iMission, iData1, iData2, pPlot, bTestVisible);
}

bool CyUnit::pushMission(int iMission, int iData1, int iData2, int iFlags, bool bAppend, bool bManual, int iMissionAI, int iX, int iY)
{
	if (m_pUnit == NULL || m_pUnit->getGroup() == NULL) return false;
	CvPlot* pPlot = GC.getMap().plot(iX, iY);
	m_pUnit->getGroup()->pushMission((MissionTypes)iMission, iData1, iData2, iFlags, bAppend, bManual,
		(MissionAITypes)iMissionAI, pPlot, m_pUnit);
	return true;
}

int CyUnit::getStatus(int iStatus) const
{
	if (m_pUnit == NULL || iStatus < 0 || iStatus >= NUM_UNIT_STATUSES) return 0;
	return m_pUnit->getStatus((UnitStatus)iStatus);
}

void CyUnit::pythonPublish()
{
	python::class_<CyUnit>("CyUnit", python::no_init)
		//	==== THE EDITOR PLANE ====
		//	Arbitrary engine fields a scenario editor pokes. They are deliberately NOT read KINDS: nothing
		//	in the game model asks 'is this unit cargo', so they stay named verbs on the unit rather than
		//	group-read slots ([python-read-map.md] par.7). Every write routes through the engine's own
		//	setter, so the domain fact still fires.
		.def("cargoSpace", &CyUnit::cargoSpace)
		.def("changeCargoSpace", &CyUnit::changeCargoSpace)
		.def("getCargo", &CyUnit::getCargo)
		.def("isFull", &CyUnit::isFull)
		.def("setTransportUnit", &CyUnit::setTransportUnit)
		.def("isMadeAttack", &CyUnit::isMadeAttack)
		.def("setMadeAttack", &CyUnit::setMadeAttack)
		.def("isMadeInterception", &CyUnit::isMadeInterception)
		.def("setMadeInterception", &CyUnit::setMadeInterception)
		.def("isPromotionReady", &CyUnit::isPromotionReady)
		.def("setPromotionReady", &CyUnit::setPromotionReady)
		.def("setBaseCombatStr", &CyUnit::setBaseCombatStr)
		.def("setXY", &CyUnit::setXY)
		.def("setLevel", &CyUnit::setLevel)
		.def("changeMoves", &CyUnit::changeMoves)
		.def("getMoves", &CyUnit::getMoves)
		.def("changeDamage", &CyUnit::changeDamage)
		.def("setAIType", &CyUnit::setAIType)
		.def("setActivity", &CyUnit::setActivity)
		.def("isReadyToMove", &CyUnit::isReadyToMove)
		.def("canStartMission", &CyUnit::canStartMission)
		.def("pushMission", &CyUnit::pushMission)
		.def("getAirBaseCombatStr", &CyUnit::getAirBaseCombatStr)
		.def("getSpecialUnitType", &CyUnit::getSpecialUnitType)
		.def("getDiscoveryTech", &CyUnit::getDiscoveryTech)
		.def("getDiscoverResearch", &CyUnit::getDiscoverResearch)
		.def("getGreatWorkCulture", &CyUnit::getGreatWorkCulture)
		.def("getHurryProduction", &CyUnit::getHurryProduction)
		.def("getTransportUnit", &CyUnit::getTransportUnit, python::return_value_policy<python::manage_new_object>())
		.def("selectGroup", &CyUnit::selectGroup)
		.def("convert", &CyUnit::convert)
		.def("canAcquirePromotion", &CyUnit::canAcquirePromotion)
		.def("canUpgrade", &CyUnit::canUpgrade)
		.def("canUpgradeToAny", &CyUnit::canUpgradeToAny)
		.def("getNumVisiblePotentialEnemyDefenders", &CyUnit::getNumVisiblePotentialEnemyDefenders)
		.def("getBaseCombatStr", &CyUnit::getBaseCombatStr)
		.def("getFlags", &CyUnit::getFlags)
		.def("getNameNoDesc", &CyUnit::getNameNoDesc)
		.def("getPosition", &CyUnit::getPosition)
		.def("getPromotions", &CyUnit::getPromotions)
		.def("getMissionQueue", &CyUnit::getMissionQueue)
		.def("getRead", &CyUnit::getRead)
		.def("getScriptData", &CyUnit::getScriptData)
		.def("getVisualOwner", &CyUnit::getVisualOwner)
		.def("hasCombat", &CyUnit::hasCombat)
		.def("hasPromotion", &CyUnit::hasPromotion)
		.def("isActionRecommended", &CyUnit::isActionRecommended)
		.def("isDead", &CyUnit::isDead)
		.def("isHiddenNationality", &CyUnit::isHiddenNationality)
		.def("isInvisible", &CyUnit::isInvisible)
		.def("isPromotionOverridden", &CyUnit::isPromotionOverridden)
		.def("isPromotionValid", &CyUnit::isPromotionValid)
		.def("changeExperience", &CyUnit::changeExperience)
		.def("doCommand", &CyUnit::doCommand)
		.def("finishMoves", &CyUnit::finishMoves)
		.def("kill", &CyUnit::kill)
		.def("setDamage", &CyUnit::setDamage)
		.def("setExperience", &CyUnit::setExperience)
		.def("setLeaderUnitType", &CyUnit::setLeaderUnitType)
		.def("setMoves", &CyUnit::setMoves)
		.def("setName", &CyUnit::setName)
		.def("setPromotion", &CyUnit::setPromotion)
		.def("setScriptData", &CyUnit::setScriptData)
		.def("setStatus", &CyUnit::setStatus)
		.def("getStatus", &CyUnit::getStatus)
		.def("getSpecialCargo", &CyUnit::getSpecialCargo)
		.def("getDomainCargo", &CyUnit::getDomainCargo)
		.def("getOwner", &CyUnit::getOwner)
		.def("getID",    &CyUnit::getID)
		.def("getX",     &CyUnit::getX)
		.def("getY",     &CyUnit::getY)
		.def("canFight", &CyUnit::canFight)
		.def("getHP",    &CyUnit::getHP)
		.def("getMaxHP", &CyUnit::getMaxHP)
		.def("getName",  &CyUnit::getName)
		;
}
