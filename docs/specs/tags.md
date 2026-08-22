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
> *"too many events is better than not enough"* ([spine.md](../spine.md)).

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

### Domain — from the unit's `DOMAIN_*`

| tag | meaning | derivation |
|---|---|---|
| `landUnit` | a land unit | `DOMAIN_LAND` → `landUnit` |
| `seaUnit` | a sea unit | `DOMAIN_SEA` → `seaUnit` |
| `airUnit` | an air unit | `DOMAIN_AIR` → `airUnit` |

⛔ **None of the three is what `IS_LAND` / `IS_WATER` / `IS_AIR` read, and believing otherwise is how a gate goes
silently wrong.** Those are NATIVE predicates, not tag lookups: `IS_LAND`/`IS_WATER` ask about a PLOT, and
`IS_AIR` asks the unit's own `identity.domain` — which is the ruling below, working. Only a token with no native
spelling falls through to the tag registry (`IS_RECON` → the `recon` tag).

> **⛔ A TAG SAYS WHAT A UNIT *IS*; A DOMAIN SAYS WHERE IT *OPERATES* — two axes, and the domain is NOT a tag
> question (owner).** The domain has its OWN entry on the unit (`identity.domain` → `CvUnitInfo::getDomain()`),
> and every domain read goes there. ⚑ The reason is the one that decides it: answering "where does this operate"
> from the tag set means FILTERING ALL TAGS for something a single field already holds.
> ⚖ The three tags below are still carried, and that is fine — a surplus tag is inert (the ruling above), so
> there is nothing to gain by removing them. ⛔ What must not happen is a consumer reading a DOMAIN through
> them; there is deliberately no composition over them for that.

> **⛔ IMMOBILE IS NOT A DOMAIN (owner).** A domain says WHERE a unit operates — land / sea / air / space — and
> not-moving is orthogonal to that, so it never belongs on this axis and gets no tag. ⚠ The data does not yet
> agree: a handful of units author `DOMAIN_IMMOBILE` (space defenders and the ICBM), which is why they carry no
> domain tag at all — they are the unmodelled `space` set, not a fourth kind of place.
> ⚑ **A DOMAIN IS EXCLUSIVE, and crossing one is a SKILL, never a second domain.** Verified: no unit carries two
> domain tags. A helicopter is `DOMAIN_LAND` + `canMoveAllTerrain` ([skills.md](skills.md)) — it flies over the
> lake without ever being an air or sea unit — and the same shape covers every land/sea unit with
> `canMoveImpassable`. So a unit's domain is answerable as ONE value; do not model multi-domain membership.

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
(`sizeMatters`/data), never forced:

- **tech / equipment:** `gunpowder` (uses gunpowder) · `mechanized` (mechanical/motorised) · `mounted` (cavalry) ·
  `armored` (vehicular/tank armour). *Type classes a unit gains/loses on upgrade — a swordsman → rifleman gains
  `gunpowder`; a `mounted` horseman loses `mounted` upgrading to a helicopter.*
- **type / combat:** `melee` · `archery` · `siege` · `recon`.
- **domain (from a combat class, complementing the `DOMAIN_*`-derived `landUnit`/`seaUnit`/`airUnit`):** `naval` ·
  `air`.
- **NEW vocabulary (owner-approved 2026-07-21):** `hero` (hero-unit identity, `UNITCOMBAT_HERO`) · `animal`
  (`UNITCOMBAT_ANIMAL`/`SEA_ANIMAL`) · `space` (spacecraft + space workers, `UNITCOMBAT_*_SPACESHIP`/`SPACE_WORKER`).
- **Animal LIFECYCLE states:** `tamed` (`UNITCOMBAT_TAMED`, 53 units) · `wild` (`UNITCOMBAT_WILD`, 198). `wild`
  is derivable as animal-and-not-`tamed` and is carried anyway, per the extra-tag ruling above — it lets a
  consumer ask for wild animals EXACTLY rather than widening to `animal` and sweeping tamed ones in with it
  (the spawn-neutrality test is the live case).
- **NEW functional/role vocabulary (owner-approved 2026-07-21, flagged-remainder 2nd pass):** `police`
  (`UNITCOMBAT_LAW_ENFORCEMENT`) · `medic` (`UNITCOMBAT_HEALTH_CARE`) · `missile` (`UNITCOMBAT_MISSILE`/`BALLISTIC`) ·
  `synthetic` (hi-tech artificial troops — `UNITCOMBAT_ROBOT`/`HITECH`/`CLONES`/`NANITE`/`NANOMORPHIC`) · `diplomat`
  (`UNITCOMBAT_DIPLOMAT`) · `entertainer` (`UNITCOMBAT_ENTERTAINER`). Plus more units folded onto EXISTING tags:
  `recon` (`HUNTER`/`STRIKE_TEAM`) · `naval` (`COMMODORE`/`CAPTAIN`) · `merchant` (`EXECUTIVE`) · `civilian`
  (`PACIFIST`) · `siege` (`ROCKET_LAUNCHER`).

**Queryable now:** the `IS_<TAG>` predicate ([json.md §3.5/§8](json.md)) reads the unit's folded tag bitset —
`{unit: IS_MOUNTED}` / `IS_GUNPOWDER` / `IS_NAVAL` / … evaluate live (`cascadeEvalCondition`), and the per-tag
tally (`CvCascadeTally::countUnitsWithTag`) counts them at empire/team/world scope.

### Capability — `unpromotable` (the one NEGATIVE tag)

**A unit whose PRIMARY combat class is named as QUALIFIED by no promotion.** Nothing can ever be acquired
through it, so the data says so directly instead of the engine rediscovering it by matching
(`CvUnit::canAcquirePromotion` refuses a tagged unit beside its existing no-primary-class gate).

> **⚖ IT IS NEGATIVE BECAUSE THE VAST MAJORITY OF UNITS ARE PROMOTABLE (owner)** — absent means promotable, so
> the authored membership is the small side: **389 of 2073**, against the 1,684 a positive `promotable` would
> have to carry.
> **⛔ IT ASKS THE PRIMARY ONLY, AND THAT DISTINCTION IS THE WHOLE MECHANIC (owner).** A unit HAS a primary
> combat class and a LIST of them, and they answer different questions: the engine's *can this be promoted at
> all* gate reads the PRIMARY (`getUnitCombatType()`), while promotion MATCHING runs over the whole HELD set
> (`isHasUnitCombat` — primary + subs + promotion-granted). Only the primary is a genuine combat ROLE.
> ⚑ **The subs are the Thunderbrd SPECIES / QUALITY / SIZE / MOTILITY taxonomy — the core reason the unitcombat
> enum bloated at all (owner)**, which [engine.md](../reference/engine.md) already records as ~96% inert
> identifiers and which the tag work exists to unwind. So matching on the held set leaks promotions into units
> whose real class grants none.
> ⚑ **The worked case is the GREAT PERSON, and it is what forced the tag:** every one has primary
> `UNITCOMBAT_PRODIGY`, which **no promotion names** — the intent was already in the data — while its
> `SPECIES_HUMAN` / `QUALITY_ELITE` subs pull in the naturalist and might lines. ⚠ So "great people cannot
> promote" is true of the design and was NOT true of the engine; the tag is what makes it so.
> ⚑ **The derivation is mechanical, never a list** ([curators/README](curators/README.md)): the curator unions
> every promotion's qualified `<UnitCombats>` and tags any unit whose primary is absent from it (or which has no
> primary at all). The population falls out as subdued animals · space and sea workers · sea animals ·
> executives · great people · warlord/captain ranks · nukes · captives · `unit_sleeper`.
> ⚠ **It gates EARNING a promotion, never RECEIVING one** — the free/granted bypass sits ahead of it, so a
> tagged unit still gets what its own type hands it (a great general keeps `PROMOTION_LEADER`).
> ⚖ **A WRONG VERDICT IS CHEAP, SO CERTAINTY IS NOT A GATE HERE EITHER (owner): *"if we notice something that
> should have promos but doesn't, we will figure that out real fast — it's not gamebreaking for a unit to have
> promotions."*** ⛔ This has to be stated for the NEGATIVE tag specifically, because the extra-tag ruling above
> does not carry over unchanged: a surplus POSITIVE tag is inert, while a surplus negative one takes a capability
> away. It is still not a reason to withhold the derivation — a unit that should promote and cannot is loud in
> play and costs a data edit.
> ⇒ **And the fix is on the DATA side, never an engine exemption:** give that unit's primary class a qualified
> promotion and it leaves the tag by itself. The populations to expect this from are the ones whose primary is a
> real ROLE that simply was never wired into a promotion (`CAPTAIN`, `HOVERCRAFT`, `AIR_RECON`), not the
> taxonomy-primaried ones.
> ⚑ **Consequence worth knowing:** `canAcquirePromotionAny()` is what `CvCity::addProductionExperience` gates
> on, so a tagged unit takes no free experience from the city it is created in — the question answers itself at
> the ONE creation step ([triggers.md](triggers.md)) rather than needing a carve-out there.

### Cargo group — from the unit's `SPECIALUNIT_*` membership

`people` (176) · `troop` (94) · `fighter` (16) · `missile` (8) · `vtol` (6) · `captive` (3) · `seaplane` (1).
Derived MECHANICALLY from the unit's own group (`SPECIALUNIT_PEOPLE` → `people`, `curate_common.specialunit_tag`),
never a table — a new group needs no curator edit.

⚑ **This is what made the cargo restriction expressible.** A carrier says WHAT it may carry as the ordinary
`{unit: IS_<TAG>}` qualifier ([modifier.md §6](../cascade.md)), so the group had to be a tag first — and had to be
DISCRIMINATING: `people` and `troop` both reduce to `landUnit` and nothing else, so converting before these
existed would have silently WIDENED every people-only transport into a troop carrier.

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

- **The FLAGGED unitcombat remainder** — the unitcombats still carrying no identity tag: the taxonomy families
  (weapon/size/species/quality/group — stay `sizeMatters`/data). Editable follow-up; map-the-obvious-flag-the-unsure,
  no completeness gate.
- `IS_*` predicates are **independent queries** (not tag-membership), but **may be defined to encompass tags**;
  JSON-definable + predicate groups come post-migration ([json](json.md) §3.7).

## See also

- [json.md](json.md) §8 — the system (the four-block classification model).
- [skills.md](skills.md) — the sibling (mutable abilities). · [state.md](state.md) · [capabilities.md](capabilities.md).
