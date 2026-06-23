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
- **structural-cleanup.md** — the source-level deletion plan (which `Cv*` files / functions go).
- **legacy-value-calc-map.md** — the per-calc destroy map: every legacy per-turn value calc traced to its getter +
  components (what the cascade reproduces, then deletes).
- **constructibility.md** — the legacy `canConstruct` / `canTrain` + reverse-index machinery the enabler replaces.
- **cascade-engine-430.md** — the engine-side build / demolition map (function-level deletion targets) + the
  build-wholesale ruling.

*(Lifted intact — transient bulldozer docs, not condensed. Their internal links still point at pre-move paths;
part of the reference-sweep follow-up. The build-wholesale ruling inside `cascade-engine-430.md` should also get a
ledger line — a follow-up.)*
