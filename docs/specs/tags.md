# Unit tags — glossary

The catalogue of a unit's **immutable, accounting-only classification tags** (the `tags` block). This is the
**glossary** (the namings); the **system** — what a tag is, the *"can a promotion grant it?"* mutability test, how
`IS_<TAG>` predicates read tags — is the [json spec](json.md) §8. Sibling of [skills.md](skills.md).

> **Open by design (owner).** The tag set grows as data is authored — identifying new tags is an ongoing activity
> for the life of the mod ([json.md §8](json.md): the classification registries mint from authored keys), so this
> glossary catalogues the tags identified so far and more arriving is the normal state, never a gap to close. A unit
> carrying no tag yet is fine (low-risk, filled in validation).
>
> ⚖ **AN EXTRA TAG COSTS NOTHING — certainty is NOT a gate (owner):** *"you can always have more tags, it doesn't
> hurt to add an extra tag, even though we don't fully know what it does."* A tag is inert until something queries
> it, so a surplus one is harmless while a MISSING one is not: it leaves its combat class doing identifier duty,
> which is precisely what blocks the class purge ([engine.md](../reference/engine.md) UnitCombat). ⛔ So do not
> withhold a tag pending a decision about what it means — author it and refine later; a wrong tag is a one-line
> data edit. This is what the asymmetry looks like applied to classification, and it mirrors the emit surface's
> *"too many events is better than not enough"* ([event-spine.md](event-spine.md)).

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

### Tech / equipment / type / domain identity — derived from unitcombats (first pass DONE)

**A unit's effective tags are its OWN ∪ its combat classes'.** The identity tag is authored ON THE UNITCOMBAT
(`TAG_BY_UNITCOMBAT` in `curate_common.py`, emitted by `curate_unitcombat.py`), and the engine unions a unit's
combat classes' tags into the unit at load — `CvUnitInfo::deriveAtRegistryComplete`, over primary `<Combat>` +
`<SubCombatTypes>`, the same walk the sizeMatters base ranks already use. ADDITIVE — the unit keeps
`UNITCOMBAT_X` (the stat-holding modifier source); the tag is its queryable identity.

⛔ **A unit carries NO baked copy, and that is the point.** Baking the fold into the unit's own block put one
fact in two places, so re-tagging a class left every unit of it stale until re-curation. One home, derived at
load. ⚑ The union is over primary + subs precisely because a tag is creation/upgrade-set and **not
promotion-grantable** — a combat class a PROMOTION grants therefore contributes no tag, so nothing is missed by
not walking the runtime set.

Only the OBVIOUS identities map; the size/species/motility/weapon taxonomy stays FLAGGED
(`sizeMatters`/data), never forced ([unitcombat-tag-mapping.md](../plans/structural-cleanup/unitcombat-tag-mapping.md)):

- **tech / equipment:** `gunpowder` (uses gunpowder) · `mechanized` (mechanical/motorised) · `mounted` (cavalry) ·
  `armored` (vehicular/tank armour). *Type classes a unit gains/loses on upgrade — a swordsman → rifleman gains
  `gunpowder`; a `mounted` horseman loses `mounted` upgrading to a helicopter.*
- **type / combat:** `melee` · `archery` · `siege` · `recon`.
- **domain (from a combat class, complementing the `DOMAIN_*`-derived `landUnit`/`seaUnit`/`airUnit`):** `naval` ·
  `air`.
- **NEW vocabulary (owner-approved 2026-07-21):** `hero` (hero-unit identity, `UNITCOMBAT_HERO`) · `animal`
  (`UNITCOMBAT_ANIMAL`/`SEA_ANIMAL`) · `space` (spacecraft + space workers, `UNITCOMBAT_*_SPACESHIP`/`SPACE_WORKER`).
- **NEW functional/role vocabulary (owner-approved 2026-07-21, flagged-remainder 2nd pass):** `police`
  (`UNITCOMBAT_LAW_ENFORCEMENT`) · `medic` (`UNITCOMBAT_HEALTH_CARE`) · `missile` (`UNITCOMBAT_MISSILE`/`BALLISTIC`) ·
  `synthetic` (hi-tech artificial troops — `UNITCOMBAT_ROBOT`/`HITECH`/`CLONES`/`NANITE`/`NANOMORPHIC`) · `diplomat`
  (`UNITCOMBAT_DIPLOMAT`) · `entertainer` (`UNITCOMBAT_ENTERTAINER`). Plus more units folded onto EXISTING tags:
  `recon` (`HUNTER`/`STRIKE_TEAM`) · `naval` (`COMMODORE`/`CAPTAIN`) · `merchant` (`EXECUTIVE`) · `civilian`
  (`PACIFIST`) · `siege` (`ROCKET_LAUNCHER`).

**Queryable now:** the `IS_<TAG>` predicate ([json.md §3.5/§8](json.md)) reads the unit's folded tag bitset —
`{unit: IS_MOUNTED}` / `IS_GUNPOWDER` / `IS_NAVAL` / … evaluate live (`cascadeEvalCondition`), and the per-tag
tally (`CvCascadeTally::countUnitsWithTag`) counts them at empire/team/world scope.

### Criminal-type — `outlaw`

Derived from the **criminal combat CLASS**, not a `DefaultUnitAI` role. A unit is criminal-type →
tag **`outlaw`** iff its **primary `<Combat>` is `UNITCOMBAT_CRIMINAL`** *or* `UNITCOMBAT_CRIMINAL` appears in its
**`<SubCombatTypes>`**. ⚑ That rule needs no special case: it is exactly "primary ∪ subs", which IS the union
above, so `outlaw` is simply `UNITCOMBAT_CRIMINAL`'s authored tag like any other.

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

- **The FLAGGED unitcombat remainder** — the 318 unitcombats left FLAGGED in the first-pass mapping
  ([unitcombat-tag-mapping.md](../plans/structural-cleanup/unitcombat-tag-mapping.md)): the taxonomy families
  (weapon/size/species/quality/group — stay `sizeMatters`/data) + the ambiguous individual classes
  (`COMBATANT`/`PACIFIST`/`HITECH`/`ROBOT`/`LAW_ENFORCEMENT`/… — need owner calls, e.g. a `police`/`medic`/`missile`
  tag). Editable follow-up; map-the-obvious-flag-the-unsure, no completeness gate.
- The **bSpy skill → `spy` tag** reconciliation (the spy notion is mis-filed as a skill too — drop the skill).
- `IS_*` predicates are **independent queries** (not tag-membership), but **may be defined to encompass tags**;
  JSON-definable + predicate groups come post-migration ([json](json.md) §3.7).

## See also

- [json.md](json.md) §8 — the system (the four-block classification model).
- [skills.md](skills.md) — the sibling (mutable abilities). · [state.md](state.md) · [capabilities.md](capabilities.md).
