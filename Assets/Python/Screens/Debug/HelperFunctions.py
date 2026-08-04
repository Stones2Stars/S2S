#Helper Functions - collection of functions meant for further usage
from CvPythonExtensions import *
# The one data-fetching library ([DEC-cy-not-fixed]): STATE = live state, ENABLER = availability,
# ENUMS = the engine enum vocabulary + name->id resolution.
GC = CyGlobalContext()
INFO = CyInfo()
STATE = CyState()
ENABLER = CyEnabler()
ENUMS = CyEnums()
aTechID = []
aTechDesc = []

class HelperFunctions:

	def __init__(self, pHelperFunctions):
		self.main = pHelperFunctions

	##### HIGHEST TECH REQUIREMENT LOCATION FINDER FUNCTIONS  #####

	#Building tech location
	def checkBuildingTechRequirements(self, CvBuildingInfo):
		#All required techs
		aTechList = []
		aTechGridXList = []
		aTechXY = []

		#Main tech requirement
		iTechMainReq = CvBuildingInfo.getPrereqAndTech()
		if iTechMainReq != -1:
			iTechMainLoc = INFO.getIntrinsic("TECH_", iTechMainReq, IntrinsicSlot.PYINT_GRID_X)
			iTechMainRow = INFO.getIntrinsic("TECH_", iTechMainReq, IntrinsicSlot.PYINT_GRID_Y)
			aTechList.append(INFO.getType("TECH_", iTechMainReq))
			aTechGridXList.append(iTechMainLoc)
			aTechXY.append(100*iTechMainLoc+iTechMainRow)
		else:
			iTechMainLoc = 0
			iTechMainRow = 0

		#Tech Type requirement
		aTechTypeLocList = []
		aTechTypeRowList = []
		for iTechType in CvBuildingInfo.getPrereqAndTechs():
			aTechTypeLocList.append(INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_X))
			aTechTypeRowList.append(INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_Y))
			aTechList.append(INFO.getType("TECH_", iTechType))
			aTechGridXList.append(INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_X))
			aTechXY.append(100*INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_X)+INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_Y))
		if len(aTechTypeLocList) > 0 and len(aTechTypeRowList) > 0:
			iTechTypeLoc = max(aTechTypeLocList)
			for iTechLoc in xrange(len(aTechTypeLocList)):
				if aTechTypeLocList[iTechLoc] == max(aTechTypeLocList):
					iTechTypeRow = aTechTypeRowList[iTechLoc]
		else:
			iTechTypeLoc = 0
			iTechTypeRow = 0

		#Tech requirement as defined in special building infos (core tech)
		iSpecialBuilding = CvBuildingInfo.getSpecialBuildingType()
		if iSpecialBuilding != -1:
			iTechSpecialReq = GC.getSpecialBuildingInfo(iSpecialBuilding).getTechPrereq()
			if iTechSpecialReq != -1:
				iTechSpecialLoc = INFO.getIntrinsic("TECH_", iTechSpecialReq, IntrinsicSlot.PYINT_GRID_X)
				iTechSpecialRow = INFO.getIntrinsic("TECH_", iTechSpecialReq, IntrinsicSlot.PYINT_GRID_Y)
				aTechList.append(INFO.getType("TECH_", iTechSpecialReq))
				aTechGridXList.append(iTechSpecialLoc)
				aTechXY.append(100*iTechSpecialLoc+iTechSpecialRow)
			elif iTechSpecialReq == -1:
				iTechSpecialLoc = 0
				iTechSpecialRow = 0
		else:
			iTechSpecialLoc = 0
			iTechSpecialRow = 0

		#Tech requirement derived from location of religion in tech tree
		iRelPrereq1 = CvBuildingInfo.getPrereqReligion()
		iRelPrereq2 = CvBuildingInfo.getReligionType()
		iRelPrereq3 = CvBuildingInfo.getPrereqStateReligion()
		if iRelPrereq1 != -1 or iRelPrereq2 != -1 or iRelPrereq3 != -1:
			iReligionBuilding = max(iRelPrereq1, iRelPrereq2, iRelPrereq3)
			if iReligionBuilding != -1:
				iTechReligionReq = GC.getReligionInfo(iReligionBuilding).getTechPrereq()
				if iTechReligionReq != -1:
					iTechReligionLoc = INFO.getIntrinsic("TECH_", iTechReligionReq, IntrinsicSlot.PYINT_GRID_X)
					iTechReligionRow = INFO.getIntrinsic("TECH_", iTechReligionReq, IntrinsicSlot.PYINT_GRID_Y)
					aTechList.append(INFO.getType("TECH_", iTechReligionReq))
					aTechGridXList.append(iTechReligionLoc)
					aTechXY.append(100*iTechReligionLoc+iTechReligionRow)
			elif iReligionBuilding == -1:
				iTechReligionLoc = 0
				iTechReligionRow = 0
		else:
			iTechReligionLoc = 0
			iTechReligionRow = 0

		#Folklore handling - X Require tech requirement is treated as one of tech requirements of building, assuming X Require is main building requirement.
		iTechAnimalLoc = 0
		iTechAnimalRow = 0
		for iBuildingRequirement in xrange(CvBuildingInfo.getNumPrereqInCityBuildings()):
			iPrereqBuilding = CvBuildingInfo.getPrereqInCityBuilding(iBuildingRequirement)
			if iPrereqBuilding == GC.getInfoTypeForString("BUILDING_FOLKLORE_REQUIREMENT"):
				iTechAnimalLoc = GC.getTechInfo(GC.getBuildingInfo(iPrereqBuilding).getPrereqAndTech()).getGridX()
				iTechAnimalRow = GC.getTechInfo(GC.getBuildingInfo(iPrereqBuilding).getPrereqAndTech()).getGridY()
				aTechList.append(GC.getTechInfo(GC.getBuildingInfo(iPrereqBuilding).getPrereqAndTech()).getType())
				aTechGridXList.append(iTechAnimalLoc)
				aTechXY.append(100*iTechAnimalLoc+iTechAnimalRow)

		#Tech GOM requirements
		aTechGOMReqList = []
		aTechGOMAndLocList = []
		aTechGOMAndRowList = []
		aTechGOMOrLocList = []
		aTechGOMOrRowList = []
		aTechGOMOrTypeList = []
		for i in range(2):
			aTechGOMReqList.append([])
		self.getGOMReqs(CvBuildingInfo.getConstructCondition(), GOMTypes.GOM_TECH, aTechGOMReqList)

		#Extract GOM AND requirements
		for iTech in xrange(len(aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND])):
			aTechGOMAndLocList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_X))
			aTechGOMAndRowList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_Y))
			aTechList.append(INFO.getType("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech]))
			aTechGridXList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_X))
			aTechXY.append(100*INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_X)+INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_Y))
		if len(aTechGOMAndLocList) > 0 and len(aTechGOMAndRowList) > 0:
			iTechGOMAndLoc = max(aTechGOMAndLocList)
			for iTechLoc in xrange(len(aTechGOMAndLocList)):
				if aTechGOMAndLocList[iTechLoc] == max(aTechGOMAndLocList):
					iTechGOMAndRow = aTechGOMAndRowList[iTechLoc]
		else:
			iTechGOMAndLoc = 0
			iTechGOMAndRow = 0

		#Extract GOM OR requirements
		for iTech in xrange(len(aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR])):
			aTechGOMOrLocList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR][iTech], IntrinsicSlot.PYINT_GRID_X))
			aTechGOMOrRowList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR][iTech], IntrinsicSlot.PYINT_GRID_Y))
			aTechGOMOrTypeList.append(INFO.getType("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR][iTech]))
		if len(aTechGOMOrLocList) > 0 and len(aTechGOMOrRowList) > 0:
			iTechGOMOrLoc = min(aTechGOMOrLocList)
			aTechGridXList.append(iTechGOMOrLoc)
			for iTechLoc in xrange(len(aTechGOMOrLocList)):
				if aTechGOMOrLocList[iTechLoc] == min(aTechGOMOrLocList):
					iTechGOMOrRow = aTechGOMOrRowList[iTechLoc]
					sTechGOMOrType = aTechGOMOrTypeList[iTechLoc]
					aTechList.append(sTechGOMOrType)
					aTechXY.append(100*iTechGOMOrLoc+iTechGOMOrRow)
		else:
			iTechGOMOrLoc = 0
			iTechGOMOrRow = 0

		#Pick most advanced column
		iTechLoc = max(iTechMainLoc, iTechTypeLoc, iTechSpecialLoc, iTechReligionLoc, iTechAnimalLoc, iTechGOMAndLoc, iTechGOMOrLoc)
		if iTechLoc == iTechMainLoc:
			iTechRow = iTechMainRow
		elif iTechLoc == iTechTypeLoc:
			iTechRow = iTechTypeRow
		elif iTechLoc == iTechSpecialLoc:
			iTechRow = iTechSpecialRow
		elif iTechLoc == iTechReligionLoc:
			iTechRow = iTechReligionRow
		elif iTechLoc == iTechAnimalLoc:
			iTechRow = iTechAnimalRow
		elif iTechLoc == iTechGOMAndLoc:
			iTechRow = iTechGOMAndRow
		elif iTechLoc == iTechGOMOrLoc:
			iTechRow = iTechGOMOrRow

		#Pick all techs in most advanced column
		aMostAdvancedColumnRequirementsNames = []
		for i in xrange(len(aTechGridXList)):
			if aTechGridXList[i] == max(aTechGridXList) and aTechList[i] not in aMostAdvancedColumnRequirementsNames:
				aMostAdvancedColumnRequirementsNames.append(aTechList[i])

		#aMostAdvancedColumnRequirementsXY is a Tech location ID array - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		aMostAdvancedColumnRequirementsXY = []
		for i in xrange(len(aTechGridXList)):
			if aTechGridXList[i] == max(aTechGridXList) and aTechXY[i] not in aMostAdvancedColumnRequirementsXY:
				aMostAdvancedColumnRequirementsXY.append(aTechXY[i])

		if len(aMostAdvancedColumnRequirementsXY) == 0:
			aMostAdvancedColumnRequirementsXY.append(0)

		return iTechLoc, iTechRow, aMostAdvancedColumnRequirementsXY, aMostAdvancedColumnRequirementsNames

	def checkBuildingEra(self, CvBuildingInfo):
		iEra = 0

		#Main tech requirement
		if CvBuildingInfo.getPrereqAndTech() != -1:
			iEra = GC.getTechInfo(CvBuildingInfo.getPrereqAndTech()).getEra()

		#Tech Type requirement
		for iTech in CvBuildingInfo.getPrereqAndTechs():
			if INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA) > iEra:
				iEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)

		#Tech requirement as defined in special building infos (core tech)
		if CvBuildingInfo.getSpecialBuildingType() != -1:
			iTech = GC.getSpecialBuildingInfo(CvBuildingInfo.getSpecialBuildingType()).getTechPrereq()
			if iTech != -1 and INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA) > iEra:
				iEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)

		#Tech requirement derived from location of religion in tech tree
		if CvBuildingInfo.getPrereqReligion() != -1:
			iTech = GC.getReligionInfo(CvBuildingInfo.getPrereqReligion()).getTechPrereq()
			if iTech != -1 and INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA) > iEra:
				iEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)
		if CvBuildingInfo.getReligionType() != -1:
			iTech = GC.getReligionInfo(CvBuildingInfo.getReligionType()).getTechPrereq()
			if iTech != -1 and INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA) > iEra:
				iEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)
		if CvBuildingInfo.getPrereqStateReligion() != -1:
			iTech = GC.getReligionInfo(CvBuildingInfo.getPrereqStateReligion()).getTechPrereq()
			if iTech != -1 and INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA) > iEra:
				iEra = INFO.getIntrinsic("TECH_", iTech, IntrinsicSlot.PYINT_ERA)

		#Folklore handling - X Require tech requirement is treated as one of tech requirements of building, assuming X Require is main building requirement.
		if CvBuildingInfo.getType().find("BUILDING_FOLKLORE_",0,18) != -1:
			iPrereqBuilding = GC.getInfoTypeForString("BUILDING_FOLKLORE_REQUIREMENT")
			if GC.getTechInfo(GC.getBuildingInfo(iPrereqBuilding).getPrereqAndTech()).getEra() > iEra:
				iEra = GC.getTechInfo(GC.getBuildingInfo(iPrereqBuilding).getPrereqAndTech()).getEra()

		#Tech GOM requirements
		aTechGOMReqList = []
		for i in range(2):
			aTechGOMReqList.append([])
		self.getGOMReqs(CvBuildingInfo.getConstructCondition(), GOMTypes.GOM_TECH, aTechGOMReqList)

		#Extract GOM AND requirements
		for iTech in xrange(len(aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND])):
			if INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_ERA) > iEra:
				iEra = INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_ERA)

		#Extract GOM OR requirements - those are OR type requirements, so pick earliest one.
		aEraList = []
		for iTech in xrange(len(aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR])):
			aEraList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR][iTech], IntrinsicSlot.PYINT_ERA))
		if len(aEraList) > 0 and min(aEraList) > iEra:
			iEra = min(aEraList)

		return iEra

	#Unit tech location
	def checkUnitTechRequirementLocation(self, CvUnitInfo):
		#Main tech
		iTechMainReq = CvUnitInfo.getPrereqAndTech()
		if iTechMainReq != -1:
			iTechMainLoc = INFO.getIntrinsic("TECH_", iTechMainReq, IntrinsicSlot.PYINT_GRID_X)
			iTechMainRow = INFO.getIntrinsic("TECH_", iTechMainReq, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechMainLoc = 0
			iTechMainRow = 0

		#Tech Type requirement
		aTechTypeLocList = []
		aTechTypeRowList = []
		for iTechType in CvUnitInfo.getPrereqAndTechs():
			aTechTypeLocList.append(INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_X))
			aTechTypeRowList.append(INFO.getIntrinsic("TECH_", iTechType, IntrinsicSlot.PYINT_GRID_Y))
		if len(aTechTypeLocList) > 0 and len(aTechTypeRowList) > 0:
			iTechTypeLoc = max(aTechTypeLocList)
			for iTechLoc in xrange(len(aTechTypeLocList)):
				if aTechTypeLocList[iTechLoc] == max(aTechTypeLocList):
					iTechTypeRow = aTechTypeRowList[iTechLoc]
		else:
			iTechTypeLoc = 0
			iTechTypeRow = 0

		#Tech GOM requirements
		aTechGOMReqList = []
		aTechGOMAndLocList = []
		aTechGOMAndRowList = []
		aTechGOMOrLocList = []
		aTechGOMOrRowList = []
		for i in range(2):
			aTechGOMReqList.append([])
		self.getGOMReqs(CvUnitInfo.getTrainCondition(), GOMTypes.GOM_TECH, aTechGOMReqList)

		#Extract GOM AND requirements
		for iTech in xrange(len(aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND])):
			aTechGOMAndLocList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_X))
			aTechGOMAndRowList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_AND][iTech], IntrinsicSlot.PYINT_GRID_Y))
		if len(aTechGOMAndLocList) > 0 and len(aTechGOMAndRowList) > 0:
			iTechGOMAndLoc = max(aTechGOMAndLocList)
			for iTechLoc in xrange(len(aTechGOMAndLocList)):
				if aTechGOMAndLocList[iTechLoc] == max(aTechGOMAndLocList):
					iTechGOMAndRow = aTechGOMAndRowList[iTechLoc]
		else:
			iTechGOMAndLoc = 0
			iTechGOMAndRow = 0

		#Extract GOM OR requirements
		for iTech in xrange(len(aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR])):
			aTechGOMOrLocList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR][iTech], IntrinsicSlot.PYINT_GRID_X))
			aTechGOMOrRowList.append(INFO.getIntrinsic("TECH_", aTechGOMReqList[BoolExprTypes.BOOLEXPR_OR][iTech], IntrinsicSlot.PYINT_GRID_Y))
		if len(aTechGOMOrLocList) > 0 and len(aTechGOMOrRowList) > 0:
			iTechGOMOrLoc = min(aTechGOMOrLocList)
			for iTechLoc in xrange(len(aTechGOMOrLocList)):
				if aTechGOMOrLocList[iTechLoc] == min(aTechGOMOrLocList):
					iTechGOMOrRow = aTechGOMOrRowList[iTechLoc]
		else:
			iTechGOMOrLoc = 0
			iTechGOMOrRow = 0

		#Pick most advanced tech
		iTechLoc = max(iTechMainLoc, iTechTypeLoc, iTechGOMAndLoc, iTechGOMOrLoc)
		if iTechLoc == iTechMainLoc:
			iTechRow = iTechMainRow
		elif iTechLoc == iTechTypeLoc:
			iTechRow = iTechTypeRow
		elif iTechLoc == iTechGOMAndLoc:
			iTechRow = iTechGOMAndRow
		elif iTechLoc == iTechGOMOrLoc:
			iTechRow = iTechGOMOrRow

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechXY = 100*iTechLoc + iTechRow
		sTechDesc = self.getTechName(iTechXY)

		return iTechLoc, iTechRow, iTechXY, sTechDesc

	#Promotion tech location
	def checkPromotionTechRequirementLocation(self, CvPromotionInfo):
		#Promotions have one tech requirement.
		TechReq = CvPromotionInfo.getTechPrereq()
		if TechReq != -1:
			iTechLoc = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_X)
			iTechRow = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechLoc = 0
			iTechRow = 0

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechXY = 100*iTechLoc + iTechRow
		sTechDesc = self.getTechName(iTechXY)

		return iTechLoc, iTechRow, iTechXY, sTechDesc

	#Bonus tech locations
	def checkBonusTechRequirementLocation(self, CvBonusInfo):
		#TechReveal - bonus shown on map, improvements and buildings can't provide it for empire yet, RawVicinity accesses resource like feature on map.
		#TechEnable - full access to bonus, can be traded
		TechReqReveal = CvBonusInfo.getTechReveal()
		TechReqEnable = CvBonusInfo.getTechCityTrade()

		if TechReqReveal != -1:
			iTechRevealLoc = INFO.getIntrinsic("TECH_", TechReqReveal, IntrinsicSlot.PYINT_GRID_X)
			iTechRevealRow = INFO.getIntrinsic("TECH_", TechReqReveal, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechRevealLoc = 0
			iTechRevealRow = 0

		if TechReqEnable != -1:
			iTechEnableLoc = INFO.getIntrinsic("TECH_", TechReqEnable, IntrinsicSlot.PYINT_GRID_X)
			iTechEnableRow = INFO.getIntrinsic("TECH_", TechReqEnable, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechEnableLoc = 0
			iTechEnableRow = 0


		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechRevealXY = 100*iTechRevealLoc + iTechRevealRow
		iTechEnableXY = 100*iTechEnableLoc + iTechEnableRow
		sTechDescReveal = self.getTechName(iTechRevealXY)
		sTechDescEnable = self.getTechName(iTechEnableXY)

		return iTechRevealLoc, iTechRevealRow, iTechEnableLoc, iTechEnableRow, iTechRevealXY, iTechEnableXY, sTechDescReveal, sTechDescEnable

	#Improvement tech location
	def checkImprovementTechRequirementLocation(self, CvImprovementInfo):
		#Improvements have one tech requirement
		TechReq = CvImprovementInfo.getPrereqTech()
		if TechReq != -1:
			iTechLoc = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_X)
			iTechRow = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechLoc = 0
			iTechRow = 0

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechXY = 100*iTechLoc + iTechRow
		sTechDesc = self.getTechName(iTechXY)

		return iTechLoc, iTechRow, iTechXY, sTechDesc

	#Civic tech location
	def checkCivicTechRequirementLocation(self, CvCivicInfo):
		#Civics have one tech requirement.
		TechReq = CvCivicInfo.getTechPrereq()
		if TechReq != -1:
			iTechLoc = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_X)
			iTechRow = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechLoc = 0
			iTechRow = 0

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechXY = 100*iTechLoc + iTechRow
		sTechDesc = self.getTechName(iTechXY)

		return iTechLoc, iTechRow, iTechXY, sTechDesc

	#Build tech location
	def checkBuildTechRequirementLocation(self, CvBuildInfo):
		#Builds have one tech requirement
		TechReq = CvBuildInfo.getTechPrereq()
		if TechReq != -1:
			iTechLoc = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_X)
			iTechRow = INFO.getIntrinsic("TECH_", TechReq, IntrinsicSlot.PYINT_GRID_Y)
		else:
			iTechLoc = 0
			iTechRow = 0

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech requirement, then both infotype X/Y grid is 0
		#Otherwise infotype gets highest Xgrid tech requirement and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechXY = 100*iTechLoc + iTechRow
		sTechDesc = self.getTechName(iTechXY)

		return iTechLoc, iTechRow, iTechXY, sTechDesc

	#Tech name - obtained from tech ID
	def getTechName(self, iTechXY):
		global aTechID, aTechDesc
		#This loop runs once, as cache is created
		if len(aTechID) != GC.getNumTechInfos() or len(aTechDesc) != GC.getNumTechInfos():
			for iTech in xrange(GC.getNumTechInfos()):
				CvTechInfo = GC.getTechInfo(iTech)
				aTechID.append(100*CvTechInfo.getGridX() + CvTechInfo.getGridY())
				aTechDesc.append(CvTechInfo.getType())
				#Position of tech corresponds tech name

		if iTechXY != 0 and iTechXY != 99999: #If we have tech requirement or tech obsoletion
			return aTechDesc[aTechID.index(iTechXY)]
		return ""
	#### THE GOM REQUIREMENT WALKER IS GONE ####
	# It recursed CyBoolExpr trees out of getConstructCondition()/getTrainCondition() to extract the required
	# techs/bonuses. Both read-maps reach the same verdict: NO boolean-expression API belongs on the new
	# surface -- the consumers want the LIST, which is the CvRequires section read, not the tree. Its inputs
	# are gone too (the BoolExpr and info bindings both went), so it could not run regardless.
	# Its callers are left DANGLING on purpose: the requirement lines stop rendering until the requires read
	# is served, which is the visible hole rather than a masked one ([DEC-no-legacy-masking]).
	#^^^^ GOM REQUIREMENT READER FUNCTIONS ^^^^#

	##### PROPERTY READER FUNCTIONS #####

	def getPropertyAmmountPerTurn(self, pPropertyManipulators):
		a = [0]*GC.getNumPropertyInfos()
		if pPropertyManipulators is not None:
			for iSource in xrange(pPropertyManipulators.getNumSources()):
				pSource = pPropertyManipulators.getSource(iSource)
				if isinstance(pSource, CvPropertySourceConstant):
					pIntExpr = pSource.getAmountPerTurnExpr()
					if isinstance(pIntExpr, IntExprConstant):
						a[pSource.getProperty()] += pIntExpr.iValue
		return a

	#^^^^ PROPERTY READER FUNCTIONS ^^^^#

	##### OBSOLETION TECH LOCATION FINDER FUNCTIONS #####

	#Building tech obsoletion location
	def checkBuildingTechObsoletionLocation(self, CvBuildingInfo):
		iTechObsLoc = 999 #Never obsoletes
		if CvBuildingInfo.getObsoleteTech() != -1:
			iTechObsLoc = GC.getTechInfo(CvBuildingInfo.getObsoleteTech()).getGridX()

		iTechObsRow = 99 #Never obsoletes
		if CvBuildingInfo.getObsoleteTech() != -1:
			iTechObsRow = GC.getTechInfo(CvBuildingInfo.getObsoleteTech()).getGridY()

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech obsoletion, then infotype X/Y grid is 999 / 99
		#Otherwise infotype gets highest Xgrid tech obsoletion and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechObsXY = 100*iTechObsLoc + iTechObsRow
		sTechDesc = self.getTechName(iTechObsXY)

		return iTechObsLoc, iTechObsXY, sTechDesc

	#Unit tech obsoletion location
	def checkUnitTechObsoletionLocation(self, CvUnitInfo):
		iTechObsLoc = 999 #Never obsoletes
		iTechObsRow = 99 #Never obsoletes

		if CvUnitInfo.getObsoleteTech() != -1:
			iTechObsLoc = GC.getTechInfo(CvUnitInfo.getObsoleteTech()).getGridX()
			iTechObsRow = GC.getTechInfo(CvUnitInfo.getObsoleteTech()).getGridY()

		#This is a Tech location ID - X grid varies from 0 to 160, and Ygrid varies from 0 to 20
		#If infotype doesn't have tech obsoletion, then infotype X/Y grid is 999 / 99
		#Otherwise infotype gets highest Xgrid tech obsoletion and related Ygrid position
		#Xgrid is multiplied by 100, and then its value is increased by Ygrid
		iTechObsXY = 100*iTechObsLoc + iTechObsRow
		sTechDesc = self.getTechName(iTechObsXY)

		return iTechObsLoc, iTechObsXY, sTechDesc

	#^^^^ OBSOLETION TECH LOCATION FINDER FUNCTIONS ^^^^#
