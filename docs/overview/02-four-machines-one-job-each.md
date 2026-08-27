# 2. Four machines, one job each

> Part of the **[overview](../overview.md)** spec.

The first ruling was organisational. The data side is **four separate systems**, each with exactly one job,
chained in one direction. → [architecture/north-star.md](../architecture/north-star.md)

| System | Its one job | Ends at |
|---|---|---|
| `readJson` | Puts the authored data *into* the infos | The info is populated — nothing else is its business |
| `infos` | *Serve* that data, in the shape consumers need | Handing data out; an info never computes with it |
| `cascade` | Sums modifiers — *"how much?"* | A magnitude |
| `enabler` | What we have and can get — *"can I?"* | An availability verdict |

Beside them sits the **tally** — *"how many?"* — not a fifth system with state of its own but a read-only
accessor over counts the game objects already own ([specs/tally.md](../specs/tally.md)).

```
                                          ┌──────────────┐
                                     ┌───▶│   cascade    │──┐
                                     │    │ "how much?"  │  │
  ┌──────────┐    ┌──────────┐       │    └──────────────┘  │  asks   ┌ ─ ─ ─ ─ ─ ─ ─┐
  │ readJson │───▶│  infos   │───────┤                      ├────────▶   tally
  │ puts in  │    │ serve it │       │    ┌──────────────┐  │         │ "how many?"  │
  └──────────┘    └──────────┘       └───▶│   enabler    │──┘          ─ ─ ─ ─ ─ ─ ─ ┘
                                          │  "can I?"    │
                                          └──────────────┘
```

**The test for any new code is one question: whose job is this?** If the answer names two systems, the design
is wrong — not the implementation. Nearly every boundary defect hit was one system doing another's job, which
is why they presented as unrelated bugs and got fixed one at a time.

---

