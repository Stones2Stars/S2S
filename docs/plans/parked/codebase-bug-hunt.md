# Codebase bug-hunt — standing initiative

A recurring, systematic sweep of the codebase (C++ in `Sources/`, Python in
`Assets/Python/`) for the class of "obvious" latent bugs that this large legacy
Civ4/C2C-derived codebase accumulates: copy-paste slips, operator-precedence
mistakes, shadowed members, inverted conditions, uninitialised variables, and
out-of-bounds access. The goal is to verify each finding against source and file
one GitHub issue per confirmed bug.

## Why this exists

The first pass (CvUnit / CvUnitAI / CvCity / CvCityAI) found ~23 real bugs.
The patterns repeat across the whole engine, so periodic sweeps keep paying off.
Keeping the method and conventions written down means anyone (or any agent) can
pick the work up where it was left.

## Method (proven)

1. **Fan out** parallel reviewers, each over a ~7–8k-line chunk of a single
   file. Ask only for high-confidence findings, with an explicit checklist:
   - `&&` vs `||`, operator precedence (missing parens around `||` inside `&&`)
   - inverted conditions, `=` vs `==`
   - off-by-one / out-of-bounds array or `std::vector` access
   - null derefs after `get*()` / `plot()` / `getPlotCity()` that can return NULL
   - copy-paste: wrong variable / enum / "checks X but uses Y"
   - member-variable shadowing (`int m_xxx = ...` declares a local, member never written)
   - a function passed without `()` (decays to an always-true pointer)
   - comma-operator slip: an argument placed outside the call parens — `f(a), b)`
   - wrong best-value comparator (`<` vs `>`), or a missing `iValue > iBestValue` guard
   - an accumulator not reset between outer-loop iterations, or mutated in the wrong place
   - uninitialised variables; missing `break` / switch fallthrough
2. **Re-read every flagged line against source before filing.** Reviewers
   produce false positives — e.g. an idiom that looks wrong but is intentional
   (the `AI_chooseUnit` cross-counter return value was one such non-bug). Do not
   file anything you have not personally confirmed.
3. **File one GitHub issue per confirmed bug** (repo `Stones2Stars/S2S`).

## GitHub issue convention

- **Title:** `` `<file>: <function>() — <symptom>` ``
- **Body**, in this order: `## Summary`, `## Location`
  (`Sources/file:line — Class::func`), `## Problem` (with a code block),
  `## Suggested fix` (code), `## Impact`.
- **Labels:** always `bug`; add `game mechanics` for core gameplay
  (CvUnit/CvCity/…), `unit automation` for CvUnitAI, `code structure` for
  dead-code/structural issues, `infrastructure` for tooling/logging.
- After landing a fix, reference the issue from the commit/PR (`Fixes #NN`).

## Progress

The C++ engine sweep is essentially complete (CvUnit/AI, CvCity/AI, CvPlot,
CvSelectionGroup/AI, CvTeam/AI, CvGame/AI, CvDeal, CvMap, CvPlayer/AI,
CvGameTextMgr). High-value Python is swept too (CvEventManager,
CvRandomEventInterface, Revolution, CvGameUtils, CvMapGeneratorUtil, CvDiplomacy,
BarbarianCiv, the advisor screens). 80+ confirmed bugs were filed against
`Stones2Stars/S2S` as `agent-found`, then worked down by tier:

- **Pass 1 (#48–#70):** CvUnit/AI, CvCity/AI — PR #71 landed the first three.
- **tier-2 (high-impact):** #97, #99, #105\*, #107, #109, #114, #116, #117 — PRs
  #167–169.
- **tier-3 (AI quality):** #77, #79, #81, #86, #87, #89, #113, #120, #121, #124,
  #128, #129, #132 — PRs #170–174.
- **low-priority (latent / assert-only / crash-guards):** #59, #60, #64, #78,
  #88, #92, #95, #102, #103, #108, #133 — PRs #178–181.
- **Python-sweep batch (#158–#162):** PRs #163–166.
- Plus standalone gameplay fixes: #140 (PR #154), #156 (PR #157), #153 (PR #176).

Remaining open issues need a human/design call rather than an auto-fix:
`needs-signoff` (#66; #105\* — game-start flag needs `>` semantics + a decision)
and `ambiguous` (#68, #139).

## Durable lessons

- **Re-verify every reviewer flag against current source.** One Python batch
  dropped 6 of ~14 flags as false positives — precedence that actually groups
  correctly, Py2-correct `int < str` idioms, an `if/elif` equivalent to `if`,
  capital-invariant guards, Py2 `cmp` not raising. Reviewers (and triage agents)
  misread lines; resolve conflicts by reading source.
- **Some "bugs" are dead code** — the right fix can be *removal* when a
  loop/branch provably never runs (#64's inverted-`while` fast-path), which is
  behavior-preserving and safer than reviving it.
- **A fix can change behaviour more than the one-line diff suggests:** #87's
  pillage tiering would infinite-loop on a naive `min`→`max`; #113's corp-trigger
  returned false on *every* success path, not just the filed one.
- Always Assert-build before the PR; multi-line tab-sensitive edits often need a
  re-read or a line-targeted `sed`.

## Next targets / open fronts

- Finish the Python sweep: `Screens/*` (Worldbuilder, Pedia, CvMainInterface),
  autolog, Platyping/Sparth, RevUtils/RevEvents remainder.
- Sea / naval AI rework — see [`sea-ai-rework.md`](sea-ai-rework.md).
- Dead-code / dead-XML pass.
- Python-specific smells to watch: bare `except:`, `==` vs `is`, mutable default
  arguments, wrong indentation under `if`/`for`, missing `self.`, wrong argument
  count, off-by-one `range()`, integer-vs-float division. (Runtime is **Python
  2.4** — do not report 2.4-correct idioms; see the false-positive lesson above.)

## See also

- The tagged AI logs are the complementary, data-driven way to surface AI bugs.
- Build / compile-check: `Tools/_Build.ps1 Assert build` from `Sources/`.
