# The rule the code never states

> Part of the **[vision](../vision.md)** spec.

**ONE detection type counters ONE concealment type**. It is a PAIRING, not a matrix and not a single
contest: a seeker's strength against submarines is weighed against a hider's submarine concealment, and against
nothing else.

The shipped data says so plainly once you know to look — the same key appears on both sides:

| side | carries | means |
|---|---|---|
| the hider | `invisible: INVISIBLE_SUBMARINE` | the METHOD it hides by |
| | `invisibilityIntensity: { INVISIBLE_SUBMARINE: n }` | how well it hides by that method |
| the seeker | `visibilityIntensity: { INVISIBLE_SUBMARINE: n }` | how well it answers that method |
| | `visibilityIntensityRange` | ⚠ a SECOND reach, parallel to vision's |
| | `visibilityIntensitySameTile` | a bonus at zero distance |

**The type IS the pairing.** Nothing in the engine says so, which is why it reads as an arbitrary pile of tables.

