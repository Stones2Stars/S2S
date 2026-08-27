# 10. Going forward

> Part of the **[overview](../overview.md)** spec.

> Anything not enforced by hard typing gets worked around.

A rule in a document binds only someone who reads it, believes it, and still remembers it while writing the
code. So a design invariant that matters is expressed as a **type that makes the wrong move fail to compile**.
The ladder, best first:

1. A type that cannot express the error.
2. **A missing verb**, so the banned operation is unsayable. The refcounted context store has no `set`, because
   a `set` overwrites a refcount — and several buildings can confer the same thing, so an assignment would strip
   a city's third ring of workable tiles the moment it lost one of two grantors.
3. A mechanical check — a script that fails the build, because a rule has to be remembered and a check does not.
4. Only last, prose.

The worked case earned the rule: "specialists do not live in the building package" was true, documented, and
re-corrected more times than anyone cares to count — until the two yield origins became separate *package
types*, after which the wrong deposit simply does not build.

### Where this goes next

| Next | What it means |
|---|---|
| **Dissolve the AI god-classes** | The per-object AI classes become interface-bounded composition. The AI is a consumer of the data side, and it is the half not yet rebuilt |
| **A pluggable AI backend** | Once the AI reads the same maintained state as everything else, the decision layer stops needing to live in the DLL |
| **Volumetric resources** | Resources move from presence (0/1) to quantity (0..N). The storage is already an integer refcount precisely so this is a change of meaning, not a reshape |
| **The events rework** | The largest remaining piece, and the most leverage for modders — gameplay moving out of Python scripts onto the trigger plane as authored data (§7) |
| **Upgrade chains** | Building tiers as a first-class chain rather than the implicit inverse of a dormancy list ([parked](../plans/parked/upgrade-chains.md)) |
| **A new Python surface** | One complete data-fetching library shaped by the new model, with the legacy binding surface disconnected rather than widened |

### The rule underneath all of them

No transitional shims. If the right design needs prerequisite work, do the prerequisite and build the real
thing — a shim that exists only to defer the real design is how a codebase accumulates a load-bearing minority
of things quietly missing while the whole looks nearly done. Which is, more or less, the condition we started
from.
