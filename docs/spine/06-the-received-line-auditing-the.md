# 6. THE RECEIVED LINE — auditing the whole event flow live

> Part of the **[spine](../spine.md)** spec.

> *"We can literally audit the entire eventflow with the `/events` endpoint live — all we have to do is have a
> 'received' event on the other side that is purely for logging."*

**A consumer announces that it ACTED on a fact.** Emitted lines and received lines then stream side by side on
`/events`, and the audit is a diff: **a DOMAIN fact with no matching received line names a MISSING CONSUMER
ROUTE** — the third gap form in [an event gap is closed the moment it is found](03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact),
and the only one with no other observable signature. The first two forms are visible today (a missing emit leaves
the stream silent, a missing field leaves the payload short); this one is not, because the fact goes out
perfectly and is simply dropped on the floor. The worked case is `SEVT_PROPERTY_ADDED / _REMOVED`, which fires from
the `CvProperties` choke points into a consumer set that carries no case for it — a defect that took a code audit
to find and that a missing received row would have named at a glance.

⚑ **It is load-bearing under the maintained-sum model specifically:** the consumer route IS the maintenance
([cascade.md](../cascade.md)), so this audits the correctness mechanism itself
rather than the number that falls out of it.

> **⚖ A "JOB DONE" ANNOUNCEMENT IS A RECEIVED LINE, AND IT IS ALWAYS `DIAGNOSTIC`.** *"`SEVT_CITY_BUILDING_PROCESSED`
> is a 'I have completed my job' event, if anything, and should purely be logging."*
> ⛔ **THE TEST: does the fact say WHAT THE STATE IS, or WHAT SOME CODE DID?** A completion notice is the second,
> so it is `DIAGNOSTIC` and **NO CONSUMER MAY BUILD STATE FROM IT** — deriving held state from an announcement
> that an apply ran is the failure this kind exists to make unsayable.
> ⚑ **The STATE a completion notice sits next to is a separate DOMAIN fact and gets its own id.** A building's
> operate crossing (`ACTIVATED` / `DORMANTED`) is what the deposit, amenity and free-promotion consumers read;
> the processed notice announces only that the apply ran, and nothing folds on it. ⚠ Letting one id carry both
> means neither consumer can tell which arrived — a completion notice and a state change are not two readings of
> one event, they are two events.
> ⛔ The repair for such a conflation is always ADDITIVE, never a deletion
> ([an event gap is closed the moment it is found](03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact)): mint the state fact, leave
> the notice a notice, re-point the folds.
> ⛔ **Do NOT suppress an EMIT to fix a CONSUMER.** Conflating "this fact fired" with "this consumer should act"
> is what produced both the plot-mark fan and this shared id. **Emit every distinct fact, always; decide handling
> per consumer, separately.**

⛔ **THE KIND IS `DIAGNOSTIC`, NEVER `DOMAIN` — and this is what keeps it from becoming the killed verifier.**
The firewall (§1) already defines `DIAGNOSTIC` as *"code ran (a function entered, a decision re-evaluated) …
logging only — never counted, never gates"*, which is exactly what a received line is, so it needs no new
machinery.
⚠ The near-miss to recognise is [superseded-ideas #19](../architecture/superseded-ideas.md), the gated in-DLL
cache verifier, which was killed for putting a **divergence** on the spine: an event is an invitation to a
consumer, and the next agent's consumer "handles" a value known to be wrong by CORRECTING it — so the shape
itself licensed self-heal. **A received line announces THAT CODE RAN, never a VERDICT ABOUT A VALUE**, so it
contains nothing to correct. Emitting it as DOMAIN would make it a synced authoritative fact the machine
consumers may read, which is the shape that grows the self-healer, and would double the unconditional stream.
⚑ As DIAGNOSTIC it rides `gStreamLogLevel` — decoupled from the file gate — so it costs nothing until it is
turned on, and stays off the bounded SSE slot budget during ordinary play ([http-endpoints.md](../specs/http-endpoints.md)).
⛔ It reports NO judgement and accumulates NO counter behind a route: it is a line, on the one surface that
already exists (the server SERVES, it does not ACCUMULATE — a fact that is on neither surface is EMITTED, never
given a side-counter).

**Events are FACTS, not causal steps.** "This building is here", "this tech is held" — order-independent,
prerequisite-free. Prerequisites are evaluated ONLY by the enabler (`canConstruct`/`canTrain`/`canResearch` — the
"*can* I?" question), never by a has-been-done fact; so the emit stream carries no ordering and no prereq logic.
Corollary — **yield is a computed RESULT, never an event**: emit the CAUSES (improvement/terrain/feature/route
changed), and a consumer computes the yield downstream.

