# Pedia overhaul by Toffer for Caveman2Cosmos.

from CvPythonExtensions import *
INFO = CyInfo()
TRNSLTR = CyTranslator()

class PediaEra:

	def __init__(self, parent, H_BOT_ROW):
		self.main = parent

		H_PEDIA_PAGE = parent.H_PEDIA_PAGE

		self.Y_TOP_ROW = Y_TOP_ROW = parent.Y_PEDIA_PAGE
		self.H_TOP_ROW = H_TOP_ROW = 3 * (H_PEDIA_PAGE - H_BOT_ROW * 3) / 4

		self.Y_BOT_ROW = Y_BOT_ROW = Y_TOP_ROW + H_TOP_ROW
		self.H_BOT_ROW = Y_TOP_ROW + H_PEDIA_PAGE - Y_BOT_ROW

		self.W_PEDIA_PAGE = W_PEDIA_PAGE = parent.W_PEDIA_PAGE

		self.W_COL_1 = H_TOP_ROW
		self.W_COL_2 = W_PEDIA_PAGE - H_TOP_ROW

		self.X_COL_1 = X_COL_1 = parent.X_PEDIA_PAGE
		self.X_COL_2 = X_COL_1 + H_TOP_ROW


	def interfaceScreen(self, iTheEra):
		TRNSLTR = CyTranslator()
		screen = self.main.screen()
		aName = self.main.getNextWidgetName

		eWidGen				= WidgetTypes.WIDGET_GENERAL
		ePnlBlue50		= PanelStyles.PANEL_STYLE_BLUE50
		eFontTitle		= FontTypes.TITLE_FONT

		szfontEdge, szfont4b, szfont4, szfont3b, szfont3, szfont2b, szfont2 = self.main.aFontList

		X_COL_1 = self.X_COL_1
		X_COL_2 = self.X_COL_2
		W_COL_2 = self.W_COL_2
		Y_ROW_1 = self.Y_TOP_ROW
		Y_ROW_2 = self.Y_BOT_ROW
		H_ROW_1 = self.H_TOP_ROW
		H_ROW_2 = self.H_BOT_ROW
		W_PEDIA_PAGE = self.W_PEDIA_PAGE

		szText = INFO.getDescription("C2C_ERA_", iTheEra)
		IMG = INFO.getButton("C2C_ERA_", iTheEra)
		x = X_COL_1
		if IMG:
			screen.setText(aName(), "", "<img=%s>" % IMG, 1<<0, X_COL_1, 0, 0, eFontTitle, eWidGen, 1, 2)
			x += 44
		screen.setText(aName(), "", szfontEdge + szText, 1<<0, x, 0, 0, eFontTitle, eWidGen, 1, 2)

		Pnl = aName()
		screen.addPanel(Pnl, "", "", False, False, X_COL_1 - 3, Y_ROW_1 + 2, H_ROW_1, H_ROW_1, PanelStyles.PANEL_STYLE_MAIN)

		if not iTheEra:
			IMG = "Art/Movies/Era/Era00-Prehistoric.dds"
		elif iTheEra == 1:
			IMG = "Art/Movies/Era/Era01-Ancient.dds"
		elif iTheEra == 2:
			IMG = "Art/Movies/Era/Era02-Classical.dds"
		elif iTheEra == 3:
			IMG = "Art/Movies/Era/Era03-Medeival.dds"
		elif iTheEra == 4:
			IMG = "Art/Movies/Era/Era04-Renaissance.dds"
		elif iTheEra == 5:
			IMG = "Art/Movies/Era/Era05-Industrial.dds"
		elif iTheEra == 6:
			IMG = "Art/Movies/Era/Era07-Modern.dds"
		elif iTheEra == 7:
			IMG = "Art/Movies/Era/info.dds"
		elif iTheEra == 8:
			IMG = "Art/Movies/Era/Era06-TransHuman.dds"
		elif iTheEra == 9:
			IMG = "Art/Movies/Era/transhuman.dds"
		elif iTheEra == 10:
			IMG = "Art/Movies/Era/galactic.dds"
		elif iTheEra == 11:
			IMG = "Art/Movies/Era/cosmic.dds"
		elif iTheEra == 12:
			IMG = "Art/Movies/Era/transcendent.dds"
		else:
			IMG = "Art/Movies/Era/Era08-Galactic.dds"

		screen.setImageButtonAt(aName(), Pnl, IMG, 12, 12, H_ROW_1-24, H_ROW_1-24, eWidGen, 1, 1)

		# --- WHAT THIS ERA DOES TO YOU ---
		#
		# Every pacing dial is a PERCENT against a 100 baseline, and printing the raw number is precisely why
		# this page was unreadable: "150% Research percent" is not a bonus, it means a tech costs half again as
		# much. So each line states the DIRECTION and how far from baseline it sits, and a dial AT baseline is
		# not printed at all -- an era's page should show what makes it different, not a wall of 100s.
		def pace(iPercent, szLabel, szMore, szLess):
			# iPercent multiplies an underlying COST or DURATION, so above 100 always means MORE of it.
			# 0 means the era authors no such dial -- absent is not "costs nothing".
			if iPercent == 100 or iPercent <= 0:
				return ""
			if iPercent > 100:
				return "\n<color=255,80,80,255>%s: %d%% %s</color>" % (szLabel, iPercent - 100, szMore)
			return "\n<color=0,230,0,255>%s: %d%% %s</color>" % (szLabel, 100 - iPercent, szLess)

		aCosts = INFO.getCostKinds("C2C_ERA_", iTheEra, CascScope.CASC_SCOPE_WORLD)
		aDurations = INFO.getDurationKinds("C2C_ERA_", iTheEra, CascScope.CASC_SCOPE_WORLD)
		iGrowth = INFO.getScalar("C2C_ERA_", iTheEra, InfoScalar.SCALAR_GROWTH,
			CascScope.CASC_SCOPE_WORLD, CascUnit.CASC_UNIT_PERCENT)
		iGreatPeople = INFO.getScalar("C2C_ERA_", iTheEra, InfoScalar.SCALAR_GREAT_PEOPLE_RATE,
			CascScope.CASC_SCOPE_WORLD, CascUnit.CASC_UNIT_PERCENT)
		iEventChance = INFO.getScalar("C2C_ERA_", iTheEra, InfoScalar.SCALAR_EVENT_CHANCE,
			CascScope.CASC_SCOPE_WORLD, CascUnit.CASC_UNIT_FLAT)

		szHeading = szfont3b + "Pace of this era" + szfont3
		szTxt = szHeading
		# COSTS are prices: above baseline is dearer.
		szTxt += pace(aCosts[CostsKind.COSTS_TRAIN], "Units", "dearer to train", "cheaper to train")
		szTxt += pace(aCosts[CostsKind.COSTS_CONSTRUCT], "Buildings", "dearer to construct", "cheaper to construct")
		szTxt += pace(aCosts[CostsKind.COSTS_CREATE], "Projects", "dearer to create", "cheaper to create")
		szTxt += pace(aCosts[CostsKind.COSTS_RESEARCH], "Research", "dearer", "cheaper")
		szTxt += pace(aCosts[CostsKind.COSTS_BUILD], "Worker builds", "slower", "faster")
		szTxt += pace(aCosts[CostsKind.COSTS_IMPROVEMENT_UPGRADE], "Improvement upgrades", "slower", "faster")
		# GROWTH is the FOOD THRESHOLD a city must fill, so a higher number is a SLOWER city. This is the dial
		# that was most often read backwards.
		szTxt += pace(iGrowth, "City growth", "more food needed", "less food needed")
		# GREAT PEOPLE is a threshold too, not a rate.
		szTxt += pace(iGreatPeople, "Great people", "slower to arrive", "faster to arrive")
		# DURATIONS are spans: longer is worse.
		szTxt += pace(aDurations[DurationsKind.DURATIONS_CIVIC_ANARCHY], "Civic anarchy", "longer", "shorter")
		szTxt += pace(aDurations[DurationsKind.DURATIONS_RELIGIOUS_ANARCHY], "Religious anarchy", "longer", "shorter")

		if iEventChance:
			szTxt += "\nRandom event chance: %d%% per turn" % iEventChance

		if szTxt == szHeading:
			szTxt = szfont3 + "This era runs at the standard pace: no cost, growth or anarchy dial departs from baseline."

		# --- STARTING HERE ---
		# What a player beginning the game in this era is handed. The unit multiplier scales the three counts,
		# so it is shown as the resulting number rather than as a bare factor nobody can apply in their head.
		iGold = INFO.getIntrinsic("C2C_ERA_", iTheEra, IntrinsicSlot.PYINT_ERA_STARTING_GOLD)
		iMult = INFO.getIntrinsic("C2C_ERA_", iTheEra, IntrinsicSlot.PYINT_ERA_STARTING_UNIT_MULTIPLIER)
		iDefence = INFO.getIntrinsic("C2C_ERA_", iTheEra, IntrinsicSlot.PYINT_ERA_STARTING_DEFENSE_UNITS)
		iWorkers = INFO.getIntrinsic("C2C_ERA_", iTheEra, IntrinsicSlot.PYINT_ERA_STARTING_WORKER_UNITS)
		iExplorers = INFO.getIntrinsic("C2C_ERA_", iTheEra, IntrinsicSlot.PYINT_ERA_STARTING_EXPLORE_UNITS)
		iFreePop = INFO.getIntrinsic("C2C_ERA_", iTheEra, IntrinsicSlot.PYINT_ERA_FREE_POPULATION)
		if iMult < 1:
			iMult = 1

		szStart = ""
		if iGold:
			szStart += "\nGold: %d" % iGold
		for iCount, szLabel in ((iDefence, "Defenders"), (iWorkers, "Workers"), (iExplorers, "Explorers")):
			if iCount:
				szStart += "\n%s: %d" % (szLabel, iCount * iMult)
		if iFreePop:
			szStart += "\nCities found with %d extra population" % iFreePop
		if szStart:
			szTxt += "\n\n" + szfont3b + "Starting in this era" + szfont3 + szStart

		if szTxt:
			screen.addPanel(aName(), "", "", True, False, X_COL_2, Y_ROW_1 + 2, W_COL_2, H_ROW_1 - 2, ePnlBlue50)
			screen.addMultilineText(aName(), szfont3 + szTxt, X_COL_2 + 4, Y_ROW_1 + 10, W_COL_2 - 8, H_ROW_1 - 18, eWidGen, 0, 0, 1<<0)

		# Text
		szTxt = TRNSLTR.getText("TXT_KEY_CIVILOPEDIA_STRATEGY", ()) + INFO.getStrategy("C2C_ERA_", iTheEra) + "\n\n"

		screen.addPanel(aName(), "", "", True, False, X_COL_1, Y_ROW_2, W_PEDIA_PAGE, H_ROW_2, ePnlBlue50)
		screen.addMultilineText(aName(), szfont2 + szTxt, X_COL_1 + 4, Y_ROW_2 + 8, W_PEDIA_PAGE - 8, H_ROW_2 - 16, eWidGen, 0, 0, 1<<0)
