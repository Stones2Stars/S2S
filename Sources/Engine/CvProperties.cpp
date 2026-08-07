//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:	CvProperties.cpp
//
//  PURPOSE: Generic properties for Civ4 classes
//
//------------------------------------------------------------------------------------------------

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvCity.h"
#include "CvGame.h"
#include "CvGameObject.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvPlayer.h"
#include "CvPlot.h"
#include "CvProperties.h"
#include "CvTeam.h"
#include "CvUnit.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"
#include "Spine/CvEventSpine.h"   // the property DOMAIN fact -- the mutation choke points + the in-read reseed
#include "Enabler/CvEnablerKernel.h"   // propertyBandThresholds -- the ONE registry of authored operate-band boundaries

CvProperties::CvProperties()
{
	m_pGameObject = NULL;
}

CvProperties::CvProperties(CvGame* pGame)
{
	m_pGameObject = pGame->getGameObject();
}

CvProperties::CvProperties(CvTeam* pTeam)
{
	m_pGameObject = pTeam->getGameObject();
}

CvProperties::CvProperties(CvPlayer* pPlayer)
{
	m_pGameObject = pPlayer->getGameObject();
}

CvProperties::CvProperties(CvCity* pCity)
{
	m_pGameObject = pCity->getGameObject();
}

CvProperties::CvProperties(CvUnit* pUnit)
{
	m_pGameObject = pUnit->getGameObject();
}

CvProperties::CvProperties(CvPlot* pPlot)
{
	m_pGameObject = pPlot->getGameObject();
}

PropertyTypes CvProperties::getProperty(int index) const
{
	FASSERT_BOUNDS(0, (int)m_aiProperty.size(), index);
	return m_aiProperty[index].prop;
}

int CvProperties::getValue(int index) const
{
	FASSERT_BOUNDS(0, (int)m_aiProperty.size(), index);
	return m_aiProperty[index].value;
}

PropertyTypes CvProperties::getChangeProperty(int index) const
{
	FASSERT_BOUNDS(0, (int)m_aiPropertyChange.size(), index);
	return m_aiPropertyChange[index].prop;
}

int CvProperties::getChange(int index) const
{
	FASSERT_BOUNDS(0, (int)m_aiPropertyChange.size(), index);
	return m_aiPropertyChange[index].value;
}

int CvProperties::getNumProperties() const
{
	return m_aiProperty.size();
}

int CvProperties::getPositionByProperty(PropertyTypes eProp) const
{
	PROFILE_EXTRA_FUNC();
	for (prop_value_const_iterator it = m_aiProperty.begin();it!=m_aiProperty.end(); ++it)
	{
		if (it->prop == eProp)
			return it - m_aiProperty.begin();
	}
	return -1;
}

int CvProperties::getValueByProperty(PropertyTypes eProp) const
{
	const int index = getPositionByProperty(eProp);

	return index < 0 ? 0 : getValue(index);
}

int CvProperties::getChangeByProperty(PropertyTypes eProp) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const PropertyValue& it, m_aiPropertyChange)
	{
		if (it.prop == eProp)
			return it.value;
	}
	return 0;
}

void CvProperties::setChangeByProperty(PropertyTypes eProp, int iVal)
{
	PROFILE_EXTRA_FUNC();
	foreach_(PropertyValue& it, m_aiPropertyChange)
	{
		if (it.prop == eProp)
		{
			it.value = iVal;
			return;
		}
	}
	m_aiPropertyChange.push_back(PropertyValue(eProp,iVal));;
}

void CvProperties::changeChangeByProperty(PropertyTypes eProp, int iChange)
{
	PROFILE_EXTRA_FUNC();
	foreach_(PropertyValue& it, m_aiPropertyChange)
	{
		if (it.prop == eProp)
		{
			it.value += iChange;
			return;
		}
	}
	m_aiPropertyChange.push_back(PropertyValue(eProp,iChange));;
}

// Announce the property DOMAIN fact for this bag's owning game object (event-spine.md: one fact, one emit, at the
// genuine mutation choke point). Every caller sits inside the `m_pGameObject` test that already guards the
// notification hook -- which is also what keeps the INFO-side bags silent: CvOutcome / CvEventInfo /
// CvEventTriggerInfo hold CvProperties as authored DATA, built through the default constructor, so their
// m_pGameObject is NULL and an XML/JSON parse announces nothing.
// ⛔ The emit lives HERE and not in CvGameObject::eventPropertyChanged: CvGameObjectUnit overrides that hook
// without chaining to the base, so an emit placed there is silently skipped for every unit.
static void emitPropertyFact(const CvGameObject* pObject, PropertyTypes eProperty, int iNewValue, int iOldValue)
{
	// The event is the operator: which way the value moved is the FACT, and the payload carries only HOW MUCH.
	if (iNewValue > iOldValue)
	{
		emitPropertyAdded((int)pObject->getGameObjectType(), pObject->getObjectInstanceId(),
			(int)pObject->getOwnerPlayerId(), (int)eProperty, iNewValue - iOldValue);
	}
	else if (iNewValue < iOldValue)
	{
		emitPropertyRemoved((int)pObject->getGameObjectType(), pObject->getObjectInstanceId(),
			(int)pObject->getOwnerPlayerId(), (int)eProperty, iOldValue - iNewValue);
	}
	// ⚖ AND THE HOLDER ANNOUNCES THE THRESHOLD CROSSING BESIDE THE VALUE (owner) -- the amenity fold's shape:
	// power announces 0 -> 1 and 1 -> 0 and says nothing about 1 -> 2, because only the VERDICT moved.
	// The verdict here is a `requires.operate` band boundary, and the whole authored set of them is one registry
	// (EnablerKernel::propertyBandThresholds), so this is where they are tested -- ONCE, for every consumer.
	// ⛔ Not in a consumer: the raw value fact fires for nearly every property of every city every turn as the
	// solver runs, so a consumer gating on it would re-derive this sweep per consumer and pay it per event
	// ([DEC-single-implementation]).
	if (pObject->getGameObjectType() != GAMEOBJECT_CITY || iNewValue == iOldValue)
	{
		return;   // only a CITY carries an operate band; the other bags announce the value alone
	}
	const std::map<int, std::set<int> >& kThresholds = EnablerKernel::propertyBandThresholds();
	const std::map<int, std::set<int> >::const_iterator itProperty = kThresholds.find((int)eProperty);
	if (itProperty == kThresholds.end())
	{
		return;   // no authored clause bands on this property -- there is no verdict to cross
	}
	const int iLow  = (iOldValue < iNewValue) ? iOldValue : iNewValue;
	const int iHigh = (iOldValue < iNewValue) ? iNewValue : iOldValue;
	// A boundary sitting in the CLOSED interval the value swept is a crossing for BOTH band senses: a `min: T`
	// clause flips between T-1 and T, a `max: T` clause between T and T+1, so the closed test catches each.
	// ⛔ Testing only `>= T` on both ends would miss every max-band's upper flip.
	const std::set<int>::const_iterator itBoundary = itProperty->second.lower_bound(iLow);
	if (itBoundary == itProperty->second.end() || *itBoundary > iHigh)
	{
		return;
	}
	if (iNewValue > iOldValue)
	{
		emitCityPropertyBandAdded(pObject->getObjectInstanceId(), (int)pObject->getOwnerPlayerId(), (int)eProperty);
	}
	else
	{
		emitCityPropertyBandRemoved(pObject->getObjectInstanceId(), (int)pObject->getOwnerPlayerId(), (int)eProperty);
	}
}

void CvProperties::setValue(int index, int iVal)
{
	//TBOOSHUNTHERE
	//CvString szBuffer;
	//szBuffer.format("SetValue, index %i, iValue %i.", index, iVal);
	//gDLL->logMsg("PropertyBuildingOOS.log", szBuffer.c_str(), false, false);
	FASSERT_BOUNDS(0, (int)m_aiProperty.size(), index);
	const int iOldVal = m_aiProperty[index].value;
	if (iOldVal != iVal)
	{
		m_aiProperty[index].value = iVal;
		if (m_pGameObject)
		{
			// The fact is announced BEFORE the band hook, so the cause precedes the consequences the hook
			// applies (banded promotions on a unit, and the property-building placement downstream).
			emitPropertyFact(m_pGameObject, m_aiProperty[index].prop, iVal, iOldVal);
			m_pGameObject->eventPropertyChanged(m_aiProperty[index].prop, iVal);
		}
	}
}

void CvProperties::setChange(int index, int iVal)
{
	FASSERT_BOUNDS(0, (int)m_aiPropertyChange.size(), index);
	m_aiPropertyChange[index].value = iVal;
}

void CvProperties::setValueByProperty(PropertyTypes eProp, int iVal)
{
	//TBOOSHUNTHERE
	//CvString szBuffer;
	//szBuffer.format("SetValueByProperty, eProp %i, iValue %i.", eProp, iVal);
	//gDLL->logMsg("PropertyBuildingOOS.log", szBuffer.c_str(), false, false);
	const int index = getPositionByProperty(eProp);
	if (index >= 0)
	{
		setValue(index, iVal);
	}
	else if (iVal != 0)
	{
		m_aiProperty.push_back(PropertyValue(eProp,iVal));
		if (m_pGameObject)
		{
			// A property absent from the bag reads as 0 (getValueByProperty), so 0 IS the old value here.
			emitPropertyFact(m_pGameObject, eProp, iVal, 0);
			m_pGameObject->eventPropertyChanged(eProp, iVal);
		}
	}
}

void CvProperties::changeValue(int index, int iChange)
{
	if (iChange == 0) return;

	const PropertyTypes eProperty = getProperty(index);

	// setValue announces the fact (and suppresses a no-op) -- a second emit here would double it.
	setValue(index, getValue(index) + iChange);
	changeChangeByProperty(eProperty, iChange);
	if (m_pGameObject)
	{
		propagateChange(eProperty, iChange);
	}
}

void CvProperties::changeValueByProperty(PropertyTypes eProp, int iChange)
{
	//TBOOSHUNTHERE
	//CvString szBuffer;
	//szBuffer.format("changeValueByProperty, eProp %i, iChange %i.", eProp, iChange);
	//gDLL->logMsg("PropertyBuildingOOS.log", szBuffer.c_str(), false, false);
	if (iChange == 0) return;

	const int index = getPositionByProperty(eProp);
	if (index < 0)
	{
		m_aiProperty.push_back(PropertyValue(eProp,iChange));
		changeChangeByProperty(eProp, iChange);
		if (m_pGameObject)
		{
			// This object's OWN fact first; propagateChange then fans the change onto OTHER objects, each of
			// which re-enters this path and announces its own -- distinct facts, never duplicates of this one.
			emitPropertyFact(m_pGameObject, eProp, iChange, 0);
			propagateChange(eProp, iChange);
			m_pGameObject->eventPropertyChanged(eProp, iChange);
		}
	}
	else changeValue(index, iChange);
}

// helper function for propagating change
void callChangeValueByProperty(const CvGameObject* pObject, PropertyTypes eProp, int iChange)
{
	pObject->getProperties()->changeValueByProperty(eProp, iChange);
}

void CvProperties::propagateChange(PropertyTypes eProp, int iChange)
{
	PROFILE_EXTRA_FUNC();
	const CvPropertyInfo& kInfo = GC.getPropertyInfo(eProp);
	for (int iI = 0; iI < NUM_GAMEOBJECTS; iI++)
	{
		const int iChangePercent = kInfo.getChangePropagator(m_pGameObject->getGameObjectType(), (GameObjectTypes)iI);
		if (iChangePercent)
		{
			const int iPropChange = (iChange * iChangePercent) / 100;
			m_pGameObject->foreach((GameObjectTypes)iI, bind(callChangeValueByProperty, _1, eProp, iPropChange));
		}
	}
}

void CvProperties::addProperties(const CvProperties* pProp)
{
	PROFILE_EXTRA_FUNC();
	const int num = pProp->getNumProperties();
	for (int index = 0; index < num; index++)
	{
		changeValueByProperty(pProp->getProperty(index), pProp->getValue(index));
	}
}

void CvProperties::subtractProperties(const CvProperties* pProp)
{
	PROFILE_EXTRA_FUNC();
	const int num = pProp->getNumProperties();
	for (int index = 0; index < num; index++)
	{
		changeValueByProperty(pProp->getProperty(index), - pProp->getValue(index));
	}
}

bool CvProperties::isEmpty() const
{
	return m_aiProperty.empty();
}

void CvProperties::clear()
{
	// ⛔ NO fact here, deliberately. clear() is the object RESET path (CvGame / CvTeam / CvPlayer / CvCity /
	// CvUnit / CvPlot ::reset), which runs while an object is being constructed, re-initialized or re-used --
	// CvCity::read and CvUnit::read call reset() as their FIRST act, with the identity arguments DEFAULTED, so
	// there is no id and no owner to name and a fact emitted here would be attributed to (id 0, NO_PLAYER).
	// Nothing is lost: the state a reset discards is re-announced by the read that follows it (the reseed in
	// readWrapper below).
	m_aiProperty.clear();
}

void CvProperties::clearChange()
{
	m_aiPropertyChange.clear();
}

void CvProperties::emitReadProperty(PropertyTypes eProp, int iValue) const
{
	// The load RESEED (event-spine.md). Both deserializers below fill m_aiProperty directly -- deliberately
	// bypassing the setters, so no fact fires -- and a loaded game would otherwise reach a different consumer
	// state than a played one. Nothing else covers the gap: unlike a plot's substrate, a property value is
	// DERIVED FROM NOTHING, so no other in-read emit re-derives the property block and there is no duplicate to
	// avoid. A stored 0 is skipped for exactly the reason setValue suppresses it -- the owning object's reset()
	// emptied the bag, so 0 -> 0 is not a change. The two deserializers are alternatives, never both run for one
	// object, so covering both cannot double a fact.
	if (m_pGameObject != NULL && iValue != 0)
	{
		emitPropertyFact(m_pGameObject, eProp, iValue, 0);
	}
}

void CvProperties::read(FDataStreamBase *pStream)
{
	PROFILE_EXTRA_FUNC();
	// This function replaces the current content if any so clear first
	m_aiProperty.clear();
	m_aiPropertyChange.clear();

	int num;
	pStream->Read(&num);
	for (int i = 0; i < num; i++)
	{
		int eProp;
		int iVal;
		pStream->Read(&eProp);
		pStream->Read(&iVal);
		// AIAndy: Changed to avoid usage of the methods that trigger property change events
		if (eProp > -1)
		{
			m_aiProperty.push_back(PropertyValue(static_cast<PropertyTypes>(eProp), iVal));
			emitReadProperty(static_cast<PropertyTypes>(eProp), iVal);
		}
	}
}

void CvProperties::readWrapper(FDataStreamBase *pStream)
{

	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper&	wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();
	wrapper.AttachToStream(pStream);

	// This function replaces the current content if any so clear first
	m_aiProperty.clear();
	m_aiPropertyChange.clear();

	int iPropertyNum = 0;
	WRAPPER_READ(wrapper, "CvProperties",&iPropertyNum);
	for (int i = 0; i < iPropertyNum; i++)
	{
		int eProp = -1;
		int iVal;
		WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvProperties", REMAPPED_CLASS_TYPE_PROPERTIES, &eProp);
		WRAPPER_READ(wrapper, "CvProperties",&iVal);
		// AIAndy: Changed to avoid usage of the methods that trigger property change events
		if (eProp > -1)
		{
			m_aiProperty.push_back(PropertyValue(static_cast<PropertyTypes>(eProp), iVal));
			emitReadProperty(static_cast<PropertyTypes>(eProp), iVal);
		}
	}

	// ⛔ The CHANGE ledger below gets NO fact. It is a per-turn accumulation of the very deltas the value facts
	// above already carry (changeChangeByProperty is fed from changeValue / changeValueByProperty), and the
	// solver clears it every turn (CvPropertySolver -> clearChange). Announcing it would restate one state
	// change in a second shape -- the duplicate the spine's one bar forbids.
	int iPropertyChangeNum = 0;
	WRAPPER_READ(wrapper, "CvProperties",&iPropertyChangeNum);
	for (int i = 0; i < iPropertyChangeNum; i++)
	{
		int eProp = -1;
		int iVal;
		WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvProperties", REMAPPED_CLASS_TYPE_PROPERTIES, &eProp);
		WRAPPER_READ(wrapper, "CvProperties",&iVal);

		if (eProp > -1) m_aiPropertyChange.push_back(PropertyValue(static_cast<PropertyTypes>(eProp), iVal));
	}
}

void CvProperties::write(FDataStreamBase *pStream)
{
	PROFILE_EXTRA_FUNC();
	const int iPropertyNum = getNumProperties();
	pStream->Write(iPropertyNum);
	for (int i = 0; i < iPropertyNum; i++)
	{
		pStream->Write(getProperty(i));
		pStream->Write(getValue(i));
	}
}

void CvProperties::writeWrapper(FDataStreamBase *pStream)
{
	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();
	wrapper.AttachToStream(pStream);

	const int iPropertyNum = getNumProperties();
	WRAPPER_WRITE(wrapper, "CvProperties", iPropertyNum);
	for (int i = 0; i < iPropertyNum; i++)
	{
		const int eProp = getProperty(i);
		const int iVal = getValue(i);

		WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvProperties", REMAPPED_CLASS_TYPE_PROPERTIES, eProp);
		WRAPPER_WRITE(wrapper, "CvProperties", iVal);
	}

	const int iPropertyChangeNum = (int)m_aiPropertyChange.size();
	WRAPPER_WRITE(wrapper, "CvProperties", iPropertyChangeNum);
	for (int i = 0; i < iPropertyChangeNum; i++)
	{
		const int eProp = getChangeProperty(i);
		const int iVal = getChange(i);

		WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvProperties", REMAPPED_CLASS_TYPE_PROPERTIES, eProp);
		WRAPPER_WRITE(wrapper, "CvProperties",iVal);
	}
}

bool CvProperties::read(CvXMLLoadUtility* pXML, const wchar_t* szTagName)
{
	PROFILE_EXTRA_FUNC();
	if (pXML->TryMoveToXmlFirstChild(szTagName))
	{
		if (pXML->TryMoveToXmlFirstChild())
		{
			if (pXML->TryMoveToXmlFirstOfSiblings(L"Property"))
			{
				do
				{
					int iVal;
					CvString szTextVal;
					pXML->GetChildXmlValByName(szTextVal, L"PropertyType");
					const int eProp = pXML->GetInfoClass(szTextVal);
					pXML->GetOptionalChildXmlValByName(&iVal, L"iPropertyValue");
					setValueByProperty(static_cast<PropertyTypes>(eProp), iVal);
				} while(pXML->TryMoveToXmlNextSibling());
			}
			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}
	return true;
}

void CvProperties::copyNonDefaults(const CvProperties* pProp)
{
	PROFILE_EXTRA_FUNC();
	const int num = pProp->getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getPositionByProperty(pProp->getProperty(index)) < 0)
			setValueByProperty(pProp->getProperty(index), pProp->getValue(index));
	}
}

bool CvProperties::operator<(const CvProperties& prop) const
{
	PROFILE_EXTRA_FUNC();
	const int num = prop.getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getValueByProperty(prop.getProperty(index)) >= prop.getValue(index))
			return false;
	}
	return true;
}

bool CvProperties::operator<=(const CvProperties& prop) const
{
	PROFILE_EXTRA_FUNC();
	const int num = prop.getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getValueByProperty(prop.getProperty(index)) > prop.getValue(index))
			return false;
	}
	return true;
}

bool CvProperties::operator>(const CvProperties& prop) const
{
	PROFILE_EXTRA_FUNC();
	const int num = prop.getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getValueByProperty(prop.getProperty(index)) <= prop.getValue(index))
			return false;
	}
	return true;
}

bool CvProperties::operator>=(const CvProperties& prop) const
{
	PROFILE_EXTRA_FUNC();
	const int num = prop.getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getValueByProperty(prop.getProperty(index)) < prop.getValue(index))
			return false;
	}
	return true;
}

bool CvProperties::operator==(const CvProperties& prop) const
{
	PROFILE_EXTRA_FUNC();
	const int num = prop.getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getValueByProperty(prop.getProperty(index)) != prop.getValue(index))
			return false;
	}
	return true;
}

bool CvProperties::operator!=(const CvProperties& prop) const
{
	PROFILE_EXTRA_FUNC();
	const int num = prop.getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (getValueByProperty(prop.getProperty(index)) == prop.getValue(index))
			return false;
	}
	return true;
}

void CvProperties::buildRequiresMinString(CvWStringBuffer& szBuffer, const CvProperties* pProp) const
{
	PROFILE_EXTRA_FUNC();
	const int num = getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (!pProp || pProp->getValueByProperty(getProperty(index)) < getValue(index))
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText(GC.getPropertyInfo((PropertyTypes)getProperty(index)).getPrereqMinDisplayText(), getValue(index)));
		}
	}
}

void CvProperties::buildRequiresMaxString(CvWStringBuffer& szBuffer, const CvProperties* pProp) const
{
	PROFILE_EXTRA_FUNC();
	const int num = getNumProperties();
	for (int index = 0; index < num; index++)
	{
		if (!pProp || pProp->getValueByProperty(getProperty(index)) > getValue(index))
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText(GC.getPropertyInfo((PropertyTypes)getProperty(index)).getPrereqMaxDisplayText(), getValue(index)));
		}
	}
}

void CvProperties::buildChangesString(CvWStringBuffer& szBuffer, CvWString* pszCity) const
{
	PROFILE_EXTRA_FUNC();
	const int num = getNumProperties();
	for (int iI = 0; iI < num; iI++)
	{
		szBuffer.append(NEWLINE);
		if (pszCity)
		{
			szBuffer.append(*pszCity);
			szBuffer.append(": ");
		}
		CvWString szTemp;
		szTemp.Format(L"%c: %+d", GC.getPropertyInfo((PropertyTypes)getProperty(iI)).getChar(), getValue(iI));
		szBuffer.append(szTemp);
	}
}

void CvProperties::buildCompactChangesString(CvWStringBuffer& szBuffer) const
{
	PROFILE_EXTRA_FUNC();
	const int num = getNumProperties();
	for (int iI = 0; iI < num; iI++)
	{
		CvWString szTemp;
		szTemp.Format(L" %d%c", getValue(iI), GC.getPropertyInfo((PropertyTypes)getProperty(iI)).getChar());
		szBuffer.append(szTemp);
	}
}

void CvProperties::buildChangesAllCitiesString(CvWStringBuffer& szBuffer) const
{
	PROFILE_EXTRA_FUNC();
	const int num = getNumProperties();
	for (int iI = 0; iI < num; iI++)
	{
		szBuffer.append(NEWLINE);
		CvWString szTemp;
		szTemp.Format(L"%c (All Cities): %+d", GC.getPropertyInfo((PropertyTypes)getProperty(iI)).getChar(), getValue(iI));
		szBuffer.append(szTemp);
	}
}

void CvProperties::buildDisplayString(CvWStringBuffer& szBuffer) const
{
	PROFILE_EXTRA_FUNC();
	const int num = getNumProperties();
	for (int iI = 0; iI < num; iI++)
	{
		szBuffer.append(NEWLINE);
		CvWString szTemp;
		szTemp.Format(L"%c: %+d", GC.getPropertyInfo((PropertyTypes)getProperty(iI)).getChar(), getValue(iI));
		szBuffer.append(szTemp);
	}
}

std::wstring CvProperties::getPropertyDisplay(int index) const
{
	CvWString szTemp;
	if (index < getNumProperties())
		szTemp.Format(L"%c %s", GC.getPropertyInfo((PropertyTypes)getProperty(index)).getChar(), GC.getPropertyInfo((PropertyTypes)getProperty(index)).getText());
	return szTemp.GetCString();
}

void CvProperties::getCheckSum(unsigned int &iSum) const
{
	CheckSumC(iSum, m_aiProperty);
}
