# Doc-comment FORM — real `///<summary>` comments on the declaration surface

> **Status:** parked · **Owner ruling:** wanted, and *"that will have to go on a separate pass and branch."*
> **Policy:** [DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas).

## The intent — a FORCING FUNCTION, not a style preference

Declarations should carry **structured doc comments** — the `///<summary>` form — rather than the free prose
that accumulated instead. The owner's words: this is something *"I should have considered, and enforced, but
have not."*

**⚑ AND THE REASON IS THE WHOLE POINT: it would have FORCED A REAL CONSIDERED APPROACH, instead of the many
many word salads we see at the moment (owner).** A `<summary>` is a slot with a shape — it asks what the thing
IS, once, in a sentence — so filling it requires deciding what the declaration actually does. Free prose asks
nothing, accepts any length, and is therefore filled with whatever was in the writer's head at the time; the
result is the sprawl now in the tree.

⇒ **So this is [DEC-hard-typing-or-rollerskate](../../architecture/decisions.md#dec-hard-typing-or-rollerskate)
applied to prose**: not a rule saying "write shorter comments" — which binds only an agent who reads it,
believes it and still remembers it at the moment of writing — but a STRUCTURE in which the sprawling version
is awkward to express. That is why the form is worth a pass of its own rather than a style note nobody follows.

## ⛔ NOT NOW, AND NOT ON THIS BRANCH

It is a **separate pass on its own branch** (owner). ⛔ So do not convert a comment to `///<summary>` while
you happen to be in a file, do not start it opportunistically, and do not treat a plain-prose comment on a
declaration as a defect to fix — today it is simply the current form.

⚠ Owner-ruled SEQUENCING with a named end state, so
[DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred) does not reach it — the same standing as
the Python import conversion ([patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md)) and
the golden-age / anarchy status carve-out ([state.md](../../specs/state.md)).

## What it does NOT change

[DEC-comments-only-when-settled](../../architecture/decisions.md#dec-comments-only-when-settled) is about
**whether** a comment is written (only where the design is hard settled and finalized) and about **volume**.
This is about the **FORM** a comment takes once written. The two are independent: adopting the summary form
does not license writing more of them, and it does not soften nuking a contradicting one on sight.

## What the pass will have to decide

- Which declarations carry one — every public surface, or only the interfaces and the calc/enabler entry points.
- Whether the frozen VC7.1 toolchain and the existing tooling do anything with the tags, or whether they are
  purely for the reader. *(Doxygen is configured in-tree — `Sources/Mainpage.dox` — so there is an existing
  consumer to reconcile with, and a decision to make about `///` versus the `/** */` Doxygen form.)*
- How it lands without one enormous diff across every header.
