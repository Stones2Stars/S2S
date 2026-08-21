# Pedia overhauled by Toffer90 for Caveman2Cosmos.

from CvPythonExtensions import *
import HandleInputUtil
import UnitUpgradesGraph

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
INFO = CyInfo()
MAP = GC.getMap()
STATE = CyState()
ENABLER = CyEnabler()
ENUMS = CyEnums()
TRNSLTR = CyTranslator()

TEXT = CyGameTextMgr()
class Pedia:

	def __init__(self, screenId):

		self.screenId = screenId
		self.bNotPedia = True
		self.pediaHistory = [(-1, "", 0)]

	def screen(self):
		return CyGInterfaceScreen("PediaMainScreen", self.screenId)

	def getScreen(self): # Legacy, will remove it.
		return CyGInterfaceScreen("PediaMainScreen", self.screenId)

	# Called when you open the pedia directly.
	def pediaShow(self):
		screen = self.screen()
		if screen.isActive(): return
		current = self.pediaHistory.pop()
		self.pediaJump(current[0], current[1], current[2])

	# Prepare pedia.
	def startPedia(self):
		print "Start Pedia"
		self.bKeyPress = False

		import InputData
		self.InputData = InputData.instance

		import PythonToolTip
		self.tooltip = PythonToolTip.PythonToolTip()

		self.fKeyTimer = 99999
		self.bIndex = True
		self.aList = []
		self.aWidgetBucket = []
		self.bMovie = False
		screen = self.screen()
		# Get screen resolution.
		import ScreenResolution as SR
		self.xRes = xRes = SR.x
		self.yRes = yRes = SR.y

		# Calibrate variables.
		if yRes > 1000:
			self.enumGBS = GenericButtonSizes.BUTTON_SIZE_CUSTOM
			H_BOT_ROW = 110
			self.H_EDGE_PANEL = H_EDGE_PANEL = 38
			szfontEdge = "<font=4b>"
		elif yRes > 800:
			self.enumGBS = GenericButtonSizes.BUTTON_SIZE_46
			H_BOT_ROW = 92
			self.H_EDGE_PANEL = H_EDGE_PANEL = 32
			szfontEdge = "<font=3b>"
		else:
			self.enumGBS = GenericButtonSizes.BUTTON_SIZE_32
			H_BOT_ROW = 78
			self.H_EDGE_PANEL = H_EDGE_PANEL = 29
			szfontEdge = "<font=2b>"

		if xRes > 1700:
			self.W_CATEGORIES = W_CATEGORIES = 197
			self.W_ITEMS = 260
			self.aFontList = aFontList = [szfontEdge, "<font=4b>", "<font=4>", "<font=3b>", "<font=3>", "<font=2b>", "<font=2>"]
			self.ICON_SIZE = 23
		elif xRes > 1400:
			self.W_CATEGORIES = W_CATEGORIES = 180
			self.W_ITEMS = 230
			self.aFontList = aFontList = [szfontEdge, "<font=3b>", "<font=3>", "<font=2b>", "<font=2>", "<font=1b>", "<font=1>"]
			self.ICON_SIZE = 20
		else:
			self.W_CATEGORIES = W_CATEGORIES = 158
			self.W_ITEMS = 200
			self.aFontList = aFontList = [szfontEdge, "<font=2b>", "<font=2>", "<font=1b>", "<font=1>", "<font=0b>", "<font=0>"]
			self.ICON_SIZE = 18

		self.X_PEDIA_PAGE = W_CATEGORIES + self.W_ITEMS + 8
		self.Y_PEDIA_PAGE = Y_PEDIA_PAGE = H_EDGE_PANEL - 6
		self.H_MID_SECTION = yRes - H_EDGE_PANEL * 2 + 14
		H_CATEGORIES = self.H_MID_SECTION / 2
		Y_SUBCATEGORIES = Y_PEDIA_PAGE + H_CATEGORIES
		self.Y_BOT_TEXT = Y_BOT_TEXT = yRes - H_EDGE_PANEL + 8
		self.R_PEDIA_PAGE = xRes - 8
		self.B_PEDIA_PAGE = yRes - H_EDGE_PANEL
		self.W_PEDIA_PAGE = self.R_PEDIA_PAGE - self.X_PEDIA_PAGE
		self.H_PEDIA_PAGE = self.B_PEDIA_PAGE - Y_PEDIA_PAGE

		import Index_Pedia
		self.pediaIndex		= Index_Pedia.Index(self, xRes, yRes, H_EDGE_PANEL, Y_PEDIA_PAGE, szfontEdge)

		# Initialize category classes
		G = GC.getGame()
		PRODUCTION	= u'%c' %TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_PRODUCTION)
		COMMERCE	= u'%c' %TEXT.getSymbolChar("YIELD_", YieldTypes.YIELD_COMMERCE)
		GOLD		= u'%c' %TEXT.getSymbolChar("COMMERCE_", CommerceTypes.COMMERCE_GOLD)
		BEAKER		= u'%c' %TEXT.getSymbolChar("COMMERCE_", CommerceTypes.COMMERCE_RESEARCH)
		STRENGHT	= u'%c' %G.getSymbolID(FontSymbols.STRENGTH_CHAR)
		MOVES		= u'%c' %G.getSymbolID(FontSymbols.MOVES_CHAR)
		SILVERSTAR	= u'%c' %G.getSymbolID(FontSymbols.SILVER_STAR_CHAR)
		GREATPEOPLE	= u'%c' %G.getSymbolID(FontSymbols.GREAT_PEOPLE_CHAR)
		MAP			= u'%c' %G.getSymbolID(FontSymbols.MAP_CHAR)
		DEF_PACT	= u'%c' %G.getSymbolID(FontSymbols.DEFENSIVE_PACT_CHAR)
		TRADE		= u'%c' %G.getSymbolID(FontSymbols.TRADE_CHAR)
		GOLDENAGE	= u'%c' %G.getSymbolID(FontSymbols.GOLDEN_AGE_CHAR)
		OCCUPATION	= u'%c' %G.getSymbolID(FontSymbols.OCCUPATION_CHAR)
		POWER		= u'%c' %G.getSymbolID(FontSymbols.POWER_CHAR)
		BULLET		= u'%c' %G.getSymbolID(FontSymbols.BULLET_CHAR)
		# Map symbols.
		self.categoryGraphics = categoryGraphics = {
			"PRODUCTION"	: PRODUCTION,
			"COMMERCE"		: COMMERCE,
			"GOLD"			: GOLD,
			"BEAKER"		: BEAKER,
			"STRENGHT"		: STRENGHT,
			"MOVES"			: MOVES,
			"SILVERSTAR"	: SILVERSTAR,
			"GREATPEOPLE"	: GREATPEOPLE,
			"MAP"			: MAP,
			"DEF_PACT"		: DEF_PACT,
			"TRADE"			: TRADE,
			"GOLDENAGE"		: GOLDENAGE,
			"OCCUPATION"	: OCCUPATION,
			"POWER"			: POWER,
			"BULLET"		: BULLET,
		}
		szCatTechs				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_TECH", ())
		szCatUnits				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_UNIT", ())
		szCatSpecialUnits		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SPECIAL_UNITS", ())
		szCatWorldUnits			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_HERO", ())
		szCatAnimals			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_ANIMALS", ())
		szCatCulturalUnits		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_CULTURAL_UNITS", ())
		szCatSpreadUnits		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SPREAD_UNITS", ())
		szCatMiscUnits			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_MISC_UNITS", ())
		szCatUnitTree			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_UNIT_UPGRADES", ())
		szCatUnitCombat			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_UNIT_COMBAT", ())
		szCatPromotions			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_PROMOTION", ())
		szCatBuildUp			= TRNSLTR.getText("TXT_KEY_MISSION_BUILDUP", ())
		szCatStatus				= TRNSLTR.getText("TXT_KEY_VICTORY_SCREEN_PERCENTAGE", ())
		szCatPromotionTree		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_PROMOTION_TREE", ())
		szCatBuildings			= TRNSLTR.getText("TXT_KEY_WB_BUILDINGS", ())
		szCatNationalWonders	= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_NATIONAL_WONDERS", ())
		szCatGreatWonders		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_GREAT_WONDERS", ())
		szCatC2CCutures			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_C2C_CULTURES", ())
		szCatSpecialBuildings	= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BUILDINGS_SPECIAL", ())
		szCatRelBuildings		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_RELIGIOUS_BUILDINGS", ())
		szCatAniBuildings		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_ANIMALISTIC_BUILDINGS", ())
		szCatSpaceBuildings		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SPACE_BUILDINGS", ())
		szCatBuildingTree		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BUILDING_TREE", ())
		szCatProjects			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_PROJECT", ())
		szCatSpecialists		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SPECIALIST", ())
		szCatTerrains			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_TERRAIN", ())
		szCatFeatures			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_FEATURE", ())
		szCatNaturalWonders		= TRNSLTR.getText("TXT_KEY_NATURAL_WONDERS", ())
		szCatBonusesMap			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BONUS_MAP", ())
		szCatBonusesMan			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BONUS_MANDFACTURED", ())
		szCatBonusesCult		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BONUS_CULTURE", ())
		szCatBonusesTech		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BONUS_GENMOD", ())
		szCatBonusesWonder		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_BONUS_WONDER", ())
		szCatImprovements		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_IMPROVEMENT", ())
		szCatRoutes				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_ROUTES", ())
		szCatCivs				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_CIV", ())
		szCatLeaders			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_LEADER", ())
		szCatTraits				= TRNSLTR.getText("TXT_KEY_PEDIA_TRAITS", ())
		szCatCivics				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_CIVIC", ())
		szCatReligions			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_RELIGION", ())
		szCatHeritage			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_HERITAGE", ())
		szCatCorporations		= TRNSLTR.getText("TXT_KEY_CONCEPT_CORPORATIONS", ())
		szCatConcepts			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_CONCEPT", ())
		szCatConceptsNew		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_CONCEPT_NEW", ())
		szCatHints				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_HINTS", ())
		szCatShortcuts			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SHORTCUTS", ())
		szCatStrategy			= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_STRATEGY", ())
		szCatEras				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_ERAS", ())
		szCatBuilds				= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_WORKER_BUILDS", ())
		szCatGroupWonders		= TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_GROUP_WONDERS", ())

		self.szCatAllEras = szCatAllEras = TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_ALL_ERAS", ())
		self.szChronology = szChronology = TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_CHRONOLOGY", ())
		# Category List.
		categoryList = [
			["GOLDENAGE",	szCatConcepts],
			["BEAKER",		szCatTechs],
			["STRENGHT",	szCatUnits],
			["MOVES",		szCatSpecialUnits],
			["SILVERSTAR",	szCatPromotions],
			["PRODUCTION",	szCatBuildings],
			["POWER",		TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SPECIAL_BUILDINGS", ())],
			["TRADE",		TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_RESOURCES", ())],
			["MAP",			TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_LANDSCAPE", ())],
			["OCCUPATION",	TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_LEADERSHIP", ())],
			["GREATPEOPLE",	TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_SPECIAL", ())],
			["DEF_PACT",	TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_UPGRADE_TREES", ())],
		]
		# Category enumeration.
		self.PEDIA_BUILDINGS_0	= PEDIA_BUILDINGS_0	= -3
		self.PEDIA_UNITS_0		= PEDIA_UNITS_0		= -2
		self.PEDIA_MAIN			= PEDIA_MAIN		= -1
		self.PEDIA_CONCEPTS		= PEDIA_CONCEPTS	= 0
		self.PEDIA_TECHS		= PEDIA_TECHS		= 1
		self.PEDIA_UNITS_1		= PEDIA_UNITS_1		= 2
		self.PEDIA_UNITS_2		= PEDIA_UNITS_2		= 3
		self.PEDIA_PROMOTIONS	= PEDIA_PROMOTIONS	= 4
		self.PEDIA_BUILDINGS_1	= PEDIA_BUILDINGS_1	= 5
		self.PEDIA_BUILDINGS_2	= PEDIA_BUILDINGS_2	= 6
		self.PEDIA_BONUSES		= PEDIA_BONUSES		= 7
		self.PEDIA_LANDSCAPE	= PEDIA_LANDSCAPE	= 8
		self.PEDIA_LEADERSHIP	= PEDIA_LEADERSHIP	= 9
		self.PEDIA_SPECIAL		= PEDIA_SPECIAL		= 10
		self.PEDIA_UPG_TREES	= PEDIA_UPG_TREES	= 11
		# Sub-Category lists.
		self.szEraList = szEraList = []
		self.iNumEras = iNumEras = GC.getNumEraInfos()
		szEraList.append(szCatAllEras)
		for i in xrange(iNumEras):
			szEraList.append(INFO.getDescription("C2C_ERA_", i))
		szTechSubCatList = list(szEraList)
		szTechSubCatList.append(szChronology)
		PEDIA_SUB_CONCEPTS 		= [szCatConcepts, szCatConceptsNew, szCatStrategy, szCatShortcuts, szCatHints, szCatEras]
		PEDIA_SUB_UNITS_2		= [szCatWorldUnits, szCatCulturalUnits, szCatAnimals, szCatSpreadUnits, szCatMiscUnits]
		PEDIA_SUB_PROMOTIONS	= [szCatPromotions, szCatBuildUp, szCatStatus]
		PEDIA_SUB_BUILDINGS_2	= [szCatNationalWonders, szCatGreatWonders, szCatGroupWonders, szCatSpecialBuildings, szCatC2CCutures, szCatRelBuildings, szCatAniBuildings, szCatSpaceBuildings]
		PEDIA_SUB_BONUSES		= [szCatBonusesMap, szCatBonusesMan, szCatBonusesCult, szCatBonusesTech, szCatBonusesWonder]
		PEDIA_SUB_LANDSCAPE		= [szCatTerrains, szCatFeatures, szCatNaturalWonders, szCatImprovements, szCatRoutes]
		PEDIA_SUB_LEADERSHIP	= [szCatCivs, szCatLeaders, szCatTraits, szCatCivics, szCatReligions, szCatHeritage]
		PEDIA_SUB_SPECIAL		= [szCatUnitCombat, szCatSpecialists, szCatProjects, szCatCorporations, szCatBuilds]
		PEDIA_SUB_UPG_TREES		= [szCatBuildingTree, szCatUnitTree, szCatPromotionTree]
		# Map sub-categories to the main categories.
		self.mapSubCat = {
			PEDIA_CONCEPTS		: PEDIA_SUB_CONCEPTS,
			PEDIA_TECHS			: szTechSubCatList,
			PEDIA_UNITS_1		: szEraList,
			PEDIA_UNITS_2		: PEDIA_SUB_UNITS_2,
			PEDIA_PROMOTIONS	: PEDIA_SUB_PROMOTIONS,
			PEDIA_BUILDINGS_1	: szEraList,
			PEDIA_BUILDINGS_2	: PEDIA_SUB_BUILDINGS_2,
			PEDIA_BONUSES		: PEDIA_SUB_BONUSES,
			PEDIA_LANDSCAPE		: PEDIA_SUB_LANDSCAPE,
			PEDIA_LEADERSHIP	: PEDIA_SUB_LEADERSHIP,
			PEDIA_SPECIAL		: PEDIA_SUB_SPECIAL,
			PEDIA_UPG_TREES		: PEDIA_SUB_UPG_TREES,
		}
		import PediaTech
		import PediaUnit
		import SevoPediaRoute
		import PediaBuilding
		import PediaPromotion
		import PediaUnitCombat
		import PediaBonus
		import PediaFeature
		import PediaImprovement
		import PediaCivic
		import PediaCivilization
		import PediaLeader
		import SevoPediaSpecialist
		import PediaProject
		import PediaReligion
		import PediaHeritage
		import PediaCorporation
		import PediaBuild
		import PediaTrait
		import PediaTerrain
		import PediaEra

		self.mapScreenFunctions = {
			PEDIA_TECHS				: PediaTech.PediaTech(self, H_BOT_ROW),
			PEDIA_UNITS_0			: PediaUnit.PediaUnit(self, H_BOT_ROW),
			szCatUnitCombat			: PediaUnitCombat.PediaUnitCombat(self, H_BOT_ROW),
			PEDIA_PROMOTIONS		: PediaPromotion.PediaPromotion(self, H_BOT_ROW),
			PEDIA_BUILDINGS_0		: PediaBuilding.PediaBuilding(self, H_BOT_ROW),
			szCatProjects			: PediaProject.PediaProject(self, H_BOT_ROW),
			szCatSpecialists		: SevoPediaSpecialist.SevoPediaSpecialist(self),
			szCatTerrains			: PediaTerrain.Page(self, H_BOT_ROW),
			"Feature"				: PediaFeature.PediaFeature(self, H_BOT_ROW),
			szCatImprovements		: PediaImprovement.PediaImprovement(self, H_BOT_ROW),
			szCatCivs				: PediaCivilization.PediaCivilization(self, H_BOT_ROW),
			szCatLeaders			: PediaLeader.PediaLeader(self, H_BOT_ROW),
			szCatTraits				: PediaTrait.Page(self, H_BOT_ROW),
			szCatCivics				: PediaCivic.Page(self, H_BOT_ROW),
			szCatReligions			: PediaReligion.Page(self, H_BOT_ROW),
			szCatHeritage			: PediaHeritage.Page(self, H_BOT_ROW),
			szCatCorporations		: PediaCorporation.PediaCorporation(self, H_BOT_ROW),
			szCatRoutes				: SevoPediaRoute.SevoPediaRoute(self),
			PEDIA_BONUSES			: PediaBonus.PediaBonus(self, H_BOT_ROW),
			szCatBuilds				: PediaBuild.PediaBuild(self, H_BOT_ROW),
			szCatEras				: PediaEra.PediaEra(self, H_BOT_ROW)
		}
		self.mapListGenerators = {
			szCatConcepts			: self.placeConcepts,
			szCatConceptsNew		: self.placeBTSConcepts,
			szCatHints				: self.placeHints,
			szCatShortcuts	 	 	: self.placeShortcuts,
			szCatStrategy	  		: self.placeStrategy,
			PEDIA_TECHS				: self.placeTechs,
			PEDIA_UNITS_1			: self.placeUnits,
			szCatWorldUnits			: self.placeWorldUnits,
			szCatAnimals			: self.placeAnimals,
			szCatCulturalUnits		: self.placeCulturalUnits,
			szCatSpreadUnits		: self.placeSpreadUnits,
			szCatMiscUnits			: self.placeMiscUnits,
			szCatUnitCombat			: self.placeUnitCombats,
			szCatPromotions			: self.placePromotions,
			szCatBuildUp			: self.placeBuildUp,
			szCatStatus				: self.placeStatus,
			PEDIA_BUILDINGS_1		: self.placeBuildings,
			szCatNationalWonders	: self.placeNationalWonders,
			szCatGreatWonders		: self.placeGreatWonders,
			szCatGroupWonders		: self.placeGroupWonders,
			szCatC2CCutures			: self.placeCulBuildings,
			szCatSpecialBuildings	: self.placeSpeBuildings,
			szCatRelBuildings		: self.placeRelBuildings,
			szCatAniBuildings		: self.placeAniBuildings,
			szCatSpaceBuildings		: self.placeSpaceBuildings,
			szCatProjects			: self.placeProjects,
			szCatSpecialists		: self.placeSpecialists,
			szCatTerrains			: self.placeTerrains,
			szCatFeatures			: self.placeFeatures,
			szCatNaturalWonders		: self.placeNaturalWonders,
			szCatBonusesMap			: self.placeMapBonuses,
			szCatImprovements		: self.placeImprovements,
			szCatCivs				: self.placeCivs,
			szCatLeaders			: self.placeLeaders,
			szCatTraits				: self.placeTraits,
			szCatCivics				: self.placeCivics,
			szCatReligions			: self.placeReligions,
			szCatHeritage			: self.placeHeritage,
			szCatCorporations		: self.placeCorporations,
			szCatRoutes				: self.placeRoutes,
			szCatBonusesMan			: self.placeManufacturedBonuses,
			szCatBonusesCult		: self.placeCulturalBonuses,
			szCatBonusesTech		: self.placeTechnoculturalBonuses,
			szCatBonusesWonder		: self.placeWonderBonuses,
			szCatBuildingTree		: self.placeBuildingTree,
			szCatUnitTree			: self.placeUnitTree,
			szCatPromotionTree		: self.placePromotionTree,
			szCatBuilds				: self.placeBuilds,
			szCatEras				: self.placeEras
		}

		# Declare misc variables
		iPlayer = G.getActivePlayer()
		if iPlayer > -1:
			self.CyPlayer = GC.getPlayer(iPlayer)
		else:
			self.CyPlayer = None
		self.nWidgetCount = 0
		self.pediaFuture = []
		self.SECTION = [-1, -1]
		self.nIndexLists = [0]
		self.inPage = None
		self.aList = []
		self.iGroupCategory = None

		eWidGen		= WidgetTypes.WIDGET_GENERAL
		eWidMain	= WidgetTypes.WIDGET_PEDIA_MAIN
		iFontTitle	= FontTypes.TITLE_FONT

		HEAD_TEXT		= szfontEdge + TRNSLTR.getText("TXT_KEY_MAIN_MENU_CIVILOPEDIA", ())
		TOC_TEXT		= szfontEdge + TRNSLTR.getText("TXT_KEY_PEDIA_SCREEN_CONTENTS", ())
		INDEX_TEXT		= szfontEdge + TRNSLTR.getText("TXT_KEY_PEDIA_SCREEN_INDEX", ())
		BACK_TEXT		= szfontEdge + TRNSLTR.getText("TXT_KEY_PEDIA_SCREEN_BACK", ())
		NEXT_TEXT		= szfontEdge + TRNSLTR.getText("TXT_KEY_PEDIA_SCREEN_FORWARD", ())
		EXIT_TEXT		= szfontEdge + TRNSLTR.getText("TXT_WORD_EXIT", ())
		# Build Pedia screen.
		screen.setRenderInterfaceOnly(True)
		screen.setScreenGroup(1)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

		screen.addDDSGFC("PediaMainBackground", CyArtFileMgr().getInterfaceArtInfo("SCREEN_BG_OPAQUE").getPath(), 0, 0, xRes, yRes, eWidGen, -1, -1)
		screen.addPanel("PediaMainTopPanel", "", "", True, False, 0, 0, xRes, H_EDGE_PANEL, PanelStyles.PANEL_STYLE_TOPBAR)
		screen.addPanel("PediaMainBottomPanel", "", "", True, False, 0, yRes - H_EDGE_PANEL, xRes, H_EDGE_PANEL, PanelStyles.PANEL_STYLE_BOTTOMBAR)

		screen.setText("PediaMain|Header", "", HEAD_TEXT, 1<<0, 16, 0, 0, iFontTitle, eWidGen, 1, 1)
		screen.setText("PediaMain|Contents", "", TOC_TEXT, 1<<0, 16, 0, 0, iFontTitle, eWidGen, -1, -1)
		screen.setText("PediaMain|Index", "", INDEX_TEXT, 1<<0, 16, Y_BOT_TEXT, 0, iFontTitle, eWidGen, -1, -1)
		screen.setText("PediaMainBack", "", BACK_TEXT, 1<<1, xRes / 2 - 8, Y_BOT_TEXT, 0, iFontTitle, WidgetTypes.WIDGET_PEDIA_BACK, 1, -1)
		screen.setText("PediaMainForward", "", NEXT_TEXT, 1<<0, xRes / 2 + 8, Y_BOT_TEXT, 0, iFontTitle, WidgetTypes.WIDGET_PEDIA_FORWARD, 1, -1)
		screen.setText("PediaMain|Exit", "", EXIT_TEXT, 1<<1, xRes - 16, 0, 0, iFontTitle, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		# Draw Category List
		print "Drawing category list"
		enumTableStd = TableStyles.TABLE_STYLE_STANDARD
		CAT_LIST_ID = "PediaMainCategoryList"
		screen.addListBoxGFC(CAT_LIST_ID, "", 0, Y_PEDIA_PAGE, W_CATEGORIES, H_CATEGORIES, enumTableStd)
		screen.enableSelect(CAT_LIST_ID, True)
		screen.setStyle(CAT_LIST_ID, "Table_StandardCiv_Style")
		for i, aList in enumerate(categoryList):
			graphic =  aFontList[3] + categoryGraphics[aList[0]]
			screen.appendListBoxStringNoUpdate(CAT_LIST_ID, graphic + aList[1], eWidMain, i, 0, 1<<0)
		screen.updateListBox(CAT_LIST_ID)
		# Define Sub-Category List
		SUBCAT_LIST_ID = "PediaMainSubCatList"
		screen.addListBoxGFC(SUBCAT_LIST_ID, "", 0, Y_SUBCATEGORIES, W_CATEGORIES, H_CATEGORIES, enumTableStd)
		screen.setStyle(SUBCAT_LIST_ID, "Table_StandardCiv_Style")

	# Called a lot internally, but can also be called externally when you open the pedia indirectly.
	def pediaJump(self, iCategory, szSubCat, iObjectType, bRemoveFwdList = True):
		if self.bNotPedia:
			self.startPedia()
			self.bNotPedia = False
			self.pediaHistory.append((iCategory, szSubCat, iObjectType))
		elif self.inPage == (iCategory, szSubCat, iObjectType):
			return
		print "Pedia Jump"
		if bRemoveFwdList:
			self.pediaFuture = []
		# Evaluate Category.
		screen = self.screen()
		if iCategory == self.PEDIA_MAIN:
			self.inPage = (iCategory, szSubCat, iObjectType)
			self.aList = []
			self.iListIndex = -1
			if iObjectType > 999:
				print "Sub Cat. link %d" % iObjectType
				iCategory = self.SECTION[0]
				aSubCatList = self.mapSubCat[iCategory]
				szSubCat = aSubCatList[iObjectType - 1000]
			else:
				if not szSubCat:
					self.welcomeMessage(screen)
				print "Category link %d" % iObjectType
				screen.deleteWidget("PediaMainItemList")
				screen.deleteWidget("PediaMainItemListCount")
				iCategory = iObjectType
				aSubCatList = self.mapSubCat.get(iCategory)
				szSubCat = "NONE"
			self.showContents(screen, iCategory, aSubCatList, szSubCat)
			return
		print ("iObjectType %d" % iObjectType)
		self.inPage = (iCategory, szSubCat, iObjectType)
		bConcept = False
		bFuncByCat = False
		bFuncByGroupCat = False
		bFuncBySubCatSimple = False
		self.iGroupCategory = None

		if iCategory == self.PEDIA_TECHS:
			bFuncByCat = True
			if not szSubCat:
				if iObjectType == -1: # When right clicking the research bar in game.
					iObjectType = self.CyPlayer.getCurrentResearch()
				if self.SECTION == [iCategory, self.szCatAllEras] or self.SECTION == [iCategory, self.szChronology]:
					szSubCat = self.SECTION[1]
				else:
					iEra = INFO.getIntrinsic("TECH_", iObjectType, IntrinsicSlot.PYINT_ERA) + 1
					szSubCat = self.mapSubCat.get(iCategory)[iEra]
				print "Selected: %s", INFO.getDescription("TECH_", iObjectType)

		elif iCategory == self.PEDIA_UNITS_0:
			bFuncByGroupCat = True
			self.iGroupCategory = iGroupCategory = iCategory
			if INFO.isAnimal(iObjectType):
				iCategory = self.PEDIA_UNITS_2
				szSubCat = self.mapSubCat.get(iCategory)[2]
			elif INFO.canSpreadReligion(iObjectType):
				iCategory = self.PEDIA_UNITS_2
				szSubCat = self.mapSubCat.get(iCategory)[3]
			elif INFO.getIntrinsic("UNIT_", iObjectType, IntrinsicSlot.PYINT_COST) <= 0:
				iCategory = self.PEDIA_UNITS_2
				szSubCat = self.mapSubCat.get(iCategory)[4]
			elif INFO.isWorldUnit(iObjectType):
				iCategory = self.PEDIA_UNITS_2
				szSubCat = self.mapSubCat.get(iCategory)[0]
			elif self.isCultureUnit(iObjectType):
				iCategory = self.PEDIA_UNITS_2
				szSubCat = self.mapSubCat.get(iCategory)[1]
			else:
				iCategory = self.PEDIA_UNITS_1
				if self.SECTION == [iCategory, self.szCatAllEras]:
					szSubCat = self.SECTION[1]
				else:
					iEra = self.getUnitEra(iObjectType)
					szSubCat = self.mapSubCat.get(iCategory)[iEra]
			print "Selected: %s", INFO.getDescription("UNIT_", iObjectType)

		elif iCategory in (self.PEDIA_UNITS_1, self.PEDIA_UNITS_2):
			bFuncByGroupCat = True
			self.iGroupCategory = iGroupCategory = self.PEDIA_UNITS_0

		elif iCategory == self.PEDIA_PROMOTIONS:
			bFuncByCat = True
			if not szSubCat:
				iPromotionType = self.getPromotionType(iObjectType)
				szSubCat = self.mapSubCat.get(iCategory)[iPromotionType]
				print "Selected: %s", INFO.getDescription("PROMOTION_", iObjectType)

		elif iCategory == self.PEDIA_BUILDINGS_0:
			bFuncByGroupCat = True
			self.iGroupCategory = iGroupCategory = iCategory
			szCategory = INFO.getPediaCategory("BUILDING_", iObjectType)
			if szCategory in self.PEDIA_BUILDING_BUCKETS:
				iCategory = self.PEDIA_BUILDINGS_2
				szSubCat = self.mapSubCat.get(iCategory)[self.PEDIA_BUILDING_BUCKETS.index(szCategory)]
			else:
				iCategory = self.PEDIA_BUILDINGS_1
				if self.SECTION == [iCategory, self.szCatAllEras]:
					szSubCat = self.SECTION[1]
				else:
					iEra = self.getBuildingEra(iObjectType)
					szSubCat = self.mapSubCat.get(iCategory)[iEra]
			print "Selected: %s", INFO.getDescription("BUILDING_", iObjectType)

		elif iCategory in (self.PEDIA_BUILDINGS_1, self.PEDIA_BUILDINGS_2):
			bFuncByGroupCat = True
			self.iGroupCategory = iGroupCategory = self.PEDIA_BUILDINGS_0

		elif iCategory == self.PEDIA_BONUSES:
			bFuncByCat = True
			if not szSubCat:
				iBonusClass = INFO.getIntrinsic("BONUS_", iObjectType, IntrinsicSlot.PYINT_BONUS_CLASS)
				if INFO.getIntrinsic("BONUS_", iObjectType, IntrinsicSlot.PYINT_IS_MAP_BONUS):
					## Map resource
					szSubCat = self.mapSubCat.get(iCategory)[0]
				elif iBonusClass == GC.getInfoTypeForString("BONUSCLASS_CULTURE"):
					## Culture resource
					szSubCat = self.mapSubCat.get(iCategory)[2]
				elif iBonusClass == GC.getInfoTypeForString("BONUSCLASS_GENMODS"):
					## Genmod resources
					szSubCat = self.mapSubCat.get(iCategory)[3]
				elif iBonusClass == GC.getInfoTypeForString("BONUSCLASS_WONDER"):
					## Wonder resource
					szSubCat = self.mapSubCat.get(iCategory)[4]
				else:
					## Manufactured resource
					szSubCat = self.mapSubCat.get(iCategory)[1]
				print "Selected: %s", INFO.getDescription("BONUS_", iObjectType)

		elif iCategory == self.PEDIA_LANDSCAPE:
			if szSubCat == "Terrain":
				szSubCat = self.mapSubCat.get(iCategory)[0]
			elif szSubCat == "Feature":
				szSubCatSimple = szSubCat
				bFuncBySubCatSimple = True
				if INFO.getType("FEATURE_", iObjectType).find("FEATURE_PLATY_") == -1:
					szSubCat = self.mapSubCat.get(iCategory)[1]
				else: ## Natural Wonder
					szSubCat = self.mapSubCat.get(iCategory)[2]
			elif szSubCat == "Improvement":
				szSubCat = self.mapSubCat.get(iCategory)[3]
			elif szSubCat == "Route":
				szSubCat = self.mapSubCat.get(iCategory)[4]

		elif iCategory == self.PEDIA_LEADERSHIP:
			if szSubCat == "Civ":
				szSubCat = self.mapSubCat.get(iCategory)[0]
			elif szSubCat == "Leader":
				szSubCat = self.mapSubCat.get(iCategory)[1]
			elif szSubCat == "Trait":
				szSubCat = self.mapSubCat.get(iCategory)[2]
			elif szSubCat == "Civic":
				szSubCat = self.mapSubCat.get(iCategory)[3]
			elif szSubCat == "Religion":
				szSubCat = self.mapSubCat.get(iCategory)[4]
			elif szSubCat == "Heritage":
				szSubCat = self.mapSubCat.get(iCategory)[5]

		elif iCategory == self.PEDIA_SPECIAL:
			if szSubCat == "UnitCombat":
				szSubCat = self.mapSubCat.get(iCategory)[0]
			elif szSubCat == "Specialist":
				szSubCat = self.mapSubCat.get(iCategory)[1]
			elif szSubCat == "Project":
				szSubCat = self.mapSubCat.get(iCategory)[2]
			elif szSubCat == "Corporation":
				szSubCat = self.mapSubCat.get(iCategory)[3]
			elif szSubCat == "Build":
				szSubCat = self.mapSubCat.get(iCategory)[4]

		elif iCategory == self.PEDIA_CONCEPTS:
			if szSubCat != "Eras":
				bConcept = True
				bNewConcept = False
				if szSubCat == "NEW":
					szSubCatSimple = szSubCat
					bFuncBySubCatSimple = True
					bNewConcept = True
					szInfoType = INFO.getType("NEWCONCEPT_", iObjectType)
					print "Selected concept type %s" %(szInfoType)
					if szInfoType.find("STRATEGY") != -1:
						szSubCat = self.mapSubCat.get(iCategory)[2]
					elif szInfoType.find("SHORTCUTS") != -1:
						szSubCat = self.mapSubCat.get(iCategory)[3]
					else:
						szSubCat = self.mapSubCat.get(iCategory)[1]
				else:
					szSubCat = self.mapSubCat.get(iCategory)[0]
					print "Selected normal concept type"
		if bFuncBySubCatSimple:
			self.pediaHistory.append((iCategory, szSubCatSimple, iObjectType))
		else:
			self.pediaHistory.append((iCategory, szSubCat, iObjectType))
		aSubCatList = self.mapSubCat[iCategory]
		self.showContents(screen, iCategory, aSubCatList, szSubCat)

		szItemList = "PediaMainItemList"
		screen.enableSelect(szItemList, True)
		for i, item in enumerate(self.aList):
			if item[1] == iObjectType:
				screen.setSelectedListBoxStringGFC(szItemList, i)
				self.iListIndex = i

		if bConcept:
			panelName = self.getNextWidgetName()
			screen.addPanel(panelName, "", "", True, True, self.X_PEDIA_PAGE, self.Y_PEDIA_PAGE, self.W_PEDIA_PAGE, self.H_PEDIA_PAGE, PanelStyles.PANEL_STYLE_BLUE50)
			if bNewConcept:
				szText = INFO.getCivilopedia("NEWCONCEPT_", iObjectType)
			else:
				szText = INFO.getCivilopedia("CONCEPT_", iObjectType)
			screen.attachMultilineText(panelName, "", szText, WidgetTypes.WIDGET_GENERAL, -1, -1, 1<<0)
		elif bFuncByGroupCat:
			self.mapScreenFunctions[iGroupCategory].interfaceScreen(iObjectType)
		elif bFuncByCat:
			self.mapScreenFunctions[iCategory].interfaceScreen(iObjectType)
		elif bFuncBySubCatSimple:
			self.mapScreenFunctions[szSubCatSimple].interfaceScreen(iObjectType)
		else:
			self.mapScreenFunctions[szSubCat].interfaceScreen(iObjectType)


	def showContents(self, screen, iCategory, aSubCatList, szSubCat):
		self.deleteAllWidgets()
		if self.bIndex:
			print "Showing category list"
			nIndexLists = self.nIndexLists[0]
			if nIndexLists:
				for i in xrange(nIndexLists):
					screen.hide("Index" + str(i))
				screen.hide("IndexLetters")
			screen.show("PediaMain|Header")
			screen.hide("PediaMain|Contents")
			screen.show("PediaMain|Index")
			screen.show("PediaMainBack")
			screen.show("PediaMainForward")
			screen.show("PediaMainCategoryList")
			screen.show("PediaMainSubCatList")
			screen.show("PediaMainItemList")
			screen.show("PediaMainItemListCount")
			self.bIndex = False
		bDrawSubCat = False
		bDrawItems = False
		if szSubCat == "NONE":
			bDrawSubCat = True
		elif self.SECTION != [iCategory, szSubCat]:
			if (self.SECTION[0] != iCategory) or (szSubCat == "NONE"):
				bDrawSubCat = True
			bDrawItems = True
		self.SECTION = [iCategory, szSubCat]
		SUBCAT_LIST_ID = "PediaMainSubCatList"
		if bDrawSubCat:
			print "Drawing sub-category list"
			screen.setSelectedListBoxStringGFC("PediaMainCategoryList", iCategory)
			screen.clearListBoxGFC(SUBCAT_LIST_ID)
			eWidMain = WidgetTypes.WIDGET_PEDIA_MAIN
		szfont3b = self.aFontList[3] + self.categoryGraphics["BULLET"]
		for i in xrange(len(aSubCatList)):
			if bDrawSubCat:
				screen.appendListBoxStringNoUpdate(SUBCAT_LIST_ID, szfont3b + aSubCatList[i], eWidMain, i + 1000, 0, 1<<0)
			if aSubCatList[i] == szSubCat:
				iSubCatIndex = i
		if bDrawSubCat:
			screen.updateListBox(SUBCAT_LIST_ID)
		if szSubCat == "NONE":
			screen.enableSelect(SUBCAT_LIST_ID, False)
		else:
			screen.enableSelect(SUBCAT_LIST_ID, True)
			screen.setSelectedListBoxStringGFC(SUBCAT_LIST_ID, iSubCatIndex)
		if bDrawItems:
			print "Drawing Item list"
			if szSubCat in self.szEraList or szSubCat == self.szChronology:
				self.mapListGenerators.get(iCategory)()
			else:
				self.mapListGenerators.get(szSubCat)()

	# Tech Lists
	def placeTechs(self):
		print "Creating item list for category: Technologies"
		self.aList = self.getSortedTechList()
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_TECH, "TECH_")

	def getSortedTechList(self):
		aList = []
		iCategory, szSubCat = self.SECTION
		aSubCatList = self.mapSubCat.get(iCategory)
		szChronology = self.szChronology
		if szSubCat == szChronology:
			aListDict = {}
			for i in xrange(GC.getNumTechInfos()):
				if INFO.getIntrinsic("TECH_", i, IntrinsicSlot.PYINT_IS_DISABLED):
					continue
				iX = INFO.getIntrinsic("TECH_", i, IntrinsicSlot.PYINT_GRID_X)
				iY = INFO.getIntrinsic("TECH_", i, IntrinsicSlot.PYINT_GRID_Y)
				aListDict[(iX, iY)] = (INFO.getDescription("TECH_", i), i)
				aList.append((iX, iY))
		else:
			if szSubCat == self.szCatAllEras:
				bAll = True
			else:
				bAll = False
			for i in xrange(GC.getNumTechInfos()):
				if INFO.getIntrinsic("TECH_", i, IntrinsicSlot.PYINT_IS_DISABLED):
					continue
				if bAll:
					aList.append((INFO.getDescription("TECH_", i), i))
				else:
					iEra = INFO.getIntrinsic("TECH_", i, IntrinsicSlot.PYINT_ERA) + 1
					if szSubCat == aSubCatList[iEra]:
						aList.append((INFO.getDescription("TECH_", i), i))
		if szSubCat == szChronology:
			aList.sort()
			for i in xrange(len(aList)):
				key = aList[i]
				aList[i] = aListDict[key]
		else:
			aList.sort()
		return aList

	# Unit Lists
	def placeUnits(self):
		print "Creating item list for category: Units"
		self.aList = self.getSortedUnitList(False, False, False, False, False)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, "UNIT_")

	def placeWorldUnits(self):
		print "Creating item list for category: Heroes"
		self.aList = self.getSortedUnitList(True, False, False, False, False)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, "UNIT_")

	def placeAnimals(self):
		print "Creating item list for category: Animals"
		self.aList = self.getSortedUnitList(False, True, False, False, False)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, "UNIT_")

	def placeCulturalUnits(self):
		print "Creating item list for category: Cultural"
		self.aList = self.getSortedUnitList(False, False, True, False, False)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, "UNIT_")

	def placeSpreadUnits(self):
		print "Creating item list for category: Corporate/Religion spreading Units"
		self.aList = self.getSortedUnitList(False, False, False, True, False)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, "UNIT_")

	def placeMiscUnits(self):
		print "Creating item list for category: Misc Units"
		self.aList = self.getSortedUnitList(False, False, False, False, True)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, "UNIT_")

	def getSortedUnitList(self, bWorld, bAnimals, bCultural, bSpread, bMisc):
		aList = []
		iCategory, szSubCat = self.SECTION
		aSubCatList = self.mapSubCat.get(iCategory)
		for kEntry in INFO.getIndex("UNIT_"):
			i = kEntry["id"]
			bValid = False
			if INFO.isAnimal(i):
				if not bAnimals:
					continue
				bValid = True
			elif INFO.canSpreadReligion(i):
				if not bSpread:
					continue
				bValid = True
			elif INFO.getIntrinsic("UNIT_", i, IntrinsicSlot.PYINT_COST) <= 0:
				if not bMisc:
					continue
				bValid = True
			elif INFO.isWorldUnit(i):
				if not bWorld:
					continue
				bValid = True
			elif self.isCultureUnit(i):
				if not bCultural:
					continue
				bValid = True
			elif bWorld or bAnimals or bCultural or bSpread or bMisc:
				continue
			elif szSubCat == self.szCatAllEras:
				bValid = True
			else:
				iEra = self.getUnitEra(i)
				if szSubCat == aSubCatList[iEra]:
					bValid = True
			if bValid:
				aList.append((kEntry["description"], i))
		aList.sort()
		return aList

	# Promotion Lists
	def placePromotions(self):
		print "Creating item list for category: Promotions"
		self.aList = self.getPromotionList(0)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_PROMOTION, "PROMOTION_")

	def placeBuildUp(self):
		print "Creating item list for category: Build Up"
		self.aList = self.getPromotionList(1)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_PROMOTION, "PROMOTION_")

	def placeStatus(self):
		print "Creating item list for category: Status"
		self.aList = self.getPromotionList(2)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_PROMOTION, "PROMOTION_")

	def getPromotionList(self, iType):
		aList = []
		for kEntry in INFO.getIndex("PROMOTION_"):
			if iType == self.getPromotionType(kEntry["id"]):
				aList.append((kEntry["description"], kEntry["id"]))
		aList.sort()
		return aList

	def getPromotionType(self, iPromotion):
		if INFO.isStatusPromotion(iPromotion):
			return 2
		if INFO.isBuildUpPromotion(iPromotion):
			return 1
		return 0

	# Building Lists
	def placeBuildings(self):
		print "Category: Buildings"
		self.aList = self.getBuildingList(-1)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeNationalWonders(self):
		print "Category: National Wonders"
		self.aList = self.getBuildingList(0)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeGreatWonders(self):
		print "Category: Great Wonders"
		self.aList = self.getBuildingList(1)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeGroupWonders(self):
		print "Category: Group Wonders"
		self.aList = self.getBuildingList(2)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeSpeBuildings(self):
		print "Category: Special Buildings"
		self.aList = self.getBuildingList(3)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeCulBuildings(self):
		print "Category: Culture Buildings"
		self.aList = self.getBuildingList(4)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeRelBuildings(self):
		print "Category: Religious Buildings"
		self.aList = self.getBuildingList(5)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeAniBuildings(self):
		print "Category: Animalistic Buildings"
		self.aList = self.getBuildingList(6)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	def placeSpaceBuildings(self):
		print "Category: Space Buildings"
		self.aList = self.getBuildingList(7)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BUILDING, "BUILDING_")

	#	The pedia BUCKET is authored data now (identity.pediaCategory, json.md §7), so this filters the index
	#	instead of re-deriving a taxonomy. ONE crossing for the registry, and the classifier that read seven
	#	legacy getters plus a substring match on the localized DISPLAY NAME is gone with it.
	PEDIA_BUILDING_BUCKETS = ["nationalWonder", "greatWonder", "groupWonder", "specialBuilding",
	                          "culture", "religious", "animalistic", "space"]

	def getBuildingList(self, iBuildingType):
		aList = []
		iCategory, szSubCat = self.SECTION
		aSubCatList = self.mapSubCat.get(iCategory)
		szWanted = ""
		if iBuildingType != -1:
			szWanted = self.PEDIA_BUILDING_BUCKETS[iBuildingType]
		for kEntry in INFO.getIndex("BUILDING_"):
			szCategory = kEntry["pediaCategory"]
			if szWanted:
				if szCategory != szWanted:
					continue
			else:
				#	The ORDINARY bucket is the absent one, sub-bucketed by era.
				if szCategory:
					continue
				if szSubCat != self.szCatAllEras:
					iEra = self.getBuildingEra(kEntry["id"])
					if szSubCat != aSubCatList[iEra]:
						continue
			aList.append((kEntry["description"], kEntry["id"]))
		aList.sort()
		return aList

	def placeUnitTree(self):
		print "Category: Unit Tree"
		screen = self.screen()
		screen.deleteWidget("PediaMainItemList")
		screen.deleteWidget("PediaMainItemListCount")
		UPGRADES_GRAPH_ID = self.getNextWidgetName()
		screen.addScrollPanel(UPGRADES_GRAPH_ID, u"", self.W_CATEGORIES, self.Y_PEDIA_PAGE - 13, self.xRes - self.W_CATEGORIES, self.H_MID_SECTION, PanelStyles.PANEL_STYLE_STANDARD)
		screen.setActivation(UPGRADES_GRAPH_ID, ActivationTypes.ACTIVATE_NORMAL)
		upgradesGraph = UnitUpgradesGraph.UnitUpgradesGraph(self, UPGRADES_GRAPH_ID)
		upgradesGraph.getGraph()
		upgradesGraph.drawGraph()

	def placePromotionTree(self):
		print "Category: Promotion Tree"
		screen = self.screen()
		screen.deleteWidget("PediaMainItemList")
		screen.deleteWidget("PediaMainItemListCount")
		UPGRADES_GRAPH_ID = self.getNextWidgetName()
		screen.addScrollPanel(UPGRADES_GRAPH_ID, u"", self.W_CATEGORIES, self.Y_PEDIA_PAGE - 13, self.xRes - self.W_CATEGORIES, self.H_MID_SECTION, PanelStyles.PANEL_STYLE_STANDARD)
		screen.setActivation(UPGRADES_GRAPH_ID, ActivationTypes.ACTIVATE_NORMAL)
		upgradesGraph = UnitUpgradesGraph.PromotionsGraph(self, UPGRADES_GRAPH_ID)
		upgradesGraph.getGraph()
		upgradesGraph.drawGraph()

	def placeBuildingTree(self):
		print "Category: Building Tree"
		screen = self.screen()
		screen.deleteWidget("PediaMainItemList")
		screen.deleteWidget("PediaMainItemListCount")
		UPGRADES_GRAPH_ID = self.getNextWidgetName()
		screen.addScrollPanel(UPGRADES_GRAPH_ID, u"", self.W_CATEGORIES, self.Y_PEDIA_PAGE - 13, self.xRes - self.W_CATEGORIES, self.H_MID_SECTION, PanelStyles.PANEL_STYLE_STANDARD)
		screen.setActivation(UPGRADES_GRAPH_ID, ActivationTypes.ACTIVATE_NORMAL)
		upgradesGraph = UnitUpgradesGraph.BuildingsGraph(self, UPGRADES_GRAPH_ID)
		upgradesGraph.getGraph()
		upgradesGraph.drawGraph()


	def placeUnitCombats(self):
		print "Category: Unit Combat Types"
		self.aList = self.getSortedList("UNITCOMBAT_")
		self.placeItems("UnitCombats", "UNITCOMBAT_")


	def placeProjects(self):
		print "Category: Projects"
		self.aList = self.getSortedList("PROJECT_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_PROJECT, "PROJECT_")

	def placeSpecialists(self):
		print "Category: Specialists"
		self.aList = self.getSortedList("SPECIALIST_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_SPECIALIST, "SPECIALIST_")


	def placeTerrains(self):
		print "Category: Terrains"
		self.aList = self.getSortedList("TERRAIN_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_TERRAIN, "TERRAIN_")

	def placeFeatures(self):
		print "Category: Features"
		self.aList = self.getFeatureList(False)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_FEATURE, "FEATURE_")

	def placeNaturalWonders(self):
		print "Category: Natural Wonders"
		self.aList = self.getFeatureList(True)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_FEATURE, "FEATURE_")

	def getFeatureList(self, bNaturalWonder):
		aList = []
		for kEntry in INFO.getIndex("FEATURE_"):
			if kEntry["graphicalOnly"]:
				continue
			if (kEntry["type"].find("_PLATY_") > -1) == bNaturalWonder:
				aList.append((kEntry["description"], kEntry["id"]))
		aList.sort()
		return aList


	def placeMapBonuses(self):
		print "Category: Map Bonuses"
		self.aList = self.getBonusList(0)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, "BONUS_")

	def placeManufacturedBonuses(self):
		print "Category: Other Bonuses"
		self.aList = self.getBonusList(1)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, "BONUS_")

	def placeCulturalBonuses(self):
		print "Category: Culture Bonuses"
		self.aList = self.getBonusList(2)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, "BONUS_")

	def placeTechnoculturalBonuses(self):
		print "Category: Genmod Bonuses"
		self.aList = self.getBonusList(3)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, "BONUS_")

	def placeWonderBonuses(self):
		print "Category: Wonder Bonuses"
		self.aList = self.getBonusList(4)
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, "BONUS_")

	def getBonusList(self, iType):
		aList = []
		BONUSCLASS_CULTURE = GC.getInfoTypeForString("BONUSCLASS_CULTURE")
		BONUSCLASS_GENMODS = GC.getInfoTypeForString("BONUSCLASS_GENMODS")
		BONUSCLASS_WONDER = GC.getInfoTypeForString("BONUSCLASS_WONDER")
		for kEntry in INFO.getIndex("BONUS_"):
			iBonus = kEntry["id"]
			szName = kEntry["description"]
			iBonusClass = INFO.getIntrinsic("BONUS_", iBonus, IntrinsicSlot.PYINT_BONUS_CLASS)
			if INFO.getIntrinsic("BONUS_", iBonus, IntrinsicSlot.PYINT_IS_MAP_BONUS): # A map resource
				if not iType:
					aList.append((szName, iBonus))
			elif BONUSCLASS_WONDER > -1 and iBonusClass == BONUSCLASS_WONDER:
				if iType == 4:
					aList.append((szName, iBonus))
			elif BONUSCLASS_GENMODS > -1 and iBonusClass == BONUSCLASS_GENMODS:
				if iType == 3:
					aList.append((szName, iBonus))
			elif BONUSCLASS_CULTURE > -1 and iBonusClass == BONUSCLASS_CULTURE:
				if iType == 2:
					aList.append((szName, iBonus))
			elif iType == 1:
				aList.append((szName, iBonus))
		aList.sort()
		return aList


	def placeImprovements(self):
		print "Category: Improvements"
		self.aList = self.getSortedList("IMPROVEMENT_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_IMPROVEMENT, "IMPROVEMENT_")

	def placeRoutes(self):
		print "Category: Routes"
		self.aList = self.getSortedList("ROUTE_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_ROUTE, "ROUTE_")

	def placeBuilds(self):
		print "Category: Worker Builds"
		self.aList = self.getSortedList("BUILD_")
		self.placeItems("Builds", "BUILD_")

	def placeCivs(self):
		print "Category: Civilizations"
		self.aList = self.getSortedList("CIVILIZATION_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_CIV, "CIVILIZATION_")

	def placeLeaders(self):
		print "Category: Leaders"
		self.aList = self.getSortedList("LEADER_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_LEADER, "LEADER_")

	def getLeaderList(self):
		return self.getSortedList("LEADER_")

	def placeTraits(self):
		print "Category: Traits"
		self.aList = self.getSortedList("TRAIT_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_TRAIT, "TRAIT_")

	def placeCivics(self):
		print "Category: Civics"
		self.aList = self.getSortedList("CIVIC_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_CIVIC, "CIVIC_")

	def placeReligions(self):
		print "Category: Religion"
		self.aList = self.getSortedList("RELIGION_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_RELIGION, "RELIGION_")

	def placeHeritage(self):
		print "Category: Heritage"
		self.aList = self.getSortedList("HERITAGE_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_HERITAGE, "HERITAGE_")

	def placeCorporations(self):
		print "Category: Corporations"
		self.aList = self.getSortedList("CORPORATION_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_JUMP_TO_CORPORATION, "CORPORATION_")

	def placeConcepts(self):
		print "Category: Concepts"
		self.aList = self.getSortedList("CONCEPT_")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_DESCRIPTION, "CONCEPT_")

	#	The NewConcept registry holds three display sets distinguished only by a marker in the TYPE key.
	#	szWanted "" is the ordinary set (neither marker); otherwise it selects that marker.
	#	⚠ The prefix is NEWCONCEPT_, never CONCEPT_: both registries author CONCEPT_* ids, so the authored
	#	token cannot tell them apart and routing on it answers from whichever the dispatch reached first.
	def getNewConceptList(self, szWanted):
		aList = []
		for kEntry in INFO.getIndex("NEWCONCEPT_"):
			szType = kEntry["type"]
			bShortcut = szType.find("SHORTCUTS") != -1
			bStrategy = szType.find("STRATEGY") != -1
			if szWanted == "SHORTCUTS":
				if not bShortcut:
					continue
			elif szWanted == "STRATEGY":
				if not bStrategy:
					continue
			elif bShortcut or bStrategy:
				continue
			aList.append((kEntry["description"], kEntry["id"]))
		aList.sort()
		return aList

	def placeBTSConcepts(self):
		print "Category: BTS Concepts"
		self.aList = self.getNewConceptList("")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_DESCRIPTION, "NEWCONCEPT_")

	def placeHints(self):
		self.aList = []
		self.iListIndex = -1
		print "Category: Hints"
		screen = self.screen()
		screen.deleteWidget("PediaMainItemList")
		screen.deleteWidget("PediaMainItemListCount")
		szHintBox = self.getNextWidgetName()
		screen.addListBoxGFC(szHintBox, "", self.W_CATEGORIES, self.Y_PEDIA_PAGE, self.W_ITEMS + self.W_PEDIA_PAGE, self.H_PEDIA_PAGE, TableStyles.TABLE_STYLE_STANDARD)
		screen.enableSelect(szHintBox, False)
		szHintsText = CyGameTextMgr().buildHintsList()
		hintText = szHintsText.split("\n")
		eWidGen	= WidgetTypes.WIDGET_GENERAL
		for hint in hintText:
			if len(hint) != 0:
				screen.appendListBoxStringNoUpdate(szHintBox, hint, eWidGen, -1, -1, 1<<0)
		screen.updateListBox(szHintBox)

	def placeEras(self):
		screen = self.screen()
		self.iListIndex = -1
		szItemList = "PediaMainItemList"
		screen.clearListBoxGFC(szItemList)
		screen.addListBoxGFC(szItemList, "", self.W_CATEGORIES, self.Y_PEDIA_PAGE, self.W_ITEMS, self.H_MID_SECTION, TableStyles.TABLE_STYLE_STANDARD)
		screen.setStyle(szItemList, "Table_StandardCiv_Style")
		screen.enableSelect(szItemList, False)
		eWidGen	= WidgetTypes.WIDGET_GENERAL

		aList = []
		aEras = INFO.getIndex("C2C_ERA_")
		screen.setLabel("PediaMainItemListCount", "", self.aFontList[0] + "# %d" % len(aEras), 1<<0, self.W_CATEGORIES, 2, 0, FontTypes.TITLE_FONT, eWidGen, 1, 1)
		for kEntry in aEras:
			szName = kEntry["description"]
			aList.append((szName, kEntry["id"]))
			screen.appendListBoxStringNoUpdate(szItemList, self.aFontList[4] + szName, eWidGen, 1, 1, 1<<0)

		screen.updateListBox(szItemList)
		self.aList = aList

	def placeShortcuts(self):
		print "Category: Shortcuts"
		self.aList = self.getNewConceptList("SHORTCUTS")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_DESCRIPTION, "NEWCONCEPT_")

	def placeStrategy(self):
		self.aList = self.getNewConceptList("STRATEGY")
		self.placeItems(WidgetTypes.WIDGET_PEDIA_DESCRIPTION, "NEWCONCEPT_")

	#	szPrefix names the registry the rows came from -- it supplies the art in ONE crossing, and it is what
	#	tells the two CONCEPT registries apart (they share an authored prefix, so nothing else can).
	def placeItems(self, widget, szPrefix):
		screen = self.screen()
		szItemList = "PediaMainItemList"
		screen.clearListBoxGFC(szItemList)

		sIcon = self.ICON_SIZE
		screen.addListBoxGFC(szItemList, "", self.W_CATEGORIES, self.Y_PEDIA_PAGE, self.W_ITEMS, self.H_MID_SECTION, TableStyles.TABLE_STYLE_STANDARD)
		screen.setStyle(szItemList, "Table_StandardCiv_Style")
		screen.enableSelect(szItemList, False)

		szFont3 = self.aFontList[4]
		iPedConcept = CivilopediaPageTypes.CIVILOPEDIA_PAGE_CONCEPT
		iPedConcNew = CivilopediaPageTypes.CIVILOPEDIA_PAGE_CONCEPT_NEW
		iWidDescrip = WidgetTypes.WIDGET_PEDIA_DESCRIPTION
		if widget == "Builds":
			bOffset = True
			widget = WidgetTypes.WIDGET_PEDIA_JUMP_TO_ROUTE
		elif widget == "UnitCombats":
			bOffset = True
			widget = WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT
		else:
			bOffset = False
		#	The registry's art, fetched ONCE for the whole list rather than per row: a boost::python call costs
		#	far more than the lookup inside it, and this redraws on every category click.
		mapButton = {}
		for kEntry in INFO.getIndex(szPrefix):
			mapButton[kEntry["id"]] = kEntry["button"]
		i = 0
		for item in self.aList:
			if szPrefix == "CONCEPT_":
				data1 = iPedConcept
				data2 = item[1]
				szName = szFont3 + item[0]
			elif widget == iWidDescrip:
				data1 = iPedConcNew
				data2 = item[1]
				szName = szFont3 + item[0]
			else:
				if bOffset:
					data1 = item[1] - 100000
				else:
					data1 = item[1]

				data2 = 0

				szName = '<img=%s size=%d></img> %s' %(mapButton.get(item[1], ""), sIcon, szFont3 + item[0])
			screen.appendListBoxStringNoUpdate(szItemList, szName, widget, data1, data2, 1<<0)
			i += 1
		screen.updateListBox(szItemList)
		screen.setLabel("PediaMainItemListCount", "", self.aFontList[0] + "# " + str(i), 1<<0, self.W_CATEGORIES, 2, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, 0, 0)

	# Support Functions
	def deleteAllWidgets(self):
		screen = self.screen()
		# Generic widgets
		iNumWidgets = self.nWidgetCount
		self.nWidgetCount = 0
		for i in xrange(iNumWidgets):
			screen.deleteWidget(self.getNextWidgetName())
		self.nWidgetCount = 0
		# Specific widgets
		for widget in self.aWidgetBucket:
			screen.deleteWidget(widget)
		self.aWidgetBucket = []

	def getNextWidgetName(self):
		szName = "PediaMainWidget" + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName


	#	ONE crossing for the whole registry, not one per entity: the per-type INDEX carries the description, the
	#	id and the art-only marker together, which is the shape every enumeration here actually asks for.
	def getSortedList(self, szPrefix):
		aList = []
		for kEntry in INFO.getIndex(szPrefix):
			#	An art-only entity carries no page to show, so it never enters a listing.
			if kEntry["graphicalOnly"]:
				continue
			aList.append((kEntry["description"], kEntry["id"]))
		aList.sort()
		return aList

	#	The pedia's era BAND for a building: the era of the LATEST tech that unlocks it, 1-based.
	#	⛔ It reads EDGEF_ENABLED_BY, which is the family that exists for exactly this question -- entities
	#	naming this building in their own `enables`. That is NOT the merged EDGEF_RELATED bucket, so an
	#	OBSOLETING tech can never band a building ([CvEdges.h]: ENABLED_BY exists to separate the two).
	#	It replaces a hand-walk of prereq/special-building/religion/folklore/GOM legs that reconstructed,
	#	one leg at a time, the single list the reverse pass already lands on the building.
	def getBuildingEra(self, iBuilding):
		iEra = 0
		for iTech in INFO.getEdgeIds("BUILDING_", iBuilding, EdgeFamily.EDGEF_ENABLED_BY, EdgeBucket.EDGEB_TECHS):
			iTechEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)
			if iTechEra > iEra:
				iEra = iTechEra
		#	A buildable building tied to no tech bands with the earliest era rather than dropping out of the list.
		if iEra == 0 and INFO.getIntrinsic("BUILDING_", iBuilding, IntrinsicSlot.PYINT_COST) > 0:
			return 1
		return iEra + 1

	#	Does this unit need a CULTURE-class resource. The unit's own bonus references answer it, so nothing
	#	sweeps the bonus registry. ⚠ EDGEF_RELATED is a MERGED bucket ([enabler.md] §2), so this reads as ANY
	#	referenced bonus rather than as the mandatory one -- safe here, because a superset only widens a
	#	display bucket, and never as a gate.
	def isCultureUnit(self, iUnit):
		iCultureClass = GC.getInfoTypeForString("BONUSCLASS_CULTURE")
		for iBonus in INFO.getEdgeIds("UNIT_", iUnit, EdgeFamily.EDGEF_RELATED, EdgeBucket.EDGEB_BONUSES):
			if INFO.getIntrinsic("BONUS_", iBonus, IntrinsicSlot.PYINT_BONUS_CLASS) == iCultureClass:
				return True
		return False

	#	The unit's era BAND, the twin of getBuildingEra above and read off the same unlock edge.
	def getUnitEra(self, iUnit):
		iEra = 0
		bTech = False
		for iTech in INFO.getEdgeIds("UNIT_", iUnit, EdgeFamily.EDGEF_ENABLED_BY, EdgeBucket.EDGEB_TECHS):
			bTech = True
			iTechEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)
			if iTechEra > iEra:
				iEra = iTechEra
		if bTech:
			return iEra + 1
		#	Untied to any tech: a buildable unit bands with the earliest era, a costless one ahead of it.
		if INFO.getIntrinsic("UNIT_", iUnit, IntrinsicSlot.PYINT_COST) < 1:
			return 0
		return 1

	# Interaction Functions
	def link(self, szLink):
		print "PediaMain.link"
		szLink = str(szLink[0])
		print "\tLink = '%s'" % szLink
		if self.bNotPedia:
			self.startPedia()
			self.bNotPedia = False

		aSplit = szLink.split('_')
		szPrefix = aSplit[0]
		if szPrefix == "PEDIA":
			print "(Sub-)Category Link"
			if szLink == "PEDIA_MAIN_TECH":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_TECHS)
			elif szLink == "PEDIA_MAIN_UNIT":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_UNITS_1)
			elif szLink == "PEDIA_MAIN_UNIT_GROUP":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_SPECIAL)
			elif szLink == "PEDIA_MAIN_PROMOTION":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_PROMOTIONS)
			elif szLink == "PEDIA_MAIN_BUILDING":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_BUILDINGS_1)
			elif szLink == "PEDIA_MAIN_PROJECT":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_SPECIAL)
			elif szLink == "PEDIA_MAIN_SPECIALIST":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_SPECIAL)
			elif szLink == "PEDIA_MAIN_TERRAIN":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LANDSCAPE)
			elif szLink == "PEDIA_MAIN_FEATURE":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LANDSCAPE)
			elif szLink == "PEDIA_MAIN_BONUS":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_BONUSES)
			elif szLink == "PEDIA_MAIN_IMPROVEMENT":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LANDSCAPE)
			elif szLink == "PEDIA_MAIN_CIV":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LEADERSHIP)
			elif szLink == "PEDIA_MAIN_LEADER":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LEADERSHIP)
			elif szLink == "PEDIA_MAIN_TRAIT":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LEADERSHIP)
			elif szLink == "PEDIA_MAIN_CIVIC":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LEADERSHIP)
			elif szLink == "PEDIA_MAIN_RELIGION":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_LEADERSHIP)
			elif szLink == "PEDIA_MAIN_CONCEPT":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_CONCEPTS)
			elif szLink == "PEDIA_MAIN_HINTS":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_CONCEPTS)
			elif szLink == "PEDIA_MAIN_SHORTCUTS":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_CONCEPTS)
			elif szLink == "PEDIA_MAIN_STRATEGY":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_CONCEPTS)
			elif szLink == "PEDIA_ROUTES":
				return self.pediaJump(self.PEDIA_MAIN, "", self.PEDIA_SPECIAL)

		elif len(aSplit) > 1:
			iType = GC.getInfoTypeForString(szLink)
			if iType > -1:
				if szPrefix == "TECH":
					return self.pediaJump(self.PEDIA_TECHS, "", iType)
				elif szPrefix == "UNIT":
					return self.pediaJump(self.PEDIA_UNITS_0, "", iType)
				elif szPrefix == "UNITCOMBAT":
					return self.pediaJump(self.PEDIA_SPECIAL, "UnitCombat", iType)
				elif szPrefix == "PROMOTION":
					return self.pediaJump(self.PEDIA_PROMOTIONS, "", iType)
				elif szPrefix == "BUILDING":
					return self.pediaJump(self.PEDIA_BUILDINGS_0, "", iType)
				elif szPrefix == "PROJECT":
					return self.pediaJump(self.PEDIA_SPECIAL, "Project", iType)
				elif szPrefix == "SPECIALIST":
					return self.pediaJump(self.PEDIA_SPECIAL, "Specialist", iType)
				elif szPrefix == "TERRAIN":
					return self.pediaJump(self.PEDIA_LANDSCAPE, "Terrain", iType)
				elif szPrefix == "FEATURE":
					return self.pediaJump(self.PEDIA_LANDSCAPE, "Feature", iType)
				elif szPrefix == "ROUTE":
					return self.pediaJump(self.PEDIA_LANDSCAPE, "Route", iType)
				elif szPrefix == "BONUS":
					return self.pediaJump(self.PEDIA_BONUSES, "", iType)
				elif szPrefix == "IMPROVEMENT":
					return self.pediaJump(self.PEDIA_LANDSCAPE, "Improvement", iType)
				elif szPrefix == "CIVILIZATION":
					return self.pediaJump(self.PEDIA_LEADERSHIP, "Civ", iType)
				elif szPrefix == "LEADER":
					return self.pediaJump(self.PEDIA_LEADERSHIP, "Leader", iType)
				elif szPrefix == "CIVIC":
					return self.pediaJump(self.PEDIA_LEADERSHIP, "Civic", iType)
				elif szPrefix == "RELIGION":
					return self.pediaJump(self.PEDIA_LEADERSHIP, "Religion", iType)
				elif szPrefix == "HERITAGE":
					return self.pediaJump(self.PEDIA_LEADERSHIP, "Heritage", iType)
				elif szPrefix == "CORPORATION":
					return self.pediaJump(self.PEDIA_SPECIAL, "Corporation", iType)
				elif szPrefix == "TRAIT":
					return self.pediaJump(self.PEDIA_LEADERSHIP, "Trait", iType)
				elif szPrefix == "BUILD":
					return self.pediaJump(self.PEDIA_SPECIAL, "Build", iType)
				elif szPrefix == "CONCEPT":
					if INFO.getType("CONCEPT_", iType) == szLink:
						return self.pediaJump(self.PEDIA_CONCEPTS, "", iType)
					if INFO.getType("NEWCONCEPT_", iType) == szLink:
						return self.pediaJump(self.PEDIA_CONCEPTS, "NEW", iType)
				elif aSplit[1] == "ERA":
					return self.pediaJump(self.PEDIA_CONCEPTS, "Eras", iType)

		print "[ERROR] - Invalid Link"

	def back(self):
		if len(self.pediaHistory) > 2:
			self.pediaFuture.append(self.pediaHistory.pop())
			current = self.pediaHistory.pop()
			self.pediaJump(current[0], current[1], current[2], False)

	def forward(self):
		if self.pediaFuture:
			current = self.pediaFuture.pop()
			self.pediaJump(current[0], current[1], current[2], False)

	def welcomeMessage(self, screen, bCredit = ""):
		if bCredit:
			szText = "Created by Kristoffer E.H.-L. \n\t AKA: Toffer90"
			uFont = self.aFontList[2]
		else:
			szText = TRNSLTR.getText("TXT_KEY_PEDIA_WELCOME_MESSAGE", ())
			uFont = self.aFontList[4]
		x = int(0.3 * self.xRes)
		y = int(0.4 * self.yRes)
		self.tooltip.handle(screen, szText, x, y, uFont)

	def handleInput(self, inputClass):
		if self.bNotPedia: return

		iCode	= inputClass.eNotifyCode
		if iCode == 19: # Movie Done
			return
		iData	= inputClass.iData
		ID		= inputClass.iItemID
		NAME	= inputClass.szFunctionName
		iData1	= inputClass.iData1

		szSplit = NAME.split("|")

		bAlt, bCtrl, bShift = self.InputData.getModifierKeys()

		szFlag = HandleInputUtil.MOUSE_FLAGS.get(inputClass.uiFlags, "UNKNOWN")
		HandleInputUtil.debugInput(inputClass)

		# Begin
		screen = self.screen()

		if self.bMovie and iCode not in (4, 5):
			screen.hideScreen()
			self.pediaShow()
			self.bMovie = False
			return

		if iCode == 4: # Mouse Enter

			if "ToolTip" in szSplit:
				if self.CyPlayer:
					bPlayerContext = True
				else:
					bPlayerContext = False
				if "TxtTT" in szSplit:
					if szSplit[-1][:8] == "TXT_KEY_":
						self.tooltip.handle(screen, TRNSLTR.getText(szSplit[-1], ()))
					else:
						self.tooltip.handle(screen, szSplit[-1].replace("_", " "))
				elif "UNIT" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getUnitHelp(ID, False, True, True, -1, -1))
				elif "BUILDING" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getBuildingHelp(ID, False, -1, -1, False, False, True))
				elif "PROMO" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getPromotionHelp(ID, False))
				elif "TECH" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getTechHelp(ID, False, bPlayerContext, False, True, -1))
				elif "BONUS" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getBonusHelp(ID, False))
				elif "CIVIC" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().parseCivicInfo(ID, not bPlayerContext, bPlayerContext, False))
				elif "CORP" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().parseCorporationInfo(ID, False))
				elif "TERRAIN" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getTerrainHelp(ID, False))
				elif "IMP" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getImprovementHelp(ID, False))
				elif "FEATURE" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getFeatureHelp(ID, False))
				elif "ROUTE" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getRouteHelp(ID, False))
				elif "PROJECT" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getProjectHelp(ID, False, -1, -1))
				elif "TRAIT" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().parseTraits(ID, False, False))
				elif "LEADER" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().parseLeaderTraits(ID, False, False))
				elif "BUILD" in szSplit:
					self.tooltip.handle(screen, INFO.getDescription("BUILD_", ID))
				elif "RELIGION" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().parseReligionInfo(ID, False))
				elif "HERITAGE" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getHeritageHelp(ID, -1, -1, True, False, False))
				elif "COMBAT" in szSplit:
					self.tooltip.handle(screen, CyGameTextMgr().getUnitCombatHelp(ID, False))
				elif "CONCEPT" in szSplit:
					self.tooltip.handle(screen, INFO.getDescription("CONCEPT_", ID))
				elif "CONCEPT_NEW" in szSplit:
					self.tooltip.handle(screen, INFO.getDescription("NEWCONCEPT_", ID))
				return 1
			return

		if iCode == 6: # Character
			bDown = self.InputData.isKeyDown(iData)
			if bDown == "Unknown":
				return
			if not bDown:
				self.fKeyTimer = 99999
				self.bKeyPress = False
				return
			# Cooldown
			if self.fKeyTimer < 1: return
			screen.hide("Tooltip")
			# Hotkeys
			if iData in (31, 35, 100, 105):
				aList = self.aList
				if not aList: return
				inPage = self.inPage
				if not inPage: return
				iListIndex = self.iListIndex
				if iListIndex == -1: return
				if iData in (35, 100) and iListIndex > 0:
					self.pediaJump(inPage[0], inPage[1], aList[iListIndex - 1][1])
				if iData in (31, 105) and iListIndex + 1 < len(aList):
					self.pediaJump(inPage[0], inPage[1], aList[iListIndex + 1][1])
			elif iData in (13, 102):
				self.back()
			elif iData in (16, 103):
				self.forward()

			if self.bKeyPress:
				self.fKeyTimer = 0.5
			else:
				self.fKeyTimer = 0
				self.bKeyPress = True
			return

		self.tooltip.reset(screen)

		if iCode == 11: # List Select
			if NAME == "PediaMainItemList":
				iCategory, szSubCat = self.SECTION
				if szSubCat == self.mapSubCat[self.PEDIA_CONCEPTS][5]:
					self.pediaJump(iCategory, szSubCat, iData)
			return

		if iCode: return
		# Click
		if szSplit[0] == "PediaMain":
			if szSplit[1] == "Contents":
				if self.bIndex:
					if len(self.pediaHistory) > 1:
						current = self.pediaHistory.pop()
					else:
						current = [-1, "", 0]
					self.pediaJump(current[0], current[1], current[2], False)
			elif szSplit[1] == "Index":
				if not self.bIndex:
					self.inPage = None
					self.deleteAllWidgets()
					screen.hide("PediaMainBack")
					screen.hide("PediaMainForward")
					screen.hide("PediaMainCategoryList")
					screen.hide("PediaMainSubCatList")
					screen.hide("PediaMainItemList")
					screen.hide("PediaMainItemListCount")
					screen.hide("PediaMain|Index")
					screen.hide("PediaMain|Header")
					screen.show("PediaMain|Contents")
					nIndexLists = self.nIndexLists[0]
					if nIndexLists:
						for i in xrange(nIndexLists):
							screen.show("Index" + str(i))
						screen.show("IndexLetters")
					self.bIndex = True
					self.nIndexLists = self.pediaIndex.interfaceScreen()

			elif szSplit[1] == "Header":

				if bAlt or bCtrl or bShift:
					self.welcomeMessage(screen, "Credit")

				elif szFlag == "MOUSE_RBUTTONUP":
					import _misc
					_misc.LaunchDefaultBrowser("https://forums.civfanatics.com/forums/civ4-caveman-2-cosmos.449/")
				else:
					self.welcomeMessage(screen)

		elif "JumpTo" in szSplit:
			if "UNIT" in szSplit:
				self.pediaJump(self.PEDIA_UNITS_0, "", ID)
			elif "BUILDING" in szSplit:
				self.pediaJump(self.PEDIA_BUILDINGS_0, "", ID)
			elif "PROMO" in szSplit:
				self.pediaJump(self.PEDIA_PROMOTIONS, "", ID)
			elif "TECH" in  szSplit:
				self.pediaJump(self.PEDIA_TECHS, "", ID)
			elif "BONUS" in szSplit:
				self.pediaJump(self.PEDIA_BONUSES, "", ID)
			elif "CIVIC" in szSplit:
				self.pediaJump(self.PEDIA_LEADERSHIP, "Civic", ID)
			elif "CORP" in szSplit:
				self.pediaJump(self.PEDIA_SPECIAL, "Corporation", ID)
			elif "TERRAIN" in szSplit:
				self.pediaJump(self.PEDIA_LANDSCAPE, "Terrain", ID)
			elif "IMP" in szSplit:
				self.pediaJump(self.PEDIA_LANDSCAPE, "Improvement", ID)
			elif "FEATURE" in szSplit:
				self.pediaJump(self.PEDIA_LANDSCAPE, "Feature", ID)
			elif "ROUTE" in szSplit:
				self.pediaJump(self.PEDIA_LANDSCAPE, "Route", ID)
			elif "PROJECT" in szSplit:
				self.pediaJump(self.PEDIA_SPECIAL, "Project", ID)
			elif "TRAIT" in szSplit:
				self.pediaJump(self.PEDIA_LEADERSHIP, "Trait", ID)
			elif "LEADER" in szSplit:
				self.pediaJump(self.PEDIA_LEADERSHIP, "Leader", ID)
			elif "BUILD" in szSplit:
				self.pediaJump(self.PEDIA_SPECIAL, "Build", ID)
			elif "RELIGION" in szSplit:
				self.pediaJump(self.PEDIA_LEADERSHIP, "Religion", ID)
			elif "HERITAGE" in szSplit:
				self.pediaJump(self.PEDIA_LEADERSHIP, "Heritage", ID)
			elif "COMBAT" in szSplit:
				self.pediaJump(self.PEDIA_SPECIAL, "UnitCombat", ID)
			elif "CONCEPT" in szSplit:
				self.pediaJump(self.PEDIA_CONCEPTS, "", ID)
			elif "CONCEPT_NEW" in szSplit:
				self.pediaJump(self.PEDIA_CONCEPTS, "NEW", ID)

		elif szSplit[0] == "Preview":
			if szSplit[1] == "Max":
				screen.deleteWidget(NAME)
			elif szSplit[1] == "Min":
				iGroupCategory = self.iGroupCategory
				if iGroupCategory:
					if iGroupCategory == self.PEDIA_BUILDINGS_0:
						screen.addBuildingGraphicGFC("Preview|Max", iData1, 0, 0, self.xRes, self.yRes, WidgetTypes.WIDGET_GENERAL, 0, 0, -20, 30, 0.7, True)
					elif iGroupCategory == self.PEDIA_UNITS_0:
						screen.addUnitGraphicGFC("Preview|Max", iData1, 0, 0, self.xRes, self.yRes, WidgetTypes.WIDGET_GENERAL, 0, 0, -20, 30, 0.7, True)
				elif self.SECTION[0] == self.PEDIA_BONUSES:
					screen.addBonusGraphicGFC("Preview|Max", iData1, 0, 0, self.xRes, self.yRes, WidgetTypes.WIDGET_GENERAL, 0, 0, -20, 30, 0.7, True)
				elif self.SECTION[1] == TRNSLTR.getText("TXT_KEY_PEDIA_CATEGORY_IMPROVEMENT", ()):
					screen.addImprovementGraphicGFC("Preview|Max", iData1, 0, 0, self.xRes, self.yRes, WidgetTypes.WIDGET_GENERAL, 0, 0, -20, 30, 0.7, True)
			elif szSplit[1] == "Quote":
				CyAudioGame().Play2DSound(INFO.getSound("TECH_", ID))
			elif szSplit[1] == "Movie":
				# Both author ui.art.movie.defineTag, so ONE path resolves either -- ART stays an unmigrated
				# boundary, so JSON carries the tag and ARTFILEMGR turns it into a file.
				if szSplit[2] == "PROJECT":
					ArtDef = INFO.getMovieDefineTag("PROJECT_", ID)
				elif szSplit[2] == "BUILDING":
					ArtDef = INFO.getMovieDefineTag("BUILDING_", ID)
				else: return
				if not ArtDef: return
				path = CyArtFileMgr().getMovieArtInfo(ArtDef).getPath()
				self.bMovie = True
				screen.playMovie(path, self.X_PEDIA_PAGE, self.Y_PEDIA_PAGE, 720, 480, 0)
		elif szSplit[0] == "Letter":
			LIST = "Index" + str(iData1)
			nRows = self.nIndexLists[1][iData1]
			screen.enableSelect(LIST, True)
			screen.setSelectedListBoxStringGFC(LIST, nRows)
			screen.setSelectedListBoxStringGFC(LIST, inputClass.iData2)
			screen.enableSelect(LIST, False)

	def update(self, fDelta):
		if self.tooltip.bLockedTT:
			self.tooltip.handle(self.screen())
		if self.bKeyPress:
			self.fKeyTimer += fDelta
			if self.fKeyTimer > 2:
				self.bKeyPress = False

	def onClose(self):
		print "Exit Pedia"
		del self.aWidgetBucket, self.bMovie, self.InputData, self.fKeyTimer, self.bKeyPress, \
			self.CyPlayer, self.bIndex, self.pediaFuture, self.SECTION, self.nWidgetCount, self.iGroupCategory, \
			self.Y_BOT_TEXT, self.H_EDGE_PANEL, self.H_MID_SECTION, self.W_CATEGORIES, self.W_ITEMS, \
			self.X_PEDIA_PAGE, self.Y_PEDIA_PAGE, self.R_PEDIA_PAGE, self.B_PEDIA_PAGE, self.W_PEDIA_PAGE, self.H_PEDIA_PAGE, \
			self.mapScreenFunctions, self.mapListGenerators, self.iNumEras, self.categoryGraphics, self.ICON_SIZE, \
			self.pediaIndex, self.inPage, self.aList, \
			self.PEDIA_BUILDINGS_0, self.PEDIA_UNITS_0, self.PEDIA_MAIN, self.PEDIA_CONCEPTS, self.PEDIA_TECHS, self.PEDIA_UNITS_1, self.PEDIA_UNITS_2, self.PEDIA_PROMOTIONS, \
			self.PEDIA_BUILDINGS_1, self.PEDIA_BUILDINGS_2, self.PEDIA_BONUSES, self.PEDIA_LANDSCAPE, self.PEDIA_LEADERSHIP, self.PEDIA_SPECIAL, self.PEDIA_UPG_TREES

		if not self.pediaHistory:
			self.pediaHistory = [(-1, "", 0)]
		else:
			self.pediaHistory = [(-1, "", 0), self.pediaHistory.pop()]
		self.bNotPedia = True