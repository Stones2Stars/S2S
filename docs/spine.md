# The event spine and its observability

> A **core spec + reference.** The event spine is **where every consumer gets its facts** — one `emit`, fanned
> out by KIND to every registered consumer. It is *not* logging; logging is one consumer of it, alongside grants,
> cache-invalidation, the trigger engine, and the out-of-process replay. *(The in-engine **tally** is NOT a
> consumer — it reads the object-owned counts directly; [tally.md](specs/tally.md).)*
>
> The **goal** this surface serves is *Orwellian* total-surveillance observability — reconstruct full game state
> from the wire, **never the screen** (§7). The load-bearing rationale is **map-before-delete**: you cannot safely
> delete a legacy maintainer you cannot fully observe, so without total observability the cascade
> ([enabler](specs/enabler.md)/[modifier](cascade.md)/[tally](specs/tally.md)) cannot prove it replicates the
> legacy machinery it replaces — so it cannot safely replace it. How the cascade is **verified live** against this
> surface (endpoint manifestation + turn time) is [validation.md](specs/validation.md).

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.**
Read the parts your work touches END TO END; the count that applies is something you FIND, not something
you decide ([AGENTS.md](../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[the primitive and the kind](spine/01-the-primitive-and-the-kind.md)** | 1. The primitive and the KIND firewall |
| **[the ieventconsumer contract and](spine/02-the-ieventconsumer-contract-and.md)** | 2. The `IEventConsumer` contract and the C++ shape |
| **[the domain emit surface every fact](spine/03-the-domain-emit-surface-every-fact.md)** | 3. The DOMAIN emit surface — every fact names a happening |
| **[consumers registration order and](spine/04-consumers-registration-order-and.md)** | 4. Consumers, registration order, and `CvEventReporter` |
| **[the load reseed](spine/05-the-load-reseed.md)** | 5. The load reseed |
| **[the received line auditing the](spine/06-the-received-line-auditing-the.md)** | 6. THE RECEIVED LINE — auditing the whole event flow live |
| **[what to log the orwell bar the](spine/07-what-to-log-the-orwell-bar-the.md)** | 7. What to log — the Orwell bar, the scale, and the three hook shapes |
| **[the live surfaces gates the tag](spine/08-the-live-surfaces-gates-the-tag.md)** | 8. The live surfaces — gates, the tag registry, the server, and the files |
| **[reading the live surface the rules](spine/09-reading-the-live-surface-the-rules.md)** | 9. Reading the live surface — the rules |

## See also

- [tally.md](specs/tally.md) — the read-only count accessor (reads object-owned counts; NOT a spine consumer). The
  KIND firewall (`DOMAIN` vs `SAVELOAD`/`DIAGNOSTIC`/`TRACE`) is still load-bearing for the synced-vs-unsynced split
  that logging + the offline replay ride.
- [validation.md](specs/validation.md) — the live-verification discipline that *uses* this observability to prove a
  maintainer before it's cut.
- [http-endpoints.md](specs/http-endpoints.md) — the HTTP transport (`/`, `/events`, the mailbox), its standing
  invariants, and why the route surface stays sparse.
- [architecture/patterns.md](architecture/patterns.md) — the `IEventConsumer` interface pattern.
