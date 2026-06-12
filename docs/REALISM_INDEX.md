# The S2S Realism Index™

> A rigorously unscientific catalogue of the game's most **super realistic** mechanics,
> ordered by the distance between what the mechanic claims to depict and anything that has
> ever happened on Earth. Distance is measured in **centibows (cb)** — the SI unit of
> verisimilitude, calibrated against an archer on a standard-size map providing effective
> fire support to a battle roughly **three hundred kilometers away**.
>
> Sibling publication of the [Despair Index](DESPAIR_INDEX.md). The Despair Index is for
> bugs — things the code does wrong. This index is for *features* — things the code does
> exactly as designed, which is somehow worse. This document is not holy writ. It is an
> outlet. No process shall ever cite it.

---

## 🥇 1. The 300-Kilometer Longbow — 100 cb *(definitional)*

On a standard map a tile spans something on the order of hundreds of kilometers. The
ranged-attack mechanic let **every ranged unit** fire into the neighboring tile — an
archer loosing a volley at six orders of magnitude beyond the effective range of a
longbow, from Oslo, hitting a man standing in Bergen.

The masterstroke is what the volley accomplished: until roughly the tank era, **nothing**.
The damage numbers were so small that entire armies camped outside cities for centuries,
performing the ritual potshot as institutional proof of effort — the *"yay, we did
something at least"* clause, fired across three hundred kilometers, dealing zero.

*Status: turned off by the owner in self-defense. Scheduled to return as a siege-only
mechanic (#410), where "bombarding the adjacent tile" abstracts to conducting siege
operations against fortifications you are actually encamped at — the one reading where
the fiction and the mechanics agree.*

---

## 🥈 2. Warfare by Appointment Only — 85 cb

For most of the early game, every unit on the map has exactly **1 movement point** —
raider and farmer, wolf and warlord, all marching in perfect lockstep. Consequence: in
the open field, **nothing can ever catch anything**. Every engagement is strictly
consensual; the enemy you can see will be precisely one tile further away tomorrow,
forever, like a horizon with a grudge.

A horde of sixty raiders can therefore be thinned by exactly two mechanisms: you find
and raze their home town, or they spontaneously decide to have a go. Until one of those
occurs, both armies coexist on the steppe in an eternal, mutually visible, perfectly
safe stalemate — history's first cold war, conducted at walking pace.

*Status: structural. Partially relieved barb-side by horde courage (#409 — they now
decide to have a go); the civ-side army lifecycle is #410.*

---

## 🥉 3. The Clandestine Battering Ram — 80 cb

A battering ram cannot be ordered to batter anything. It has no bombard command, no
button, no siege verb of any kind. Its entire reason for existing — reducing city
defenses — is implemented as a **hidden dice roll** (`iBreakdownChance`, 25%) that procs
per combat round *only while the ram is personally assaulting the garrison*, an
engagement a strength-4 wooden frame conducts approximately once. The walls are chipped,
if they are chipped, as an invisible side effect of the ram's death, at a rate scaled
down by the defenses it is trying to chip.

Nobody is told any of this. The mechanic's documentation lives on a fifteen-year-old
forum thread; the in-code comments reveal it was *originally* a pure no-combat "siege
event" that was later converted to real combat with a note that the rams' strength
should probably be reduced "when I get into the H2H/Distance mechanism" — a mechanism
the code is still waiting for. The AI, for its part, values breakdown chance like a
bombard rate it could aim, overbuilds the units, and then discovers the only way to cash
the stat is to die holding it.

At best, stealthy. At worst — in the owner's full technical assessment — *"just........
no words....."*

Deeper archaeology only makes it better. The mechanic appears in none of the public
documentation (the CivFanatics combat-mod guides do not mention it); the owner knows of
it only because **the designer alluded to it in a conversation once**; the alluded-to
form — defenses grinding down *passively* while siege units camp adjacent, which would
at least have explained why AI stacks besiege cities by picnicking next to them — has no
implementation, and no vestige of one, anywhere in the code. There is a real chance that
part never actually worked. The game may contain AI behaviour tuned around a siege
mechanic that exists exclusively as an oral tradition.

*Status: working as redesigned, which is not the same as designed, which is not the same
as described. The army-lifecycle work (#410) owns the question of what early-era siege
should actually be.*

---

## 4. The Hundred-Year Picnic — 75 cb

An AI stack assembles, marches to an enemy city, encamps — so far, textbook Vegetius —
and then discovers its composition is entirely ranged units, which cannot assault
anything. Lacking both an assault component and any concept of "this is not working,"
the army settles in: taking passive damage from the defenses, returning fire with the
zero-damage 300-kilometer volleys of entry №1, generation after generation, a siege
with all the military significance of a family reunion in hostile weather.

No commitment, no completion, no retreat. Just vibes, attrition, and the unshakeable
institutional conviction that the potshots will, eventually, maybe, do something.

*Status: the acceptance test for the #410 army model — that stack must either requisition
assault troops or go home.*

---

## 5. The Grid Before the Generator — 72 cb

You discover Electricity. Eager to enter the modern age, you order a power plant. You
cannot. First your city must build **Power Lines** — a complete electrical distribution
network, erected pole by pole, for electricity that *does not exist anywhere on the
planet* — and only once the dead copper is humming with anticipation does the game
permit you to construct something that generates power to put in it. `BUILDING_POWER_LINES`
is the gateway prerequisite for the entire electric economy (45 references downstream),
making "wiring the city for nothing" the mandatory first act of every electrical age.

And every city repeats the whole ritual independently — poles, then plant, in each one —
because power is strictly municipal (see the issue trying to make electricity empire-wide).

*Status: working exactly as designed. Empire-wide power generation/consumption is the
productive direction (#411).*

---

## 6. The 326 Artisanal Supply Chains — 68 cb

The game tracks **481 distinct bonus resources**, of which **326 are manufactured goods**
— each a discrete, named, individually-plumbed commodity flowing through the trade
network. This is not an abstraction of an industrial economy; it is an attempt at a
full-relational inventory of one, in a game whose map cannot decide how long a tile is
(see entry №1).

Special mention to the energy aisle, where the seventeen-plus distinct power plant types
include "Osmotic", "Jetstream", "Quantum Vacuum", and — for the late game — "Cosmic
Expansion", a power plant fueled by the universe getting bigger.

*Status: working exactly as designed, at O(bonuses) cost in every prereq sweep the
constructibility work (#195) keeps having to index around.*

---

*Scale calibration note: 100 cb = 1 full Longbow. Readings above 100 are reserved for
mechanics that violate not merely military history but causality.*

*Contribution policy (owner-sanctioned): when play or rework surfaces a mechanic that is
working exactly as designed and the design describes a physically impossible world — a
"super realistic" mechanic — it may be added here, with a score, a name, and a citation
to whichever issue is trying to make it less realistic in the productive direction.*

*Fresh realism may be reported, contested, or savored on the
[S2S Discord](https://discord.gg/R8Uejx6uaK).*
