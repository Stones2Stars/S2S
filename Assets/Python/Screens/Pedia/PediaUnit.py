# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()
UNITINFO = CyUnitInfo()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class PediaUnit:

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
		self.W_COL_2 = W_PEDIA_PAGE - W_BASE - H_TOP_ROW - 4
		self.W_COL_3 = W_HALF_PP - 4

		self.S_ICON = S_ICON = H_TOP_ROW - 10

		a = H_TOP_ROW / 12
		self.X_STATS = X_COL_1 + S_ICON
		self.Y_STATS = Y_TOP_ROW + a
		self.H_STATS = H_TOP_ROW - a * 2 + 4
		self.W_STATS = W_BASE - S_ICON


	def interfaceScreen(self, iTheUnit):
		GC = CyGlobalContext()
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		CyPlayer = self.main.CyPlayer
		aName = self.main.getNextWidgetName

		eWidGen				= WidgetTypes.WIDGET_GENERAL
		ePnlBlue50			= PanelStyles.PANEL_STYLE_BLUE50
		eFontTitle			= FontTypes.TITLE_FONT

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
		Y_BOT_ROW_3 = Y_BOT_ROW_2 - H_BOT_ROW
		W_COL_1 = self.W_COL_1
		W_COL_2 = self.W_COL_2
		W_COL_3 = self.W_COL_3
		W_PEDIA_PAGE = self.W_PEDIA_PAGE
		H_ROW_2 = H_TOP_ROW * 3
		S_BOT_ROW = self.S_BOT_ROW

		szText = INFO.getDescription("UNIT_", iTheUnit)
		iCombatType = INFO.getIntrinsic("UNIT_", iTheUnit, IntrinsicSlot.PYINT_UNIT_COMBAT)
		if iCombatType != -1:
			if szfontEdge == "<font=4b>":
				aSize = 22
			elif szfontEdge == "<font=3b":
				aSize = 18
			else:
				aSize = 16
			szText += " - " + '<img=%s size=%d></img>' %(INFO.getButton("UNITCOMBAT_", iCombatType), aSize) + " " + INFO.getDescription("UNITCOMBAT_", iCombatType)

			Txt = "JumpTo|COMBAT" + str(iCombatType)
			self.main.aWidgetBucket.append(Txt)
		else:
			Txt = aName()
		screen.setText(Txt, "", szfontEdge + szText, 1<<0, X_COL_1, 0, 0, eFontTitle, eWidGen, iCombatType, 0)

		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_TOP_ROW_1 + 2, W_COL_1 + 8, H_TOP_ROW + 2, PanelStyles.PANEL_STYLE_MAIN)
		Img = "ToolTip|UNIT" + str(iTheUnit)
		screen.setImageButtonAt(Img, Pnl, INFO.getButton("UNIT_", iTheUnit), 4, 6, S_ICON, S_ICON, eWidGen, 1, 1)

		# Stats
		szName = aName()
		screen.addListBoxGFC(szName, "", self.X_STATS, self.Y_STATS, self.W_STATS, self.H_STATS, TableStyles.TABLE_STYLE_EMPTY)
		screen.enableSelect(szName, False)

		import TextUtil

		# The unit's AUTHORED base strength. The realized, Size-Matters-composed value is a COMPUTED game-state
		# read and stays off the info payload ([pedia-read-map.md] finding 5); the pedia is a static reader.
		iType = INFO.getScalar("UNIT_", iTheUnit, InfoScalar.SCALAR_STRENGTH, CascScope.CASC_SCOPE_UNIT, CascUnit.CASC_UNIT_FLAT)

		if iType > 0:
			szText = TextUtil.floatToString(iType / 100.0) + " " + unichr(8855)
		else: szText = ""


		iType = INFO.getMovementKinds("UNIT_", iTheUnit, CascScope.CASC_SCOPE_UNIT)[MovementKind.MOVEMENT_MOVES] / 100
		if iType > 0:
			szTemp = "%d" %iType + unichr(8856)
			if szText:
				szText = szTemp + " " + szText
			else:
				szText = szTemp
		if szText:
			screen.appendListBoxStringNoUpdate(szName, szfont3 + szText, eWidGen, 0, 0, 1<<0)

		szText = ""
		iType = INFO.getIntrinsic("UNIT_", iTheUnit, IntrinsicSlot.PYINT_COST)
		if iType >= 0:
			if CyPlayer:
				szText = str(CyPlayer.getUnitProductionNeeded(iTheUnit))
			elif not UNITINFO.isFound(iTheUnit):
				szText = str(iType * GC.getDefineINT("UNIT_PRODUCTION_PERCENT")/100)
			if szText:
				szText += u" %c" % TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION)
				screen.appendListBoxStringNoUpdate(szName, szfont3 + szText, eWidGen, 0, 0, 1<<0)
		iType = INFO.getScalar("UNIT_", iTheUnit, InfoScalar.SCALAR_RANGE, CascScope.CASC_SCOPE_UNIT, CascUnit.CASC_UNIT_FLAT) / 100
		if iType > 0:
			szText = TRNSLTR.getText("TXT_KEY_PEDIA_RANGE", (iType,))
			screen.appendListBoxStringNoUpdate(szName, szfont3 + szText, eWidGen, 0, 0, 1<<0)

		iType = INFO.getScalar("UNIT_", iTheUnit, InfoScalar.SCALAR_WORK_RATE, CascScope.CASC_SCOPE_UNIT, CascUnit.CASC_UNIT_FLAT) / 100
		if iType > 0:
			szText = TRNSLTR.getText("TXT_KEY_PEDIA_WORKRATE", (iType,))
			screen.appendListBoxStringNoUpdate(szName, szfont3 + szText, eWidGen, 0, 0, 1<<0)
		screen.updateListBox(szName)

		PF = "ToolTip|JumpTo|"
		aList0 = []
		aList1 = []
		# The unit's OWN combat classes -- primary first, then the subs. The legacy form swept the whole
		# unitcombat registry asking each id whether the unit held it, which is the own-data inversion
		# [DEC-one-reverse-view] bans: the unit already carries the handful it names.
		for k in UNITINFO.getCombatClasses(iTheUnit):
			aList0.append((INFO.getButton("UNITCOMBAT_", k), k))
		if aList0:
			Pnl = aName()
			screen.addPanel(Pnl, "", "", True, True, X_COL_2, Y_TOP_ROW_1, W_COL_2 - 4, H_TOP_ROW, ePnlBlue50)
			szTxt = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SUBCOMBAT_TYPE", ())
			screen.setLabelAt("", Pnl, szTxt, 1<<2, (W_COL_2 - 4) / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
			ScrlPnl = aName()
			screen.addScrollPanel(ScrlPnl, "", X_COL_2 - 2, Y_TOP_ROW_1, W_COL_2, H_TOP_ROW - 26, ePnlBlue50)
			screen.setStyle(ScrlPnl, "ScrollPanel_Alt_Style")
			aSize = (H_TOP_ROW - 48) / 2
			aSize = aSize - aSize % 4
			szChild = PF + "COMBAT"
			iDelta = aSize + 4
			x = 4
			y2 = H_TOP_ROW - 20 - aSize
			y1 = y2 - iDelta
			y = y1
			i = 0
			for BTN, iUnitCombat in aList0:
				screen.setImageButtonAt(szChild + str(iUnitCombat), ScrlPnl, BTN, x, y, aSize, aSize, eWidGen, 1, 2)

				if i % 2:
					x += iDelta
					y = y1
				else:
					y = y2
				i += 1

			aList0 = []

		# Graphic
		s = H_TOP_ROW - 6
		screen.addUnitGraphicGFC("Preview|Min", iTheUnit, X_COL_2 + W_COL_2 + 4, Y_TOP_ROW_1 + 8, s, s, eWidGen, iTheUnit, 0, -20, 30, 0.4, True)
		self.main.aWidgetBucket.append("Preview|Min")

		# Requires -- one BUCKET read per entity kind off the unit's own `requires` section.
		# ⚠ The AND/OR bracket structure the legacy form drew is NOT reproduced, and that is deliberate:
		# getRequiresIds has MENTION semantics (it reports all/anyOf/noneOf alike), so it is explicitly not the
		# requires display. The requires BLOCK COMPOSER that would restore the structure is the text manager's
		# job and is unbuilt ([patterns.md] THE DIVISION OF LABOUR) -- so this lists what is named, flat, rather
		# than asserting a grouping the read cannot support.
		aReqList = []
		n = 0
		for szPrefix, eBucket in (
			("TECH",     EdgeBucket.EDGEB_TECHS),
			("BONUS",    EdgeBucket.EDGEB_BONUSES),
			("CIVIC",    EdgeBucket.EDGEB_CIVICS),
			("RELIGION", EdgeBucket.EDGEB_RELIGIONS),
			("BUILDING", EdgeBucket.EDGEB_BUILDINGS),
		):
			szChild = PF + szPrefix
			for iType in INFO.getRequiresIds("UNIT_", iTheUnit, eBucket):
				aReqList.append([szChild + str(iType) + "|" + str(n), INFO.getButton(szPrefix + "_", iType)])
				n += 1

		# Upgrades To -- the FORWARD successor set (a unit's direct upgrades are its dormant triggers,
		# [enabler.md] par.3). The inverse ("what upgrades INTO me") is not asked here.
		aUpgList = []
		szChild = PF + "UNIT"
		for iUnit in INFO.getDormantTriggerIds("UNIT_", iTheUnit):
			aUpgList.append([szChild + str(iUnit),  INFO.getButton("UNIT_", iUnit)])

		aObsList = INFO.getEdgeIds("UNIT_", iTheUnit, EdgeFamily.EDGEF_OBSOLETED_BY, EdgeBucket.EDGEB_TECHS)
		iType = -1
		if aObsList:
			iType = aObsList[0]
		H_SCROLL = H_BOT_ROW - 50
		if aReqList or aUpgList or iType != -1:
			W_BOT_ROW = W_PEDIA_PAGE
			x = (H_BOT_ROW - S_BOT_ROW) / 2
			if iType != -1:
				X_OBS = X_COL_1 + W_PEDIA_PAGE - H_BOT_ROW
				Pnl = aName()
				screen.addPanel(Pnl, "", "", True, True, X_OBS, Y_BOT_ROW_1, H_BOT_ROW, H_BOT_ROW, ePnlBlue50)
				szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_BONUS_OBSOLETE", ())
				screen.setLabelAt(aName(), Pnl, szText, 1<<2, H_BOT_ROW / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
				screen.setImageButtonAt(PF + "TECH" + str(iType), Pnl, INFO.getButton("TECH_", iType), x, x + 4, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
				W_BOT_ROW -= H_BOT_ROW + 8
			if aReqList or aUpgList:
				i = len(aReqList)
				j = len(aUpgList)
				if aReqList and aUpgList:
					if i < 4 and j > 4:
						W_REQ = W_BOT_ROW / 3 - 4
						W_UPG = 2 * W_BOT_ROW / 3 - 4
					elif j < 4 and i > 4:
						W_REQ = 2 * W_BOT_ROW / 3 - 4
						W_UPG = W_BOT_ROW / 3 - 4
					else:
						W_REQ = W_UPG = W_BOT_ROW / 2 - 4
					X_UPG = X_COL_1 + W_REQ + 8
				else:
					W_REQ = W_UPG = W_BOT_ROW
					X_UPG = X_COL_1

				if aReqList:
					Pnl = aName()
					screen.addPanel(Pnl, "", "", False, True, X_COL_1, Y_BOT_ROW_1, W_REQ, H_BOT_ROW, ePnlBlue50)
					szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_REQUIRES", ())
					screen.setLabelAt(aName(), Pnl, szText, 1<<2, W_REQ / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
					Pnl = aName()
					screen.addScrollPanel(Pnl, "", X_COL_1 - 2, Y_BOT_ROW_1 + 24, W_REQ + 4, H_SCROLL, ePnlBlue50)
					screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
					x = 4
					y = H_SCROLL / 2 - 12
					for entry in aReqList:
						if entry[0] == "TXT":
							x += entry[3]
							screen.setLabelAt(aName(), Pnl, entry[1], entry[2], x, y, 0, eFontTitle, eWidGen, 0, 0)
							x += entry[4]
						else:
							screen.setImageButtonAt(entry[0], Pnl, entry[1], x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
							x += S_BOT_ROW + 4
					screen.hide(Pnl)
					screen.show(Pnl)
				if aUpgList:
					Pnl = aName()
					screen.addPanel(Pnl, "", "", False, True, X_UPG, Y_BOT_ROW_1, W_UPG, H_BOT_ROW, ePnlBlue50)
					szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_UPGRADES_TO", ())
					screen.setLabelAt(aName(), Pnl, szText, 1<<2, W_UPG / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
					Pnl = aName()
					screen.addScrollPanel(Pnl, "", X_UPG - 2, Y_BOT_ROW_1 + 24, W_UPG + 4, H_SCROLL, ePnlBlue50)
					screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
					x = 4
					for NAME, BTN in aUpgList:
						screen.setImageButtonAt(NAME, Pnl, BTN, x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
						x += S_BOT_ROW + 4
					screen.hide(Pnl)
					screen.show(Pnl)
		else:
			H_ROW_2 += H_BOT_ROW
			Y_BOT_ROW_2 += H_BOT_ROW
			Y_BOT_ROW_3 += H_BOT_ROW

		# Promotions -- the ones this unit is CREATED with (its `grants.promotions`).
		# ⚠ This is NOT the legacy panel. That one swept the whole promotion registry asking each id whether the
		# unit QUALIFIED for it -- a whole-database scan [DEC-one-reverse-view] bans, and the rebuilt info carries
		# no qualified-promotion member to answer it from. A promotion's own qualified-unitcombat list is the
		# authored direction; the inverse is UNSERVED, so it is dropped rather than approximated
		# ([DEC-no-legacy-masking]: the hole shows).
		aList0 = []
		aList1 = []
		for k in UNITINFO.getGrantedPromotions(iTheUnit):
			aList0.append((INFO.getButton("PROMOTION_", k), k))

		if aList0:
			screen.addPanel(aName(), "", "", False, False, X_COL_1, Y_BOT_ROW_2, W_PEDIA_PAGE, H_BOT_ROW, ePnlBlue50)
			Pnl = aName()
			screen.addScrollPanel(Pnl, "", X_COL_1 - 2, Y_BOT_ROW_2, W_PEDIA_PAGE + 4, H_BOT_ROW - 26, ePnlBlue50)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			aSize = (H_BOT_ROW - 24) / 2
			aSize = aSize - aSize % 4
			szChild = PF + "PROMO"
			x = 4
			y1 = 6
			y2 = H_BOT_ROW - 20 - aSize
			i = 0
			for BTN, iPromo in aList0:
				if i % 2:
					screen.setImageButtonAt(szChild + str(iPromo), Pnl, BTN, x, y2, aSize, aSize, eWidGen, 1, 1)
					x += aSize + 4
				else:
					screen.setImageButtonAt(szChild + str(iPromo), Pnl, BTN, x, y1, aSize, aSize, eWidGen, 1, 1)
				i += 1
		else:
			H_ROW_2 += H_BOT_ROW
			Y_BOT_ROW_3 += H_BOT_ROW

		# Builds -- the unit's own BUILD_* repertoire (json par.9: which builds THIS unit can perform).
		for iBuild in UNITINFO.getBuilds(iTheUnit):
			aList1.append((INFO.getButton("BUILD_", iBuild), iBuild))

		if aList1:
			Pnl = aName()
			screen.addPanel(Pnl, "", "", False, True, X_COL_1, Y_BOT_ROW_3, W_PEDIA_PAGE, H_BOT_ROW, ePnlBlue50)
			szText = szfont3b + TRNSLTR.getText("TXT_KEY_PEDIA_BUILD", ())
			screen.setLabelAt(aName(), Pnl, szText, 1<<2, W_PEDIA_PAGE / 2, 2, 0, eFontTitle, eWidGen, 0, 0)
			Pnl = aName()
			screen.addScrollPanel(Pnl, "", X_COL_1 - 2, Y_BOT_ROW_3 + 24,W_PEDIA_PAGE + 4, H_SCROLL, ePnlBlue50)
			screen.setStyle(Pnl, "ScrollPanel_Alt_Style")
			x = 4
			y = H_SCROLL / 2 - 12
			szChild = PF + "BUILD"
			for BTN, iBuild in aList1:
				screen.setImageButtonAt(szChild + str(iBuild), Pnl, BTN, x, -2, S_BOT_ROW, S_BOT_ROW, eWidGen, 1, 1)
				x += S_BOT_ROW + 4
			screen.hide(Pnl)
			screen.show(Pnl)
		else:
			H_ROW_2 += H_BOT_ROW

		# Special
		szSpecial = ""
		if UNITINFO.isIgnoreBuildingDefense(iTheUnit):
			szSpecial += TRNSLTR.getText("TXT_KEY_PEDIA_UNIT_IGNORES_BUILDING_DEFENSE", ()) + "\n"
		if UNITINFO.getConscription(iTheUnit) > 0:
			szSpecial += TRNSLTR.getText("TXT_KEY_PEDIA_UNIT_DRAFTABLE", ()) + "\n"
		if UNITINFO.getCaptureUnit(iTheUnit) > 0:
			szSpecial += TRNSLTR.getText("TXT_KEY_PEDIA_UNIT_CAN_BE_CAPTURED", ()) + "\n"
		szSpecial += CyGameTextMgr().getUnitHelp(iTheUnit, True, False, False, -1, -1)[1:]
		# History
		szText = ""
		szTemp = INFO.getStrategy("UNIT_", iTheUnit)
		if szTemp:
			szText += TRNSLTR.getText("TXT_KEY_CIVILOPEDIA_STRATEGY", ()) + szTemp + "\n\n"
		szTemp = INFO.getCivilopedia("UNIT_", iTheUnit)
		if szTemp:
			szText += TRNSLTR.getText("TXT_KEY_CIVILOPEDIA_BACKGROUND", ()) + szTemp

		if szSpecial and szText:
			WIDTH = W_COL_3
		elif szSpecial or szText:
			WIDTH = W_PEDIA_PAGE
			X_COL_3 = X_COL_1
		if szSpecial:
			screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_TOP_ROW_2, WIDTH, H_ROW_2, ePnlBlue50)
			screen.addMultilineText(aName(), szfont3 + szSpecial, X_COL_1 + 4, Y_TOP_ROW_2 + 8, WIDTH - 8, H_ROW_2 - 16, eWidGen, 0, 0, 1<<0)
		if szText:
			screen.addPanel(aName(), "", "", True, False, X_COL_3, Y_TOP_ROW_2, WIDTH, H_ROW_2, ePnlBlue50)
			screen.addMultilineText(aName(), szfont2 + szText, X_COL_3 + 4, Y_TOP_ROW_2 + 8, WIDTH - 8, H_ROW_2 - 16, eWidGen, 0, 0, 1<<0)
