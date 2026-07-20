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
- **Represented as an ALWAYS-PRESENT ARRAY OF STRINGS** (owner 2026-07-20) — `"tags": ["military","landUnit"]`,
  never `{name:true}` (a tag carries no value, like a skill). The array is **mandatory even when empty** (`[]`):
  there is no real unit with zero tags, but the schema keeps it present (the unitcombat→tag pass fills the rest).

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

### Domain — from the unit's `DOMAIN_*` (owner 2026-07-20)

| tag | meaning | derivation |
|---|---|---|
| `landUnit` | a land unit | `DOMAIN_LAND` → `landUnit` (read by `IS_LAND`) |
| `seaUnit` | a sea unit | `DOMAIN_SEA` → `seaUnit` (read by `IS_WATER`) |
| `airUnit` | an air unit | `DOMAIN_AIR` → `airUnit` (read by `IS_AIR`) |

The domain is membership of what the unit *is* → a tag. The `DOMAIN_*` **enum stays engine-side** (movement/stacking
are wired to it deeply); the tag is the classification view, and `IS_LAND`/`IS_WATER`/`IS_AIR` read it.

### Tech / equipment class — PERMANENT carve-out (from unitcombats, unitcombat→tag pass)

`gunpowder` (uses gunpowder) · `mechanized` (mechanical/motorised) · `mounted` (cavalry) · … — these are *type*
classes a unit gains/loses on upgrade (a swordsman → rifleman gains `gunpowder`; a `mounted` horseman *loses*
`mounted` upgrading to a helicopter). To be derived from **unitcombats** in the unitcombat→tag pass (a PERMANENT carve-out),
not the first cut.

### Criminal-type — `outlaw`

Derived from the **criminal combat CLASS**, not a `DefaultUnitAI` role. A unit is criminal-type →
tag **`outlaw`** iff its **primary `<Combat>` is `UNITCOMBAT_CRIMINAL`** *or* `UNITCOMBAT_CRIMINAL` appears in its
**`<SubCombatTypes>`**. Curator: `combat_class == UNITCOMBAT_CRIMINAL or UNITCOMBAT_CRIMINAL in SubCombatTypes`
(`curate_unit.py`).

This combat-class signal is **broader** than the old `UNITAI_INFILTRATOR` gate (which caught only **13**): it now
covers **22** units, adding the ones INFILTRATOR missed — `OUTLAW` (primary `RUFFIAN` + subcombat `CRIMINAL`),
`ASSASSIN`/`HASHISHIN` (primary `STRIKE_TEAM` + subcombat `CRIMINAL`), `CUTTHROAT`, `BEGGAR`, `BOSNEGERS`, `HAJDUK`,
`HEZBOLLAH`, `KARAI_PYHARE` — alongside the original INFILTRATOR set (`biker_gang` · `burglar` · `exile` ·
`gunfighter` · `hacker` · `mobster_car` · `robber` · `rogue` · `scoundrel` · `street_gang` · `technarchist` ·
`thief` · `thug`). The full current set (22): `assassin` · `beggar` · `biker_gang` · `bosnegers` · `burglar` ·
`cutthroat` · `exile` · `gunfighter` · `hacker` · `hajduk` · `hashishin` · `hezbollah` · `karai_pyhare` ·
`mobster_car` · `outlaw` · `robber` · `rogue` · `scoundrel` · `street_gang` · `technarchist` · `thief` · `thug`.

> `hiddenNationality` is **not** the gate — it is a **skill** (mutable, promotion-grantable; e.g.
> `PROMOTION_PROUD_PIRATE` grants it), see [skills.md](skills.md) §1. The criminal-type `outlaw` tag and the
> hidden-nationality skill are independent: most outlaws carry the skill, but the tag is defined by the combat class.

## Open

- `gunpowder`/`mechanized`/… from **unitcombats** (PERMANENT carve-out — unitcombat→tag pass).
- **Completeness** — most units untagged for now (fine).
- The **bSpy skill → `spy` tag** reconciliation (the spy notion is mis-filed as a skill too — drop the skill).
- `IS_*` predicates are **independent queries** (not tag-membership), but **may be defined to encompass tags**;
  JSON-definable + predicate groups come post-migration ([json](json.md) §3.7).

## See also

- [json.md](json.md) §8 — the system (the four-block classification model).
- [skills.md](skills.md) — the sibling (mutable abilities). · [state.md](state.md) · [capabilities.md](capabilities.md).
