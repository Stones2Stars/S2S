# 5. Targets — scope-wide, object-plural, or keyed

> Part of the **[cascade](../cascade.md)** spec.

A deposit lands in one of three ways ([json](../specs/json.md) §6.1):

> **⚖ AN EMPIRE→CITIES DEPOSIT HAS TWO LEGS, FOR THE SAME REASON THE AMENITY FOLD DOES**
> (§ THE FOLD HAS TWO LEGS, above). A source above city scope delivers its
> CITY-scope deposits by fanning over the owner's cities — which reaches exactly the cities standing **at that
> moment**, and that is not all of them:
> - **at LOAD the emit order is not uniform**, and nothing makes it so: some empire-level facts are announced
>   before the cities deserialize and some after, so one grantor's fan lands and the next one's iterates an empty
>   list. A fan alone therefore delivers a subset decided by where a member happens to sit in a read.
> - **at PLAY a city that starts existing later** — founded, or acquired — receives nothing from what its owner
>   already holds, permanently.
>
> ⇒ **The second leg is the CITY's: when a city starts existing it folds the city-scope deposits of every source
> its owner already holds.** The trigger is the city's own OWNERSHIP fact, which is the one announcement common
> to founding, conquest and the save read alike — so there is no separate load pass and no city-founded special
> case beside it.
> ⛔ **It must be IDEMPOTENT rather than guarded.** The package already records which sources have deposited into
> it (the same liveness key planes B and C test), so the fold SKIPS what the fan already delivered. Suppressing
> the fan during load instead would work only while a hand-written guard stays in step with an emit order nobody
> controls; the package's own record cannot disagree with what was applied.
> ⚠ This is not a rebuild and not a recompute — the worklist is the owner's HELD sources, each resolved through
> the one per-entry evaluator ([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).

- **scope-wide** — no target: the scope object itself (the city is the common case).
- **plural object-target** (`plots` / `units` / …, predicate-filtered) — realized by evaluating the predicate
  against **every object of that kind in scope** and depositing onto each match. One uniform mechanism: an
  empire-wide sea-tile buff is `production.empire.plots {IS_WATER}`, applied to every worked water plot. This
  retires all the legacy per-plot-type / per-tile accumulators.
- **named-entity key** (`improvements.{FARM}`, `terrains.{…}`, `buildings.{…}`) — a deposit onto a specific
  named target, kept on the source (the deliveryguy, §4).

> **⛔ A KEYED DEPOSIT IS READ AS AN ENTRY-LIST READ OVER THE LIVE SOURCES — never off a scope package.** Outside
> PLOT scope a keyed entry deliberately does **not** fold into the scope's Σflat/Σpercent slots (only the plot's own
> substrate keys resolve there, §2 plot-as-base; the `empires` fan is the one target whose fold IS the deposit). So
> a consumer answering *"how much does this source give THIS target"* asks each live source what IT deposits onto
> that key — the city's OPERATING buildings, its specialists × count, the empire's held traits — and sums.
>
> ⛔ **A SPECIALIST IS A SPECIALIST — FREE OR OTHERWISE DOES NOT MATTER.** The count is the city's WHOLE
> specialist population: the assigned citizens **plus** the free ones (the derivable `freeSpecialists.{X}` grants
> of its operating buildings and empire sources, plus the unattributed ledger a settled Great Person lands in).
> They are one provider kind (§ THE FOUR-PROVIDER LAW), so the origin of a specialist has no standing in what it
> provides. ⚠ Reading only the assigned plane is the shape to recognise, because the two live in SEPARATE members
> (`m_paiSpecialistCount` vs the `getFreeSpecialists` group read) and the assigned one is the obvious getter:
> it makes a settled Great Person contribute NOTHING while the city visibly holds it, which is silent and
> plausible in exactly the way this rule exists to prevent. *(Worked: a settled Great Hunter granted no
> `experience.city.unitCombats` to the hunters trained beside it — the whole point of settling one.)*
> ⚑ **Take the GROUP read once** (`getFreeSpecialists`): it builds an eval ctx and walks the city's operating set
> AND the empire's sources, so a per-specialist call inside a loop pays all of that per specialist.
>
> **⚖ A KEYED ROW'S REACH IS ITS AUTHORED SCOPE, AND BOTH SCOPES ARE REAL ON A BUILDING.** A building is
> a per-city source, so a CITY-scope keyed row means faster HERE — *"units are scoped on the city the building is
> in"* — and the read over the city's own OPERATING buildings is exactly that semantic. An EMPIRE-scope keyed row
> on the same building means faster in EVERY city of the owner, and is answered player-side.
> ⛔ **So the two halves are read at DIFFERENT SCOPES and must each filter to their own**, or the city holding the
> source claims the empire rows a second time on top of the player's answer. This is the live case the
> `collectKeyedTarget` scope filter exists for, and the reason the point form needs it too.
> ⚠ **Neither half is mis-authored data, and re-scoping one to "simplify" the read is a BALANCE CHANGE wearing a
> cleanup.** The empire half is the classic wonder effect (a wonder cheapening a building across the empire) and
> it is populated; the city half is the local one. A cut that collapses them would silently delete whichever
> mechanic it did not keep.
> ⚑ CIVIC- and TRAIT-authored keyed rows are empire-scope by nature — those sources have no city — so the
> player-side walk is the only thing that could serve them, and it does.
> ⚑ **Why it is a rule and not a detail: folding a keyed entry into the scope slot is silently, plausibly WRONG.**
> A building's `experience.city.unitCombats.{UNITCOMBAT_MELEE}` folded scope-wide would hand EVERY unit trained
> there the melee-only experience — a number that looks reasonable, breaks no invariant, and nothing catches. The
> package answers the scope-wide leg; the keyed axes are read beside it.
> ⚠ The read is per-source and cheap because it iterates the handful an entity AUTHORED
> ([materialize at mapFrom](../architecture/patterns/07-materialize-at-mapfrom-no-runtime.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)); it is never a walk of a
> keyed container the info no longer holds, which is the own-data inversion
> ([pedia-read-map](../reference/pedia-read-map.md) finding 2).
>
> ⛔ **A KEYED READ SERVES THE UNCONDITIONED ENTRIES ONLY — the conditioned tail is the VALUATION's, exactly as
> it is on the point plane** ([patterns.md § THE GETTER SETUP](../architecture/patterns.md): the compiled sum,
> the conditioned list and the `expected*` what-if are three distinct reads, and a keyed deposit needs all three
> just as a scope-wide one does). A keyed walk that sums the tail applies every tech-gated and age-gated deposit
> from turn 0 — silently, because the number stays plausible. ⚑ The keyed twin of `expected*` is what serves
> that tail (through the ONE evaluator against the contexts); until it exists a keyed+conditioned deposit is
> honestly UNSERVED, which is the correct exposed state rather than a gap to paper over with an unconditional
> sum ([legacy must fail loud, never mask a cascade gap](../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)).
>
> ⚠ **THE DIRECT-KEYED ADDRESS IS A REAL SHAPE, AND ITS SENTINEL MUST NOT COLLIDE WITH "NOT AUTHORED".** A
> named-entity key may sit straight under the scope with no plural container token
> (`allowedSpecialists.city.{SPECIALIST_X}`, `religion.city.{RELIGION_X}`), so the compiled entry carries NO
> target-segment. A read that treats "no segment" as a failure answers 0 for every such address while the caller
> passes the right family, kind and target — invisible, because nothing errors. The two meanings are opposite
> intents and each needs its own value: *this address carries no container token* vs *that token was never
> authored anywhere*.

---

