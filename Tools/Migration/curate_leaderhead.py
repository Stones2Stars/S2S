#!/usr/bin/env python3
"""Curate LeaderHead (#428, Tier D #30) — the AI PERSONALITY entity: ~90 AI/diplomacy/strategy params that
define how an AI player behaves all game. NOT a cascade source/target (zero per-turn-effect fields) — virtually
everything lands in the unified `ai` group, subgrouped (this is the densest `ai` entity, so it shapes the `ai`
sub-group vocabulary; the authored-object `ai` group, json.md). 119 records, base only. EXE-link: 1 DllExport (getArtInfo
-> ArtDefineTag is EXE-bound; all else unconstrained). Bespoke curator.

`ai` subgroups (owner 2026-06-16: keep granular — a big ai block is expected for the AI-behaviour entity):
- flavours          <- Flavors (FLAVOR_* weights)
- personality       <- the misc personality knobs (baseAttitude/peaceWeight/warmongerRespect/espionageWeight/
                       wonderConstructRand/buildUnitProb/freedomAppreciation/vassalPowerModifier)
- war               <- war/peace decision rands (maxWar*/limitedWar*/dogpile/makePeace/declareWarTrade/
                       demandRebuked*/refuseToTalkWar/baseAttackOddsChange/attackOddsChangeRand/razeCityProb)
- victory           <- {culture,space,conquest,domination,diplomacy}VictoryWeight
- trade             <- maxGold{,PerTurn}TradePercent / noTechTradeThreshold / techTradeKnownPercent
- attitude          <- relation families {change?,divisor?,changeLimit?}: closeBorders, lostWar, atWar, atPeace,
                       same/differentReligion, bonusTrade, openBorders, defensivePact, shareWar, favoriteCivic,
                       worse/betterRankDifference
- refuse            <- the *AttitudeThreshold strings (ATTITUDE_*): the min attitude to agree to each deal/action
- memory            <- MemoryDecays (rand) + MemoryAttitudePercents (percent), keyed by MEMORY_*
- contact           <- ContactRands + ContactDelays, keyed by CONTACT_*
- noWarProb         <- NoWarAttitudeProbs, keyed by ATTITUDE_*
- unitWeights       <- UnitAIWeightModifiers, keyed by UNITAI_*
- improvementWeights<- ImprovementWeightModifiers, keyed by IMPROVEMENT_*
- favorites         <- FavoriteCivic, FavoriteReligion (AI attitude drivers; owner-agreed -> ai)

Non-ai:
- ArtDefineTag                 -> world.art.icon (EXE-bound leaderhead portrait).
- Diplomacy{Intro,}Music{Peace,War} -> sound.diplo* (era -> DiploScriptId maps; audio).
- bNPC                         -> ai.npc (barbarian/NPC leader; an AI classification; 3 leaders).
- Description / Civilopedia    -> text.

TRAITS -> STRIPPED for now (owner ruling 2026-07-01). ALL leader trait assignments — `Traits`, `DefaultTraits`,
`DefaultComplexTraits` (simple AND complex) — are DROPPED; **no leader carries traits in the JSON**. The
leader<->trait mapping (incl. the simple->complex mirroring, which depends on the simple<->complex correspondence
the TRAIT pass deliberately dropped) is handed to a dedicated POST-MIGRATION pass; whoever does it re-adds a
`grants.traits` emit here. Pre-cutover the game still runs traits off XML, so stripping the JSON assignments is
safe — it just marks them TODO. (Mid-game trait-type swapping is also catastrophic — see WorldBuilder-safe-swap #438.)

  python3 curate_leaderhead.py --sample LEADER_ALEXANDER LEADER_GANDHI LEADER_BARBARIAN
  python3 curate_leaderhead.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import Store, REPO

# --- flat scalar AI knobs: tag -> (subgroup, key). Only non-zero ints emitted. ---
SCALAR = {
    # personality
    "iBaseAttitude": ("personality", "baseAttitude"), "iBasePeaceWeight": ("personality", "basePeaceWeight"),
    "iPeaceWeightRand": ("personality", "peaceWeightRand"), "iWarmongerRespect": ("personality", "warmongerRespect"),
    "iEspionageWeight": ("personality", "espionageWeight"), "iWonderConstructRand": ("personality", "wonderConstructRand"),
    "iBuildUnitProb": ("personality", "buildUnitProb"), "iFreedomAppreciation": ("personality", "freedomAppreciation"),
    "iVassalPowerModifier": ("personality", "vassalPowerModifier"),
    # war / peace decisions
    "iMaxWarRand": ("war", "maxWarRand"), "iMaxWarNearbyPowerRatio": ("war", "maxWarNearbyPowerRatio"),
    "iMaxWarDistantPowerRatio": ("war", "maxWarDistantPowerRatio"), "iMaxWarMinAdjacentLandPercent": ("war", "maxWarMinAdjacentLandPercent"),
    "iLimitedWarRand": ("war", "limitedWarRand"), "iLimitedWarPowerRatio": ("war", "limitedWarPowerRatio"),
    "iDogpileWarRand": ("war", "dogpileWarRand"), "iMakePeaceRand": ("war", "makePeaceRand"),
    "iDeclareWarTradeRand": ("war", "declareWarTradeRand"), "iDemandRebukedSneakProb": ("war", "demandRebukedSneakProb"),
    "iDemandRebukedWarProb": ("war", "demandRebukedWarProb"), "iRefuseToTalkWarThreshold": ("war", "refuseToTalkWarThreshold"),
    "iBaseAttackOddsChange": ("war", "baseAttackOddsChange"), "iAttackOddsChangeRand": ("war", "attackOddsChangeRand"),
    "iRazeCityProb": ("war", "razeCityProb"),
    # victory weights
    "iCultureVictoryWeight": ("victory", "culture"), "iSpaceVictoryWeight": ("victory", "space"),
    "iConquestVictoryWeight": ("victory", "conquest"), "iDominationVictoryWeight": ("victory", "domination"),
    "iDiplomacyVictoryWeight": ("victory", "diplomacy"),
    # trade
    "iMaxGoldTradePercent": ("trade", "maxGoldPercent"), "iMaxGoldPerTurnTradePercent": ("trade", "maxGoldPerTurnPercent"),
    "iNoTechTradeThreshold": ("trade", "noTechTradeThreshold"), "iTechTradeKnownPercent": ("trade", "techTradeKnownPercent"),
}

# --- attitude relation families: tag -> (relation, member). ai.attitude.<relation>.<member>. ---
ATTITUDE = {
    "iCloseBordersAttitudeChange": ("closeBorders", "change"),
    "iLostWarAttitudeChange": ("lostWar", "change"),
    "iAtWarAttitudeDivisor": ("atWar", "divisor"), "iAtWarAttitudeChangeLimit": ("atWar", "changeLimit"),
    "iAtPeaceAttitudeDivisor": ("atPeace", "divisor"), "iAtPeaceAttitudeChangeLimit": ("atPeace", "changeLimit"),
    "iSameReligionAttitudeChange": ("sameReligion", "change"), "iSameReligionAttitudeDivisor": ("sameReligion", "divisor"),
    "iSameReligionAttitudeChangeLimit": ("sameReligion", "changeLimit"),
    "iDifferentReligionAttitudeChange": ("differentReligion", "change"), "iDifferentReligionAttitudeDivisor": ("differentReligion", "divisor"),
    "iDifferentReligionAttitudeChangeLimit": ("differentReligion", "changeLimit"),
    "iBonusTradeAttitudeDivisor": ("bonusTrade", "divisor"), "iBonusTradeAttitudeChangeLimit": ("bonusTrade", "changeLimit"),
    "iOpenBordersAttitudeDivisor": ("openBorders", "divisor"), "iOpenBordersAttitudeChangeLimit": ("openBorders", "changeLimit"),
    "iDefensivePactAttitudeDivisor": ("defensivePact", "divisor"), "iDefensivePactAttitudeChangeLimit": ("defensivePact", "changeLimit"),
    "iShareWarAttitudeChange": ("shareWar", "change"), "iShareWarAttitudeDivisor": ("shareWar", "divisor"),
    "iShareWarAttitudeChangeLimit": ("shareWar", "changeLimit"),
    "iFavoriteCivicAttitudeChange": ("favoriteCivic", "change"), "iFavoriteCivicAttitudeDivisor": ("favoriteCivic", "divisor"),
    "iFavoriteCivicAttitudeChangeLimit": ("favoriteCivic", "changeLimit"),
    "iBetterRankDifferenceAttitudeChange": ("betterRankDifference", "change"),
    "iWorseRankDifferenceAttitudeChange": ("worseRankDifference", "change"),
}

# --- attitude-threshold strings (ATTITUDE_*): tag -> key under ai.refuse. ---
REFUSE = {
    "DemandTributeAttitudeThreshold": "demandTribute", "NoGiveHelpAttitudeThreshold": "noGiveHelp",
    "TechRefuseAttitudeThreshold": "tech", "StrategicBonusRefuseAttitudeThreshold": "strategicBonus",
    "HappinessBonusRefuseAttitudeThreshold": "happinessBonus", "HealthBonusRefuseAttitudeThreshold": "healthBonus",
    "MapRefuseAttitudeThreshold": "map", "DeclareWarRefuseAttitudeThreshold": "declareWar",
    "DeclareWarThemRefuseAttitudeThreshold": "declareWarThem", "StopTradingRefuseAttitudeThreshold": "stopTrading",
    "StopTradingThemRefuseAttitudeThreshold": "stopTradingThem", "AdoptCivicRefuseAttitudeThreshold": "adoptCivic",
    "ConvertReligionRefuseAttitudeThreshold": "convertReligion", "OpenBordersRefuseAttitudeThreshold": "openBorders",
    "DefensivePactRefuseAttitudeThreshold": "defensivePact", "PermanentAllianceRefuseAttitudeThreshold": "permanentAlliance",
    "VassalRefuseAttitudeThreshold": "vassal",
}

# --- keyed lists: tag -> (subgroup, key). value = the entry's numeric (the *Type child is the key). ---
KEYED = {
    "MemoryDecays": ("memory", "decay"), "MemoryAttitudePercents": ("memory", "attitudePercent"),
    "ContactRands": ("contact", "rand"), "ContactDelays": ("contact", "delay"),
    "NoWarAttitudeProbs": ("noWarProb", None), "UnitAIWeightModifiers": ("unitWeights", None),
    "ImprovementWeightModifiers": ("improvementWeights", None),
}
FAVORITES = {"FavoriteCivic": "civic", "FavoriteReligion": "religion"}
MUSIC = {"DiplomacyIntroMusicPeace": "diploIntroMusicPeace", "DiplomacyMusicPeace": "diploMusicPeace",
         "DiplomacyIntroMusicWar": "diploIntroMusicWar", "DiplomacyMusicWar": "diploMusicWar"}
TEXT = {"Description": "description", "Civilopedia": "civilopedia"}
# ALL leader trait assignments STRIPPED for now (owner 2026-07-01) -> DROP; the leader<->trait mapping is a
# post-migration pass (re-add a `grants.traits` emit + the TRAITS map then). No leader carries traits meanwhile.
DROP = {"Type", "Traits", "DefaultTraits", "DefaultComplexTraits"}

AI_ORDER = ["npc", "flavours", "personality", "war", "victory", "trade", "attitude", "refuse",
            "memory", "contact", "noWarProb", "unitWeights", "improvementWeights", "favorites"]


def _keyed(node):
    """<Wrapper><Entry><FooType>K</FooType><iVal>N</iVal></Entry>...> -> {K: int}. Key child ends with 'Type'."""
    out = OrderedDict()
    for entry in list(node):
        k, val = None, None
        for c in entry:
            if k is None and c.tag.endswith("Type"):
                k = engine.text(c)
            else:
                t = engine.text(c)
                if engine.is_int(t):
                    val = int(t)
        if k and k != "NONE" and val is not None:
            out[k] = val
    return out


def _music(node):
    """Full music -> {era: DiploScriptId} map; intro music (entries carry EraType only, no script) -> [era,...] list."""
    pairs = []
    for entry in list(node):
        era, script = None, None
        for c in entry:
            if c.tag == "EraType":
                era = engine.text(c)
            elif c.tag == "DiploScriptId":
                script = engine.text(c)
        if era and era != "NONE":
            pairs.append((era, script))
    if any(s for _e, s in pairs):
        return OrderedDict(pairs)
    return [e for e, _s in pairs]


def _list(node):
    out = []
    for c in node:
        t = engine.text(c)
        if not t:
            for cc_ in c:
                if engine.text(cc_):
                    t = engine.text(cc_)
                    break
        if t and t != "NONE":
            out.append(t)
    return out


def curate(typ, rec):
    text, ai, identity, grants, art_blocks, sound = {}, {}, {}, {}, {}, {}
    leftover = []

    def put_ai(sub, key, val):
        node = ai.setdefault(sub, OrderedDict())
        if key is None:
            node.update(val)
        else:
            node[key] = val

    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in SCALAR:
            if engine.is_int(t) and int(t) != 0:
                sub, key = SCALAR[tag]
                put_ai(sub, key, int(t))
        elif tag in ATTITUDE:
            if engine.is_int(t) and int(t) != 0:
                rel, member = ATTITUDE[tag]
                ai.setdefault("attitude", OrderedDict()).setdefault(rel, OrderedDict())[member] = int(t)
        elif tag in REFUSE:
            if t and t != "NONE":
                ai.setdefault("refuse", OrderedDict())[REFUSE[tag]] = t
        elif tag in KEYED:
            sub, key = KEYED[tag]
            vals = _keyed(c)
            if vals:
                if key is None:
                    put_ai(sub, None, vals)
                else:
                    ai.setdefault(sub, OrderedDict())[key] = vals
        elif tag in FAVORITES:
            if t and t != "NONE":
                ai.setdefault("favorites", OrderedDict())[FAVORITES[tag]] = t
        elif tag == "Flavors":
            v = engine.generic(c)
            if v:
                ai["flavours"] = v
        elif tag in MUSIC:
            m = _music(c)
            if m:
                sound[MUSIC[tag]] = m
        elif tag == "ArtDefineTag":
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> world.art.icon
        elif tag == "bNPC":
            if t in ("1", "true", "True"):
                ai["npc"] = True                          # AI-only leader (barbarian/NPC) — an AI classification
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, cc.de_i(tag))] = engine.generic(c)

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia"):
        if k in text:
            out[k] = text[k]
    if grants:
        out["grants"] = grants
    if ai:
        ordered = OrderedDict()
        for sub in AI_ORDER:
            if sub in ai:
                ordered[sub] = ai[sub]
        for sub in ai:
            if sub not in ordered:
                ordered[sub] = ai[sub]
        out["ai"] = ordered
    cc.emit_art(out, art_blocks)
    if sound:
        out["sound"] = sound
    if identity:
        out["identity"] = identity
    cc.fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("LeaderHeadInfo")
    results, all_leftover = OrderedDict(), set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec)
        results[typ] = obj
        all_leftover.update(leftover)
    n = len(results)
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("LeaderHeadInfo curated: %d" % n)
    for k in ("grants", "ai", "world", "sound", "identity"):
        print("  with %-9s: %d" % (k, has(k)))
    if all_leftover:
        print("  !! leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "leaderheads")
        os.makedirs(out_dir, exist_ok=True)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d LeaderHeadInfo JSON files under Assets/Data/leaderheads" % n)


if __name__ == "__main__":
    main()
