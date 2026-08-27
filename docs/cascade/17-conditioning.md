# 3. Conditioning — re-applied when its own dependency moves (the dormancy model)

> Part of the **[cascade](../cascade.md)** spec.

A deposit may carry `enabled` / `disabled` / `per` ([json](../specs/json.md) §3.7, §3.9). A deposit's condition uses the
**same vocabulary** as the enabler's `requires` — the same `all`/`any`/`noneOf` tree over the same atoms and
predicates — so a conditioned deposit is, in essence, **a `requires`-shaped gate with an output attached**: the
enabler resolves that shape to *availability* ("can I?"), the modifier resolves the *same* shape to a *magnitude*
("how much?").

> **⛔ A condition is a PREDICATE, never a bespoke sub-scope MEMBER ([conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query)).**
> A deposit that applies only under some game state carries that state as a **predicate** in its `enabled`/`disabled`
> (or a `per`/`unit:` scaler, [json](../specs/json.md) §3.7), at the deposit's normal scope — `{family}.empire.percent` +
> `enabled:"IS_CAPITAL"`, never a bespoke `{family}.empire.capital.percent` member. Encoding the condition as a new
> member instead *changes the core structure* — the kraken way. Full ruling, the extensible predicate registry, and
> the golden-age exception (`empire.goldenAge` — a PERMANENT engine member-mirror): [json](../specs/json.md) §3.5.

**But they are SEPARATE FIELDS, not one condition** — because a thing can **require one condition yet gate its
effect (a buff *or* a nerf) on another**: a Forge `requires` connected iron to *operate*, but its +1 happiness is
`enabled` by *power*, not iron — and the magnitude can equally be negative (e.g. −production while polluted). So
the entity carries its `requires` once (whole-entity availability — the [enabler](../specs/enabler.md)'s job), and each
deposit carries its **own** `enabled`/`disabled` (does *this effect* apply). Same condition language, two
independent fields.

**⛔ NOTHING IS RE-CHECKED ON A RECOMPUTE, BECAUSE THERE IS NO RECOMPUTE.** A conditioned deposit is applied
`±value` by **the ATOM's own verdict crossing**, and a `per`-scaled one by `±value × Δcount` from **the COUNT's
own fact** — the two routed planes of the maintained sum
(§ THE MAINTAINED SUM, above;
[the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)). That re-application *is* the dormancy
model: a deposit whose `enabled` stops holding (or whose `disabled` starts) is withdrawn from the slot at that
instant — the source goes quiet without being removed.
⚑ **Both routes are reverse indices derived from the compiled deposit index** (atom → the deposits it gates,
count-key → the deposits it scales), so the cost is the deposits that atom or count actually touches — never a
walk of the scope's deposits asking each whether it cares, and never a sweep of the entity database.

> **⛔ THE TWO INDICES ARE KEYED THE SAME WAY AND ARE NOT INTERCHANGEABLE — asking the wrong one answers EMPTY,
> which is indistinguishable from "nothing is conditioned on this".** A condition atom's `type` interns into the
> **ATOM** index (`gatedByType`); a `per` scaler's token interns into the **COUNT** index (`gatedByToken`). Both
> are keyed by a plain string, so `"ERA"` is a legal key in either — and a route that reaches for the wrong one
> compiles, runs, reports nothing, and moves nothing.
> ⚑ **The tell is that a bare TOKEN can appear on both sides.** Most atoms are `INFOTYPE_NAME` ids and most
> count-keys are tokens, so the two key spaces look disjoint until a family uses a token as a THRESHOLD:
> `{type: "ERA", max: 1}` is a condition (atom index), while `per: {type: "ERA"}` would be a scaler (count index).
> ⇒ **When wiring a route, decide which QUESTION the deposits ask — "is this gate true?" or "how many?" — and
> take the matching index. Where a family is authored both ways, route BOTH.**
> ⚠ An empty list is silent by design (the route census reports nothing when the list size is zero), so a
> mis-keyed route leaves no trace at all. **Report the real list size, never a placeholder** — that count is the
> only thing that distinguishes a route with nothing to do from a route asking the wrong question.
>
> **⛔ AND A THRESHOLD IS NOT A PRESENCE CROSSING, so it cannot ride the ±1 atom route.** An `ERA`/`POPULATION`
> threshold has no held/not-held verdict for the as-if-held hypothetical to pin: when the counter moves, some
> deposits turn OFF and others turn ON in the same step. Such a gate is **RE-RESOLVED against the new state and
> moved by the DIFFERENCE** from what the slot already holds — which handles both directions in one pass and is
> idempotent if the fact is seen twice. The `±value` crossing form is only ever correct for a genuine presence
> atom.

- **`enabled` then `disabled`** — `enabled` is read first, `disabled` second; a `disabled` that holds overrides
  ([json](../specs/json.md) §3.9).
- **`per`** scales the deposit by a count — local at `city`/`plot`, via the [tally](../specs/tally.md) at cross-city scopes.
- Whole-entity availability (is this building active at all?) is the [enabler](../specs/enabler.md)'s `requires`, not a
  per-deposit condition: a dormant entity deposits nothing, so the modifier machine never special-cases it.
- **Age-gated deposits** — legacy `CommerceChangeDoubleTimes` ("double after N YEARS") is **not** a timer/stage
  but a SECOND deposit on the same slot with `enabled:{existedFor:{min:N}}` (no post-sum multiply). ⚠ The unit is GAME
  YEARS, not turns — the age is measured against the stored build YEAR, and that is what the tooltip has
  always promised ([json.md §3.5](../specs/json.md)).

  > **⚖ THE TURN BOUNDARY IS THE AGE GATE'S FACT, AND IT CARRIES EVERYTHING THE GATE NEEDS.** *"Start
  > turn should be an event, like anything else, that has turn number, which should give cascade what it needs
  > to figure it out."*
  > ⚑ **This is the one condition class whose dependency is ELAPSED TIME.** No source moves, no count moves and
  > no atom crosses when a build becomes due — so there is nothing else in the engine that could announce it,
  > and the age gate is the only member of the family that needs a cadence fact at all. The turn number is the
  > whole of the input; the deposit's own stored build year supplies the rest.
  > ⇒ **It rides the PLAYER-scoped turn-started fact**, whose cities are the ones whose builds can come due, and
  > it is a RE-BOOK by value difference rather than a `±1` crossing (an age gate has no held/not-held verdict to
  > pin, exactly as a threshold has none).
  > ⛔ **It is NOT the banned per-turn blanket** ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)):
  > the worklist is exactly the deposits the `existedFor` reverse index names, and a turn on which nothing came
  > due moves nothing. It satisfies the sanctioned event-triggered recalc test
  > (§ THE SANCTIONED EXCEPTION, above) — a genuine DOMAIN fact, a NON-LOCAL consequence the fact cannot
  > name, and no finer route to derive.
  >
  > **⛔ THE APPLY PATH MUST SET THE CARRIER SLOT, OR THE GATE ANSWERS FALSE EVERYWHERE.** `existedFor` asks about
  > the DEPOSITING entity, so it reads `sourceBuilding` off the eval ctx and answers FALSE when nothing set it
  > (§ THE SOURCE SLOTS, above — deliberately, since resolving it against
  > whichever entity a walk reached last is worse than declining). Every walk that resolves a building's entries
  > therefore sets it: the plane-A city apply and the re-book routes alike. ⚠ A walk that resolves a building's
  > entries WITHOUT setting it silently answers FALSE for this whole class, so every deposit gated on
  > `existedFor` goes missing — a divergence no missed emit explains.

---

