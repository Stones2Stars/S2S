# Cached civic data
from CvPythonExtensions import *

def initCivicData():
	GC = CyGlobalContext()
	print "CivicData.initCivicData"

	global civicLists
	civicLists = []
	for _ in xrange(GC.getNumCivicOptionInfos()):
		civicLists.append([])

	for iCivic in xrange(GC.getNumCivicInfos()):
		info = GC.getCivicInfo(iCivic)
		civicLists[info.getCivicOptionType()].append((info, iCivic))
