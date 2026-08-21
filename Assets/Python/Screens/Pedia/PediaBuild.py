# Sevopedia overhauled by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()
BUILDINFO = CyBuildInfo()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class PediaBuild:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.Y_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - H_BOT_ROW

		self.H_TOP_ROW = H_TOP_ROW = (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4
		self.H_BOT_ROW = H_BOT_ROW

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE

		self.S_ICON = S_ICON = H_TOP_ROW - H_TOP_ROW % 8

		self.W_STATS = W_STATS = W_PEDIA_PAGE - S_ICON * 3

		a = H_TOP_ROW / 14
		b = H_TOP_ROW / 22
		self.X_MAIN_L = X_MAIN_L = X_COL_1 + S_ICON - 8
		self.Y_MAIN_L = Y_TOP_ROW + a
		self.H_MAIN_L = H_TOP_ROW - a
		self.Y_MAIN_R = Y_TOP_ROW + b * b / 2
		self.H_MAIN_R = H_TOP_ROW - b * b
		self.X_MAIN_R = X_MAIN_L + W_STATS

	def interfaceScreen(self, iTheBuild):
		GC = CyGlobalContext()
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		CyPlayer = self.main.CyPlayer

		iWidGen				= WidgetTypes.WIDGET_GENERAL
		iPanelBlue50		= PanelStyles.PANEL_STYLE_BLUE50
		iPanelIn			= PanelStyles.PANEL_STYLE_IN
		iPanelStd			= PanelStyles.PANEL_STYLE_STANDARD
		iPanelEmpty			= PanelStyles.PANEL_STYLE_EMPTY
		iFontGame			= FontTypes.GAME_FONT

		enumGBS = self.main.enumGBS
		uFontEdge, uFont4b, uFont4, uFont3b, uFont3, uFont2b, uFont2 = self.main.aFontList
		PF = "ToolTip|JumpTo|"

		W_PEDIA_PAGE = self.W_PEDIA_PAGE
		S_ICON = self.S_ICON
		H_TOP_ROW = self.H_TOP_ROW
		H_BOT_ROW = self.H_BOT_ROW
		X_COL_1 = self.X_COL_1
		Y_BOT_ROW_1 = self.Y_BOT_ROW
		Y_TOP_ROW_1 = self.Y_TOP_ROW
		Y_TOP_ROW_2 = Y_TOP_ROW_1 + H_TOP_ROW
		H_TOP_ROW_2 = Y_BOT_ROW_1 - Y_TOP_ROW_2
		W_STATS = self.W_STATS

		aName = self.main.getNextWidgetName

		# Main Panel
		screen.setText(aName(), "", uFontEdge + INFO.getDescription("BUILD_", iTheBuild), 1<<0, X_COL_1, 0, 0, FontTypes.TITLE_FONT, iWidGen, 0, 0)
		screen.addPanel(aName(), "", "", False, False, X_COL_1, Y_TOP_ROW_1 + 3, W_PEDIA_PAGE + 8, H_TOP_ROW, PanelStyles.PANEL_STYLE_MAIN)
		screen.addDDSGFC(aName(), INFO.getButton("BUILD_", iTheBuild), X_COL_1 - 2, Y_TOP_ROW_1 + 8, S_ICON, S_ICON, iWidGen, -1, -1)
		# Stats
		szStats = ""
		BULLET = unichr(8854)
		if BUILDINFO.isConsumesUnit(iTheBuild):
			szStats += BULLET + " " + TRNSLTR.getText("TXT_KEY_PEDIA_CONSUMES_WORKER", ()) + "\n"
		iCost = BUILDINFO.getGoldCost(iTheBuild)
		fTime = BUILDINFO.getTime(iTheBuild) / 100.0
		if iCost or fTime:
			szStats += BULLET
			if iCost:
				szStats += TRNSLTR.getText("TXT_KEY_PEDIA_COSTS", ()) + " %d%s" %(iCost, unichr(8500))
			if fTime:
				if CyPlayer:
					G = GC.getGame()
					fGameSpeedMod = G.getHammerCostPercent() / 100.0
					# The start era's build-cost percent, off the era's own COSTS group. It is a PERCENT and is
					# therefore NOT x100 ([DEC-fixedpoint-x100]: the scaling exists to carry decimals, and a
					# percent has none) -- so this is the /100 that turns 100% into the 1.0 multiplier the
					# display composes with, never a fixed-point reduce.
					fEraMod = INFO.getCostKinds("C2C_ERA_", G.getStartEra(), CascScope.CASC_SCOPE_WORLD)[CostsKind.COSTS_BUILD] / 100.0
				szTime = str(fTime) + "00"
				index = szTime.find(".")
				if szTime[index + 1] == "0" and szTime[index + 2] == "0":
					pass
				elif szTime[index + 2] == "0":
					index += 2
				else:
					index += 3
				szTime = szTime[:index]
				if iCost:
					szStats += " " + TRNSLTR.getText("TXT_KEY_PEDIA_AND_BUILD_TIME", ()) + " ("
				else:
					szStats += " " + TRNSLTR.getText("TXT_KEY_PEDIA_BUILD_TIME", ()) + " ("
				szStats += szTime + " " + TRNSLTR.getText("TXT_KEY_PEDIA_TURNS_FEATURE_BUILD_TIME", ())
				if CyPlayer:
					szStats += " * ("
					if fGameSpeedMod:
						szGameSpeedMod = str(fGameSpeedMod) + "00"
						index = szGameSpeedMod.find(".")
						if szGameSpeedMod[index + 1] == "0" and szGameSpeedMod[index + 2] == "0":
							pass
						elif szGameSpeedMod[index + 2] == "0":
							index += 2
						else:
							index += 3
						szStats += szGameSpeedMod[:index] + " " + TRNSLTR.getText("TXT_KEY_PEDIA_FROM_GAME_SPEED", ())
						if fEraMod:
							szStats += " * ("
					if fEraMod:
						szEraMod = str(fEraMod) + "00"
						index = szEraMod.find(".")
						if szEraMod[index + 1] == "0" and szEraMod[index + 2] == "0":
							pass
						elif szEraMod[index + 2] == "0":
							index += 2
						else:
							index += 3
						szStats += szEraMod[:index] + " " + TRNSLTR.getText("TXT_KEY_PEDIA_FROM_STARTING_ERA", ())
		if szStats:
			screen.addMultilineText(aName(), uFont3b + szStats, self.X_MAIN_L + 4, self.Y_MAIN_L, W_STATS - 8, self.H_MAIN_L, iWidGen, 0, 0, 1<<0)
		# Construct & Requires
		iImp = BUILDINFO.getImprovement(iTheBuild) + 1
		iRou	= BUILDINFO.getRoute(iTheBuild) + 1
		iTech = BUILDINFO.getTechPrereq(iTheBuild) + 1
		if iImp or iRou or iTech:
			enumBS = GenericButtonSizes.BUTTON_SIZE_CUSTOM
			Panel = aName()
			screen.addPanel(Panel, "", "", False, True, self.X_MAIN_R, self.Y_MAIN_R, W_PEDIA_PAGE - W_STATS - S_ICON, self.H_MAIN_R, iPanelEmpty)
			if iImp or iRou:
				Panel2 = aName()
				screen.attachPanel(Panel, Panel2, "", "", True, True, iPanelEmpty)
				screen.attachLabel(Panel2, "", uFont4b + TRNSLTR.getText("TXT_KEY_PEDIA_CONSTRUCT", ()))
				Panel3 = aName()
				screen.attachPanel(Panel2, Panel3, "", "", False, True, iPanelEmpty)
				if iImp:
					iImp -= 1
					screen.attachImageButton(Panel3, PF + "IMP" + str(iImp), INFO.getButton("IMPROVEMENT_", iImp), enumBS, iWidGen, 1, 1, False)
				if iRou:
					iRou -= 1
					screen.attachImageButton(Panel3, PF + "ROUTE" + str(iRou), INFO.getButton("ROUTE_", iRou), enumBS, iWidGen, 1, 1, False)
			if iTech:
				iTech -= 1
				Panel2 = aName()
				screen.attachPanel(Panel, Panel2, "", "", True, True, iPanelEmpty)
				screen.attachLabel(Panel2, "", uFont4b + TRNSLTR.getText("TXT_KEY_PEDIA_REQUIRES", ()))
				Panel3 = aName()
				screen.attachPanel(Panel2, Panel3, "", "", False, True, iPanelEmpty)
				screen.attachImageButton(Panel3, PF + "TECH" + str(iTech), INFO.getButton("TECH_", iTech), enumBS, iWidGen, 1, 1, False)
		# Capable
		# The units that can perform this build -- the build's own reverse edge family, landed once at load.
		# The legacy form swept every unit asking hasBuild, which is the cross-link [DEC-one-reverse-view] bans:
		# a page walking a DIFFERENT registry to find "what needs me" reads the reverse family instead.
		aList = []
		for i in INFO.getEdgeIds("BUILD_", iTheBuild, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_UNITS):
			aList.append((INFO.getButton("UNIT_", i), i))
		if aList:
			szChild = PF + "UNIT"
			Panel = aName()
			screen.addPanel(Panel, TRNSLTR.getText("TXT_KEY_PEDIA_CAPABLE_UNITS", ()), "", False, True, X_COL_1, Y_BOT_ROW_1, W_PEDIA_PAGE, H_BOT_ROW, iPanelBlue50)
			for BTN, i in aList:
				screen.attachImageButton(Panel, szChild + str(i), BTN, enumGBS, iWidGen, 1, 1, False)
		else:
			H_TOP_ROW_2 += H_BOT_ROW
		# Feature table
		# The build's OWN per-feature rows. The legacy form asked this build four questions about every feature
		# id in the registry; the build already names the handful it acts on ([DEC-one-reverse-view]).
		aList = []
		for dRow in BUILDINFO.getFeatureRows(iTheBuild):
			aList.append((dRow["feature"], dRow["tech"] + 1, dRow["time"], dRow["remove"], dRow["production"]))
		if aList:
			# Background panel
			ScrollPanel = aName()
			screen.addScrollPanel(ScrollPanel, "", X_COL_1 - 2, Y_TOP_ROW_2, W_PEDIA_PAGE + 16, H_TOP_ROW_2 - 22, iPanelStd)
			screen.setStyle(ScrollPanel, "ScrollPanel_Alt_Style")
			HAMMER = u'%c' %(TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION))
			iOne2 = W_PEDIA_PAGE / 2
			iOne4 = W_PEDIA_PAGE / 4
			iOne6 = W_PEDIA_PAGE / 6
			iOne12 = W_PEDIA_PAGE / 12
			if uFont3b == "<font=3b>":
				sIcon = 28
				dy = 32
			elif uFont3b == "<font=2b>":
				sIcon = 24
				dy = 28
			else:
				sIcon = 20
				dy = 24
			# Header panel
			Panel = aName()
			screen.attachPanelAt(ScrollPanel, Panel, "", "", True, False, iPanelStd, -6, 0, W_PEDIA_PAGE, dy, iWidGen, 0, 0)
			screen.setStyle(Panel, "Panel_Game_HudMap_Style")
			screen.setLabelAt(aName(), ScrollPanel, uFont3b + "Feature", 1<<0, 10, 6, 0, iFontGame, iWidGen, 0, 0)
			screen.attachPanelAt(ScrollPanel, aName(), "", "", True, False, iPanelIn, iOne4, 0, iOne4, dy - 2, iWidGen, 0, 0)
			screen.setLabelAt(aName(), ScrollPanel, uFont3b + TRNSLTR.getText("TXT_KEY_PEDIA_TECH_REQUIRED", ()), 1<<0, iOne4 + 10, 6, 0, iFontGame, iWidGen, 0, 0)
			screen.setLabelAt(aName(), ScrollPanel, uFont3b + "Build Time", 1<<2, 7*iOne12, 6, 0, iFontGame, iWidGen, 0, 0)
			screen.attachPanelAt(ScrollPanel, aName(), "", "", True, False, iPanelIn, iOne2 + iOne6, 0, iOne6, dy - 2, iWidGen, 0, 0)
			screen.setLabelAt(aName(), ScrollPanel, uFont3b + "Removed?", 1<<2, 3*iOne4 + 10, 6, 0, iFontGame, iWidGen, 0, 0)
			screen.setLabelAt(aName(), ScrollPanel, uFont3b + "Obtain", 1<<2, 11*iOne12, 6, 0, iFontGame, iWidGen, 0, 0)
			y = 25
			szChild1 = PF + "FEATURE"
			szChild2 = PF + "TECH"
			for iRow, entry in enumerate(aList):
				iFeature, iTech, iTime, bRemove, iHammers = entry

				screen.attachPanelAt(ScrollPanel, aName(), "", "", True, False, iPanelBlue50, 0, y, W_PEDIA_PAGE - 4, dy + 4, iWidGen, 0, 0)
				if iRow % 2:
					screen.attachPanelAt(ScrollPanel, aName(), "", "", True, False, iPanelIn, 0, y + 4, W_PEDIA_PAGE - 4, dy, iWidGen, 0, 0)
				# Col 1 - Feature
				Panel = "Feature" + str(iRow)
				screen.attachPanelAt(ScrollPanel, Panel, "", "", True, False, iPanelStd, 0, y + 3, iOne4, dy, iWidGen, 0, 0)

				screen.addDDSGFCAt(aName(), Panel, INFO.getButton("FEATURE_", iFeature), 1, 1, sIcon, sIcon, iWidGen, 1, 1, False)
				szText = "<color=230,230,0,255>" + uFont2b + INFO.getDescription("FEATURE_", iFeature)
				screen.setTextAt(szChild1 + str(iFeature), Panel, szText, 1<<0, sIcon + 4, 4, 0, iFontGame, iWidGen, 1, 1)
				# Col 2 - Tech
				Panel = "Tech" + str(iRow)
				screen.attachPanelAt(ScrollPanel, Panel, "", "", True, False, iPanelStd, iOne4, y + 3, iOne4, dy, iWidGen, 0, 0)

				if iTech:
					iTech -= 1
					screen.addDDSGFCAt(aName(), Panel, INFO.getButton("TECH_", iTech), 1, 1, sIcon, sIcon, iWidGen, 1, 1, False)
					szText = "<color=230,230,0,255>" + uFont2b + INFO.getDescription("TECH_", iTech)
					szChild = szChild2 + str(iTech) + "|" + str(iRow)
					screen.setTextAt(szChild, Panel, szText, 1<<0, sIcon + 4, 4, 0, iFontGame, iWidGen, 1, 1)
				# Col 3 - Time
				Panel = "Time" + str(iRow)
				screen.attachPanelAt(ScrollPanel, Panel, "", "", True, False, iPanelStd, iOne2, y + 3, iOne6, dy, iWidGen, 0, 0)
				if iTime:
					if iTime > 0:
						COLOR = "<color=255,0,0,255>"
					else:
						COLOR = "<color=0,230,0,255>"
					szText = str(iTime / 100.0) + "00"
					index = szText.find(".")
					if szText[index + 1] == "0" and szText[index + 2] == "0":
						pass
					elif szText[index + 2] == "0":
						index += 2
					else:
						index += 3
					szText = uFont2b + COLOR + szText[:index] + "<color=255,255,255,255> " + TRNSLTR.getText("TXT_KEY_TURNS", ())
					screen.setLabelAt(aName(), Panel, szText, 1<<2, iOne12, 6, 0, iFontGame, iWidGen, 1, 1)
				# Col 4 - Removed?
				Panel = "Remove" + str(iRow)
				screen.attachPanelAt(ScrollPanel, Panel, "", "", True, False, iPanelStd, 4*iOne6, y + 3, iOne6, dy, iWidGen, 0, 0)
				screen.setLabelAt(aName(), Panel, uFont2b + str(bRemove), 1<<2, iOne12, 6, 0, iFontGame, iWidGen, 1, 1)
				# Col 5 - Hammer
				Panel = "Hammer" + str(iRow)
				screen.attachPanelAt(ScrollPanel, Panel, "", "", True, False, iPanelStd, 5*iOne6, y + 3, iOne6, dy, iWidGen, 0, 0)
				if iHammers:
					if iHammers < 0:
						szText = " <color=255,0,0,255>"
					else:
						szText = " <color=0,230,0,255>"
					szText = szText + str(iHammers) + HAMMER
					screen.setLabelAt(aName(), Panel, uFont2b + szText, 1<<2, iOne12, 6, 0, iFontGame, iWidGen, 1, 1)

				y += dy - 2
