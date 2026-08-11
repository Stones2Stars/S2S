# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
INFO = CyInfo()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class PediaBonus:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.Y_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - H_BOT_ROW

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4
		self.H_BOT_ROW = H_BOT_ROW

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE
		self.W_HALF_PP = W_HALF_PP = W_PEDIA_PAGE / 2 - 4
		self.W_3RD_PP = W_PEDIA_PAGE / 3 - 4

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + W_HALF_PP + 8

		self.X_GRAPHIC = X_COL_1 + W_PEDIA_PAGE - H_TOP_ROW

		self.W_COL_1 = W_COL_1 = W_PEDIA_PAGE - H_TOP_ROW - 16

		self.S_ICON = S_ICON = H_TOP_ROW - 6

		self.X_STATS = X_COL_1 + S_ICON
		self.Y_STATS = Y_TOP_ROW + H_TOP_ROW / 12
		self.W_STATS = (W_COL_1 - S_ICON) / 2

		self.Y_REQOBS = Y_TOP_ROW + H_TOP_ROW / 6 - 8

	#	The per-improvement yield this resource is worth.
	#	⚑ It is NOT a keyed value on the improvement -- an improvement's bonus yield is a CONDITIONED entry
	#	gated on the resource ([DEC-conditions-are-predicates]: a condition is a predicate, never a member), so
	#	the magnitude and its gate live on the entry together and only the entry read answers it.
	def yieldFromImprovement(self, iImprovement, iTheBonus):
		aOut = []
		for iYield, eFamily in (
			(YieldTypes.YIELD_FOOD,       ModifierFamily.MODFAM_FOOD),
			(YieldTypes.YIELD_PRODUCTION, ModifierFamily.MODFAM_PRODUCTION),
			(YieldTypes.YIELD_COMMERCE,   ModifierFamily.MODFAM_COMMERCE)):
			iValue = 0
			for entry in INFO.getConditionedEntries("IMPROVEMENT_", iImprovement, eFamily, EdgeBucket.EDGEB_BONUSES):
				if iTheBonus in entry["atoms"]:
					iValue += entry["value"]
			if iValue:
				aOut.append((iYield, iValue / 100))
		return aOut

	def interfaceScreen(self, iTheBonus):
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		aName = self.main.getNextWidgetName

		eWidGen				= WidgetTypes.WIDGET_GENERAL
		eWidJuToBuilding	= WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING
		eWidJuToImprove		= WidgetTypes.WIDGET_PEDIA_JUMP_TO_IMPROVEMENT
		eWidJuToTech		= WidgetTypes.WIDGET_PEDIA_JUMP_TO_TECH
		eWidJuToUnit		= WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT
		ePanelBlue50		= PanelStyles.PANEL_STYLE_BLUE50
		ePanelEmpty			= PanelStyles.PANEL_STYLE_EMPTY

		enumGBS = self.main.enumGBS
		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		S_ICON = self.S_ICON
		H_TOP_ROW = self.H_TOP_ROW
		H_BOT_ROW = self.H_BOT_ROW
		H_MID = H_TOP_ROW * 3 - H_BOT_ROW - 16
		X_COL_1 = self.X_COL_1
		X_STATS = self.X_STATS
		Y_TOP_ROW_1 = self.Y_TOP_ROW
		Y_TOP_ROW_2 = Y_TOP_ROW_1 + H_TOP_ROW
		Y_TOP_ROW_3 = Y_TOP_ROW_2 + H_BOT_ROW + 16
		Y_BOT_ROW_1 = self.Y_BOT_ROW
		Y_BOT_ROW_2 = Y_BOT_ROW_1 - H_BOT_ROW
		Y_BOT_ROW_3 = Y_BOT_ROW_2 - H_BOT_ROW
		Y_STATS = self.Y_STATS
		W_STATS = self.W_STATS
		W_PEDIA_PAGE = self.W_PEDIA_PAGE

		bMapBonus = INFO.getIntrinsic("BONUS_", iTheBonus, IntrinsicSlot.PYINT_IS_MAP_BONUS)
		if bMapBonus:
			# Graphic
			screen.addBonusGraphicGFC("Preview|Min", iTheBonus, self.X_GRAPHIC, Y_TOP_ROW_1 + 8, H_TOP_ROW, H_TOP_ROW, eWidGen, iTheBonus, 0, -20, 30, 0.6, True)
			self.main.aWidgetBucket.append("Preview|Min")
			W_COL_1 = self.W_COL_1
		else:
			W_COL_1 = W_PEDIA_PAGE

		#	EVERY cross-link on this page is the resource's OWN reverse edge, landed at load. The page used to
		#	sweep all 5,180 buildings, all 2,073 units and all improvements, asking each one whether it named
		#	this bonus -- the own-data inversion [DEC-one-reverse-view] exists to delete.
		#
		#	The two axes answer different questions and are NOT interchangeable:
		#	  · REQUIRED_BY is the GATE axis -- who cannot be built without me.
		#	  · RELATED is the merged DISPLAY axis -- everything that names me anywhere: a condition, a keyed
		#	    deposit, a `provides`. It is a SUPERSET and cannot say WHICH, so it is filtered below rather
		#	    than trusted ([enabler.md] §2: safe for a display list with ANY semantics, never for ALL).
		aNeededByBuildings = INFO.getEdgeIds("BONUS_", iTheBonus, EdgeFamily.EDGEF_REQUIRED_BY, EdgeBucket.EDGEB_BUILDINGS)
		aNeededByUnits = INFO.getEdgeIds("BONUS_", iTheBonus, EdgeFamily.EDGEF_REQUIRED_BY, EdgeBucket.EDGEB_UNITS)
		aRelatedBuildings = INFO.getEdgeIds("BONUS_", iTheBonus, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_BUILDINGS)
		aRelatedUnits = INFO.getEdgeIds("BONUS_", iTheBonus, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_UNITS)

		#	SOURCE OF -- a building that supplies the resource in its city. The exact relation is the building's
		#	own `provides` block, so the merged candidate set is filtered down by it rather than guessed at.
		aSourceOfBonus = []
		for iBuilding in aRelatedBuildings:
			if INFO.providesBonus("BUILDING_", iBuilding, iTheBonus):
				aSourceOfBonus.append(iBuilding)

		#	AFFECTED -- what this resource makes better, and by how much. The magnitude lives on a CONDITIONED
		#	entry gated on the resource, so it is read entry-wise and grouped by value.
		aAffectedBuildings = []
		for iBuilding in aRelatedBuildings:
			if iBuilding in aNeededByBuildings or iBuilding in aSourceOfBonus:
				continue
			aAffectedBuildings.append(iBuilding)
		aAffectedUnits = []
		for iUnit in aRelatedUnits:
			if iUnit in aNeededByUnits:
				continue
			iModifier = 0
			for entry in INFO.getConditionedEntries("UNIT_", iUnit, ModifierFamily.MODFAM_BUILD_RATE, EdgeBucket.EDGEB_BONUSES):
				if iTheBonus in entry["atoms"]:
					iModifier += entry["value"]
			if iModifier:
				aAffectedUnits.append((iModifier, iUnit))

		# Main Panel
		szBonusChar = u'%c' % TEXT.getSymbolChar("BONUS_", iTheBonus)
		szBonusName = szBonusChar + " " + INFO.getDescription("BONUS_", iTheBonus) + " " + szBonusChar
		screen.setText(aName(), "", szfontEdge + szBonusName, 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, eWidGen, 0, 0)
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW_1 + 2, W_COL_1 + 8, H_TOP_ROW + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "ToolTip|BONUS" + str(iTheBonus)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("BONUS_", iTheBonus), 2, 2, S_ICON, S_ICON, eWidGen, 1, 1)
		# Stats
		iMinLatitude = INFO.getIntrinsic("BONUS_", iTheBonus, IntrinsicSlot.PYINT_MIN_LATITUDE)
		iMaxLatitude = INFO.getIntrinsic("BONUS_", iTheBonus, IntrinsicSlot.PYINT_MAX_LATITUDE)
		szTxt = ""
		if iMinLatitude or iMaxLatitude < 90:
			if not iMinLatitude:
				szTxt = szfont4b + "<color=200,240,120,255>" + TRNSLTR.getText("TXT_KEY_PEDIA_LATITUDE", ()) + " 0&#176  &#187  &#177 " + str(iMaxLatitude) + "&#176"
			else:
				szTxt = szfont4b + "<color=200,240,120,255>" + TRNSLTR.getText("TXT_KEY_PEDIA_LATITUDE", ()) + " &#177 " + str(iMinLatitude) + "&#176  &#187  &#177 " + str(iMaxLatitude) + "&#176"

		#	The resource's OWN tile output is a PLOT-scope flat -- what it adds to the tile it sits on.
		#	x100 like every amount, so it reduces here ([DEC-fixedpoint-x100]).
		szChange = ""
		aYields = INFO.getFlatYields("BONUS_", iTheBonus, CascScope.CASC_SCOPE_PLOT)
		for k in xrange(YieldTypes.NUM_YIELD_TYPES):
			iYieldChange = aYields[k] / 100
			if iYieldChange:
				if iYieldChange < 0:
					szChange += " <color=255,0,0,255>"
				else:
					szChange += " <color=0,230,0,255>"
				szChange += "%d%s" % (iYieldChange, unichr(8483 + k))

		#	The connected-resource benefit. It is authored as the `cities` FAN ([modifier.md] §2b -- a bare
		#	empire flat would roll down to every city instead of landing in the holding one), and the four
		#	channels are separate answers: a negative deposit is routed to the opposing channel at fill, so
		#	there is no sign to branch on.
		aWellbeing = INFO.getWellbeing("BONUS_", iTheBonus, CascScope.CASC_SCOPE_EMPIRE)
		for iChannel, iGlyph, szColour in (
			(WellbeingChannel.WELLBEING_HAPPINESS, 8850, "0,230,0,255"),
			(WellbeingChannel.WELLBEING_ANGER,     8851, "255,0,0,255"),
			(WellbeingChannel.WELLBEING_HEALTH,    8852, "0,230,0,255"),
			(WellbeingChannel.WELLBEING_UNHEALTH,  8853, "255,0,0,255")):
			iValue = aWellbeing[iChannel] / 100
			if iValue:
				szChange += " <color=%s>%d%s" % (szColour, iValue, unichr(iGlyph))

		if szChange:
			szTxt += '\n' + szfont3b + szChange
		if szTxt:
			panelName = aName()
			screen.addListBoxGFC(panelName, "", X_STATS, Y_STATS, W_STATS, H_TOP_ROW - 12, TableStyles.TABLE_STYLE_EMPTY)
			screen.enableSelect(panelName, False)
			screen.appendListBoxString(panelName, szTxt, eWidGen, 0, 0, 1<<0)
		# Reveals, enables, and obsoletes.
		iRevealTech = INFO.getIntrinsic("BONUS_", iTheBonus, IntrinsicSlot.PYINT_TECH_REVEAL)
		iEnableTech = INFO.getIntrinsic("BONUS_", iTheBonus, IntrinsicSlot.PYINT_TECH_CITY_TRADE)
		iObsoleteTech = INFO.getIntrinsic("BONUS_", iTheBonus, IntrinsicSlot.PYINT_TECH_OBSOLETE)
		if iRevealTech != -1 or iEnableTech != -1 or iObsoleteTech != -1:
			enumBS = GenericButtonSizes.BUTTON_SIZE_CUSTOM
			panelName = aName()
			screen.addPanel(panelName, "", "", False, True, X_STATS + W_STATS, self.Y_REQOBS, W_STATS, H_TOP_ROW - 16, ePanelEmpty)
			if iRevealTech == iEnableTech and iRevealTech != -1:
				childPanelName = aName()
				screen.attachPanel(panelName, childPanelName, "", "", True, True, ePanelEmpty)
				screen.attachLabel(childPanelName, "", szfont4b + TRNSLTR.getText("TXT_KEY_PEDIA_BONUS_APPEARANCE_AND_TRADE", ()))
				screen.attachImageButton(childPanelName, "", INFO.getButton("TECH_", iRevealTech), enumBS, eWidJuToTech, iRevealTech, 2, False)
			else:
				if iRevealTech != -1:
					childPanelName = aName()
					screen.attachPanel(panelName, childPanelName, "", "", True, True, ePanelEmpty)
					screen.attachLabel(childPanelName, "", szfont4b + TRNSLTR.getText("TXT_KEY_PEDIA_BONUS_APPEARANCE", ()))
					screen.attachImageButton(childPanelName, "", INFO.getButton("TECH_", iRevealTech), enumBS, eWidJuToTech, iRevealTech, 2, False)
				if iEnableTech != -1:
					childPanelName = aName()
					screen.attachPanel(panelName, childPanelName, "", "", True, True, ePanelEmpty)
					screen.attachLabel(childPanelName, "", szfont4b + TRNSLTR.getText("TXT_KEY_PEDIA_BONUS_TRADE", ()))
					screen.attachImageButton(childPanelName, "", INFO.getButton("TECH_", iEnableTech), enumBS, eWidJuToTech, iEnableTech, 2, False)
			if iObsoleteTech != -1:
				childPanelName = aName()
				screen.attachPanel(panelName, childPanelName, "", "", True, True, ePanelEmpty)
				screen.attachLabel(childPanelName, "", szfont4b + TRNSLTR.getText("TXT_KEY_PEDIA_BONUS_OBSOLETE", ()))
				screen.attachImageButton(childPanelName, "", INFO.getButton("TECH_", iObsoleteTech), enumBS, eWidJuToTech, iObsoleteTech, 2, False)
		# Improvement -- the resource's OWN list of what makes it tradeable, plus that improvement's yield here.
		aImpList = []
		if bMapBonus:
			for iImprovement in INFO.getIdList("BONUS_", iTheBonus, IdListSlot.PYLIST_TRADE_PROVIDING_IMPROVEMENTS):
				szYield = " " + szBonusChar
				for iYield, iValue in self.yieldFromImprovement(iImprovement, iTheBonus):
					if iValue < 0:
						szYield += " <color=255,0,0,255>"
					else:
						szYield += " <color=0,230,0,255>"
					szYield += str(iValue) + (u'%c' % (TEXT.getSymbolChar("YIELD_", iYield)))
				aImpList.append((iImprovement, szfont3b + szYield))
		if aSourceOfBonus or aImpList:
			if aSourceOfBonus and aImpList:
				W_IMP = W_SoB = self.W_HALF_PP
				X_IMP = self.X_COL_2
				W_3RD_PP = self.W_3RD_PP
				if len(aImpList) < 4:
					if len(aSourceOfBonus) > 4:
						W_IMP = W_3RD_PP
						W_SoB = W_PEDIA_PAGE - W_3RD_PP - 4
						X_IMP = X_COL_1 + W_SoB + 8
				elif len(aSourceOfBonus) < 4:
					if len(aImpList) > 4:
						W_IMP = W_PEDIA_PAGE - W_3RD_PP - 4
						W_SoB = W_3RD_PP
						X_IMP = X_COL_1 + W_SoB + 8
				sobPanel = aName()
				impPanel = aName()
				screen.addPanel(sobPanel, TRNSLTR.getText("TXT_KEY_PEDIA_RESOURCE_SOURCE", ()), "", False, True, X_COL_1, Y_TOP_ROW_2, W_SoB, H_BOT_ROW + 16, ePanelBlue50)
				screen.addPanel(impPanel, TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_IMPROVEMENT", ()), "", False, True, X_IMP, Y_TOP_ROW_2, W_IMP, H_BOT_ROW + 16, ePanelBlue50)
			elif aSourceOfBonus:
				sobPanel = aName()
				szSource = TRNSLTR.getText("TXT_KEY_PEDIA_RESOURCE_SOURCE", ())
				screen.addPanel(sobPanel, szSource, "", False, True, X_COL_1, Y_TOP_ROW_2, W_PEDIA_PAGE, H_BOT_ROW + 16, ePanelBlue50)
			else:
				impPanel = aName()
				szImp = TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_IMPROVEMENT", ())
				screen.addPanel(impPanel, szImp, "", False, True, X_COL_1, Y_TOP_ROW_2, W_PEDIA_PAGE, H_BOT_ROW + 16, ePanelBlue50)
			if aSourceOfBonus:
				for iBuilding in aSourceOfBonus:
					screen.attachImageButton(sobPanel, "", INFO.getButton("BUILDING_", iBuilding), enumGBS, eWidJuToBuilding, iBuilding, 1, False)
			if aImpList:
				for i in xrange(len(aImpList)):
					childPanelName = aName()
					screen.attachPanel(impPanel, childPanelName, "", "", True, True, ePanelEmpty)
					screen.attachImageButton(childPanelName, "", INFO.getButton("IMPROVEMENT_", aImpList[i][0]), enumGBS, eWidJuToImprove, aImpList[i][0], 1, False)
					screen.attachLabel(childPanelName, "", aImpList[i][1])
		else:
			Y_TOP_ROW_3 -= H_BOT_ROW + 16
			H_MID += H_BOT_ROW + 16
		# Buildings Enabled
		#	⚠ The legacy page split this into "needs it nationwide" and "needs it locally". That distinction is
		#	the `connection` / `vicinity` discriminator on the requires ATOM, which no edge family preserves
		#	([enabler.md] §2: RELATED and REQUIRED_BY are merged buckets), so the two are shown as one list.
		#	A stated display change, not a silent one -- the composer still renders the exact clause in prose.
		if aNeededByBuildings:
			panelName = aName()
			szBuildingsEnabled = TRNSLTR.getText("TXT_KEY_PEDIA_BUILDINGS_ENABLED", ())
			screen.addPanel(panelName, szBuildingsEnabled, "", False, True, X_COL_1, Y_BOT_ROW_1, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
			for iBuilding in aNeededByBuildings:
				screen.attachImageButton(panelName, "", INFO.getButton("BUILDING_", iBuilding), enumGBS, eWidJuToBuilding, iBuilding, 1, False)
		else:
			Y_BOT_ROW_2 += H_BOT_ROW
			Y_BOT_ROW_3 += H_BOT_ROW
			H_MID += H_BOT_ROW
		# Units Enabled
		if aNeededByUnits:
			panelName = aName()
			screen.addPanel(panelName, TRNSLTR.getText("TXT_KEY_PEDIA_UNITS_ENABLED", ()), "", False, True, X_COL_1, Y_BOT_ROW_2, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
			for iUnit in aNeededByUnits:
				screen.attachImageButton(panelName, "", INFO.getButton("UNIT_", iUnit), enumGBS, eWidJuToUnit, iUnit, 1, False)
		else:
			H_MID += H_BOT_ROW
			Y_BOT_ROW_3 += H_BOT_ROW
		# Units & Buildings Affected
		if aAffectedBuildings or aAffectedUnits:
			if aAffectedBuildings and aAffectedUnits:
				W_UNIT = W_BUIL = self.W_HALF_PP
				X_UNIT = self.X_COL_2
				W_3RD_PP = self.W_3RD_PP
				if len(aAffectedUnits) < 4:
					if len(aAffectedBuildings) > 4:
						W_UNIT = W_3RD_PP
						W_BUIL = W_PEDIA_PAGE - W_3RD_PP - 4
						X_UNIT = X_COL_1 + W_BUIL + 8
				elif len(aAffectedBuildings) < 4:
					if len(aAffectedUnits) > 4:
						W_UNIT = W_PEDIA_PAGE - W_3RD_PP - 4
						W_BUIL = W_3RD_PP
						X_UNIT = X_COL_1 + W_BUIL + 8
				builPanel = aName()
				unitPanel = aName()
				screen.addPanel(builPanel, TRNSLTR.getText("TXT_KEY_BONUSHELP_EFFECTS_BUILDING", ()), "", False, True, X_COL_1, Y_BOT_ROW_3, W_BUIL, H_BOT_ROW, ePanelBlue50)
				screen.addPanel(unitPanel, TRNSLTR.getText("TXT_KEY_BONUSHELP_EFFECTS_UNIT", ()), "", False, True, X_UNIT, Y_BOT_ROW_3, W_UNIT, H_BOT_ROW, ePanelBlue50)
			elif aAffectedBuildings:
				builPanel = aName()
				szBuild = TRNSLTR.getText("TXT_KEY_BONUSHELP_EFFECTS_BUILDING", ())
				screen.addPanel(builPanel, szBuild, "", False, True, X_COL_1, Y_BOT_ROW_3, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
			else:
				unitPanel = aName()
				szUnit = TRNSLTR.getText("TXT_KEY_BONUSHELP_EFFECTS_UNIT", ())
				screen.addPanel(unitPanel, szUnit, "", False, True, X_COL_1, Y_BOT_ROW_3, W_PEDIA_PAGE, H_BOT_ROW, ePanelBlue50)
			if aAffectedBuildings:
				for iBuilding in aAffectedBuildings:
					screen.attachImageButton(builPanel, "", INFO.getButton("BUILDING_", iBuilding), enumGBS, eWidJuToBuilding, iBuilding, 1, False)
			if aAffectedUnits:
				#	Grouped by the modifier VALUE, so one "{ +25% }" label heads every unit that shares it.
				#	A percent is never scaled, so it prints as authored.
				szBracketL = szfont4b + " {"
				szBracketR = szfont4b + "} "
				szChar = u'%c' % (TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION))
				aAffectedUnits.sort()
				test = 0
				for entry in aAffectedUnits:
					iModifier, iUnit = entry
					if iModifier != test:
						szTxt = szBracketL
						if test != 0:
							screen.attachLabel(unitPanel, "", szBracketR)
						if iModifier < 0:
							szTxt += " <color=255,0,0,255>"
						else:
							szTxt += " <color=0,230,0,255>"
						szTxt += str(iModifier) + "%" + szChar
						screen.attachLabel(unitPanel, "", szTxt)
						test = iModifier
					screen.attachImageButton(unitPanel, "", INFO.getButton("UNIT_", iUnit), enumGBS, eWidJuToUnit, iUnit, 1, False)
				screen.attachLabel(unitPanel, "", szBracketR)
		else:
			H_MID += H_BOT_ROW
		# History
		szTxt = INFO.getCivilopedia("BONUS_", iTheBonus)
		if szTxt:
			screen.addPanel(aName(), "", "", True, True, X_COL_1, Y_TOP_ROW_3, W_PEDIA_PAGE, H_MID, ePanelBlue50)
			szTxt = szfont3b + TRNSLTR.getText("TXT_KEY_CIVILOPEDIA_BACKGROUND", ()) + szfont3 + szTxt
			screen.addMultilineText(aName(), szTxt, X_COL_1 + 4, Y_TOP_ROW_3 + 8, W_PEDIA_PAGE - 8, H_MID - 16, eWidGen, 1, 2, 1<<0)
