# Where it lands — THE `hideAndSeek` BLOCK, never inside `vision`

> Part of the **[vision](../vision.md)** spec.

⛔ **HIDE AND SEEK IS ITS OWN BLOCK AND ITS OWN EVALUATION.** `vision` answers ONE question — *how far
do you see* — and stops there. Whether a unit standing inside that reach is PERCEIVED is a graduated CONTEST
between how well it hides and how well the seeker detects, which is a different mechanic with a different
evaluation. ⚑ **The separation is the deliverable, not tidiness: the legacy engine's hide-and-seek evaluation
bled into its classic-visibility evaluation for years**, so the two must not be expressed in one family
where the same bleed can re-form. The contest data therefore lives in **`hideAndSeek`**, the option-gated block
([json.md §9](../json.md): a dedicated system's data lives in its own block, and the module is ON iff that block
exists and is non-empty), and `vision` keeps only the budget — strength, `elevation`, `obstruction`.

> **⚖ THE CONTEST'S CARRIERS TODAY ARE UNITS, PROMOTIONS AND UNIT-COMBAT CLASSES: no building shows
> hidden units, classically or in the contest — detection travels with seeker UNITS ("various kinds of
> dogs").** The absence is INHERITED DESIGN, not a data accident: vanilla Civ4 deliberately had no detection
> on buildings at all, which is why no building surface down the whole lineage ever carried one. So the block
> folds onto the UNIT's resolved plane and nowhere else today.
> ⚖ **A BUILDING-FED CITY PLANE IS UN-KILLED FORWARD INTENT, not a dead idea: a scenario is wanted
> where buildings do it — a NEW mechanic, since legacy never had one — so the city must stay PREPARED for
> it.** When data authors a building `hideAndSeek` block, the city gains its own cached fold over its
> OPERATING buildings, marked on the building facts (the unit block's shape one scope over). ⛔ Until that
> data exists nothing is built (a shape with zero authorings is an example, not live data —
> [triggers.md](../triggers.md)); a building authoring the block today surfaces on the readJson
> unconsumed-section census, which is the fail-loud signal that the wiring's moment has come.

> **⛔ VISIBILITY ITSELF IS NOT A SKILL, AND IS NOT MODELLED AS ONE BEYOND FILTERING: *"if visibility was
> a skill it would only be absolute values, and hide and seek has gradient values."*** A skill is a pure boolean
> ENABLER ([json.md §8](../json.md)) — it carries no value — so it can express WHICH method is in play and never HOW
> WELL. The contest is graduated on both sides, so the strength lives in `concealment` / `detection` and the skill
> plane is used **only as the membership filter**: which method a hider hides by, which method a seeker answers.
> ⛔ So do not model a visibility LEVEL as skills (a ladder of `camouflage1/2/3`, a per-tier key) — that re-encodes
> a magnitude in a plane that cannot hold one, which is exactly what the retired per-type intensity tables did
> ([superseded-ideas #35](../../architecture/superseded-ideas.md)). ⚑ And it is why the membership test is the SKILL
> while the contest reads the magnitudes beside it — the two are not alternatives, they are the filter and the
> value.
>
> **⚖ ⇒ AND THEREFORE THE VISIBILITY AND HIDING VALUES ARE MODELLED THE SAME WAY NORMAL VISION IS, JUST WITH
> DIFFERENT PARAMETERS.** That is the conclusion the gradient forces, not a separate preference: §1a's
> scale and §2's budget-against-cost shape already express a graduated quantity correctly, so `concealment` and
> `detection` are the same KIND of number as `sight` and `obstruction` — same ×100 fixed point, same
> one-step-is-100 denominator, differing only in the parameters they carry.
> ⛔ **So there is no bespoke intensity scale here, and none is to be invented.** A per-method 1…26 ladder is the
> legacy shape that died ([superseded-ideas #35](../../architecture/superseded-ideas.md)); a fresh one would be the
> same mistake re-authored.
> ⚠ **What this does NOT reopen is the REACH** — detection still gets none of its own (§4 below), and "modelled
> like vision" is a statement about how the VALUES behave, never a licence to grow the second range system
> `visibilityIntensityRange` was retired for. The contest runs on the plot the §2 budget already granted.

⚖ **THE METHOD IS A SKILL, NOT A TAG.** The operative test is *can a promotion grant it?*
([json.md §8](../json.md)) — and it plainly can: **optical camouflage** is exactly a late-game promotion INTO a
hiding method. So the method is a [skill](../skills.md), which fits on both counts: promotion-grantable, and a pure
boolean enabler carrying no value — correct, because the LEVEL is the `concealment` magnitude beside it.
⛔ It is NOT a [tag](../tags.md), and the reason generalizes: a tag says what a unit **IS**, while `camouflage` /
`size` / `political` say how it **HIDES**. **`submarine` is the case that proves the split** — it is a genuine
identity tag AND carries the method skill, because a surfaced submarine is not hidden: *"submarine does not need
to be hidden/invisible, it just mostly is"*.
⚑ **The tag reading also DESTROYED authored data, which is what settles it.** Tags are not promotion-grantable,
so a method named by a PROMOTION had nowhere to land and was dropped on the floor — and **73 promotions author
one** (`CAMOUFLAGE` 40 · `DISGUISED` 21 · `NAVAL_DISGUISE` 16 · `POLITICAL` 15 · `INVISIBLE` 10 · `SIZE` 9 ·
`CLOAKED` 8 · `SUBMARINE` 3), the cloaking line among them. A carrier that cannot hold what the data authors is
the wrong carrier.

```jsonc
// the hider: the METHOD is a skill (promotion-grantable), the LEVEL is a magnitude
"skills":      [ "camouflage" ],
"hideAndSeek": { "concealment": { "flat": 300 } }

// the seeker: sonar answers submarines well and camouflage poorly
"hideAndSeek": { "detection": [ { "value": 500, "unit": "HAS_SUBMARINE" },
                                { "value": 200, "unit": "HAS_CAMOUFLAGE" } ] }
```

A skill is something a unit **HAS**, so the qualifier reads `HAS_<SKILL>` ([json.md §3.5](../json.md): `IS_*` is
what the target IS, `HAS_*` is what it has) — the same `{unit: …}` qualifier cargo uses, pointed at the skill
plane rather than the tag plane.

`perceived ⟺ reachable ∧ detection(against that method) ≥ concealment`

⛔ **Detection gets NO reach of its own.** Reach is the §2 budget, already computed; the contest only
ever runs on a plot that budget already granted. That is what retires `visibilityIntensityRange` — a second
range system running beside vision's, with nothing keeping the two in step. Negatives need no mechanism either:
the block's entries sum, so counter-detection is a negative deposit.

