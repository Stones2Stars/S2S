# 1. One step: deposit DOWN, accumulate, read O(1)

> Part of the **[cascade](../cascade.md)** spec.

Where the [enabler](../specs/enabler.md) is two passes, the modifier is **one step**: each source drops its deposit
onto a target, the deposits **accumulate**, and the target reads an **O(1) summed total**. No source needs the
whole picture; order doesn't matter (sums are commutative).

Magnitudes flow **DOWN** the scope spine (`world → … → city → plot | unit`). An empire-scope deposit on a civic
rolls down to each of the player's cities; a city-scope deposit lands locally; a `plots`-target deposit lands on
each matching worked plot (§5). The target reads a combined value — it never re-walks the sources.

> **⚖ STORAGE SEMANTICS — the SCOPE PRINCIPLE.** Deposits **ACCUMULATE** in a package **AT THEIR OWN SCOPE** — one
> uniform package format (Σflat / Σpercent per channel, §2) held on each scope object (world / team / empire /
> city / plot), each package **event-MAINTAINED** at its own scope only: the fact names the source, the compiled
> index names that source's deposits, and applying them keeps the slot current with nothing marked or deferred
> ([the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)). The downward "roll" is
> realized **AT READ TIME**: the realized value is the trivial sum of the ~5 scope packages, with per-city gates
> (state-religion-in-city, coastal, connected, area membership) applied live at the combine. **A lower
> scope never STORES an upper scope's sums** — that would force downward fan-out and "break
> the principle of the cascade in the first place." LOAD is not a rebuild either: the reseed's in-read facts
> apply through the same path play uses.
> (Mechanics: § THE MAINTAINED SUM, below — the maintained-sum model + the two planes, only one of which is ever
> evaluated.)
>
> **⛔ THE ORIGIN RULE — THE PURE CASCADE DESIGN.** Which half of a package a scope ever fills is not
> incidental, it IS the model:
> - **YIELDS come from exactly three sources — PLOT, SPECIALISTS, and BUILDINGS (city).** Nothing else produces a
>   yield, so the flat/yield side exists at **plot** and **city** only.
> - **MODIFIERS come from everything BUT plot** — city, empire, team, world. The percent side exists at
>   every scope except plot.
>
> Plot and the upper scopes are mirror images (yield-only vs percent-only); **CITY is the one scope carrying
> both**. This is why every scope can hold the SAME package type while many stay half-empty — emptiness is a
> property of the origin rule, never a reason to omit a scope's package or to hand-shape a bespoke struct for it.
> *(The extended form of this rule — the three-package split within CITY itself, the four-provider law, and why
> a trade route is a provider with no package — lives in § THE READ PATH, below.)*
>
> **⚖ WHAT THE RULE GOVERNS — ONLY THE CHANNELS THAT ACTUALLY PRODUCE OUTPUT.** *"Only commerce yields
> and base yields actually produce output."* So the rule binds exactly those: the **base yields**
> (food / production / commerce) and the **commerce yields** (gold / research / culture / espionage). Their flats
> are authored at plot and city only, and none authors a percent at plot — that is the origin rule, in full.
>
> ⛔ **Every other family is NOT output, so it is not bound by it — and this is a CATEGORY difference, not a list
> of exceptions.** **Happiness is the worked case: it is a TRANSIENT STATE, not a yield that produces anything**
> — a condition the city is *in*, which changes how other things behave (growth, anger, food consumption) while
> producing no output of its own. Nothing is *made* by happiness. So "where output originates" simply has no
> claim on it, and wellbeing authoring **flats at EMPIRE and AREA** (the civic/tech/trait grants that roll down,
> §2b) is the model working, not an exception to it. The same holds for **plot-scope PERCENTS** — health's
> feature-fallout class (§2b), defense, the property plane.
>
> ⚖ **PROPERTIES are the honest IN-BETWEEN, and the test does not need to resolve them.** You *could*
> argue a property produces output — a value genuinely accumulates and propagates — but what it ultimately does
> is **affect a transient state**, so it sits between the two. That ambiguity costs nothing here: either way it
> is not an output-producing YIELD, so the origin rule does not bind it (the property plane authors plot-scope
> percents), and the property engine is **self-contained by design** — what happens inside it stays inside it
> ([engine.md](../reference/engine.md)), so no classification of it needs to leak outward. ⛔ Do not force
> properties to one side to make the taxonomy tidy; the in-between is the accurate answer.
> ⚠ **"Self-contained" scopes the engine's MATH, never its INPUTS.** Each property is a CHANNEL in this machine
> and the cascade owns which sources apply and what they sum to; the engine owns integrating that rate — decay,
> diffusion, the ordered solver passes ([property-audit.md](../plans/structural-cleanup/property-audit.md), the
> governing model). Reading this paragraph as "property sources are the engine's too" is what leaves the source
> side re-derived per turn.
>
> ⚠ **The word "yield" carries TWO senses, and conflating them is what makes this look like a contradiction.**
> [every modifiable number is a yield](#1-one-step-deposit-down-accumulate-read-o1)'s *"every modifiable number is a
> yield"* means **every such number is a CHANNEL in the one machine** — a statement about carriage. The origin
> rule's "yields" means **output-producing yields** — a statement about where output comes from. A family is
> classified by asking *"does this produce output?"*, never by which list it appears on.
>
> **How a non-output family's sides are enforced: BY THE DATA.** Each scope's channel set is **minted from the
> compiled deposits** (KEYS ONLY WHERE NEEDED, § THE READ PATH), so which sides a scope fills is answered at
> load, and a read of a side no source authored answers 0 with no storage existing anywhere. ⛔ **A read-side
> roll-up therefore never hand-gates a scope out of its chain** — the channel set is the gate; a hand-written one
> silently deletes an authored family's contribution, and with no runtime to catch it (the empire wellbeing flats
> are the case that bites: 558 authorings).
> **Consequence (owner requirement): every modifier/yield cache consolidates to ONE shape** — the per-family
> hand-named scalar members (`scGpBaseBld`, `scDefense`, `scMaintModCity`, …) collapse into the same
> Σflat/Σpercent-per-channel form the yields and commerce already use, so a new scope or channel is DATA rather
> than a new struct.

This is purely top-down: a condition *inside* a deposit (`enabled`/`per`) is a forward **read** of state, never
an upward cascade-walk. **The reverse view ("who references/modifies me") is derived once at load, never on a
hot path** — realized as reverse edge FAMILIES on the referenced info object itself, populated by the readJson
reverse pass (`EDGEF_RELATED` = the display/pedia candidate lists the tooltips iterate; `EDGEF_REQUIRED_BY` =
the enabler's requires-reverse-index). After load every info ALREADY CARRIES its reverse lookups; no consumer
builds its own scan or side index ([reverse lookups are populated once, at load](#1-one-step-deposit-down-accumulate-read-o1)).

**Three governing rules:** (a) **purely top-down** — sources deposit DOWN, targets read an O(1) accumulator; the
reverse index is cold-path only. (b) **tech-inflation is a downward DEPOSIT, not an upward gate** — a researched
tech deposits down onto everything below it (cheaper/better); the lower thing never reaches UP with a `hasTech`
gate. (c) **info DATA vs engine MACHINERY is a hard boundary** — the JSON carries only values + relationships;
the producers, evaluators, and tally that consume them are engine-side, so authoring stays declarative.

**Every modifiable number is a yield.** ANY number game mechanics modify — base yields, commerce, free XP, free
specialists, property magnitudes, combat percents, heal rates — is a channel in this ONE machine, carried in the
ONE uniform package format (Σflat / Σpercent per channel per scope; the unit is part of the slot key). A number
still computed by a legacy ad-hoc path outside the machine is a shortcut to fold in
([every modifiable number is a yield](#1-one-step-deposit-down-accumulate-read-o1)).

> ⚠ **Two shapes get mistaken for exemptions from that last sentence; neither is one.** A **PARTIAL leg** (a
> pre-improvement / nature-only yield) is still yield compute — it is a SEGMENT of the scope's own package
> (§ THE CONTEXTS, below), never a per-call walk kept outside the machine because it answers a narrower question.
> A **WHAT-IF** (*"what would this improvement yield here"*) is yield compute too: the what-if plane is a READ of
> this machine (the `expected*` valuations, [patterns.md](../architecture/patterns.md)), not licence for a consumer
> to keep its own yield arithmetic. ⇒ The test is the QUESTION, never the caller or the name — a `calculate*` on
> a game object that sums info getters per read is by construction the legacy path this replaces.

**The output-seam.** Where the engine performs placement/application, the machine owns the two ends and the engine
the middle: (1) authored INPUTS are source-centric deposits (a package); (2) placement/application is engine
infrastructure (free-specialist assignment; the golden-age plot-base-yield-threshold "+1"), not modeled; (3) the
OUTPUT yields flow back as a package, consumed exactly like plot yields. Free specialists (amount + forced type →
deposits; engine places; output yields = package) and golden age (length + grant = JSON inputs; plot-threshold
effect = engine middle; extra plot yield = output package) are the exemplars.

---

