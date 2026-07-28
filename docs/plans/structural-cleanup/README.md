# Structural cleanup — the bulldozer reference (transient)

> **Not everyday docs (owner ruling).** This is what you reach for **when the bulldozer arrives** — the work that
> deletes the legacy machinery the cascade replaces. It is the *destroy map*, never a result set
> ([DEC-no-parity-results-in-docs](../../architecture/decisions.md#dec-no-parity-results-in-docs)). Kept until the
> legacy is gone; **dropped then.**
>
> Only [roadmap.md](roadmap.md) is session-start reading. Everything else is reached for deliberately, for the one
> job named below.

## The plan

- **[roadmap.md](roadmap.md)** — 🔝 **the master plan.** The design the code conforms to, what exists on the
  branch, and the open access-surface item. Mandated session-start reading. How legacy is removed is delete-driven
  — the compiler is the census, correctness is verified live per mechanism
  ([DEC-playability-not-a-gate](../../architecture/decisions.md#dec-playability-not-a-gate)); there is no gated
  phase to treat as holy writ.

## The legacy inventories (what must be replaced, grounded in `file:line`)

The engine game-object classes are back on `main`, so these describe live code again.

- **[legacy-value-calc-map.md](../../reference/legacy-value-calc-map.md)** — every legacy per-turn value calc traced to its getter
  and its components. Confirm the named FUNCTION, never the line number.

> ⛔ **A per-channel reader census is NOT how the consumer sweep is driven, and one is not to be re-generated.**
> The previous one classified every reader as cascade / legacy / mixed against a substrate that has since been
> archived, which left a confident-looking map whose "cascade" and "mixed" rows all silently meant legacy. Worse,
> working from it makes the sweep a per-channel HOLE-PLUGGING exercise, and a hole plugged today is plugged with
> the only thing standing there — legacy (the roadmap banner's structure-first rule). Drive the sweep from the
> target surface and let the COMPILER census the consumers
> ([DEC-playability-not-a-gate](../../architecture/decisions.md#dec-playability-not-a-gate)).
- **[constructibility.md](../../reference/legacy-constructibility.md)** — the legacy `canConstruct`/`canTrain` + reverse-index machinery
  the enabler replaces.
- **[structural-cleanup.md](structural-cleanup.md)** — the `Sources/` tree reorg (landed) + the dead-code /
  dead-XML pass (candidate-generation only).

## The data work (unaffected by the runtime rebuild)

- **[todo.md](todo.md)** — the curator/JSON worklist; the #1-priority tier
  ([DEC-data-first](../../architecture/decisions.md#dec-data-first)).
- **[property-audit.md](property-audit.md)** — LOCKED, owner-approved. The property SOURCE-data migration; the
  property ENGINE math is KEEP-legacy and must NOT be rewritten.
- **[stub-census.md](stub-census.md)** — poco getters returning a constant where legacy computed a real value,
  with named live consumers. Keep CURRENT: delete rows as they are fixed.
- **[unitcombat-distillation.md](unitcombat-distillation.md)** — slim the `UnitCombat` god-group into tags /
  skills / modifier-source. **Required before #430 completes** — the common blocker under three fronts.
  Worklists: **[unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)** ·
  **[unitcombat-merge-candidates.md](unitcombat-merge-candidates.md)** (map the obvious, FLAG the unsure — never
  blunt-purge).
- **[fixed-point-conformance.md](fixed-point-conformance.md)** — the ×100 conversion worklist
  ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)).

## The grants machine (resolver built, apply-loop NOT built)

- **[grants-machine.md](../../specs/triggers.md)** — the machine's spec.
- **[grant-apply-sites.md](../../reference/legacy-grant-apply-sites.md)** — the `file:line` map of where provisions are handed over
  today. The apply surface accreted over fifteen years across two languages and is not reconstructible from
  memory; this is what the machine must replace.
- **[start-packages.md](../../specs/triggers.md)** — the game-start provisions as authored data (design, not built).

## The enabler + perf

- **[enabler-finished-set.md](enabler-finished-set.md)** — the AI decides from the enabler's LISTED frontier in
  ONE unified scoring pass, instead of probing the whole entity database per id.
- **[enabler-frontier-perf.md](enabler-frontier-perf.md)** — the frontier perf model: the GENERATE walk is a pure
  function of HAVE and is computed once per HAVE-change; the GATE walk is the dynamic part and stays.
- **[perf-profile-wiring.md](perf-profile-wiring.md)** — the census-based perf surface. ⛔ The internal
  `PROFILE_*` profiler is permanently dead and is NEVER reinstated.

---

⛔ **No doc in this tier carries a "superseded — ignore §X" banner, and none may be given one.** A banner asking
the reader to discount the text beneath it does not work: the body is longer, more specific and more actionable
than the banner, so the body wins and the marking becomes permission-shaped. Content that no longer holds is
**DELETED**, its durable part promoted into the owning spec, and — where the dead approach carries revival risk —
recorded in [superseded-ideas.md](../../architecture/superseded-ideas.md), the ONE tombstone registry
([DEC-docs-current-truth](../../architecture/decisions.md#dec-docs-current-truth),
[DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)).
