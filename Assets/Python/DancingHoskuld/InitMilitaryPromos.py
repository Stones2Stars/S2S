##=========================##
## MILITIA PROMOTIONS CODE ##
## Code for Stones2Stars   ##
##=========================##
from CvPythonExtensions import *

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
MAP = GC.getMap()
STATE = CyState()
ENABLER = CyEnabler()
ACT = CyAct()
ENUMS = CyEnums()

def init():
	global giMilInstCivic, giVolArmyCivic, gaiSettlerWorkerCombatList, aReefList, aTreeList

	gaiSettlerWorkerCombatList = [
		GC.getInfoTypeForString("UNITCOMBAT_SETTLER"),
		GC.getInfoTypeForString("UNITCOMBAT_WORKER"),
		GC.getInfoTypeForString("UNITCOMBAT_SEA_WORKER")
	]
	aReefList = [
		GC.getInfoTypeForString('FEATURE_REEF'),
		GC.getInfoTypeForString('FEATURE_REEF_BEACON'),
		GC.getInfoTypeForString('FEATURE_REEF_LIGHTHOUSE'),
		GC.getInfoTypeForString('FEATURE_CORAL_REEF'),
		GC.getInfoTypeForString('FEATURE_CORAL_REEF_BEACON'),
		GC.getInfoTypeForString('FEATURE_CORAL_REEF_LIGHTHOUSE')
	]
	aTreeList = [
		GC.getInfoTypeForString('FEATURE_FOREST'),
		GC.getInfoTypeForString('FEATURE_FOREST_YOUNG'),
		GC.getInfoTypeForString('FEATURE_FOREST_ANCIENT'),
		GC.getInfoTypeForString('FEATURE_JUNGLE'),
		GC.getInfoTypeForString('FEATURE_BAMBOO')
	]
	giMilInstCivic = GC.getInfoTypeForString("CIVIC_MARTIAL")
	giVolArmyCivic = GC.getInfoTypeForString("CIVIC_VOLUNTARY")

def onUnitBuilt( argsList):
	city = argsList[0]
	unit = argsList[1]
	iCityOwner, iCityId = city
	iOwner, iUnitId = unit
	aUnit = STATE.getUnitRead(iOwner, iUnitId)
	pPlayer = GC.getPlayer(iOwner)

# BEGIN MILITIA PROMOTIONS CODE - based on a prototype from FfH mod
# If the civic is a military one and if the unit being built is not a settler, worker or hero, then begin the function
# Every tile around the city is checked and added to a count based on terrain/plot type
# Based on these results, there is a chance the unit will be given a free Winterborn, Sand Devil, Cliff Walker or Tree Warden promotion

	iMilitaryCivic = 0
	if pPlayer.isCivic(giMilInstCivic):
		iMilitaryCivic += 1
	if pPlayer.isCivic(giVolArmyCivic):
		iMilitaryCivic += 1

	if iMilitaryCivic:
		if (aUnit[UnitReadKind.UNIT_READ_COMBAT_CLASS] not in gaiSettlerWorkerCombatList
		and not isWorldUnit(aUnit[UnitReadKind.UNIT_READ_TYPE])):
			aCityPos = STATE.getCityPosition(iCityOwner, iCityId)
			iX = aCityPos[0]
			iY = aCityPos[1]
			MAP = GC.getMap()

			if aUnit[UnitReadKind.UNIT_READ_DOMAIN] == DomainTypes.DOMAIN_LAND:
				iNumCold = 0
				iNumHot = 0
				iNumBush = 0
				iNumHill = 0
				iNumTree = 0
				iNumCoast = 0
				iTaiga = GC.getInfoTypeForString('TERRAIN_TAIGA')
				iTundra = GC.getInfoTypeForString('TERRAIN_TUNDRA')
				iPermafrost = GC.getInfoTypeForString('TERRAIN_PERMAFROST')
				iSnow = GC.getInfoTypeForString('TERRAIN_ICE')
				iDesert = GC.getTERRAIN_DESERT()
				iDunes = GC.getInfoTypeForString('TERRAIN_DUNES')
				iSaltFlats = GC.getInfoTypeForString('TERRAIN_SALT_FLATS')
				iBarren = GC.getInfoTypeForString('TERRAIN_BARREN')
				iRocky = GC.getInfoTypeForString('TERRAIN_ROCKY')
				iScrub = GC.getInfoTypeForString('TERRAIN_SCRUB')
				iMarsh = GC.getInfoTypeForString('TERRAIN_MARSH')

				for x in range(iX - 1, iX + 2):
					for y in range(iY - 1, iY + 2):
						plot = MAP.plot(x, y)
						if not plot: continue
						if plot.isWater():
							if plot.isCoastal():
								iNumCoast += 1
							continue
						elif plot.isHills() or plot.isPeak():
							iNumHill += 1

						iTerrain = plot.getTerrainType()
						if iTerrain in (iDesert, iDunes, iSaltFlats):
							iNumHot += 1
						elif iTerrain in (iTaiga, iTundra, iSnow, iPermafrost):
							iNumCold += 1
						elif iTerrain in (iBarren, iRocky, iScrub, iMarsh):
							iNumBush += 1

						iFeature = plot.getFeatureType()
						if iFeature > -1 and iFeature in aTreeList:
							iNumTree += 1

				attemptPromotion(iOwner, iUnitId, (iNumTree  * 1.25 * iMilitaryCivic), "PROMOTION_GREEN_WARDEN")
				attemptPromotion(iOwner, iUnitId, (iNumCold  * 1.25 * iMilitaryCivic), "PROMOTION_WINTERBORN")
				attemptPromotion(iOwner, iUnitId, (iNumHot   * 1.5  * iMilitaryCivic), "PROMOTION_SAND_DEVIL")
				attemptPromotion(iOwner, iUnitId, (iNumBush  * 2    * iMilitaryCivic), "PROMOTION_BUSHMAN")
				attemptPromotion(iOwner, iUnitId, (iNumHill  * 1.5  * iMilitaryCivic), "PROMOTION_CLIFF_WALKER")
				attemptPromotion(iOwner, iUnitId, (iNumCoast * 1.5  * iMilitaryCivic), "PROMOTION_AMPHIBIOUS")

			elif aUnit[UnitReadKind.UNIT_READ_DOMAIN] == DomainTypes.DOMAIN_SEA:
				iNumReef = 0
				iNumIce = 0
				iIce = GC.getInfoTypeForString('FEATURE_ICE')

				for x in range(iX - 1, iX + 2):
					for y in range(iY - 1, iY + 2):
						plot = MAP.plot(x, y)
						if not plot or not plot.isWater():
							continue
						iFeature = plot.getFeatureType()
						if iFeature < 0:
							continue
						if iFeature in aReefList:
							iNumReef += 1
						elif iFeature == iIce:
							iNumIce += 1

				attemptPromotion(iOwner, iUnitId, (iNumReef * 1.25 * iMilitaryCivic), "PROMOTION_COASTAL_ASSAULT1")
				attemptPromotion(iOwner, iUnitId, (iNumIce  * 1.25 * iMilitaryCivic), "PROMOTION_COASTAL_GUARD1")

def attemptPromotion(iPlayer, iUnit, iChance, szProposedPromotion):
	if GC.getGame().getSorenRandNum(100, "") < iChance:
		ePromotion = GC.getInfoTypeForString(szProposedPromotion)
		# The WHOLE verdict, not ENABLER.getPromotionUnlocked: that one answers only whether the PLAYER has the
		# promotion available and would offer it on units it does not apply to ([enabler.md] par.8).
		if STATE.canUnitAcquirePromotion(iPlayer, iUnit, ePromotion):
			ACT.setUnitPromotion(iPlayer, iUnit, ePromotion, True)
