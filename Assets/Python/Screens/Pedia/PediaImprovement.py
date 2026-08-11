# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class PediaImprovement:

	# The three OUTPUT yields, each its own modifier family (json.md par.6: the yields are SPLIT families, so
	# there is no single "yield" family to name), paired with the engine yield index the symbol table is keyed on.
	aYieldFamilies = (
		(YieldTypes.YIELD_FOOD, ModifierFamily.MODFAM_FOOD),
		(YieldTypes.YIELD_PRODUCTION, ModifierFamily.MODFAM_PRODUCTION),
		(YieldTypes.YIELD_COMMERCE, ModifierFamily.MODFAM_COMMERCE),
	)

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.Y_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - H_BOT_ROW

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4
		self.H_BOT_ROW = H_BOT_ROW

		iSize = 64
		iRoom = H_BOT_ROW - 40
		while True:
			if iSize < iRoom:
				self.S_BOT_ROW = iSize
				break
			iSize -= 4

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE
		self.W_3RD_PP = W_PEDIA_PAGE / 3

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE

		self.X_GRAPHIC = X_COL_1 + W_PEDIA_PAGE - H_TOP_ROW

		self.W_COL_1 = W_COL_1 = W_PEDIA_PAGE - H_TOP_ROW - 16

		self.S_ICON = S_ICON = H_TOP_ROW - 10

		self.W_STATS = W_COL_1 - S_ICON * 3

		i = H_TOP_ROW / 8
		self.X_MAIN = X_COL_1 + S_ICON - 8
		self.Y_MAIN = Y_TOP_ROW + i
		self.H_MAIN = H_TOP_ROW - i * 2

	def gatherConditionedYields(self, iTheImprove, eBucket):
		# The improvement's OWN conditioned yield entries, grouped by the entity that gates each one.
		#
		# A tech/civic/bonus boost to an improvement is authored deliverer-side and LANDED HERE by the readJson
		# reverse pass as a conditioned own-output entry, so the improvement already carries the answer. What
		# this replaces sweeps every tech, then every civic, then every bonus, asking each what it grants us --
		# the own-data inversion, and dead besides.
		#
		# The three output yields are SPLIT families (json.md par.6), so "every yield" is the three, asked once
		# each. Values arrive x100 like every amount; this is a reader, so it reduces at its point of use.
		dByAtom = {}
		for k, eFamily in self.aYieldFamilies:
			for entry in INFO.getConditionedEntries("IMPROVEMENT_", iTheImprove, eFamily, eBucket):
				iValue = entry["value"] / 100
				if not iValue:
					continue
				for iAtom in entry["atoms"]:
					if not dByAtom.has_key(iAtom):
						dByAtom[iAtom] = {}
					dByAtom[iAtom][k] = dByAtom[iAtom].get(k, 0) + iValue
		aOut = []
		aAtoms = dByAtom.keys()
		aAtoms.sort()
		for iAtom in aAtoms:
			szYield = ""
			for k, eFamily in self.aYieldFamilies:
				iYieldChange = dByAtom[iAtom].get(k, 0)
				if iYieldChange:
					if iYieldChange < 0:
						szYield += " <color=255,0,0,255>"
					else:
						szYield += " <color=0,230,0,255>"
					szYield += str(iYieldChange) + u'%c' % (TEXT.getSymbolChar("YIELD_", k))
			if szYield:
				aOut.append((iAtom, szYield))
		return aOut

	def interfaceScreen(self, iTheImprove):
		GC = CyGlobalContext()
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		aName = self.main.getNextWidgetName

		eWidGen			= WidgetTypes.WIDGET_GENERAL
		eWidJuToBonus	= WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS
		eWidJuToCivic	= WidgetTypes.WIDGET_PEDIA_JUMP_TO_CIVIC
		eWidJuToTech	= WidgetTypes.WIDGET_PEDIA_JUMP_TO_TECH
		ePnlBlue50		= PanelStyles.PANEL_STYLE_BLUE50
		ePnlEmpty		= PanelStyles.PANEL_STYLE_EMPTY
		eTblEmpty		= TableStyles.TABLE_STYLE_EMPTY
		eFontTitle		= FontTypes.TITLE_FONT

		enumGBS = self.main.enumGBS
		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		W_PEDIA_PAGE = self.W_PEDIA_PAGE
		S_ICON = self.S_ICON
		X_COL_1 = self.X_COL_1
		W_COL_1 = self.W_COL_1
		H_BOT_ROW = self.H_BOT_ROW
		Y_BOT_ROW_1 = self.Y_BOT_ROW
		H_TOP_ROW_1 = self.H_TOP_ROW
		Y_TOP_ROW_1 = self.Y_TOP_ROW
		H_TOP_ROW_2 = H_BOT_ROW + H_BOT_ROW / 4
		Y_TOP_ROW_2 = Y_TOP_ROW_1 + H_TOP_ROW_1
		Y_MID_ROW = Y_TOP_ROW_2 + H_TOP_ROW_2
		H_MID_ROW = Y_BOT_ROW_1 - Y_MID_ROW
		W_STATS = self.W_STATS
		X_MAIN = self.X_MAIN
		Y_MAIN = self.Y_MAIN
		H_MAIN = self.H_MAIN
		S_BOT_ROW = self.S_BOT_ROW

		# Graphic
		screen.addImprovementGraphicGFC("Preview|Min", iTheImprove, self.X_GRAPHIC, Y_TOP_ROW_1 + 8, H_TOP_ROW_1, H_TOP_ROW_1, eWidGen, iTheImprove, 0, -20, 30, 0.7, True)
		self.main.aWidgetBucket.append("Preview|Min")
		# Main Panel
		screen.setText(aName(), "", szfontEdge + INFO.getDescription("IMPROVEMENT_", iTheImprove), 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, eWidGen, 0, 0)
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW_1 + 2, W_COL_1 + 8, H_TOP_ROW_1 + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "ToolTip|IMP" + str(iTheImprove)
		self.main.aWidgetBucket.append(Img)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("IMPROVEMENT_", iTheImprove), 4, 6, S_ICON, S_ICON, eWidGen, 1, 1)
		# Stats
		szText = ""
		szYield = ""
		aFlatYields = INFO.getFlatYields("IMPROVEMENT_", iTheImprove)
		for k in range(YieldTypes.NUM_YIELD_TYPES):
			szChar = u'%c' % (TEXT.getSymbolChar("YIELD_", k))
			iYieldChange = aFlatYields[k]
			if iYieldChange:
				if iYieldChange < 0:
					szYield += " <color=255,0,0,255>"
				else:
					szYield += " <color=0,230,0,255>+"
				szYield += str(iYieldChange) + szChar + "</color>"
		if szYield:
			szText += szYield + "\n"
		if szText:
			listBox = aName()
			screen.addListBoxGFC(listBox, "", X_MAIN, Y_MAIN, W_STATS, H_MAIN, eTblEmpty)
			screen.enableSelect(listBox, False)
			if szText:
				screen.appendListBoxString(listBox, szfont3b + szText, eWidGen, 0, 0, 1<<0)
		# Builds
		PF = "ToolTip|JumpTo|"
		aList0 = []
		for i in INFO.getImprovementBuilds(iTheImprove):
			aList0.append((i, INFO.getButton("BUILD_", i)))

		if aList0:
			szChild = PF + "BUILD"
			W_REQUIRES = W_COL_1 - W_STATS - S_ICON
			Pnl = aName()
			y = Y_MAIN + (H_MAIN - H_BOT_ROW)/2
			screen.addPanel(Pnl, "", "", False, True, X_MAIN + W_STATS, y, W_REQUIRES, H_BOT_ROW, ePnlBlue50)
			szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_BUILD", ())
			screen.setLabelAt(aName(), Pnl, szText, 1<<0, 16, 2, 0, eFontTitle, eWidGen, 0, 0)
			Pnl = aName()
			screen.addScrollPanel(Pnl, "", X_MAIN + W_STATS - 2, y + 24, W_REQUIRES + 4, H_BOT_ROW - 50, ePnlBlue50)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			x = 4
			for iType, BTN in aList0:
				screen.setImageButtonAt(szChild + str(iType), Pnl, BTN, x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
				x += S_BOT_ROW + 4
			aList0 = []

		# Synergy
		szAnd = "<font=4b>&#38"
		aValidList = []
		aList1 = self.gatherConditionedYields(iTheImprove, EdgeBucket.EDGEB_TECHS)
		aList2 = self.gatherConditionedYields(iTheImprove, EdgeBucket.EDGEB_CIVICS)
		aList3 = self.gatherConditionedYields(iTheImprove, EdgeBucket.EDGEB_BONUSES)
		szChild = PF + "BONUS"
		n = 0
		for iType in range(GC.getNumBonusInfos()):
			if INFO.isImprovementValidOnBonus(iTheImprove, iType):
				aValidList.append([szChild + str(iType) + "|" + str(n), INFO.getButton("BONUS_", iType)])
				n += 1
		if aList1 or aList2 or aList3:
			panelName = aName()
			screen.addPanel(panelName, TRNSLTR.getText("TXT_KEY_PEDIA_SYNERGY", ()), "", False, True, X_COL_1, Y_TOP_ROW_2, W_PEDIA_PAGE, H_TOP_ROW_2, ePnlBlue50)
			bAnd = False
			if aList1:
				bAnd = True
				for i, szYield in aList1:
					childPanelName = aName()
					screen.attachPanel(panelName, childPanelName, "", "", True, True, ePnlEmpty)
					screen.attachImageButton(childPanelName, "", INFO.getButton("TECH_", i), enumGBS, eWidJuToTech, i, 2, False)
					screen.attachLabel(childPanelName, "", szfont3b + " " + szYield)
					screen.attachLabel(panelName, "", "   ")
			if aList2:
				if bAnd:
					screen.attachLabel(panelName, "", szAnd + "  ")
				else:
					bAnd = True
				for i, szYield in aList2:
					childPanelName = aName()
					screen.attachPanel(panelName, childPanelName, "", "", True, True, ePnlEmpty)
					screen.attachImageButton(childPanelName, "", INFO.getButton("CIVIC_", i), enumGBS, eWidJuToCivic, i, 2, False)
					screen.attachLabel(childPanelName, "", szfont3b + " " + szYield)
					screen.attachLabel(panelName, "", "   ")
			if aList3:
				if bAnd:
					screen.attachLabel(panelName, "", szAnd + "  ")
				else:
					bAnd = True
				for i, szYield in aList3:
					childPanelName = aName()
					screen.attachPanel(panelName, childPanelName, "", "", True, True, ePnlEmpty)
					screen.attachImageButton(childPanelName, "", INFO.getButton("BONUS_", i), enumGBS, eWidJuToBonus, i, 2, False)
					screen.attachLabel(childPanelName, "", szfont3b + " " + szYield)
					screen.attachLabel(panelName, "", "   ")
		else:
			Y_MID_ROW -= H_TOP_ROW_2
			H_MID_ROW += H_TOP_ROW_2
		# Requires
		nTerrains = GC.getNumTerrainInfos()
		for iType in INFO.getRequiresIds("IMPROVEMENT_", iTheImprove, EdgeBucket.EDGEB_TECHS):
			aList0.append([PF + "TECH" + str(iType), INFO.getButton("TECH_", iType)])

		# bNotOnAnyBonus is not exposed to python.

		if INFO.isImprovementWaterOnly(iTheImprove):
			aList0.append([PF + "CONCEPT_NEW" + str(GC.getInfoTypeForString("CONCEPT_WATER_TERRAINS")), "Art/Interface/Buttons/BaseTerrain/Ocean.dds"])

		if INFO.isImprovementPeakOnly(iTheImprove):
			aList0.append([PF + "TERRAIN" + str(GC.getInfoTypeForString("TERRAIN_PEAK")) + "|" + str(n), "Art/Interface/Buttons/BaseTerrain/Peak.dds"])
			n += 1
		elif not INFO.isImprovementValidOnPeak(iTheImprove):
			aList0.append([PF + "TERRAIN" + str(GC.getInfoTypeForString("TERRAIN_PEAK")) + "|" + str(n), "Art/Interface/Buttons/BaseTerrain/Peak.dds", "NOT"])
			n += 1

		if INFO.isImprovementFlatlandsOnly(iTheImprove):
			aList0.append(["ToolTip|TxtTT|TXT_KEY_IMPROVEMENTHELP_ONLY_BUILD_FLATLANDS1", "Art/Interface/Buttons/BaseTerrain/Grassland.dds"])

		if INFO.isImprovementRiverSideOnly(iTheImprove):
			aList0.append(["ToolTip|TxtTT|TXT_KEY_IMPROVEMENTHELP_REQUIRES_RIVER1", "Art/Interface/Buttons/WorldBuilder/River_Placement.dds"])

		if INFO.isImprovementRequiresFeature(iTheImprove):
			aList0.append(["ToolTip|TxtTT|Any_plot_feature1", "Art/bug/questionmark.dds"])

		if INFO.isImprovementRequiresIrrigation(iTheImprove):
			aList0.append(["ToolTip|TxtTT|TXT_KEY_IMPROVEMENTHELP_REQUIRES_IRRIGATION1", "Art/Interface/Buttons/Buildings/Irrigation.dds"])

		if INFO.isImprovementNoFreshWater(iTheImprove):
			aList0.append(["ToolTip|TxtTT|TXT_KEY_IMPROVEMENTHELP_NO_BUILD_FRESH_WATER1", "Art/bug/questionmark.dds"])

		szChild = PF + "TERRAIN"
		for i in range(nTerrains):
			if INFO.isImprovementValidOnTerrain(iTheImprove, i):
				aValidList.append((szChild + str(i) + "|" + str(n), INFO.getButton("TERRAIN_", i)))
				n += 1
		if INFO.isImprovementValidOnHills(iTheImprove):
			aValidList.append((szChild + str(GC.getInfoTypeForString("TERRAIN_HILL")) + "|" + str(n), "Art/Interface/Buttons/BaseTerrain/Hill.dds"))
			n += 1

		szChild = PF + "FEATURE"
		for i in range(GC.getNumFeatureInfos()):
			if INFO.isImprovementValidOnFeature(iTheImprove, i):
				aValidList.append((szChild + str(i) + "|" + str(n), INFO.getButton("FEATURE_", i)))
				n += 1

		if aList0 or aValidList:
			szNot = "<font=4b>{!="
			szOr = "<font=4b>||"
			szBracketL = "<font=4b>{"
			szBracketR = "<font=4b>}"
			H_SCROLL = H_BOT_ROW - 50
			Pnl = aName()
			screen.addPanel(Pnl, "", "", False, True, X_COL_1, Y_BOT_ROW_1, W_PEDIA_PAGE, H_BOT_ROW, ePnlBlue50)
			szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_REQUIRES", ())
			screen.setLabelAt(aName(), Pnl, szText, 1<<2, W_PEDIA_PAGE / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
			Pnl = aName()
			screen.addScrollPanel(Pnl, "", X_COL_1 - 2, Y_BOT_ROW_1 + 24, W_PEDIA_PAGE + 4, H_SCROLL, ePnlBlue50)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			screen.hide(Pnl)
			x = 4
			y = H_SCROLL / 2 - 12
			bValidBracket = False
			for entry in aList0:
				if entry[-1] == "NOT":
					if x != 4:
						x += 8
					screen.setLabelAt(aName(), Pnl, szNot, 1<<0, x, y, 0, eFontTitle, eWidGen, 0, 0)
					x += 44
				screen.setImageButtonAt(entry[0], Pnl, entry[1], x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
				x += S_BOT_ROW + 4
				if entry[-1] == "NOT":
					x += 6
					screen.setLabelAt(aName(), Pnl, szBracketR, 1<<0, x, y, 0, eFontTitle, eWidGen, 0, 0)
					x += 14
			if aList0 and len(aValidList):
				bValidBracket = True
				x += 8
				screen.setLabelAt(aName(), Pnl, szBracketL, 1<<0, x, y, 0, eFontTitle, eWidGen, 0, 0)
				x += 16
			i = 0
			for NAME, BTN in aValidList:
				if i == 0:
					i+= 1
				else:
					x += 6
					screen.setLabelAt(aName(), Pnl, szOr, 1<<2, x, y, 0, eFontTitle, eWidGen, 0, 0)
					x += 10
				screen.setImageButtonAt(NAME, Pnl, BTN, x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
				x += S_BOT_ROW + 4
			if bValidBracket:
				screen.setLabelAt(aName(), Pnl, szBracketR, 1<<0, x + 8, y, 0, eFontTitle, eWidGen, 0, 0)
			screen.show(Pnl)

		else:
			H_MID_ROW += H_BOT_ROW
		# Effects and History
		szEffects = CyGameTextMgr().getImprovementHelp(iTheImprove, False)
		i = szEffects.find("\n") + 1
		if not i:
			szEffects = ""
		szHistory = INFO.getCivilopedia("IMPROVEMENT_", iTheImprove)
		if szEffects or szHistory:
			if szEffects and szHistory:
				W_MID_COL_1 = self.W_3RD_PP
				W_MID_COL_2 = W_PEDIA_PAGE - W_MID_COL_1 - 8
				X_COL_2 = X_COL_1 + W_MID_COL_1 + 8
			elif szEffects:
				W_MID_COL_1 = W_PEDIA_PAGE
			elif szHistory:
				X_COL_2 = X_COL_1
				W_MID_COL_2 = W_PEDIA_PAGE
			if szEffects:
				szHeader = TRNSLTR.getText("TXT_KEY_PEDIA_EFFECTS", ())
				screen.addPanel(aName(), szHeader, "", True, False, X_COL_1, Y_MID_ROW, W_MID_COL_1, H_MID_ROW, ePnlBlue50)
				screen.addMultilineText(aName(), szfont3 + szEffects[i:], X_COL_1 + 4, Y_MID_ROW + 32, W_MID_COL_1 - 8, H_MID_ROW - 40, eWidGen, 0, 0, 1<<0)
			if szHistory:
				szHeader = TRNSLTR.getText("TXT_KEY_PEDIA_HISTORY", ())
				screen.addPanel(aName(), szHeader, "", True, True, X_COL_2, Y_MID_ROW, W_MID_COL_2, H_MID_ROW, ePnlBlue50)
				screen.addMultilineText(aName(), szfont2 + szHistory, X_COL_2 + 4, Y_MID_ROW + 32, W_MID_COL_2 - 8, H_MID_ROW - 40, eWidGen, 0, 0, 1<<0)
