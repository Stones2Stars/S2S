# Structural cleanup — the #430 work tier

> **Three docs carry this tier.** The DESIGN and the rulings are [roadmap.md](roadmap.md) (mandated session-start
> reading); what is NOT done is [todo.md](todo.md); the live defect register (verified bugs, each with evidence)
> is [issues.md](issues.md). Nothing else here is a plan
> ([DEC-spec-plus-todo](../../architecture/decisions.md#dec-spec-plus-todo)).
>
> ⛔ **No status lives in this tier.** No `LANDED`, no completion ledger, no "what exists" table: a finished item is
> DELETED from the todo, and anything durable it established moves into its owning SPEC first. What is built is
> answered by the tree — verify against the code, never against a doc.

- **[roadmap.md](roadmap.md)** — 🔝 the design the code conforms to, the governing rulings (compiling is not a gate ·
  wired outranks correct · design surface → contexts → THEN the AI calls), the demolition rule, and the open
  access-surface item.
- **[todo.md](todo.md)** — everything left, as short bullets.
- **[issues.md](issues.md)** — the live defect register: verified, evidence-bearing bugs (repro/root-cause/impact),
  each closed by DELETING its entry once fixed — never ticked.

## The decision worklists

Neither is a plan; each is a surface awaiting owner input, reached from the todo:

- **[unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)** — the unitcombat → identity-tag first pass. Map the
  OBVIOUS, FLAG the unsure.
- **[unitcombat-merge-candidates.md](unitcombat-merge-candidates.md)** — purge candidates for owner review. ⛔ Never
  blunt-purge: a previous one over-reached and was reverted, and the purge is GATED on tags taking over the
  identifier role ([engine.md](../../reference/engine.md)).
- **[stub-census.md](stub-census.md)** — poco getters returning a constant where legacy computed a real value, with
  named consumers. Rows are DELETED as they are fixed.

## Owner-LOCKED

- **[property-audit.md](property-audit.md)** — the property SOURCE-data migration. The property ENGINE math is
  KEEP-legacy and must NOT be rewritten.

---

**Where the legacy MAPS went:** they are censuses of how the legacy behaves, not work to do, so they live in
[`docs/reference/`](../../reference/) — a legacy map filed under `plans/` reads as planned work, which is exactly
the bait. See `legacy-value-calc-map`, `legacy-grant-apply-sites`, `pedia-read-map`,
`python-read-map`.
