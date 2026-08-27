# 4c-rev. ⛔ TWO SCALES IN ONE TAG — the RevolutionDCM mods

> Part of the **[fixed-point-and-scales](../fixed-point-and-scales.md)** spec.

`fRevIdxNationalityMod` · `fRevIdxGoodReligionMod` · `fRevIdxBadReligionMod` · `fRevViolentMod`
(Civic + Trait). RevolutionDCM consumes them as **`1.0 + mod`** (`Revolution.py`; `violentMod` as a fraction of
the always-violent threshold), so the number carries a PERCENTAGE either way — but legacy authored it on **two
incompatible scales in the same tag**:

| population | form | example |
|---|---|---|
| the CIVICS + the BASE trait file | a RATIO | `0.5` = +50% |
| the **Thunderbrd trait module** | PERCENT POINTS | `40` = +40% — which legacy fed to `1.0 + mod` as **41×** |

⚖ **OWNER RULING: they all mean PERCENT POINTS.** The ratio population converts (×100) and the module's values
pass through — TB *"does not follow any spec, plan, or any kind of coherent structure … we make sure it does
now"*, so the incoherence is normalized AT THE CURATOR and never reaches the data.
⚠ **This is a stated BEHAVIOUR change** ([validation.md](../../validation.md)): the TB traits stop being ~11–41×
multipliers. It is currently INERT — no leaderhead authors a trait — and bites the moment the community does.
⛔ The two populations are **disjoint by an order of magnitude** (ratios reach 2.0, points start at 10), so the
boundary is exact over the authored data rather than a judgement re-made per value. A value landing in the gap
is a scale nobody has ruled on: `curate_common.ratio_to_percent` **RAISES** instead of guessing.
⚑ The reader divides once, at its own point of use (`RevUtils._revModRatio`), turning the whole percent back
into the ratio the index formulas want.
⚠ The three `CvPlayer` float accumulators these also feed (`m_fRevIdx*Mod`) have **no readers at all** — the
Python side reads `INFO.getRevolution(...)` directly — so they are inert here and belong to the
writerless-accumulator sweep, not to this scale question.

## 4c-bis. ⛔ CONVERT BY ARITHMETIC CLUSTER, NEVER BY GETTER

**A getter cannot be converted alone.** Its co-operands are on the same scale *by arithmetic necessity*: convert one
side and every mixing site needs a compensating `÷100` — manufacturing the very fudge factor that signals a
misplaced reduce. Convert the whole cluster and the mixing sites need **no change at all**, because the units
already cancel. This is why such a sweep keeps getting re-shoehorned: each getter looks independently convertible,
and none is.

**The acceptance gate per cluster: ZERO new fudge factors at the mixing sites.** If a conversion forces compensating
constants, the cluster boundary was drawn WRONG — stop and redraw it, never push through. (This is the second of
[AGENTS.md](../../../../AGENTS.md)'s drift detectors, stated as a conversion method.)

⚑ **A fudge factor points AT the unmigrated consumer.** In practice the constant is not a mis-drawn
boundary in the abstract — it is **legacy being forced into the new surface at an AI call site**: the multiplier
exists so a consumer that has not moved can keep reading a new-surface value in its old shape. ⛔ So when one turns
up, do not ask where the conversion belongs — ask **WHICH SIDE IS STILL LEGACY**, and re-point that side. The
constant then deletes itself, and it takes its scale question with it: a hand-rolled sum is the only thing that
ever needed to know its operands' units, so re-pointing DISSOLVES the question rather than answering it.
⚠ The failure mode is the opposite move — adding the multiplier and calling it done. That leaves the AI half on
legacy while the surface beneath it moves, which is exactly the half-migrated state
[build a new getter surface, never widen a legacy one](../../../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers) names.

⚑ **A cluster is defined by what MIXES, not by what looks similar.** Worked groupings: the yield/food/wellbeing
chain is one unit because food consumption subtracts angry population and health rate; commerce joins it at the
production→commerce term; gold/maintenance/upkeep joins commerce because gold IS a yield
([every modifiable number is a yield](../../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)); unit experience is genuinely
self-contained and so is the one safely parallelizable cluster.
⚠ **Same SHAPE is not same NATURE:** a `…Times100` on AI unit counts or plot strength carries *fractional
SizeMatters counts*, not a modifier channel — it is not a scale violation and must not be swept in with the yields.

**Sequencing within a cluster: set the mechanic up to spec FIRST, then wire the consumers.** Do not open
with a hundred consumer edits; build the value chain so it is internally ×100-consistent, then reduce at the readers.

