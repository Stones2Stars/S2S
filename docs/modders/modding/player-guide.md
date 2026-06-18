> **Provenance note:** Imported and adapted from the Caveman2Cosmos v44 Player Guide thread on CivFanatics, primarily authored by Thunderbrd. Review for S2S accuracy — S2S inherits C2C's systems but has diverged in AI, combat model, and other areas. Source thread: https://forums.civfanatics.com/threads/caveman2cosmos-v44-player-guide.607564/

---

# C2C v44 Player Guide — Adapted for S2S

## Overview

This document summarizes the player-facing systems of Caveman2Cosmos as documented in the v44 Player Guide. S2S inherits these systems; some details may have changed. Where a system is known to differ in S2S, it is flagged.

---

## 1. Core Philosophy and Scope

C2C/S2S is a total-conversion mod for Civilization IV: Beyond the Sword that dramatically expands every dimension of the base game — era span, unit variety, building depth, combat complexity, and civilizational simulation. The game is designed for Long and Epic speeds; Standard speed is technically playable but not the design target.

The mod spans from Prehistoric (before the Stone Age) through a Space/Future era with distinct tech trees for each era. Era transitions are governed by "gateway techs" — reaching these milestones imposes penalties and grants bonuses to the first civilization that crosses each threshold.

---

## 2. Game Setup Options

The Custom Game screen exposes approximately 60 configurable options. Key options by category:

### Economy and Trade

- **Advanced Economy** — enables inflation, decimalized trade values
- **No Technology Trading / No Technology Brokering**
- **Realistic Corporations**

### Diplomacy

- **Advanced Diplomacy** — adds embassies, Rites of Passage agreements
- **Permanent Alliances**
- **No Vassal States**

### Combat and Military

- **Fight or Flight** — enhanced withdrawal mechanics (Early Withdrawal, Pursuit, Repel, Knockback; see §7)
- **Without Warning Combat Mod** — ambush mechanics with stealth combat modifiers
- **Surround and Destroy** — unit positioning grants combat bonuses from adjacent support units
- **Great Commanders** — field commanders provide bonuses to nearby units
- **No Zones of Control**
- **Require Complete Kills**

### Population and Culture

- **Realistic Culture Spread (RCS)** — terrain and tech resist cultural expansion; jungles, hills, rivers, forests slow spread based on culture level
- **Equilibrium Culture** — culture decays toward a natural equilibrium level
- **No City Flipping from Culture / City Flipping After Conquest**
- **No Fixed Borders**
- **Culturally Linked Starts**

### Religion

- **Choose Religions** — players may select which religion to found when reaching a religious tech
- **Divine Prophets** — Great Prophets found religions rather than automatic assignment (see §9)
- **Limited Religions**
- **Religion Decay**
- **Religious Disabling**
- **No Inquisitions**

### Difficulty and Progression

- **Increasing Difficulty** — difficulty scales upward as the game progresses
- **Upscaled Building and Unit Costs** / **Upscaled Research Costs**
- **Flexible Difficulty** (v41) — mid-game difficulty adjustment

### Civilization Development

- **Developing Leaders** — leader traits are selected organically through play rather than preset definitions; combine with "No Positive Traits on Gamestart" for full organic play
- **Start Without Positive Traits / No Negative Traits**
- **Pure Traits** / **Complex Traits** — alternative trait set with tiered system and point-buy balance (see §10)
- **Unrestricted Leaders / Random Personalities**

### Barbarians and NPCs

- **No Barbarians / Raging Barbarians / Barbarian World**
- **No Barbarian Civs / Barbarians Always Raze**

### Espionage and Crime

- **Advanced Espionage** — spy leveling, additional mission types
- **No Espionage**

### Military Units

- **No Nukes / Advanced Nukes**
- **Infinite XP / Unlimited National Units / Usable Mountains**

### Size Matters (Combat)

- **Size Matters** — enables Combat Quality, Group Volume, and Individual Entity Size dimensions on all units (see §8)

### Miscellaneous

- **Advanced Start** — begin game with pre-placed improvements
- **Final Five** — eliminates the weakest player every 50 turns
- **High to Low** — rotating player control
- **No City Razing**
- **Aggressive AI / Ruthless AI**
- **Tech Diffusion / No Tech Diffusion** — slower civs gradually learn technologies discovered by others
- **Beeline Stings** — skipped technologies cost 20% more to research later
- **No Revolutions** — disables the Revolution stability system

---

## 3. Technology System

### Era Structure

The tech tree spans multiple eras from Prehistoric through Information/Future. Each era has its own distinct tech tree section.

### Gateway Techs

When a player reaches an era-transition ("gateway") technology, penalties take effect for all civs:

- Education cost per population point increases
- A cumulative 10% production cost increase applies to units and buildings
- With the Beeline Stings option active: technologies skipped in the previous era cost 20% more

The first civilization to reach a gateway tech receives one free technology.

### Tech Diffusion

With Tech Diffusion enabled, civilizations that lag behind gradually learn technologies discovered by others, preventing extreme technological divergence in long games.

---

## 4. City Mechanics

### Replacement Buildings

Buildings automatically upgrade when they would otherwise obsolete — typically when the next replacement building's prerequisite technology is discovered. This removes the need for manual obsolescence management.

Example: the Mythology-era building upgraded to the Folklore-era building automatically when Folklore's tech was researched.

### Free Buildings to New Cities

Cities can receive free buildings from various sources (civics, wonders, traits). The XML tag system for this is documented in the Modder's Documentation thread.

### Terrain Features on City Plots

Cities can coexist with terrain features (forests, caves) on their plot. Features are destroyed when city population reaches defined thresholds. Caves are an exception and persist permanently.

### Treasury Upkeep (v42+)

Gold held in the treasury incurs a per-turn upkeep cost that scales steeply at high amounts. This motivates spending accumulated gold rather than hoarding it.

### Group Wonders (v37+)

A "Group Wonder" is a set of wonders where:

- Each individual wonder can only be built once in the entire game
- Each player can only build one wonder from the set total

Example: Elite University group wonders, each specializing in a different yield (commerce, production, medicine).

---

## 5. Unit System

### Statuses (Unit Operational Modes)

Statuses are promotion-like abilities displayed as diamond-shaped icons. Key properties:

- Only one status per status group can be active simultaneously
- Selecting a status typically consumes all movement points ("quick" statuses are exceptions)
- A status with an "X" overlay icon removes the current status in that group

**Named statuses:**

- **Stay the Hand** — prevents attacking; allows peaceful passage through enemy territory
- **Standout** — removes all unit invisibility for reliable escort duty
- **Surprise** — reveals to defenders; grants stealth combat bonuses for defending unit
- **Forced March / Quick March** — improved movement at a combat penalty

### Invisibility

Each invisibility type is denoted by a distinct icon. Observing a unit requires a vision rating meeting or beating that unit's invisibility rating. Multiple invisibility types exist.

### Route Upgrades

Routes no longer auto-upgrade when technologies are discovered. Workers must manually upgrade routes. The upgrade cost deducts the previous route's build cost from the new route's build cost (net cost, not full cost).

### Armed Guards

Units with the Armed Guard promotion line (distinguished by a tall shield icon) gain:

- Strong defensive bonuses against criminal/strike unit attacks
- Escort protection capabilities

Trade-off: taking the promotion permanently prevents the unit from attacking.

### Criminals, Strike Teams, Ruffians

These unit types have distinct roles:

- **Criminals:** Infiltration missions (subject to investigation checks); black market trade missions; movement halts if "wanted" (failed investigation); successful investigations return unit to training city.
- **Strike Teams / Ruffians:** Separate roles with distinct capabilities (details not fully recovered — see original thread).

### Merchants

Merchants no longer die after completing trade missions — they return to their origin city for reuse. Production and food merchants cannot be affected by non-specific production modifiers.

### Great Commanders and Great Admirals (v44)

- **Great Commanders:** Land combat specialists; provide bonuses to adjacent/same-plot land units.
- **Great Admirals (v44):** Naval combat specialists; provide bonuses to naval units only.

Previously these roles were combined. As of v44 they are split.

Great Commanders gain promotions over time that expand their area of influence. Large numbers of them can contribute to end-of-turn slowdowns.

### Unit Upkeep

The unit upkeep system (v41+) provides per-unit cost transparency in the UI. With Size Matters enabled, upkeep scales with unit volume.

---

## 6. Culture System

### Realistic Culture Spread (RCS)

When enabled, cultural expansion is not purely radius-based. Terrain acts as resistance: jungles, hills, rivers, and forests slow or block incoming culture at lower culture levels. Resources on terrain ease cultural expansion. Culture levels were extended beyond vanilla BtS ranges.

### Equilibrium Culture

When this option is active, culture levels decay toward a computed equilibrium value rather than accumulating indefinitely. Cities with excess culture gradually lose it; cities with deficit culture grow toward equilibrium.

---

## 7. Combat System

> **S2S note:** S2S has significantly reworked combat internals in `CvCombatModel`. The player-facing mechanics described here (withdrawal, Fight or Flight, etc.) derive from the same XML schema but engine behavior should be verified against current S2S code.

### Standard (Vanilla) Combat

Units fight a series of rounds; the winner of each round deals damage based on strength ratios.

### Fight or Flight (Game Option)

When enabled, C2C's expanded withdrawal system activates:

**Early Withdrawal:** A unit with, e.g., 25 Early Withdrawal attempts to escape when its HP drops to 25% of maximum.

**Pursuit:** Directly subtracts from the opponent's total Withdrawal chance. Example: attacker has 50 Pursuit, defender has 60 Withdrawal → defender withdraws at only 10% chance.

**Defensive Withdrawal:** Defending units may withdraw to unoccupied, non-threatened adjacent plots rather than fighting to the death.

**Repel:** Defenders can force a non-lethal combat exit for the attacker on first successful defensive hit.

**Knockback:** Attackers can force a non-lethal exit for the defender on first successful offensive hit (primarily flame-wielding units).

### Without Warning Combat Mod

The Surprise status enables ambush: first strikes on both sides are negated; Stealth Strikes and Stealth Combat modifiers apply for the defending ambusher only.

### Surround and Destroy

Adjacent friendly units contribute Unnerve bonuses (strength percentage) to an attack. The cap is 60% by default, raised by the attacking unit's Enclose value. The primary attacker's Lunge multiplies the total S&D bonus. Defenders counter with Dynamic Defense.

### Barbarian Generals

Barbarian factions can generate general units when this option is active.

---

## 8. Size Matters

When the Size Matters game option is enabled, three dimensions of each unit become mechanically significant:

**Combat Quality (0–10):**
Represents innate combat proficiency. Baseline is Standard (5). Higher quality: slower XP gain, better combat performance. Units lose all XP when upgrading but retain quality. Ranges from Incapable (0) through Divine (10).

**Group Volume (1–13):**
Represents the quantity of individuals in the unit. Baseline is Battalion (5). Units can **merge** three-to-one (gain one rank) or **split** one-to-three (lose one rank). Smaller groups gain +20% XP per rank below baseline; larger groups lose 20% per rank above. Merge/split requires no movement or action points. Cannot be used by Criminal, Law Enforcement, or Healer units, or by injured/cargo-carrying units.

**Individual Entity Size (1–9):**
Fixed on the unit definition; cannot be changed by the player. Baseline is Medium (5). Larger units hit harder per successful round but have worse Precision and Dodge. Ranges from Fine through Colossal.

All three rankings interact multiplicatively (1.5× per rank step) on: Strength, Max HP, Asset/Power Value, Cargo, Bombard rates, Work Rate, Revolt Protection.

A merged unit is worth 3× more gold when disbanded than each of its three component splits.

---

## 9. Religion System

### Divine Prophets (when enabled)

When the "Divine Prophets" option is active:

- The first civilization to research a religious technology receives a **Great Prophet** rather than automatically founding a religion.
- That Great Prophet can spread any tech-qualifying religion to a chosen city. The first city to receive the spread founds that religion there.
- With "Choose Religions" OFF: Prophets can only found religions tied to their possessed tech.
- With "Choose Religions" ON: Prophets can found any religion not yet founded.

### Religion Decay

When enabled, religions that are not actively maintained and spread gradually lose influence and can disappear from cities.

### Limited Religions

Caps the number of religions that can exist simultaneously in the game.

---

## 10. Traits System

### Leader Traits vs. Civilization Traits

- **Leader Traits:** Bonuses attached to the leader personality. Selectable via Developing Leaders. Affect combat, research, production, etc.
- **Civilization Traits:** Represent the cultural personality of the civilization itself. **Cannot** be selected via Developing Leaders. Ancient Ways Group Wonders can grant civilization traits that persist until the wonder's building obsoletes.

### Developing Leaders

When enabled, leaders begin with no preset traits and accumulate trait points through gameplay, selecting traits organically. Combined with "No Positive Traits on Gamestart" for a fully organic experience.

### Complex Traits (Optional System)

An alternative trait set designed for deep strategic planning:

- Tier system: Tier I (early), Tier II (mid-game), Tier III (late-game). Each tier unlocks stronger trait versions with bigger bonuses and bigger penalties.
- **Point-buy balance:** Every trait within a tier is designed to be approximately equal total value — benefits purchase penalties of matching weight.
- **Synergies:** Certain trait combinations produce outsized effects beyond the sum of their parts.
- **Symmetry:** Most positive traits have a mirrored negative counterpart with inverted effects.
- Six design spreadsheets track trait values across Military, Religious, Yields/Commerce, and Personality categories.
- Recommended configuration: Complex Traits + Developing Leaders + No Positive Traits on Gamestart.

---

## 11. Civic System

C2C substantially expanded the vanilla civic system. Major changes (v41):

- Significant rebalancing of early-game civics to make early civic choice meaningful.
- Civic categories and slot counts expanded far beyond vanilla BtS.
- The Revolution system (when enabled) tracks stability based on civic choices, city contentment, financial state, and war weariness.

### Revolution System

When the "No Revolutions" option is **not** active, civilizations accumulate instability. Triggers include:

- Civic mismatches (civics that poorly fit the civilization's state)
- Widespread unhappiness or unhealthiness
- Financial strain
- War weariness

High instability can cause city revolts, forced civic changes, barbarian spawns from rebel cities, or in extreme cases a civilization split where a breakaway rebel civ forms.

`<iRevolutionIndexModifier>` on buildings adjusts a city's revolution index (negative = stabilizing, positive = destabilizing).

---

## 12. Economic System

### Treasury Upkeep

Gold accumulating in the treasury past normal operating levels incurs increasing per-turn costs. The penalty escalates sharply at high gold reserves, intended to encourage active gold investment.

### Mod Modifier Rule (v42+)

Percentage yield modifiers from buildings apply primarily to land-worked and specialist-generated yields and commerces. They do **not** apply to base amounts generated by other buildings. This focuses modifier stacking on tile-worked output rather than building-chain stacking.

### Food System (v42+)

Food consumption scales gradually with population changes — it increments or decrements by at most 1 per turn rather than jumping sharply at each population milestone.

---

## 13. Gateway Tech Penalties

When any civilization reaches an era gateway technology, all civilizations face:

- Increased education cost per population point
- +10% cumulative production cost for all units and buildings
- With Beeline Stings: techs skipped in the prior era cost 20% more to research

The first civilization to reach the gateway receives one free technology as a reward.

---

## 14. Multi-Maps

When the Multi-Maps game option is active, multiple independent maps run simultaneously. The primary application in C2C was separating Space as a distinct map from the surface world.

**Player experience:**

- Units can transfer between maps via mission actions (teleport-style transition without loading screen)
- Players must manually check each map each turn — there is no automatic notification of events on other maps
- WorldBuilder can edit each map independently
- Some UI jumpiness occurs during map transitions

**Memory caution:** Large multi-map configurations significantly increase RAM usage. Monitor for memory allocation failures on long games.

> **S2S note:** As noted in the player guide, Multi-Maps was described as aspirational/future content in C2C v44. Verify its current state in S2S.

---

## 15. Map and Game Speed Recommendations

**Recommended maps:** C2C_PerfectWorld2F, C2C_PlanetGenerator_0_68, C2C_World.

**Recommended speeds:** Long and Epic. Standard speed is playable but does not give systems like culture, crimes, and outbreaks adequate time to develop meaningfully.

**Difficulty:** Start on a lower difficulty than you think you need — C2C/S2S is substantially harder than vanilla BtS due to AI improvements, expanded threat types, and the depth of the economic and military systems.

---

## 16. Mechanics Quick Reference

| Mechanic | Key Point |
|---|---|
| Route upgrades | Manual only; net cost = new cost minus old cost |
| Building replacement | Automatic on tech unlock of next tier |
| Merchant lifespan | Returns to origin city after trade mission (not destroyed) |
| Unit merge/split | 3-to-1 merge; no movement cost; Criminal/LE/Healer exempt |
| Great Commander vs Admiral | v44: split by land vs. naval |
| Divine Prophets | Prophet gets religion, not the tech |
| Trait tiers | Complex Traits: Tier I/II/III unlock progressively |
| Treasury upkeep | Gold costs increase sharply at high reserves |
| Yield modifiers | Apply to land/specialist yields, not building-generated base |
| Food demand | Increments by ≤1/turn, not per-population-step jumps |
