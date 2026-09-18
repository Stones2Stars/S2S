# Unit rendering — the pipeline, and graphics paging ON vs OFF

> **⛔ SETUP IS PLACEMENT, MOVEMENT IS MOVEMENT, AND CONFLATING THEM IS THE RUN-FROM-MID-MAP BUG.** The EXE
> spawns a fresh node at the SCENE ORIGIN — the map centre — so a move-family call issued on a node that has not
> been presented yet tells the engine to reconcile origin→plot as a walk the player watches, on a unit that never
> moved. The whole census is therefore a rule: `SetPosition` at the two placement moments
> ([§9](unit-rendering/10-the-firaxis-reference-contract.md)), `QueueMove`/`ExecuteMove` **only** for real
> movement — `CvSelectionGroup::groupMove` and `CvUnit::updateCombat`, and nowhere else.
> ⚠ A REFRESH symptom (one figure of a stack showing, the rest arriving seconds later) is a different question
> from placement and is never answered by putting a move call back; it needs a non-movement mechanism
> ([§8](unit-rendering/09-open-questions-not-decidable-from.md)).

> **Reference — how the DLL drives unit graphics today.** The renderer is the closed EXE; the DLL reaches it only
> through the 26 virtuals of `CvDLLEntityIFaceBase` (`Sources/Infrastructure/CvDLLEntityIFaceBase.h:20-48`) and
> the 71 of `CvDLLEngineIFaceBase`. Everything here is what the DLL CALLS and WHEN; what the EXE does with a call
> is stated only where it has been MEASURED on the `[GFX]` spine domain (`Graphics.log`,
> `Sources/UI/CvGraphicsTrace.cpp:141`), and is otherwise listed under §8. Every line number is a citation into
> `Sources/`; the tree outranks this page.

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.**
Read the parts your work touches END TO END; the count that applies is something you FIND, not something
you decide ([AGENTS.md](../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[the model](unit-rendering/01-the-model.md)** | 1. The model |
| **[the entity lifecycle](unit-rendering/02-the-entity-lifecycle.md)** | 2. The entity lifecycle |
| **[the plot side choosing the centre](unit-rendering/03-the-plot-side-choosing-the-centre.md)** | 3. The plot side — choosing the centre unit |
| **[graphics paging on vs off](unit-rendering/04-graphics-paging-on-vs-off.md)** | 4. Graphics paging ON vs OFF |
| **[timelines](unit-rendering/05-timelines.md)** | 5. Timelines |
| **[the working model and where the](unit-rendering/06-the-working-model-and-where-the.md)** | 6. The working model, and where the tree differs from it |
| **[doc contradictions fix the doc](unit-rendering/07-doc-contradictions-fix-the-doc.md)** | 7. Doc contradictions (fix-the-doc items) |
| **[the run from origin reconciliation](unit-rendering/08-the-run-from-origin-reconciliation.md)** | 7b. The run-from-origin reconciliation — MEASURED engine behaviour |
| **[open questions not decidable from](unit-rendering/09-open-questions-not-decidable-from.md)** | 8. Open questions (not decidable from the DLL) |
| **[the firaxis reference contract](unit-rendering/10-the-firaxis-reference-contract.md)** | 9. The Firaxis reference contract (vanilla BTS source, `<BTS install>/CvGameCoreDLL/`) |
| **[see also](unit-rendering/11-see-also.md)** | 10. See also |

