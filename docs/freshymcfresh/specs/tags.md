# Unit tags — glossary

The catalogue of a unit's **immutable, accounting-only classification tags** (the `tags` block). This is the
**glossary** (the namings); the **system** — what a tag is, the *"can a promotion grant it?"* mutability test, how
`IS_<TAG>` predicates read tags — is the [json spec](json.md) §8. Sibling of [skills.md](skills.md).

> **Started, not complete** (owner: a glossary needn't be complete to begin). Tags are a first-pass set derived in
> `curate_unit.py`; many units are untagged and that's fine (low-risk, fixed in validation).

## What a tag is (recap)
- **Immutable** — derived from the unit's *type*, re-set only at creation/upgrade; **not** promotion-grantable.
- **Accounting-only** — pure membership, **no behaviour or modifiers**; read by `IS_<TAG>` predicates for
  counting/gating.
- **Overlapping** — a unit holds several at once.

## Tags (first pass)

### Role / category — derived from `DefaultUnitAI` + the IS_MILITARY signal
| tag | meaning | derivation |
|---|---|---|
| `military` | a military unit | `bMilitarySupport` (the IS_MILITARY signal) — **suppressed** when a specific role below applies |
| `civilian` | a genuinely-civilian unit | rides with `worker`/`merchant`/`settler`/`missionary` — **not** `spy` |
| `worker` | builds improvements | `UNITAI_WORKER` / `_SEA` → `worker` + `civilian` |
| `settler` | founds cities | `UNITAI_SETTLE` → `settler` + `civilian` |
| `missionary` | spreads religion | `UNITAI_MISSIONARY` → `missionary` + `civilian` |
| `merchant` | trade-mission unit | `UNITAI_MERCHANT` → `merchant` + `civilian` |
| `spy` | runs espionage missions (only spies do) | `UNITAI_SPY` → `spy` (not civilian, not military) |

### Tech / equipment class — ⏳ deferred (from unitcombats, post-migration)
`gunpowder` (uses gunpowder) · `mechanized` (mechanical/motorised) · … — these are *type* classes a unit gains on
upgrade (a swordsman → rifleman gains `gunpowder`). To be derived from **unitcombats** in the post-migration
unitcombat pass, not the first cut.

### Criminal-type — ⏳ deferred ("map the lunacy")
The 13 `UNITAI_INFILTRATOR` units — `biker_gang` · `burglar` · `exile` · `gunfighter` · `hacker` · `mobster_car` ·
`robber` · `rogue` · `scoundrel` · `street_gang` · `technarchist` · `thief` · `thug` — the hidden-nationality
"criminal-but-not-criminal" group. Tag name(s) **TBD** (owner: a separate mapping pass).

## Open
- The **criminal-type** tag name(s) + the hidden-nationality nuance.
- `gunpowder`/`mechanized`/… from **unitcombats** (post-migration).
- **Completeness** — most units untagged for now (fine).
- The **bSpy skill → `spy` tag** reconciliation (the spy notion is mis-filed as a skill too — drop the skill).
- Whether `IS_<TAG>` predicates *are* tag-membership or independent queries — owner leans **independent**
  ([json](json.md) §3.7).

## See also
- [json.md](json.md) §8 — the system (the four-block classification model).
- [skills.md](skills.md) — the sibling (mutable abilities). · [state.md](state.md) · [capabilities.md](capabilities.md).
