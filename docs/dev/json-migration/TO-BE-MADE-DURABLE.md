# To-be-made-durable (TBMD) — extract these BEFORE this folder is archived out

> **What this is.** A running, **append-as-you-go** checklist of the **durable** knowledge that has accumulated
> inside this one-time project folder and must be **lifted into the durable doc set** (`reference/`,
> `explanation/`, the [decisions ledger](../architecture/decisions.md)) before the folder is archived out of
> active scope. It is the mechanism behind
> [CONVENTIONS §9](../_meta/CONVENTIONS.md#9-one-time-project-folders--when-done-theyre-gone) /
> [DEC-ephemeral-project-folder](../architecture/decisions.md#dec-ephemeral-project-folder).
>
> **Rules.** (1) The moment you write something durable in this folder (a design decision, a ruling, a
> reference fact that outlives the migration), add a **POINTER row** here — *not* a copy (single-source,
> [CONVENTIONS §2](../_meta/CONVENTIONS.md)). (2) **Archive done-condition:** this table is **worked to
> empty** — every row lifted to its durable home and the source repointed — *then* the folder is archived. A
> non-empty table BLOCKS archiving. This turns "remember to extract before archiving" into a checklist with a
> clear done-condition.

| Durable item | Lives now (in this folder) | Target durable home | Status |
|---|---|---|---|
| **BoolExpr-reuse** decision — JSON `requires`/`enabled`/`disabled` → a runtime `BoolExpr` tree; the offline harness renders cleartext as a substitute | `cascade-engine-430.md` | decisions ledger (`DEC-boolexpr-reuse`) | ☐ pending |
| **CvDerivedData-skeleton deferral** — the derived-data repository skeleton stays live through shadow, removed only at the atomic cutover; the cascade accumulators do NOT borrow its staleness machinery | `cascade-engine-430.md` | decisions ledger, or `reference/engine/save-load-format.md` | ☐ pending |
| **readJson is a (future modding) TOOL** — the `--render` readable-item-summary surface; out-of-game "what do my changes do" | `cascade-engine-430.md` + this folder's `README.md` | a `Tools/ReadJson/` README / modding-tools doc | ☐ pending |

*(Empty table ⇒ nothing left to extract ⇒ safe to archive. Seeded 2026-06-20 from the docs-parity pass; add rows as more durable knowledge lands here.)*
