#!/usr/bin/env python3
"""curate_all -- run every per-entity curator (--write), then the manifests + the post-curation additions overlay.

The single "full regen" entry point. Order is NOT load-bearing among the entity curators (each reads the XML store
fresh and writes its own folder); `curate_order` + `curate_additions` run LAST because the additions overlay must
land after every re-curate (docs/specs/curators/README.md).

⛔ LOCKED entities are EXCLUDED (owner ruling 2026-07-21): `trait`, `leaderhead`, and `tech` are content-LOCKED and
hand-maintained from here on (the community owns trait assignments post-launch), so a full regen must NEVER clobber
them. Edit LOCKED to change what is frozen. Shared modules + superseded emitters are never run as curators.
`tech` is locked so archiving the trait XML cannot regress the store-derived tech->trait `enables` on a re-curate;
future tech-edge changes ride the `_additions` overlay, not a curate_tech re-run.

  python curate_all.py            # full regen (all unlocked entities + order + additions)
  python curate_all.py --list     # print what WOULD run, run nothing
"""
import os, sys, glob, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
PY = sys.executable

LOCKED = {"trait", "leaderhead", "tech"}               # hand-maintained; never regenerated
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
