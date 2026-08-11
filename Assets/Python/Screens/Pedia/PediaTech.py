# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
INFO = CyInfo()
STATE = CyState()
ENUMS = CyEnums()
TRNSLTR = CyTranslator()
TEXT = CyGameTextMgr()

class PediaTech:

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

		W_BASE = 64 + H_TOP_ROW + W_PEDIA_PAGE / 8
		W_HALF_PP = W_PEDIA_PAGE / 2

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + W_BASE + 4
		self.X_COL_3 = X_COL_1 + W_HALF_PP + 4

		self.W_COL_1 = W_BASE - 4
		self.W_COL_2 = W_PEDIA_PAGE - W_BASE - 4
		self.W_COL_3 = W_HALF_PP - 4

		self.S_ICON = S_ICON = H_TOP_ROW - 10

		self.X_STATS = X_COL_1 + S_ICON - 8
		self.Y_STATS = Y_TOP_ROW + H_TOP_ROW / 6
		self.W_STATS = W_BASE - S_ICON - 16


	def interfaceScreen(self, iTheTech):
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		aName = self.main.getNextWidgetName

		eWidGen				= WidgetTypes.WIDGET_GENERAL
		eWidJuToBuilding	= WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING
		eWidJuToDerTech		= WidgetTypes.WIDGET_PEDIA_JUMP_TO_DERIVED_TECH
		eWidJuToProject		= WidgetTypes.WIDGET_PEDIA_JUMP_TO_PROJECT
		eWidJuToUnit		= WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT
		ePnlBlue50	= PanelStyles.PANEL_STYLE_BLUE50
		eFontTitle	= FontTypes.TITLE_FONT
		enumGBS = self.main.enumGBS

		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		W_COL_1 = self.W_COL_1
		W_COL_2 = self.W_COL_2
		W_COL_3 = self.W_COL_3
		W_PEDIA_PAGE = self.W_PEDIA_PAGE
		H_BOT_ROW = self.H_BOT_ROW
		H_TOP_ROW = self.H_TOP_ROW
		H_HISTORY = H_TOP_ROW * 2
		H_SPECIAL = H_TOP_ROW * 3
		S_ICON = self.S_ICON
		X_COL_1 = self.X_COL_1
		X_COL_2 = self.X_COL_2
		Y_TOP_ROW_1 = self.Y_TOP_ROW
		Y_TOP_ROW_2 = Y_TOP_ROW_1 + H_TOP_ROW
		Y_STRATEGY	= Y_TOP_ROW_2 + H_HISTORY
		Y_BOT_ROW_1 = self.Y_BOT_ROW
		Y_BOT_ROW_2 = Y_BOT_ROW_1 - H_BOT_ROW
		Y_BOT_ROW_3 = Y_BOT_ROW_2 - H_BOT_ROW
		H_SCROLL = H_BOT_ROW - 50
		S_BOT_ROW = self.S_BOT_ROW

		# Main Panel
		screen.setText(aName(), "", szfontEdge + INFO.getDescription("TECH_", iTheTech), 1<<0, X_COL_1, 0, 0, eFontTitle, eWidGen, 0, 0)
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW_1 + 2, W_COL_1 + 8, H_TOP_ROW + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "Preview|Quote|TECH|ToolTip" + str(iTheTech)
		self.main.aWidgetBucket.append(Img)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("TECH_", iTheTech), 4, 6, S_ICON, S_ICON, eWidGen, 1, 1)
		# Stats
		Pnl = aName()
		screen.addListBoxGFC(Pnl, "", self.X_STATS, self.Y_STATS, self.W_STATS, H_TOP_ROW - 12, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(Pnl, False)

		iEra = INFO.getIntrinsic("TECH_", iTheTech, IntrinsicSlot.PYINT_ERA)

		# The COST is two different questions, and the page asks whichever it can answer.
		# In a running game what matters is what THIS TEAM pays -- computed game state, scaled by gamespeed,
		# era, handicap and team size -- so it is asked of the STATE plane. Out of game there is no team, and
		# the authored base cost on the info is the honest answer.
		iCost = -1
		iPlayer = STATE.getActivePlayer()
		if iPlayer >= 0:
			iTeam = STATE.getPlayerTeam(iPlayer)
			if iTeam >= 0:
				iCost = STATE.getTechResearchCost(iTeam, iTheTech)
		if iCost < 0:
			iCost = INFO.getIntrinsic("TECH_", iTheTech, IntrinsicSlot.PYINT_COST)
		szCostText = TRNSLTR.getText("%d1_Num", (iCost,)) + u"%c" % (TEXT.getSymbolChar("COMMERCE_", CommerceTypes.COMMERCE_RESEARCH))

		screen.appendListBoxStringNoUpdate(Pnl, szfont4b + INFO.getDescription("C2C_ERA_", iEra), eWidGen, 0, 0, 1<<2)
		screen.appendListBoxStringNoUpdate(Pnl, szfont4b + szCostText, eWidGen, 0, 0, 1<<2)

		# Wellbeing is FOUR channels, read as the group. A source depositing a negative value is routed to the
		# opposing channel at fill, so there is no sign to branch on here -- happiness and anger are separate
		# answers, as are health and unhealth. Flats are x100, so each reduces at this point of use.
		aWellbeing = INFO.getWellbeing("TECH_", iTheTech, CascScope.CASC_SCOPE_EMPIRE)
		szText = ""
		for iChannel, iGlyph, szColour in (
			(WellbeingChannel.WELLBEING_HAPPINESS, 8850, "0,230,0,255"),
			(WellbeingChannel.WELLBEING_ANGER,     8851, "255,0,0,255"),
			(WellbeingChannel.WELLBEING_HEALTH,    8852, "0,230,0,255"),
			(WellbeingChannel.WELLBEING_UNHEALTH,  8853, "255,0,0,255")):
			iValue = aWellbeing[iChannel] / 100
			if iValue:
				if szText:
					szText += " "
				szText += "<color=%s>%d%s" % (szColour, iValue, unichr(iGlyph))

		# The route COUNT is a flat amount like any other, so it is x100 too.
		iTradeRoutes = INFO.getIntrinsic("TECH_", iTheTech, IntrinsicSlot.PYINT_TRADE_ROUTE_AMOUNT) / 100
		if iTradeRoutes:
			if szText:
				szText += " "
			if iTradeRoutes < 0:
				szText += "<color=255,0,0,255>"
			else:
				szText += "<color=0,230,0,255>"
			szText += "%d" % iTradeRoutes + unichr(8860)
		if szText:
			screen.appendListBoxStringNoUpdate(Pnl, szfont3b + szText, eWidGen, 0, 0, 1<<2)

		# A PERCENT is never scaled, so this one is read as authored.
		iWorkerSpeedModifier = INFO.getScalar("TECH_", iTheTech, InfoScalar.SCALAR_WORK_RATE,
			CascScope.CASC_SCOPE_EMPIRE, CascUnit.CASC_UNIT_PERCENT)
		if iWorkerSpeedModifier:
			if iWorkerSpeedModifier < 0:
				szText = "<color=255,0,0,255>"
			else:
				szText = "<color=0,230,0,255>"
			szText += str(iWorkerSpeedModifier) + TRNSLTR.getText("TXT_KEY_PEDIA_TECH_WORKER_SPEED", ())
			screen.appendListBoxStringNoUpdate(Pnl, szfont3b + szText, eWidGen, 0, 0, 1<<2)

		screen.updateListBox(Pnl)

		# "What does this tech unlock?" is the tech's OWN forward edge -- a list it already carries, landed at
		# load. Asking it backwards (sweep every building, test whether this tech is required) is the whole-
		# database scan the edge families exist to delete, and it is how these three panels used to be built.
		aBuildings = INFO.getEdgeIds("TECH_", iTheTech, EdgeFamily.EDGEF_ENABLES, EdgeBucket.EDGEB_BUILDINGS)
		aProjects = INFO.getEdgeIds("TECH_", iTheTech, EdgeFamily.EDGEF_ENABLES, EdgeBucket.EDGEB_PROJECTS)
		aUnits = INFO.getEdgeIds("TECH_", iTheTech, EdgeFamily.EDGEF_ENABLES, EdgeBucket.EDGEB_UNITS)
		aLeadsTo = INFO.getEdgeIds("TECH_", iTheTech, EdgeFamily.EDGEF_ENABLES, EdgeBucket.EDGEB_TECHS)

		# Buildings Enabled
		if aBuildings or aProjects:
			Pnl = aName()
			szBuildingsEnabled = TRNSLTR.getText("TXT_KEY_PEDIA_BUILDINGS_ENABLED", ())
			screen.addPanel(Pnl, szBuildingsEnabled, "", False, True, X_COL_1, Y_BOT_ROW_1, W_PEDIA_PAGE, H_BOT_ROW, ePnlBlue50)
			for iBuilding in aBuildings:
				screen.attachImageButton(Pnl, "", INFO.getButton("BUILDING_", iBuilding), enumGBS, eWidJuToBuilding, iBuilding, 1, False)
			for iProject in aProjects:
				screen.attachImageButton(Pnl, "", INFO.getButton("PROJECT_", iProject), enumGBS, eWidJuToProject, iProject, 1, False)
		else:
			Y_BOT_ROW_3 += H_BOT_ROW
			Y_BOT_ROW_2 += H_BOT_ROW
			Y_STRATEGY += H_BOT_ROW
			H_HISTORY += H_BOT_ROW
			H_SPECIAL += H_BOT_ROW
		# Units Enabled
		if aUnits:
			Pnl = aName()
			screen.addPanel(Pnl, TRNSLTR.getText("TXT_KEY_PEDIA_UNITS_ENABLED", ()), "", False, True, X_COL_1, Y_BOT_ROW_2, W_PEDIA_PAGE, H_BOT_ROW, ePnlBlue50)
			for iUnit in aUnits:
				screen.attachImageButton(Pnl, "", INFO.getButton("UNIT_", iUnit), enumGBS, eWidJuToUnit, iUnit, 1, False)
		else:
			Y_BOT_ROW_3 += H_BOT_ROW
			Y_STRATEGY += H_BOT_ROW
			H_HISTORY += H_BOT_ROW
			H_SPECIAL += H_BOT_ROW

		# Requires -- the TECH prereqs only.
		# The AND and OR groups are two published lists precisely because the merged edge family cannot tell an
		# enabling tech from an obsoleting one, and a prereq strip drawing "&" between them has ALL semantics.
		# A tech's BUILDING prereqs (two techs author one, both as OR-groups) are NOT drawn here: the composer
		# renders the whole requires tree as prose in the Special panel below, minimum counts included, so an
		# icon strip beside it would be a second reading of the same clause.
		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, True, X_COL_1, Y_BOT_ROW_3, W_COL_3, H_BOT_ROW, ePnlBlue50)
		szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_REQUIRES", ())
		screen.setLabelAt(aName(), Pnl, szText, 1<<2, W_COL_3 / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
		OR = ["TXT", "<font=4b>||", 1<<2, 6, 10]
		braL = ["TXT", "<font=4b> {", 1<<0, 0, 14]
		braR = ["TXT", "<font=4b>} ", 1<<0, 0, 14]
		szChild = "ToolTip|JumpTo|TECH"
		aList1 = []
		n = 0
		for iType in INFO.getIdList("TECH_", iTheTech, IdListSlot.PYLIST_PREREQ_AND_TECHS):
			aList1.append([szChild + str(iType) + "|" + str(n), INFO.getButton("TECH_", iType)])
			n += 1
		aOrTechs = INFO.getIdList("TECH_", iTheTech, IdListSlot.PYLIST_PREREQ_OR_TECHS)
		if aOrTechs:
			if len(aOrTechs) > 1:
				aList1.append(braL)
			bFirst = True
			for iType in aOrTechs:
				if not bFirst:
					aList1.append(OR)
				bFirst = False
				aList1.append([szChild + str(iType) + "|" + str(n), INFO.getButton("TECH_", iType)])
				n += 1
			if len(aOrTechs) > 1:
				aList1.append(braR)

		if aList1:
			Pnl = aName()
			screen.addScrollPanel(Pnl, "", X_COL_1 - 2, Y_BOT_ROW_3 + 24, W_COL_3 + 4, H_SCROLL, ePnlBlue50)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			x = 4
			y = H_SCROLL / 2 - 12
			for entry in aList1:
				if entry[0] == "TXT":
					x += entry[3]
					screen.setLabelAt(aName(), Pnl, entry[1], entry[2], x, y, 0, eFontTitle, eWidGen, 0, 0)
					x += entry[4]
				else:
					screen.setImageButtonAt(entry[0], Pnl, entry[1], x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
					x += S_BOT_ROW + 4
			screen.hide(Pnl)
			screen.show(Pnl)
		# Leads To -- the same forward edge, pointed at the tech bucket.
		Pnl = aName()
		screen.addPanel(Pnl, TRNSLTR.getText("TXT_KEY_PEDIA_LEADS_TO", ()), "", False, True, self.X_COL_3, Y_BOT_ROW_3, W_COL_3, H_BOT_ROW, ePnlBlue50)
		for iTechX in aLeadsTo:
			screen.attachImageButton(Pnl, "", INFO.getButton("TECH_", iTechX), enumGBS, eWidJuToDerTech, iTechX, 1, False)

		# Quote
		szTxt = INFO.getQuote("TECH_", iTheTech)
		if szTxt:
			szQuote = TRNSLTR.getText("TXT_KEY_PEDIA_QUOTE", ())
			screen.addPanel(aName(), szQuote, "", True, False, X_COL_2, Y_TOP_ROW_1, W_COL_2, H_TOP_ROW, ePnlBlue50)
			screen.addMultilineText(aName(), szfont2 + szTxt, X_COL_2 + 4, Y_TOP_ROW_1 + 32, W_COL_2 - 8, H_TOP_ROW - 40, eWidGen, 0, 0, 1<<0)

		# Special -- the composer's own body: the entry lines per family, the unlock edges, and the requires
		# tree rendered clause by clause.
		szSpecial = TEXT.getTechHelp(iTheTech, True, False, False, False, -1)[1:]
		if iTheTech == ENUMS.getInfoType("TECH_COLONIALISM"):
			szSpecial += TRNSLTR.getText("TXT_KEY_COLONAILISM_EXTRA_POP", ())
		elif iTheTech == ENUMS.getInfoType("TECH_STEAM_POWER"):
			szSpecial += TRNSLTR.getText("TXT_KEY_STEAM_POWER_EXTRA_POP", ())
		# History
		szTxt = ""
		szTemp = INFO.getStrategy("TECH_", iTheTech)
		if szTemp:
			szTxt += szfont2b + TRNSLTR.getText("TXT_KEY_CIVILOPEDIA_STRATEGY", ()) + szfont2 + szTemp + "\n\n"
		szTemp = INFO.getCivilopedia("TECH_", iTheTech)
		if szTemp:
			szTxt += szfont2b + TRNSLTR.getText("TXT_KEY_CIVILOPEDIA_BACKGROUND", ()) + szfont2 + szTemp

		if szSpecial and szTxt:
			WIDTH = W_COL_2
		elif szTxt:
			WIDTH = W_PEDIA_PAGE
			X_COL_2 = X_COL_1
		if szSpecial:
			screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_TOP_ROW_2, W_COL_1, H_SPECIAL, ePnlBlue50)
			screen.addMultilineText(aName(), szfont3 + szSpecial, X_COL_1 + 4, Y_TOP_ROW_2 + 8, W_COL_1 - 8, H_SPECIAL - 16, eWidGen, 0, 0, 1<<0)
		if szTxt:
			screen.addPanel(aName(), "", "", True, False, X_COL_2, Y_TOP_ROW_2, WIDTH, H_SPECIAL, ePnlBlue50)
			screen.addMultilineText(aName(), szTxt, X_COL_2 + 4, Y_TOP_ROW_2 + 8, WIDTH - 8, H_SPECIAL - 16, eWidGen, 0, 0, 1<<0)
