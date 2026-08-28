from CvPythonExtensions import *

# The one data-fetching library: INFO = what an entity CARRIES, ENABLER = can I?, ENUMS = the engine
# enum vocabulary + name->id resolution. A game object's own data is asked OF THAT OBJECT --
# GC.getPlayer(i).getCity(id).getYields(), never a flat class keyed by (owner, id).
GC = CyGlobalContext()
INFO = CyInfo()   # entity data: the context serves settings, CyInfo serves entities
ENABLER = CyEnabler()
ENUMS = CyEnums()

class CvWonderMovieScreen:

	def __init__(self):
		# The ENGINE drives update() for a screen it has turned on, and it does so independently of
		# interfaceScreen -- which returns early for four perfectly ordinary reasons: the NO_MOVIES graphic
		# option, an invalid movie type, an entity that simply HAS no movie file ("not all projects have
		# movies"), and an unrecognised file extension. On any of those the movie state was never established,
		# and update() read an attribute that did not exist -- an AttributeError per frame, which the engine
		# reports as "Python function update failed" and which leaves the window rendering NOTHING.
		# So the state update() reads is established HERE, once, and is never absent.
		self.resetMovieState()

	def resetMovieState(self):
		self.screenId = -1
		self.iMovieType = -1
		self.iWonderId = -1
		self.fTime = 0
		self.bTimer = False
		self.iInfoPanel = 0

	def interfaceScreen(self, iMovieItem, iCityId, iMovieType, screenId):
		# iMovieItem is either the WonderID, the ReligionID, or the ProjectID, depending on iMovieType (or the feature id :p)

		if CyUserProfile().getGraphicOption(GraphicOptionTypes.GRAPHICOPTION_NO_MOVIES):
			return
		if iMovieType < 0 or iMovieType > 3:
			print "[WARNING] CvWonderMovieScreen - Invalid movie type"
			return

		self.iMovieType = iMovieType
		self.iInfoPanel = 0

		# not all projects have movies
		szMovieFile = None
		#	All three carry the movie as an ART DEFINE TAG now and resolve the same way. Legacy handed a
		#	building's back as a path and a project's as a tag, which is why they used to be read differently.
		if not iMovieType:
			szArtDef = INFO.getMovieDefineTag("BUILDING_", iMovieItem)
			if szArtDef:
				szMovieFile = CyArtFileMgr().getMovieArtInfo(szArtDef).getPath()

		elif iMovieType == 1:
			szArtDef = INFO.getMovieDefineTag("RELIGION_", iMovieItem)
			if szArtDef:
				szMovieFile = CyArtFileMgr().getMovieArtInfo(szArtDef).getPath()

		elif iMovieType == 2:
			szArtDef = INFO.getMovieDefineTag("PROJECT_", iMovieItem)
			if szArtDef:
				szMovieFile = CyArtFileMgr().getMovieArtInfo(szArtDef).getPath()

		elif iMovieType == 3:
			sType = INFO.getType("FEATURE_", iMovieItem)
			if sType:
				iPlaty = sType.find("_PLATY_")
				if iPlaty >= 0:
					szArtDef = CyArtFileMgr().getMovieArtInfo("ART_DEF_MOVIE_" + sType[iPlaty+7:])
					if szArtDef:
						szMovieFile = szArtDef.getPath()

		if not szMovieFile:
			return
		fileExt = szMovieFile[-4:]
		if fileExt in (".bik", ".nif"):
			bMovie = True
		elif fileExt == ".dds":
			bMovie = False
		else: return

		self.iWonderId = iMovieItem

		# move the camera and mark the interface camera as dirty so that it gets reset - JW
		if iCityId > -1:
			CyIF = CyInterface()
			if not iMovieType:
				CyIF.lookAtCityBuilding(iCityId, iMovieItem)
			else: CyIF.lookAtCityBuilding(iCityId, -1)

			CyIF.setDirty(InterfaceDirtyBits.SelectionCamera_DIRTY_BIT, True)

		self.screenId = screenId
		screen = CyGInterfaceScreen("WonderMovieScreen", screenId)

		import ScreenResolution as SR
		screen.setDimensions((SR.x - 752)/2, (SR.y - 566)/2, 752, 566)
		screen.enableWorldSounds(False)
		screen.showWindowBackground(True)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

		screen.addPanel("", "", "", True, True, 0, 0, 752, 566, PanelStyles.PANEL_STYLE_MAIN)

		# Header
		if not iMovieType:
			szHeader = INFO.getDescription("BUILDING_", iMovieItem)

		elif iMovieType == 1:
			szHeader = CyTranslator().getText("TXT_KEY_MISC_REL_FOUNDED_MOVIE", (INFO.getTextKey("RELIGION_", iMovieItem), ))

		elif iMovieType == 2:
			szHeader = INFO.getDescription("PROJECT_", iMovieItem)

		elif iMovieType == 3:
			# The FEATURE path never binds CvInfo -- it resolves its movie through the id surface, not through
			# a legacy info handle -- so the header reads the same way (: the id surface IS
			# the read). Falling through to CvInfo here raised UnboundLocalError AFTER showScreen and BEFORE
			# the exit button was added, which is what left an undismissable empty window.
			szHeader = INFO.getDescription("FEATURE_", iMovieItem)

		screen.setText("", "", "<font=4b>" + szHeader, 1<<2, 376, 10, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, 1, 2)

		self.iInfoPanel = 0

		if bMovie:
			# Play the movie
			self.bTimer = False
			self.fTime = 0
			if self.iMovieType == 1:
				screen.addReligionMovieWidgetGFC("ReligionMovie", szMovieFile, 16, 42, 720, 480, WidgetTypes.WIDGET_GENERAL, 1, 2) # Not sure if this one understand .dds files...
			else:
				screen.playMovie(szMovieFile, 16, 42, 720, 480, 0) # This one can play .dds files too, but I think addDDSGFC is better to use in that case anyhow.
		else:
			self.bTimer = True
			self.fTime = 3
			screen.addDDSGFC("", szMovieFile, 16, 42, 720, 480, WidgetTypes.WIDGET_GENERAL, 1, 2)

		# Sound
		if self.iMovieType == 1:
			szSound = INFO.getSound("RELIGION_", iMovieItem)
			if szSound:
				CyInterface().playGeneralSound(szSound)

		screen.setButtonGFC("WonderExit", CyTranslator().getText("TXT_KEY_MAIN_MENU_OK", ()), "", 316, 526, 120, 30, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1, ButtonStyles.BUTTON_STYLE_STANDARD)

	def infoPanel(self):
		if not self.iInfoPanel:
			if not self.iMovieType:
				szHelp = CyGameTextMgr().getBuildingHelp(self.iWonderId, False, -1, -1, False, False, False)

			elif self.iMovieType == 2:
				szHelp = CyGameTextMgr().getProjectHelp(self.iWonderId, False, -1, -1)

			elif self.iMovieType == 3:
				szHelp = INFO.getCivilopedia("FEATURE_", self.iWonderId)
			else:
				szHelp = ""

			if szHelp:
				screen = CyGInterfaceScreen("WonderMovieScreen", self.screenId)
				screen.addPanel("MonkeyPanel", "", "", True, True, 120, 82, 512, 416, PanelStyles.PANEL_STYLE_MAIN_BLACK50)
				screen.attachMultilineText("MonkeyPanel", "", szHelp, WidgetTypes.WIDGET_GENERAL, 1, 2, 1<<0)
				self.iInfoPanel = 1
			self.bTimer = False
		else:
			if self.iInfoPanel == 1:
				CyGInterfaceScreen("WonderMovieScreen", self.screenId).hide("MonkeyPanel")
			else:
				CyGInterfaceScreen("WonderMovieScreen", self.screenId).show("MonkeyPanel")
			self.iInfoPanel = -self.iInfoPanel

	# Will handle the input for this screen...
	def handleInput(self, inputClass):
		iCode = inputClass.eNotifyCode
		if iCode == 19: # NOTIFY_MOVIE_DONE
			if not self.iInfoPanel:
				self.infoPanel()
			return 1

		elif iCode == 17: # Key Up
			self.infoPanel()

		elif not iCode: # Click
			if inputClass.szFunctionName != "WonderExit":
				self.infoPanel()
				return 1
		return 0

	def update(self, fDelta):
		if self.bTimer:
			self.fTime -= fDelta
			if self.fTime < 0:
				self.infoPanel()
				self.bTimer = False

	def onClose(self):
		# RESET, never `del`. Deleting these is what guaranteed the failure above: the screen instance is
		# long-lived (it is built once by the screen factory), so a closed movie left an object whose state
		# had been destroyed while the engine could still call update() on it.
		self.resetMovieState()