#!/usr/bin/env python3
"""
curate_classification_ids.py -- emit the CLASSIFICATION ID TABLE (`Sources/Infos/CvClassificationIds.h`).

WHY: the json.md par.8/par.9 classification categories are OPEN BY DESIGN -- their member set grows with
authored data and their ids are minted at LOAD from the union of authored keys
([DEC-classification-infos]). That openness is why no compile-time id existed to pass, and why the consumer
side had to memoize an id per key -- which produced a GETTER PER KEY (CvSkillReads and its siblings, ~60
static methods each doing the identical thing). That is the very getter-per-channel shape the rebuild is
deleting, merely relocated ([patterns.md] par. THE GETTER SETUP: the per-key reads are TRANSITIONAL and
collapse onto one parameterized read).

The blocker was never the openness -- it was that the id ORDER was DISCOVERED at load. It does not have to
be. The curator already enumerates every authored key, so it pins the order here exactly as
`curate_order.py` pins category id order in `_order.json`: the registry SEEDS from this table instead of
minting in discovery order, so `SKILL_BLITZ` is a compile-time constant that equals the runtime id, and the
whole surface becomes `info.hasSkill(SKILL_BLITZ)`.

The category stays OPEN: a key authored but absent from this table is still minted at load, appended after
the seeded block (and reported by the reader's fail-loud key coverage). Regenerating simply promotes it to a
compile-time id. Nothing serializes -- ids are derived and re-minted per load
([DEC-classification-infos]) -- so a regenerated table shifts no save state and needs no migration.

Derived artifact -- regenerate + commit freely (curator OUTPUT, never hand-edited):
    python3 curate_classification_ids.py --write
"""
import argparse
import glob
import json
import os

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA = os.path.join(REPO, "Assets", "Data")
OUT = os.path.join(REPO, "Sources", "Infos", "CvClassificationIds.h")

# The block key in the JSON -> (ClsDomain enum name, the naming.md id PREFIX, the C++ enum type name).
# Mirrors CLS_PREFIX / ClsDomain in CvClassificationBlock.h -- one row per generated-info category.
# ⛔ The C++ constants carry a reserved `CLS_` namespace -- NOT the bare naming.md infotype prefix. The runtime
# INFOTYPE STRING is still "TAG_MILITARY" (the registry mints it from CLS_PREFIX); only the C++ enumerator is
# namespaced. The reason is collision and it is not hypothetical: the property engine already owns global
# `TagTypes` (TAG_SPY, TAG_WATER, ...) and `AttributeTypes` (ATTRIBUTE_POPULATION, ...) in CvEnums.h, and that
# engine is owner-LOCKED. More decisively, these registries are OPEN BY DESIGN -- any future authored key could
# collide with any engine enumerator -- so a reserved namespace makes the whole class of collision impossible
# rather than fixing them one at a time.
DOMAINS = [
    ("skills",          "CLSD_SKILL",          "CLS_SKILL_",          "SkillClsTypes"),
    ("tags",            "CLSD_TAG",            "CLS_TAG_",            "TagClsTypes"),
    ("attributes",      "CLSD_ATTRIBUTE",      "CLS_ATTRIBUTE_",      "AttributeClsTypes"),
    ("amenities",       "CLSD_AMENITY",        "CLS_AMENITY_",        "AmenityClsTypes"),
    ("characteristics", "CLSD_CHARACTERISTIC", "CLS_CHARACTERISTIC_", "CharacteristicClsTypes"),
    ("capabilities",    "CLSD_CAPABILITY",     "CLS_CAPABILITY_",     "CapabilityClsTypes"),
    ("policies",        "CLSD_POLICY",         "CLS_POLICY_",         "PolicyClsTypes"),
    ("canTrade",        "CLSD_CANTRADE",       "CLS_CANTRADE_",       "CanTradeClsTypes"),
    ("canWorkOn",       "CLSD_CANWORKON",      "CLS_CANWORKON_",      "CanWorkOnClsTypes"),
]

# ⚖ HEADROOM: keys the SPEC glossaries name as real members but which NO shipped entity authors today
# ([skills.md] §1/§2). They mint ids so a consumer can still ASK -- the read simply answers false until data
# authors one, which is the live-but-inert pattern the model already carries elsewhere (the wellbeing
# off-switches, a corporation's obsoleting tech). Without them a legitimate consumer would not compile.
# ⛔ This is NOT a place to park a key you merely expect to want: a member earns a row here by being in a
# glossary, and it leaves the moment data authors it (the data scan below supersedes it automatically).
HEADROOM = {
    "skills": ["freeDrop", "noSelfHeal", "offensiveVictoryMove", "pillageEspionage", "pillageResearch"],
    # The wellbeing OFF-SWITCH family ([json.md] §8): the unqualified `abolishedAnger` IS authored, but the two
    # narrowed forms are authored by NOTHING -- deliberately, because the mechanic is "wildly overpowered"
    # (owner, [modifier.md] §2b). The chain stays wired and every read answers false: live-but-inert headroom,
    # never a data gap to fill.
    "amenities": ["abolishedUnhealthFromPopulation", "abolishedUnhealthFromBuildings"],
    # `freeSpeech` is mapped by curate_civic's POLICIES table and authored by NOTHING -- all 146 civics carry
    # `bFreeSpeech>0<`, in base and modules alike -- so the curator correctly emits no key and the legacy read
    # answered false for every civic. The revolutions view still ASKS (RevUtils canDoFreeSpeech / isFreeSpeech),
    # so the id mints here and those reads answer false, exactly as they always did. It leaves this list the
    # moment a civic authors the flag.
    # `noLandmarkAnger` is the same shape: mapped by the POLICIES table, authored by no civic, and the landmark
    # anger gate (CvPlayer::isNoLandmarkAnger) still asks.
    "policies": ["freeSpeech", "noLandmarkAnger"],
}


def upper_snake(key):
    """The EXACT mirror of clsUpperSnake in CvClassificationRegistry.cpp.

    '_' before an upper that starts a word (prev lower/digit, or an acronym run followed by a lower) and
    before a digit run following a letter: setScienceRate -> SET_SCIENCE_RATE, maxHP -> MAX_HP,
    dcmAirBomb -> DCM_AIR_BOMB, is_cargo_vessel -> IS_CARGO_VESSEL.
    """
    out = []
    for i, c in enumerate(key):
        is_upper = "A" <= c <= "Z"
        is_digit = "0" <= c <= "9"
        if i > 0:
            prev = key[i - 1]
            prev_lower = "a" <= prev <= "z"
            prev_upper = "A" <= prev <= "Z"
            prev_digit = "0" <= prev <= "9"
            next_lower = i + 1 < len(key) and "a" <= key[i + 1] <= "z"
            if (is_upper and (prev_lower or prev_digit or (prev_upper and next_lower))) \
               or (is_digit and (prev_lower or prev_upper)):
                out.append("_")
        out.append(c.upper())
    return "".join(out)


def collect():
    """The union of authored keys per domain, over every entity file (BOTH planes -- a revoke-only key is
    still a real ability id, matching the registry's own mint loop)."""
    found = dict((d[0], set()) for d in DOMAINS)
    for path in glob.glob(os.path.join(DATA, "**", "*.json"), recursive=True):
        if os.path.basename(path) == "_order.json":
            continue
        try:
            with open(path, encoding="utf-8") as handle:
                entity = json.load(handle)
        except (ValueError, OSError):
            continue
        if not isinstance(entity, dict):
            continue
        for block_key in found:
            block = entity.get(block_key)
            if isinstance(block, dict):
                found[block_key].update(block.keys())          # {name: true|false|entry}
            elif isinstance(block, list):
                found[block_key].update(k for k in block if isinstance(k, str))   # ["blitz", ...]
    for block_key, keys in HEADROOM.items():
        found[block_key].update(keys)
    return found


def render(found):
    lines = []
    lines.append("#pragma once")
    lines.append("#ifndef CV_CLASSIFICATION_IDS_H")
    lines.append("#define CV_CLASSIFICATION_IDS_H")
    lines.append("")
    lines.append("//")
    lines.append("//\tCvClassificationIds -- the COMPILE-TIME id table for the json.md par.8/par.9 classification")
    lines.append("//\tcategories. GENERATED by Tools/Migration/curate_classification_ids.py -- do NOT hand-edit;")
    lines.append("//\tregenerate and commit, exactly like an `_order.json` manifest.")
    lines.append("//")
    lines.append("//\tThe ClassificationRegistry SEEDS from this table before minting, so each constant below IS")
    lines.append("//\tthe runtime id and a consumer reads `info.hasSkill(SKILL_BLITZ)` -- ONE parameterized read")
    lines.append("//\tper domain, never a getter per key ([patterns.md] par. THE GETTER SETUP).")
    lines.append("//")
    lines.append("//\tThe categories stay OPEN ([DEC-classification-infos]): a key authored but absent here is still")
    lines.append("//\tminted at load, appended after the seeded block. Regenerating promotes it to a constant.")
    lines.append("//\tNothing serializes -- ids are re-derived per load -- so a regenerated table is save-neutral.")
    lines.append("//")
    lines.append("//\t⛔ The constants carry a reserved CLS_ namespace; the runtime INFOTYPE STRING is still")
    lines.append("//\t\"TAG_MILITARY\". The property engine already owns global TagTypes/AttributeTypes in CvEnums.h,")
    lines.append("//\tand these registries are OPEN, so the namespace makes collision impossible by construction.")
    lines.append("//")
    lines.append("")
    lines.append("#include \"CvClassificationBlock.h\"   // ClsDomain / NUM_CLS_DOMAINS (the seed table's index)")
    lines.append("")
    for block_key, domain_enum, prefix, type_name in DOMAINS:
        keys = sorted(found[block_key])
        lines.append("// %s -- the `%s` block (%d authored keys)" % (domain_enum, block_key, len(keys)))
        lines.append("enum %s" % type_name)
        lines.append("{")
        for index, key in enumerate(keys):
            lines.append("\t%s%s = %d," % (prefix, upper_snake(key), index))
        lines.append("\tNUM_%sTYPES = %d" % (prefix, len(keys)))
        lines.append("};")
        lines.append("")
    # The seed table the registry reads: domain -> the authored keys IN ID ORDER, NULL-terminated.
    lines.append("// The seed table -- ClassificationRegistry::buildAndResolve mints these first, in order, so the")
    lines.append("// constants above equal the runtime ids. One NULL-terminated list per domain, indexed by ClsDomain.")
    for block_key, domain_enum, prefix, type_name in DOMAINS:
        keys = sorted(found[block_key])
        lines.append("static const char* const CLS_SEED_%s[] =" % domain_enum)
        lines.append("{")
        for key in keys:
            lines.append("\t\"%s\"," % key)
        lines.append("\tNULL")
        lines.append("};")
        lines.append("")
    lines.append("static const char* const* const CLS_SEED_KEYS[NUM_CLS_DOMAINS] =")
    lines.append("{")
    for block_key, domain_enum, prefix, type_name in DOMAINS:
        lines.append("\tCLS_SEED_%s," % domain_enum)
    lines.append("};")
    lines.append("")
    lines.append("#endif // CV_CLASSIFICATION_IDS_H")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="write the header (default: report only)")
    args = parser.parse_args()

    found = collect()
    for block_key, domain_enum, _prefix, _type in DOMAINS:
        print("  %-18s %3d keys" % (block_key, len(found[block_key])))

    text = render(found)
    if args.write:
        with open(OUT, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        print("wrote %s" % OUT)
    else:
        print("(dry run -- pass --write to emit %s)" % OUT)


if __name__ == "__main__":
    main()
