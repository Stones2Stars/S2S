# 9. Auxiliary & bespoke sections

> Part of the **[json](../json.md)** spec.

Data read by a specific system, not the cascade. Use only when the entity needs it:

- **`policies`** — **pure empire STATES** an entity enacts: named declarative on/off conditions of the whole
  civilization (`noForeignTrade`, `noCorporations`, `allReligionsBanned`, `fixedBorders`, `noNonStateReligionSpread`,
  …). Granted by a **civic** (adopted — active while the civic is in force) or a **trait** (permanent while the trait
  is held) — one meaning, two grantors, exactly parallel to a tech granting a [capability](../capabilities.md). **A policy
  is a PURE STATE, never a parameterized/targeted rule:** `allReligionsBanned` ✓;
  `onlyAllowedToBuildReligion: X` ✗ — a *targeted* restriction that carries a WHAT is an [enabler](../enabler.md) concern
  (`enables`/`disables`/`requires`), not a policy. This is the group-unambiguity discipline (each group name = exactly
  one meaning; cf. empire `capabilities` vs unit `skills` vs `tags`). *(NOT here: civilization selectability
  `playable`/`aiPlayable` → `identity` §7, load-only. The legacy NPC `stronglyRestricted` build-lockdown and its
  `EnabledCivilization` whitelists are a KILLED mechanic — owner: a civ whitelist is poorly visible game design;
  techs decide what a civ can build, and `disables` covers any bar ([superseded-ideas #38](../../architecture/superseded-ideas.md)).
  Some legacy trait keys under `policies` are EFFECTS, not states: `freeSpecialistPer{World,National,Team}Wonder`
  (free specialists scaled by wonder count, CvCity:5764) belong to the `freeSpecialists` modifier family, keyed by
  the `WORLD_WONDER`/`NATIONAL_WONDER`/`TEAM_WONDER` count token (§3.1).
  (NB `nonStateReligionCommerce` was *suspected* an effect but is VERIFIED a pure STATE — a Free-Church permission that
  non-state religions' `stateReligionCommerce` applies — so it correctly STAYS a policy.)*
> **⛔ THERE IS NO `succession` SECTION — it was an INVENTION, and it is the `cityFounding` failure repeated
>.** A minted block reached this spec's bespoke list, the curator, `CvTraitInfo`'s members and the
> authored data, so every layer ratified it and each reader in turn found it sanctioned.
> ⚖ **A LADDER IS AN `enables` EDGE.** "What does holding this unlock?" is the GENERATE pass's own question
> ([enabler.md §2](../enabler.md)): a rung `enables` the rung above it, and the gate that a rung needs the one
> beneath is its `requires` — the shape §9 already specs for a promotion line. Ordering needs no section of
> its own, and a manual upgrade link is likewise an edge, never a bespoke key.
> ⛔ The tell that it was never real: NOTHING authored `enables.traits`, because the edge that belongs there
> was being parked in a section instead — so the enabler could not see a relationship it owns.
- **`promotionLine`** (PROMOTION) — `{ PROMOTIONLINE_X: rank }`, the promotion's rung on a named ladder.
  **⚖ A LINE IS A LADDER, AND HOLDING A RUNG IMPLIES THE RUNGS BENEATH IT** — each level's `requires.build`
  names the level below (`ACCURACY3` → `ACCURACY2` → `ACCURACY`), so a unit carrying the top of a line carries
  the whole chain. ⇒ **What that unit HAS from the promotion is the SUM down the line**, not the rung's own
  value.
  ⚑ **That is why there are TWO reads, answering different questions** — the promotion's own getters say what
  THIS RUNG contributes (what the pedia says *about* a promotion), and the line accrual says what a unit
  HOLDING it actually has (what the unit's tooltip shows). Neither approximates the other; a consumer picking
  the wrong one displays a number the unit does not have. The accrual's membership is derived ONCE at load and
  summed in exactly one place ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)),
  never rebuilt per read.
  ⛔ **A STATUS promotion accrues only ITSELF** — status / affliction / equipment lines are parallel states
  rather than a ladder, so summing them would invent a compounding that does not exist.
- **`excludes`** — same-tier mutual exclusion (conflicting traits).
- **`produces`** — a Build's outcome FKs (what laying it creates).
- **`replacedBy`** — a conditional whole-entity swap (an alternate Info under a culture level / game option; e.g.
  `CULTURELEVEL_ALT_POOR`). *(NOT the building `ReplacementBuildings`, which is reversible dormancy → `requires.operate.dormant`, §4.2/§4.3.)*
- **`condition`** (Victory) · **`effect`** (Vote) · **`outcomes`** (mission results) · **`mapGeneration`**
  (placement/spawn config). *(**`vision`** is NOT here — it is an ordinary modifier family with its own machine,
  [vision.md](../vision.md): a sight budget spent walking outward, exactly as movement works.)*
- **`shrine`** — the building is a religion's SHRINE: `shrine: RELIGION_X` (the religion FK). A top-level
  section, not an `identity` marker — the shrine relationship IS the data.
  ⚖ **THE COMMERCE LANDS ON THE SHRINE BUILDING, IN THE CITY THE SHRINE STANDS IN** — an ordinary
  `<commerce>.city.flat` entry on the BUILDING, `per`-scaled by the count of cities holding the religion
  (`{type: RELIGION_X, scope: "world"}`, which resolves through the same count legacy scaled by). It needs no
  condition: a building deposits only where it stands and only while active, so its presence IS the gate — the
  [the deliveryguy ownership rule](../../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule) rule read straight, since the shrine building
  is what brings the commerce to the table.
  ⛔ **The values do NOT live on the religion**, and a `shrine` block holding per-commerce magnitudes is not a
  home for them — that is a bespoke section carrying a MAGNITUDE, outside the one machine
  ([every modifiable number is a yield](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)).
  ⚠ **Do NOT gate it on `IS_HOLY_CITY` instead.** The tempting symmetry with the corp HQ fails here: a
  headquarters building is PLACED by `setHeadquarters`, while a shrine must be BUILT — so a holy city that has
  not built its shrine would start collecting.
  ⚑ **Its AI valuation reads the `expected*` what-if, never the compiled sum** — the scaler is what
  makes a shrine worth building, so an unscaled read prices it at a single point of commerce and the AI
  "downprioritizes building a shrine over taking tech."
- **`headquarters`** — the corp-HQ analog of `shrine`: the building is a corporation's HEADQUARTERS,
  `headquarters: CORPORATION_X` (the corporation FK). Same FK shape as `shrine`, one for religion and one for
  corporation.
  ⚖ **But the VALUES live elsewhere than the shrine's, and the difference is the mechanic, not an inconsistency.**
  The HQ's per-commerce revenue stays on the **corporation** as `<commerce>.city.flat` gated
  `{IS_HEADQUARTERS: CORPORATION_X}`, because the HQ is a DESIGNATION the engine places and announces
  (`SEVT_CITY_HEADQUARTERS_ADDED / _REMOVED`), so the predicate has a crossing to ride. A shrine has no such
  designation — only a building somebody BUILT — so its values live on that building, whose presence is the gate.
  ⇒ Both land in the one city that holds the thing; what differs is whether a maintainable predicate exists to
  express it. ⛔ Do not "unify" them by moving either: each is on the only carrier its own mechanic supports.
  ⚑ Two more corp-HQ shapes ride this FK and are not authored anywhere today: the HQ's FREE UNIT is an ordinary
  `grants` payload keyed off the headquarters fact ([triggers.md](../triggers.md)), never an info getter; corp-vs-corp
  EXCLUSION is a same-tier `excludes` entry (above), not the consumed-bonus overlap competition currently answers
  from alone.
- **`spread`** (UNIT) — the unit's per-religion / per-corporation **spread strength** (a standing capability, NOT a
  timed handout): `spread.religion: { RELIGION_X: N }` / `spread.corporation: { CORPORATION_X: N }` — keyed magnitude
  maps (`N` = the legacy `iReligionSpread`/`iCorporationSpread`). Its **own** block on purpose:
  burying spread strength under `grants` (one-shot/recurring handouts) misleads a modder — it is what the unit is
  *able* to spread and *how strongly*, read by the missionary / corporate-executive spread systems.
- **`sizeMatters`** — the data the **Size-Matters** combat system needs (gated by `GAMEOPTION_COMBAT_SIZE_MATTERS`),
  a dedicated block per the own-block rule below. It is **cross-entity** — "size matters is mostly governed in
  unitcombat" — so the **base ranks are authored on UnitCombat** (the source) and summed onto the unit at load, while
  **Promotion** carries the runtime deltas:
  The block's keys are the same across entity kinds; the **base-vs-delta semantic is carried by the entity**, not a
  key suffix. Members: base ranks `qualityBase`/`groupBase`/`sizeBase` (UnitCombat only); the scalars `quality`,
  `group`, `sizeModifier`, `maxHP`; `combatModifier: { perSizeMore, perSizeLess, perVolumeMore, perVolumeLess }`;
  `cargo: { smSpace, volume, volumeModifier }`.
  > **⛔ `maxHP` IS ONLY EVER AN INCREASE — HP ITSELF IS A PURE ENGINE VALUE AND IS NOT CURATED.** The
  > base is the `MAX_HIT_POINTS` global; authored data contributes percentage/flat INCREASES on top and nothing
  > else, which is why every consumer of this key feeds `changeExtraMaxHP` rather than seating a base.
  > ⚠ **Reading it as the base is silent and total.** No entity authors `sizeMatters.maxHP` — none does in the
  > legacy XML either, because the value was always the engine default — so a base read resolves to 0 and every
  > unit in the game floors to ONE hit point. Combat then ends on the first connecting hit, so the WINNER takes
  > no damage (the loser still dies, through a direct `setDamage`), and the interface draws every health bar
  > against `MAX_HIT_POINTS` and shows them all red. Nothing errors and the build is green throughout.
  > ⛔ **AND IT REACHES COMBAT RESOLUTION, NOT ONLY THE DISPLAY, BECAUSE A UNIT'S STRENGTH IS SCALED BY ITS HP
  >: a damaged unit also does less damage.** `CvUnit::currCombatStr` is
  > `maxCombatStr × getHP() / getMaxHP()`, and `currEffectiveStr` normalizes by firepower over the same ratio —
  > so `getMaxHP()` returning 1 collapses that whole curve to a STEP: a unit is at full strength or it is dead,
  > with nothing in between. ⇒ The wrong base did not merely mis-draw a bar; it deleted attrition from every
  > combat in the game, which is why this key's disposition is a combat question rather than a UI one.
  > ⚑ The same test settles the rest of this block: a key here is a DELTA unless the entity's own row says it is
  > a base rank, and the three `*Base` names are the only bases.
  - **UnitCombat** (the intrinsic **base ranks** + SM combat data): carries `qualityBase`/`groupBase`/`sizeBase` plus
    `maxHP`/`combatModifier`/`cargo`. A base equal to the legacy `−10` "unset" sentinel is emitted **absent** (never
    `0` — `0` is a real rank).
  - **Unit** (its own SM fields): `combatModifier` (its per-rank combat mods), plus `groupSize`/`baseCargoVolume`
    where authored. ⚖ **`smSpace` is DERIVED, not authored: under Size Matters a carrier's space follows
    from how many units it can carry** — so it derives from the `cargo` family's capacity, the same
    derived-at-load class as the ranks below. The data agrees: **no unit authors it** (legacy `iSMCargo` appears
    in no unit record); the only authorings are the 23 PROMOTION deltas, which are the delta plane working as
    intended. ⛔ So carrying capacity has ONE home — **`cargo`** ([modifier.md §6](../../cascade.md)) — and the SM
    figure is read off it, never a second authored number to keep in step.
    The unit's quality/group/size **RANK is DERIVED at load, never stored**: `Σ` over the unit's
    combat classes (primary `combatClass` + the `combatClasses` subs) of each `*Base` where `> −10` — reproducing the
    engine post-load pass (`CvUnitInfo`: `m_iBaseGroupRank += getGroupBase()`). The group rank feeds `getUnitCountSM`
    (`count ⁄ 3^(groupRank−1)`), so a stubbed `0` divides by zero — the getter MUST return the real derived value.
  - **Promotion** (the SM **deltas** a promotion applies): `quality`/`group`/`sizeModifier`/`maxHP` +
    `combatModifier`/`cargo` — same keys, applied as changes when the promotion is gained.

  Effective runtime rank = the derived info base + `Σ` held-promotion changes + the engine merge/split accumulators
  (`getExtraQuality`/`Group`/`Size` — live engine state, **never** data). Block absent ⇒ the entity carries no SM
  data.

  > **⛔ SIZE MATTERS PIVOTS ON THE UNIT TYPE'S OWN RANK SUM, NEVER ON A GLOBAL CONSTANT.** The rank
  > scaling expresses how far THIS unit sits from what its TYPE is, so a unit at its type's profile is offset 0
  > and receives **exactly what the data authored**. The deviation is the only thing SM says: a `groupSpawn` roll
  > below the type's own group class, a merge, a rank promotion.
  > ⚑ **This is what keeps an authored number meaningful, and the option gate is why it has to.**
  > `GAMEOPTION_COMBAT_SIZE_MATTERS` **defaults OFF** and the non-SM read takes the authored strength RAW, so any
  > pivot that is not the type's own makes one number mean two different things depending on a player toggle.
  > ⛔ **So the authored data is NEVER the place to correct a pivot mismatch** — raising a strength to compensate
  > inflates that unit in the DEFAULT game, by the same factor, invisibly.
  > ⚠ **The retired form subtracted a flat 15** (three ranks at a nominal 5). That is the MILITARY plane's
  > profile — 851 of ~1000 non-animal combat units sum to exactly 15 — but **the animal taxonomy was never
  > normalised to it**: only 316 of 582 animals reach 15, and 79 sit at 4–11, where the shortfall divides their
  > authored strength by `1.5^n` (up to 86×). ⚑ The worked case: `UNIT_ELEPHANT_ASIAN` authors 6, its type sums
  > 14 (`QUALITY_MEDIOCRE 4` + `GROUP_SQUAD 3` + `SIZE_HUGE 7`), and `groupSpawn` rolls SOLO half the time — so it
  > was delivered 4.0 at its own profile and **1.77** once solo, displaying and fighting as **1** while still
  > paying a full `outcomes.kill` reward.
  > ⚑ **The tell that the pivot was the wrong half, not the data:** the correction the data would have needed is
  > exactly the factor the pivot was already applying — a fudge factor whose existence says two operands are on
  > different scales ([AGENTS.md](../../../AGENTS.md#conduct) drift detector 2). *(This is the pattern for every game-option-specific system — each gets its own block; `hideAndSeek`
  below is its sibling.)*

  > **⚖ THE SM REVOLT-PROTECTION PLANE IS DELIBERATELY OFF, AND IT STAYS OFF UNTIL REVOLUTIONS ARE REWORKED
  >.** `CvUnit::revoltProtectionTotal` returns the plain authored value — the SM branch and the
  > `setSMRevoltProtection()` call that feeds it are both commented out, because the multiplicative plane
  > *"seems to give some weird results"*.
  > ⛔ **So `m_iSMRevoltProtection` is SERIALIZED with no live writer, and that is NOT a defect to clean up.**
  > It is the exact shape a writerless-accumulator sweep deletes on sight, and deleting it would silently retire
  > a switch someone turned off on purpose — a deliberate off-switch is protected by its REASON
  > ([AGENTS.md](../../../AGENTS.md#design)), which is why the reason is recorded here rather than left in a
  > comment. Leave the member, its save tag and the commented plane as they are; the verdict is re-taken when
  > revolutions are.
- **`hideAndSeek`** — the concealment-vs-detection CONTEST (gated by `GAMEOPTION_COMBAT_HIDE_SEEK`), the
  own-block sibling of `sizeMatters`. **Two contest members, one per side of the equation:** `concealment` (how
  well this unit hides) and `detection` (how well it finds a hidden one, per method it answers, each entry
  qualified `{unit: HAS_<SKILL>}`). Both are graduated magnitudes and both may be NEGATIVE — a negative
  detection deposit is counter-detection, a negative concealment strips cover (the `WANTED` line does exactly
  that).
  ⛔ **The block's third member, `method`, belongs to the CLASSIC system, not the contest** — the ONE method a
  unit hides by when the option is OFF, carried from the legacy single `<Invisible>` tag (`"method":
  "camouflage"`). Legacy authored TWO invisibility planes — the single classic tag, and the per-type intensity
  tables the contest replaced — and only units carrying the single tag were ever classically invisible. The
  classic engine branch reads `method` ALONE; deriving it from the method-skill union instead made every
  contest-only hider (the robber class authors no classic tag) classically invisible for the first time ever,
  and border patrols stopped killing criminals. ABSENT means classically never-invisible; the skill set still
  carries the contest's membership beside it.
  ⚑ **The METHOD is not in this block at all — it is a [skill](../skills.md)** (`camouflage`, `cloaked`,
  `disguised`, …), because a promotion can grant one and optical camouflage is precisely that
  ([vision.md §4](../vision.md)). A [tag](../tags.md) could not hold it: tags are not promotion-grantable, and 73
  promotions author a method.
  ⛔ **It is NOT part of `vision`, and the separation is load-bearing.** `vision` answers *how far do you
  see*; this answers *do I perceive what is standing there*. The legacy engine's two evaluations bled into each
  other for years, so expressing them as one family is what lets that bleed re-form. The contest READS the
  [vision](../vision.md) budget for reach and never the reverse.

  > **⚖ TRAINING A UNIT ABOVE ITS BASE RANK IS AN OFFSET, `base + x` — NEVER AN ABSOLUTE RANK.** A base
  > group rank is DERIVED per unit from its combat classes, so an absolute number means a different thing for
  > every unit — and a DOWNGRADE for one whose base already exceeds it — while an offset stays correct when
  > re-tagging a combat class moves that base. ⚑ The engine already agrees: its merge ceiling
  > (`CvUnit::eraGroupMergeLimit`) was written in exactly this form long before the build side wanted it, which
  > is why an absolute rank would have contradicted the live cap rather than merely read oddly.
  > ⚑ **The ERA bounds `x`** — it decides how many merges are reachable — so the offer is per-ERA while the base
  > is per-UNIT. Two sources, one number; collapsing them into an absolute rank loses both.
  > ⚖ **WHY IT EXISTS: the merge GRIND, not the cost** — merging hundreds of units by hand in the late
  > game is the problem, and building at `base + x` is the shortcut past it. ⇒ **The cost is therefore the
  > EQUIVALENCE, not a free choice:** it must come out the same as building `3^x` units and merging them, or the
  > shortcut is a trap or an exploit. That falls straight out of the rank geometry above (`count / 3^(rank−1)`),
  > so it is derived rather than balanced.
  > ⛔ It is a QUANTITY term, NOT a training-PACE percent: the Size-Matters pace discount still applies on top,
  > because a directly-built ranked unit IS the merged result and that discount exists precisely because units
  > merge. ⚠ The ceiling, the offer range and the equivalent cost must be ONE implementation shared with the
  > merge gate, or the price and the reachable rank can disagree
  > ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)) — note the ceiling
  > lives on `CvUnit` today, which a build menu cannot ask, having no unit yet.
  >
  > **⚖ A PRE-MERGED BUILT UNIT IS INDISTINGUISHABLE FROM A NORMALLY MERGED ONE** — that is the
  > acceptance bar for the build side, and it decides the implementation rather than merely describing the goal.
  > ⛔ **RANK IS NOT A STORED FIELD — it is carried by PROMOTIONS**, so there is no "set the rank" to write.
  > `CvUnit::mergeUnits` raises it by applying its offset through `normalizeUnitPromotions` over the
  > group-upgrade/downgrade promotions, and a built rank-up reaches the same state ONLY by going through that
  > same application. A bespoke loop beside it is a second implementation of the rank that will drift.
  > ⚠ **Only the GROUP rank rises on a merge** (its offset starts at 1; the QUALITY offset starts at 0 and
  > merely carries the sources' own quality promotions forward, and size is not touched). So `base + x` moves
  > the group axis — do not "complete" it by moving quality and size as well.
  > ⚑ The rest of the merged object needs no faking: a merge averages its three sources' XP and keeps the
  > promotions all three shared, and three FRESH units average to fresh XP and share only their free promotions
  > — so a newly-built ranked unit already equals "three fresh units merged" without inventing any history.
- **bespoke** object-sections, each read by its own system: `promotionLine` · `buildUp` · `shrine` · `headquarters` ·
  `spread` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` ·
  `unitCapability` · `sizeMatters` · `hideAndSeek`.

A dedicated system's data lives in its **own block** — a module is "on" iff its block exists and is non-empty — so
a system can be added, swapped, or removed as a unit.

---

