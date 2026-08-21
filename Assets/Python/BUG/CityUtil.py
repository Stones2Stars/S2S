## CityUtil
##
## Collection of utility functions for dealing with cities.
##
## Copyright (c) 2009 The BUG Mod.
##
## Author: EmperorFool

from CvPythonExtensions import *


## Globals

# The one data-fetching library ([DEC-cy-not-fixed]): ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
ENABLER = CyEnabler()
ENUMS = CyEnums()


## Growth and Starvation
##
## A city is an (iOwner, iCityId) PAIR, never a handle -- the identity every CyState read is addressed by and
## the one an engine callback hands over.

# EMPHASIZE_AVOID_GROWTH. The emphasis registry is data-driven, so the id is resolved by NAME rather than
# written as a literal ([patterns.md] ENUM OPERATIONS ARE FIRST CLASS).
_iAvoidGrowth = -1

def _avoidGrowth():
	global _iAvoidGrowth
	if _iAvoidGrowth < 0:
		_iAvoidGrowth = ENUMS.getInfoType("EMPHASIZE_AVOID_GROWTH")
	return _iAvoidGrowth

def willGrowThisTurn(cityId):
	"""
	Returns True if <cityId> will increase its population due to growth this turn.

	Emphasize No Growth must be off for the city, and its food rate plus storage must reach the growth threshold.
	"""
	if GC.getPlayer(cityId[0]).getCity(cityId[1]).isEmphasizing(_avoidGrowth()):
		return False
	aGrowth = GC.getPlayer(cityId[0]).getCity(cityId[1]).getGrowth()
	return (aGrowth[CityGrowthRead.GROWTH_READ_FOOD_STORED]
	        + aGrowth[CityGrowthRead.GROWTH_READ_FOOD_PER_TURN]
	        >= aGrowth[CityGrowthRead.GROWTH_READ_THRESHOLD])

def willShrinkThisTurn(cityId):
	"""
	Returns True if <cityId> will decrease its population due to starvation this turn.

	It must have at least two population, and its food rate plus storage must be negative.
	"""
	if GC.getPlayer(cityId[0]).getCity(cityId[1]).getPopulation() <= 1:
		return False
	aGrowth = GC.getPlayer(cityId[0]).getCity(cityId[1]).getGrowth()
	return (aGrowth[CityGrowthRead.GROWTH_READ_FOOD_STORED]
	        + aGrowth[CityGrowthRead.GROWTH_READ_FOOD_PER_TURN] < 0)
