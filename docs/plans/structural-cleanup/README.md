# Structural cleanup — the bulldozer reference (transient)

> **Not everyday docs (owner ruling 2026-06-23).** This is what you reach for **when the bulldozer arrives** — the
> atomic cutover that deletes the legacy machinery the cascade replaces. It is the *destroy map*: which legacy
> calcs, enabler machinery, and engine functions get deleted, and what they were verified against first. Kept until
> the cutover; **dropped when the legacy is gone.**
>
> Per [DEC-parity](../../architecture/decisions.md#dec-parity) +
> [DEC-no-parity-results-in-docs](../../architecture/decisions.md#dec-no-parity-results-in-docs): the live shadow +
> the curators do the *verification*; these docs are the *deletion plan*, never a result set.

## Contents
- **cutover.md** — the cutover roadmap: the gates (StoneBase-completeness / shadow-parity / classification consumption),
  the prerequisites, and the sequencing from shadow to `main`.
- **code-cut-map.md** — ✅ the master **CODE-CUT MAP** (built 2026-07-02, two-pass adversarial): every legacy
  mechanism/consumer → cascade replacement → cut action, grounded in `file:line`. The line-item plan the cutover works
  down + the Gate-1 completeness proof (Gate-1 gaps, Gate-3 worklist, BLOCKED tail, divergences with proof).
- **structural-cleanup.md** — the source-level deletion plan (which `Cv*` files / functions go).
- **legacy-value-calc-map.md** — the per-calc destroy map: every legacy per-turn value calc traced to its getter +
  components (what the cascade reproduces, then deletes).
- **constructibility.md** — the legacy `canConstruct` / `canTrain` + reverse-index machinery the enabler replaces.
- **cascade-engine-430.md** — the engine-side build / demolition map (function-level deletion targets) + the
  build-wholesale ruling. (Status table rebuilt 2026-06-29 to post-purge truth.)
- **readjson.md** — the `readJson` BoolExpr-routed reader build plan (the first #430 build item; the data-feed
  prerequisite for the modifier + enabler).

*(Lifted intact — transient bulldozer docs, not condensed. Their internal links still point at pre-move paths;
part of the reference-sweep follow-up. The build-wholesale ruling inside `cascade-engine-430.md` should also get a
ledger line — a follow-up.)*
