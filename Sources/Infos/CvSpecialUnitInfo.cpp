//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialUnitInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvSpecialUnitInfo.h"
#include "CvJsonParse.h"          // jsonFamVal / jsonChildObj / jsonIdBool


CvSpecialUnitInfo::CvSpecialUnitInfo()
	: m_bValid(true)          // legacy default TRUE (curator elides valid:true; only an explicit valid:false overrides)
	, m_bCityLoad(false)
	, m_bSMLoadSame(false)
	, m_iCombatPercent(0)
	, m_iWithdrawalChange(0)
{
}


// #430: strength.unit.percent -> iCombatPercent, withdrawal.unit.percent -> iWithdrawalChange (raw percents);
// identity.{cityLoad,smLoadSame} flags; identity.valid overrides the true default only when explicitly present.
void CvSpecialUnitInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_iCombatPercent    = jsonFamVal(o, "strength", "unit", "percent");
	m_iWithdrawalChange = jsonFamVal(o, "withdrawal", "unit", "percent");

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_bCityLoad   = jsonIdBool(*io, "cityLoad");
		m_bSMLoadSame = jsonIdBool(*io, "smLoadSame");
		picojson::object::const_iterator it = io->find("valid");
		if (it != io->end() && it->second.is<bool>()) m_bValid = it->second.get<bool>();
	}
}
