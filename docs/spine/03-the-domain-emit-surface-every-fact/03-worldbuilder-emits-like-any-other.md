# ⛔ WORLDBUILDER EMITS LIKE ANY OTHER PATH — no WB special case, anywhere

> Part of the **[03-the-domain-emit-surface-every-fact](../03-the-domain-emit-surface-every-fact.md)** spec.

**⚑ WHY it is categorically different: *"WorldBuilder can add or remove anything, at will."*** Every
other surface reaches state through a genuine acquisition — a building is CONSTRUCTED, a unit is TRAINED, a tech
is RESEARCHED — and this whole spine is built on that: one fact, emitted at the genuine mutation choke point. WB
instead mutates arbitrary state directly, so it can violate every invariant the model rests on — an entity
appearing with no acquisition, vanishing with no death, changing owner with no conquest. **A WB edit that changes
state silently leaves every cache, context and enabler set wrong, exactly as a missing emit does**; WB is simply
the surface that can produce that condition deliberately, on any field, in one click.

⛔ **So WB adding or removing anything EMITS, exactly as the normal path does.** Do NOT build a "WorldBuilder
mode" that suppresses or reroutes facts: a second, quieter mutation path is precisely the hole this model closes.
- **ADDING is "grants on demand"** — the grants machine hands an entity over on a genuine acquisition;
  WB hands the same entity over on a click. From the model's side they are the SAME event: same DOMAIN fact,
  every consumer reacting identically ([triggers.md](../../specs/triggers.md)).
- ⚠ **REMOVING is the mirror, and *"we do not have any"*** — a WB removal is an inverse grant, and the
  machine has no such notion, so the remove side cannot lean on precedent the way adding can. ⛔ The answer is
  NOT grant-removal machinery for WB's sake: the removal FACT must exist and be emitted, the same fact a genuine
  in-play removal would announce. **Those facts are THINNEST exactly where WB is most arbitrary**, because normal
  gameplay rarely removes — a tech is monotonic in play, but WB can un-research one. Expect to FIND MISSING
  removal facts rather than merely route existing ones; per *"add all the events, ever"*, the answer to a missing
  one is to add it.

⚖ **WB does not CONSTRAIN a cut, and that is not licence to leave a break.** It *"will need a real review
and pass, post rework"* and may temporarily lag — so a WB path is never a reason to preserve a shape or keep a
legacy call alive. ⛔ But *"we cannot accept actually breaking worldbuilder stuff, we fix things we see"*: a WB
path that shows up broken, in a log or on screen, is wired onto the new surface like any other consumer, never
patched by restoring a legacy binding. The misreading that has already cost a pass is reading "not a constraint"
as "WB errors are accepted breakage" and skipping them in a sweep.

