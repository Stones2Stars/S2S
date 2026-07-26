#pragma once
#ifndef CV_ENTRY_TEXT_H
#define CV_ENTRY_TEXT_H

//
//	CvEntryText -- the ONE per-entry text renderer (patterns.md § THE GETTER SETUP category 5; info-rebuild.md
//	ruling 29; [DEC-single-implementation]). Every compiled §3.9 modifier entry renders itself as ONE localized
//	detail line -- the CvCombatModel::computeCombatPreview detailLines pattern: a complete, ready-to-render
//	string the consumer prints generically, so tooltip/pedia composers consume rendered entry lines and never
//	hand-assemble from getters. COLD PATH by design (clarity over speed): names resolve through the UNCACHED
//	gDLL->getObjectText read (TXT keys where the vocabulary reaches them -- channel/property/FK-target infos);
//	everything the TXT infrastructure cannot reach spells back its authored segment (the honest fallback).
//
//	Line grammar (one line, left to right):
//	  <sign><magnitude><unit> <family/kind name>[ for <target>][, <scope phrase>][ per <per-scaler>]
//	  [ -- top N by <metric>][ -- while <enabled>][ -- unless <disabled>]
//	  [ -- units matching <unitQual>][ -- per city religion matching <religionQual>][ [AI only]]
//	The magnitude is /100 at THIS out boundary (the reader rule, [DEC-fixedpoint-x100]).
//

#include "Defines/CvString.h"   // CvWString -- the rendered line type

class CvModEntry;
class CvCondition;

// ONE compiled entry -> one localized detail line (see the grammar above).
CvWString entryDetailLine(const CvModEntry& entry);

// The minimal condition-to-text renderer (no other condition renderer exists in the tree -- census 2026-07):
// a prebuilt CvCondition tree -> one phrase ("Coal connected", "IS_WATER and not HAS_HILLS", "3+ Grassland").
// Shared by the detail line's enabled/disabled/qualifier clauses; future requires/gate renders reuse it.
CvWString entryConditionText(const CvCondition* condition);

#endif // CV_ENTRY_TEXT_H
