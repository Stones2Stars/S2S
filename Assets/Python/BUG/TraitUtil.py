## TraitUtil
##
## Utilities for dealing with Traits and TraitInfos.
##
## Notes
##   - Must be initialized externally by calling init()
##
## Copyright (c) 2008 The BUG Mod.
##
## Author: EmperorFool

from CvPythonExtensions import *

GENERIC_ICON = "*"
TRAIT_ICONS = {}

GENERIC_BUTTON = "Art/Interface/Buttons/TechTree/"
TRAIT_BUTTONS = {}

# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
gc = GC   # this module spells it lowercase
STATE = CyState()
ENABLER = CyEnabler()
ENUMS = CyEnums()
# The FONT GLYPH of a yield/commerce is not info data -- it is a symbol slot the text manager's symbol pass
# assigns at load, and the translator already publishes each one as an [ICON_*] token. So the icons below come
# from the text layer that owns them, exactly as the FontSymbols ones come from the game's symbol table.
TRNSLTR = CyTranslator()

def init():
	"Performs one-time initialization after the game starts up."
	game = gc.getGame()
	global GENERIC_ICON
	GENERIC_ICON = u"%c" % game.getSymbolID(FontSymbols.MAP_CHAR)

	addTrait("AGGRESSIVE", game.getSymbolID(FontSymbols.STRENGTH_CHAR), "Art/Interface/Buttons/Promotions/Combat1.dds")
	addTrait("CHARISMATIC", game.getSymbolID(FontSymbols.HAPPY_CHAR), "Art/Interface/Buttons/TechTree/MassMedia.dds")
	addTrait("CREATIVE", TRNSLTR.getText("[ICON_CULTURE]", ()), "Art/Interface/Buttons/TechTree/Music.dds")
	addTrait("EXPANSIVE", game.getSymbolID(FontSymbols.HEALTHY_CHAR), "Art/Interface/Buttons/Actions/Heal.dds")
	addTrait("FINANCIAL", TRNSLTR.getText("[ICON_GOLD]", ()), "Art/Interface/Buttons/TechTree/Banking.dds")
	addTrait("IMPERIALIST", game.getSymbolID(FontSymbols.OCCUPATION_CHAR), "Art/Interface/Buttons/Actions/FoundCity.dds")
	addTrait("INDUSTRIOUS", TRNSLTR.getText("[ICON_PRODUCTION]", ()), "Art/Interface/Buttons/TechTree/Industrialism.dds")
	addTrait("ORGANIZED", game.getSymbolID(FontSymbols.TRADE_CHAR), "Art/Interface/Buttons/Buildings/Courthouse.dds")
	addTrait("PHILOSOPHICAL", game.getSymbolID(FontSymbols.GREAT_PEOPLE_CHAR), "Art/Interface/Buttons/TechTree/Philosophy.dds")
	addTrait("PROTECTIVE", game.getSymbolID(FontSymbols.DEFENSE_CHAR), "Art/Interface/Buttons/Promotions/CityGarrison1.dds")
	addTrait("SPIRITUAL", game.getSymbolID(FontSymbols.RELIGION_CHAR), "Art/Interface/Buttons/TechTree/Meditation.dds")
	addTrait("NOMAD", game.getSymbolID(FontSymbols.MOVES_CHAR), "Art/Interface/Buttons/Units/HorseArcher.dds")
	addTrait("AGRICULTURAL", TRNSLTR.getText("[ICON_FOOD]", ()), "Art/Interface/Buttons/TechTree/Agriculture.dds")
	addTrait("SEAFARING", game.getSymbolID(FontSymbols.MAP_CHAR), "Art/Interface/Buttons/TechTree/seafaring.dds")
	addTrait("DECEIVER", TRNSLTR.getText("[ICON_ESPIONAGE]", ()), "Art/Interface/Buttons/Units/Spy.dds")
	addTrait("SCIENTIFIC", TRNSLTR.getText("[ICON_RESEARCH]", ()), "Art/Interface/Buttons/Process/ProcessResearch.dds")
	addTrait("HUMANITARIAN", game.getSymbolID(FontSymbols.HEALTHY_CHAR), "Art/Interface/Buttons/Actions/Heal.dds")
	addTrait("PROGRESSIST", game.getSymbolID(FontSymbols.GREAT_PEOPLE_CHAR), "Art/Interface/Buttons/TechTree/enlighten1.dds")
	addTrait("POLITICIAN", game.getSymbolID(FontSymbols.HAPPY_CHAR), "Art/Interface/Buttons/actions/steal_tech.dds")
	addTrait("ANTI_CLERICAL", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/Civics/secular1.dds")
	addTrait("CRUEL", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/Civics/ruleoffear.dds")
	addTrait("IDEALISTIC", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), ",Art/Interface/Buttons/Civics/Pacifism.dds,Art/Interface/Buttons/Civics_Civilizations_Religions_Atlas.dds,3,2")
	addTrait("REVOLUTIONARY", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/TechTree/labor_movement.dds")
	addTrait("MEGALOMANIAC", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/TechTree/sculpture.dds")
	addTrait("BARBARIC", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/Units/cannibal.dds")
	addTrait("ISOLATIONIST", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/Buildings/toll_house.dds")
	addTrait("FANATICAL", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/TechTree/theocracy.dds")
	addTrait("POPULIST", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/Civics/hiredlabor.dds")
	addTrait("EXCESSIVE", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), ",Art/Interface/Buttons/Buildings/Versailles.dds,Art/Interface/Buttons/Buildings_Atlas.dds,7,7")
	addTrait("FOREIGN", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), ",Art/Interface/Buttons/TechTree/Constitution.dds,Art/Interface/Buttons/TechTree_Atlas.dds,8,2")
	addTrait("TEMPERAMENTAL", game.getSymbolID(FontSymbols.UNHAPPY_CHAR), "Art/Interface/Buttons/TechTree/explosives.dds")
	addTrait("HUNTER_GATHERER", TRNSLTR.getText("[ICON_FOOD]", ()), ",Art/Interface/Buttons/TechTree/Archery.dds,Art/Interface/Buttons/TechTree_Atlas.dds,4,1")
	addTrait("BARBARIAN", game.getSymbolID(FontSymbols.OCCUPATION_CHAR), ",Art/Interface/Buttons/Civilizations/Barbarian.dds,Art/Interface/Buttons/Civics_Civilizations_Religions_Atlas.dds,4,5")

# Rise of Mankind 2.6 - new traits

def addTrait(trait, icon, button):
	eTrait = gc.getInfoTypeForString("TRAIT_" + trait)
	if eTrait != -1:
		if icon is not None:
			# A FontSymbols id arrives as an int and still needs formatting; an [ICON_*] token has already been
			# resolved to its glyph by the translator, so it is stored as-is.
			TRAIT_ICONS[eTrait] = icon if isinstance(icon, basestring) else u"%c" % icon
		if button is not None:
			TRAIT_BUTTONS[eTrait] = button


def getIcon(eTrait):
	if eTrait in TRAIT_ICONS:
		return TRAIT_ICONS[eTrait]
	else:
		return GENERIC_ICON

def getButton(eTrait):
	if eTrait in TRAIT_BUTTONS:
		return TRAIT_BUTTONS[eTrait]
	else:
		return GENERIC_BUTTON
