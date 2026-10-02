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

# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
INFO = CyInfo()
GAME = GC.getGame()
ENABLER = CyEnabler()
ENUMS = CyEnums()
TRNSLTR = CyTranslator()

def onCityRazed(argsList):
	# A game-object event arg is the (owner, id) IDENTITY TUPLE, never a handle -- unpack and resolve.
	(iCityOwner, iCityID), iPlayer = argsList
	CyCity = GC.getPlayer(iCityOwner).getCity(iCityID)
	if not CyCity: return

	CyPlayer = GC.getPlayer(iPlayer)
	bHuman = CyPlayer.isHuman()

	iCityID = CyCity.getID()
	sCityName = GC.getPlayer(iPlayer).getCity(iCityID).getName()
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

	iCountSettled = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveSettled)
	iCountFood = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveFood)
	iCountProd = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveProd)
	iCountCom = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveCom)
	iCountHealth = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveHealth)
	iCountEntertain = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveEntertain)
	iCountTutor = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveTutor)
	iCountMilitary = GC.getPlayer(iPlayer).getCity(iCityID).getAddedFreeSpecialists(iSlaveMilitary)

	## Process those that can become population or immagrants
	##	where 3 slaves = 1 pop or immigrant
	##	and can only increase the city pop to 7
	iCount = iCountSettled + iCountFood + iCountCom + iCountTutor + iCountMilitary
	iCountNewPop = int(iCount/3)
	iCount = iCount - 3*iCountNewPop

	if iCount > 0:
		for _ in xrange(iCount):
			CyPlayer.createUnit(iUnitCaptiveSlave, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS", (sCityName, INFO.getDescription("UNIT_", iUnitCaptiveSlave), iCount))
			CyInterface().addMessage(iPlayer, False, 15, sMessage, '', 0, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)

	if iCountNewPop > 0:
		iCountImmigrants = iCountNewPop
		if iCountImmigrants > 0:
			for _ in range (iCountImmigrants):
				CyPlayer.createUnit(iUnitImmigrant, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			if bHuman:
				sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS_IMMIGRANTS", (iCountImmigrants*3, sCityName, iCountImmigrants))
				CyInterface().addMessage(iPlayer, False, 15, sMessage, '', 0, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)

	## Now remove those slaves
	if iCountSettled > 0:
		GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveSettled, -iCountSettled)
	if iCountFood > 0:
		GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveFood, -iCountFood)
	if iCountCom > 0:
		GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveCom, -iCountCom)
	if iCountTutor > 0:
		GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveTutor, -iCountTutor)
	if iCountMilitary > 0:
		GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveMilitary, -iCountMilitary)

	## Now convert the other slaves
	if iCountProd > 0:
		for _ in range (iCountProd):
			CyPlayer.createUnit(iUnitMerCaravan, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveProd, -1)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS",(sCityName, INFO.getDescription("UNIT_", iUnitMerCaravan), iCountProd))
			CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	if iCountHealth > 0:
		for _ in range (iCountHealth):
			CyPlayer.createUnit(iUnitHealth, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveHealth, -1)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS",(sCityName, INFO.getDescription("UNIT_", iUnitHealth), iCountHealth))
			CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	if iCountEntertain > 0:
		for _ in range (iCountEntertain):
			CyPlayer.createUnit(iUnitEntertain, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			GC.getPlayer(iPlayer).getCity(iCityID).addFreeSpecialist(iSlaveEntertain, -1)
		if bHuman:
			sMessage = BugUtil.getText("TXT_KEY_MSG_FREED_SLAVES_AS",(sCityName, INFO.getDescription("UNIT_", iUnitEntertain), iCountEntertain))
			CyInterface().addMessage(iPlayer,False,15, sMessage,'',0,'Art/Interface/Buttons/Civics/Serfdom.dds',ColorTypes(44), X, Y, True,True)

	## Convert population to captives
	iUnit = GC.getInfoTypeForString('UNIT_CAPTIVE_CIVILIAN')
	iCount = 0
	iPop = GC.getPlayer(iPlayer).getCity(iCityID).getPopulation()
	if iPop == 1:
		if GAME.getSorenRandNum(100, "Slave") < 66:
			CyPlayer.createUnit(iUnit, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			iCount = 1
	else:
		iCivilianCitizenUnits = (iPop + 1) // 2
		for _ in xrange(iCivilianCitizenUnits):
			CyPlayer.createUnit(iUnit, X, Y, UnitAITypes.NO_UNITAI, DirectionTypes.NO_DIRECTION)
			iCount += 1

	if bHuman and iCount:
		sMessage = BugUtil.getText("TXT_KEY_MSG_CIVILIAN_CAPTIVE", iCount)
		CyInterface().addMessage(iPlayer, False, 15, sMessage, '', 0, 'Art/Interface/Buttons/Civics/Serfdom.dds', ColorTypes(44), X, Y, True, True)
