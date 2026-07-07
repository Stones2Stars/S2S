#pragma once
#ifndef CV_JSON_TECH_INFO_H
#define CV_JSON_TECH_INFO_H

//
//	CvJsonTechInfo -- the per-type cascade info for TECHS. Extension over the base: the empire-ability blocks this
//	tech PROVIDES when held (json.md §8; capabilities.md) -- the flat `capabilities` plus the three sibling blocks
//	`canTrade` (trade-table items/agreements), `canTradeOn` (tradable TERRAIN_ refs, FK-resolved) and `canWorkOn`
//	(coarse workable plot classes). Techs are the only grantor authored in data TODAY (the model allows civic/
//	building grantors -- owner 2026-07-02, capabilities.md; widen the union when data authors them). The empire's
//	ACTIVE set is derived on query -- CascadeCapabilities is the query surface; nothing is stored on a team object.
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonTechInfo : public CvJsonInfo
{
public:
	std::set<std::string> capabilities;   // the `capabilities:{name:true}` block -- the granted capability names
	std::set<std::string> canTrade;       // the `canTrade:{item:true}` block -- trade-table items/agreements unlocked
	std::set<int> canTradeOnTerrains;     // the `canTradeOn:{terrains:[TERRAIN_..]}` block -- FK-resolved terrain ids
	std::set<std::string> canWorkOn;      // the `canWorkOn:{class:true}` block -- workable plot classes (water/..)
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_TECH_INFO_H
