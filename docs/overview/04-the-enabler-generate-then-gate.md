# 4. The enabler: generate, then gate

> Part of the **[overview](../overview.md)** spec.

→ [specs/enabler.md](../specs/enabler.md)

In the old engine "can I build this?" was not a system. It was a family of functions — `canConstruct`,
`canTrain`, `canResearch` and variants — each walking conditions in whatever order had accumulated, called from
the build list, the AI's production decision, the tech screen, the pedia and a dozen other places, several
re-scanning the whole database per call. They returned a boolean, so the moment you wanted to *explain* a
refusal, a second body of code re-derived the reason and was free to disagree with the answer.

Replacing it starts with noticing that it is two questions which cannot fold into one:

```
  ┌──────────┐   ┌────────────────────────┐   ┌──────────────────┐    ┌──────────────────────┐
  │   HAVE   │──▶│   PASS 1 · GENERATE    │──▶│  PASS 2 · GATE   │──┬▶│ LISTED  build it now │
  │  built · │   │  union(enables)        │   │  requires.build  │  │ └──────────────────────┘
  │ researched│  │    − disables          │   │  requires.operate│  │ ┌──────────────────────┐
  └──────────┘   │    − obsoletes         │   │  allowed         │  ├▶│ GREYED  "go get      │
                 │    − replaces          │   │                  │  │ │          copper"     │
                 │  pure set algebra,     │   │  cannot change   │  │ └──────────────────────┘
                 │  ZERO conditions       │   │  membership      │  │ ┌──────────────────────┐
                 └────────────────────────┘   └──────────────────┘  └▶│ HIDDEN  nothing to do│
                          ↑ CAN GET                                    └──────────────────────┘
```

Pass 1 alone decides what is in the tree, and evaluates no conditions at all. Pass 2 decides whether a tree
member is reachable *now* — it never adds or removes a candidate. Generation is a cheap top-down sweep, so the
only calculation is the gate, and it runs over just the frontier, never the whole database.

### The gate carries *why*, not a boolean

A greyed entry that doesn't say what is missing hands the player a question instead of an answer. The stored
verdict is the identity of the failing clause, and grey-vs-hide is read off it. The discriminator: **can the
asker act on it?** A missing resource greys. An unresearched tech hides (greying it would double-list every
future building). The ground hides — a city cannot acquire the tile it stands on.

> Otherwise a user would just have to guess what is wrong when they see greyed stuff, be it human or AI, and we
> try to avoid that.

The frontier is **one shared choice set**: the UI greys from it and the AI iterates it to decide what to
produce. They cannot disagree, because there is only one. And the reason is *stored* rather than re-derived — a
consumer that re-evaluated the clauses to explain a verdict would be a second gate implementation.

Scale: 4,381 of 5,180 buildings name a tech in their build requirement, 1,216 of those capped.

### Why it runs in both directions

Generation flows down from sources, but the requirement gate resolves by a callback **up** the scope chain — a
city-scope candidate asking its empire about civics, counts, state religion. A down-only design can model OR
but cannot reliably model AND, and forces the author to maintain every requirement at the top of the chain.
Roughly **75% of building requirements are AND**. The up-walk stays.

---

