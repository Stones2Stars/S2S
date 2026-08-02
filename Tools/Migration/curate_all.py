#!/usr/bin/env python3
"""curate_all -- run every per-entity curator (--write), then the manifests + the post-curation additions overlay.

The single "full regen" entry point. Order is NOT load-bearing among the entity curators (each reads the XML store
fresh and writes its own folder); `curate_order` + `curate_additions` run LAST because the additions overlay must
land after every re-curate (docs/specs/curators/README.md).

⛔ LOCKED entities are EXCLUDED from the full regen: their folders are hand-maintained, so a regen must never
clobber them. Edit LOCKED to change what is frozen. Shared modules + superseded emitters are never run as curators.

⚑ `trait` is NOT locked (owner): the lock is what let the two trait sets drift — a hand edit could put an edge in
one set pointing at an entity only the other set has, with nothing regenerable to correct it. `curate_trait` reads
the ARCHIVED trait XML (`SourceArchive/Assets/**`, searched by store.py alongside the live roots) and rewrites both
folders; community-owned trait CONTENT rides the `_additions` overlay like every other post-curation authoring.
⚑ `tech` is NOT locked either, and the reason is worth keeping: it was locked so archiving the trait XML could not
regress the store-derived tech->trait `enables` on a re-curate. But the lock landed AFTER the archiving, so it did
not preserve those edges — it FROZE THEIR ABSENCE. All 152 (the `PrereqTech` on every rank +-2/+-3 trait rung, i.e.
the tech gate on advancing a developing line) were missing from the tech JSON until the store could read the
archive again. A lock over a value with no live source hides the hole instead of protecting it.

  python curate_all.py            # full regen (all unlocked entities + order + additions)
  python curate_all.py --list     # print what WOULD run, run nothing
"""
import os, sys, glob, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
PY = sys.executable

LOCKED = {"leaderhead"}                                # hand-maintained; never regenerated
NOT_A_CURATOR = {"all", "common", "pocos"}             # this runner, the shared core, the legacy batch
TAIL = ["curate_order.py", "curate_additions.py"]      # run LAST, in this order


def _entities():
    names = sorted(os.path.basename(p)[len("curate_"):-3] for p in glob.glob(os.path.join(HERE, "curate_*.py")))
    tail_ent = {t[len("curate_"):-3] for t in TAIL}
    return [e for e in names if e not in (LOCKED | NOT_A_CURATOR | tail_ent)]


def _run(script):
    print("\n=== %s --write ===" % script)
    return subprocess.run([PY, os.path.join(HERE, script), "--write"], cwd=HERE).returncode


def main():
    run_list = _entities()
    if "--list" in sys.argv[1:]:
        print("would run (%d): %s" % (len(run_list), ", ".join(run_list)))
        print("then tail: %s" % ", ".join(TAIL))
        print("LOCKED (skipped): %s" % ", ".join(sorted(LOCKED)))
        return
    failed = []
    for e in run_list:
        if _run("curate_%s.py" % e) != 0:
            failed.append(e)
    for t in TAIL:
        if os.path.exists(os.path.join(HERE, t)) and _run(t) != 0:
            failed.append(t)
    print("\n=== curate_all done. LOCKED (skipped): %s ===" % ", ".join(sorted(LOCKED)))
    if failed:
        print("FAILED: %s" % ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
