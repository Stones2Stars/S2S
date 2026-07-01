#pragma once
#ifndef CV_JSON_CORPORATION_INFO_H
#define CV_JSON_CORPORATION_INFO_H

//
//	CvJsonCorporationInfo -- the per-type cascade info for CORPORATIONS (ports StoneBase's CorporationInfo: modifier
//	families on the base). The per-city corp commerce (getCommerceChange + the per-prereq-bonus getCommerceProduced) is
//	read from the base `deposits` -- the per-bonus scaler lives on the deposit (CvCascadeDeposit.perAnyOf), NOT a field
//	here (owner ruling 2026-06-30: map the corp commerce properly as deposits + the per list, not StoneBase's per-hack).
//

#include "CvJsonInfo.h"

class CvJsonCorporationInfo : public CvJsonInfo
{
};

#endif // CV_JSON_CORPORATION_INFO_H
