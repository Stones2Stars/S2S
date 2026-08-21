# Cached civic data
from CvPythonExtensions import *
GC = CyGlobalContext()
INFO = CyInfo()

def initCivicData():
	GC = CyGlobalContext()
	INFO = CyInfo()
	print "CivicData.initCivicData"

	global civicLists
	civicLists = []
	for _ in xrange(GC.getNumCivicOptionInfos()):
		civicLists.append([])

	# ONE boundary crossing for the whole civic -> CIVICOPTION_ column, cached here for the rest of the session.
	# The global context no longer hands out info OBJECTS (that was the legacy escape hatch); entity data comes
	# from CyInfo, and the bulk index shape is what scales -- a boost::python call costs far more than the
	# lookup inside it, so crossing once and caching beats one crossing per civic.
	civicOptions = INFO.civicOptions()
	for iCivic in xrange(GC.getNumCivicInfos()):
		civicLists[civicOptions.getValue(iCivic)].append(iCivic)
