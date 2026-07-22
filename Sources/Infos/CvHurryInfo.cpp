//------------------------------------------------------------------------------------------------
//  FILE:    CvHurryInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvHurryInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool


CvHurryInfo::CvHurryInfo()
	: m_iGoldPerProduction(0)
	, m_iProductionPerPopulation(0)
	, m_bAnger(false)
{
}


// #430: conversion.{goldPerProduction,productionPerPopulation} (mutually exclusive rush rates, raw ints,
// each absent -> 0); causesAnger top-level flag -> bAnger.
void CvHurryInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* c = jsonChildObj(o, "conversion"))
	{
		m_iGoldPerProduction       = jsonIdInt(*c, "goldPerProduction");
		m_iProductionPerPopulation = jsonIdInt(*c, "productionPerPopulation");
	}
	m_bAnger = jsonIdBool(o, "causesAnger");
}
