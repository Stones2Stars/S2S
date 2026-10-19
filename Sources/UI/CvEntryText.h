#pragma once
#ifndef CV_ENTRY_TEXT_H
#define CV_ENTRY_TEXT_H

//
//	CvEntryText -- the ONE per-entry text renderer (patterns.md § THE GETTER SETUP category 5; info-rebuild.md
//	ruling 29; docs/architecture/patterns.md §DRY (single implementation)). Every compiled §3.9 modifier entry renders itself as ONE localized
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
//	The `while` clause is left off when a page subject is named and the clause is only a plain mention of it. A
//	condition that only names a kind of unit reads "-- for <tag> units" / "-- except for <tag> units". A predicate
//	renders as its player-facing phrase (TXT_KEY_CONDITION_*: "in your capital"), never as its JSON token.
//	A unit's own strength and moves, and a property that has a font glyph, render as their glyphs; a combat
//	membership flag carries no magnitude: it reads "<container heading>: <target>".
//

#include "Defines/CvString.h"   // CvWString -- the rendered line type
#include "Infos/CvEdges.h"      // EnEdgeBucket -- the page-subject axis
#include "Infos/CvCondition.h"  // CvCascPredKind -- the predicate infoHasEntryConditionedOn asks about

class CvModEntry;
class CvInfo;

///<summary>One compiled entry as one localized detail line (the grammar above). A page SUBJECT may be named:
/// when the entry's `enabled` condition is nothing but a plain mention of that subject, the "while" clause is
/// left off, because under the subject's own heading it restates the heading.</summary>
CvWString entryDetailLine(const CvModEntry& entry, EnEdgeBucket eSubjectBucket = NO_EDGEB, int iSubjectId = -1);

///<summary>The same line for a value the engine resolved the entry to at one object (a building's deposit in its
/// city): the resolved magnitude, with the `per` clause left off because the scaling is already in the number.</summary>
CvWString entryResolvedLine(const CvModEntry& entry, int64_t iResolvedValue);

///<summary>Do the game's options alone rule this entry out for the current game? Such an entry is not listed,
/// and a game-option atom is never printed in a condition clause, because an option is fixed for the game.</summary>
bool entryHiddenByGameOptions(const CvModEntry& entry);

// Is this a PLAIN FLAT CHANNEL deposit -- a yield or commerce AMOUNT with nothing qualifying it: no condition,
// no target, no member, no per-scaler, no unit/religion/rank qualifier, not AI-only?
// ⚖ Those group onto ONE line ("+2<food> +1<hammer>") because that is what a player reads at a glance and what
// legacy showed; anything CARRYING something keeps its own line, since the thing it carries is the half worth
// reading ("+25%<hammer> -- while Coal connected"). The two renderings are the same grammar at two densities,
// so the test that sorts them lives HERE beside both rather than in each composer.
bool entryIsPlainFlatChannel(const CvModEntry& entry);

// The COMPACT rendering of one of those: magnitude then glyph, no separating space and no trailing clause
// ("+2<food>") -- the same idiom the wellbeing pair already renders in.
CvWString entryFlatSummary(const CvModEntry& entry);

///<summary>The same compact rendering for a yield amount that is not an entry, such as a plot's total: magnitude
/// then glyph ("3<food>").</summary>
CvWString yieldAmountSummary(YieldTypes eYield, int64_t iAmount);

///<summary>An amount as the entry lines print it: "2", "-0.5", "12.75".</summary>
CvWString amountText(int64_t iAmount);

///<summary>A sight, detection or concealment amount in plots of open ground ([vision.md] §1a): "2",
/// "1.25".</summary>
CvWString visionPlotsText(int iVisionAmount);

///<summary>Does the info carry an entry whose `enabled` condition tests ePredicate? For a composer that states the
/// verdict of that predicate beside the entity, because one of its entries depends on it.</summary>
bool infoHasEntryConditionedOn(const CvInfo& info, CvCascPredKind ePredicate);

///<summary>A unit's strength as the unit and stack lines show it, from the engine's current and full strength:
/// "4<strength>" at full health, "3.5/4<strength>" when damaged, "?/4<strength>" when the current value cannot be
/// answered (mid-combat). The one rendering both lines go through, so they read alike.</summary>
CvWString unitStrengthText(int iCurrentStrength, int iFullStrength, bool bCurrentUnknown);

///<summary>A per-turn property amount a unit delivers, as the entry lines show a property: "-6<crime>" to its city,
/// "-3<crime>, on this plot" to its plot. The property's name stands in where its font glyph is not set.</summary>
CvWString propertySourceLine(PropertyTypes eProperty, int iAmountPerTurn, bool bOnPlot);

///<summary>A live unit's upkeep (CvUnit::getUpkeep) as the number a tooltip prints: "2", "2.5".</summary>
CvWString upkeepAmountText(int iUpkeep100);

///<summary>A sight amount, in the engine's vision scale, as plots of open ground: "2 vision", "1.25 vision".</summary>
CvWString visionAmountText(int iSight);

///<summary>Is this a unit's plain strength, moves or sight amount -- unconditioned, untargeted, memberless? On a
/// unit TYPE that is its base value rather than a bonus; the same key on a promotion is a bonus. Only the
/// composer knows which it is composing.</summary>
bool entryIsUnitBaseStat(const CvModEntry& entry);

///<summary>One of those as a unit type's base value, unsigned: "2<strength>", "1<moves>", "1 vision".</summary>
CvWString entryUnitBaseStatText(const CvModEntry& entry);

///<summary>Is this a combat MEMBERSHIP flag, a `combat.unit.<container>.{TYPE}: true` key such as the targets and
/// defenders lists? It names an entity and carries no magnitude, so it never renders as "+1".</summary>
bool entryIsMembershipFlag(const CvModEntry& entry);

///<summary>What a membership flag's container means, as the heading its targets are listed under ("Targets first
/// in combat", "Defends first against"). Several flags of one container can share one line.</summary>
CvWString entryMembershipHeading(const CvModEntry& entry);

///<summary>The name of the entity a membership flag lists ("Criminals").</summary>
CvWString entryMembershipTarget(const CvModEntry& entry);

///<summary>A hideAndSeek concealment as a line, in plots on the vision scale ("+3 concealment").</summary>
CvWString hideAndSeekConcealmentLine(int iConcealment);

///<summary>One hideAndSeek detection row -- a hiding method's SKILL id and the detection against it -- as a line,
/// in plots on the vision scale ("+2 detection vs Disguised").</summary>
CvWString hideAndSeekDetectionLine(int iMethodSkill, int iDetection);

///<summary>Is this an unconditioned, untargeted `combat.unit` amount? Those read together on one "Combat:"
/// line; a keyed or conditioned one keeps its own line.</summary>
bool entryIsPlainCombatMember(const CvModEntry& entry);

///<summary>One of those as an item of that line, without the family word: "+4% dynamic defense",
/// "+10% strength" for the memberless amount.</summary>
CvWString entryCombatSummary(const CvModEntry& entry);

// The minimal condition-to-text renderer (no other condition renderer exists in the tree -- census 2026-07):
// a prebuilt CvCondition tree -> one phrase ("Coal connected", "IS_WATER and not HAS_HILLS", "3+ Grassland").
// Shared by the detail line's enabled/disabled/qualifier clauses; future requires/gate renders reuse it.
CvWString entryConditionText(const CvCondition* condition);

// A §8/§9 CLASSIFICATION KEY -> a readable name ("canBuildBridges" -> "Can Build Bridges"). The classification
// registries are minted from the authored keys at load (docs/specs/json.md §8 + docs/architecture/patterns.md §The coherent surface (THE GETTER SETUP)), so no TXT key exists for
// one and the authored spelling IS the vocabulary -- this is the same honest spell-back the detail line uses for
// everything TXT cannot reach, differing only in that a block key is camelCase where a token is underscored.
CvWString entryClassificationName(const std::string& szKey);

#endif // CV_ENTRY_TEXT_H
