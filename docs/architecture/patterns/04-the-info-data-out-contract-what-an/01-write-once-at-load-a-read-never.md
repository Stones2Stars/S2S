# ⛔ WRITE-ONCE-AT-LOAD — A READ NEVER CREATES, AND AN UNANSWERABLE READ FAILS LOUD

> Part of the **[04-the-info-data-out-contract-what-an](../04-the-info-data-out-contract-what-an.md)** spec.

> Binding: [the info plane is write-once-at-load](#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud). The read-side twin of
> [exactly one JSON reader](../08-the-one-reader-the-load-pipeline.md#the-one-reader--the-load-pipeline-law) — that rule says there is exactly ONE writer; this one
> says everybody else is a reader, and says what a reader does when it cannot be served.

**Two access paths, and they are not interchangeable.** `InfoRepo::edit`/`editPtr` are **get-or-create**: they
resize the array and `new` a payload for whatever id they are handed. That is correct for the LOAD pipeline, which
is what they were written for, and they belong to exactly three callers — the one reader (`loadJson`), the reverse
pass, and the classification registry. **Every other caller is a READ and uses `atPtr` / `get`.** A read that
creates violates all three of the contract's clauses at once: it writes to the plane outside the one writer, it
does work on a path specified as a bare fetch ([cascade.md](../../../cascade.md)), and it answers
quietly where it should fail ([legacy must fail loud, never mask a cascade gap](../../../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)).

**What get-or-create actually answers with is a BLANK info, and that is why this is a hard rule rather than a
preference.** A blank is indistinguishable from a real one until something asks it a question — and then
`CvInfoBase::getType()` returns **NULL**, which crosses into the EXE's UI and into boost::python and is
dereferenced *there*. The failure therefore surfaces in someone else's frame, as an access violation at an address
in the EXE's image, with nothing left pointing back at the id that caused it. ⚠ **That address is the bait:** it
reads as an EXE defect, and an agent who stops there is chasing the closed binary for a NULL we supplied.

**⛔ Worse on an ALIASED repo, which is most of them.** The backing IS `GC.m_pa<X>Info`, and `getNum<X>Infos()`
returns that vector's `size()` — so creating on read **moves the registry's own bound**. Every bounded walk over
the registry then runs off into the entries the walk itself created; the observed signature is a walk that never
terminates and dies as an out-of-memory rather than as a bad id. ⚠ And the bounds assert that looks like it guards
this (`FASSERT_BOUNDS`) is **compiled out of `Release`/`FinalRelease`** (`fbuild.bff`), i.e. absent from every
build the game is actually played in — so a fail-loud here is a real function, never an assert.

**⚖ THE RULING: crash at the main menu because things are not loaded, rather than manually incrementing
the registry to limp past it.** An unanswerable read is a LOAD defect — some pass did not fill the slot — and the
only useful thing to do with it is name it, at the bad read, while the registry and the id are still known. It is
reported into `Exceptions.log` beside the handler's own entries and into `Loading.log` beside the `[READJSON]`
census that built the plane, then raised NONCONTINUABLE so the ordinary unhandled path writes the minidump and the
Python callstack.

**⛔ And the corollary that is easiest to get wrong: DO NOT DEFER THE READ TO MAKE THE FAILURE QUIET.** Moving a
read later — screens built on first use instead of at `earlyInit`, a plane consulted lazily "to get to the menu" —
initializes *nothing*. It relocates the failure from a named Python `AttributeError` at the menu to an access
violation deep in the interface, and buys the appearance of a load that got further. **A load that reaches further
by asking fewer questions has not got further.** Screens are therefore constructed eagerly, on the engine's entry
path, precisely so an incompletely stood-up info plane fails where the failure is legible
([python-load-sequence.md](../../../reference/python-load-sequence.md)).

**The failure this closes.** Asking each info type for its data through a DIFFERENT accessor is the same defect as
a hand-named scalar per channel ([every derived cache is one shape](../../../cascade/04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)): it cannot be
addressed uniformly, so every type needs bespoke read code, and the cascade ends up shaped by the info surface
instead of the other way round.

