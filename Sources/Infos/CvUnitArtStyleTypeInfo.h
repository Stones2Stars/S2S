#pragma once

#ifndef CV_UNIT_ART_STYLE_TYPE_INFO_H
#define CV_UNIT_ART_STYLE_TYPE_INFO_H

#include "CvInfoBase.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvUnitArtStyleTypeInfo
//
//  A registered info type (CIV4UnitArtStyleTypeInfos.xml, "Civilizations") that carries
//  nothing beyond its own identity. Unit art resolves through the single art-define tag
//  held by CvUnitInfo: CvUnitInfo::getArtInfo() ignores its era and style arguments, so a
//  style contributes no art data. What stays live is the Type/Description that
//  CvInfoBase::read() parses -- CvCivilizationInfo::getUnitArtStyleType() resolves a
//  civilization's style by Type against this registry.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvUnitArtStyleTypeInfo
	: public CvInfoBase
	, private bst::noncopyable
{
public:

	CvUnitArtStyleTypeInfo();
	virtual ~CvUnitArtStyleTypeInfo();
};

#endif // CV_UNIT_ART_STYLE_TYPE_INFO_H
