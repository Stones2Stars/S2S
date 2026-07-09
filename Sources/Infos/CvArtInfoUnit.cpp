//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoUnit.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvJsonBuildingInfo.h"
#include "CvJsonHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvArtInfoUnit.h"



/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoUnit
/////////////////////////////////////////////////////////////////////////////////////////////

CvArtInfoUnit::CvArtInfoUnit()
	// Non-declarative fields (read by the hand-written part of read(), see getDataMembers)
	: m_fShadowScale(0.0f)
	, m_fTrailWidth(0.0f)
	, m_fTrailLength(0.0f)
	, m_fTrailTaper(0.0f)
	, m_fTrailFadeStartTime(0.0f)
	, m_fTrailFadeFalloff(0.0f)
	, m_iRunLoopSoundTag(0)
	, m_iRunEndSoundTag(0)
	, m_iSelectionSoundScriptId(0)
	, m_iActionSoundScriptId(0)
	, m_iPatrolSoundTag(0)
{
	CvInfoUtil(this).initDataMembers();
}


CvArtInfoUnit::~CvArtInfoUnit()
{
}


bool CvArtInfoUnit::getActAsRanged() const
{
	return m_bActAsRanged;
}


bool CvArtInfoUnit::getActAsLand() const
{
	return m_bActAsLand;
}


bool CvArtInfoUnit::getActAsAir() const
{
	return m_bActAsAir;
}


const char* CvArtInfoUnit::getShaderNIF() const
{
	return m_szShaderNIF;
}


void CvArtInfoUnit::setShaderNIF(const char* szDesc)
{
	m_szShaderNIF = szDesc;
}


const char* CvArtInfoUnit::getShadowNIF() const
{
	return m_szShadowNIF;
}


float CvArtInfoUnit::getShadowScale() const
{
	return m_fShadowScale;
}


const char* CvArtInfoUnit::getShadowAttachNode() const
{
	return m_szShadowAttach;
}


int CvArtInfoUnit::getDamageStates() const
{
	return m_iDamageStates;
}



const char* CvArtInfoUnit::getTrailTexture() const
{
	return m_szTrailTexture;
}


float CvArtInfoUnit::getTrailWidth() const
{
	return m_fTrailWidth;
}


float CvArtInfoUnit::getTrailLength() const
{
	return m_fTrailLength;
}


float CvArtInfoUnit::getTrailTaper() const
{
	return m_fTrailTaper;
}


float CvArtInfoUnit::getTrailFadeStarTime() const
{
	return m_fTrailFadeStartTime;
}


float CvArtInfoUnit::getTrailFadeFalloff() const
{
	return m_fTrailFadeFalloff;
}


float CvArtInfoUnit::getBattleDistance() const
{
	return m_fBattleDistance;
}


float CvArtInfoUnit::getRangedDeathTime() const
{
	return m_fRangedDeathTime;
}


float CvArtInfoUnit::getExchangeAngle() const
{
	return m_fExchangeAngle;
}


bool CvArtInfoUnit::getCombatExempt() const
{
	return m_bCombatExempt;
}


bool CvArtInfoUnit::getSmoothMove() const
{
	return m_bSmoothMove;
}


float CvArtInfoUnit::getAngleInterpRate() const
{
	return m_fAngleInterRate;
}


float CvArtInfoUnit::getBankRate() const
{
	return m_fBankRate;
}


void CvArtInfoUnit::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoScalableAsset::getDataMembers(util);
	util
		.add(m_bActAsRanged, L"bActAsRanged")
		.add(m_bActAsLand, L"bActAsLand")
		.add(m_bActAsAir, L"bActAsAir")
		.add(m_bCombatExempt, L"bCombatExempt")
		.add(m_fExchangeAngle, L"fExchangeAngle")
		.add(m_bSmoothMove, L"bSmoothMove")
		.add(m_fAngleInterRate, L"fAngleInterpRate", FLT_MAX)
		.add(m_fBankRate, L"fBankRate")
		.add(m_iDamageStates, L"iDamageStates")
		.add(m_fBattleDistance, L"fBattleDistance")
		.add(m_fRangedDeathTime, L"fRangedDeathTime")
		.add(m_szTrainSound, L"TrainSound")
		.add(m_szShaderNIF, L"SHADERNIF")
	;
}


bool CvArtInfoUnit::read(CvXMLLoadUtility* pXML)
{
	// Single declarative delegation (covers this class's declared fields too via virtual dispatch)
	if (!CvArtInfoScalableAsset::read(pXML))
	{
		return false;
	}

	CvString szTextVal;

	pXML->GetOptionalChildXmlValByName(szTextVal, L"ActionSound");
	m_iActionSoundScriptId = (szTextVal.GetLength() > 0) ? gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_3DSCRIPT ) : -1;
	pXML->GetOptionalChildXmlValByName(szTextVal, L"SelectionSound");
	m_iSelectionSoundScriptId = (szTextVal.GetLength() > 0) ? gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_3DSCRIPT ) : -1;
	pXML->GetOptionalChildXmlValByName(szTextVal, L"PatrolSound");
	m_iPatrolSoundTag = (szTextVal.GetLength() > 0) ? gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_3DSCRIPT ) : -1;

	if ( pXML->TryMoveToXmlFirstChild(L"ShadowDef" ))
	{
		pXML->GetOptionalChildXmlValByName( m_szShadowAttach, L"ShadowAttachNode" );
		pXML->GetOptionalChildXmlValByName(m_szShadowNIF, L"ShadowNIF");
		pXML->GetOptionalChildXmlValByName(&m_fShadowScale, L"fShadowScale");
		pXML->MoveToXmlParent();
	}

	m_fTrailWidth = -1.0f; // invalid.
	if (pXML->TryMoveToXmlFirstChild(L"TrailDefinition"))
	{
		pXML->GetChildXmlValByName(m_szTrailTexture, L"Texture" );
		pXML->GetOptionalChildXmlValByName(&m_fTrailWidth,  L"fWidth", -1.0f);
		pXML->GetOptionalChildXmlValByName(&m_fTrailLength, L"fLength" );
		pXML->GetOptionalChildXmlValByName(&m_fTrailTaper,  L"fTaper" );
		pXML->GetOptionalChildXmlValByName(&m_fTrailFadeStartTime, L"fFadeStartTime" );
		pXML->GetOptionalChildXmlValByName(&m_fTrailFadeFalloff,   L"fFadeFalloff" );
		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"AudioRunSounds"))
	{
		pXML->GetOptionalChildXmlValByName(szTextVal, L"AudioRunTypeLoop");
		m_iRunLoopSoundTag = GC.getFootstepAudioTypeByTag(szTextVal);
		pXML->GetOptionalChildXmlValByName(szTextVal, L"AudioRunTypeEnd");
		m_iRunEndSoundTag = GC.getFootstepAudioTypeByTag(szTextVal);
		pXML->MoveToXmlParent();
	}

	return true;
}


void CvArtInfoUnit::copyNonDefaults(const CvArtInfoUnit* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}


const char* CvArtInfoUnit::getTrainSound() const
{
	return m_szTrainSound;
}


void CvArtInfoUnit::setTrainSound(const char* szVal)
{
	m_szTrainSound = szVal;
}


int CvArtInfoUnit::getRunLoopSoundTag() const
{
	return m_iRunLoopSoundTag;
}


int CvArtInfoUnit::getRunEndSoundTag() const
{
	return m_iRunEndSoundTag;
}


int CvArtInfoUnit::getPatrolSoundTag() const
{
	return m_iPatrolSoundTag;
}


int CvArtInfoUnit::getSelectionSoundScriptId() const
{
	return m_iSelectionSoundScriptId;
}


int CvArtInfoUnit::getActionSoundScriptId() const
{
	return m_iActionSoundScriptId;
}

