## By StrategyOnly converted to BUG by Dancing Hoskuld
## Completely rewritten when we went fom jus slaves to captives

## Modified by Dancing Hoskuld
##   Now Captives not Slaves
##     Chance of capturing a Military Captive when you attach a unit depends n your and their civics (to be done)
##     Capturing a military init now gives a Captive (Military)
##     Raizing a city will give Captive (Civilians) instead

from CvPythonExtensions import *
import BugUtil
import CvUtil

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
INFO = CyInfo()
GAME = GC.getGame()
STATE = CyState()
ACT = CyAct()
ENABLER = CyEnabler()
ENUMS = CyEnums()
TRNSLTR = CyTranslator()

giDomainLand = -1

def init():
	global giDomainLand
	giDomainLand = GC.getInfoTypeForString('DOMAIN_LAND')

def getSurroundBonus(iDefX, iDefY, iDefTeam):
    """
    Counts the number of surrounding enemy tiles and returns a bonus
    (e.g., 10 per enemy tile).
    Takes the defender's POSITION and TEAM rather than the unit: a unit reaches Python as its (owner, id)
    identity, and both values are already resolved at the one call site.
    """
    # used for checking if its called
    # CvUtil.sendMessage("getSurroundBonus called!", GAME.getActivePlayer(), 0, '', ColorTypes(7), 0, 0, True, True)
    iSurroundCount = -1

    for dx in [-1, 0, 1]:
        for dy in [-1, 0, 1]:
            if dx == 0 and dy == 0:
                continue  # skip defender's own tile
            x = iDefX + dx
            y = iDefY + dy

            if 0 <= x < GC.getMap().getGridWidth() and 0 <= y < GC.getMap().getGridHeight():
                plotX = GC.getMap().plot(x, y)

                for i in range(plotX.getNumUnits()):
                    unitX = plotX.getUnit(i)
                    # enemy check — is at war with defender
                    iTeamX = GC.getPlayer(unitX.getOwner()).getTeam()
                    if iTeamX != iDefTeam and GC.getTeam(iTeamX).isAtWar(iDefTeam):
                        iSurroundCount += 1
                        break  # only count one per tile

    # used for debug checking if suround count works good
    # CvUtil.sendMessage("Surround count: %d" % iSurroundCount, GAME.getActivePlayer(), 0, '', ColorTypes(7), 0, 0, True, True)
    surroundBonus = iSurroundCount * 5
    return surroundBonus


def onCombatResult(argsList):
	CyUnitW, CyUnitL = argsList
	iOwnerW, iUnitW = CyUnitW
	iOwnerL, iUnitL = CyUnitL

	aW = STATE.getUnitRead(iOwnerW, iUnitW)
	aL = STATE.getUnitRead(iOwnerL, iUnitL)
	aFlagsW = STATE.getUnitFlags(iOwnerW, iUnitW)

	# Captives
	# Check that the losing unit is not an animal and the unit does not have a capture type defined in the XML
	if (aFlagsW[UnitFlagKind.UNIT_FLAG_MADE_ATTACK]
	and not INFO.isAnimal(aL[UnitReadKind.UNIT_READ_TYPE])
	and aL[UnitReadKind.UNIT_READ_DOMAIN] == giDomainLand
	and aW[UnitReadKind.UNIT_READ_DOMAIN] == giDomainLand
	and aL[UnitReadKind.UNIT_READ_CAPTURE_UNIT_TYPE] == -1
	):
		aPosL = STATE.getUnitPosition(iOwnerL, iUnitL)
		iCaptureProbability = (aW[UnitReadKind.UNIT_READ_CAPTURE_PROBABILITY]
		                       + getSurroundBonus(aPosL[0], aPosL[1], GC.getPlayer(iOwnerL).getTeam()))
		iCaptureResistance = aL[UnitReadKind.UNIT_READ_CAPTURE_RESISTANCE]

		iChance = iCaptureProbability - iCaptureResistance

		BugUtil.info("CaptureSlaves: Chance to capture a captive is %d (%d - %d)", iChance, iCaptureProbability, iCaptureResistance)

		if iChance > GAME.getSorenRandNum(100, "Slave"):  # 0-99

			if STATE.hasUnitCombat(iOwnerL, iUnitL, GC.getInfoTypeForString('UNITCOMBAT_SPECIES_NEANDERTHAL')):
				iUnit = GC.getInfoTypeForString('UNIT_CAPTIVE_NEANDERTHAL')
				sMessage = TRNSLTR.getText("TXT_KEY_MSG_NEANDERTHAL_CAPTIVE",())
			else:
				iUnit = GC.getInfoTypeForString('UNIT_CAPTIVE_MILITARY')
				sMessage = TRNSLTR.getText("TXT_KEY_MSG_MILITARY_CAPTIVE",())

			iPlayerW = iOwnerW
			aPosW = STATE.getUnitPosition(iOwnerW, iUnitW)
			X = aPosW[0]
			Y = aPosW[1]
			CyUnit = GC.getPlayer(iPlayerW).initUnit(iUnit, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			if iPlayerW == GAME.getActivePlayer():
				CvUtil.sendMessage(sMessage, iPlayerW, 8, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)


def onCityRazed(argsList):
	CyCity, iPlayer = argsList
	if not CyCity: return

	CyPlayer = GC.getPlayer(iPlayer)
	bHuman = CyPlayer.isHuman()

	iCityID = CyCity.getID()
	sCityName = STATE.getCityName(iPlayer, iCityID)
	X = CyCity.getX()
	Y = CyCity.getY()

	'''
	# Convert Great Specialists into captives or other
	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_PROPHET'))
	if iCount > 0:
		iCountKilled = iCount
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_PRIESTS",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage ,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_ARTIST'))
	if iCount > 0:
		iCountKilled = iCount
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_ARTISTS",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_SCIENTIST'))
	if iCount > 0:
		iCountKilled = iCount
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_SCIENTISTS",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_MERCHANT'))
	if iCount > 0:
		iCountKilled = iCount
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_MERCHANTS",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_ENGINEER'))
	if iCount > 0:
		iCountKilled = iCount
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_ENGINEERS",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_DOCTOR'))
	if iCount > 0:
		iCountKilled = iCount
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_DOCTORS",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_SPY'))
	if iCount > 0:
		iCountKilled = iCount
		Inhiding = 0
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_SPIES",(iCount,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	iCount = CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_GREAT_MILITARY_INSTRUCTOR')) + CyCity.getSpecialistCount(GC.getInfoTypeForString('SPECIALIST_MILITARY_INSTRUCTOR'))
	if iCount > 0:
		iCountKilled = iCount
		iCountRebelled = 0
		iCountCaptured = 0
		sMessage = BugUtil.getText("TXT_KEY_MSG_CITY_HAD_GENERALS",(iCount,iCountKilled,iCountRebelled,iCountCaptured))
		CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)
	'''

	## Slaves
	iSlaveSettled = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE")
	iSlaveFood = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_FOOD")
	iSlaveProd = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_PRODUCTION")
	iSlaveCom = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_COMMERCE")
	iSlaveHealth = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_HEALTH")
	iSlaveEntertain = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_ENTERTAINMENT")
	iSlaveTutor = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_TUTOR")
	iSlaveMilitary = GC.getInfoTypeForString("SPECIALIST_SETTLED_SLAVE_MILITARY")

	iUnitCaptiveSlave = GC.getInfoTypeForString("UNIT_FREED_SLAVE")
	iUnitImmigrant = GC.getInfoTypeForString("UNIT_CAPTIVE_IMMIGRANT")
	iUnitEntertain = GC.getInfoTypeForString("UNIT_STORY_TELLER")
	iUnitMerCaravan = GC.getInfoTypeForString("UNIT_EARLY_MERCHANT_C2C")
	iUnitHealth = GC.getInfoTypeForString("UNIT_HEALER")

	iCountSettled = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveSettled)
	iCountFood = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveFood)
	iCountProd = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveProd)
	iCountCom = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveCom)
	iCountHealth = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveHealth)
	iCountEntertain = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveEntertain)
	iCountTutor = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveTutor)
	iCountMilitary = STATE.getAddedFreeSpecialists(iPlayer, iCityID, iSlaveMilitary)

	## Process those that can become population or immagrants
	##	where 3 slaves = 1 pop or immigrant
	##	and can only increase the city pop to 7
	iCount = iCountSettled + iCountFood + iCountCom + iCountTutor + iCountMilitary
	iCountNewPop = int(iCount/3)
	iCount = iCount - 3*iCountNewPop

	if iCount > 0:
		for _ in xrange(iCount):
			CyPlayer.initUnit(iUnitCaptiveSlave, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS", (sCityName, INFO.getDescription("UNIT_", iUnitCaptiveSlave), iCount))
			CyInterface().addMessage(iPlayer, False, 15, sMessage, '', 0, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)

	if iCountNewPop > 0:
		iCountImmigrants = iCountNewPop
		if iCountImmigrants > 0:
			for _ in range (iCountImmigrants):
				CyPlayer.initUnit(iUnitImmigrant, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			if bHuman:
				sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS_IMMIGRANTS", (iCountImmigrants*3, sCityName, iCountImmigrants))
				CyInterface().addMessage(iPlayer, False, 15, sMessage, '', 0, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)

	## Now remove those slaves
	if iCountSettled > 0:
		ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveSettled, -iCountSettled)
	if iCountFood > 0:
		ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveFood, -iCountFood)
	if iCountCom > 0:
		ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveCom, -iCountCom)
	if iCountTutor > 0:
		ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveTutor, -iCountTutor)
	if iCountMilitary > 0:
		ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveMilitary, -iCountMilitary)

	## Now convert the other slaves
	if iCountProd > 0:
		for _ in range (iCountProd):
			CyPlayer.initUnit(iUnitMerCaravan, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveProd, -1)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS",(sCityName, INFO.getDescription("UNIT_", iUnitMerCaravan), iCountProd))
			CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	if iCountHealth > 0:
		for _ in range (iCountHealth):
			CyPlayer.initUnit(iUnitHealth, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveHealth, -1)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS",(sCityName, INFO.getDescription("UNIT_", iUnitHealth), iCountHealth))
			CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	if iCountEntertain > 0:
		for _ in range (iCountEntertain):
			CyPlayer.initUnit(iUnitEntertain, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			ACT.addCityFreeSpecialist(iPlayer, iCityID, iSlaveEntertain, -1)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS",(sCityName, INFO.getDescription("UNIT_", iUnitEntertain), iCountEntertain))
			CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	## Convert population to captives
	iUnit = GC.getInfoTypeForString('UNIT_CAPTIVE_CIVILIAN')
	iCount = 0
	iPop = STATE.getCityPopulation(iPlayer, iCityID)
	if iPop == 1:
		if GAME.getSorenRandNum(100, "Slave") < 66:
			CyPlayer.initUnit(iUnit, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			iCount = 1
	else:
		iCivilianCitizenUnits = (iPop + 1) // 2
		for _ in xrange(iCivilianCitizenUnits):
			CyPlayer.initUnit(iUnit, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			iCount += 1

	if bHuman and iCount:
		sMessage = BugUtil.getText("TXT_KEY_MSG_CIVILIAN_CAPTIVE", iCount)
		CyInterface().addMessage(iPlayer, False, 15, sMessage, '', 0, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)
