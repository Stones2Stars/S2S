# ⛔ THE EVAL CTX CARRIES CONTEXTS, NOT GAME OBJECTS — the contract must be STRUCTURAL

> Part of the **[cascade](../cascade.md)** spec.

**"Otherwise we can just pass the full player, city, and whatever other objects again, without any
distinguishing."** That is the whole test, and it is a CONTRACT, not a prohibition: if the evaluation context
holds a `CvCity*`/`CvPlayer*`, then "the reader goes through the context" is enforced only by reviewer memory —
the god-object is right there, and reaching past the context is one `->` away (a derived
`&ctx.city->getCityContext()` is the tell: the ctx never held a context at all). The isolation must be
**unsayable to violate**, exactly as [patterns.md](../architecture/patterns.md) states the info DATA-OUT contract: there is no
member to reach through.

So `CvCascadeEvalCtx` carries **`const CityContext*` / `const EmpireContext*` / `const PlotContext*`** — never
the bound game objects. Consequences, each already implied by the model above:

- **TEAM facts route through `EmpireContext`** (team is deliberately not a context; team-held techs are read
  through the player) — the ctx carries no `CvTeam*`.
- **`CvPlotGroup` STAYS a first-class ctx member** — it is the reserved explicit traded-bonus source (§ the
  read, below), not a scope whose state a context owns.
- **`CvUnit` stays raw FOR NOW** — units are the deliberate FUTURE role-specific scope; when that context
  lands it replaces the pointer, and until then this is the one acknowledged hole, not a licence for others.
- **The enabler's precomputed sets** (operating/active/obsolete buildings, vicinity-provided bonuses) stay ctx
  members: they are the ENABLER's derived output fed to the evaluator, never per-scope live state.
- **⚖ THE SOURCE SLOTS — a predicate about the CARRIER needs the carrier named, because an entry cannot name
  itself.** Neither a compiled entry nor an info knows its own engine id, so a condition asking about the
  DEPOSITING entity rather than the target (`existedFor` — how long has THIS building stood) has no other way
  to ask. The ctx therefore carries a slot per such axis — `religion` (the §3.7 counted-kind filter),
  `sourceBuilding`, and `civic` (the `{CIVIC_CATEGORY}` predicate's carrier, set by the civic-upkeep resolve) —
  **set per-iteration by the walk that knows the id, on a LOCAL COPY of the ctx, and -1
  everywhere else**; the shared ctx is never mutated. ⛔ -1 means "no carrier in hand" and a source-predicate
  must answer FALSE there: a scope-wide read that never set it would otherwise resolve against whichever
  entity the walk happened to reach last.
  ⚠ **A source slot is only meaningful where the carrier is SINGULAR.** The city fold resolves one building at
  a time, so it sets one; the EMPIRE fold walks a building the player may hold N copies of, each acquired at a
  different moment, so the question has no single answer and the slot stays unset by design. That is a
  structural limit, not an unwired leg — do not "complete" it by setting the slot there.
- **A context that cannot answer a needed fact is a CONTEXT GAP to close** (add the forward), never a reason to
  re-add an object pointer. That is the forcing function the structural form buys.

**⚖ THE HAVE AXIS LIVES IN THE CONTEXTS.** What a scope POSSESSES — the city's buildings-present /
religions / corporations / bonuses, the empire's civics / traits / heritages, the team-held techs (read through
the player's team — team is deliberately not a context) — is read through that scope's context, never by an
ad-hoc reach into the game object. The STORES-vs-FORWARDS discipline above is unchanged: possession state the
object already owns O(1) is FORWARDED, and only a homeless aggregate is stored (`policies` is the realized
exemplar). The context is the RESPONSIBILITY home — the one place every reader (the evaluator's atoms, the
enabler's gates, the `expected*` valuations) goes for HAVE. The enabler's DERIVED sets (the domain vectors, the
operating-building set) remain enabler-owned ([enabler.md §7](../specs/enabler.md)); the contexts serve the raw
possession facts those machines gate against.

**⚖ IF IT IS CURRENT STATE, IT IS THE CONTEXT'S — there is no third home.** A value that looks like it
needs a new category almost always just IS current state, and current state has one home. The worked case: the
city's **thresholds** (growth / culture / great-people — what is REQUIRED right now, moving with population,
gamespeed and era), its **turns-left** projections, and its cross-city **ranks** are none of them a new kind of
read — they are this city's state now, so they are asked of the context like every other state fact. The
STORES-vs-FORWARDS split then decides each one on its own merits (a value the object already computes O(1) is
FORWARDED; only a homeless aggregate is stored) — the classification is never "invent a shape for it".
⚠ **Ranks are the case that STRESSES the rule**, and stressing it is not breaking it: a rank is a comparison
ACROSS cities, so no single city owns the answer and a stored rank would need maintaining on every sibling's
change. Read it at the scope that can actually answer it — the player — rather than bending the city's context
around it.

### COUNTS, not objects — "how many, not which"

An aggregate holds **counts keyed by id**, never the objects themselves. A building cares HOW MANY river plots /
vicinity bonuses it has, never WHICH. So a `plots`-target (or keyed) deposit's output is `flat × count(id)`, and a
gate is `has(id)` (count > 0).

> **⚖ THAT PRODUCT IS A YIELD, AND THE DICTIONARY IS ONE OF ITS TWO OPERANDS.** `Δ(flat × count) = flat ×
> Δcount` is exact — this is plane **B** of the maintained sum
> (§ THE MAINTAINED SUM, above), and it is why a count fact is emitted at
> all: an `add(id, ±1)` IS a yield delta, never a re-derivation.
> ⛔ **So a dictionary is not merely a gate store beside the value plane — it IS part of the value plane.** A
> count with no route leaves every deposit scaled on it permanently wrong, exactly as a missing source fact does. The uniform keyed dictionary is **`ContextDict`** (`id → count`, read `has`/`count`,
maintained `add(id, ±1)`, zeroed `clear()` at owner reset — **there is deliberately no `set`**, which would
overwrite a refcount) — ONE kind, shared by every context, so the read is uniform and each family's key set is
OPEN (a new predicate/type is a new key, never a reshape). It is also the destination the mark-and-recompute
component retires ONTO ([ContextDict replaces CvDerivedCache](06-spatial-and-contextdict.md#-cvderivedcache-is-replaced-by-contextdict--virtually-everywhere)). `plotAttrs` keys on the `CASC_PRED_*` HAS_/IS_ plot
predicate ids; `policies` on the `POLICY_*` classification ids.

Non-dictionary scalars stay plain: population is an `int` forwarded from `CvCity::m_iPopulation`; state religion is a
**single enum**, not a dictionary (there is exactly one). ⛔ **Power is NOT one of them** — it is an amenity
DICTIONARY count (`CityContext::amenityCount(CLS_AMENITY_PROVIDES_POWER)`), which is why a removal DECREMENTS and a
city with two plants stays lit. A **volumetric** power model would not widen a scalar either: it would move power
from a classification key to a modifier-family CHANNEL (§ the airlift worked case), so there is nothing here to
future-proof.

