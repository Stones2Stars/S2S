#pragma once

#ifndef CV_BONUSINFO_H
#define CV_BONUSINFO_H

//
//	CvBonusInfo -- the EXE SHIM LEAF for resources (cascade-engine-430.md #3: mapscripts are launched by the
//	EXE, which reads bonus art around map generation). The old class name returns as a THIN LEAF on the JSON
//	chain (CvInfoBase -> CvJsonInfo -> CvJsonBonusInfo -> CvBonusInfo) carrying ONLY the EXE-called DllExport
//	method -- the EXE import binds on decorated class-name + signature, never layout. All data lives on the
//	CvJsonBonusInfo poco; the art resolves through ArtFileMgr keyed by the art-define tag (world.art.icon).
//

#include "CvJsonBonusInfo.h"

class CvArtInfoBonus;

class CvBonusInfo : public CvJsonBonusInfo
{
public:
	DllExport const CvArtInfoBonus* getArtInfo() const;
};

#endif // CV_BONUSINFO_H
