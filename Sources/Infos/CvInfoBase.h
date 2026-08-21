#pragma once

//------------------------------------------------------------------------------------------------
//
//  FILE:   CvInfoBase.h
//
//  PURPOSE: Foundational base classes for the Cv*Info hierarchy.
//
//------------------------------------------------------------------------------------------------
#ifndef CV_INFOBASE_H
#define CV_INFOBASE_H

#include "Tools/FProfiler.h"

#include "Engine/CvProperties.h"
#include "Engine/CvPropertySource.h"
#include "Engine/CvPropertyInteraction.h"
#include "Engine/CvPropertyPropagator.h"
#include "Engine/CvPropertyManipulators.h"
#include "UI/CvOutcomeList.h"
#include "Engine/CvOutcomeMission.h"
#include "Engine/CvDate.h"
#include "Infrastructure/BoolExpr.h"
//#include "Infrastructure/IntExpr.h"
#include "Infrastructure/IDValueMap.h"
#include <boost/python/list.hpp>

extern bool shouldHaveType;

class CvXMLLoadUtility;
class IntExpr;
struct CvInfoUtil;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvInfoBase
//
//  DESC:   The base class for all info classes to inherit from.  This gives us
//			the base description and type strings
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

class CvInfoBase
{
//---------------------------PUBLIC INTERFACE---------------------------------
public:
	CvInfoBase();
	explicit CvInfoBase(const char* szType);
	DllExport virtual ~CvInfoBase();

	virtual void reset();

	bool isGraphicalOnly() const;

	DllExport const char* getType() const;
	virtual const char* getButton() const;

	const wchar_t* getCivilopediaKey() const;
	const wchar_t* getHelpKey() const;
	const wchar_t* getStrategyKey() const;

	// for python wide string handling
	std::wstring pyGetTextKey() const		{ return getTextKeyWide(); }
	std::wstring pyGetDescription() const	{ return getDescription(0); }
	std::wstring pyGetDescriptionForm(uint uiForm) const { return getDescription(uiForm); }
	std::wstring pyGetText() const			{ return getText(); }
	std::wstring pyGetCivilopedia() const	{ return getCivilopedia(); }
	std::wstring pyGetHelp() const			{ return getHelp(); }
	std::wstring pyGetStrategy() const		{ return getStrategy(); }

	DllExport const wchar_t* getTextKeyWide() const;
	DllExport const wchar_t* getDescription(uint uiForm = 0) const;
	DllExport const wchar_t* getText() const;
	const wchar_t* getCivilopedia() const;
	DllExport const wchar_t* getHelp() const;
	const wchar_t* getStrategy() const;

	virtual void read(FDataStreamBase*) {}
	virtual void write(FDataStreamBase*) {}

	// Empty default: a class that hasn't declared any fields contributes nothing to the declarative
	// read/link/checksum passes (it still uses its hand-written read()). Overridden by migrated
	// classes. The empty base is what lets the uniform link phase call this on every info.
	virtual void getDataMembers(CvInfoUtil&) {}
	virtual bool read(CvXMLLoadUtility* pXML);
	virtual bool readPass2(CvXMLLoadUtility*) { FErrorMsg("Override this"); return false; }
	virtual bool readPass3() { FErrorMsg("Override this"); return false; }
	virtual void copyNonDefaults(const CvInfoBase* pClassInfo);
	virtual void copyNonDefaultsReadPass2(CvInfoBase*, CvXMLLoadUtility*, bool = false)
	{ /* AIAndy: Default implementation for full copy of info without knowledge of one/twopass */ }
	virtual void getCheckSum(uint32_t&) const { }
	virtual const wchar_t* getExtraHoverText() const { return L""; }

protected:

	bool doneReadingXML(CvXMLLoadUtility* pXML);
	/// <summary>Frees the heap-owned cached descriptions. The ONE free path -- both the destructor and reset()
	/// call it, so the delete loop cannot drift between them.</summary>
	void clearCachedDescriptions() const;

	bool m_bGraphicalOnly;
	CvString m_szType;
	CvString m_szButton; // Used for Infos that don't require an ArtAssetInfo
	CvWString m_szTextKey;
	CvWString m_szCivilopediaKey;
	CvWString m_szHelpKey;
	CvWString m_szStrategyKey;

	// translated text
	std::vector<CvString> m_aszExtraXMLforPass3;
	//	⛔ POINTERS, and the element type is the ONLY thing that may change here. getDescription hands the EXE a
	//	`const wchar_t*` into an element and the EXE KEEPS it; holding the strings BY VALUE meant a later
	//	push_back reallocated the vector, moved every element, and left every pointer issued earlier pointing at
	//	freed heap (seen as msvcr71!wcslen faulting on a garbage, non-zero, varying address with the EXE as the
	//	only caller). Heap-owned strings never move, so an issued pointer stays valid for the life of the info.
	//	⛔ sizeof(std::vector<T>) is identical for every T, so this does NOT move a single member. Changing the
	//	CONTAINER would: CvInfoBase's layout is bound by the closed EXE (AGENTS.md, patterns.md) -- a std::deque
	//	here crashed the game on load, in the EXE's own basic_string::assign, exactly as adding a member did.
	mutable std::vector<CvWString*> m_aCachedDescriptions;
	mutable CvWString m_szCachedText;
	mutable CvWString m_szCachedHelp;
	mutable CvWString m_szCachedStrategy;
	mutable CvWString m_szCachedCivilopedia;
};

//
// holds the scale for scalable objects
//
class CvScalableInfo
{
public:

	CvScalableInfo() : m_fScale(1.0f), m_fInterfaceScale(1.0f) { }

	DllExport float getScale() const;
	DllExport float getInterfaceScale() const;

	// Declarative field registry for the mixin; NOT virtual (CvScalableInfo is not a CvInfoBase).
	// Chained explicitly from CvArtInfoScalableAsset::getDataMembers. The hand-written
	// read()/copyNonDefaults() below are kept for the non-declarative consumers of this mixin
	// (CvAttachableInfo, CvEffectInfo) and must stay in sync with getDataMembers.
	void getDataMembers(CvInfoUtil& util);
	bool read(CvXMLLoadUtility* pXML);
	void copyNonDefaults(const CvScalableInfo* pClassInfo);

protected:

	float m_fScale;
	float m_fInterfaceScale;	//!< the scale of the unit appearing in the interface screens
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CLASS:	  CvHotkeyInfo
//!  \brief			holds the hotkey info for an info class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvHotkeyInfo : public CvInfoBase
{
public:
	//constructor
	CvHotkeyInfo();
	//destructor
	virtual ~CvHotkeyInfo();

	bool read(CvXMLLoadUtility* pXML);
	void copyNonDefaults(const CvHotkeyInfo* pClassInfo);

	int getActionInfoIndex() const;
	void setActionInfoIndex(int i);

	int getHotKeyVal() const;
	int getHotKeyPriority() const;
	int getHotKeyValAlt() const;
	int getHotKeyPriorityAlt() const;
	int getOrderPriority() const;

	bool isAltDown() const;
	bool isShiftDown() const;
	bool isCtrlDown() const;
	bool isAltDownAlt() const;
	bool isShiftDownAlt() const;
	bool isCtrlDownAlt() const;
	const char* getHotKey() const;
	const wchar_t* getHotKeyDescriptionKey() const;
	const wchar_t* getHotKeyAltDescriptionKey() const;
	const wchar_t* getHotKeyString() const;
	std::wstring pyGetHotKeyString() const { return getHotKeyString(); }

	std::wstring getHotKeyDescription() const;
	void setHotKeyDescription(const wchar_t* szHotKeyDescKey, const wchar_t* szHotKeyAltDescKey, const wchar_t* szHotKeyString);

protected:

	int m_iActionInfoIndex;

	int m_iHotKeyVal;
	int m_iHotKeyPriority;
	int m_iHotKeyValAlt;
	int m_iHotKeyPriorityAlt;
	int m_iOrderPriority;

	bool m_bAltDown;
	bool m_bShiftDown;
	bool m_bCtrlDown;
	bool m_bAltDownAlt;
	bool m_bShiftDownAlt;
	bool m_bCtrlDownAlt;

	CvString m_szHotKey;
	CvWString m_szHotKeyDescriptionKey;
	CvWString m_szHotKeyAltDescriptionKey;
	CvWString m_szHotKeyString;
};

#endif // CV_INFOBASE_H
