#!/usr/bin/env python3
"""One-shot: update doc/script references to the renamed IDE project (C2C (VS2019) -> S2S).
Targets a fixed list (NOT regen_project.py, which intentionally names the old files as its source,
and NOT the 'The .vcxproj of Lies' Despair label, which is an entry name, not the filename)."""
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = [
    "Tools/_UpdateVSUserFile.ps1",
    "Tools/_Build.ps1",
    "AGENTS.md",
    "docs/dev/plans/ai-logging-rollout.md",
    "Sources/Mainpage.md",
    ".claude/skills/build-dll/SKILL.md",
]
for rel in FILES:
    p = os.path.join(ROOT, rel)
    t = open(p, encoding="utf-8", errors="replace").read()
    n = t.count("C2C (VS2019)")
    if n:
        open(p, "w", encoding="utf-8", newline="").write(t.replace("C2C (VS2019)", "S2S"))
    print(f"{rel}: {n} refs updated")
