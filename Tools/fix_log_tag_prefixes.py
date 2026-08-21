#!/usr/bin/env python3
"""Phase 2 collision-proof: give each logging domain a UNIQUE field-tag prefix.

The spine logging tags use 2-letter field prefixes (CF_/DF_/EF_/WF_/...) that clash across domains
(CF_: CitField+ComField+CtbField; DF_: DipField+DaiField; EF_: EspField+EngField; WF_: WaiField+WarField).
In a FastBuild unity TU those become ambiguous/redefined when the files co-batch -- latent today, would
fire when the structural move reshuffles batches. Fix: rename each domain's field prefix to <DOMAIN>F_,
per-file, word-boundary, case-insensitive-prefix (matches both the lowercase def CF_owner and the
UPPERCASE mirror CF_OWNER). Event enums (COM_/UNT_/... 3-letter, already domain-unique) are untouched.
Run from Sources/ (or anywhere; paths are resolved to the repo's Sources/).
"""
import re, os, sys

SRCDIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Sources")

RENAMES = {
    "CvWorkerAI.cpp":          [("WF_", "WAIF_")],
    "CvHunterAI.cpp":          [("HF_", "HAIF_")],
    "CvCityLogTags.h":         [("CF_", "CITF_")],
    "CvCity.cpp":              [("CF_", "CITF_")],
    "CvCityAI.cpp":            [("CF_", "CITF_")],
    "CvUnitAI.cpp":            [("CF_", "COMF_"), ("UF_", "UNTF_"), ("FF_", "FNDF_")],
    "CvSelectionGroupAI.cpp":  [("CF_", "COMF_"), ("UF_", "UNTF_"), ("GF_", "GRPF_")],
    "CvArmy.cpp":              [("GF_", "GRPF_")],
    "CvPlayerAI.cpp":          [("DF_", "DIPF_"), ("EF_", "ESPF_")],
    "CvDeal.cpp":              [("DF_", "DIPF_")],
    "CvDecisionAI.cpp":        [("DF_", "DAIF_")],
    "CvTeamAI.cpp":            [("WF_", "WARF_")],
    "CvPlot.cpp":              [("EF_", "ENGF_")],
    "CvContractBroker.cpp":    [("CF_", "CTBF_")],
}

def main():
    total = 0
    for fname, subs in RENAMES.items():
        path = os.path.join(SRCDIR, fname)
        if not os.path.exists(path):
            print(f"  !! MISSING {fname}"); continue
        txt = open(path, encoding="utf-8", errors="replace").read()
        n = 0
        for old, new in subs:
            # \b<old> ; old ends with '_' so the boundary is at the start. Match exact uppercase prefix
            # (matches both CF_owner and CF_OWNER since both start with the literal 'CF_').
            txt, k = re.subn(r"\b" + re.escape(old), new, txt)
            n += k
            print(f"  {fname}: {old} -> {new}  ({k})")
        open(path, "w", encoding="utf-8", newline="").write(txt)
        total += n
    print(f"\nTOTAL replacements: {total}")

if __name__ == "__main__":
    main()
