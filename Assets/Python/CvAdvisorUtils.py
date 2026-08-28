from CvPythonExtensions import *

# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
GAME = GC.getGame()
ENABLER = CyEnabler()
ENUMS = CyEnums()
INFO = CyInfo()
UNIT = CyUnitInfo()           # the per-info UNIT accessor
BUILDING = CyBuildingInfo()   # the per-info BUILDING accessor
TRNSLTR = CyTranslator()
MAP = GC.getMap()

#	CyCity carries the IDENTITY SET only -- owner, id, position ([patterns.md] THE IDENTITY SET) -- so a
#	handler holding (owner, id) reaches the city's PLOT and AREA through the MAP by its position, never off the
#	handle. Both are ordinary engine handles the cut never touched: it was DIRECTIONAL and took the READ
#	bindings, so CyPlot and CyArea still publish their own surfaces.
def cityPlot(iPlayer, iCityID):
	aPosition = GC.getPlayer(iPlayer).getCity(iCityID).getPosition()
	return MAP.plot(aPosition[0], aPosition[1])

def cityArea(iPlayer, iCityID):
	return cityPlot(iPlayer, iCityID).area()

lPopulation = [
	[2000000000, FeatTypes.FEAT_POPULATION_2_BILLION, "TXT_KEY_FEAT_2_BILLION"],
	[1000000000, FeatTypes.FEAT_POPULATION_1_BILLION, "TXT_KEY_FEAT_1_BILLION"],
	[500000000, FeatTypes.FEAT_POPULATION_500_MILLION, "TXT_KEY_FEAT_500_MILLION"],
	[200000000, FeatTypes.FEAT_POPULATION_200_MILLION, "TXT_KEY_FEAT_200_MILLION"],
	[100000000, FeatTypes.FEAT_POPULATION_100_MILLION, "TXT_KEY_FEAT_100_MILLION"],
	[50000000, FeatTypes.FEAT_POPULATION_50_MILLION, "TXT_KEY_FEAT_50_MILLION"],
	[20000000, FeatTypes.FEAT_POPULATION_20_MILLION, "TXT_KEY_FEAT_20_MILLION"],
	[10000000, FeatTypes.FEAT_POPULATION_10_MILLION, "TXT_KEY_FEAT_10_MILLION"],
	[5000000, FeatTypes.FEAT_POPULATION_5_MILLION, "TXT_KEY_FEAT_5_MILLION"],
	[2000000, FeatTypes.FEAT_POPULATION_2_MILLION, "TXT_KEY_FEAT_2_MILLION"],
	[1000000, FeatTypes.FEAT_POPULATION_1_MILLION, "TXT_KEY_FEAT_1_MILLION"],
	[500000, FeatTypes.FEAT_POPULATION_HALF_MILLION, "TXT_KEY_FEAT_HALF_MILLION"]
]
g_iAdvisorNags = 0

def resetNoLiberateCities():
	global g_listNoLiberateCities
	g_listNoLiberateCities = []

	#	Walk the CORPORATIONS -- a few dozen -- and ask each one its own edges. The old shape swept all ~5200
	#	buildings asking each what it founds, with a nested sweep of all ~2000 units inside it, on the LOAD path.
	#	`FoundsCorporation` curates as the building's `enables.corporations`, so the corp's ENABLED_BY family IS
	#	its founding building, and the building's ENABLED_BY/techs family IS its unlocking techs
	#	([DEC-one-reverse-view]: every info already carries its reverse lookups; never a registry scan).
	global lCorporations
	lCorporations = []
	for eCorporation in xrange(GC.getNumCorporationInfos()):
		if GAME.isCorporationFounded(eCorporation):
			continue

		bonuses = INFO.getIdList("CORPORATION_", eCorporation, IdListSlot.PYLIST_CONSUMED_BONUSES)
		if not bonuses:
			continue

		for iBuilding in INFO.getEdgeIds("CORPORATION_", eCorporation, EdgeFamily.EDGEF_ENABLED_BY, EdgeBucket.EDGEB_BUILDINGS):
			#	The founder is the unit whose `grants.buildings` names the HQ. RELATED is a candidate SUPERSET
			#	(it merges every relation), so it is FILTERED to the exact grant rather than trusted.
			iFounder = -1
			for iUnit in INFO.getEdgeIds("BUILDING_", iBuilding, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_UNITS):
				if iBuilding in INFO.getIdList("UNIT_", iUnit, IdListSlot.PYLIST_GRANTED_BUILDINGS):
					iFounder = iUnit
					break
			if iFounder < 0:
				continue

			techs = INFO.getEdgeIds("BUILDING_", iBuilding, EdgeFamily.EDGEF_ENABLED_BY, EdgeBucket.EDGEB_TECHS)
			lCorporations.append([eCorporation, techs, iFounder, bonuses])
			break

	#	A bonus's own compiled wellbeing decides whether it is a luxury / a health resource -- the entity is
	#	asked what it carries, rather than the registry being swept through a dead accessor.
	global lBonus
	lBonus = []
	lLuxury = []
	lFood = []
	for i in xrange(GC.getNumBonusInfos()):
		aWellbeing = INFO.getWellbeing("BONUS_", i, CascScope.CASC_SCOPE_CITY)
		if aWellbeing[WellbeingChannel.WELLBEING_HAPPINESS] > 0:
			lLuxury.append(i)
		if aWellbeing[WellbeingChannel.WELLBEING_HEALTH] > 0:
			lFood.append(i)
	iBonus = GC.getInfoTypeForString("BONUS_COPPER_ORE")
	if iBonus > -1:
		lBonus.append([FeatTypes.FEAT_COPPER_CONNECTED, [iBonus], "TXT_KEY_FEAT_COPPER_CONNECTED"])
	iBonus = GC.getInfoTypeForString("BONUS_HORSE")
	if iBonus > -1:
		lBonus.append([FeatTypes.FEAT_HORSE_CONNECTED, [iBonus], "TXT_KEY_FEAT_HORSE_CONNECTED"])
	iBonus = GC.getInfoTypeForString("BONUS_IRON_ORE")
	if iBonus > -1:
		lBonus.append([FeatTypes.FEAT_IRON_CONNECTED, [iBonus], "TXT_KEY_FEAT_IRON_CONNECTED"])
	if lLuxury:
		lBonus.append([FeatTypes.FEAT_LUXURY_CONNECTED, lLuxury, "TXT_KEY_FEAT_LUXURY_CONNECTED"])
	if lFood:
		lBonus.append([FeatTypes.FEAT_FOOD_CONNECTED, lFood, "TXT_KEY_FEAT_FOOD_CONNECTED"])

	global unitCombatFeats
	unitCombatFeats = []
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_ARCHER"), FeatTypes.FEAT_UNITCOMBAT_ARCHER, "TXT_KEY_FEAT_UNITCOMBAT_ARCHER"))
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_MOUNTED"), FeatTypes.FEAT_UNITCOMBAT_MOUNTED, "TXT_KEY_FEAT_UNITCOMBAT_MOUNTED"))
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_MELEE"), FeatTypes.FEAT_UNITCOMBAT_MELEE, "TXT_KEY_FEAT_UNITCOMBAT_MELEE"))
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_SIEGE"), FeatTypes.FEAT_UNITCOMBAT_SIEGE, "TXT_KEY_FEAT_UNITCOMBAT_SIEGE"))
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_GUN"), FeatTypes.FEAT_UNITCOMBAT_GUN, "TXT_KEY_FEAT_UNITCOMBAT_GUN"))
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_HELICOPTER"), FeatTypes.FEAT_UNITCOMBAT_HELICOPTER, "TXT_KEY_FEAT_UNITCOMBAT_HELICOPTER"))
	unitCombatFeats.append((GC.getInfoTypeForString("UNITCOMBAT_MOTILITY_NAVAL"), FeatTypes.FEAT_UNITCOMBAT_NAVAL, "TXT_KEY_FEAT_UNITCOMBAT_NAVAL"))


def unitBuiltFeats(CyCity, CyUnit):
	#	The handle carries its ADDRESS and nothing else (THE IDENTITY SET), so resolve the pair
	#	once and ask the object itself for every value below.
	iPlayer, iCityID = CyCity
	iUnitOwner, iUnitID = CyUnit
	aUnit = GC.getPlayer(iUnitOwner).getUnit(iUnitID).getRead()
	szUnitName = GC.getPlayer(iUnitOwner).getUnit(iUnitID).getName()
	CyPlayer = GC.getPlayer(iPlayer)

	for iCombat, eFeat, szTxt in unitCombatFeats:
		if not CyPlayer.isFeatAccomplished(eFeat) and GC.getPlayer(iUnitOwner).getUnit(iUnitID).hasCombat(iCombat):
			CyPlayer.setFeatAccomplished(eFeat, True)
			if not GAME.isNetworkMultiPlayer() and GAME.getElapsedGameTurns() != 0 and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
				popupInfo.setData1(eFeat)
				popupInfo.setData2(iCityID)
				popupInfo.setText(TRNSLTR.getText(szTxt, (szUnitName, GC.getPlayer(iPlayer).getCity(iCityID).getName(),)))
				popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
				popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
				popupInfo.addPopup(iPlayer)

	if not CyPlayer.isFeatAccomplished(FeatTypes.FEAT_UNIT_PRIVATEER):
		if (GC.getPlayer(iUnitOwner).getUnit(iUnitID).isHiddenNationality()
		and aUnit[UnitReadKind.UNIT_READ_DOMAIN] == DomainTypes.DOMAIN_SEA):
			CyPlayer.setFeatAccomplished(FeatTypes.FEAT_UNIT_PRIVATEER, True)
			if not GAME.isNetworkMultiPlayer() and GAME.getElapsedGameTurns() != 0 and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
				popupInfo.setData1(FeatTypes.FEAT_UNIT_PRIVATEER)
				popupInfo.setData2(iCityID)
				popupInfo.setText(TRNSLTR.getText("TXT_KEY_FEAT_UNIT_PRIVATEER", (szUnitName, GC.getPlayer(iPlayer).getCity(iCityID).getName(), )))
				popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
				popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
				popupInfo.addPopup(iPlayer)

	if not CyPlayer.isFeatAccomplished(FeatTypes.FEAT_UNIT_SPY):
		#	The handle carries owner + id only, so the TYPE is asked of the state surface and the spy verdict
		#	of the info surface -- neither is on a wrapper any more.
		if INFO.isSpy(aUnit[UnitReadKind.UNIT_READ_TYPE]):
			CyPlayer.setFeatAccomplished(FeatTypes.FEAT_UNIT_SPY, True)
			if not GAME.isNetworkMultiPlayer() and GAME.getElapsedGameTurns() != 0 and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
				popupInfo.setData1(FeatTypes.FEAT_UNIT_SPY)
				popupInfo.setData2(iCityID)
				popupInfo.setText(TRNSLTR.getText("TXT_KEY_FEAT_UNIT_SPY", (szUnitName, GC.getPlayer(iPlayer).getCity(iCityID).getName(), )))
				popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
				popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
				popupInfo.addPopup(iPlayer)


def endTurnFeats(iPlayer):
	global g_iAdvisorNags
	g_iAdvisorNags = 0

	CyPlayer = GC.getPlayer(iPlayer)
	CyCity0 = CyPlayer.getCapitalCity()
	if CyCity0 is None: return
	# A city handle carries its ADDRESS only; every value below it is asked of the object itself by that
	# address, so resolve the capital's id once rather than per read.
	iCity0 = CyCity0.getID()

	# Population feat
	lRealPopulation = CyPlayer.getRealPopulation()
	for item in lPopulation:
		if CyPlayer.isFeatAccomplished(item[1]): break
		if lRealPopulation > item[0]:
			CyPlayer.setFeatAccomplished(item[1], True)
			if not GAME.isNetworkMultiPlayer() and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
				popupInfo.setData1(item[1])
				popupInfo.setText(TRNSLTR.getText(item[2], (CyPlayer.getCivilizationDescriptionKey(), )))
				popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
				popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
				popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
				popupInfo.addPopup(iPlayer)
	# Trade Route
	if not CyPlayer.isFeatAccomplished(FeatTypes.FEAT_TRADE_ROUTE):
		for CyCityX in CyPlayer.cities():
			iCityX = CyCityX.getID()
			aFlags = GC.getPlayer(iPlayer).getCity(iCityX).getFlags()
			if not aFlags[CityFlagKind.CITY_FLAG_CAPITAL]:
				if aFlags[CityFlagKind.CITY_FLAG_CONNECTED_TO_CAPITAL]:
					CyPlayer.setFeatAccomplished(FeatTypes.FEAT_TRADE_ROUTE, True)
					if not GAME.isNetworkMultiPlayer() and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
						popupInfo = CyPopupInfo()
						popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
						popupInfo.setData1(FeatTypes.FEAT_TRADE_ROUTE)
						popupInfo.setData2(iCityX)
						popupInfo.setText(TRNSLTR.getText("TXT_KEY_FEAT_TRADE_ROUTE", (GC.getPlayer(iPlayer).getCity(iCityX).getName(), )))
						popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
						popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
						popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
						popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
						popupInfo.addPopup(iPlayer)
					break
	# First Bonuses Obtained
	for item in lBonus:
		if CyPlayer.isFeatAccomplished(item[0]): continue
		for iBonus in item[1]:
			if GC.getPlayer(iPlayer).getCity(iCity0).getNumBonusesAvailable(iBonus) > 0:
				CyPlayer.setFeatAccomplished(item[0], True)
				if not GAME.isNetworkMultiPlayer() and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
					popupInfo = CyPopupInfo()
					popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
					popupInfo.setData1(item[0])
					popupInfo.setData2(iCity0)
					popupInfo.setText(TRNSLTR.getText(item[2], (INFO.getTextKey("BONUS_", iBonus),)))
					popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
					popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
					popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
					popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
					popupInfo.addPopup(iPlayer)
					break
	# Corporations
	if not CyPlayer.isFeatAccomplished(FeatTypes.FEAT_CORPORATION_ENABLED):
		global lCorporations
		eTeam = CyPlayer.getTeam()
		pTeam = GC.getTeam(eTeam)
		i = 0
		while i < len(lCorporations):
			item = lCorporations[i]
			if GAME.isCorporationFounded(item[0]):
				del lCorporations[i]
			else:
				bValid = True
				for iTech in item[1]:
					if not pTeam.isHasTech(iTech):
						bValid = False
						break
				if bValid:
					CyPlayer.setFeatAccomplished(FeatTypes.FEAT_CORPORATION_ENABLED, True)
					szBonusList = u""
					for j in xrange(len(item[3])):
						eBonus = item[3][j]
						szBonusList += INFO.getDescription("BONUS_", eBonus)
						if j != len(item[3]) - 1:
							szBonusList += TRNSLTR.getText("TXT_KEY_OR", ())

					szFounder = INFO.getTextKey("UNIT_", item[2])
					szCorporation = INFO.getTextKey("CORPORATION_", item[0])

					if not GAME.isNetworkMultiPlayer() and iPlayer == GAME.getActivePlayer() and CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):
						popupInfo = CyPopupInfo()
						popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
						popupInfo.setData1(FeatTypes.FEAT_CORPORATION_ENABLED)
						popupInfo.setData2(CyCity0.getID())
						popupInfo.setText(TRNSLTR.getText("TXT_KEY_FEAT_CORPORATION_ENABLED", (szCorporation, szFounder, szBonusList)))
						popupInfo.setOnClickedPythonCallback("featAccomplishedOnClickedCallback")
						popupInfo.setOnFocusPythonCallback("featAccomplishedOnFocusCallback")
						popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_OK", ()), "")
						popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_FEAT_ACCOMPLISHED_MORE", ()), "")
						popupInfo.addPopup(iPlayer)
					break
				i += 1

#	The caller hands over the city's ADDRESS, not a handle -- see CvEventManager.onCityDoTurn.
def cityAdvise(iPlayer, iCityID):

	global g_iAdvisorNags

	aFlags = GC.getPlayer(iPlayer).getCity(iCityID).getFlags()
	if g_iAdvisorNags > 1 or aFlags[CityFlagKind.CITY_FLAG_DISORDER]:
		return
	CyPlayer = GC.getPlayer(iPlayer)

	if CyPlayer.isOption(PlayerOptionTypes.PLAYEROPTION_ADVISOR_POPUPS):

		iTurn = GAME.getGameTurn()
		iTurnFounded = GC.getPlayer(iPlayer).getCity(iCityID).getCounts()[CityCountRead.CITY_COUNT_GAME_TURN_FOUNDED]
		if iTurn % 40 == iTurnFounded % 40:

			if not iCityID in g_listNoLiberateCities:
				iPlayerX = GC.getPlayer(iPlayer).getCity(iCityID).getLiberationPlayer()
				if iPlayerX != -1:
					CyPlayerX = GC.getPlayer(iPlayerX)

					if GC.getTeam(CyPlayer.getTeam()).isHasMet(CyPlayerX.getTeam()):
						if not GC.getTeam(CyPlayerX.getTeam()).isAtWarWith(GAME.getActiveTeam()):
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_LIBERATION_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), CyPlayerX.getCivilizationDescriptionKey(), CyPlayerX.getNameKey())))
							popupInfo.setOnClickedPythonCallback("liberateOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_listNoLiberateCities.append(iCityID)
							g_iAdvisorNags += 1

				elif CyPlayer.canSplitEmpire() and CyPlayer.canSplitArea(cityArea(iPlayer, iCityID).getID()) and GC.getPlayer(iPlayer).getCity(iCityID).getAiValue() < 0:
					popupInfo = CyPopupInfo()
					popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
					popupInfo.setData1(iCityID)
					popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_COLONY_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), )))
					popupInfo.setOnClickedPythonCallback("colonyOnClickedCallback")
					popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
					popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
					popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
					popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
					popupInfo.addPopup(iPlayer)
					g_listNoLiberateCities.append(iCityID)
					g_iAdvisorNags += 1

		if aFlags[CityFlagKind.CITY_FLAG_PRODUCING]:

			if not GC.getPlayer(iPlayer).getCity(iCityID).isProductionUnit() and GC.getPlayer(iPlayer).getCity(iCityID).getOrderQueueLength() <= 1:

				if (iTurn + 3) % 40 == iTurnFounded % 40:

					if GAME.getElapsedGameTurns() < 200 and GC.getPlayer(iPlayer).getCity(iCityID).getPopulation() > 2 and not CyPlayer.AI_isFinancialTrouble():

						CyArea = cityArea(iPlayer, iCityID)
						if not CyPlayer.AI_totalAreaUnitAIs(CyArea, UnitAITypes.UNITAI_SETTLE) and CyArea.getBestFoundValue(iPlayer) > 0:

							iBestValue = 0
							eBestUnit = -1

							#	Iterate the ENABLER's maintained frontier, never the whole unit registry --
							#	it answers what this city can actually train, so canTrain is subsumed.
							#	BUT a QUEUED unit is STILL offered, and must be: you can build multiple
							#	copies, so a unit leaves the frontier only on a cap or supersession
							#	([enabler.md] par.7.1 -- the leave-rules differ per domain). Not nagging
							#	about what is already ordered is the RECOMMENDER's own concern, so it is
							#	asked of the CITY, never of availability.
							for iUnitX in ENABLER.getAvailableUnits(iPlayer, iCityID):

								if UNIT.getDomain(iUnitX) != DomainTypes.DOMAIN_LAND:
									continue
								if GC.getPlayer(iPlayer).getCity(iCityID).isUnitQueued(iUnitX):
									continue

								iValue = CyPlayer.AI_unitValue(iUnitX, UnitAITypes.UNITAI_SETTLE, CyArea)

								if iValue > iBestValue:

									iBestValue = iValue
									eBestUnit = iUnitX

							if eBestUnit > -1:
								popupInfo = CyPopupInfo()
								popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
								popupInfo.setData1(iCityID)
								popupInfo.setData2(OrderTypes.ORDER_TRAIN)
								popupInfo.setData3(eBestUnit)
								popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_UNIT_SETTLE_DEMAND", (INFO.getTextKey("UNIT_", eBestUnit), )))
								popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
								popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
								popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
								popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
								popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
								popupInfo.addPopup(iPlayer)
								g_iAdvisorNags += 1

				if (iTurn + 15) % 40 == iTurnFounded % 40:
					if GC.getPlayer(iPlayer).getCity(iCityID).getPopulation() > 1 and not GC.getPlayer(iPlayer).getCity(iCityID).getImprovedPlotCount():
						CyArea = cityArea(iPlayer, iCityID)
						eBestUnit = -1

						if GC.getPlayer(iPlayer).getCity(iCityID).getAiBestBuildCount() > 3:
							iBestValue = 0
							#	A queued unit stays on the frontier by design (multiple copies), so the
							#	'already ordered' suppression is the recommender's, asked of the city.
							for iUnit in ENABLER.getAvailableUnits(iPlayer, iCityID):
								if UNIT.getDomain(iUnit) != DomainTypes.DOMAIN_LAND:
									continue
								if GC.getPlayer(iPlayer).getCity(iCityID).isUnitQueued(iUnit):
									continue

								iValue = CyPlayer.AI_unitValue(iUnit, UnitAITypes.UNITAI_WORKER, CyArea)
								if iValue > iBestValue:
									iBestValue = iValue
									eBestUnit = iUnit

						if eBestUnit != -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_TRAIN)
							popupInfo.setData3(eBestUnit)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_UNIT_WORKER_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("UNIT_", eBestUnit))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if (iTurn + 27) % 40 == iTurnFounded % 40:

					if not cityPlot(iPlayer, iCityID).getNumDefenders(iPlayer):

						CyArea = cityArea(iPlayer, iCityID)
						iBestValue = 0
						eBestUnit = -1

						#	The maintained frontier, not a registry sweep -- already gated, so canTrain is
						#	subsumed. NO queued-suppression here, deliberately: this fires only when the
						#	city has NO defender at all, and the legacy path did not suppress either.
						for iUnit in ENABLER.getAvailableUnits(iPlayer, iCityID):

							if UNIT.getDomain(iUnit) != DomainTypes.DOMAIN_LAND:
								continue
							iValue = CyPlayer.AI_unitValue(iUnit, UnitAITypes.UNITAI_CITY_DEFENSE, CyArea) * 2
							iValue += CyPlayer.AI_unitValue(iUnit, UnitAITypes.UNITAI_ATTACK, CyArea)
							if iValue > iBestValue:
								iBestValue = iValue
								eBestUnit = iUnit

						if eBestUnit != -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_TRAIN)
							popupInfo.setData3(eBestUnit)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_UNIT_DEFENSE_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("UNIT_", eBestUnit))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if (iTurn + 36) % 40 == iTurnFounded % 40:

					CyArea = cityArea(iPlayer, iCityID)
					if not CyPlayer.AI_totalAreaUnitAIs(CyArea, UnitAITypes.UNITAI_MISSIONARY) and not GC.getTeam(CyPlayer.getTeam()).isAtWar(False):

						eStateReligion = CyPlayer.getStateReligion()

						if eStateReligion != -1:

							if CyPlayer.getHasReligionCount(eStateReligion) < CyPlayer.getNumCities() / 2:

								iBestValue = 0
								iBestUnit = -1

								#	The maintained frontier, not the unit registry -- already gated, so canTrain
								#	is subsumed. The religion test is the SPECIFIC one: a missionary for somebody
								#	else's faith spreads the wrong religion.
								for iUnitX in ENABLER.getAvailableUnits(iPlayer, iCityID):

									if UNIT.getDomain(iUnitX) != DomainTypes.DOMAIN_LAND: continue
									if not INFO.spreadsReligion(iUnitX, eStateReligion): continue

									iValue = CyPlayer.AI_unitValue(iUnitX, UnitAITypes.UNITAI_MISSIONARY, CyArea)

									if iValue > iBestValue:

										iBestValue = iValue
										iBestUnit = iUnitX

								if iBestUnit > -1:
									popupInfo = CyPopupInfo()
									popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
									popupInfo.setData1(iCityID)
									popupInfo.setData2(OrderTypes.ORDER_TRAIN)
									popupInfo.setData3(iBestUnit)
									popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_MISSIONARY_DEMAND", (INFO.getTextKey("RELIGION_", eStateReligion), INFO.getTextKey("UNIT_", iBestUnit), GC.getPlayer(iPlayer).getCity(iCityID).getName())))
									popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
									popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
									popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
									popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
									popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
									popupInfo.addPopup(iPlayer)
									g_iAdvisorNags += 1

			if GC.getPlayer(iPlayer).getCity(iCityID).getOrder()[CityOrderRead.ORDER_READ_TYPE] != OrderTypes.ORDER_CONSTRUCT and GC.getPlayer(iPlayer).getCity(iCityID).getOrderQueueLength() <= 1:

				if GC.getPlayer(iPlayer).getCity(iCityID).getHealthRate(0) < 0:

					if (iTurn + 6) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The ENABLER's maintained frontier, never the building registry -- it answers what this
						#	city can actually construct, so canConstruct is subsumed ([enabler.md] par.6: the frontier
						#	IS the shared choice set, iterated instead of the entity database).
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedWellbeing("BUILDING_", iBuildingX, iPlayer, iCityID)[WellbeingChannel.WELLBEING_HEALTH]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_UNHEALTHY_CITIZENS_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_UNHEALTHY_DO_SO_NEXT", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_UNHEALTHY_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_UNHEALTHY_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if GC.getPlayer(iPlayer).getCity(iCityID).getAngryPopulation(0) > 0:

					if (iTurn + 9) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The ENABLER's maintained frontier, never the building registry -- it answers what this
						#	city can actually construct, so canConstruct is subsumed ([enabler.md] par.6: the frontier
						#	IS the shared choice set, iterated instead of the entity database).
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedWellbeing("BUILDING_", iBuildingX, iPlayer, iCityID)[WellbeingChannel.WELLBEING_HAPPINESS]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_UNHAPPY_CITIZENS_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_UNHAPPY_DO_SO_NEXT", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_UNHAPPY_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_UNHEALTHY_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if iTurn < 100 and GC.getTeam(CyPlayer.getTeam()).getHasMetCivCount(True) > 0 and not GC.getPlayer(iPlayer).getCity(iCityID).getDefenseKinds()[DefenseKind.DEFENSE_AMOUNT]:

					if (iTurn + 12) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The ENABLER's maintained frontier, never the building registry -- it answers what this
						#	city can actually construct, so canConstruct is subsumed ([enabler.md] par.6: the frontier
						#	IS the shared choice set, iterated instead of the entity database).
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedDefenseKinds("BUILDING_", iBuildingX, iPlayer, iCityID)[DefenseKind.DEFENSE_AMOUNT]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_BUILDING_DEFENSE_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if GC.getPlayer(iPlayer).getCity(iCityID).getRealizedMaintenance() >= 800:

					if (iTurn + 18) % 40 == iTurnFounded % 40:

						iBestBuilding = -1

						#	No metric here -- the first constructible non-wonder wins, as it always did. The
						#	frontier is already the constructible set, so the walk is the gate.
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iBestBuilding = iBuildingX
							break

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_MAINTENANCE_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if GC.getPlayer(iPlayer).getCity(iCityID).getCommerces()[CommerceTypes.COMMERCE_CULTURE] < 1000 and not aFlags[CityFlagKind.CITY_FLAG_OCCUPATION]:

					if (iTurn + 21) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The ENABLER's maintained frontier, never the building registry -- it answers what this
						#	city can actually construct, so canConstruct is subsumed ([enabler.md] par.6: the frontier
						#	IS the shared choice set, iterated instead of the entity database).
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedFlatCommerces("BUILDING_", iBuildingX, iPlayer, iCityID)[CommerceTypes.COMMERCE_CULTURE]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_CULTURE_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if GC.getPlayer(iPlayer).getCity(iCityID).getCommerces()[CommerceTypes.COMMERCE_GOLD] > 1000:

					if (iTurn + 24) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The ENABLER's maintained frontier, never the building registry -- it answers what this
						#	city can actually construct, so canConstruct is subsumed ([enabler.md] par.6: the frontier
						#	IS the shared choice set, iterated instead of the entity database).
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedCommerceModifiers("BUILDING_", iBuildingX, iPlayer, iCityID)[CommerceTypes.COMMERCE_GOLD]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_GOLD_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if GC.getPlayer(iPlayer).getCity(iCityID).getCommerces()[CommerceTypes.COMMERCE_RESEARCH] > 1000:

					if (iTurn + 30) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The ENABLER's maintained frontier, never the building registry -- it answers what this
						#	city can actually construct, so canConstruct is subsumed ([enabler.md] par.6: the frontier
						#	IS the shared choice set, iterated instead of the entity database).
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedCommerceModifiers("BUILDING_", iBuildingX, iPlayer, iCityID)[CommerceTypes.COMMERCE_RESEARCH]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_RESEARCH_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1

				if GC.getPlayer(iPlayer).getCity(iCityID).getWaterPlotCount() > 10:

					if (iTurn + 33) % 40 == iTurnFounded % 40:

						iBestValue = 0
						iBestBuilding = -1

						#	The candidate's whole PLOTS-TARGET food contribution in ONE read, scaled by this
						#	city's own stored plotAttrs counts ([contexts.md]). The legacy per-PlotType entry
						#	walk has no counterpart: PLOT_OCEAN stopped being a key ([json.md] par.6.1 -- a water
						#	plot is `plots {IS_WATER}`), and the data now authors this effect TWO ways at once
						#	(a terrain-keyed flat, and a predicate-gated plots entry). The what-if folds both.
						for iBuildingX in ENABLER.getAvailableBuildings(iPlayer, iCityID):

							if BUILDING.isLimitedWonder(iBuildingX): continue

							iValue = INFO.expectedPlotYields("BUILDING_", iBuildingX, iPlayer, iCityID)[YieldTypes.YIELD_FOOD]
							if iValue > iBestValue:

								iBestValue = iValue
								iBestBuilding = iBuildingX

						if iBestBuilding > -1:
							popupInfo = CyPopupInfo()
							popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON)
							popupInfo.setData1(iCityID)
							popupInfo.setData2(OrderTypes.ORDER_CONSTRUCT)
							popupInfo.setData3(iBestBuilding)
							popupInfo.setText(TRNSLTR.getText("TXT_KEY_POPUP_WATER_FOOD_DEMAND", (GC.getPlayer(iPlayer).getCity(iCityID).getName(), INFO.getTextKey("BUILDING_", iBestBuilding))))
							popupInfo.setOnClickedPythonCallback("cityWarningOnClickedCallback")
							popupInfo.setOnFocusPythonCallback("cityWarningOnFocusCallback")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_AGREE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_REFUSE", ()), "")
							popupInfo.addPythonButton(TRNSLTR.getText("TXT_KEY_POPUP_DEMAND_EXAMINE", ()), "")
							popupInfo.addPopup(iPlayer)
							g_iAdvisorNags += 1
