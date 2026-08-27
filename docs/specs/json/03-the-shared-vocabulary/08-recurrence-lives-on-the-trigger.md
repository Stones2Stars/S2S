# 3.8 Recurrence lives on the TRIGGER plane

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

There is no `interval` field. Anything recurring is a `triggers` entry (§5): the cadence is the trigger
(`"onTurn"`, `{ "onTurn": N }` = every N turns), the odds are its `chance`, the payload its `action`.

