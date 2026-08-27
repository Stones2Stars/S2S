# 4a. Already-human (×1) — emit as-is

> Part of the **[fixed-point-and-scales](../fixed-point-and-scales.md)** spec.

| field | accessor | why ×1 |
|---|---|---|
| `YieldChange` / `CommerceChange` | `getYieldChange` / `getCommerceChange` | deposited `× 100` by the engine |
| `YieldModifier` / `CommerceModifier` | `getYieldModifier` … | an integer **percent** (emit `percent`) |

