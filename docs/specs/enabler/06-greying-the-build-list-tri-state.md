# 6. Greying — the build-list tri-state falls out for free

> Part of the **[enabler](../enabler.md)** spec.

The same gate that decides buildability yields **why** a thing isn't buildable — no separate "why greyed" pass.
Each clause carries a disposition (set once by its kind):

| state | condition |
|---|---|
| **HIDDEN** | not in CAN GET — generation never reached it (or it was obsoleted/replaced/banned away) |
| **LISTED** (buildable) | in CAN GET ∧ all `requires` met ∧ under `allowed` |
| **GREYED** | in CAN GET ∧ only *greyable* clauses unmet — a connectable resource, an unadopted civic (named to the player) |

Grey vs hide is a **UI choice per clause**, not engine behaviour: author a resource on `requires` to **grey**
(surfacing "go get copper"), or on `enables` to **hide** until present. General lean: grey on resources.

> **⛔ THE GATE CARRIES *WHY* IT FAILED, NOT A BOOLEAN** — the tooltip and the AI both need to say what
> is missing, so the stored verdict is the failing clause's IDENTITY and hide-vs-grey is read off it. A
> capped-out wonder (HIDE — nothing to do) and a missing resource (GREY — go connect it) cannot share one bit.
> ⛔ **So the clause set is never collapsed into one flag** — dormancy, the entity-level option gate, `requires`,
> and each `allowed` cap (self / group / per-city category) are each their own reason; a HIDE clause must never
> present as GREYED merely because it shares the bit. This reaches the AI as much as the screen: a consumer
> testing `>= GREYED` gets a different answer once a hide-clause stops greying. The REQUIRES reason names the
> clause KIND; which atom is unmet is the requires tree's own per-clause render, so the two compose.
> ⚑ The reason is STORED rather than re-derived — a consumer that re-evaluated the clauses to find the cause
> would be a second gate implementation, free to disagree with the verdict it claims to explain
> ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
>
> **⚖ THE REASON EXISTS SO NOBODY HAS TO GUESS — HUMAN OR AI: *"otherwise a user would just have to
> guess what is wrong when they see greyed stuff, be it human or ai, and we try to avoid that."*** —
> [the no-guessing rule](../../../AGENTS.md#conduct) pointed at the CONSUMER: a greyed entry that
> does not say what is missing hands the player and the AI a question instead of an answer, the same defect a
> non-specific fact commits on the emit side
> ([a fact names the happening](../../spine/03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact)).
> ⇒ **So "unavailable" is never a complete verdict.** A candidate the player can act on says what to go get; one
> they cannot says so and stops occupying the list — stored at the gate, never re-derived by whoever displays it.
>
> **⚖ THE DISPOSITION IS PER ATOM KIND, AND THE KINDS STAY DISTINCT: *"per atom kind … then we can
> collapse as needed when we want to."*** A `requires` tree mixes kinds freely (`all: [TECH_X, BONUS_Y]`), so one
> disposition per clause is wrong for both halves — a missing BONUS is the "go get copper" case grey exists for,
> an unresearched TECH is not fetchable. The reason names the ATOM KIND that refused, never the clause as a
> whole. Carry kinds separately even where two share a disposition today: collapsing later is a cheap mapping
> edit; pre-merging is not reversible — the disposition is a MAPPING OVER the kind, never a property stored per
> entity.
> ⚑ Scale: 4,381 of 5,180 buildings name a `TECH_` atom in `requires.build`, 1,216 of them capped — this
> disposition decides the visible build list for thousands of entities, not a handful.
> ⚠ A `noneOf` names what it FORBIDS, so "the tree mentions a tech" is not "a tech refused it" — the kind comes
> from the atoms that actually caused the failure.
>
> **⚖ THE DISCRIMINATOR: CAN THE ASKER ACT ON IT?** That is the whole test, already stated above from the other
> side. Two calls it decides: a **TECH HIDES** (§2's multi-parent rule already keeps it out of the tree until it
> lands — greying it would double-list every future building), and **THE GROUND HIDES** — river, coast, hills,
> latitude, terrain, map category — because a city cannot acquire the tile it stands on.
> ⚑ The DEFAULT is GREY, including for an unnamed atom kind — §5's asymmetry applied to disposition: an extra
> greyed row costs a line, a wrong HIDE costs the asker the answer entirely.
> ⚠ Changing a kind's disposition only moves entries between HIDDEN and GREYED — LISTED is membership's own
> stored plane (§7.1) and never rides this mapping, which is what makes collapsing a kind later cheap.
>
> **⛔ AND WHEN SEVERAL CLAUSES FAIL, HIDE WINS.** Only ONE reason is stored, weighed over every
> top-level clause of BOTH timings (`requires.build` and `requires.operate`): any hiding reason wins outright,
> and only if none hides does the first greying one stand — a clause the asker cannot act on makes the whole
> entity unactionable. *(The defect this replaces: taking the FIRST failing clause let `all: [BONUS_COPPER,
> TECH_X]` grey on the bonus while the unresearched tech beside it — which should hide — sat exposed.)*
> ⚑ Machine-checked: `/computed/enabler/buildings`'s `greyedByReason` histogram must contain no reason
> `reasonHides` returns true for ([http-endpoints.md](../http-endpoints.md)). The tooltip renderer shares this same
> clause decomposition, one `all`-walk for both ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).

**The frontier is one shared choice set — UI *and* AI.** It is computed once per recompute; the UI greys from
it, and the AI's production decision iterates **only this small frontier** instead of scoring the whole entity
database. That consolidation — one recompute replacing dozens of scattered ad-hoc `canBuild` checks — is the
biggest systemic win.

> **⛔ A CONSUMER TAKES THE FRONTIER WHOLE — NOTHING FILTERS IT ON THE WAY OUT: *"if it does anything
> other than just give us the complete `canConstruct` list from enabler, then we are doing something wrong."***
> The frontier IS the narrowing, so a second filter at the consumer is never a refinement of it — it is a
> competing gate.
> ⛔ **And NARROWING IT FOR COST IS REFUSED OUTRIGHT: *"trying to do some fancy calculation to reduce
> that would hurt far more than it helps."*** A cleverer candidate filter trades a guaranteed correctness risk
> for a speculative saving, and §5's asymmetry already settles which way that goes: over-inclusion is SAFE, a
> MISS is the bug. The scoring cost of the frontier is the honest cost of the decision.
> ⚑ **The failure mode is not redundancy, it is CONTRADICTION — and the worked case is why this is a hard
> rule.** The building scorer re-asked the empire cap via `CvPlayer::isBuildingMaxedOut` over the offered set.
> That test adds `getMaxPlayerInstancesExtra()` to the cap, so it fires strictly LATER than `allowedOk` and
> could never catch anything the enabler had let through — its only reachable effect was on the buildings
> `allowedOk` deliberately WAIVES (`identity.noInstanceLimit`, the Palace-relocate case), where it dropped a
> candidate the enabler had chosen to offer. A duplicate gate does not merely cost cycles; it overrides the
> waiver the real gate exists to grant.
> ⚠ So an over-offer is diagnosed exactly as §3.2 already says — **a fact that is not being read, fixed at the
> ROUTE** — never by re-filtering at the consumer, which hides the gap instead of naming it
> ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).

---

