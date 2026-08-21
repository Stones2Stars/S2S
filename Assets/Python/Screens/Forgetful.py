from CvPythonExtensions import *

#	This screen enumerates EVERY registered type, so it is the acceptance case for "the library can enumerate
#	every type" ([python-read-map] 3.6) -- which is why it is addressed by INFOTYPE PREFIX rather than by an
#	accessor per registry. INFO.getIndex(prefix) hands back the whole registry's identity in ONE crossing, and a
#	boost::python call costs far more than the lookup inside it ([patterns.md] THE PEDIA IS THE ONE PLACE A FULL
#	SCAN IS UNAVOIDABLE: what changes is the COST, never the scan).
GC = CyGlobalContext()
INFO = CyInfo()

class Forgetful:
	def __init__(self):
		self.iForgetfulType = 0

	def interfaceScreen(self, screenId):
		self.screenId = screenId
		import ScreenResolution
		self.xRes = ScreenResolution.x
		self.yRes = ScreenResolution.y
		GC = CyGlobalContext()

		#	(label, INFOTYPE prefix). ⚠ Four prefixes do NOT spell their label: an era is C2C_ERA_, a world SIZE is
		#	WORLDSIZE_, a leaderhead is LEADER_ and an espionage mission is ESPIONAGEMISSION_ ([naming.md]).
		self.lForgetful = [
			["Bonus", "BONUS_"],
			["Build", "BUILD_"],
			["Building", "BUILDING_"],
			["Civic", "CIVIC_"],
			["CivicOption", "CIVICOPTION_"],
			["Civilization", "CIVILIZATION_"],
			["Climate", "CLIMATE_"],
			["Command", "COMMAND_"],
			["Commerce", "COMMERCE_"],
			["Concept", "CONCEPT_"],
			["Control", "CONTROL_"],
			["Corporation", "CORPORATION_"],
			["CultureLevel", "CULTURELEVEL_"],
			["Domain", "DOMAIN_"],
			["Era", "C2C_ERA_"],
			["Espionage", "ESPIONAGEMISSION_"],
			["Event", "EVENT_"],
			["EventTrigger", "EVENTTRIGGER_"],
			["Feature", "FEATURE_"],
			["GameOption", "GAMEOPTION_"],
			["GameSpeed", "GAMESPEED_"],
			["Goody", "GOODY_"],
			["Handicap", "HANDICAP_"],
			["Hurry", "HURRY_"],
			["Improvement", "IMPROVEMENT_"],
			["LeaderHead", "LEADER_"],
			["Memory", "MEMORY_"],
			["Mission", "MISSION_"],
			["NewConcept", "NEWCONCEPT_"],
			["Process", "PROCESS_"],
			["Project", "PROJECT_"],
			["Promotion", "PROMOTION_"],
			["Religion", "RELIGION_"],
			["Route", "ROUTE_"],
			["SpecialBuilding", "SPECIALBUILDING_"],
			["Specialist", "SPECIALIST_"],
			["SpecialUnit", "SPECIALUNIT_"],
			["Tech", "TECH_"],
			["Terrain", "TERRAIN_"],
			["Trait", "TRAIT_"],
			["Unit", "UNIT_"],
			["UnitAI", "UNITAI_"],
			["UnitCombat", "UNITCOMBAT_"],
			["Upkeep", "UPKEEP_"],
			["Victory", "VICTORY_"],
			["Vote", "VOTE_"],
			["VoteSource", "VOTESOURCE_"],
			["World", "WORLDSIZE_"],
			["Yield", "YIELD_"],
		]
		self.iTypes = len(self.lForgetful)

		screen = CyGInterfaceScreen("ForgetfulScreen", screenId)
		screen.addPanel("", "", "", True, False, -10, -10, self.xRes + 20, self.yRes + 20, PanelStyles.PANEL_STYLE_MAIN)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		screen.setLabel("", "", "<font=4b>TXT_KEY_XML_TAGS", 1<<2, self.xRes/2, 12, 0, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, 1, 2)
		screen.setText("", "", "<font=4b>" + CyTranslator().getText("TXT_WORD_EXIT", ()), 1<<1, self.xRes - 12, 12, 0, FontTypes.GAME_FONT, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)

		self.setDropDown()

	def setDropDown(self):
		szDropdownName = "ForgetfulType"
		screen = CyGInterfaceScreen("ForgetfulScreen", self.screenId)
		if screen.getWidget(szDropdownName):  # Check if it exists first
			screen.deleteWidget(szDropdownName)
		screen.addDropDownBoxGFC(szDropdownName, 12, 12, 180, WidgetTypes.WIDGET_GENERAL, 1, 2, FontTypes.GAME_FONT)
		for i in xrange(self.iTypes):
			screen.addPullDownString(szDropdownName, self.lForgetful[i][0], i, i, i == self.iForgetfulType)
		self.drawTable()

	def drawTable(self):
		Table = "ForgetfulTable"
		iWidth = self.xRes - 16
		w0 = (iWidth - 64)/3
		screen = CyGInterfaceScreen("ForgetfulScreen", self.screenId)
		if screen.getWidget(Table):  # Guard this too
			screen.deleteWidget(Table)
		screen.addTableControlGFC(Table, 4, 8, 52, iWidth, self.yRes - 60, True, False, 24, 24, TableStyles.TABLE_STYLE_STANDARD)
		screen.setTableColumnHeader(Table, 0, "ID", 64)
		screen.setTableColumnHeader(Table, 1, "NAME", w0)
		screen.setTableColumnHeader(Table, 2, "TYPE", w0)
		screen.setTableColumnHeader(Table, 3, "TEXT", w0)
		screen.enableSort(Table)
		eWidGen = WidgetTypes.WIDGET_GENERAL
		#	ONE crossing for the whole registry. The row index is the TABLE's, never the entity's -- a JSON repo
		#	may hold a hole, so the ids are not necessarily contiguous and the id travels in the entry itself.
		iRow = 0
		for kEntry in INFO.getIndex(self.lForgetful[self.iForgetfulType][1]):
			screen.appendTableRow(Table)
			screen.setTableInt(Table, 0, iRow, str(kEntry["id"]), "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(Table, 1, iRow, kEntry["description"], kEntry["button"], eWidGen, 1, 2, 1<<0)
			screen.setTableText(Table, 2, iRow, "<font=1>" + kEntry["type"], "", eWidGen, 1, 2, 1<<0)
			screen.setTableText(Table, 3, iRow, "<font=1>" + kEntry["textKey"], "", eWidGen, 1, 2, 1<<0)
			iRow += 1

	def update(self, fDelta): return

	def handleInput(self, inputClass):
		if inputClass.getFunctionName() == "ForgetfulType":
			self.iForgetfulType = int(inputClass.getData())
			self.drawTable()

	def back(self):
		if self.iForgetfulType > 0:
			self.iForgetfulType -= 1
		else:
			self.iForgetfulType = self.iTypes - 1
		self.drawTable()  # Not setDropDown()

	def forward(self):
		if self.iForgetfulType < self.iTypes - 1:
			self.iForgetfulType += 1
		else:
			self.iForgetfulType = 0
		self.drawTable()  # Not setDropDown()

	def onClose(self):
		try:
			screen = CyGInterfaceScreen("ForgetfulScreen", self.screenId)
			screen.hideScreen()
		except:
			pass
		del self.lForgetful, self.screenId, self.xRes, self.yRes, self.iTypes