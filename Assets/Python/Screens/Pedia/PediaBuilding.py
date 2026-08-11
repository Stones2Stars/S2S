# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()
BUILDING = CyBuildingInfo()   # the per-info BUILDING accessor
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class PediaBuilding:

	def __init__(self, parent, H_BOT_ROW):

		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.Y_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - H_BOT_ROW

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4
		self.H_BOT_ROW = H_BOT_ROW

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE

		W_BASE = 64 + H_TOP_ROW + W_PEDIA_PAGE / 8
		W_HALF_PP = W_PEDIA_PAGE / 2

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + W_BASE + 4
		self.X_COL_3 = X_COL_1 + W_HALF_PP + 4

		self.W_COL_1 = W_BASE - 4
		self.W_COL_2 = W_PEDIA_PAGE - W_BASE - H_TOP_ROW - 4
		self.W_COL_3 = W_HALF_PP - 4

		self.S_ICON = S_ICON = H_TOP_ROW - 10

		a = H_TOP_ROW / 12
		self.X_STATS = X_COL_1 + S_ICON
		self.Y_STATS = Y_TOP_ROW + a
		self.H_STATS = H_TOP_ROW - a * 2 + 4
		self.W_STATS = W_BASE - S_ICON

	def interfaceScreen(self, iTheBuilding):
		GC = CyGlobalContext()
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		CyPlayer = self.main.CyPlayer
		aName = self.main.getNextWidgetName

		bNotCulture = self.main.SECTION[1] != TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_C2C_CULTURES", ())

		eWidGen				= WidgetTypes.WIDGET_GENERAL
		ePanelBlue50		= PanelStyles.PANEL_STYLE_BLUE50

		enumGBS	= self.main.enumGBS
		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		H_TOP_ROW = self.H_TOP_ROW
		H_BOT_ROW = self.H_BOT_ROW
		S_ICON = self.S_ICON
		X_COL_1 = self.X_COL_1
		X_COL_2 = self.X_COL_2
		X_COL_3 = self.X_COL_3
		Y_TOP_ROW_1 = self.Y_TOP_ROW
		Y_TOP_ROW_2 = Y_TOP_ROW_1 + H_TOP_ROW
		Y_BOT_ROW_1 = self.Y_BOT_ROW
		Y_BOT_ROW_2 = Y_BOT_ROW_1 - H_BOT_ROW
		W_COL_1 = self.W_COL_1
		W_COL_2 = self.W_COL_2
		W_COL_3 = self.W_COL_3
		W_PEDIA_PAGE = self.W_PEDIA_PAGE
		H_ROW_2 = H_TOP_ROW * 3 + H_BOT_ROW

		# Main Panel
		szBuildingName = INFO.getDescription("BUILDING_", iTheBuilding)
		iMaxGlobalInstances = INFO.getAllowedCap("BUILDING_", iTheBuilding, AllowedCap.ALLOWEDCAP_WORLD)
		iMaxPlayerInstances = INFO.getAllowedCap("BUILDING_", iTheBuilding, AllowedCap.ALLOWEDCAP_EMPIRE)
		iMaxTeamInstances = INFO.getAllowedCap("BUILDING_", iTheBuilding, AllowedCap.ALLOWEDCAP_TEAM)
		if iMaxGlobalInstances > 0:
			szBuildingName += "<color=192,192,128,255> | " + TRNSLTR.getText("TXT_KEY_PEDIA_WORLD_WONDER",()) + " - Max. " + str(iMaxGlobalInstances)
		elif iMaxPlayerInstances > 0:
			szBuildingName += "<color=192,192,128,255> | " + TRNSLTR.getText("TXT_KEY_PEDIA_NATIONAL_WONDER",()) + " - Max. " + str(iMaxPlayerInstances)
		elif iMaxTeamInstances > 0:
			szBuildingName += "<color=192,192,128,255> | " + TRNSLTR.getText("TXT_KEY_PEDIA_TEAM_WONDER",()) + " - Max. " + str(iMaxTeamInstances)
		screen.setText(aName(), "", szfontEdge + szBuildingName, 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, eWidGen, 0, 0)

		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW_1 + 2, W_COL_1 + 8, H_TOP_ROW + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "Preview|Movie|BUILDING|ToolTip" + str(iTheBuilding)
		self.main.aWidgetBucket.append(Img)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("BUILDING_", iTheBuilding), 4, 6, S_ICON, S_ICON, eWidGen, 1, 1)
		# Stats
		panelName = aName()
		screen.addListBoxGFC(panelName, "", self.X_STATS, self.Y_STATS, self.W_STATS, self.H_STATS, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(panelName, False)
		iProductionCost = INFO.getIntrinsic("BUILDING_", iTheBuilding, IntrinsicSlot.PYINT_COST)
		if iProductionCost > 0:
			if CyPlayer:
				szCost = TRNSLTR.getText("TXT_KEY_PEDIA_COST", (CyPlayer.getBuildingProductionNeeded(iTheBuilding),))
			else:
				szCost = TRNSLTR.getText("TXT_KEY_PEDIA_COST", ((iProductionCost * GC.getDefineINT("BUILDING_PRODUCTION_PERCENT"))/100,))
			screen.appendListBoxStringNoUpdate(panelName, szfont3b + szCost + u'%c' %TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION), eWidGen, 0, 0, 1<<0)
		elif INFO.isAutoBuild(iTheBuilding):
			screen.appendListBoxStringNoUpdate(panelName, szfont3 + TRNSLTR.getText("TXT_KEY_PEDIA_AUTOBUILD",()) , eWidGen, 0, 0, 1<<0)
		szText1 = ""
		szText2 = ""
		aFlatYields = INFO.getFlatYields("BUILDING_", iTheBuilding, CascScope.CASC_SCOPE_CITY)
		aPctYields = INFO.getPercentYields("BUILDING_", iTheBuilding, CascScope.CASC_SCOPE_CITY)
		for k in range(YieldTypes.NUM_YIELD_TYPES):
			char = unichr(8483 + k)
			iTemp = aFlatYields[k] / 100
			if iTemp:
				if iTemp < 0:
					szValue = " <color=255,0,0,255"
				else:
					szValue = " <color=0,230,0,255"
				szValue += ">%d" % iTemp + char
				szText1 += szValue
			iTemp = aPctYields[k]
			if iTemp:
				if iTemp < 0:
					szValue = " <color=255,0,0,255>"
				else:
					szValue = " <color=0,230,0,255>"
				szValue += "%d%%" % iTemp + char
				szText2 += szValue
		if szText1:
			screen.appendListBoxStringNoUpdate(panelName, szfont3 + szText1, eWidGen, 0, 0, 1<<0)
			szText1 = ""
		if szText2:
			screen.appendListBoxStringNoUpdate(panelName, szfont3 + szText2, eWidGen, 0, 0, 1<<0)
			szText2 = ""
		aFlatCommerces = INFO.getFlatCommerces("BUILDING_", iTheBuilding, CascScope.CASC_SCOPE_CITY)
		aPctCommerces = INFO.getPercentCommerces("BUILDING_", iTheBuilding, CascScope.CASC_SCOPE_CITY)
		for k in range(CommerceTypes.NUM_COMMERCE_TYPES):
			char = unichr(8500 + k)
			iTemp = aFlatCommerces[k] / 100
			if iTemp:
				if iTemp < 0:
					szValue = " <color=255,0,0,255>"
				else:
					szValue = " <color=0,230,0,255>"
				szValue += "%d" % iTemp + char
				szText1 += szValue
			iTemp = aPctCommerces[k]
			if iTemp:
				if iTemp < 0:
					szValue = " <color=255,0,0,255>"
				else:
					szValue = " <color=0,230,0,255>"
				szValue += "%d%%" % iTemp + char
				szText2 += szValue
		if szText1:
			screen.appendListBoxStringNoUpdate(panelName, szfont3 + szText1, eWidGen, 0, 0, 1<<0)
			szText1 = ""
		if szText2:
			screen.appendListBoxStringNoUpdate(panelName, szfont3 + szText2, eWidGen, 0, 0, 1<<0)
			szText2 = ""
		aWellbeing = INFO.getWellbeing("BUILDING_", iTheBuilding, CascScope.CASC_SCOPE_CITY)
		iHappiness = (aWellbeing[WellbeingChannel.WELLBEING_HAPPINESS] - aWellbeing[WellbeingChannel.WELLBEING_ANGER]) / 100
		iHealth = (aWellbeing[WellbeingChannel.WELLBEING_HEALTH] - aWellbeing[WellbeingChannel.WELLBEING_UNHEALTH]) / 100
		if CyPlayer:
			iHappiness += CyPlayer.getExtraBuildingHappiness(iTheBuilding)
			iHealth += CyPlayer.getExtraBuildingHealth(iTheBuilding)
		if iHappiness:
			if iHappiness > 0:
				szText1 += " <color=0,230,0,255>%d" %iHappiness + unichr(8850)
			else:
				szText1 += " <color=255,0,0,255>%d" %-iHappiness + unichr(8851)
		if iHealth:
			if iHealth > 0:
				szText1 += " <color=0,230,0,255>%d" %iHealth + unichr(8852)
			else:
				szText1 += " <color=255,0,0,255>%d" %-iHealth + unichr(8853)
		if szText1:
			screen.appendListBoxStringNoUpdate(panelName, szfont3 + szText1, eWidGen, 0, 0, 1<<0)
			szText1 = ""
		if szText1:
			screen.appendListBoxStringNoUpdate(panelName, szfont3b + szText1, eWidGen, 0, 0, 1<<0)
		screen.updateListBox(panelName)
		# Strategy
		szStrategy = TRNSLTR.getText("TXT_KEY_PEDIA_STRATEGY", ())
		screen.addPanel(aName(), szStrategy, "", False, False, X_COL_2, Y_TOP_ROW_1, W_COL_2 - 4, H_TOP_ROW, ePanelBlue50)
		szStrategyText = szfont2 + INFO.getStrategy("BUILDING_", iTheBuilding)
		screen.addMultilineText(aName(), szStrategyText, X_COL_2 + 4, Y_TOP_ROW_1 + 32, W_COL_2 - 4, H_TOP_ROW - 40, eWidGen, 0, 0, 1<<0)
		# Graphic
		s = H_TOP_ROW - 6
		screen.addBuildingGraphicGFC("Preview|Min", iTheBuilding, X_COL_2 + W_COL_2 + 4, Y_TOP_ROW_1 + 8, s, s, eWidGen, iTheBuilding, 0, -20, 30, 0.4, True)
		self.main.aWidgetBucket.append("Preview|Min")
		# Replaced By
		PF = "ToolTip|JumpTo|"
		szChild = PF + "BUILDING"
		aList1 = []
		aList2 = []
		aList3 = []
		aList4 = []
		aList5 = []
		if bNotCulture:
			for iReplacement in INFO.getDormantTriggerIds("BUILDING_", iTheBuilding):
				if not INFO.isNotConstructible("BUILDING_", iReplacement):
					aList2.append(iReplacement)
			if aList1 or aList2:
				if aList1 and aList2:
					W_REP_1 = W_REP_2 = W_COL_3
					X_REP_2 = X_COL_3
					if len(aList1) < 4:
						if len(aList2) > 4:
							W_REP_1 = W_COL_1
							W_REP_2 = W_PEDIA_PAGE - W_COL_1 - 8
							X_REP_2 = X_COL_2
					elif len(aList2) < 4:
						if len(aList1) > 4:
							W_REP_1 = W_PEDIA_PAGE - W_COL_1 - 8
							W_REP_2 = W_COL_1
							X_REP_2 = X_COL_1 + W_REP_1 + 4
					replaceFor = aName()
					replacedBy = aName()
					screen.addPanel(replaceFor, TRNSLTR.getText("TXT_KEY_PEDIA_REPLACEMENT_FOR", ()), "", False, True, X_COL_1, Y_BOT_ROW_1, W_REP_1, H_BOT_ROW, ePanelBlue50)
					screen.addPanel(replacedBy, TRNSLTR.getText("TXT_KEY_PEDIA_REPLACED_BY", ()), "", False, True, X_REP_2, Y_BOT_ROW_1, W_REP_2, H_BOT_ROW, ePanelBlue50)
				elif aList1:
					replaceFor = aName()
					screen.addPanel(replaceFor, TRNSLTR.getText("TXT_KEY_PEDIA_REPLACEMENT_FOR", ()), "", False, True, X_COL_1, Y_BOT_ROW_1, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
				elif aList2:
					replacedBy = aName()
					screen.addPanel(replacedBy, TRNSLTR.getText("TXT_KEY_PEDIA_REPLACED_BY", ()), "", False, True, X_COL_1, Y_BOT_ROW_1, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
				if aList1:
					for i in range(len(aList1)):
						ID = aList1[i]
						screen.attachImageButton(replaceFor, szChild + str(ID), INFO.getButton("BUILDING_", ID), enumGBS, eWidGen, 1, 1, False)
					aList1 = []
				if aList2:
					for i in range(len(aList2)):
						ID = aList2[i]
						screen.attachImageButton(replacedBy, szChild + str(ID), INFO.getButton("BUILDING_", ID), enumGBS, eWidGen, 1, 1, False)
					aList2 = []
			else:
				Y_BOT_ROW_2 = Y_BOT_ROW_1
				H_ROW_2 += H_BOT_ROW
		else:
			Y_BOT_ROW_2 = Y_BOT_ROW_1
			H_ROW_2 += H_BOT_ROW
		# Requires
		panelName = aName()
		screen.addPanel(panelName, TRNSLTR.getText("TXT_KEY_PEDIA_REQUIRES", ()), "", False, True, X_COL_1, Y_BOT_ROW_2, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
		szAnd	= szfont3 + "&#38"
		szOr	= szfont2b + "||"
		szBracketL = szfont4b + " {"
		szBracketR = szfont4b + "} "
		bPlus = False
		# The requires tree, read PER CLAUSE so a needed entity is told apart from a barred one.
		# ⛔ REQCLAUSE_NONE is never collected: a `noneOf` names what BLOCKS the building, and drawing it in a
		# "Requires" panel tells the player to go and get the thing that refuses it.
		# ⚠ The mandatory and one-of ids are concatenated rather than bracketed here -- this panel draws each
		# kind as one flat run of buttons, so the {A || B} grouping PediaUnit now renders wants a layout change
		# to go with it. The FORBIDDEN half is the correctness half and is fixed; the brackets are cosmetic.
		def reqIds(eBucket):
			aIds = list(INFO.getRequiresIdsInClause("BUILDING_", iTheBuilding, eBucket, RequiresClause.REQCLAUSE_ALL))
			aIds.extend(INFO.getRequiresIdsInClause("BUILDING_", iTheBuilding, eBucket, RequiresClause.REQCLAUSE_ANY))
			return aIds
		aReqTechs = reqIds(EdgeBucket.EDGEB_TECHS)
		aReqReligions = reqIds(EdgeBucket.EDGEB_RELIGIONS)
		aReqCorps = reqIds(EdgeBucket.EDGEB_CORPORATIONS)
		aReqBonuses = reqIds(EdgeBucket.EDGEB_BONUSES)
		aReqBuildings = reqIds(EdgeBucket.EDGEB_BUILDINGS)
		aReqCivics = reqIds(EdgeBucket.EDGEB_CIVICS)
		aReqImprovements = reqIds(EdgeBucket.EDGEB_IMPROVEMENTS)
		# Tech Req
		szChild = PF + "TECH"
		for iType in aReqTechs:
			screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("TECH_", iType), enumGBS, eWidGen, 1, 1, False)
			bPlus = True

		# Religion Req
		szChild = PF + "RELIGION"
		for iType in aReqReligions:
			if bPlus:
				screen.attachLabel(panelName, "", szAnd)
			else:
				bPlus = True
			screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("RELIGION_", iType), enumGBS, eWidGen, 1, 1, False)
		# Corporation Req
		szChild = PF + "CORP"
		for iType in aReqCorps:
			if bPlus:
				screen.attachLabel(panelName, "", szAnd)
			else:
				bPlus = True
			screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("CORPORATION_", iType), enumGBS, eWidGen, 1, 1, False)
		# Bonus Req
		szChild = PF + "BONUS"
		iType = -1
		nOr = 0
		for iCheck in aReqBonuses:
			aList1.append(iCheck)
			nOr += 1
		if bPlus:
			if iType != -1 or nOr:
				screen.attachLabel(panelName, "", szAnd)
		elif iType != -1 or nOr:
			bPlus = True
		if iType > -1:
			screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BONUS_", iType), enumGBS, eWidGen, 1, 1, False)
		if nOr > 1:
			screen.attachLabel(panelName, "", szBracketL)
		i = 0
		if aList1:
			for iType in aList1:
				if i:
					screen.attachLabel(panelName, "", szOr)
				else: i = 1
				screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BONUS_", iType), enumGBS, eWidGen, 1, 1, False)
			aList1 = []
		if nOr > 1:
			screen.attachLabel(panelName, "", szBracketR)
		# Corporation Bonus Req
		iType = BUILDING.getHeadquartersCorporation(iTheBuilding)
		if iType != -1:
			lPrereqBonuses = INFO.getIdList("CORPORATION_", iType, IdListSlot.PYLIST_CONSUMED_BONUSES)
			nOr = len(lPrereqBonuses)
			if bPlus:
				if nOr:
					screen.attachLabel(panelName, "", szAnd)
			elif nOr:
				bPlus = True
			if nOr > 1:
				screen.attachLabel(panelName, "", szBracketL)
			for i in range(nOr):
				iType = lPrereqBonuses[i]
				if i != 0:
					screen.attachLabel(panelName, "", szOr)
				screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BONUS_", iType), enumGBS, eWidGen, 1, 1, False)
			if nOr > 1:
				screen.attachLabel(panelName, "", szBracketR)
		# Building Req
		szChild = PF + "BUILDING"
		szChild1 = szChild + "|Own"
		# And building requirements
		for j in aReqBuildings:
			aList1.append(j)


		if aList1 or aList2 or aList3 or aList4 or aList5:
			if bPlus:
				screen.attachLabel(panelName, "", szAnd)
			else:
				bPlus = True
			if aList3:
				screen.attachLabel(panelName, "", szBracketL + szfont2b + TRNSLTR.getText("TXT_KEY_PEDIA_OWN", ()))
				for i in range(len(aList3)):
					iType, iAmount = aList3[i]
					screen.attachLabel(panelName, "", szfont4b + " " + str(iAmount))
					screen.attachImageButton(panelName, szChild1 + str(iType), INFO.getButton("BUILDING_", iType), enumGBS, eWidGen, 1, 1, False)
				screen.attachLabel(panelName, "", szBracketR)
				if aList2 and not aList1:
					screen.attachLabel(panelName, "", szAnd)
				aList3 = []
			if aList1:
				for i in range(len(aList1)):
					iType = aList1[i]
					screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BUILDING_", iType), enumGBS, eWidGen, 1, 1, False)
				aList1 = []
			if aList2:
				iListLength = len(aList2)
				if iListLength > 1:
					screen.attachLabel(panelName, "", szBracketL)
				for i in range(iListLength):
					iType = aList2[i]
					if i != 0:
						screen.attachLabel(panelName, "", szOr)
					screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BUILDING_", iType), enumGBS, eWidGen, 1, 1, False)
				if iListLength > 1:
					screen.attachLabel(panelName, "", szBracketR)
				aList2 = []
			if aList4:
				for i in range(len(aList4)):
					iType = aList4[i]
					screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BUILDING_", iType), enumGBS, eWidGen, 1, 1, False)
				aList4 = []
			if aList5:
				iListLength = len(aList5)
				if iListLength > 1:
					screen.attachLabel(panelName, "", szBracketL)
				for i in range(iListLength):
					iType = aList5[i]
					if i != 0:
						screen.attachLabel(panelName, "", szOr)
					screen.attachImageButton(panelName, szChild + str(iType), INFO.getButton("BUILDING_", iType), enumGBS, eWidGen, 1, 1, False)
				if iListLength > 1:
					screen.attachLabel(panelName, "", szBracketR)
				aList5 = []

		# Civic Req
		szChild = PF + "CIVIC"
		for j in aReqCivics:
			aList1.append(CivicTypes(j))
		if aList1 or aList2:
			if bPlus:
				screen.attachLabel(panelName, "", szAnd)
			else:
				bPlus = True
		if aList1:
			for i in range(len(aList1)):
				ID = aList1[i]
				screen.attachImageButton(panelName, szChild + str(ID), INFO.getButton("CIVIC_", ID), enumGBS, eWidGen, 1, 1, False)
			aList1 = []
		if aList2:
			iListLength = len(aList2)
			if  iListLength > 1:
				screen.attachLabel(panelName, "", szBracketL)
			for i in range(iListLength):
				ID = aList2[i]
				if i != 0:
					screen.attachLabel(panelName, "", szOr)
				screen.attachImageButton(panelName, szChild + str(ID), INFO.getButton("CIVIC_", ID), enumGBS, eWidGen, 1, 1, False)
			if iListLength > 1:
				screen.attachLabel(panelName, "", szBracketR)
			aList2 = []
		# Improvement Req
		szChild = PF + "IMP"
		for iPrereqOrImprovement in aReqImprovements:
			aList2.append(iPrereqOrImprovement)
		if aList2:
			if bPlus:
				screen.attachLabel(panelName, "", szAnd)
			else:
				bPlus = True
			iListLength = len(aList2)
			if  iListLength > 1:
				screen.attachLabel(panelName, "", szBracketL)
			for i in range(iListLength):
				ID = aList2[i]
				if i != 0:
					screen.attachLabel(panelName, "", szOr)
				screen.attachImageButton(panelName, szChild + str(ID), INFO.getButton("IMPROVEMENT_", ID), enumGBS, eWidGen, 1, 1, False)
			if iListLength > 1:
				screen.attachLabel(panelName, "", szBracketR)
			aList2 = []
		if not bPlus:
			screen.deleteWidget(panelName)
			H_ROW_2 += H_BOT_ROW
		# Special Abilities
		screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_TOP_ROW_2, W_COL_3, H_ROW_2, ePanelBlue50)
		szSpecialText = szfont3 + CyGameTextMgr().getBuildingHelp(iTheBuilding, False, -1, -1, True, False, False)[1:]
		screen.addMultilineText(aName(), szSpecialText, X_COL_1 + 4, Y_TOP_ROW_2 + 12, W_COL_3 - 8, H_ROW_2 - 20, eWidGen, 0, 0, 1<<0)
		# History
		szHistory = TRNSLTR.getText("TXT_KEY_PEDIA_HISTORY", ())
		screen.addPanel(aName(), szHistory, "", True, False, X_COL_3, Y_TOP_ROW_2, W_COL_3, H_ROW_2, ePanelBlue50)
		szHistoryText = szfont2 + INFO.getCivilopedia("BUILDING_", iTheBuilding)
		screen.addMultilineText(aName(), szHistoryText, X_COL_3 + 4, Y_TOP_ROW_2 + 32, W_COL_3 - 8, H_ROW_2 - 40, eWidGen, 0, 0, 1<<0)
