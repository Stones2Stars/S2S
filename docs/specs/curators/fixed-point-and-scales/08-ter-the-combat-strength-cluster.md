# 4c-ter. The COMBAT-STRENGTH cluster — the target shape

> Part of the **[fixed-point-and-scales](../fixed-point-and-scales.md)** spec.

⚖ **"The strength legs should not need to reduce to human until actually SHOWN IN THE UI."** Combat strength is an
AMOUNT, so it is ×100 all the way through the calculation and reduces ONLY at the OUT boundary.

**What that fixes, and why the current shape is worse than a stray divide.** Today `baseCombatStr()` returns a
**different SCALE depending on a GAME OPTION** — ×100 under `GAMEOPTION_COMBAT_SIZE_MATTERS`, human without it —
and `baseCombatStrNonGranular()` exists only to undo that. A read whose scale depends on live game state cannot be
reasoned about at any call site, and it means every consumer is already wrong under one option or the other.

**The target:**
- **Strength is ×100 internally, ALWAYS — independent of any game option.** `baseCombatStrPreCheck` keeps both
  legs ×100 (no reduce on the base seed, none on the resolved delta) and drops the `*= 100` that currently
  re-manufactures the scale under SIZE_MATTERS; SizeMatters then simply scales an already-×100 value.
- **ONE human accessor at the boundary**, and it is the ONLY `÷100` — for the UI, the `Cy*` bindings and the WBS
  scenario field. `baseCombatStrNonGranular` becomes that accessor unconditionally (and should be NAMED for what
  it is: the human read, not "non-granular").
- **`m_iBaseCombat` goes ×100 with the cluster.** It is serialized, so this is a deliberate save-semantics change
  — and the WorldBuilder boundary converts, because WB edits in human units.

⛔ **It converts as ONE atomic pass, never piecemeal.** The internal reads (AI + engine) are the large majority
and stay ×100 untouched; the boundary is a handful of sites. **The audit is NOT "every call site" — it is every
site that MIXES strength with a human literal or a differently-scaled quantity.** A comparison of two strengths is
scale-invariant and needs no edit; `> 5` or `+ someHumanCount` does. ⚠ A changed scale compiles silently on the
same `int`, so the compiler is NOT the census here ([AGENTS.md](../../../../AGENTS.md) drift detectors) — the mapped
mixing-site list is, and a miss surfaces only as wrong combat numbers at runtime.

## 4d. ⛔ THE EDGE — where a scale error can occur at all, and therefore what an audit checks

**A scale error cannot happen inside the cascade.** ×100 is native EVERYWHERE within it (§1), so every magnitude
there is ×100 by construction and any two operands already agree. **A scale error is only possible where a value
CROSSES A BOUNDARY** — which makes the audit an ENUMERATION OF BOUNDARIES, never a sweep over every multiply:

1. the **IN** boundary — readJson's single human→×100 conversion;
2. the **OUT** boundary — a reader's `÷100` at the point of use;
3. a **sanctioned engine INPUT** — a value the cascade folds in rather than computes.

⚑ **The trade-route fold is THE EDGE** — the exemplar of class 3 and the reason class 3 exists.
`tradeYield` is the ONE sanctioned live-yield input ([modifier.md §2a](../../../cascade.md)): the cascade cannot
re-derive the trade NETWORK, so that calculation stays engine-owned ([north-star.md](../../../architecture/north-star.md)
KEEP — it is none of the four systems' job) and its value is FOLDED IN. That is precisely why the scales differ
there, and why **the conversion belongs THERE: an edge converts**, exactly as the IN and OUT boundaries do.

**How to audit, since the naming ruling removed the marker.** Every value is ×100 and NO name says so, so a
grep for a `100` token returns nothing and proves nothing. Instead, at each boundary site check the two operands
against the **DECLARED scale of the surface each came from** (the calc functions' documented inputs/outputs, the
package slot reads, the compiled sums) — never against a name. ⛔ Where a boundary function mixes a plain engine
percent with a ×100 sum, fix it STRUCTURALLY — have the function lift and take both down together — so a caller
passes what it holds and cannot get the scale wrong; a comment warning the caller is not a fix.
⚠ **And never multiply two ×100 values without rescaling** — the product is ×10000, so the `÷100` belongs at the
multiply.

> **⛔ THE OUT BOUNDARY IS DECIDED PER UNIT, AND A FAMILY-WIDE `÷100` RULE IS ITSELF A DEFECT.** Within one
> family the kinds differ: `infoKindUnit` makes some PERCENT (unscaled — a re-point is 1:1) and some FLAT
> (×100 — the reader reduces). So "this getter set is ×100, reduce at the reader" is never a safe blanket; it
> zeroes every percent it touches. **Ask the KIND's unit, never the family's or the getter's name.**
>
> **⛔ MOVEMENT IS ALREADY A PER-100 VALUE — `MOVE_DENOMINATOR` is its fixed point, and always was.**
> That is why routes author 5–100: they are denominator units expressing PART STEPS. So the cascade's ×100
> sits on top of a denominator the mechanic already had, and the family slot holds **two scales, each ×100'd**
> — terrain/feature as whole moves (1–6), routes as denominator units (5–100).
> ⛔ Do NOT "finish" this by carrying ×100 deeper into the resolver: that compounds the double-scaling instead
> of resolving it. What has to be decided FIRST is which single denominator movement speaks in, and that is a
> CURATOR question (does terrain author denominator units too?), never a consumer sweep.
>
> ⚑ **The worked case, both ways round, on ONE family (handicap).** `DIPLOMACY_DECLARE_WAR` is a percent, so a
> blanket `÷100` would have turned a 90% AI war probability into **0** — the difficulty setting silently
> switched off. `BARBARIANS_DEFENDERS` is a flat, and reading it raw returned the authored **8 as 800**: a loop
> bound spawning 800 initial defenders, and a `getNumUnits() >= 800` test that could never fire, leaving an
> entire AI branch dead.
> ⚠ **Neither failure crashes, and that is the point** — a mis-scaled CONFIG scaler produces a game that runs
> perfectly while playing by different numbers, so it survives every smoke test. This class is found by
> checking the unit at the boundary, never by observing that the build is green.

## 5. Verification — the math proves the scales, not manual JSON review

The owner cannot eyeball thousands of JSONs, so a mis-scaled field is found by the MATH: the effective value the
authored JSON produces is observed live on the `/computed` decomposition censuses, on a real save
([done = observable in the running game](../../validation.md)). **Residual divergence localises
the next mis-scaled field** → fix the curator → regenerate → re-check. Exact parity is the bar — 0 in-scope mismatches; a residual divergence is a data-collection gap (a still-mis-scaled field), never a formula difference ([the completeness+attribution bar](../../validation.md#the-observation-surface)).

⚖ **CALIBRATION — a scale error BREAKS BALANCE AND BEHAVIOUR, NOT THE GAME.** *"It's obvious when numbers
are out of whack in a new game, and it does not actually break the game — it just breaks balance."* A wrong scale
(and the fudge factor that hides one) costs no crash and no corrupt save; it shows up on a fresh start.
⚠ **But do not read "just balance" as "just tuning."** Integer truncation does not mis-tune a mechanic, it SWITCHES
IT OFF: the AI declaring war on no difficulty, property decay never running, starting gold landing at zero. The
mechanic is absent, not weak — which is a behaviour break wearing a balance costume, and it is why these are worth
finding rather than living with.
⚑ **Read this as licence to CONVERT, not as licence to guess.** It means a well-reasoned conversion should be
made and observed rather than parked behind more analysis — over-caution here costs more than a wrong scale does,
because a mis-scaled field sitting unconverted is just as wrong and nobody is looking at it. The no-guessing rule
is unchanged: establish the unit from `infoKindUnit` + the authored data, then convert.
⛔ The exceptions that are NOT cheap, and still want care before landing: anything that changes what a SAVE means
(a serialized member's scale), and anything feeding the synchronized RNG
([the synchronized RNG is shared state](../../../reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws)) — those fail
silently or desync rather than looking odd.

⚑ **AND THE AI DECISION LOG IS A SCALE INSTRUMENT: a decision that NEVER VARIES is a truncated-to-zero
input.** Integer division is what makes a mis-scaled value fail this way — a percent reduced by 100 lands on 0,
and the branch it gates then resolves the same way forever. Because every AI decision is logged
([spine.md](../../../spine.md)), that shows up as a decision going one way 100% of the time, which is far easier to
spot than a number being quietly wrong.
⛔ So read an always-the-same AI decision as a SCALE SUSPECT first, before theorising about the AI logic — a
rand-versus-threshold that never fires, a gate that never opens, a modifier that never applies.
*(Worked: `rand(100) < declareWar.ai.percent / 100` truncated 50–100 to 0, so the AI declared war on NO
difficulty. The decision log would have shown that branch never taken; the code read as reasonable.)*

## See also
- This doc is the permanent home of the ×100 fixed-point model and the curator-owns-descale rule (§1 above).
- [modifier.md](../../../cascade.md) — the §2 arithmetic that consumes ×100 values.
